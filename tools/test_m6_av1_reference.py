"""AV1 reference frame-count, plane metric and resume integrity checks."""
import json
from pathlib import Path
import tempfile
import unittest

import numpy as np

from m53_batch import atomic_json
from m6_av1_reference import frozen, quality, reusable, validate


class ReferenceContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.meta = dict(width=8, height=8, depth=10, chroma='420', fps='25:1', frames=2)

    def tearDown(self):
        self.temp.cleanup()

    def y4m(self, name, value=0, count=2):
        path = self.root / name
        path.write_bytes(b'YUV4MPEG2 W8 H8 F25:1 Ip C420p10\n' +
            (b'FRAME\n' + np.full(96, value, dtype='<u2').tobytes()) * count)
        return path

    def test_exact_and_known_error(self):
        src = self.y4m('src.y4m')
        self.assertEqual(quality(src, src, self.meta)['psnr_yuv_db'], [None] * 3)
        row = quality(src, self.y4m('rec.y4m', 2), self.meta)
        self.assertEqual(row['mse_yuv'], [4.0] * 3)
        self.assertAlmostEqual(row['psnr_yuv_db'][0], 54.176912760963575)

    def test_short_extra_and_format_fail(self):
        src = self.y4m('src.y4m')
        for count in [1, 3]:
            with self.assertRaises(ValueError):
                quality(src, self.y4m('rec.y4m', count=count), self.meta)
        with self.assertRaises(ValueError):
            quality(src, src, dict(self.meta, fps='24:1'))

    def test_resume_requires_artifact_hash(self):
        artifact = self.root / 'vmaf.json'
        artifact.write_text('{}')
        result = self.root / 'result.json'
        atomic_json(result, dict(status='PASS', run_id='run', retained_artifacts=[frozen(artifact)]))
        self.assertTrue(reusable(result, 'run'))
        self.assertFalse(reusable(result, 'different'))
        artifact.write_text('{"changed":true}')
        self.assertFalse(reusable(result, 'run'))

    def test_dependency_mutation_rejected(self):
        source = self.y4m('src.y4m')
        config = dict(frozen_files=[frozen(source)])
        validate(config)
        source.write_bytes(b'changed')
        with self.assertRaises(ValueError):
            validate(config)


if __name__ == '__main__':
    unittest.main()
