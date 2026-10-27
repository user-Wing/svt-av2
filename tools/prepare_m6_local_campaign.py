"""Freeze a local-original-video continuation, leaving the failed r1 untouched."""
import argparse
import json
from pathlib import Path
import shutil
import sys

from m53_batch import atomic_json, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-root', type=Path, required=True)
    parser.add_argument('--media-root', type=Path, required=True)
    parser.add_argument('--input-dir', type=Path, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    run, media, inputs = args.run_root.resolve(), args.media_root.resolve(), args.input_dir.resolve()
    if run.exists():
        parser.error('choose fresh RunRoot; use Start-M6Campaign.ps1 to resume an existing campaign')
    old = Path('C:/Private/ffmpeg-native-m6-tests/campaign-r1-20261017')
    proof = json.loads((root / 'artifacts/m6-root/short-verification.json').read_text())
    assert digest(old / 'snapshot/current/encoder.exe') == proof['app_sha256']
    assert json.loads((root / 'artifacts/m6-review/partial-verification.json').read_text())['completed_tasks'] == 20796
    run.mkdir(parents=True)
    snapshot = run / 'snapshot'
    snapshot.mkdir()
    tools = snapshot / 'tools'
    tools.mkdir()
    for path in (root / 'tools').iterdir():
        if path.is_file() and path.suffix in ['.py', '.json', '.ps1', '.sh']:
            shutil.copy2(path, tools / path.name)
    for name in ['current', 'm5']:
        shutil.copytree(old / 'snapshot' / name, snapshot / name)
    for name in ['decoder.exe', 'ffmpeg.exe', 'ffprobe.exe', 'model.json']:
        shutil.copy2(old / 'snapshot' / name, snapshot / name)
    shutil.copy2(root / 'docs/m6-rd-contracts.md', snapshot / 'm6-rd-contracts.md')
    shutil.copy2(root / 'artifacts/m6-review/partial-verification.json', snapshot / 'r1-partial-verification.json')
    shutil.copy2(root / 'artifacts/m6-root/short-verification.json', snapshot / 'short-verification.json')
    stages = []
    py = sys.executable
    def stage(name, command, **extra):
        stages.append(dict(name=name, command=list(map(str, command)), **extra))
    stage('prepare-local', [py, tools / 'prepare_m6_local.py', '--input-dir', inputs,
        '--root', media, '--ffmpeg', snapshot / 'ffmpeg.exe', '--ffprobe', snapshot / 'ffprobe.exe'],
        summary=str(media / 'prepare-status.json'), progress=str(media / 'prepare-status.json'))
    def batch(name, corpus, qps, modes, tool='11', pred='low-delay-p', repeat=1, jobs=3, baseline=False):
        out = run / name
        command = [py, tools / 'm6_batch.py', '--app', snapshot / 'current/encoder.exe',
            '--decoder', snapshot / 'decoder.exe', '--ffmpeg', snapshot / 'ffmpeg.exe',
            '--vmaf-model', snapshot / 'model.json', '--corpus', media / corpus,
            '--out-dir', out, '--qps', qps, '--predictions', pred, '--tools', tool,
            '--rd-modes', modes, '--repeats', repeat, '--jobs', jobs, '--timeout', 21600,
            '--contract', snapshot / 'm6-rd-contracts.md', '--resume']
        if baseline:
            command += ['--baseline-app', snapshot / 'm5/encoder.exe']
        stage(name, command, summary=str(out / 'summary.json'), progress=str(out / 'summary.json'),
              batch_directory=str(out))
        stage('audit-' + name, [py, tools / 'audit_m6_batch.py', '--out-dir', out], summary=str(out / 'audit.json'))
    qps = '40,80,100,140,180,200,216,224,232,240,248,255'
    batch('pilot-local', 'pilot-corpus.json', '100', 'pruned', jobs=1, baseline=True)
    stage('av1-existing', [py, tools / 'm6_av1_reference.py', '--pairs', media / 'pairs.json',
        '--ffmpeg', snapshot / 'ffmpeg.exe', '--ffprobe', snapshot / 'ffprobe.exe',
        '--vmaf-model', snapshot / 'model.json', '--out-dir', run / 'av1-existing', '--resume'],
        summary=str(run / 'av1-existing/summary.json'))
    stage('av1-fresh', [py, tools / 'm6_av1_reference.py', '--pairs', media / 'pairs.json',
        '--ffmpeg', snapshot / 'ffmpeg.exe', '--ffprobe', snapshot / 'ffprobe.exe',
        '--vmaf-model', snapshot / 'model.json', '--out-dir', run / 'av1-fresh', '--resume',
        '--crfs', '28,32,36,40,44,48', '--preset', '6'], summary=str(run / 'av1-fresh/summary.json'))
    batch('small-exhaustive', 'small-corpus.json', '40,80,100,160,220,248,255',
          'off,exhaustive,pruned', tool='00,01,10,11', pred='all-intra,low-delay-p', jobs=4)
    batch('screen-1080', 'pilot-corpus.json', qps, 'off,pruned',
          tool='00,01,10,11', pred='all-intra,low-delay-p', jobs=4)
    batch('real-quality', 'local-corpus.json', qps, 'off,pruned', jobs=3, baseline=True)
    stage('off-compatibility-local', [py, tools / 'm6_verify_off.py', '--out-dir', run / 'real-quality'],
          summary=str(run / 'real-quality/off-verification.json'))
    batch('real-ai', 'local-corpus.json', '80,160,224,248', 'pruned', pred='all-intra', jobs=3, baseline=True)
    batch('isolated-speed', 'speed-corpus.json', '80,160,240', 'pruned', jobs=1, repeat=3, baseline=True)
    stage('report', [py, tools / 'm6_local_report.py', '--campaign', run],
          summary=str(run / 'reports/summary.json'))
    frozen = {str(p): digest(p) for p in snapshot.rglob('*') if p.is_file()}
    originals = sorted(inputs.glob('*.mkv'))
    assert len(originals) == 6, 'expected exactly three original/AV1 pairs'
    frozen.update({str(p): digest(p) for p in originals})
    config = dict(schema=6, working_directory=str(run), frozen_files=frozen, stages=stages,
        media_root=str(media), source_directory=str(inputs), inherited_campaign=str(old),
        policy='User-selected local H264 decoded frame sequences replace online sources. Three genres only, no independent holdout. M6 release remains pending.',
        comparison='Existing AV1 p6 CRF36 and fresh AV1 p6 RA CRF curves vs current AV2 AI/low-delay P; configuration comparison, not a universal codec-standard result.',
        timing='Full-quality jobs3/4 are concurrent; only isolated-speed jobs1 has three repeats on first native-rate second. Do not extrapolate its time to the whole clip.',
        approximate_tasks=dict(pilot_local=6, av1_existing=3, av1_fresh=18,
            small_exhaustive=1512, screen1080=576, real_quality=108, real_ai=24, isolated_speed=18))
    atomic_json(run / 'campaign-config.json', config)
    (run / 'campaign-config.sha256').write_text(digest(run / 'campaign-config.json') + '\n')
    atomic_json(run / 'campaign-status.json', dict(status='PREPARED', stages=len(stages)))
    print(json.dumps(dict(run_root=str(run), stages=len(stages), frozen_files=len(frozen),
                         approximate_tasks=config['approximate_tasks']), indent=2))


if __name__ == '__main__':
    main()
