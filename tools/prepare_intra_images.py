"""Freeze 24 Kodak + 26 Urban100 images before any holdout scoring.

This initial corpus covers photographs and architecture. Screen content/noise/
native HDR coverage must be supplemented before claiming a general I-GATE pass.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
from pathlib import Path
import subprocess
from urllib.request import Request, urlopen

from PIL import Image


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--ffmpeg', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    args = p.parse_args()
    args.out = args.out.resolve()
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out/'original').mkdir()
    (args.out/'y4m').mkdir()
    commit = subprocess.check_output(['git', 'ls-remote',
        'https://github.com/wuzhongwulidong/URBAN100.git', 'refs/heads/master'], text=True).split()[0]
    jobs = [(f'kodak-{i:02d}', 'natural-photography', f'https://r0k.us/graphics/kodak/kodak/kodim{i:02d}.png')
            for i in range(1, 25)]
    jobs += [(f'urban-{i:03d}', 'architecture-fine-lines',
              f'https://raw.githubusercontent.com/wuzhongwulidong/URBAN100/{commit}/img{i:03d}.png')
             for i in range(1, 27)]
    # Freeze the split by identity, independent of pixels, encoder output or scores.
    holdout = {j[0] for j in sorted(jobs, key=lambda j: hashlib.sha256(('I0-20261024/'+j[0]).encode()).digest())[:20]}
    protocol = dict(script_sha256=sha(Path(__file__)), ffmpeg_sha256=sha(args.ffmpeg),
        urban_commit=commit, jobs=jobs, holdout=sorted(holdout),
        conversion='RGB full range to BT.709 limited-range 8-bit YUV420 once; all encoders read identical Y4M',
        resize='fit within 1920x1080, floor each dimension to a multiple of 8; record original dimensions',
        limitations=['no screen-content holdout', 'no deliberate high-ISO category', 'no native HDR'],
        holdout_scores_read=False)
    (args.out/'protocol.json').write_text(json.dumps(protocol, indent=2), encoding='utf-8')

    def prepare(job):
        identity, category, url = job
        original = args.out/'original'/(identity+'.png')
        with urlopen(Request(url, headers={'User-Agent':'SVT-AV2-intra-evaluation/1.0'}), timeout=90) as response:
            original.write_bytes(response.read())
        with Image.open(original) as image:
            original_width, original_height = image.size
        scale = min(1.0, 1920/original_width, 1080/original_height)
        width, height = (int(d*scale)//8*8 for d in [original_width, original_height])
        target = args.out/'y4m'/(identity+'.y4m')
        command = [str(args.ffmpeg.resolve()), '-hide_banner', '-nostdin', '-v', 'error', '-threads', '1',
            '-i', str(original), '-vf', f'scale={width}:{height}:flags=lanczos:in_range=full:out_range=limited:out_color_matrix=bt709',
            '-pix_fmt', 'yuv420p', '-color_range', 'tv', '-colorspace', 'bt709', '-frames:v', '1', '-f', 'yuv4mpegpipe', str(target)]
        with (args.out/(identity+'.log')).open('wb') as log:
            subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=120)
        with target.open('rb') as f:
            header = f.readline().decode('ascii').strip()
            if not f.readline().startswith(b'FRAME'):
                raise ValueError('missing frame')
            payload = f.read()
        if len(payload) != width*height*3//2:
            raise ValueError('wrong Y4M pixel count')
        print(identity, width, height, flush=True)
        return dict(id=identity, category=category, split='holdout' if identity in holdout else 'calibration',
            path=str(target), sha256=sha(target), pixel_sha256=hashlib.sha256(payload).hexdigest(),
            source_url=url, original_path=str(original), original_sha256=sha(original),
            original_dimensions=[original_width, original_height], width=width, height=height,
            bit_depth=8, chroma='420', range='limited', matrix='bt709', header=header, conversion_command=command)

    with ThreadPoolExecutor(max_workers=4) as pool:
        rows = list(pool.map(prepare, jobs))
    (args.out/'manifest.json').write_text(json.dumps(rows, indent=2), encoding='utf-8')
    for split in ['calibration', 'holdout']:
        (args.out/(split+'.json')).write_text(json.dumps([r for r in rows if r['split'] == split], indent=2), encoding='utf-8')
    print('PASS', len(rows), 'images, 30 calibration / 20 unscored holdout', flush=True)


if __name__ == '__main__':
    main()
