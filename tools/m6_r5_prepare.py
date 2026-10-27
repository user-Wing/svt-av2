"""Freeze the preselected native 24-frame sequences without decoding or scaling."""
import argparse
from fractions import Fraction
import hashlib
import json
from pathlib import Path


CAMPAIGN = Path('C:/Private/ffmpeg-native-m6-tests/campaign-local-r2-20261017')
MEDIA = Path('C:/Private/ffmpeg-native-m6-local-media/local')
START, COUNT = 48, 24
FRAME_BYTES = 1920 * 1080 * 3


def sha(path):
    value = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(8 * 1024 * 1024), b''):
            value.update(chunk)
    return value.hexdigest()


def guard():
    if json.loads((CAMPAIGN / 'campaign-status.json').read_text())['stage'] == 'isolated-speed':
        raise RuntimeError('isolated-speed active; no new computation')


def prepare(out):
    guard()
    out.mkdir(parents=True, exist_ok=False)
    sources = []
    for scene in ['2D', '3D', 'Real']:
        guard()
        original = MEDIA / (scene + '-yuv420p10le.y4m')
        provenance = json.loads(original.with_suffix('.json').read_text())
        selected = out / (scene + '-frames48-71.y4m')
        full_hash, entries = hashlib.sha256(), []
        with original.open('rb') as stream, selected.open('wb') as target:
            header = stream.readline()
            if not all(token in header.split() for token in [b'W1920', b'H1080', b'F24000:1001', b'C420p10']):
                raise ValueError('source format or frame rate differs')
            full_hash.update(header)
            target.write(header)
            index = 0
            while True:
                marker = stream.readline()
                if not marker:
                    break
                if not marker.startswith(b'FRAME'):
                    raise ValueError('invalid frame marker')
                full_hash.update(marker)
                offset = stream.tell()
                payload = stream.read(FRAME_BYTES)
                if len(payload) != FRAME_BYTES:
                    raise ValueError('truncated source frame')
                full_hash.update(payload)
                if START <= index < START + COUNT:
                    target.write(b'FRAME\n' + payload)
                    entries.append(dict(source_index=index, source_payload_offset=offset,
                                        payload_sha256=hashlib.sha256(payload).hexdigest()))
                index += 1
        if full_hash.hexdigest() != provenance['sha256'] or len(entries) != COUNT:
            raise ValueError('full frozen source hash/count differs')
        # Re-open both files and independently compare each extracted payload.
        with original.open('rb') as source, selected.open('rb') as target:
            target.readline()
            for entry in entries:
                if target.readline() != b'FRAME\n':
                    raise ValueError('extracted frame marker differs')
                payload = target.read(FRAME_BYTES)
                source.seek(entry['source_payload_offset'])
                if payload != source.read(FRAME_BYTES) or hashlib.sha256(payload).hexdigest() != entry['payload_sha256']:
                    raise ValueError('extracted payload differs from indexed source frame')
            if target.read(1):
                raise ValueError('extra extracted data')
        sources.append(dict(scene=scene, path=str(selected), sha256=sha(selected), width=1920,
            height=1080, depth=10, chroma='420', fps='24000:1001', frames=COUNT,
            start_index=START, end_index=START + COUNT - 1,
            normalized_start_seconds=str(Fraction(START * 1001, 24000)),
            duration_seconds=str(Fraction(COUNT * 1001, 24000)),
            source_y4m=dict(path=str(original), sha256=full_hash.hexdigest(), frames=index),
            original_h264=dict(path=provenance['source']['path'], sha256=provenance['source']['sha256']),
            provenance_sha256=sha(original.with_suffix('.json')), frame_payloads=entries,
            verification='full source SHA matches frozen provenance; independently reopened and compared each indexed payload'))
    baseline = Path('C:/Private/ffmpeg-native-m6-tests/optimization-r4-20261018/root/WindowsV2/bin/SvtAv2EncApp.exe')
    manifest = dict(status='PASS', selection='all scenes start at nearest frame to 2s: round(2*24000/1001)=48; no content/result selection',
        qps=[80, 100, 120, 140], prediction='low-delay-p', intra='adaptive', partition='adaptive',
        rd_mode='pruned', coeff_opt='off', sources=sources,
        baseline=dict(path=str(baseline), sha256=sha(baseline)),
        timing=dict(qp=100, jobs=1, order='ABBAAB', pairs=['AB', 'BA', 'AB'],
                    repetitions_per_variant=3, frames_per_run=COUNT,
                    note='encoding only, no decoder/VMAF/build concurrent in new-work window; frozen campaign remains running; not whole-machine isolated'))
    (out / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    print('PASS:', len(sources), 'sources;', COUNT, 'frames each;', sha(out / 'manifest.json'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument('--out', type=Path, required=True)
    prepare(parser.parse_args().out)
