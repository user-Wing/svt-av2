"""Execute a frozen M6 stage list without an AI session; fail closed and resume."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

from m53_batch import atomic_json, digest, run_lock


def validate(config):
    for name, sha in config['frozen_files'].items():
        if digest(name) != sha:
            raise ValueError('campaign dependency changed: ' + name)


def run(config_path):
    root = config_path.resolve().parent
    expected_config = (root / 'campaign-config.sha256').read_text().strip()
    if digest(config_path) != expected_config:
        raise ValueError('campaign configuration fingerprint mismatch')
    config = json.loads(config_path.read_text(encoding='utf-8-sig'))
    stage_name = 'prepare'
    completed = []
    started = time.time()
    with run_lock(root / 'campaign.lock'):
        try:
            validate(config)
            for index, stage in enumerate(config['stages']):
                stage_name = stage['name']
                if digest(config_path) != expected_config:
                    raise ValueError('campaign configuration changed during execution')
                validate(config)
                atomic_json(root / 'campaign-status.json', dict(status='RUNNING', stage=stage_name,
                    index=index, stage_count=len(config['stages']), started_unix=started,
                    pid=os.getpid(), completed=completed, progress=stage.get('progress')))
                command = stage['command']
                print(stage_name, 'START', flush=True)
                log = root / (stage_name + '.log')
                with log.open('ab') as f:
                    subprocess.run(command, stdout=f, stderr=subprocess.STDOUT,
                                   cwd=config['working_directory'], check=True,
                                   timeout=stage.get('timeout', 604800))
                validate(config)
                if stage.get('summary'):
                    result = json.loads(Path(stage['summary']).read_text())
                    if result['status'] != 'PASS':
                        raise ValueError('stage did not report PASS: ' + stage_name)
                completed.append(dict(name=stage_name, log=str(log), log_sha256=digest(log),
                                      command=command, summary=stage.get('summary')))
                print(stage_name, 'PASS', flush=True)
            report = dict(status='PASS', stage='complete', completed=completed,
                started_unix=started, finished_unix=time.time(),
                config_sha256=digest(config_path),
                note='Execution completed; independent AI review and M6 release remain pending.')
            atomic_json(root / 'campaign-status.json', report)
            return 0
        except BaseException as exc:
            atomic_json(root / 'campaign-status.json', dict(status='FAIL', stage=stage_name,
                error=str(exc), completed=completed, started_unix=started, failed_unix=time.time()))
            raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--config', type=Path, required=True)
    args = parser.parse_args()
    return run(args.config)


if __name__ == '__main__':
    raise SystemExit(main())
