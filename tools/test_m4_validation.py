"""Negative/independent checks for M4 validation tools."""
import tempfile
import unittest
from pathlib import Path
import numpy as np
from m4_model import predict, motion_estimate
from m4_stream import inspect_stream, obus
from verify_m4 import compare_rate, quality, write_frames, frame_bytes


class M4ValidationTests(unittest.TestCase):
    def test_normative_filter_matches_half_phase_arithmetic(self):
        rng = np.random.default_rng(84137)
        for peak in (255, 1023):
            ref = rng.integers(0, peak+1, size=(7, 9), dtype=np.int64)
            for mx, my in ((0, 0), (-1, 3), (1, -3), (-8, 8), (7, -7)):
                bx, px = divmod(mx, 2)
                by, py = divmod(my, 2)
                actual = predict(ref, (mx, my), 1)
                expected = np.empty_like(actual)
                for y in range(7):
                    for x in range(9):
                        points = [ref[min(6, max(0, y+by+dy)), min(8, max(0, x+bx+dx))]
                                  for dy in range(py+1) for dx in range(px+1)]
                        expected[y, x] = (sum(points) + len(points)//2) // len(points)
                np.testing.assert_array_equal(actual, expected)
                # One wrong edge/phase/MV must fail the pixel oracle.
                wrong = actual.copy()
                wrong[0, 0] ^= 1
                self.assertFalse(np.array_equal(wrong, expected))

    def test_motion_sign_and_tie(self):
        rng = np.random.default_rng(64)
        reference = rng.integers(0, 1024, size=(48, 48), dtype=np.int64)
        current = predict(reference, (-4, 2), 0)
        self.assertEqual(motion_estimate(current, reference, 16, 16, 8, 8), (-4, 2, 0))
        uniform = np.full((16, 16), 512, dtype=np.int64)
        self.assertEqual(motion_estimate(uniform, uniform, 0, 0, 8, 8), (0, 0, 0))
        self.assertFalse(np.array_equal(predict(reference, (4, -2), 0), current))

    def test_fixed_m3_stream_and_corruption(self):
        # Frozen M3 8-bit420 64x64 production stream, not writer-generated expected.
        data = bytes.fromhex("01081304800a00557ffc000001d0000001801b7000401310f001b2010000414646b363f1811d2e080298")
        result = inspect_stream(data, 1, False)
        self.assertEqual(result["sequence"]["width"], 64)
        for bad in (data[:-1], b"\xff"*8, data+b"\x00", b"\x00"):
            with self.assertRaises(ValueError):
                list(obus(bad))
        with self.assertRaises(ValueError):
            inspect_stream(data, 2, False)
        with self.assertRaises(ValueError):
            inspect_stream(data, 1, True)

    def test_rate_and_quality_reject_bad_claims(self):
        ai = {"bytes": 1000, "quality": {"mse": 10}}
        inter = {"bytes": 400, "quality": {"mse": 10}}
        self.assertTrue(compare_rate(ai, inter))
        self.assertFalse(compare_rate(ai, {"bytes": 600, "quality": {"mse": 10}}))
        self.assertFalse(compare_rate(ai, {"bytes": 100, "quality": {"mse": 20}}))
        with tempfile.TemporaryDirectory() as tmp:
            source, recon = Path(tmp)/"input.y4m", Path(tmp)/"recon.yuv"
            planes = [np.arange(64).reshape(8, 8), np.full((4, 4), 1023), np.zeros((4, 4))]
            write_frames(source, 8, 8, 10, "420", [planes])
            original = frame_bytes(planes, 10)
            recon.write_bytes(original)
            self.assertEqual(quality(source, recon, 8, 8, 1, 10, "420")["mse"], 0)
            for data in (original[:-2], original+b"\x00\x00"):
                recon.write_bytes(data)
                with self.assertRaises(RuntimeError):
                    quality(source, recon, 8, 8, 1, 10, "420")
            changed = [p.copy() for p in planes]
            changed[1][0, 0] = 1022
            recon.write_bytes(frame_bytes(changed, 10))
            self.assertGreater(quality(source, recon, 8, 8, 1, 10, "420")["mse"], 0)


if __name__ == "__main__":
    unittest.main()
