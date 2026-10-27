"""Failure-path checks for the M6 measurement contract."""
import json
from pathlib import Path
import tempfile
import unittest

from m6_batch import rd_trace, validate_frozen, expected_jobs
from m53_batch import bd_rate, digest, reusable


class BatchContract(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.log = self.root / 'encode.log'
        self.valid = 'rd frame=0 mode=1 candidates=84 forward=252 replay=1 D=123 Rq12=456 J=789 lambdaq8=512 full=1 partial=0 winner=42\n'

    def tearDown(self):
        self.temp.cleanup()

    def test_trace_valid_and_off(self):
        self.log.write_text(self.valid)
        self.assertEqual(rd_trace(self.log,1,'exhaustive')[0]['Rq12'],456)
        self.log.write_text('ordinary app log')
        self.assertEqual(rd_trace(self.log,1,'off'),[])
        self.assertEqual(rd_trace(self.log,1,'m5'),[])

    def test_trace_missing_or_incomplete(self):
        for text in ['',self.valid.replace(' Rq12=456','')]:
            self.log.write_text(text)
            with self.assertRaises(ValueError):
                rd_trace(self.log,1,'exhaustive')

    def test_trace_wrong_mode(self):
        self.log.write_text(self.valid)
        for mode in ['pruned','off','m5']:
            with self.assertRaises(ValueError):
                rd_trace(self.log,1,mode)

    def test_trace_frames_and_replay(self):
        for text in [self.valid*2,self.valid.replace('frame=0','frame=1'),self.valid.replace('replay=1','replay=0')]:
            self.log.write_text(text)
            with self.assertRaises(ValueError):
                rd_trace(self.log,1,'exhaustive')

    def test_bd_invalid_not_fabricated(self):
        valid = [(1,1),(2,2),(3,3),(4,4)]
        for invalid in [valid[:3],[(1,1),(1,2),(3,3),(4,4)],[(1,1),(2,3),(3,2),(4,4)],[(5,1),(6,2),(7,3),(8,4)]]:
            self.assertIsNone(bd_rate(valid,invalid)['percent'])

    def test_dependency_change_rejected(self):
        artifact = self.root / 'input'
        artifact.write_bytes(b'frozen')
        config = dict(sources=[dict(path=str(artifact),sha256=digest(artifact))],frozen_files=[],
                      app=None,baseline_app=None,decoder=None,ffmpeg=None,vmaf_model=None)
        validate_frozen(config)
        artifact.write_bytes(b'changed')
        with self.assertRaises(ValueError):
            validate_frozen(config)

    def test_resume_retained_hash_and_mode(self):
        stream = self.root / 'encode-0.av2'
        stream.write_bytes(b'av2')
        path = self.root / 'result.json'
        job = dict(source=0,qp=100,prediction='all-intra',partition=1,intra=1,rd_mode='pruned')
        row = dict(status='PASS',run_id='id',job=job,stream_sha256=digest(stream),
                   retained_artifacts=[dict(path=str(stream),sha256=digest(stream))])
        path.write_text(json.dumps(row))
        self.assertTrue(reusable(path,'id',job))
        self.assertFalse(reusable(path,'other',job))
        self.assertFalse(reusable(path,'id',dict(job,rd_mode='exhaustive')))
        stream.write_bytes(b'broken')
        self.assertFalse(reusable(path,'id',job))

    def test_job_domain_has_rd_dimension(self):
        config=dict(sources=[{}],qps=[1,255],predictions=['all-intra','low-delay-p'],tools=['00','11'],rd_modes=['m5','off','pruned'])
        jobs=expected_jobs(config)
        self.assertEqual(len(jobs),24)
        self.assertEqual({j['rd_mode'] for j in jobs},{'m5','off','pruned'})


if __name__ == '__main__':
    unittest.main()
