"""Prepare frozen local H264/AV1 pairs; preserve native frame sequence and rate."""
import argparse
from fractions import Fraction
import json
from pathlib import Path
import shutil
import subprocess
import time

from m53_batch import atomic_json, digest, inspect_source, run_lock


def probe(path, ffprobe):
    command = [str(ffprobe), '-v', 'error', '-select_streams', 'v:0',
        '-show_streams', '-show_format', '-show_frames',
        '-show_entries', 'frame=best_effort_timestamp_time,pkt_duration_time',
        '-of', 'json', str(path)]
    result = subprocess.run(command, check=True, capture_output=True, text=True)
    value = json.loads(result.stdout)
    packet_command = [str(ffprobe), '-v', 'error', '-select_streams', 'v:0',
        '-show_packets', '-show_entries', 'packet=size', '-of', 'json', str(path)]
    packets = json.loads(subprocess.run(packet_command, check=True,
        capture_output=True, text=True).stdout)['packets']
    return dict(path=str(path), sha256=digest(path), bytes=path.stat().st_size,
        video_payload_bytes=sum(int(p['size']) for p in packets), probe=value,
        commands=[command, packet_command])


def align(source, av1):
    a, b = source['probe']['streams'][0], av1['probe']['streams'][0]
    if a['codec_name'] != 'h264' or b['codec_name'] != 'av1':
        raise ValueError('expected H264 source and AV1 comparison')
    for field in ['width', 'height', 'avg_frame_rate']:
        if a[field] != b[field]:
            raise ValueError('source/AV1 geometry or frame rate mismatch: ' + field)
    if a['pix_fmt'] not in ['yuv420p', 'yuv420p10le']:
        raise ValueError('unsupported source pixel format: ' + a['pix_fmt'])
    if a['width'] % 8 or a['height'] % 8 or a['width'] > 1920 or a['height'] > 1080:
        raise ValueError('source exceeds encoder native geometry contract')
    for stream in [a, b]:
        if stream.get('color_transfer') in ['smpte2084', 'arib-std-b67']:
            raise ValueError('HDR needs a separately frozen metric contract')
    af, bf = source['probe']['frames'], av1['probe']['frames']
    if not af or len(af) != len(bf):
        raise ValueError('source/AV1 decoded frame count mismatch')
    ap = [Fraction(f['best_effort_timestamp_time']) for f in af]
    bp = [Fraction(f['best_effort_timestamp_time']) for f in bf]
    pair_deviation = max(abs(x-y) for x, y in zip(ap,bp))
    if pair_deviation > Fraction(1,1000) or any(y <= x for pts in [ap,bp] for x,y in zip(pts,pts[1:])):
        raise ValueError('source/AV1 per-frame presentation timestamps mismatch')
    fps = Fraction(a['avg_frame_rate'])
    # Paired remuxes round midpoint timestamps differently; keep every decoded
    # frame once and freeze observed gaps instead of duplicating trailing frames.
    deviation = max(abs(t - ap[0] - Fraction(i, 1) / fps) for i, t in enumerate(ap))
    return dict(status='PASS', frames=len(af), fps=f'{fps.numerator}:{fps.denominator}',
        timestamp_comparison='Same decoded frame count/order; corresponding PTS differ by at most 1ms',
        maximum_pair_pts_difference_seconds=float(pair_deviation),
        maximum_source_cfr_deviation_seconds=float(deviation),
        timestamp_gaps=[dict(after_frame=i,seconds=float(y-x)) for i,(x,y) in enumerate(zip(ap,ap[1:]))
            if abs(y-x-1/fps)>Fraction(1,1000)],
        duration_contract='Y4M/metrics use one decoded frame per native-rate index; source container timestamp gaps are not duplicated',
        limitation='PTS/geometry alignment verifies frame order, not independent proof of content identity')


