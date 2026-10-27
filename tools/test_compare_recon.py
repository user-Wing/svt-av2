#!/usr/bin/env python3
"""Self-tests for tools/compare_recon.py (stdlib unittest, no third-party deps).

These tests build their own synthetic raw files; they never depend on a real
encoder or on T005. Run from any working directory:

    python -m unittest discover -s svt-av2/tools -p "test_*.py" -v

The tool is exercised as a subprocess so the exit-code contract is tested end
to end, not just the in-process API.
"""

import json
import os
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
TOOL = os.path.join(HERE, "compare_recon.py")

W, H, FRAMES = 4, 2, 2
CW, CH = W // 2, H // 2
FRAME = W * H + 2 * CW * CH  # 8 + 2*2 = 12
TOTAL = FRAME * FRAMES


def make_frame(seed):
    return bytes((seed + i) & 0xFF for i in range(FRAME))


class CompareBase(unittest.TestCase):
    def setUp(self):
        self._tmp = tempfile.TemporaryDirectory()
        self.tmp = self._tmp.name

    def tearDown(self):
        self._tmp.cleanup()

    def write(self, name, data):
        path = os.path.join(self.tmp, name)
        with open(path, "wb") as f:
            f.write(data)
        return path

    def run_tool(self, *args):
        return subprocess.run(
            [sys.executable, TOOL] + list(args),
            capture_output=True, text=True,
        )

    def run_json(self, *args):
        proc = self.run_tool(*(list(args) + ["--json"]))
        try:
            payload = json.loads(proc.stdout)
        except json.JSONDecodeError:
            payload = None
        return proc, payload

    def compare(self, a, b, width=W, height=H, frames=FRAMES, extra=()):
        return self.run_tool(
            "--a", a, "--b", b,
            "--width", str(width), "--height", str(height),
            "--frames", str(frames), *extra,
        )

    def compare_json(self, a, b, width=W, height=H, frames=FRAMES, extra=()):
        return self.run_json(
            "--a", a, "--b", b,
            "--width", str(width), "--height", str(height),
            "--frames", str(frames), *extra,
        )


class TestHappyPath(CompareBase):
    def test_identical(self):
        a = self.write("a.raw", make_frame(10) + make_frame(20))
        b = self.write("b.raw", make_frame(10) + make_frame(20))
        proc, payload = self.compare_json(a, b)
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertTrue(payload["match"])
        self.assertEqual(payload["status"], "match")
        self.assertEqual(payload["mismatch_counts"]["total"], 0)
        self.assertEqual(payload["expected_bytes"], TOTAL)


class TestPerPlane(CompareBase):
    def _single_byte(self, plane_index):
        a_data = bytearray(make_frame(0) + make_frame(0))
        b_data = bytearray(a_data)
        off = {0: 0, 1: W * H, 2: W * H + CW * CH}[plane_index]
        b_data[off] ^= 0xFF
        a = self.write("a.raw", bytes(a_data))
        b = self.write("b.raw", bytes(b_data))
        proc, payload = self.compare_json(a, b)
        self.assertEqual(proc.returncode, 1)
        self.assertFalse(payload["match"])
        m = payload["first_mismatch"]
        self.assertEqual(m["frame"], 0)
        self.assertEqual(m["plane"], ("Y", "U", "V")[plane_index])
        self.assertEqual(m["a_offset"], off)
        self.assertEqual(m["expected"], a_data[off])
        self.assertEqual(m["actual"], b_data[off])

    def test_single_byte_Y(self):
        self._single_byte(0)

    def test_single_byte_U(self):
        self._single_byte(1)

    def test_single_byte_V(self):
        self._single_byte(2)

    def test_uv_coordinates(self):
        # 4x4 luma -> 2x2 chroma; V byte index 3 -> x=1, y=1.
        w, h = 4, 4
        cw, ch = w // 2, h // 2
        frame = w * h + 2 * cw * ch
        a_data = bytearray((i & 0xFF for i in range(frame)))
        b_data = bytearray(a_data)
        v_off = w * h + cw * ch + 3
        b_data[v_off] ^= 0x01
        a = self.write("a.raw", bytes(a_data))
        b = self.write("b.raw", bytes(b_data))
        _, payload = self.compare_json(a, b, width=w, height=h, frames=1)
        m = payload["first_mismatch"]
        self.assertEqual(m["plane"], "V")
        self.assertEqual((m["x"], m["y"]), (1, 1))


class TestFrames(CompareBase):
    def test_first_frame_differs(self):
        frames = [make_frame(0)] * 4
        a = self.write("a.raw", b"".join(frames))
        mod = [bytearray(f) for f in frames]
        mod[0][0] ^= 0xFF
        b = self.write("b.raw", b"".join(bytes(f) for f in mod))
        _, payload = self.compare_json(a, b, frames=4)
        self.assertEqual(payload["first_mismatch"]["frame"], 0)

    def test_last_frame_differs(self):
        frames = [make_frame(0)] * 4
        a = self.write("a.raw", b"".join(frames))
        mod = [bytearray(f) for f in frames]
        mod[-1][0] ^= 0xFF
        b = self.write("b.raw", b"".join(bytes(f) for f in mod))
        _, payload = self.compare_json(a, b, frames=4)
        self.assertEqual(payload["first_mismatch"]["frame"], 3)

    def test_multiple_differences_counted(self):
        a_data = bytearray(make_frame(0))
        b_data = bytearray(a_data)
        b_data[0] ^= 0x01
        b_data[5] ^= 0x02
        b_data[W * H] ^= 0x04  # U
        a = self.write("a.raw", bytes(a_data))
        b = self.write("b.raw", bytes(b_data))
        _, payload = self.compare_json(a, b, frames=1)
        c = payload["mismatch_counts"]
        self.assertEqual((c["Y"], c["U"], c["V"], c["total"]), (2, 1, 0, 3))


