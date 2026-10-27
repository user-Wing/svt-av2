"""Require independent M5.2 evidence before freezing sources and binaries."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'artifacts/m52-root'
MODES = ['p0i0', 'p0i1', 'p1i0', 'p1i1']


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def results(name, count):
    data = read(OUT / name / 'acceptance.json')
    assert len(data) == count and all(x['status'] == 'PASS' for x in data), name
    assert len({x['case'] for x in data}) == count, name
    return data


matrix = results('matrix144', 144)
for compiler in ['gcc144', 'clang144']:
    data = {x['case']: x for x in results(compiler, 144)}
    for x in matrix:
        for mode in MODES:
            for field in ['stream_sha256', 'recon_sha256', 'frame_sha256']:
                assert x[mode][field] == data[x['case']][mode][field], (compiler, x['case'], mode, field)
results('smoke', 18)
results('large24', 24)
results('m4-off156', 156)
long_run = results('long1000', 6)
san_run = {x['case']: x for x in results('san-long1000', 6)}
assert {(x['bit_depth'], x['chroma'], x['prediction']) for x in long_run} == {
    (depth, chroma, prediction) for depth, chroma in [(8, '420'), (10, '420'), (10, '444')]
    for prediction in ['all-intra', 'low-delay-p']}
assert {x['case'] for x in long_run} == set(san_run)
for x in long_run:
    for mode in MODES:
        assert len(x[mode]['frame_sha256']) == 1000
        for field in ['stream_sha256', 'recon_sha256', 'frame_sha256']:
            assert x[mode][field] == san_run[x['case']][mode][field]
for name, count in [('windows-final-ctest.log', 38), ('GCC-ctest.log', 37),
                    ('Clang-ctest.log', 37), ('SAN-ctest.log', 37)]:
    assert re.search(rf'100% tests passed(?:, 0 tests failed)? out of {count}\b',
                     (OUT / name).read_text()), name
traces = []
for name, count in [('trace-matrix.json', 72), ('partial-trace-matrix.json', 18)]:
    trace = read(ROOT / 'artifacts/m52-b' / name)
    assert trace['pass'] and len(trace['cases']) == count
    assert all(x['pass'] and x['partition']['pass'] and
               x['raw_sha256'] == x['internal_sha256'] for x in trace['cases'])
    traces += trace['cases']
trace_evidence = read(ROOT / 'artifacts/m52-b/final-evidence.json')
assert trace_evidence['pass']
assert len(trace_evidence['same_size_content_contrasts']) == 12
assert all(x['pass'] for x in trace_evidence['same_size_content_contrasts'])
assert len(trace_evidence['mixed_17_frame_changes']) == 6
assert all(x['pass'] for x in trace_evidence['mixed_17_frame_changes'])
diagnostic = ROOT / 'artifacts/m52-b/diagnostic-decoder'
assert sha(diagnostic / 'source-manifest.json') == trace_evidence['diagnostic_source_manifest_sha256']
assert sha(diagnostic / 'build/avmdec') == trace_evidence['diagnostic_binary_sha256']
for row in read(diagnostic / 'source-manifest.json'):
    assert sha(diagnostic / 'source' / row['path']) == row['sha256'], row['path']
for row in read(ROOT / 'artifacts/m52-a/source-manifest.json')['files']:
    assert sha(ROOT / row['path']) == row['after_sha256'], row['path']
for row in read(ROOT / 'artifacts/m52-b/source-manifest.json'):
    assert sha(ROOT / row['path']) == row['sha256'], row['path']
leak = (OUT / 'valgrind-m5-api.log').read_text()
assert 'in use at exit: 0 bytes in 0 blocks' in leak
assert 'ERROR SUMMARY: 0 errors from 0 contexts' in leak
assert re.search(r'test_m5_api: \d+ checks, 0 failures\b', leak)
measurements = read(OUT / 'measurements/measurements.json')
assert measurements['status'] == 'PASS' and len(measurements['cases']) == 6
assert {x['case'] for x in measurements['cases']} == set(san_run)
assert all(set(x['modes']) == set(MODES) for x in measurements['cases'])
negative = read(OUT / 'cli-negative.json')
assert len(negative) == 4 and all(x['exit'] != 0 for x in negative)
oracle = ROOT.parent / 'AVM-AV2-TEST/tools/bin/avmdec.exe'
spec = ROOT.parent / 'docs/spec/AV2_Spec_v1.0.0_20260528_38f28e7.pdf'
assert sha(oracle) == '110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac'
assert sha(spec) == 'e9916f091e4e83446aad6b4601641c5b292e569c144c4163b26a4497573b533f'
base_path = ROOT / 'artifacts/m51-root/source-manifest.json'
assert sha(base_path) == '6ecfabcce2396708e1ceb6c5f2ef3e27232107945daf81e909834107506acc55'
assert sha(ROOT / 'artifacts/m4-root/source-manifest.json') == '23eaf3e1d7d319d403cccdf5296e4cf5fdd2624ac00414c6cedb906cca2296aa'
assert sha(base_path) == sha(OUT / 'm51-baseline-source-manifest.json')
for path, expected in read(ROOT / 'artifacts/m51-root/final-verification.json')['binaries'].items():
    if path.startswith('/root/'):
        actual = subprocess.check_output(['wsl', '-d', 'ARXChem-Ubuntu-22.04-WSL1', '--',
                                          'sha256sum', path], text=True).split()[0]
    else:
        actual = sha(ROOT / path)
    assert actual == expected, path
base = read(base_path)
extras = ['tools/verify_m52.py', 'tools/measure_m52.py', 'tools/build_m52_linux.sh',
          'tools/m52_partition_trace.py', 'tools/finalize_m52.py']
manifest = {name: sha(ROOT / name) for name in sorted(set(base) | set(extras))}
(OUT / 'source-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
binaries = {str(p.relative_to(ROOT)): sha(p) for p in [
    ROOT / 'build/m52-root/Windows/bin/SvtAv2EncApp.exe',
    ROOT / 'build/m52-root/Windows/bin/svt_av2_enc_shared.dll']}
for compiler in ['GCC', 'Clang', 'SAN']:
    path = f'/root/svt-av2-m52/{compiler}/bin/SvtAv2EncApp'
    output = subprocess.check_output(['wsl', '-d', 'ARXChem-Ubuntu-22.04-WSL1', '--', 'sha256sum', path], text=True)
    binaries[path] = output.split()[0]
report = {'status': 'M5.2 DONE', 'milestone_status': 'M5 RUNNING; M5.3 pending',
    'official_version': '0.0.4-m4', 'matrix_cases': 144, 'streams_per_case': 4,
    'large_cases': 24, 'frozen_m4_off_cases': 156, 'long_cases_per_platform': 6,
    'frames_per_long_case': 1000, 'partition_trace_streams': len(traces),
    'partition_trace_frames': sum(len(x['partition']['frames']) for x in traces),
    'partition_trace_nodes': sum(f['nodes'] for x in traces for f in x['partition']['frames']),
    'source_files': len(manifest), 'm51_changed_files': [p for p in base if manifest[p] != base[p]],
    'source_manifest_sha256': sha(OUT / 'source-manifest.json'), 'binaries': binaries,
    'oracle_sha256': sha(oracle), 'spec_sha256': sha(spec),
    'trace_evidence_sha256': sha(ROOT / 'artifacts/m52-b/final-evidence.json'),
    'measurements': 'measurements/measurements.json'}
(OUT / 'final-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