def convert(scene, source, profile, root, ffmpeg, frozen_tools):
    folder = root / profile['name']
    folder.mkdir(exist_ok=True)
    output = folder / (scene + '-' + profile['pixel'] + '.y4m')
    record = output.with_suffix('.json')
    frozen = dict(source_sha256=source['sha256'], profile=profile, tools=frozen_tools)
    if output.exists() and record.exists():
        old = json.loads(record.read_text(encoding='utf-8'))
        if old['frozen'] != frozen or digest(output) != old['sha256']:
            raise ValueError('derived corpus cache fingerprint mismatch: ' + str(output))
        return dict(path=str(output), scene=scene, split='test', category=scene,
            provenance_record=str(record), provenance_sha256=digest(record))
    temporary = output.with_suffix('.tmp.y4m')
    command = [str(ffmpeg), '-hide_banner', '-nostdin', '-v', 'error', '-y',
        '-i', source['path'], '-map', '0:v:0', '-an', '-sn', '-dn']
    if profile['name'] == 'small':
        command += ['-vf', 'scale=256:144:flags=lanczos:force_original_aspect_ratio=decrease:'
            'force_divisible_by=8,pad=256:144:(ow-iw)/2:(oh-ih)/2:color=black']
    command += ['-frames:v', str(profile['frames']), '-fps_mode', 'passthrough',
        '-pix_fmt', profile['pixel'], '-f', 'yuv4mpegpipe', '-strict', '-1', str(temporary)]
    started = time.perf_counter()
    subprocess.run(command, check=True, capture_output=True)
    meta = inspect_source(temporary)
    if meta['frames'] != profile['frames'] or meta['fps'] != profile['fps']:
        raise ValueError('conversion frame count/rate mismatch')
    temporary.replace(output)
    atomic_json(record, dict(frozen=frozen, sha256=meta['sha256'], command=command,
        source=source, seconds=time.perf_counter()-started, meta=dict(meta,path=str(output)),
        precision_note='8bit-to-10bit promotion and small-only 420-to-444 upsampling add no original information',
        color_note='Decoded sample values retained; source color metadata is recorded without guessing missing BT.709 tags',
        timing_note='Frame order/count and native nominal rate preserved; Y4M normalizes source timestamp gaps without duplicating or dropping decoded frames',
        scaling_note='Only small corpus uses Lanczos aspect-preserving fit and black padding; full/pilot/speed are native resolution'))
    return dict(path=str(output), scene=scene, split='test', category=scene,
        provenance_record=str(record), provenance_sha256=digest(record))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input-dir', type=Path, required=True)
    parser.add_argument('--root', type=Path, required=True)
    parser.add_argument('--ffmpeg', type=Path, required=True)
    parser.add_argument('--ffprobe', type=Path, required=True)
    parser.add_argument('--pilot-only', action='store_true', help='3 native frames and miniature only; no full decode')
    args = parser.parse_args()
    root = args.root.resolve()
    root.mkdir(parents=True, exist_ok=True)
    ffmpeg, ffprobe = args.ffmpeg.resolve(), args.ffprobe.resolve()
    frozen_tools = dict(ffmpeg_sha256=digest(ffmpeg), ffprobe_sha256=digest(ffprobe),
        script_sha256=digest(Path(__file__).resolve()))
    with run_lock(root/'prepare.lock'):
        atomic_json(root/'prepare-status.json',dict(status='RUNNING',pilot_only=args.pilot_only))
        try:
            pairs, corpora = [], {name:[] for name in ['local','pilot','speed','small']}
            for scene in ['2D','3D','Real']:
                source_path = args.input_dir.resolve() / ('1080p-' + scene + '.mkv')
                av1_path = source_path.with_name(source_path.stem + '-SVT-CRF36-p6.mkv')
                source, av1 = probe(source_path, ffprobe), probe(av1_path, ffprobe)
                alignment = align(source, av1)
                video = source['probe']['streams'][0]
                frames, fps = alignment['frames'], alignment['fps']
                profile = dict(name='pilot', frames=min(3,frames), pixel='yuv420p10le', fps=fps)
                profiles = [profile]
                profiles += [dict(name='small', frames=min(5,frames),pixel=p,fps=fps)
                    for p in ['yuv420p','yuv420p10le','yuv444p10le']]
                if not args.pilot_only:
                    profiles += [dict(name='local',frames=frames,pixel='yuv420p10le',fps=fps),
                        dict(name='speed',frames=min(24,frames),pixel='yuv420p10le',fps=fps)]
                budget = video['width']*video['height']*3*sum(p['frames'] for p in profiles)
                if shutil.disk_usage(root).free < budget:
                    raise OSError('insufficient disk for derived local corpus')
                for profile in profiles:
                    corpora[profile['name']].append(convert(scene,source,profile,root,ffmpeg,frozen_tools))
                pair = dict(scene=scene, source_path=str(source_path),av1_path=str(av1_path),
                    reference_y4m=str(root/'local'/(scene+'-yuv420p10le.y4m')),
                    frames=frames,fps=fps,width=video['width'],height=video['height'],depth=10,chroma='420',
                    source_sha256=source['sha256'],av1_sha256=av1['sha256'],
                    video_payload_bytes=av1['video_payload_bytes'],frame_alignment=alignment,
                    source=source,av1=av1)
                pairs.append(pair)
                print(scene, 'PREPARED', frames, fps, flush=True)
            suffix = '-preflight' if args.pilot_only else ''
            for name, rows in corpora.items():
                if rows:
                    atomic_json(root/(name+suffix+'-corpus.json'),rows)
            atomic_json(root/('pairs'+suffix+'.json'),pairs)
            atomic_json(root/'prepare-status.json',dict(status='PASS',pilot_only=args.pilot_only,
                tools=frozen_tools,frames=[p['frames'] for p in pairs],
                limitation='H264-decoded user references, not camera masters; AV1 measured as supplied, encode time unknown'))
        except BaseException as exc:
            atomic_json(root/'prepare-status.json',dict(status='FAIL',error=str(exc)))
            raise
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
