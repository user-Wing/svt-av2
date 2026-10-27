"""Freeze the verified M5.3 candidate, then separately audit an unattended long campaign."""
import argparse
import json
from pathlib import Path
import re
import subprocess

from audit_m53_batch import audit
from m53_batch import atomic_json, digest

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'artifacts/m53-root'


def read(path):
    return json.loads(path.read_text(encoding='utf-8'))


def passed(name, count):
    rows = read(OUT / name / 'acceptance.json')
    assert len(rows) == count and all(r['status'] == 'PASS' for r in rows), name
    return rows


def wsl_sha(path):
    return subprocess.check_output(['wsl', '-d', 'ARXChem-Ubuntu-22.04-WSL1', '--', 'sha256sum', path], text=True).split()[0]


def candidate():
    for name, count in [('windows-release-ctest.log', 40), ('GCC-ctest.log', 39),
                        ('Clang-ctest.log', 39), ('SAN-ctest.log', 39)]:
        assert re.search(rf'100% tests passed(?:, 0 tests failed)? out of {count}\b',
                         (OUT / name).read_text()), name
    reports = {}
    for name, count in [('final-boundary360-r2', 360), ('allqp6120', 6120), ('scenes768-r2', 768),
                        ('measure768-r2', 768), ('large720-144', 144), ('large1080-144', 144)]:
        report = audit(OUT / name)
        assert report['jobs'] == count
        reports[name] = report
    assert reports['measure768-r2']['repeats'] == 3
    baseline = passed('boundary360', 360)
    for name in ['gcc360', 'clang360', 'san360']:
        rows = passed(name, 360)
        assert all(a['job'] == b['job'] and a['stream_sha256'] == b['stream_sha256'] and
                   a['recon_sha256'] == b['recon_sha256'] for a, b in zip(rows, baseline))
    passed('final-m52-qp100-576', 576)
    passed('m4-off156', 156)
    trace = read(ROOT / 'artifacts/m53-b/final-evidence.json')
    assert trace['pass'] and trace['boundary_cases'] == 360 and trace['boundary_frames'] == 1080
    assert trace['large_level_cases'] == 6 and trace['actual_large_level'] > 8191
    for path, sha in trace['old_diagnostic_hashes_unchanged'].items():
        assert digest(ROOT / path) == sha, path
    assert digest(Path(trace['diagnostic_binary'])) == trace['diagnostic_binary_sha256']
    diagnostic = ROOT / 'artifacts/m53-b/diagnostic-decoder'
    assert digest(diagnostic / 'source-manifest.json') == trace['diagnostic_source_manifest_sha256']
    for row in read(diagnostic / 'source-manifest.json'):
        assert digest(diagnostic / 'source' / row['path']) == row['sha256'], row['path']
    a_files = read(ROOT / 'artifacts/m53-a/source-manifest.json')['files']
    b_files = read(ROOT / 'artifacts/m53-b/source-manifest.json')
    for row in a_files:
        assert digest(ROOT / row['path']) == row['after_sha256'], row['path']
    for row in b_files:
        assert digest(ROOT / row['path']) == row['sha256'], row['path']
    for name in ['check_resume.json', 'check_pchip.json']:
        assert read(OUT / name)['status'] == 'PASS', name
    assert read(OUT / 'campaign-proof/campaign-status.json')['status'] == 'PASS'
    leak = (OUT / 'valgrind-m5-api.log').read_text()
    assert re.search(r'test_m5_api: \d+ checks, 0 failures\b', leak)
    assert 'in use at exit: 0 bytes in 0 blocks' in leak
    assert 'ERROR SUMMARY: 0 errors from 0 contexts' in leak
    old = ROOT / 'artifacts/m52-root/source-manifest.json'
    assert digest(old) == '957413e59c0f840c741b9bac42bd699df539e50052aa539e81faabc3a86cdf8d'
    assert digest(old) == digest(OUT / 'm52-baseline-source-manifest.json')
    oracle = ROOT.parent / 'AVM-AV2-TEST/tools/bin/avmdec.exe'
    spec = ROOT.parent / 'docs/spec/AV2_Spec_v1.0.0_20260528_38f28e7.pdf'
    assert digest(oracle) == trace['fixed_oracle_sha256']
    assert digest(spec) == trace['fixed_spec_sha256']
    for path, sha in read(ROOT / 'artifacts/m52-root/final-verification.json')['binaries'].items():
        assert (wsl_sha(path) if path.startswith('/root/') else digest(ROOT / path)) == sha, path
    paths = set(read(old)) | {r['path'] for r in a_files+b_files}
    paths |= {str(p.relative_to(ROOT)).replace('\\', '/') for p in (ROOT / 'tools').glob('m53*.py')}
    paths |= {'tools/audit_m53_batch.py', 'tools/test_m53_batch.py', 'tools/verify_m53_baselines.py',
              'tools/build_m53_linux.sh', 'tools/Start-M53Batch.ps1', 'tools/collect_m53.py'}
    manifest = {p: digest(ROOT / p) for p in sorted(paths)}
    atomic_json(OUT / 'candidate-source-manifest.json', manifest)
    binaries = {str(p.relative_to(ROOT)): digest(p) for p in [
        ROOT / 'build/m53-root/Windows-final/bin/SvtAv2EncApp.exe',
        ROOT / 'build/m53-root/Windows-final/bin/svt_av2_enc_shared.dll']}
    for kind in ['GCC', 'Clang', 'SAN']:
        path = f'/root/svt-av2-m53/{kind}/bin/SvtAv2EncApp'
        binaries[path] = wsl_sha(path)
    report = {'status': 'M5.3 DEVELOPMENT VERIFIED; LONG CAMPAIGN PENDING', 'official_version': '0.0.4-m4',
              'source_files': len(manifest), 'source_manifest_sha256': digest(OUT / 'candidate-source-manifest.json'),
              'binaries': binaries, 'batches': reports, 'trace_streams': 360, 'trace_frames': 1080,
              'large_level_streams': 6, 'm52_qp100_streams': 576, 'm4_off_cases': 156,
              'rate_control_policy': ['CQP', 'future CRF'], 'long_campaign': 'long-campaign-r1'}
    atomic_json(OUT / 'candidate-verification.json', report)
    return report


