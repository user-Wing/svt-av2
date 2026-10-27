"""Sequential ABBAAB encoding-only timing with process CPU and system load."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import json
import os
from pathlib import Path
import statistics
import subprocess
import time

from m6_r5_prepare import CAMPAIGN, guard, sha


class FileTime(ctypes.Structure):
    _fields_ = [('low', wintypes.DWORD), ('high', wintypes.DWORD)]


kernel = ctypes.WinDLL('kernel32', use_last_error=True)
kernel.GetSystemTimes.argtypes = [ctypes.POINTER(FileTime)] * 3
kernel.GetSystemTimes.restype = wintypes.BOOL
kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(FileTime)] * 4
kernel.GetProcessTimes.restype = wintypes.BOOL


def ticks(value):
    return ((value.high << 32) | value.low) / 10000000


def system_times():
    idle, kern, user = FileTime(), FileTime(), FileTime()
    if not kernel.GetSystemTimes(ctypes.byref(idle), ctypes.byref(kern), ctypes.byref(user)):
        raise ctypes.WinError(ctypes.get_last_error())
    return ticks(idle), ticks(kern) + ticks(user)


def process_seconds(process):
    created, ended, kern, user = FileTime(), FileTime(), FileTime(), FileTime()
    if not kernel.GetProcessTimes(int(process._handle), ctypes.byref(created), ctypes.byref(ended),
                                  ctypes.byref(kern), ctypes.byref(user)):
        raise ctypes.WinError(ctypes.get_last_error())
    return ticks(kern) + ticks(user)


def measure(command, log, timeout, deadline):
    guard()
    samples, started = [], time.perf_counter()
    initial = system_times()
    previous = (started, initial, 0.0)
    with log.open('wb') as output, subprocess.Popen([str(x) for x in command], stdout=output, stderr=subprocess.STDOUT) as process:
        try:
            while process.poll() is None:
                guard()
                if time.perf_counter() > deadline:
                    raise TimeoutError('speed window exceeded six-hour global budget')
                if time.perf_counter() - started > timeout:
                    raise TimeoutError('speed command exceeded bounded timeout')
                try:
                    process.wait(timeout=0.5)
                except subprocess.TimeoutExpired:
                    pass
                now, system, cpu = time.perf_counter(), system_times(), process_seconds(process)
                wall_delta, total_delta = now - previous[0], system[1] - previous[1][1]
                total = 100 * (1 - (system[0] - previous[1][0]) / total_delta) if total_delta > 0 else None
                own = 100 * (cpu - previous[2]) / wall_delta / (os.cpu_count() or 1)
                samples.append(dict(seconds=now-started, process_cpu_seconds=cpu,
                    system_cpu_percent=total, process_cpu_percent_of_machine=own,
                    background_cpu_percent_estimate=None if total is None else max(0.0, total-own),
                    campaign_stage=json.loads((CAMPAIGN / 'campaign-status.json').read_text())['stage']))
                previous = (now, system, cpu)
            cpu = process_seconds(process)
            process.wait()
            if process.returncode:
                raise RuntimeError('speed encoder failed: ' + str(log))
        except Exception:
            process.terminate()
            process.wait()
            raise
    return dict(wall_seconds=time.perf_counter()-started, process_cpu_seconds=cpu, samples=samples)


def run(manifest_path, candidate, candidate_args, out):
    guard()
    manifest = json.loads(manifest_path.read_text())
    baseline = Path(manifest['baseline']['path'])
    if sha(baseline) != manifest['baseline']['sha256']:
        raise ValueError('frozen baseline changed')
    out.mkdir(parents=True, exist_ok=False)
    report = dict(status='RUNNING', manifest_sha256=sha(manifest_path), order='ABBAAB',
        script_sha256=sha(Path(__file__)),
        candidate=dict(path=str(candidate), sha256=sha(candidate), extra_args=candidate_args),
        baseline=manifest['baseline'], logical_cpus=os.cpu_count(),
        timing_scope='CLI input read and bitstream write; no recon export, decoding, VMAF, or new builds; background frozen campaign remains active',
        whole_machine_isolated=False, warmups=[], runs=[])
    target = out / 'result.json'
    overall_start = time.perf_counter()
    try:
        for source in manifest['sources']:
            if sha(Path(source['path'])) != source['sha256']:
                raise ValueError('frozen sequence changed')
            first_hash = {}
            for variant, app in [('A', baseline), ('B', candidate)]:
                expected = report['baseline']['sha256'] if variant == 'A' else report['candidate']['sha256']
                if sha(app) != expected:
                    raise ValueError('encoder changed before warmup')
                stream = out / f"{source['scene']}-warmup-{variant}.av2"
                warmup = [str(x) for x in [app, '--input', source['path'], '--output', stream,
                    '--frames', 2, '--qp', 100, '--pred-structure', 'low-delay-p',
                    '--intra-select', 'adaptive', '--partition-select', 'adaptive',
                    '--rd-mode', 'pruned', '--coeff-opt', 'off']]
                if variant == 'B':
                    warmup += candidate_args
                item = measure(warmup, stream.with_suffix('.log'), 1800, overall_start+6*3600)
                item.update(scene=source['scene'], variant=variant, command=warmup,
                    command_sha256=hashlib.sha256(json.dumps(warmup).encode()).hexdigest(),
                    stream_sha256=sha(stream), excluded_from_paired_timing=True)
                report['warmups'].append(item)
                target.write_text(json.dumps(report, indent=2) + '\n')
            for index, variant in enumerate('ABBAAB'):
                if time.perf_counter() - overall_start > 6 * 3600:
                    raise TimeoutError('speed window exceeded six-hour global budget')
                stream = out / f"{source['scene']}-{index}-{variant}.av2"
                app = baseline if variant == 'A' else candidate
                expected = report['baseline']['sha256'] if variant == 'A' else report['candidate']['sha256']
                if sha(app) != expected:
                    raise ValueError('encoder changed during speed window')
                command = [app, '--input', source['path'], '--output', stream, '--qp', 100,
                    '--pred-structure', 'low-delay-p', '--intra-select', 'adaptive',
                    '--partition-select', 'adaptive', '--rd-mode', 'pruned', '--coeff-opt', 'off']
                if variant == 'B':
                    command += candidate_args
                item = measure(command, stream.with_suffix('.log'), 1800, overall_start+6*3600)
                fingerprint = sha(stream)
                if variant in first_hash and first_hash[variant] != fingerprint:
                    raise ValueError('repeated variant bitstream is not deterministic')
                first_hash[variant] = fingerprint
                item.update(scene=source['scene'], order_index=index, variant=variant,
                    command=[str(x) for x in command], stream_sha256=fingerprint, bytes=stream.stat().st_size,
                    fps=source['frames']/item['wall_seconds'])
                report['runs'].append(item)
                target.write_text(json.dumps(report, indent=2) + '\n')
        report['paired'] = []
        for scene in ['2D', '3D', 'Real']:
            runs = [r for r in report['runs'] if r['scene'] == scene]
            pairs = []
            for index in [0, 2, 4]:
                a, b = sorted(runs[index:index+2], key=lambda r:r['variant'])
                pairs.append(dict(wall_ratio_B_A=b['wall_seconds']/a['wall_seconds'],
                                  cpu_ratio_B_A=b['process_cpu_seconds']/a['process_cpu_seconds']))
            report['paired'].append(dict(scene=scene, pairs=pairs,
                median_wall_ratio_B_A=statistics.median(p['wall_ratio_B_A'] for p in pairs),
                wall_ratio_min=min(p['wall_ratio_B_A'] for p in pairs),
                wall_ratio_max=max(p['wall_ratio_B_A'] for p in pairs),
                median_cpu_ratio_B_A=statistics.median(p['cpu_ratio_B_A'] for p in pairs)))
        report['status'] = 'PASS'
    except Exception as error:
        report.update(status='FAIL', error=str(error))
        raise
    finally:
        target.write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--manifest', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--candidate-args-json', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    run(args.manifest, args.candidate, json.loads(args.candidate_args_json.read_text()), args.out)
