"""Record build provenance and generate the Chinese verification report."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess

root = Path(__file__).resolve().parents[2]
workspace = root.parents[2]
out = root / 'dist/M4-ffmpeg-min'
report = json.loads((out / 'proof/acceptance.json').read_text())
assert report['status'] == 'PASS'

def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()

def wsl_file(name):
    return subprocess.check_output(['wsl', '-d', 'ARXChem-Ubuntu-22.04-WSL1', '--', 'cat',
                                    '/root/svt-av2-m4-ffmpeg-min/' + name])

frozen = json.loads((root / 'artifacts/m4-root/source-manifest.json').read_text())
assert all(sha(root / name) == digest for name, digest in frozen.items())
baseline = json.loads((out / 'build-info/ffmpeg-baseline.json').read_text())
assert all(sha(workspace / 'ffmpeg-src' / name) == digest for name, digest in baseline.items())
components = (out / 'build-info/config_components.h').read_text()
assert '#define CONFIG_LIBSVTAV2_ENCODER 1' in components
assert '#define CONFIG_LIBAVM_AV2_DECODER 1' in components
assert '#define CONFIG_LIBAVM_AV2_ENCODER 0' in components
archive = root / 'artifacts/m4-a/debug-oracle-source/source.tar.gz'
assert sha(archive) == '2e003352dab3a609c8ba2284ed7be606bf77d283615fb60ef8483c5290b8189e'
avm_hashes = {
    'av2/decoder/decodemv.c': '9e89fb98b64e33abf978baa8d8e074504f454d78848e482434cf24503626b37d',
    'av2/decoder/decodeframe.c': 'f9ffb64fe58b5be3dc9e10262cc65e85ebe5446287ae4946e3d05c634644032d',
}
assert all(hashlib.sha256(wsl_file('src-avm/' + name)).hexdigest() == digest
           for name, digest in avm_hashes.items())
provenance = {
    'ffmpeg_commit': subprocess.check_output(['git', '-C', str(workspace / 'ffmpeg-src'),
        'rev-parse', 'HEAD'], text=True).strip(),
    'svt_av2_version': '0.0.4-m4', 'svt_frozen_files_verified': len(frozen),
    'ffmpeg_original_baseline_files_verified': len(baseline),
    'svt_source_manifest_sha256': sha(root / 'artifacts/m4-root/source-manifest.json'),
    'avm_commit': '966a7d7cd6fcf60360caf5dc413b2aeeb65e144d',
    'avm_version': '1.0.0', 'avm_encoder_enabled': False,
    'avm_decoder_enabled': True, 'avm_original_decoder_sources': avm_hashes,
    'avm_source_archive_sha256': sha(archive), 'binaries': report['binaries'],
    'oracle_sha256': report['oracle_sha256'],
    'build_directory': '/root/svt-av2-m4-ffmpeg-min',
    'external_codec_linkage': 'static', 'platform': 'Windows x64 / UCRT',
    'ffmpeg_source_copy_newlines': 'CRLF normalized to LF for text files',
}
(out / 'build-info/provenance.json').write_text(json.dumps(provenance, indent=2), encoding='utf-8')
for name in ['build-avm/CMakeCache.txt', 'build-svt/CMakeCache.txt']:
    (out / 'build-info' / (name.split('/')[0] + '-CMakeCache.txt')).write_bytes(wsl_file(name))
shutil.copy2(root / 'artifacts/m4-root/source-manifest.json', out / 'build-info/svt-source-manifest.json')
shutil.copy2(root / 'artifacts/m4-ffmpeg-min/build.log', out / 'build-info/final-incremental-build.log')
for p in (root / 'tools/ffmpeg-min').iterdir():
    if p.is_file():
        shutil.copy2(p, out / 'source-patches' / p.name)
patch = subprocess.check_output(['git', '-C', str(workspace / 'ffmpeg-src'),
                                 'diff', '--binary', 'HEAD'])
(out / 'source-patches/av2-baseline.patch').write_bytes(patch)
for name in ['libavmdec.c', 'libavmenc.c']:
    dest = out / 'source-patches/baseline-untracked/libavcodec' / name
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(workspace / 'ffmpeg-src/libavcodec' / name, dest)
rows = '\n'.join(f"| {c['name']} | {c['frames']} | PASS | PASS | PASS |" for c in report['cases'])
lines = [
    '# M4 最小 FFmpeg 编解码证明', '',
    '2026-10-11，Windows 本机直接运行交付目录中的程序。完整结果为 `proof/acceptance.json`，状态 **PASS**。', '',
    '## 实测结果', '',
    '| 测试 | 帧数 | 编码→解码 | 内部重建逐字节一致 | 固定 AVM 解码逐字节一致 |',
    '|---|---:|---|---|---|', rows, '',
    '共 6 组、102 帧。三种格式均实际编码和解码。IVF 中逐包拼接的 AV2 payload 与独立 MSVC M4 `SvtAv2EncApp.exe` 输出完全相同；FFmpeg 解码、独立编码器内部重建、固定 `avmdec.exe` 解码的 RAW 逐字节相同。这里的一致性比较对象是有损编码后的内部重建，而非原始输入。', '',
    '每组均检查 IVF 的 DKIF/AV02 标记、176×144 尺寸、25/1 帧率、17 个包及 PTS 0–16；ffprobe 解码计数为 17。独立语法检查确认 All-Intra 关键帧结构，以及 Low-Delay P 首关键帧、后续 P 帧和引用关系。', '',
    '另外三种格式各测试一次 17 帧 Y4M 解码输出，去除头信息后与内部重建逐字节一致；各测试一次 RAW 输入、30000/1001 帧率、单帧 EOF，编码后解码与独立重建第一帧一致。', '',
    '非法 QP99、threads2、非零目标码率、B 帧、174×144 非 8 倍数尺寸、周期 GOP10 共 6 项均以非零返回码拒绝，日志在 proof/。', '',
    '## 二进制和依赖', '',
]
for name, info in report['binaries'].items():
    lines.extend([f"- `{name}`：{info['bytes']} 字节，SHA256 `{info['sha256']}`。"])
lines.extend(['',
    '外部 SVT/AVM 库静态链接，仅导入 Windows 系统 DLL/UCRT。无 SVT/AVM、LLVM/MinGW 运行时 DLL 依赖。PE 导入证据为 build-info/ffmpeg-pe.txt 和 ffprobe-pe.txt。', '',
    '## 来源', '',
    f"FFmpeg 基线 commit `{provenance['ffmpeg_commit']}` 加本地既有 AV2 修改及本次最小接入补丁。SVT-AV2 `0.0.4-m4` 的 {len(frozen)} 个冻结源文件 SHA 均复核一致。AVM 固定 v1.0.0 commit `{provenance['avm_commit']}`，仅编入解码器；两个曾用于诊断的 decoder 源文件已验证为原始归档版本。详情见 build-info/provenance.json。", '',
    '复验入口：`demo.ps1`。完整仓库验收脚本：`tools/ffmpeg-min/verify.py`；源码副本、构建配置和补丁随包留存。', '',
])
(out / 'PROOF.md').write_text('\n'.join(lines), encoding='utf-8')
manifest = '\n'.join(f'{sha(p)}  {p.relative_to(out).as_posix()}'
    for p in sorted(out.rglob('*')) if p.is_file() and p.name != 'SHA256SUMS.txt'
    and 'demo-output' not in p.parts)
(out / 'SHA256SUMS.txt').write_text(manifest + '\n', encoding='utf-8')
print(json.dumps({'status': 'PASS', 'source_files_verified': len(frozen),
                  'binaries': report['binaries']}, indent=2))
