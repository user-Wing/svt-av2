"""冻结的 CLIP CPU 单图分类入口；不下载权重或重新生成文本向量。"""
import argparse
import hashlib
import io
import importlib.metadata
import json
import os
from pathlib import Path
import re


def sha_file(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


class Runtime:
    def __init__(self, checkpoint, config=None):
        config = Path(config) if config else Path(__file__).with_name("runtime.json")
        expected = config.with_suffix(".sha256").read_text(encoding="ascii").strip()
        if sha_file(config) != expected:
            raise ValueError("Runtime configuration digest mismatch")
        self.config = json.loads(config.read_text(encoding="utf-8"))
        cfg = self.config
        if (cfg["schema_version"] != 1 or cfg["kind"] != "zero-shot" or
                cfg["classifier"]["class_order"] != ["real", "anime"] or
                cfg["classifier"]["threshold"] != 0.0):
            raise ValueError("Unsupported frozen classifier contract")
        checkpoint = Path(checkpoint).resolve(strict=True)
        if (checkpoint.stat().st_size != cfg["model"]["bytes"] or
                sha_file(checkpoint) != cfg["model"]["sha256"]):
            raise ValueError("CLIP checkpoint size/SHA256 mismatch")
        threads = cfg["inference"]["torch_threads"]
        os.environ["OMP_NUM_THREADS"] = str(threads)
        os.environ["MKL_NUM_THREADS"] = str(threads)
        import clip
        import numpy as np
        import torch
        for name, version in cfg["dependencies"]["tested_packages"].items():
            if importlib.metadata.version(name) != version:
                raise ValueError("Frozen dependency version mismatch: " + name)
        clip_root = Path(clip.__file__).parent
        for name, digest in cfg["dependencies"]["clip_package_files_sha256"].items():
            if sha_file(clip_root / name) != digest:
                raise ValueError("Frozen official CLIP package mismatch: " + name)
        torch.set_num_threads(threads)
        torch.set_num_interop_threads(cfg["inference"]["torch_interop_threads"])
        torch.manual_seed(0)
        self.torch = torch
        self.model, self.preprocess = clip.load(str(checkpoint), device="cpu", jit=False)
        self.model.eval()
        for parameter in self.model.parameters():
            parameter.requires_grad_(False)
        preprocessing = re.sub(r" at 0x[0-9a-fA-F]+", "", repr(self.preprocess))
        if (preprocessing != cfg["inference"]["preprocess"] or
                self.model.visual.input_resolution != 224):
            raise ValueError("Frozen official preprocessing mismatch")
        text = np.asarray(cfg["classifier"]["text_features_float32"], dtype=np.float32)
        if text.shape != (2, 512) or not np.isfinite(text).all():
            raise ValueError("Invalid frozen text features")
        self.text = torch.from_numpy(text)

    def infer_bytes(self, payload):
        from PIL import Image
        with Image.open(io.BytesIO(payload)) as image:
            dimensions = [image.width, image.height]
            tensor = self.preprocess(image).unsqueeze(0)
        with self.torch.inference_mode():
            features = self.model.encode_image(tensor).float()
            features /= features.norm(dim=-1, keepdim=True)
            if tuple(features.shape) != (1, 512) or not self.torch.isfinite(features).all():
                raise ValueError("Invalid CLIP image features")
            cosine = features @ self.text.T
            score = float((cosine[0, 1] - cosine[0, 0]).item())
        label = "anime" if score >= 0.0 else "real"
        return {"label": label,
                "encoder_content_type": self.config["encoder_content_type_mapping"][label],
                "anime_score": score, "threshold": 0.0,
                "score_kind": self.config["classifier"]["score_kind"],
                "cosine_real": float(cosine[0, 0]), "cosine_anime": float(cosine[0, 1]),
                "input_dimensions": dimensions, "input_sha256": hashlib.sha256(payload).hexdigest(),
                "original_selected_model_sha256": self.config["original_selected_model_sha256"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkpoint", type=Path, required=True)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--config", type=Path)
    parser.add_argument("--format", choices=("json", "content-type"), default="json")
    args = parser.parse_args()
    runtime = Runtime(args.checkpoint, args.config)
    result = runtime.infer_bytes(args.image.read_bytes())
    print(result["encoder_content_type"] if args.format == "content-type" else
          json.dumps(result, ensure_ascii=False, allow_nan=False))


if __name__ == "__main__":
    main()
