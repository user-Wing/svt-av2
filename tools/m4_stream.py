"""Independent reader for the frozen M4 Annex-B header subset.

AV2 spec 5.2, 5.4, 5.18; decoder decodeframe.c 8025-8086,
8170-8280, 8312-8396, 8525-8563. This is not a tile entropy
decoder; full syntax validity and reconstructed samples use fixed avmdec.
"""
class Bits:
    def __init__(self, data):
        self.data, self.offset = data, 0

    def read(self, count):
        if self.offset + count > len(self.data) * 8:
            raise ValueError("truncated header")
        value = 0
        for _ in range(count):
            value = (value << 1) | ((self.data[self.offset // 8] >> (7 - self.offset % 8)) & 1)
            self.offset += 1
        return value

    def uvlc(self):
        zeros = 0
        while self.read(1) == 0:
            zeros += 1
            if zeros > 31:
                raise ValueError("UVLC overflow")
        return ((1 << zeros) - 1) + self.read(zeros)

    def expect(self, count, value, name):
        if self.read(count) != value:
            raise ValueError("unsupported or incorrect " + name)


def obus(data):
    offset = 0
    while offset < len(data):
        start, size, shift = offset, 0, 0
        while True:
            if offset >= len(data) or shift > 28:
                raise ValueError("invalid/truncated LEB128")
            byte = data[offset]
            offset += 1
            size |= (byte & 127) << shift
            if not byte & 128:
                break
            shift += 7
        if size < 1 or offset + size > len(data):
            raise ValueError("invalid/truncated OBU length")
        header = data[offset]
        if header & 131:
            raise ValueError("extension/layers outside M4 subset")
        yield (header >> 2) & 31, data[offset+1:offset+size], start
        offset += size


def sequence(payload):
    b = Bits(payload)
    if b.uvlc() != 0:
        raise ValueError("nonzero sequence id")
    profile = b.read(5)
    b.expect(1, 0, "single_picture_header_flag")
    level = b.read(5)
    if level >= 4:
        b.expect(1, 0, "tier")
    chroma, bd = b.uvlc(), b.uvlc()
    if (profile, chroma, bd) not in ((0, 0, 0), (0, 0, 1), (4, 2, 0)):
        raise ValueError("unsupported profile/format/depth")
    b.expect(3, 0, "seq_lcr_id")
    b.expect(1, 0, "still_picture")
    b.expect(5, 0, "temporal/embedded layers")
    b.expect(1, 1, "monotonic output")
    wb, hb = b.read(4)+1, b.read(4)+1
    width, height = b.read(wb)+1, b.read(hb)+1
    b.expect(3, 0, "crop/display-delay/decoder-model")
    b.expect(5, 0, "SB/SDP/extended partition")
    b.expect(2, 0, "segmentation")
    b.expect(8, 0, "intra tools")
    b.expect(6, 0, "motion modes/compound/ref_frame_mvs")
    hint_bits = b.read(4) + 1
    b.expect(1, 0, "refmvbank")
    b.expect(1, 1, "disabled DRL reorder")
    explicit_map = b.read(1)
    explicit_count = b.read(1)
    slots = b.read(4)+1 if explicit_count else 8
    b.expect(3, 0, "long term frame id")
    # Restricted ns(5)=0 and ns(3)=0, each followed by no per-frame override.
    b.expect(2, 0, "maximum DRL minus minimum")
    b.expect(1, 0, "frame DRL override")
    b.expect(1, 0, "maximum IBC DRL minus minimum")
    b.expect(1, 0, "frame IBC DRL override")
    b.expect(17, 0, "compound/TIP/trajectory/BAWP/CWP/refinement/MVD/global tools")
    b.expect(2, 3, "screen-content/integer-MV selectable")
    b.expect(10, 0, "transform/TCQ/parity tools")
    b.expect(2, 3, "average per-tile CDF")
    b.expect(1, 0, "separate UV delta Q")
    b.expect(1, 1, "equal AC/DC Q")
    b.expect(5, 23, "base UV delta Q")
    b.expect(1, 0, "UV AC delta Q enabled")
    b.expect(12, 0, "filters/tile/film-grain/extension tools")
    return {"width": width, "height": height, "bit_depth": 10 if bd == 0 else 8,
            "chroma": "444" if chroma == 2 else "420", "slots": slots,
            "hint_bits": hint_bits, "explicit_map": explicit_map}


def inspect_stream(data, expected_frames, inter=True):
    seq, frames, delimiters = None, [], 0
    for kind, payload, offset in obus(data):
        if kind == 2:
            if payload:
                raise ValueError("nonempty temporal delimiter")
            delimiters += 1
        elif kind == 1:
            if seq is not None or frames:
                raise ValueError("duplicate/late sequence header")
            seq = sequence(payload)
            if inter and (seq["slots"] != 1 or not seq["explicit_map"]):
                raise ValueError("expected single-slot explicit reference map")
        elif kind in (4, 7):
            if seq is None:
                raise ValueError("frame before sequence header")
            index = len(frames)
            if kind != (4 if not inter or index == 0 else 7):
                raise ValueError("unexpected frame OBU type")
            b = Bits(payload)
            b.expect(1, 1, "is_first_tile_group")
            if b.uvlc() != 0 or b.uvlc() != 0:
                raise ValueError("nonzero multi-frame/sequence header id")
            if kind == 7:
                b.expect(1, 1, "INTER_FRAME")
            b.expect(1, 1, "immediate output")
            b.expect(1, 0, "size override")
            hint = b.read(seq["hint_bits"])
            if hint != index % (1 << seq["hint_bits"]):
                raise ValueError("incorrect order hint")
            info = {"index": index, "obu": kind, "order_hint": hint, "offset": offset}
            if kind == 7:
                signal_primary = b.read(1)
                b.expect(1, 1, "cross frame context disabled")
                primary = b.read(3) if signal_primary else None
                # With cross-frame context disabled, primary can be absent.
                if primary not in (None, 7):
                    raise ValueError("unexpected inherited entropy reference")
                b.expect(seq["slots"], 1, "refresh slot0")
                b.expect(1, 1, "explicit reference map")
                b.expect(3, 1, "one reference")
                b.expect(1, 1, "screen content enabled")
                b.expect(1, 1, "integer MV")
                b.expect(1, 0, "intrabc")
                b.expect(1, 0, "fixed interpolation filter")
                b.expect(2, 3, "BILINEAR")
                info.update(reference_slot=0, reference_index=index-1, refresh=1, filter=3)
            else:
                b.expect(2, 0, "key screen-content/intrabc")
            b.expect(1, 1, "CDF updates disabled")
            b.expect(1, 1, "uniform tile spacing")
            if seq["width"] > 64:
                b.expect(1, 0, "single tile column")
            if seq["height"] > 64:
                b.expect(1, 0, "single tile row")
            b.expect(8 if seq["bit_depth"] == 8 else 9,
                     100 if seq["bit_depth"] == 8 else 148, "QP mapping")
            b.expect(6, 0, "segmentation/QM/delta-Q/deblocking/tx-mode")
            if kind == 7:
                b.expect(1, 0, "single reference mode")
                b.expect(1, 0, "skip mode flag")
            b.expect(2, 2, "reduced transform set")
            padding = (-b.offset) % 8
            b.expect(padding, 0, "frame byte alignment")
            info["entropy_offset"] = b.offset // 8
            frames.append(info)
        else:
            raise ValueError("unexpected OBU type in M4 stream")
    if len(frames) != expected_frames or delimiters != expected_frames:
        raise ValueError("frame/delimiter count mismatch")
    return {"sequence": seq, "frames": frames}
