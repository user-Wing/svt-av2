"""Independent M4 pixel/cost model, using AV2 spec 7.13.3.16-18.

Production MC simplifies phase 0/8 BILINEAR. This model evaluates the
normative separable 8-tap filters and two rounds, not the production formula.
NumPy is a test-tool dependency only; the C encoder has no runtime dependency.
"""
import numpy as np


def predict(reference, mv, subsampling, x=0, y=0, width=None, height=None):
    height = reference.shape[0] if height is None else height
    width = reference.shape[1] if width is None else width
    scale = 1 << subsampling
    bx, fx = divmod(mv[0] * 16, scale * 16)
    by, fy = divmod(mv[1] * 16, scale * 16)
    phase_x, phase_y = fx // scale, fy // scale
    # filter table: phase p has taps 3/4 of 128-8*p / 8*p.
    kx = [0, 0, 0, 128 - 8 * phase_x, 8 * phase_x, 0, 0, 0]
    ky = [0, 0, 0, 128 - 8 * phase_y, 8 * phase_y, 0, 0, 0]
    xs = np.arange(x + bx, x + bx + width)
    ys = np.arange(y + by - 3, y + by + height + 4)
    horizontal = np.zeros((height + 7, width), dtype=np.int64)
    for tap, weight in enumerate(kx):
        if weight:
            horizontal += weight * reference[
                np.clip(ys, 0, reference.shape[0] - 1)[:, None],
                np.clip(xs + tap - 3, 0, reference.shape[1] - 1)[None, :]]
    horizontal = (horizontal + 4) >> 3
    vertical = np.zeros((height, width), dtype=np.int64)
    for tap, weight in enumerate(ky):
        if weight:
            vertical += weight * horizontal[tap:tap + height]
    return (vertical + 1024) >> 11


def motion_estimate(source, reference, x, y, width, height):
    block = source[y:y + height, x:x + width].astype(np.int64)
    candidates = []
    for my in range(-8, 9):
        for mx in range(-8, 9):
            candidate = predict(reference, (mx, my), 0, x, y, width, height)
            cost = int(np.abs(block - candidate).sum())
            candidates.append((cost, abs(mx) + abs(my), my, mx))
    cost, _, my, mx = min(candidates)
    return mx, my, cost


def texture(width, height, bit_depth, plane, epoch=0):
    rng = np.random.default_rng(20261011 + 503 * plane + epoch * 7919)
    # Smooth nonperiodic cells plus fine detail retain measurable coding error.
    coarse = rng.integers(24, 220, size=((height + 7) // 8, (width + 7) // 8))
    coarse = np.repeat(np.repeat(coarse, 8, axis=0), 8, axis=1)[:height, :width]
    yy, xx = np.indices((height, width))
    values = np.clip(coarse + ((3 * xx + 5 * yy) % 17) - 8, 0, 255)
    return (values * (1 << (bit_depth - 8))).astype(np.int64)
