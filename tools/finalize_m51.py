"""Check independent M5.1 evidence, then freeze current sources and binaries."""
import hashlib
import json
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'artifacts/m51-root'


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def results(name, count):
    data = json.loads((OUT / name / 'acceptance.json').read_text())
    assert len(data) == count and all(x['status'] == 'PASS' for x in data), name
    return data


matrix = results('matrix144', 144)
for compiler in ['gcc144', 'clang144']:
    data = {x['case']: x for x in results(compiler, 144)}
    assert all(x[mode]['stream_sha256'] == data[x['case']][mode]['stream_sha256'] and
               x[mode]['recon_sha256'] == data[x['case']][mode]['recon_sha256']
               for x in matrix for mode in ['dc', 'adaptive'])
assert all(x['adaptive_stream_changed'] and x['adaptive_recon_changed']
           for x in matrix if x['width'] == 176 and x['pattern'] != 'flat')
results('large24', 24)
results('m4-off156', 156)
long_run = results('long1000', 6)
san_run = {x['case']: x for x in results('san-long1000', 6)}
for x in long_run:
    for mode in ['dc', 'adaptive']:
        assert len(x[mode]['frame_sha256']) == 1000
        assert x[mode]['stream_sha256'] == san_run[x['case']][mode]['stream_sha256']
        assert x[mode]['frame_sha256'] == san_run[x['case']][mode]['frame_sha256']
for name, count in [('windows-final-ctest.log', 37), ('GCC-ctest.log', 36),
                    ('Clang-ctest.log', 36), ('SAN-ctest.log', 36)]:
    text = (OUT / name).read_text()
    assert re.search(rf'100% tests passed(?:, 0 tests failed)? out of {count}\b', text), name
trace = json.loads((ROOT / 'artifacts/m5-b/mode-trace-matrix.json').read_text())
assert trace['self_test']['pass'] and len(trace['cases']) == 12
assert all(x['pass'] and x['raw_sha256'] == x['internal_sha256'] for x in trace['cases'])
leak = (OUT / 'valgrind-m5-api.log').read_text()
assert 'in use at exit: 0 bytes in 0 blocks' in leak
assert 'ERROR SUMMARY: 0 errors from 0 contexts' in leak
measurements = json.loads((OUT / 'measurements/measurements.json').read_text())
assert measurements['status'] == 'PASS' and len(measurements['cases']) == 6
oracle = ROOT.parent / 'AVM-AV2-TEST/tools/bin/avmdec.exe'
spec = ROOT.parent / 'docs/spec/AV2_Spec_v1.0.0_20260528_38f28e7.pdf'
assert sha(oracle) == '110c7a30b2d429f1c75adb3340df149638929863eaaa241e3c3735f3e6b7c1ac'
assert sha(spec) == 'e9916f091e4e83446aad6b4601641c5b292e569c144c4163b26a4497573b533f'
base = json.loads((ROOT / 'artifacts/m4-root/source-manifest.json').read_text())
extras = ['tests/unit/test_m5_api.c', 'tools/m5_mode_trace.py', 'tools/verify_m51.py',
          'tools/verify_m4_off.py', 'tools/measure_m51.py', 'tools/build_m51_linux.sh',
          'tools/test_m5_mode_selection.py', 'tools/finalize_m51.py']
manifest = {name: sha(ROOT / name) for name in sorted(set(base) | set(extras))}
(OUT / 'source-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
binaries = {str(p.relative_to(ROOT)): sha(p) for p in [
    ROOT / 'build/m51-root/Windows/bin/SvtAv2EncApp.exe',
    ROOT / 'build/m51-root/Windows/bin/svt_av2_enc_shared.dll']}
for compiler in ['GCC', 'Clang', 'SAN']:
    path = f'/root/svt-av2-m51/{compiler}/bin/SvtAv2EncApp'
    output = subprocess.check_output(['wsl', '-d', 'ARXChem-Ubuntu-22.04-WSL1', '--', 'sha256sum', path], text=True)
    binaries[path] = output.split()[0]
report = {'status': 'M5.1 DONE', 'milestone_status': 'M5 RUNNING; partition not implemented',
    'official_version': '0.0.4-m4', 'matrix_cases': 144, 'large_cases': 24,
    'frozen_m4_off_cases': 156, 'long_cases_per_platform': 6,
    'frames_per_long_case': 1000, 'mode_trace_cases': 12,
    'trace_y_rows': sum(x['rows'] for x in trace['cases']),
    'trace_uv_rows': sum(x['uv_rows'] for x in trace['cases']),
    'trace_inter_neighbours': sum(x['inter_neighbour_uses'] for x in trace['cases']),
    'source_files': len(manifest), 'm4_changed_files': [p for p in base if manifest[p] != base[p]],
    'source_manifest_sha256': sha(OUT / 'source-manifest.json'), 'binaries': binaries,
    'oracle_sha256': sha(oracle), 'spec_sha256': sha(spec), 'measurements': 'measurements/measurements.json'}
(OUT / 'final-verification.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report, indent=2))