class TestLengths(CompareBase):
    def test_one_side_truncated(self):
        a = self.write("a.raw", make_frame(0))
        b = self.write("b.raw", make_frame(0)[:-1])
        proc, payload = self.compare_json(a, b, frames=1)
        self.assertEqual(proc.returncode, 3)
        self.assertEqual(payload["status"], "length_error")
        self.assertNotEqual(payload["a_bytes"], payload["b_bytes"])

    def test_one_side_extra(self):
        a = self.write("a.raw", make_frame(0))
        b = self.write("b.raw", make_frame(0) + b"\x00")
        self.assertEqual(self.compare(a, b, frames=1).returncode, 3)

    def test_both_sides_common_truncation_fails(self):
        data = make_frame(0)[:-1]
        a = self.write("a.raw", data)
        b = self.write("b.raw", data)
        proc, payload = self.compare_json(a, b, frames=1)
        self.assertEqual(proc.returncode, 3)
        self.assertEqual(payload["sha256"]["a"], payload["sha256"]["b"])
        self.assertEqual(payload["status"], "length_error")

    def test_empty_files(self):
        a = self.write("a.raw", b"")
        b = self.write("b.raw", b"")
        self.assertEqual(self.compare(a, b, frames=1).returncode, 3)

    def test_wrong_frame_count(self):
        a = self.write("a.raw", make_frame(0))
        b = self.write("b.raw", make_frame(0))
        self.assertEqual(self.compare(a, b, frames=2).returncode, 3)


class TestArgErrors(CompareBase):
    def _bad_dims(self, w, h):
        a = self.write("a.raw", b"\x00" * 64)
        self.assertEqual(self.compare(a, a, width=w, height=h).returncode, 2)

    def test_odd_width(self):
        self._bad_dims(3, 2)

    def test_odd_height(self):
        self._bad_dims(4, 3)

    def test_zero_width(self):
        self._bad_dims(0, 2)

    def test_zero_height(self):
        self._bad_dims(4, 0)

    def test_unsupported_bit_depth(self):
        a = self.write("a.raw", make_frame(0))
        self.assertEqual(self.compare(a, a, extra=["--bit-depth", "12"]).returncode, 2)

    def test_unsupported_chroma(self):
        a = self.write("a.raw", make_frame(0))
        self.assertEqual(self.compare(a, a, extra=["--chroma", "444"]).returncode, 2)

    def test_missing_required_arg(self):
        a = self.write("a.raw", make_frame(0))
        proc = self.run_tool("--a", a, "--width", str(W), "--height", str(H),
                             "--frames", "1")
        self.assertEqual(proc.returncode, 2)


class TestFileErrors(CompareBase):
    def test_missing_path(self):
        a = self.write("a.raw", make_frame(0))
        missing = os.path.join(self.tmp, "nope.raw")
        self.assertEqual(self.compare(a, missing).returncode, 3)


class TestPortability(CompareBase):
    def test_paths_with_spaces_and_other_cwd(self):
        spaced = os.path.join(self.tmp, "dir with spaces")
        os.makedirs(spaced)
        a = self.write(os.path.join("dir with spaces", "a a.raw"), make_frame(0))
        b = self.write(os.path.join("dir with spaces", "b b.raw"), make_frame(0))
        proc = subprocess.run(
            [sys.executable, TOOL, "--a", a, "--b", b, "--width", str(W),
             "--height", str(H), "--frames", "1", "--json"],
            capture_output=True, text=True, cwd=self.tmp,
        )
        self.assertEqual(proc.returncode, 0, proc.stderr)
        self.assertTrue(json.loads(proc.stdout)["match"])

    def test_text_mode_reports_first_mismatch(self):
        a_data = bytearray(make_frame(0))
        b_data = bytearray(a_data)
        b_data[3] ^= 0xFF
        a = self.write("a.raw", bytes(a_data))
        b = self.write("b.raw", bytes(b_data))
        proc = self.compare(a, b, frames=1)
        self.assertEqual(proc.returncode, 1)
        self.assertIn("MISMATCH", proc.stdout)
        self.assertIn("frame=0", proc.stdout)
        self.assertIn("plane=Y", proc.stdout)
        self.assertIn("offset=3", proc.stdout)

    def test_explicit_text_matches_default(self):
        a = self.write("a.raw", make_frame(0))
        proc = self.compare(a, a, frames=1, extra=["--text"])
        self.assertEqual(proc.returncode, 0)
        self.assertIn("MATCH", proc.stdout)
        self.assertNotIn("{", proc.stdout)

    def test_json_and_text_are_mutually_exclusive(self):
        a = self.write("a.raw", make_frame(0))
        proc = self.compare(a, a, frames=1, extra=["--json", "--text"])
        self.assertEqual(proc.returncode, 2)


if __name__ == "__main__":
    unittest.main(verbosity=2)
