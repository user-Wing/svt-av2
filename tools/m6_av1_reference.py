"""Measure existing AV1 files against the exact canonical M6 source frames."""
import argparse
import json
import math
from pathlib import Path
import shutil

import numpy as np

from m53_batch import (atomic_json, digest, dimensions, execute, header,
                       run_lock, y4m_frame)


def quality(reference, decoded, meta):
    dims = dimensions(meta)
    size = sum(dims) * 2
    sums = np.zeros(3, dtype=np.float64)
    with reference.open('rb') as src, decoded.open('rb') as dec:
        for stream in [src, dec]:
            info = header(stream)
            if any(info[k] != meta[k] for k in ['width', 'height', 'depth', 'chroma', 'fps']):
                raise ValueError('reference/AV1 frame geometry or rate mismatch')
        for _ in range(meta['frames']):
            a, b = y4m_frame(src, size), y4m_frame(dec, size)
            if a is None or b is None:
                raise ValueError('missing reference/AV1 frame')
            a, b = np.frombuffer(a, dtype='<u2'), np.frombuffer(b, dtype='<u2')
            pos = 0
            for plane, count in enumerate(dims):
                delta = a[pos:pos+count].astype(np.float64) - b[pos:pos+count]
                sums[plane] += np.sum(delta * delta)
                pos += count
        if src.read(1) or dec.read(1):
            raise ValueError('extra reference/AV1 frames')
    mse = [float(sums[p] / (dims[p] * meta['frames'])) for p in range(3)]
    return dict(mse_yuv=mse, psnr_yuv_db=[10 * math.log10(1023**2 / x) if x else None for x in mse],
                psnr_null_means_infinite=True)


def reusable(path, run_id):
    try:
        row = json.loads(path.read_text(encoding='utf-8'))
        return (row['status'] == 'PASS' and row['run_id'] == run_id
                and bool(row['retained_artifacts'])
                and all(digest(x['path']) == x['sha256'] for x in row['retained_artifacts']))
    except (OSError, ValueError, KeyError, TypeError):
        return False


def frozen(path):
    return dict(path=str(Path(path).resolve()), sha256=digest(path))


def validate(config):
    for item in config['frozen_files']:
        if digest(item['path']) != item['sha256']:
            raise ValueError('frozen AV1 reference dependency changed: ' + item['path'])


