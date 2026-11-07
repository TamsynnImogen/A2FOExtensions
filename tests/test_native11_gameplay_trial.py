"""Synthetic tests for a gameplay copy; never invokes a game or Wine."""
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import native11_gameplay_trial as trial


def digest(data):
    return hashlib.sha256(data).hexdigest()


class GameplayTrialTests(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        root = Path(self.tmp.name)
        self.source = root / 'private' / 'Data'
        self.source.mkdir(parents=True)
        (self.source.parent / 'FleetOpsHook.dll').write_bytes(b'private sibling hook')
        self.target = root / 'gameplay'
        self.live = root / 'live'
        self.live.mkdir()
        self.critical = {}
        for name in trial.CRITICAL:
            path = self.source / name
            path.parent.mkdir(parents=True, exist_ok=True)
            data = b'[Native11]\r\nOffscreenWorld=1\r\n' if name.endswith('.ini') else name.encode()
            path.write_bytes(data)
            self.critical[name] = digest(data)
        (self.source / 'asset.bin').write_bytes(b'asset')
        (self.source / 'asset-link').symlink_to('asset.bin')
        self.candidate = root / 'A2FOExtensions.dll'
        self.helper = root / 'A2FONative11Renderer.dll'
        self.candidate.write_bytes(b'new core')
        self.helper.write_bytes(b'new helper')
        self.fake_exe = root / 'fake.exe'
        self.fake_input = root / 'fake-input.bin'
        self.fake_exe.write_bytes(b'checked runtime executable')
        self.fake_input.write_bytes(b'checked runtime input')
        protected = {}
        for n in range(7):
            path = self.live / f'protected{n}'
            path.write_bytes(f'protected{n}'.encode())
            protected[str(path)] = digest(path.read_bytes())
        self.receipt = root / 'gate.json'
        names = sorted(trial.REQUIRED_CASES) + [f'case-{n:03d}' for n in range(155)]
        self.gate = {'core_dll': {'core_sha256': digest(self.candidate.read_bytes()),
                                  'helper_sha256': digest(self.helper.read_bytes())},
                     'runtime_cases': [{'case': name, 'executable': self.fake_exe.name,
                                        'executable_sha256': digest(self.fake_exe.read_bytes()),
                                        'input_file_sha256': {str(self.fake_input): digest(self.fake_input.read_bytes())},
                                        'exit_code': 0} for name in names],
                     'protected_live_sha256_unchanged': protected}
        self.save_gate()

    def save_gate(self):
        self.receipt.write_text(json.dumps(self.gate))

    def prepare(self, apply=False):
        return trial.prepare(self.receipt, self.candidate, self.helper,
                             self.source, self.target, apply=apply)

    def test_dry_run_and_isolated_copy(self):
        dry = self.prepare()
        self.assertFalse(self.target.exists())
        self.assertFalse(dry['applied'])
        result = self.prepare(apply=True)
        self.assertTrue(result['applied'])
        self.assertIn(result['clone_method'], ('reflink', 'copy'))
        self.assertEqual(result['source_critical_before_sha256'], self.critical)
        self.assertEqual(result['source_critical_after_sha256'], self.critical)
        self.assertEqual(trial.sha(self.target / 'Data/A2FOExtensions.dll'), digest(b'new core'))
        self.assertEqual(trial.sha(self.target / 'Data/A2FONative11Renderer.dll'), digest(b'new helper'))
        self.assertEqual(trial.sha(self.target / 'FleetOpsHook.dll'), digest(b'private sibling hook'))
        self.assertEqual(trial.sha(self.source / 'A2FOExtensions.dll'), self.critical['A2FOExtensions.dll'])
        config = (self.target / 'Data/Mods/FO4 Nexus/A2FORenderer.ini').read_bytes()
        for key, value in trial.OPTIONS.items():
            self.assertIn((key + '=' + value).encode(), config)
        self.assertEqual(self.source.joinpath('Mods/FO4 Nexus/A2FORenderer.ini').read_bytes(),
                         b'[Native11]\r\nOffscreenWorld=1\r\n')
        self.assertTrue((self.target / 'Data/asset-link').is_symlink())
        self.assertTrue((self.target / 'Data/Native11Preview').is_dir())
        self.assertIn('flatpak run ru.linux_gaming.PortProton',
                      (self.target / 'Launch Game.sh').read_text())
        self.assertIn('Native11 Inspection.bat', (self.target / 'Launch Game.sh').read_text())
        batch = self.target / 'Native11 Inspection.bat'
        self.assertEqual(batch.read_bytes(), trial.USER_LAUNCH_BATCH)
        self.assertIn(b'/mod "FO4 Nexus"', batch.read_bytes())
        self.assertEqual(result['user_launch_batch_sha256'], trial.sha(batch))
        viewer_script = (self.target / 'View Renderer.sh').read_text()
        self.assertIn('--port 18711', viewer_script)
        self.assertIn(str(self.target / 'Inspector/native11_live_view.py'), viewer_script)
        self.assertNotIn(str(Path(trial.__file__).parent), viewer_script)
        copied_viewer = self.target / 'Inspector/native11_live_view.py'
        self.assertEqual(result['inspector_viewer_sha256'], trial.sha(copied_viewer))
        self.assertEqual(copied_viewer.read_bytes(),
                         Path(trial.__file__).with_name('native11_live_view.py').read_bytes())
        guide = (self.target / 'README.md').read_text()
        self.assertIn('http://127.0.0.1:18711/', guide)
        self.assertIn('`FO4 Nexus` selected', guide)
        self.assertIn('start a skirmish', guide)
        self.assertIn('shadows', guide)
        self.assertIn('Data/A2FORenderer.log', guide)
        self.assertTrue((self.target / 'gameplay-trial-receipt.json').is_file())
        self.assertNotEqual((self.source / 'asset.bin').stat().st_ino,
                            (self.target / 'Data/asset.bin').stat().st_ino)

    def test_gate_and_tamper_refused_before_copy(self):
        self.gate['runtime_cases'].pop()
        self.save_gate()
        with self.assertRaisesRegex(ValueError, '160'):
            self.prepare(apply=True)
        self.assertFalse(self.target.exists())
        self.gate['runtime_cases'].append({'exit_code': 0})
        self.save_gate()
        self.candidate.write_bytes(b'tampered')
        with self.assertRaisesRegex(ValueError, 'core_sha256'):
            self.prepare(apply=True)
        self.assertFalse(self.target.exists())

    def test_previous_private_preview_is_archived_in_copy(self):
        old = self.source / 'Native11Preview'
        old.mkdir()
        (old / 'status.json').write_bytes(b'old private run')
        result = self.prepare(apply=True)
        self.assertEqual((old / 'status.json').read_bytes(), b'old private run')
        self.assertEqual((self.target / 'Seed Preview/status.json').read_bytes(), b'old private run')
        self.assertEqual(list((self.target / 'Data/Native11Preview').iterdir()), [])
        self.assertIn('seed_preview_archive', result)

    def test_runtime_artifact_and_feature_gates(self):
        self.fake_exe.write_bytes(b'changed executable')
        with self.assertRaisesRegex(ValueError, 'runtime executable hash mismatch'):
            self.prepare(apply=True)
        self.fake_exe.write_bytes(b'checked runtime executable')
        self.fake_input.write_bytes(b'changed input')
        with self.assertRaisesRegex(ValueError, 'runtime input hash mismatch'):
            self.prepare(apply=True)
        self.fake_input.write_bytes(b'checked runtime input')
        self.gate['runtime_cases'][0]['case'] = 'replacement'
        self.save_gate()
        with self.assertRaisesRegex(ValueError, 'missing required runtime cases'):
            self.prepare(apply=True)
        self.gate['runtime_cases'][0]['case'] = sorted(trial.REQUIRED_CASES)[0]
        self.gate['runtime_cases'][1]['case'] = self.gate['runtime_cases'][0]['case']
        self.save_gate()
        with self.assertRaisesRegex(ValueError, 'duplicate runtime case'):
            self.prepare(apply=True)
        self.assertFalse(self.target.exists())

    def test_symlink_escape_and_repeat_refused(self):
        (self.source / 'bad-link').symlink_to(os.path.relpath(self.live / 'protected0', self.source))
        with self.assertRaisesRegex(ValueError, 'escapes private Data'):
            self.prepare(apply=True)
        self.assertFalse(self.target.exists())
        (self.source / 'bad-link').unlink()
        self.prepare(apply=True)
        with self.assertRaisesRegex(ValueError, 'target already exists'):
            self.prepare(apply=True)

    def test_protected_live_tamper_refused(self):
        (self.live / 'protected0').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'protected live hash mismatch'):
            self.prepare(apply=True)
        self.assertFalse(self.target.exists())

    def test_absolute_asset_link_and_symlink_target_refused(self):
        (self.source / 'absolute-link').symlink_to(self.source / 'asset.bin')
        with self.assertRaisesRegex(ValueError, 'absolute asset link refused'):
            self.prepare(apply=True)
        (self.source / 'absolute-link').unlink()
        self.target.symlink_to(self.live, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, 'symlink path refused'):
            self.prepare(apply=True)
        self.assertEqual(sorted(path.name for path in self.live.iterdir()),
                         [f'protected{n}' for n in range(7)])


if __name__ == '__main__':
    unittest.main()
