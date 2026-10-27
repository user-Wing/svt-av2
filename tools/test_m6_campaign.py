"""Fault injection for the detached M6 coordinator, with tiny child processes."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from m53_batch import digest


class CampaignTests(unittest.TestCase):
    def run_case(self, fail=False, corrupt=False):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            source = root / 'source.txt'
            source.write_text('frozen')
            result = root / 'result.json'
            child = [sys.executable, '-c', 'import json,sys;from pathlib import Path;Path(sys.argv[1]).write_text(json.dumps(dict(status="PASS")))', str(result)]
            config = dict(working_directory=str(root), frozen_files={str(source): digest(source)},
                stages=[dict(name='first', command=[sys.executable, '-c', 'raise SystemExit(7)'] if fail else child),
                        dict(name='second', command=child, summary=str(result))])
            path = root / 'campaign-config.json'
            path.write_text(json.dumps(config))
            (root / 'campaign-config.sha256').write_text(digest(path))
            if corrupt:
                source.write_text('changed')
            proc = subprocess.run([sys.executable, str(Path(__file__).with_name('m6_campaign.py')),
                                   '--config', str(path)], capture_output=True)
            state = json.loads((root / 'campaign-status.json').read_text())
            self.assertEqual(proc.returncode == 0, not (fail or corrupt))
            self.assertEqual(state['status'], 'FAIL' if fail or corrupt else 'PASS')
            self.assertEqual(result.exists(), not (fail or corrupt))
            if not fail and not corrupt:
                # A restart revalidates dependencies and executes stage commands;
                # each expensive batch command itself owns fingerprinted resume.
                again = subprocess.run([sys.executable, str(Path(__file__).with_name('m6_campaign.py')),
                                        '--config', str(path)], capture_output=True)
                self.assertEqual(again.returncode, 0)

    def test_complete_and_restart(self): self.run_case()
    def test_child_failure_stops_later_stages(self): self.run_case(fail=True)
    def test_changed_dependency_rejected(self): self.run_case(corrupt=True)


if __name__ == '__main__':
    unittest.main()
