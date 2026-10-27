#!/usr/bin/env python3
"""Independent negative/positive cases for the M3 acceptance tools."""
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

from verify_m3 import unpack_y4m, write_input


class Validation(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def compare(self, a, b, chroma="420", frames=1, bit_depth=10):
        paths = [self.root / "a.raw", self.root / "b.raw"]
        for path, data in zip(paths, (a, b)):
            path.write_bytes(data)
        proc = subprocess.run([sys.executable, str(Path(__file__).with_name("compare_recon.py")),
                               "--a", str(paths[0]), "--b", str(paths[1]), "--width", "8",
                               "--height", "8", "--frames", str(frames), "--chroma", chroma,
                               "--bit-depth", str(bit_depth), "--json"], capture_output=True, text=True)
        return proc.returncode, json.loads(proc.stdout)

    def raw(self, chroma="420"):
        count = 96 if chroma == "420" else 192
        return struct.pack("<" + "H" * count, *([0, 1, 255, 256, 511, 512, 1022, 1023] * (count // 8)))

    def test_roundtrip_and_low_bits(self):
        for chroma in ("420", "444"):
            with self.subTest(chroma=chroma):
                data = self.raw(chroma)
                self.assertEqual(self.compare(data, data, chroma)[0], 0)
                # U sample at x=2,y=1; flip only its low bit.
                cw = 4 if chroma == "420" else 8
                sample = 64 + cw + 2
                changed = bytearray(data)
                changed[sample * 2] ^= 1
                code, result = self.compare(data, changed, chroma)
                self.assertEqual(code, 1)
                m = result["first_mismatch"]
                self.assertEqual((m["plane"], m["x"], m["y"], m["a_offset"]), ("U", 2, 1, sample * 2))
                self.assertEqual(result["mismatch_counts"]["total"], 1)
                self.assertEqual(m["expected"] ^ m["actual"], 1)

    def test_sample_count_not_byte_count(self):
        data = bytearray(self.raw())
        struct.pack_into("<H", data, 0, 513)
        reference = bytearray(data)
        struct.pack_into("<H", reference, 0, 255)
        code, result = self.compare(reference, data)
        self.assertEqual(code, 1)
        self.assertEqual(result["mismatch_counts"]["total"], 1)
        self.assertEqual(result["first_mismatch"]["actual"], 513)

    def test_range_endian_size_format_and_count(self):
        raw = self.raw()
        for a, b, chroma, frames, depth, expected in (
            (raw, raw[:-1], "420", 1, 10, 3),
            (raw, raw + b"xx", "420", 1, 10, 3),
            (raw, raw, "420", 2, 10, 3),
            (raw, raw, "444", 1, 10, 3),
            (raw, raw, "420", 1, 8, 3),
            (raw, raw, "444", 1, 8, 2),
            (b"\xff\xff" * 96, b"\xff\xff" * 96, "420", 1, 10, 3),
            (b"\x02\x00" * 96, b"\x00\x02" * 96, "420", 1, 10, 1),
        ):
            with self.subTest(chroma=chroma, frames=frames, depth=depth, expected=expected):
                self.assertEqual(self.compare(a, b, chroma, frames, depth)[0], expected)

    def test_uv_swap_and_frame_order(self):
        y, u, v = b"\x00\x02" * 64, b"\x01\x02" * 16, b"\x02\x02" * 16
        frame = y + u + v
        swapped = y + v + u
        self.assertEqual(self.compare(frame, swapped)[0], 1)
        self.assertEqual(self.compare(frame + swapped, swapped + frame, frames=2)[0], 1)

    def test_decoder_owned_metadata_and_payload(self):
        for chroma in ("420", "444"):
            data = self.raw(chroma)
            good = f"YUV4MPEG2 W8 H8 F25:1 Ip C{chroma}p10\n".encode() + b"FRAME\n" + data
            path, raw = self.root / "decoder.y4m", self.root / "decoder.raw"
            path.write_bytes(good)
            header, hashes = unpack_y4m(path, raw, 8, 8, 1, 10, chroma)
            self.assertEqual(raw.read_bytes(), data)
            self.assertEqual(len(hashes), 1)
            for bad in (good.replace(b"W8 H8", b"W4 H16"), good.replace(b"p10", b"p12"),
                        good.replace(b"FRAME", b"BROKE"), good[:-1], good + b"junk",
                        good.replace(b"p10", b"")):
                path.write_bytes(bad)
                with self.assertRaises(RuntimeError):
                    unpack_y4m(path, raw, 8, 8, 1, 10, chroma)
            path.write_bytes(good)
            with self.assertRaises(RuntimeError):
                unpack_y4m(path, raw, 8, 8, 2, 10, chroma)
            with self.assertRaises(RuntimeError):
                unpack_y4m(path, raw, 8, 8, 1, 10, "444" if chroma == "420" else "420")

    def test_generated_input_contains_extremes(self):
        for chroma in ("420", "444"):
            source, raw = self.root / "input.y4m", self.root / "raw.yuv"
            write_input(source, 64, 64, 1, 10, chroma)
            unpack_y4m(source, raw, 64, 64, 1, 10, chroma)
            values = set(struct.unpack("<" + "H" * (raw.stat().st_size // 2), raw.read_bytes()))
            self.assertTrue({0, 1, 255, 256, 511, 512, 1022, 1023} <= values)


if __name__ == "__main__":
    unittest.main(verbosity=2)
