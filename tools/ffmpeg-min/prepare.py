"""Copy the dirty AV2-enabled FFmpeg baseline without changing its checkout."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys

base = Path(sys.argv[1])
workspace = Path('/mnt/c/Private/ffmpeg-native')
source = workspace / 'ffmpeg-src'
dest = base / 'src-ffmpeg'
dest.mkdir(parents=True, exist_ok=True)
names = subprocess.check_output(['git', '-C', str(source), 'ls-files', '-z',
                                  '--cached', '--others', '--exclude-standard']).split(b'\0')
manifest = {}
for raw in names:
    if not raw:
        continue
    name = raw.decode()
    p = source / name
    if not p.is_file():
        continue
    q = dest / name
    q.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(p, q)
    data = p.read_bytes()
    manifest[name] = hashlib.sha256(data).hexdigest()
    if b'\0' not in data:
        q.write_bytes(data.replace(b'\r\n', b'\n'))
(base / 'ffmpeg-baseline.json').write_text(json.dumps(manifest, indent=2))
changes = {
    'configure': [
        ('  --enable-libavm', '  --enable-libsvtav2       enable SVT-AV2 M4 encoding [no]\n  --enable-libavm'),
        ('    libavm\n', '    libavm\n    libsvtav2\n'),
        ('libavm_av2_encoder_deps="libavm"', 'libavm_av2_encoder_deps="libavm"\nlibsvtav2_encoder_deps="libsvtav2"'),
        ('enabled libavm ', 'enabled libsvtav2 && require_pkg_config libsvtav2 SvtAv2 svt_av2.h svt_av2_enc_init_handle\nenabled libavm '),
        ('require_pkg_config libavm libavm avm/avm_codec.h', 'require_pkg_config libavm avm avm/avm_codec.h'),
    ],
    'libavcodec/Makefile': [
        ('OBJS-$(CONFIG_LIBAVM_AV2_ENCODER)', 'OBJS-$(CONFIG_LIBSVTAV2_ENCODER)          += libsvtav2.o\nOBJS-$(CONFIG_LIBAVM_AV2_ENCODER)'),
    ],
    'libavcodec/allcodecs.c': [
        ('extern const FFCodec ff_libavm_av2_encoder;', 'extern const FFCodec ff_libavm_av2_encoder;\nextern const FFCodec ff_libsvtav2_encoder;'),
    ],
    'libavcodec/libavmdec.c': [
        ('const AVPacket *pkt)', 'AVPacket *pkt)'),
        ('    av_image_copy(frame->data, frame->linesize, src, stride,\n'
         '                  avctx->pix_fmt, img->d_w, img->d_h);',
         '''    /* AVM v1.0.0 stores even 8-bit decoded samples in uint16_t. */
    if (img->bit_depth == 8 && (img->fmt & AVM_IMG_FMT_HIGHBITDEPTH)) {
        for (int p = 0; p < (img->monochrome ? 1 : 3); p++) {
            int w = AV_CEIL_RSHIFT((int)img->d_w, p ? img->x_chroma_shift : 0);
            int h = AV_CEIL_RSHIFT((int)img->d_h, p ? img->y_chroma_shift : 0);
            for (int y = 0; y < h; y++) {
                const uint16_t *row = (const uint16_t *)(src[p] + y * stride[p]);
                uint8_t *dst = frame->data[p] + y * frame->linesize[p];
                for (int x = 0; x < w; x++)
                    dst[x] = row[x];
            }
        }
    } else {
        av_image_copy(frame->data, frame->linesize, src, stride,
                      avctx->pix_fmt, img->d_w, img->d_h);
    }'''),
    ],
}
for name, replacements in changes.items():
    p = dest / name
    data = (source / name).read_text()
    for old, new in replacements:
        assert data.count(old) == 1, (name, old, data.count(old))
        data = data.replace(old, new)
    p.write_text(data)
shutil.copy2(Path(__file__).with_name('libsvtav2.c'), dest / 'libavcodec/libsvtav2.c')
