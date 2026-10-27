"""Run Windows FFmpeg binaries and compare with independent M4 reconstruction."""
from pathlib import Path
import hashlib
import json
import shutil
import struct
import subprocess
import sys

root = Path(__file__).resolve().parents[2]
out = root / 'dist/M4-ffmpeg-min'
proof = out / 'proof'
samples = out / 'samples'
proof.mkdir(exist_ok=True)
samples.mkdir(exist_ok=True)
ffmpeg = out / 'ffmpeg.exe'
ffprobe = out / 'ffprobe.exe'
app = root / 'build/m4-root/Windows/bin/Release/SvtAv2EncApp.exe'
oracle = root.parent / 'AVM-AV2-TEST/tools/bin/avmdec.exe'
report = {'cases': [], 'commands': [], 'negative_tests': []}
sys.path.insert(0, str(root / 'tools'))
from m4_stream import inspect_stream

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def run(args, name, ok=True):
    args = list(map(str, args))
    p = subprocess.run(args, capture_output=True, timeout=180)
    (proof / (name + '.log')).write_bytes(p.stdout + p.stderr)
    report['commands'].append({'argv': args, 'returncode': p.returncode, 'log': name + '.log'})
    if ok:
        assert p.returncode == 0, (name, p.stderr.decode(errors='replace'))
    else:
        assert p.returncode != 0, name
    return p.stdout

