"""Source-defined local quality diagnostics; these do not replace frozen VMAF gates."""
import argparse
import hashlib
import json
from pathlib import Path
import numpy as np
from scipy.ndimage import maximum_filter, uniform_filter


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def read420(path, width, height):
    values = np.fromfile(path, dtype='<u2')
    assert values.size == width * height * 3 // 2
    assert values.max() <= 1023
    count = width * height
    return (values[:count].reshape(height, width).astype(np.float32) / 4,
            values[count:count * 5 // 4].reshape(height // 2, width // 2).astype(np.float32) / 4,
            values[count * 5 // 4:].reshape(height // 2, width // 2).astype(np.float32) / 4)


def gradient(frame):
    return (np.diff(frame, axis=1, append=frame[:, -1:]),
            np.diff(frame, axis=0, append=frame[-1:, :]))


def rmse(error, mask):
    return float(np.sqrt(np.mean(np.square(error[mask]), dtype=np.float64))) if mask.any() else None


def measure(reference, reconstructed):
    y, u, v = reference
    dy, du, dv = reconstructed
    gx, gy = gradient(y)
    rx, ry = gradient(dy)
    strength = np.hypot(gx, gy)
    strong = strength >= 12.0
    neighborhood = maximum_filter(strong, size=5, mode='nearest')
    flat = maximum_filter(strength, size=5, mode='nearest') <= 2.0
    texture = (~neighborhood) & (strength > 2.0)
    ring = neighborhood & (~strong)
    error = dy - y
    grad_error = np.hypot(rx - gx, ry - gy)
    highpass_error = error - uniform_filter(error, size=3, mode='nearest')
    ch = flat.reshape(y.shape[0] // 2, 2, y.shape[1] // 2, 2).all(axis=(1, 3))
    detail = y - uniform_filter(y, size=3, mode='nearest')
    detail_rec = dy - uniform_filter(dy, size=3, mode='nearest')
    energy = float(np.mean(np.square(detail[texture]), dtype=np.float64)) if texture.any() else 0.0
    return {'edge_pixels': int(strong.sum()), 'flat_pixels': int(flat.sum()),
            'texture_pixels': int(texture.sum()), 'ring_pixels': int(ring.sum()),
            'edge_gradient_rmse': rmse(grad_error, strong),
            'edge_luma_rmse': rmse(error, strong),
            'flat_luma_rmse': rmse(error, flat),
            'flat_u_rmse': rmse(du - u, ch), 'flat_v_rmse': rmse(dv - v, ch),
            'edge_neighbor_highpass_rmse': rmse(highpass_error, ring),
            'texture_gradient_rmse': rmse(grad_error, texture),
            'texture_detail_rmse': rmse(detail_rec - detail, texture),
            'texture_energy_ratio': (float(np.mean(np.square(detail_rec[texture]), dtype=np.float64)) / energy) if energy > 0 else None,
            'global_luma_rmse': float(np.sqrt(np.mean(np.square(error), dtype=np.float64)))}


CRITICAL = {
    'animation': ('edge_gradient_rmse', 'edge_luma_rmse', 'flat_luma_rmse',
                  'flat_u_rmse', 'flat_v_rmse', 'edge_neighbor_highpass_rmse'),
    'natural': ('texture_gradient_rmse', 'texture_detail_rmse', 'global_luma_rmse'),
    'unknown': ('edge_gradient_rmse', 'flat_luma_rmse', 'texture_gradient_rmse', 'global_luma_rmse')}


def compare(candidate, baseline, content):
    # Freeze a conservative research gate before inspecting candidate outputs.
    # This is a regression screen against a specified anchor, not a perceptual JND.
    failures = []
    for key in CRITICAL[content]:
        a, b = candidate[key], baseline[key]
        if a is None or b is None:
            continue
        limit = max(b * 1.02, b + 0.02)
        if a > limit:
            failures.append({'metric': key, 'candidate': a, 'baseline': b, 'limit': limit})
    return {'status': 'PASS_LOCAL_REGRESSION_SCREEN' if not failures else 'FAIL_LOCAL_REGRESSION_SCREEN',
            'content': content, 'relative_tolerance': 0.02, 'absolute_tolerance_8bit': 0.02,
            'failures': failures, 'same_quality_comparison_required': True,
            'subjective_quality_validated': False}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--reference', type=Path, required=True)
    p.add_argument('--reconstructed', type=Path, required=True)
    p.add_argument('--baseline', type=Path)
    p.add_argument('--width', type=int, required=True)
    p.add_argument('--height', type=int, required=True)
    p.add_argument('--content', choices=CRITICAL, required=True)
    p.add_argument('--out', type=Path, required=True)
    a = p.parse_args()
    assert (a.width, a.height) in ((1920, 1080), (3840, 2160))
    assert not a.out.exists()
    ref = read420(a.reference, a.width, a.height)
    result = {'schema_version': 1, 'content': a.content,
              'source_sha256': sha(a.reference), 'recon_sha256': sha(a.reconstructed),
              'implementation_sha256': sha(__file__),
              'mask_source': 'reference pixels only', 'units': '8-bit-equivalent code values',
              'metrics': measure(ref, read420(a.reconstructed, a.width, a.height)),
              'scope': 'Local distortion diagnostics; frozen VMAF v1/NEG remain required'}
    if a.baseline:
        result['baseline_sha256'] = sha(a.baseline)
        result['baseline_metrics'] = measure(ref, read420(a.baseline, a.width, a.height))
        result['gate'] = compare(result['metrics'], result['baseline_metrics'], a.content)
    a.out.write_text(json.dumps(result, indent=2, allow_nan=False) + '\n', encoding='utf-8')
    print(json.dumps({'out': str(a.out), 'metrics': result['metrics'], 'gate': result.get('gate')}))


if __name__ == '__main__':
    main()