def long_campaign(path):
    frozen = read(OUT / 'candidate-verification.json')
    assert digest(OUT / 'candidate-source-manifest.json') == frozen['source_manifest_sha256']
    for name, sha in read(OUT / 'candidate-source-manifest.json').items():
        assert digest(ROOT / name) == sha, name
    assert read(path / 'campaign-status.json')['status'] == 'PASS'
    report = audit(path / 'results')
    meta = read(path / 'results/run-manifest.json')
    assert len(meta['qps']) >= 4 and meta['predictions'] == ['all-intra', 'low-delay-p']
    assert set(meta['tools']) == {'00', '01', '10', '11'}
    assert {(s['depth'], s['chroma']) for s in meta['sources']} == {(8, '420'), (10, '420'), (10, '444')}
    assert all(s['frames'] >= 1000 for s in meta['sources'])
    assert meta['app']['sha256'] == frozen['binaries']['build\\m53-root\\Windows-final\\bin\\SvtAv2EncApp.exe']
    baseline = read(path / 'results/acceptance.json')
    snapshots = read(path / 'linux-snapshots.json')
    for kind in ['GCC', 'Clang', 'SAN']:
        assert read(path / kind / 'summary.json')['status'] == 'PASS'
        assert snapshots[kind] == frozen['binaries'][f'/root/svt-av2-m53/{kind}/bin/SvtAv2EncApp']
        rows = read(path / kind / 'acceptance.json')
        assert len(rows) == len(baseline)
        for index, (row, ref) in enumerate(zip(rows, baseline)):
            assert row['status'] == 'PASS' and row['job'] == ref['job']
            assert row['stream_sha256'] == ref['stream_sha256'] == digest(path / kind / str(index) / 'output.av2')
            assert row['recon_sha256'] == ref['recon_sha256']
            assert digest(row['log']) == row['log_sha256']
    result = {'status': 'M5.3 LONG CAMPAIGN VERIFIED; OFFICIAL VERSION RELEASE PENDING',
              'campaign': str(path), 'windows_audit': report,
              'platforms': ['Windows', 'GCC', 'Clang', 'ASan/UBSan/LSan'],
              'source_manifest_sha256': frozen['source_manifest_sha256']}
    atomic_json(OUT / 'long-campaign-verification.json', result)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--campaign', type=Path, help='audit a completed long campaign after candidate freeze')
    args = parser.parse_args()
    print(json.dumps(long_campaign(args.campaign.resolve()) if args.campaign else candidate(), indent=2))


if __name__ == '__main__':
    main()
