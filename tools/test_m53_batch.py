"""Failure, resume and independent metric checks for long-running M5.3 jobs."""
import json
import math
import os
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import sys
import subprocess
import tempfile
import unittest
from unittest import mock

import numpy as np
from audit_m53_batch import audit
from m53_batch import (atomic_json, bd_rate, compare_quality, digest, execute,
                       inspect_source, reusable, run_job, run_lock, validate_frozen)


class BatchTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def fixture(self, depth):
        source, recon, decoded = [self.root / n for n in ['source.y4m', 'recon.yuv', 'decoded.y4m']]
        token = '420jpeg' if depth == 8 else '420p10'
        hdr = f'YUV4MPEG2 W8 H8 F25:1 Ip C{token}\n'.encode()
        dtype = 'u1' if depth == 8 else '<u2'
        original = np.full(96, 100, dtype=dtype).tobytes()
        actual = np.full(96, 98, dtype=dtype).tobytes()
        source.write_bytes(hdr + b'FRAME\n' + original)
        decoded.write_bytes(hdr + b'FRAME\n' + actual)
        recon.write_bytes(actual)
        return source, recon, decoded, inspect_source(source)

    def test_exact_mse_and_bit_depth(self):
        for depth in [8, 10]:
            result = compare_quality(*self.fixture(depth))
            self.assertEqual(result['mse_yuv'], [4, 4, 4])
            self.assertAlmostEqual(result['psnr_yuv_db'][0], 10*math.log10(((1 << depth)-1)**2/4))
            self.assertEqual(len(result['frame_sha256']), 1)

    def test_decoder_mismatch_and_trailing_data(self):
        source, recon, decoded, meta = self.fixture(10)
        decoded.write_bytes(decoded.read_bytes()[:-1])
        with self.assertRaises(ValueError):
            compare_quality(source, recon, decoded, meta)
        source, recon, decoded, meta = self.fixture(8)
        recon.write_bytes(recon.read_bytes() + b'0')
        with self.assertRaises(ValueError):
            compare_quality(source, recon, decoded, meta)

    def test_truncated_or_empty_source(self):
        source, _, _, _ = self.fixture(10)
        source.write_bytes(source.read_bytes()[:-1])
        with self.assertRaises(ValueError):
            inspect_source(source)
        source.write_bytes(b'YUV4MPEG2 W8 H8 C420jpeg\n')
        with self.assertRaises(ValueError):
            inspect_source(source)

    def test_known_bd_rate_and_invalid_curves(self):
        reference = [(20, 100), (30, 200), (40, 400), (50, 800)]
        result = bd_rate(reference, [(q, r*0.85) for q, r in reference])
        self.assertAlmostEqual(result['percent'], -15, places=8)
        self.assertIsNone(bd_rate(reference[:3], reference)['percent'])
        self.assertIsNone(bd_rate(reference, [(q+40, r) for q, r in reference])['percent'])
        self.assertIsNone(bd_rate(reference, [(20, 100), (30, 90), (40, 400), (50, 800)])['percent'])
        self.assertIsNone(bd_rate(reference, [(20, 100), (30, 200), (40, 400), (50, float('nan'))])['percent'])

    def test_resume_requires_matching_complete_artifact(self):
        artifact, result = self.root / 'encode-0.av2', self.root / 'result.json'
        artifact.write_bytes(b'original')
        record = {'status': 'PASS', 'run_id': 'frozen', 'job': {'qp': 100},
                  'stream_sha256': digest(artifact),
                  'retained_artifacts': [{'path': str(artifact), 'sha256': digest(artifact)}]}
        atomic_json(result, record)
        self.assertTrue(reusable(result, 'frozen', {'qp': 100}))
        self.assertFalse(reusable(result, 'changed', {'qp': 100}))
        self.assertFalse(reusable(result, 'frozen', {'qp': 101}))
        artifact.write_bytes(b'corrupted')
        self.assertFalse(reusable(result, 'frozen', {'qp': 100}))

        artifact.unlink()
        self.assertFalse(reusable(result, 'frozen', {'qp': 100}))
        record['retained_artifacts'] = []
        atomic_json(result, record)
        self.assertFalse(reusable(result, 'frozen', {'qp': 100}))
        result.write_text('{incomplete')
        self.assertFalse(reusable(result, 'frozen', {'qp': 100}))

    def test_saturated_vmaf_curve_does_not_explode(self):
        reference = [(94.181453, 145790), (92.48902, 58347), (75.566123, 18233), (16.784819, 3345)]
        tested = [(99.843543, 160460), (99.838586, 62624), (82.957291, 18555), (20.990127, 3278)]
        result = bd_rate(reference, tested)
        self.assertTrue(math.isfinite(result['percent']))
        self.assertTrue(-100 < result['percent'] < 100)

    def test_cleanup_failure_is_not_pass(self):
        source, recon, decoded, meta = self.fixture(8)
        raw, y4m = recon.read_bytes(), decoded.read_bytes()
        def fake_execute(command, log, timeout, cwd):
            log.write_text('fixture command')
            if '--export-recon' in command:
                Path(command[command.index('--output')+1]).write_bytes(b'stream')
                Path(command[command.index('--export-recon')+1]).write_bytes(raw)
            else:
                Path(command[command.index('-o')+1]).write_bytes(y4m)
            return {'seconds': 1.0, 'exit': 0}
        config = {'sources': [meta], 'app': {'path': 'fixture'}, 'decoder': {'path': 'fixture'},
                  'repeats': 1, 'ffmpeg': None}
        job = {'source': 0, 'qp': 100, 'prediction': 'all-intra', 'partition': 0, 'intra': 0}
        with mock.patch('m53_batch.execute', side_effect=fake_execute), mock.patch.object(
                Path, 'unlink', side_effect=PermissionError('cleanup denied')):
            result = run_job(job, config, self.root, 'run', 5)
        self.assertEqual(result['status'], 'FAIL')
        self.assertIn('cleanup denied', result['error'])

    def test_one_coordinator_at_a_time(self):
        path = self.root / 'run.lock'
        with run_lock(path):
            with self.assertRaises(OSError):
                with run_lock(path):
                    self.fail('second coordinator acquired lock')
        with run_lock(path):
            pass

    def test_changed_dependency_cannot_pass(self):
        source, _, _, meta = self.fixture(8)
        config = {'sources': [meta], 'app': None, 'decoder': None, 'ffmpeg': None, 'vmaf_model': None}
        validate_frozen(config)
        source.write_bytes(source.read_bytes() + b'changed')
        with self.assertRaises(ValueError):
            validate_frozen(config)

    def test_running_summary_never_passes_audit(self):
        manifest = self.root / 'run-manifest.json'
        atomic_json(manifest, {})
        atomic_json(self.root / 'summary.json', {'status': 'RUNNING', 'run_id': digest(manifest)})
        atomic_json(self.root / 'acceptance.json', [{'status': 'PASS'}])
        with self.assertRaises(AssertionError):
            audit(self.root)

    def test_failed_and_timed_out_command_cannot_pass(self):
        log = self.root / 'command.log'
        with self.assertRaises(RuntimeError):
            execute([sys.executable, '-c', 'raise SystemExit(7)'], log, 5, self.root)
        with self.assertRaises(TimeoutError):
            execute([sys.executable, '-c', 'import time; time.sleep(10)'], log, 0.1, self.root)

    def test_coarse_monotonic_clock_does_not_make_zero_duration(self):
        with mock.patch('m53_batch.time.monotonic', return_value=100.0):
            result = execute([sys.executable, '-c', 'pass'], self.root / 'fast.log', 5, self.root)
        self.assertGreater(result['seconds'], 0)

    @unittest.skipUnless(os.name == 'nt', 'Windows memory counter ABI')
    def test_concurrent_windows_memory_sampling(self):
        from verify_m4 import memory_snapshot
        interval = sys.getswitchinterval()
        sys.setswitchinterval(0.000001)
        with subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(30)']) as child:
            try:
                def sample_many(_):
                    for _ in range(2000):
                        sample = memory_snapshot(child)
                        self.assertIsNotNone(sample)
                        self.assertGreater(sample['peak_rss_bytes'], 0)
                with ThreadPoolExecutor(max_workers=8) as pool:
                    list(pool.map(sample_many, range(8)))
            finally:
                sys.setswitchinterval(interval)
                child.terminate()
                child.wait()


if __name__ == '__main__':
    unittest.main()
