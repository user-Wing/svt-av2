"""Download pinned Netflix references and prepare a bounded external M6 corpus.

References already contain QP15 compression. They define the identical input
for every encoder under test; results do not measure against camera masters.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import time
import http.client
import urllib.error
import urllib.request

from m53_batch import atomic_json, digest, inspect_source, run_lock


def s3_etag(path):
    # Netflix listing objects use the AWS CLI default 8 MiB multipart chunks.
    hashes = []
    with path.open('rb') as f:
        for chunk in iter(lambda: f.read(8 * 1024 * 1024), b''):
            hashes.append(hashlib.md5(chunk).digest())
    return hashlib.md5(b''.join(hashes)).hexdigest() + '-' + str(len(hashes))


def download(source, root):
    path = root / (source['id'] + '.mp4')
    record = path.with_suffix('.source.json')
    if path.exists() and record.exists():
        old = json.loads(record.read_text(encoding='utf-8'))
        if old['source'] != source or path.stat().st_size != source['expected_size'] or digest(path) != old['sha256']:
            raise ValueError('download cache fingerprint mismatch: ' + str(path))
        return path, old
    part = path.with_suffix('.part')
    offset = part.stat().st_size if part.exists() else 0
    if offset > source['expected_size']:
        raise ValueError('oversized partial download')
    failures = 0
    while offset < source['expected_size']:
        end = min(offset + 8 * 1024 * 1024, source['expected_size']) - 1
        headers = {'If-Match': '"' + source['expected_etag'] + '"', 'Range': f'bytes={offset}-{end}'}
        request = urllib.request.Request(source['url'], headers=headers)
        try:
            with urllib.request.urlopen(request, timeout=60) as response:
                if response.headers.get('ETag','').strip('"') != source['expected_etag']:
                    raise ValueError('remote source ETag changed')
                if response.status != 206 or not response.headers.get('Content-Range','').startswith(f'bytes {offset}-{end}/'):
                    raise ValueError('server did not honor exact download resume range')
                with part.open('ab' if offset else 'wb') as output:
                    remaining = end - offset + 1
                    while remaining:
                        block = response.read(min(1024 * 1024, remaining))
                        if not block:
                            raise ConnectionError('truncated HTTP response')
                        output.write(block)
                        remaining -= len(block)
            failures = 0
        except (OSError, http.client.HTTPException) as exc:
            if isinstance(exc, urllib.error.HTTPError) and exc.code not in [408,429,500,502,503,504]:
                raise
            failures += 1
            if failures >= 3:
                raise
            time.sleep(2 * failures)
        offset = part.stat().st_size if part.exists() else 0
    if part.stat().st_size != source['expected_size']:
        raise ValueError('download length mismatch')
    etag = source['expected_etag']
    actual_etag = s3_etag(part) if '-' in etag else hashlib.md5(part.read_bytes()).hexdigest()
    if actual_etag != etag:
        raise ValueError('download content does not match pinned S3 ETag')
    sha = digest(part)
    if source['expected_sha256'] and source['expected_sha256'] != sha:
        raise ValueError('download SHA256 mismatch')
    part.replace(path)
    value = dict(source=source,sha256=sha,size=path.stat().st_size,
        note='Official SHA256 unavailable; pinned S3 size/ETag verified, then SHA256 frozen locally.')
    atomic_json(record,value)
    return path,value


def convert(source, source_record, source_path, profile, root, ffmpeg, ffprobe):
    folder = root / profile['name']
    folder.mkdir(exist_ok=True)
    output = folder / (source['id'] + '-' + profile['pixel'] + '.y4m')
    record_path = output.with_suffix('.json')
    filters = (f"fps=25,scale={profile['width']}:{profile['height']}:flags=lanczos:"
               'force_original_aspect_ratio=decrease:force_divisible_by=8:out_range=tv:out_color_matrix=bt709,'
               f"pad={profile['width']}:{profile['height']}:(ow-iw)/2:(oh-ih)/2:color=black,"
               'setsar=1,setpts=PTS-STARTPTS,format=' + profile['pixel'])
    frozen = dict(source_sha256=source_record['sha256'],profile=profile,filters=filters,
                  ffmpeg_sha256=digest(ffmpeg),ffprobe_sha256=digest(ffprobe),start_seconds=0)
    if output.exists() and record_path.exists():
        old = json.loads(record_path.read_text(encoding='utf-8'))
        if old['frozen'] != frozen or digest(output) != old['sha256']:
            raise ValueError('derived corpus fingerprint mismatch: ' + str(output))
        return dict(path=str(output),scene=source['id'],split=source['split'],
                    category=source['category'],source_sha256=source_record['sha256'],
                    provenance_record=str(record_path),provenance_sha256=digest(record_path))
    probe = subprocess.run([str(ffprobe),'-v','error','-show_streams','-show_format','-of','json',str(source_path)],
        check=True,capture_output=True,text=True)
    info = json.loads(probe.stdout)
    video = next(s for s in info['streams'] if s['codec_type'] == 'video')
    if video.get('color_transfer') in ['smpte2084','arib-std-b67']:
        raise ValueError('HDR input requires a separately frozen tone-map contract')
    if float(video.get('duration',info['format'].get('duration',0))) < profile['frames']/25:
        raise ValueError('clip is too short for requested profile; no looping/padding frames')
    temporary = output.with_suffix('.tmp.y4m')
    command = [str(ffmpeg),'-hide_banner','-nostdin','-v','error','-y','-i',str(source_path),
        '-an','-sn','-vf',filters,'-frames:v',str(profile['frames']),'-pix_fmt',profile['pixel'],
        '-color_range','tv','-colorspace','bt709','-color_primaries','bt709','-color_trc','bt709',
        '-f','yuv4mpegpipe','-strict','-1',str(temporary)]
    start = time.perf_counter()
    subprocess.run(command,check=True,capture_output=True)
    inspected = inspect_source(temporary)
    if inspected['frames'] != profile['frames']:
        raise ValueError('conversion frame count mismatch')
    temporary.replace(output)
    value = dict(frozen=frozen,sha256=digest(output),source=source,source_file=source_record,
        command=command,probe=info,seconds=time.perf_counter()-start,
        color_assumption='SDR BT.709 limited; if source metadata is absent this is an explicit interpretation',
        precision_note='Derived '+profile['pixel']+'; 8bit promotion or 420-to-444 upsampling adds no original precision/chroma information. Source pix_fmt='+video['pix_fmt'],
        scaling_note='Lanczos aspect-preserving scale, dimension rounding to multiples of 8, centered black padding; no crop',
        meta=dict(inspected,path=str(output)))
    atomic_json(record_path,value)
    return dict(path=str(output),scene=source['id'],split=source['split'],category=source['category'],
                source_sha256=source_record['sha256'],provenance_record=str(record_path),provenance_sha256=digest(record_path))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=Path('C:/Private/ffmpeg-native-m6-media'))
    parser.add_argument('--sources',type=Path,default=Path(__file__).with_name('m6_real_sources.json'))
    parser.add_argument('--ffmpeg',type=Path,required=True)
    parser.add_argument('--ffprobe',type=Path,required=True)
    parser.add_argument('--only',help='comma separated source ids for short preflight; omitted means all nine')
    parser.add_argument('--pilot-only',action='store_true',help='only 1 second and five-frame miniature; no 5 second corpus')
    args=parser.parse_args()
    root=args.root.resolve();root.mkdir(parents=True,exist_ok=True)
    sources=json.loads(args.sources.read_text(encoding='utf-8'))['sources']
    if args.only:
        selected=args.only.split(',')
        if not set(selected) <= {s['id'] for s in sources}:
            parser.error('unknown source id')
        sources=[s for s in sources if s['id'] in selected]
    profiles=[dict(name='pilot',width=1280,height=720,frames=25,pixel='yuv420p10le')]
    profiles += [dict(name='small',width=128,height=128,frames=5,pixel=p)
                 for p in ['yuv420p','yuv420p10le','yuv444p10le']]
    if not args.pilot_only:
        profiles.insert(0,dict(name='real720',width=1280,height=720,frames=125,pixel='yuv420p10le'))
    with run_lock(root/'prepare.lock'):
        download_root=root/'downloads';download_root.mkdir(exist_ok=True)
        per_profile={p['name']:[] for p in profiles}
        budget=sum(s['expected_size'] for s in sources)+sum(p['width']*p['height']*3*p['frames'] for p in profiles)*len(sources)
        if shutil.disk_usage(root).free < budget:
            raise OSError('insufficient disk for bounded real corpus')
        atomic_json(root/'prepare-status.json',dict(status='RUNNING',source_count=len(sources),budget_bytes=budget))
        source = None
        try:
            for source in sources:
                original,record=download(source,download_root)
                for profile in profiles:
                    if profile['name'] in ['pilot','small'] and source['id'] not in ['narrator','foodmarket']:
                        continue
                    per_profile[profile['name']].append(convert(source,record,original,profile,root,args.ffmpeg.resolve(),args.ffprobe.resolve()))
                print(source['id'],'PREPARED',flush=True)
        except Exception as exc:
            atomic_json(root/'prepare-status.json',dict(status='FAIL',source=source['id'] if source else None,
                error=str(exc),exception=type(exc).__name__,budget_bytes=budget))
            raise
        for name,rows in per_profile.items():
            # A preflight never overwrites the full final corpus manifests.
            suffix='-preflight' if args.only or args.pilot_only else ''
            atomic_json(root/(name+suffix+'-corpus.json'),[r for r in rows if r['split'] != 'calibration'])
            for split in ['test','holdout','calibration']:
                atomic_json(root/(name+'-'+split+suffix+'-corpus.json'),[r for r in rows if r['split']==split])
            if name == 'real720':
                atomic_json(root/('calibration'+suffix+'-corpus.json'),[r for r in rows if r['split']=='calibration'])
        atomic_json(root/'prepare-status.json',dict(status='PASS',source_count=len(sources),budget_bytes=budget,
            sources_sha256=digest(args.sources),script_sha256=digest(Path(__file__)),profiles=profiles,
            limitation='Official QP15 pre-encoded references. Comparison is against these identical decoded inputs, not camera masters.'))
    return 0


if __name__=='__main__':
    raise SystemExit(main())
