"""Run and audit a detached Windows campaign, optionally followed by GCC/Clang/sanitizer long replays."""
import argparse
import json
from pathlib import Path
import subprocess
import sys

from m53_batch import atomic_json, digest, run_lock
from m53_cross_platform import linux_path


def run_campaign(config_path):
    config = json.loads(config_path.read_text(encoding='utf-8-sig'))
    root = config_path.resolve().parent
    tools = root / 'snapshot/tools'
    results = root / 'results'
    state = root / 'campaign-status.json'
    stage = 'prepare'
    try:
        atomic_json(state, {'status': 'RUNNING', 'stage': stage})
        if config['cross_platform']:
            for kind in ['GCC', 'Clang', 'SAN']:
                target = root / f'snapshot/linux-{kind}'
                if not target.exists():
                    subprocess.run(['wsl', '-d', 'ARXChem-Ubuntu-22.04-WSL1', '--', 'cp',
                        f'/root/svt-av2-m53/{kind}/bin/SvtAv2EncApp', linux_path(target)], check=True)
            frozen = {kind: digest(root / f'snapshot/linux-{kind}') for kind in ['GCC', 'Clang', 'SAN']}
            manifest = root / 'linux-snapshots.json'
            if manifest.exists():
                assert json.loads(manifest.read_text()) == frozen, 'Linux snapshot changed'
            else:
                atomic_json(manifest, frozen)
        commands = [('windows', [sys.executable] + config['batch_arguments']),
                    ('audit-windows', [sys.executable, str(tools / 'audit_m53_batch.py'), '--out-dir', str(results)])]
        if results.exists() and '--resume' not in commands[0][1]:
            commands[0][1].append('--resume')
        if config['cross_platform']:
            for kind in ['GCC', 'Clang', 'SAN']:
                out = root / kind
                command = [sys.executable, str(tools / 'm53_cross_platform.py'), '--kind', kind,
                    '--baseline', str(results), '--out-dir', str(out), '--jobs', str(config['jobs']),
                    '--linux-app', linux_path(root / f'snapshot/linux-{kind}')]
                if out.exists():
                    command.append('--resume')
                commands.append((kind, command))
        for stage, command in commands:
            atomic_json(state, {'status': 'RUNNING', 'stage': stage})
            with (root / (stage + '.campaign.log')).open('ab') as log:
                subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
        atomic_json(state, {'status': 'PASS', 'stage': 'complete', 'cross_platform': config['cross_platform']})
        return 0
    except (OSError, ValueError, AssertionError, subprocess.CalledProcessError) as exc:
        atomic_json(state, {'status': 'FAIL', 'stage': stage, 'error': str(exc)})
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    args = parser.parse_args()
    with run_lock(args.config.resolve().parent / 'campaign.lock'):
        return run_campaign(args.config)


if __name__ == '__main__':
    raise SystemExit(main())
