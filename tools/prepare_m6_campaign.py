"""Freeze one large, ordered M6 validation campaign; this command does not run it."""
import argparse
import json
from pathlib import Path
import shutil
import sys

from m53_batch import atomic_json, digest
from m53_corpus import generate
from m53_cross_platform import linux_path


def prepare(args):
    source = Path(__file__).resolve().parents[1]
    run = args.run_root.resolve()
    if run.exists():
        raise ValueError('choose a fresh campaign directory; resume with Start-M6Campaign.ps1')
    run.mkdir(parents=True)
    snapshot = run / 'snapshot'
    frozen_source = snapshot / 'source'
    frozen_source.mkdir(parents=True)
    # Source-only checkout: preserve CTest fixtures, omit generated media/builds.
    for name in ['Source', 'include', 'app', 'cmake', 'tests', 'tools', 'benchmarks', 'docs']:
        shutil.copytree(source / name, frozen_source / name,
                        ignore=shutil.ignore_patterns('__pycache__', '*.pyc', '*.bak*'))
    for name in ['CMakeLists.txt', 'LICENSE']:
        shutil.copy2(source / name, frozen_source / name)
    shutil.copytree(frozen_source / 'tools', snapshot / 'tools')
    for folder, app in [('current', args.app), ('m5', args.baseline_app)]:
        target = snapshot / folder
        target.mkdir()
        shutil.copy2(app, target / 'encoder.exe')
        for dll in app.parent.glob('*.dll'):
            shutil.copy2(dll, target / dll.name)
    for path, name in [(args.decoder, 'decoder.exe'), (args.ffmpeg, 'ffmpeg.exe'),
                       (args.ffprobe, 'ffprobe.exe'), (args.model, 'model.json')]:
        shutil.copy2(path, snapshot / name)
    for name in ['final-verification.json', 'source-manifest.json']:
        shutil.copy2(source / 'artifacts/m53-release' / name, snapshot / ('m5-' + name))
    media = args.media_root.resolve()
    media.mkdir(parents=True, exist_ok=True)
    synthetic = run / 'corpus'
    synthetic.mkdir()
    def corpus(name, width, height, frames, scenes):
        folder = synthetic / name
        folder.mkdir()
        records = []
        for depth, chroma in [(8, '420'), (10, '420'), (10, '444')]:
            for scene in scenes:
                path = folder / f'{depth}bit{chroma}-{scene}.y4m'
                generate(path, width, height, frames, depth, chroma, scene)
                records.append({'path': str(path), 'scene': scene})
        path = folder / 'corpus.json'
        atomic_json(path, records)
        return path
    matrix = corpus('matrix', 128, 128, 3, ['cut', 'pan', 'chroma'])
    allqp = corpus('allqp', 64, 64, 3, ['cut'])
    edge = corpus('edge', 136, 72, 3, ['cut'])
    long = corpus('long1000', 128, 128, 1000, ['cut', 'noise'])
    large720 = corpus('large720', 1280, 720, 3, ['pan'])
    large1080 = corpus('large1080', 1920, 1080, 3, ['pan'])
    stages = []
    python = sys.executable
    tools = snapshot / 'tools'
    prefix = ['wsl', '-d', 'ARXChem-Ubuntu-22.04-WSL1', '--']
    native = '/root/svt-av2-m6-' + run.name
    def stage(name, command, **extra):
        stages.append(dict(name=name, command=list(map(str, command)), **extra))
    for kind in ['GCC', 'Clang', 'SAN']:
        stage('build-' + kind, prefix + ['bash', linux_path(tools / 'build_m6_linux.sh'),
              linux_path(frozen_source), native + '/' + kind, kind, linux_path(run / 'native-builds')], timeout=3600)
    def batch(name, corpus_path, qps, modes, tools_arg='11', repeats=1, jobs=4,
              baseline=False, vmaf=False, predictions='all-intra,low-delay-p'):
        output = run / name
        command = [python, tools / 'm6_batch.py', '--app', snapshot / 'current/encoder.exe',
            '--decoder', snapshot / 'decoder.exe', '--corpus', corpus_path, '--out-dir', output,
            '--qps', qps, '--rd-modes', modes, '--tools', tools_arg, '--repeats', repeats,
            '--jobs', jobs, '--predictions', predictions, '--timeout', 14400, '--resume',
            '--contract', frozen_source / 'docs/m6-rd-contracts.md']
        if baseline:
            command += ['--baseline-app', snapshot / 'm5/encoder.exe']
        if vmaf:
            command += ['--ffmpeg', snapshot / 'ffmpeg.exe', '--vmaf-model', snapshot / 'model.json']
        stage(name, command, batch_directory=str(output), progress=str(output / 'summary.json'),
              summary=str(output / 'summary.json'))
        stage('audit-' + name, [python, tools / 'audit_m6_batch.py', '--out-dir', output],
              summary=str(output / 'audit.json'))
        return output
    boundary_qps = '1,42,43,90,91,92,93,100,140,141,142,143,190,191,255'
    primary = batch('boundary-matrix', matrix, boundary_qps, 'off,exhaustive,pruned',
                    tools_arg='00,01,10,11', baseline=True)
    stage('off-compatibility', [python, tools / 'm6_verify_off.py', '--out-dir', primary],
          summary=str(primary / 'off-verification.json'))
    batch('edge-matrix', edge, '1,100,255', 'off,exhaustive,pruned', tools_arg='00,01,10,11', baseline=True)
    batch('allqp', allqp, ','.join(map(str, range(1, 256))), 'exhaustive,pruned')
    for kind in ['GCC', 'Clang', 'SAN']:
        out = run / ('cross-' + kind)
        stage('cross-' + kind, [python, tools / 'm6_cross_platform.py', '--baseline', primary,
              '--out-dir', out, '--linux-app', native + '/' + kind + '/bin/SvtAv2EncApp',
              '--jobs', 4, '--resume'], progress=str(out / 'summary.json'), summary=str(out / 'summary.json'))
    batch('large720', large720, '1,100,255', 'off,pruned', baseline=True, jobs=2, vmaf=True)
    batch('large1080', large1080, '1,100,255', 'off,pruned', baseline=True, jobs=2, vmaf=True)
    batch('long1000', long, '40,80,100,160,220', 'pruned', repeats=3, jobs=4, vmaf=True)
    stage('prepare-real', [python, tools / 'prepare_m6_real.py', '--root', media,
          '--ffmpeg', snapshot / 'ffmpeg.exe', '--ffprobe', snapshot / 'ffprobe.exe'],
          progress=str(media / 'prepare-status.json'), summary=str(media / 'prepare-status.json'))
    batch('pilot-real', media / 'pilot-corpus.json', '100', 'pruned', baseline=True,
          repeats=1, jobs=1, vmaf=True, predictions='low-delay-p')
    stage('pilot-budget', [python, tools / 'm6_pilot_budget.py', '--campaign', run],
          summary=str(run / 'pilot-budget.json'))
    batch('small-exhaustive', media / 'small-corpus.json', '40,80,100,160,220',
          'exhaustive,pruned', baseline=True, repeats=3, jobs=1, vmaf=True)
    batch('real-quality-speed', media / 'real720-corpus.json', '40,80,100,160,220',
          'pruned', baseline=True, repeats=3, jobs=1, vmaf=True)
    stage('report', [python, tools / 'm6_report.py', '--campaign', run],
          summary=str(run / 'reports/summary.json'))
    frozen = {str(p): digest(p) for p in snapshot.rglob('*') if p.is_file()}
    frozen.update({str(p): digest(p) for p in synthetic.rglob('*') if p.is_file()})
    config = dict(schema=6, working_directory=str(run), frozen_files=frozen, stages=stages,
        source_root=str(frozen_source), media_root=str(media),
        policy='No automatic M6 release. AI must review mechanism, correctness and compression results after completion.',
        resource_budget=dict(real_frames=125, real_fps=25, speed_jobs=1, correctness_jobs=4,
            repeats=3, per_command_timeout_seconds=14400,
            retention='streams/log/JSON retained; verified raw removed; corpus and outputs outside workspace'),
        approximate_jobs=dict(boundary_matrix=4320, edge_matrix=288, allqp=3060,
            cross_platform=12960, large720=54, large1080=54, long1000=60,
            pilot_real=4, small_exhaustive=180, real_quality_speed=160))
    atomic_json(run / 'campaign-config.json', config)
    (run / 'campaign-config.sha256').write_text(digest(run / 'campaign-config.json') + '\n')
    atomic_json(run / 'campaign-status.json', dict(status='PREPARED', stages=len(stages)))
    print(json.dumps(dict(run_root=str(run), stages=len(stages), frozen_files=len(frozen),
                         approximate_jobs=config['approximate_jobs']), indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-root', type=Path, required=True)
    parser.add_argument('--media-root', type=Path, required=True)
    for name in ['app', 'baseline-app', 'decoder', 'ffmpeg', 'ffprobe', 'model']:
        parser.add_argument('--' + name, type=Path, required=True)
    prepare(parser.parse_args())


if __name__ == '__main__':
    main()
