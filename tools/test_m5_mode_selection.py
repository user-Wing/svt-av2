"""Independent end-to-end M5 restricted V/H mode-selection acceptance probe.

Generates Y4M without using SVT-AV2 code; checks the fixed decoder's visible
samples against reconstruction exported by the encoder before decoding.
"""
import hashlib
import argparse
import json
import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
APP = ROOT / "build/m5-root/Windows/bin/SvtAv2EncApp.exe"
DEC = ROOT.parent / "AVM-AV2-TEST/tools/bin/avmdec.exe"
OUT = ROOT / "artifacts/m5-root/mode-selection"

def sha(data):
    return hashlib.sha256(data).hexdigest()

def invoke(args):
    result = subprocess.run([str(x) for x in args], capture_output=True, text=True, timeout=90)
    if result.returncode:
        raise RuntimeError(f"exit={result.returncode} cmd={args}\n{result.stderr}\n{result.stdout}")
    return result

def frames_y4m(path, width, height, bitdepth, chroma):
    data = path.read_bytes()
    if not data.startswith(b"YUV4MPEG2 "):
        raise ValueError("not a Y4M stream")
    pos = data.index(b"\n") + 1
    samples = width * height * (3 if chroma == "444" else 3 / 2)
    frame_bytes = int(samples) * (2 if bitdepth == 10 else 1)
    frames = []
    while pos < len(data):
        end = data.index(b"\n", pos)
        if not data[pos:end].startswith(b"FRAME"):
            raise ValueError("bad frame marker")
        start = end + 1
        frame = data[start:start + frame_bytes]
        if len(frame) != frame_bytes:
            raise ValueError("short frame")
        frames.append(frame)
        pos = start + frame_bytes
    return b"".join(frames)

def make_y4m(width, height, depth, chroma, frames, axis):
    step = 1 if depth == 8 else 2
    pmax = (1 << depth) - 1
    cs = "C420jpeg" if depth == 8 else "C420p10" if chroma == "420" else "C444p10"
    data = bytearray(f"YUV4MPEG2 W{width} H{height} F25:1 Ip A0:0 {cs}\n".encode())
    for frame in range(frames):
        data.extend(b"FRAME\n")
        for y in range(height):
            for x in range(width):
                # Two blocks along one axis share their first block's edge signal.
                direction = "h" if axis == "mixed" and (y // 64) % 2 == 0 else "v" if axis == "mixed" else axis
                value = ((1 << (depth - 1)) if axis == "flat" else
                         ((y * 4 if direction == "h" else x * 4) + frame * 7) & pmax)
                data.extend(value.to_bytes(step, "little"))
        cw = width if chroma == "444" else width // 2
        ch = height if chroma == "444" else height // 2
        data.extend((1 << (depth - 1)).to_bytes(step, "little") * cw * ch * 2)
    return bytes(data)

def main():
    global APP, OUT
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--app', type=pathlib.Path, default=ROOT / 'build/m51-root/Windows/bin/SvtAv2EncApp.exe')
    parser.add_argument('--out-dir', type=pathlib.Path, default=ROOT / 'artifacts/m51-root/ai-controlled')
    args = parser.parse_args()
    APP, OUT = args.app, args.out_dir
    if OUT.exists():
        parser.error('choose a fresh output directory')
    OUT.mkdir(parents=True, exist_ok=True)
    cases = []
    for width, height, axis in [(128, 64, "h"), (64, 128, "v"), (128, 128, "h"), (128, 128, "v"), (176, 144, "h"), (176, 144, "v"), (128, 128, "mixed"), (176, 144, "mixed"), (128, 128, "flat"), (1280, 720, "h"), (1280, 720, "v"), (1920, 1080, "h"), (1920, 1080, "v")]:
        for depth, chroma in [(8, "420"), (10, "420"), (10, "444")]:
            for nframes in ([1, 2] if width <= 176 else [1]):
                label = f"{axis}_{width}x{height}_{depth}bit{chroma}_{nframes}f"
                folder = OUT / label
                folder.mkdir(parents=True, exist_ok=True)
                src = folder / "source.y4m"
                src.write_bytes(make_y4m(width, height, depth, chroma, nframes, axis))
                result = {"case": label, "source_sha": sha(src.read_bytes())}
                for selected in ["dc", "adaptive"]:
                    stream = folder / (selected + ".av2")
                    recon = folder / (selected + ".yuv")
                    dec_y4m = folder / (selected + ".decoded.y4m")
                    invoke([APP, "--input", src, "--output", stream, "--export-recon", recon,
                            "--intra-select", selected])
                    original = recon.read_bytes()
                    result[selected + "_stream_sha"] = sha(stream.read_bytes())
                    result[selected + "_recon_sha"] = sha(original)
                    invoke([DEC, "--summary", "-o", dec_y4m, stream])
                    decoded = frames_y4m(dec_y4m, width, height, depth, chroma)
                    if original != decoded:
                        position = next((i for i, (a, b) in enumerate(zip(original, decoded)) if a != b), -1)
                        raise AssertionError(f"{label} {selected}: recon mismatch at byte {position} "
                                             f"(encoder={len(original)} decoder={len(decoded)})")
                result["stream_changed"] = result["dc_stream_sha"] != result["adaptive_stream_sha"]
                result["recon_changed"] = result["dc_recon_sha"] != result["adaptive_recon_sha"]
                result["status"] = ("PASS" if (not result["stream_changed"] and not result["recon_changed"])
                                    else "UNEXPECTED_SELECTION") if axis == "flat" else (
                                        "PASS" if result["stream_changed"] and result["recon_changed"]
                                        else "NO_MODE_TRIGGER")
                cases.append(result)
                print(label, result["status"], flush=True)
    # Fail-closed CLI and configuration validation (no fake enable).
    probe = OUT / "h_128x64_8bit420_1f" / "source.y4m"
    reject_cases = [
        [APP, "--input", probe, "--output", OUT / "reject-invalid.av2",
         "--intra-select", "nonsense"],
        [APP, "--input", probe, "--output", OUT / "reject-p.av2",
         "--intra-select", "adaptive", "--threads", "2"],
        [APP, "--input", probe, "--output", OUT / "reject-qp.av2",
         "--intra-select", "adaptive", "--qp", "99"],
    ]
    for number, args in enumerate(reject_cases):
        check = subprocess.run([str(x) for x in args], capture_output=True, text=True, timeout=20)
        if check.returncode == 0:
            raise AssertionError(f"invalid configuration #{number} was silently accepted")
    print("invalid CLI/QP/threads 3/3 rejected", flush=True)
    (OUT / "acceptance.json").write_text(json.dumps(cases, indent=2), encoding="utf-8")
    if any(x["status"] != "PASS" for x in cases):
        sys.exit(1)

if __name__ == "__main__":
    main()