for fmt, pix, size in [('8bit420', 'yuv420p', 176*144*3//2),
                       ('10bit420', 'yuv420p10le', 176*144*3),
                       ('10bit444', 'yuv444p10le', 176*144*6)]:
    source = samples / (fmt + '.y4m')
    shutil.copy2(root / f'artifacts/m4-root/version-p-windows/{fmt}_176x144_17f_mixed/input.y4m', source)
    for mode in ['all-intra', 'low-delay-p']:
        name = fmt + '-' + mode
        ivf = proof / (name + '.ivf')
        decoded = proof / (name + '.yuv')
        native = proof / (name + '.av2')
        recon = proof / (name + '-recon.yuv')
        run([ffmpeg, '-y', '-i', source, '-c:v', 'libsvtav2', '-qp', '100',
             '-pred-structure', mode, '-pix_fmt', pix, '-f', 'ivf', ivf], name + '-encode')
        run([ffmpeg, '-y', '-c:v', 'libavm-av2', '-threads', '1', '-i', ivf,
             '-pix_fmt', pix, '-f', 'rawvideo', decoded], name + '-decode')
        run([app, '--input', source, '--output', native, '--export-recon', recon,
             '--pred-structure', mode], name + '-reference')
        data = ivf.read_bytes()
        assert data[:4] == b'DKIF' and data[8:12] == b'AV02'
        assert struct.unpack_from('<HHIIII', data, 12) == (176, 144, 25, 1, 17, 0)
        offset, packets, pts = 32, [], []
        while offset < len(data):
            length, timestamp = struct.unpack_from('<IQ', data, offset)
            offset += 12
            assert length > 0 and offset + length <= len(data)
            packets.append(data[offset:offset+length])
            pts.append(timestamp)
            offset += length
        assert offset == len(data) and len(packets) == 17 and pts == list(range(17))
        assert b''.join(packets) == native.read_bytes(), name + ': native stream mismatch'
        syntax = inspect_stream(native.read_bytes(), 17, mode == 'low-delay-p')
        assert decoded.stat().st_size == size*17
        assert decoded.read_bytes() == recon.read_bytes(), name + ': recon mismatch'
        oracle_raw = proof / (name + '-oracle.yuv')
        run([oracle, '--rawvideo', '-o', oracle_raw, native], name + '-oracle')
        assert oracle_raw.read_bytes() == decoded.read_bytes(), name + ': oracle mismatch'
        metadata = json.loads(run([ffprobe, '-v', 'error', '-count_frames', '-show_streams',
                                   '-show_packets', '-of', 'json', ivf], name + '-probe'))
        stream = metadata['streams'][0]
        assert stream['codec_name'] == 'av2' and stream['pix_fmt'] == pix
        assert int(stream['nb_read_frames']) == 17 and stream['time_base'] == '1/25'
        assert len(metadata['packets']) == 17
        report['cases'].append({'name': name, 'frames': 17, 'pix_fmt': pix,
            'ivf_sha256': sha(ivf), 'annex_b_sha256': sha(native),
            'decoded_sha256': sha(decoded), 'reconstruction_sha256': sha(recon),
            'oracle_sha256': sha(oracle_raw), 'decoded_bytes': decoded.stat().st_size,
            'stream_matches_independent_m4_app': True, 'reconstruction_exact': True,
            'oracle_exact': True, 'pts': pts, 'syntax': syntax, 'status': 'PASS'})
    # Y4M output must preserve the decoded samples, including 10-bit storage.
    y4m = proof / (fmt + '-decoded.y4m')
    run([ffmpeg, '-y', '-i', proof / (fmt + '-low-delay-p.ivf'),
         '-strict', '-1', '-f', 'yuv4mpegpipe', y4m], fmt + '-decode-y4m')
    with y4m.open('rb') as f:
        header = f.readline().decode('ascii').strip()
        raw = bytearray()
        for i in range(17):
            assert f.readline().startswith(b'FRAME')
            raw.extend(f.read(size))
        assert not f.read(1)
    assert raw == (proof / (fmt + '-low-delay-p-recon.yuv')).read_bytes()
    report.setdefault('y4m_tests', []).append({'format': fmt, 'header': header, 'status': 'PASS'})
    # Raw input, fractional frame rate and one-frame EOF exercise a separate input path.
    raw_input = proof / (fmt + '-one-frame-input.yuv')
    with source.open('rb') as f:
        f.readline()
        assert f.readline().startswith(b'FRAME')
        raw_input.write_bytes(f.read(size))
    raw_ivf = proof / (fmt + '-raw-fractional.ivf')
    run([ffmpeg, '-y', '-f', 'rawvideo', '-pix_fmt', pix, '-video_size', '176x144',
         '-framerate', '30000/1001', '-i', raw_input, '-c:v', 'libsvtav2',
         '-f', 'ivf', raw_ivf], fmt + '-encode-raw')
    data = raw_ivf.read_bytes()
    assert struct.unpack_from('<II', data, 16) == (30000, 1001)
    assert struct.unpack_from('<I', data, 24)[0] == 1
    run([ffmpeg, '-y', '-i', raw_ivf, '-f', 'rawvideo', proof / (fmt + '-raw-decoded.yuv')],
         fmt + '-decode-raw')
    assert (proof / (fmt + '-raw-decoded.yuv')).read_bytes() == (
        proof / (fmt + '-all-intra-recon.yuv')).read_bytes()[:size]
    report.setdefault('raw_input_tests', []).append({'format': fmt, 'frames': 1,
        'frame_rate': '30000/1001', 'status': 'PASS'})

source = samples / '8bit420.y4m'
for name, options in [('qp99', ['-qp', '99']), ('threads2', ['-threads', '2']),
                      ('bitrate', ['-b:v', '1M']), ('bframes', ['-bf', '1']),
                      ('badsize', ['-vf', 'scale=174:144']),
                      ('badgop', ['-g', '10'])]:
    run([ffmpeg, '-y', '-i', source, '-frames:v', '1', '-c:v', 'libsvtav2',
         *options, '-f', 'ivf', proof / (name + '-rejected.ivf')], name, ok=False)
    report['negative_tests'].append({'name': name, 'status': 'PASS'})
report['binaries'] = {p.name: {'sha256': sha(p), 'bytes': p.stat().st_size}
                      for p in [ffmpeg, ffprobe]}
report['oracle_sha256'] = sha(oracle)
report['status'] = 'PASS'
(proof / 'acceptance.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps({'status': report['status'], 'cases': len(report['cases']),
                  'negative_tests': len(report['negative_tests'])}))
