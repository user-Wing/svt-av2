"""Score existing frozen r4 streams; no new encodes or parameter selection."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess

from m6_r4_validate import CAMPAIGN, bd_rate, digest, guard, read_y4m, write_y4m


def command(args, log, cwd):
    guard()
    result = subprocess.run([str(x) for x in args], capture_output=True, timeout=180, cwd=cwd)
    log.write_bytes(result.stdout + result.stderr)
    if result.returncode:
        raise RuntimeError(str(log))


def main(out):
    results = json.loads((out / 'result.json').read_text())
    if results['status'] != 'PASS':
        raise ValueError('complete codec validation required before scoring')
    manifest = json.loads((out / 'manifest.json').read_text())
    sources = {s['name']: s for s in manifest['sources']}
    snapshot = CAMPAIGN / 'snapshot'
    model = out / 'model.json'
    shutil.copyfile(snapshot / 'model.json', model)
    scored = dict(status='RUNNING', result_sha256=digest(out / 'result.json'),
        manifest_sha256=digest(out / 'manifest.json'),
        dependencies={str(p): digest(p) for p in [snapshot / 'decoder.exe', snapshot / 'ffmpeg.exe', model]}, points=[])
    for pair in results['cases']:
        source = sources[pair['source']]
        if not source['native'] and not (pair['source'].endswith('-small3') and pair['prediction'] == 'low-delay-p'):
            continue
        if digest(Path(source['path'])) != source['sha256']:
            raise ValueError('frozen input changed')
        for variant in ['baseline', 'candidate']:
            folder = out / f"{pair['source']}-{pair['prediction']}-{pair['qp']}" / variant
            stream, decoded = folder / 'stream.av2', folder / 'vmaf-decoded.y4m'
            if digest(stream) != pair[variant]['stream_sha256']:
                raise ValueError('retained stream changed')
            command([snapshot / 'decoder.exe', '--summary', '-o', decoded, stream], folder / 'vmaf-decode.log', out)
            # AVM emits F0:0. Give the unchanged decoded frame sequence the same
            # explicit time base as the source before framesync-based scoring.
            write_y4m(decoded, source, read_y4m(decoded, source))
            metric = folder / 'vmaf.json'
            relative = metric.relative_to(out).as_posix()
            filters = '[0:v]setpts=PTS-STARTPTS,format=yuv420p10le[d];[1:v]setpts=PTS-STARTPTS,format=yuv420p10le[r];'
            filters += f'[d][r]libvmaf=model=path=model.json:log_fmt=json:log_path={relative}:n_threads=1'
            command([snapshot / 'ffmpeg.exe', '-hide_banner', '-nostdin', '-v', 'error',
                '-filter_complex_threads', '1', '-i', decoded, '-i', source['path'],
                '-filter_complex', filters, '-f', 'null', '-'], folder / 'vmaf.log', out)
            data = json.loads(metric.read_text())
            if len(data['frames']) != source['frames']:
                raise ValueError('VMAF frame count differs')
            scored['points'].append(dict(source=source['name'], source_sha256=source['sha256'],
                frames=source['frames'], variant=variant, qp=pair['qp'], bytes=pair[variant]['bytes'],
                stream_sha256=digest(stream), vmaf=data['pooled_metrics']['vmaf']['mean']))
            decoded.unlink()
            (out / 'vmaf-result.json').write_text(json.dumps(scored, indent=2) + '\n')
    scored['bd_rate_vmaf'] = []
    for source in manifest['sources']:
        if source['native'] or not source['name'].endswith('-small3'):
            continue
        curves = [[(p['vmaf'], p['bytes']) for p in scored['points'] if p['source'] == source['name'] and
                  p['variant'] == variant] for variant in ['baseline', 'candidate']]
        scored['bd_rate_vmaf'].append(dict(source=source['name'], **bd_rate(*curves)))
    scored['status'] = 'PASS'
    (out / 'vmaf-result.json').write_text(json.dumps(scored, indent=2) + '\n')
    print('PASS', len(scored['points']), 'VMAF points')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--out', type=Path, required=True)
    main(parser.parse_args().out)