def measure(pair, config, out, run_id, timeout):
    fresh = pair.get('job_crf') is not None
    folder = out / (pair['scene'] + (f"-crf{pair['job_crf']}" if fresh else ''))
    folder.mkdir(exist_ok=True)
    result = folder / 'result.json'
    if reusable(result, run_id):
        return json.loads(result.read_text(encoding='utf-8'))
    number = 1
    while (folder / f'attempt-{number:04}').exists():
        number += 1
    work = folder / f'attempt-{number:04}'
    work.mkdir()
    decoded = work / 'decoded.y4m'
    row = dict(scene=pair['scene'], status='FAIL', run_id=run_id, commands=[],
               codec='av1', preset=config['preset'] if fresh else 6,
               crf=pair['job_crf'] if fresh else 36,
               origin='fresh-single-thread' if fresh else 'user-existing', encoding_seconds=None,
               prediction_structure='random-access' if fresh else 'unknown',
               threads=1 if fresh else None,
               median_seconds=None, fps=None,
               timing_note='existing user encode; original encoding time and thread settings unknown')
    try:
        if fresh:
            encoded = work / 'fresh-av1.mkv'
            command = [config['ffmpeg'], '-hide_banner', '-nostdin', '-v', 'error',
                '-i', pair['reference_y4m'], '-map', '0:v:0', '-an', '-c:v', 'libsvtav1',
                '-preset', config['preset'], '-crf', pair['job_crf'], '-threads', '1',
                '-svtav1-params', 'lp=1', '-y', encoded]
            record = execute(command, work / 'encode.log', timeout, out)
            record['stage'] = 'encode'
            row['commands'].append(record)
            row.update(encoding_seconds=[record['seconds']], median_seconds=record['seconds'],
                fps=pair['frames']/record['seconds'],
                timing_note='fresh SVT-AV1 lp=1 threads=1; one encode, includes Y4M read/container write; no recon export')
            pair = dict(pair, av1_path=str(encoded), video_payload_bytes=None)
        probe_log = work / 'packets.json'
        row['commands'].append(execute([config['ffprobe'], '-v', 'error', '-select_streams', 'v:0',
            '-show_packets', '-show_entries', 'packet=size', '-of', 'json', pair['av1_path']],
            probe_log, timeout, out))
        packets = json.loads(probe_log.read_text(encoding='utf-8'))['packets']
        payload = sum(int(p['size']) for p in packets)
        if payload <= 0 or (pair.get('video_payload_bytes') is not None and payload != pair['video_payload_bytes']):
            raise ValueError('AV1 packet payload missing or differs from media preparation')
        row['commands'].append(execute([config['ffmpeg'], '-hide_banner', '-nostdin', '-v', 'error',
            '-threads', '1', '-i', pair['av1_path'], '-map', '0:v:0', '-an', '-sn', '-dn',
            '-vf', 'format=yuv420p10le', '-fps_mode', 'passthrough', '-strict', '-1', '-y', decoded],
            work / 'decode.log', timeout, out))
        row['quality'] = quality(Path(pair['reference_y4m']), decoded, pair)
        vmaf = work / 'vmaf.json'
        filters = ('[0:v]setpts=PTS-STARTPTS,format=yuv420p10le[d];'
                   '[1:v]setpts=PTS-STARTPTS,format=yuv420p10le[r];'
                   '[d][r]libvmaf=model=path=model.json:log_fmt=json:log_path='
                   + vmaf.relative_to(out).as_posix() + ':n_threads=1')
        row['commands'].append(execute([config['ffmpeg'], '-hide_banner', '-nostdin', '-v', 'error',
            '-filter_complex_threads', '1', '-i', decoded, '-i', pair['reference_y4m'],
            '-filter_complex', filters, '-f', 'null', '-'], work / 'vmaf.log', timeout, out))
        metrics = json.loads(vmaf.read_text(encoding='utf-8'))
        if len(metrics['frames']) != pair['frames']:
            raise ValueError('AV1 VMAF frame count mismatch')
        n, d = map(int, pair['fps'].split(':'))
        duration = pair['frames'] * d / n
        row.update(bytes=payload, video_payload_bytes=payload,
            container_bytes=Path(pair['av1_path']).stat().st_size, frames=pair['frames'],
            source_sha256=pair['source_sha256'], canonical_sha256=digest(pair['reference_y4m']),
            av1_sha256=digest(pair['av1_path']), width=pair['width'], height=pair['height'],
            depth=pair['depth'], chroma=pair['chroma'], duration_seconds=duration,
            bitrate_kbps=payload * 8 / duration / 1000, vmaf=metrics['pooled_metrics']['vmaf']['mean'])
        row['frame_alignment'] = pair.get('frame_alignment')
        row['duration_contract'] = 'frames/nativefps, frame-index metrics; no dup/drop; excludes container tail timestamp gaps'
        row['retained_artifacts'] = [frozen(p) for p in sorted(work.iterdir()) if p != decoded]
        decoded.unlink()
        row['status'] = 'PASS'
    except (OSError, ValueError, RuntimeError, TimeoutError, KeyError) as exc:
        row['error'] = str(exc)
    atomic_json(result, row)
    print(pair['scene'], row['status'], row.get('error', ''), flush=True)
    return row


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ['pairs', 'ffmpeg', 'ffprobe', 'vmaf-model', 'out-dir']:
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--resume', action='store_true')
    parser.add_argument('--crfs', help='optional fresh AV1 curve, e.g. 28,32,36,40,44,48; skips user-existing points')
    parser.add_argument('--preset', type=int, default=6)
    parser.add_argument('--timeout', type=int, default=86400)
    args = parser.parse_args()
    crfs = list(map(int, args.crfs.split(','))) if args.crfs else []
    if len(set(crfs)) != len(crfs) or any(not 0 <= crf <= 63 for crf in crfs) or not 0 <= args.preset <= 13:
        parser.error('invalid fresh AV1 CRF/preset')
    out = args.out_dir.resolve()
    if out.exists() and not args.resume:
        parser.error('output exists; use a fresh directory or --resume')
    out.mkdir(parents=True, exist_ok=True)
    with run_lock(out / 'run.lock'):
        pairs = json.loads(args.pairs.read_text(encoding='utf-8'))
        if not pairs or len({p['scene'] for p in pairs}) != len(pairs):
            parser.error('empty or duplicate pairs')
        dependencies = [args.pairs, args.ffmpeg, args.ffprobe, args.vmaf_model, Path(__file__),
            Path(__file__).with_name('m53_batch.py'), Path(__file__).with_name('verify_m4.py')]
        for binary in [args.ffmpeg, args.ffprobe]:
            dependencies.extend(sorted(binary.resolve().parent.glob('*.dll')))
        for pair in pairs:
            if pair['depth'] != 10 or pair['chroma'] != '420':
                parser.error('AV1 reference requires the canonical 10bit420 sequence')
            if Path(pair['scene']).name != pair['scene'] or pair['scene'] in ['.', '..']:
                parser.error('scene must be a plain directory name')
            for key in ['source_path', 'av1_path', 'reference_y4m']:
                dependencies.append(Path(pair[key]))
            if digest(pair['source_path']) != pair['source_sha256']:
                parser.error('H264 source fingerprint differs from pairs')
            if pair.get('av1_sha256') and digest(pair['av1_path']) != pair['av1_sha256']:
                parser.error('AV1 source fingerprint differs from pairs')
        config = dict(schema=1, pairs=pairs, crfs=crfs, preset=args.preset, ffmpeg=str(args.ffmpeg.resolve()),
            ffprobe=str(args.ffprobe.resolve()), frozen_files=[frozen(p) for p in dict.fromkeys(dependencies)],
            numpy_version=np.__version__,
            metric_rule='same canonical frames as AV2; no scaling; 10bit420; PTS reset for VMAF',
            payload_rule='sum ffprobe v:0 packet.size; excludes audio/container overhead',
            comparison_limit='H264 decode is reference, not uncompressed master; user AV1 preset/CRF inferred from filename')
        manifest = out / 'run-manifest.json'
        if manifest.exists():
            if json.loads(manifest.read_text(encoding='utf-8')) != config:
                parser.error('resume fingerprint/configuration mismatch')
        else:
            atomic_json(manifest, config)
            shutil.copyfile(args.vmaf_model, out / 'model.json')
        validate(config)
        if digest(out / 'model.json') != digest(args.vmaf_model):
            parser.error('saved VMAF model changed')
        run_id = digest(manifest)
        atomic_json(out / 'summary.json', dict(status='RUNNING', run_id=run_id))
        jobs = [dict(pair, job_crf=crf) for pair in pairs for crf in crfs] if crfs else pairs
        rows = [measure(pair, config, out, run_id, args.timeout) for pair in jobs]
        validate(config)
        atomic_json(out / 'points.json', rows)
        status = 'PASS' if all(r['status'] == 'PASS' for r in rows) else 'FAIL'
        atomic_json(out / 'summary.json', dict(status=status, run_id=run_id, jobs=len(rows),
                                             passed=sum(r['status'] == 'PASS' for r in rows)))
        lines = ['# AV1 对照点' if crfs else '# 用户既有 AV1 对照点', '',
            '参考为同一 H264 原片解码后的完整帧序列，10bit420 无缩放；原片并非无损母版。', '',
            ('AV1 为同一 canonical 序列新编码，preset ' + str(args.preset) + '，lp=1 threads=1，默认 RA 结构；与 AV2 低延迟结构不同。' if crfs else
             'AV1 来自用户现有 SVT-AV1 p6 CRF36 文件；编码耗时、线程数等原始设置未知。') +
            '码量为视频 packet payload，不计音频和容器。', '',
            '| 片段 | CRF | 视频 MB | kb/s | VMAF | PSNR-Y | 编码秒数 |',
            '|---|---:|---:|---:|---:|---:|---|']
        for row in rows:
            if row['status'] == 'PASS':
                psnr = row['quality']['psnr_yuv_db'][0]
                psnr_text = f'{psnr:.3f}' if psnr is not None else 'inf'
                seconds = f"{row['median_seconds']:.3f}" if row['median_seconds'] is not None else '未知'
                lines.append(f"| {row['scene']} | {row['crf']} | {row['bytes']/1e6:.3f} | {row['bitrate_kbps']:.1f} | {row['vmaf']:.3f} | {psnr_text} | {seconds} |")
            else:
                lines.append(f"| {row['scene']} | FAIL | | | | |")
        lines += ['', '既有点每片只有一个，不计算跨编码器 BD-rate；fresh-single-thread 曲线使用另行明确的单线程设置，不混入既有成片点。' if not crfs else
                  '本表为新鲜 SVT-AV1 单线程曲线，使用 lp=1 threads=1，仅编码一次，时间不含重建导出；AV2时间含重建导出，耗时比较须说明该差异。既有用户成片点单独保留，不混合计算 BD-rate。']
        (out / 'README.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
        return int(status != 'PASS')


if __name__ == '__main__':
    raise SystemExit(main())
