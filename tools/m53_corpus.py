"""Generate deterministic scene fixtures for M5.3; user Y4M files can use the same manifest schema."""
import argparse
import json
from pathlib import Path

import numpy as np

SCENES = ['static', 'pan', 'cut', 'noise', 'animation', 'texture', 'lines', 'chroma']


def generate(path, width, height, frames, depth, chroma, scene):
    rng = np.random.default_rng(5300 + SCENES.index(scene))
    scale, maximum = 1 << (depth-8), (1 << depth)-1
    with path.open('wb') as f:
        token = '420jpeg' if depth == 8 else chroma + 'p10'
        f.write(f'YUV4MPEG2 W{width} H{height} F25:1 Ip C{token}\n'.encode())
        for frame in range(frames):
            f.write(b'FRAME\n')
            for plane in range(3):
                div = 2 if plane and chroma == '420' else 1
                y, x = np.indices((height//div, width//div), dtype=np.int64)
                if scene == 'static':
                    pixels = ((x*3 + y*5 + plane*29) % 256) * scale
                elif scene == 'pan':
                    pixels = (((x+frame*3)*7 + (y+frame)*11 + plane*31) % 256) * scale
                elif scene == 'cut':
                    segment = frame // 3
                    pixels = (((x*13 + y*17) ^ (segment*97 + plane*43)) % 256) * scale
                elif scene == 'noise':
                    pixels = rng.integers(0, maximum+1, x.shape, dtype=np.int64)
                elif scene == 'animation':
                    pixels = np.where(((x-frame*3) % 96 < 32) & ((y-frame) % 80 < 32),
                                      224*scale, (32+plane*37)*scale)
                elif scene == 'texture':
                    pixels = (((x*19) ^ (y*23) ^ (frame*7 + plane*29)) % 256) * scale
                elif scene == 'lines':
                    pixels = np.where(((x+frame) % 31 == 0) | ((y+frame) % 29 == 0), maximum, 0)
                else:
                    pixels = np.full_like(x, 128*scale) if plane == 0 else (
                        ((x*17 + y*29 + frame*31 + plane*47) % 256) * scale)
                f.write(pixels.astype('u1' if depth == 8 else '<u2').tobytes())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out-dir', type=Path, required=True)
    parser.add_argument('--size', default='176x144')
    parser.add_argument('--frames', type=int, default=17)
    parser.add_argument('--scenes', default=','.join(SCENES))
    args = parser.parse_args()
    width, height = map(int, args.size.split('x'))
    scenes = args.scenes.split(',')
    if (args.out_dir.exists() or args.frames < 1 or width % 8 or height % 8 or
            not 0 < width <= 1920 or not 0 < height <= 1080 or
            len(scenes) != len(set(scenes)) or not all(s in SCENES for s in scenes)):
        parser.error('choose fresh output directory, valid geometry, frames and scenes')
    args.out_dir.mkdir(parents=True)
    records = []
    for depth, chroma in [(8, '420'), (10, '420'), (10, '444')]:
        for scene in scenes:
            path = args.out_dir / f'{depth}bit{chroma}-{scene}.y4m'
            generate(path, width, height, args.frames, depth, chroma, scene)
            records.append({'path': path.name, 'scene': scene})
    (args.out_dir / 'corpus.json').write_text(json.dumps(records, indent=2), encoding='utf-8')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
