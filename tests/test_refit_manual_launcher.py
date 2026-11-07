"""Safety regressions; no Wine, desktop, device access or game launch."""
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
import refit_manual_launcher as launcher


class ManualLauncherSafety(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        for name in ('Data', 'prefix', 'home', 'logs', 'run'):
            (self.root / name).mkdir()
        dummy = self.root / 'dummy'; dummy.write_text('test')
        sha = hashlib.sha256(dummy.read_bytes()).hexdigest()
        self.data = {'schema': 'a2fo-manual-test-v1', 'package': str(self.root),
            'critical_sha256': {}, 'wine': str(dummy), 'wineserver': str(dummy),
            'wine_sha256': sha, 'wineserver_sha256': sha,
            'nvidia_icd': str(dummy), 'nvidia_icd_sha256': sha}
        (self.root / 'package.json').write_text(json.dumps(self.data))

    def tearDown(self):
        self.temp.cleanup()

    def test_gpu_and_session_outputs_refuse_symlinks(self):
        outside = self.root / 'preserved'; outside.write_text('unchanged')
        for relative in ('logs/gpu-preflight.log', 'logs/gpu-preflight.json', 'run/active-session.json'):
            target = self.root / relative; target.symlink_to(outside)
            with self.assertRaises(ValueError):
                launcher.write_text_atomic(target, 'must not be written')
            self.assertEqual(outside.read_text(), 'unchanged')

    def test_lock_cannot_follow_link(self):
        target = self.root / 'preserved'; target.write_text('unchanged')
        (self.root / 'run/session.lock').symlink_to(target)
        with patch.object(sys, 'argv', ['test', '--package', str(self.root), '--probe-gpu']), \
             patch.object(launcher.subprocess, 'run') as run:
            with self.assertRaises(OSError):
                launcher.main()
            run.assert_not_called()
        self.assertEqual(target.read_text(), 'unchanged')

    def test_stop_remains_available_with_changed_test_inputs(self):
        with patch.object(sys, 'argv', ['test', '--package', str(self.root), '--stop']), \
             patch.object(launcher, 'check', side_effect=AssertionError('must not hash binaries')), \
             patch.object(launcher, 'stop_session') as stop:
            launcher.main()
            stop.assert_called_once_with(self.root)

    def test_wrong_command_line_is_never_signalled(self):
        (self.root / 'run/active-session.json').write_text(json.dumps({
            'package': str(self.root), 'wrapper_pid': os.getpid(), 'status': 'running',
            'process_start': launcher.process_start(os.getpid())}))
        with patch.object(launcher.signal, 'pidfd_send_signal') as send:
            with self.assertRaises(ValueError):
                launcher.stop_session(self.root)
            send.assert_not_called()

    def test_hardware_environment_does_not_inherit_fixture_or_software_flags(self):
        contaminated = {'A2FO_REFIT_HEADLESS_SCENE': '1', 'A2FO_REFIT_HEADLESS_MATERIAL': '1',
                        'LIBGL_ALWAYS_SOFTWARE': '1', 'GALLIUM_DRIVER': 'llvmpipe',
                        'VK_ICD_FILENAMES': '/software.json', 'DBUS_SESSION_BUS_ADDRESS': 'private'}
        with patch.dict(os.environ, contaminated):
            env = launcher.environment(self.data)
        for name in contaminated:
            if name != 'VK_ICD_FILENAMES':
                self.assertNotIn(name, env)
        self.assertEqual(env['VK_ICD_FILENAMES'], self.data['nvidia_icd'])
        self.assertEqual(env['WINEPREFIX'], '/mnt/prefix')

    def test_desktop_authority_fallback_and_missing_override(self):
        auth = self.root / '.Xauthority'; auth.write_text('synthetic test data')
        with patch.dict(os.environ, {'XAUTHORITY': ''}), patch.object(Path, 'home', return_value=self.root):
            self.assertEqual(launcher.desktop_authority(), str(auth))
        with patch.dict(os.environ, {'XAUTHORITY': str(self.root / 'missing')}):
            with self.assertRaises(ValueError):
                launcher.desktop_authority()

    def test_abstract_xwayland_retains_desktop_network_namespace(self):
        with patch.dict(os.environ, {'DISPLAY': ':97'}), \
             patch.object(Path, 'is_socket', return_value=False), \
             patch.object(launcher, 'abstract_x11_listener', return_value=True), \
             patch.object(launcher, 'desktop_authority', return_value=None):
            command = launcher.sandbox(self.root, True)
        self.assertNotIn('--unshare-net', command)
        self.assertIn('--unshare-pid', command)
        self.assertNotIn('/dev/input', command)

    def test_filesystem_x11_preserves_network_isolation(self):
        def socket(path):
            return str(path) == '/tmp/.X11-unix/X97'
        with patch.dict(os.environ, {'DISPLAY': ':97'}), \
             patch.object(Path, 'is_socket', socket), \
             patch.object(launcher, 'desktop_authority', return_value=None):
            command = launcher.sandbox(self.root, True)
        self.assertIn('--unshare-net', command)
        self.assertIn('/tmp/.X11-unix/X97', command)

    def test_missing_desktop_endpoint_fails_before_launch(self):
        with patch.dict(os.environ, {'DISPLAY': ':97'}), \
             patch.object(Path, 'is_socket', return_value=False), \
             patch.object(launcher, 'abstract_x11_listener', return_value=False):
            with self.assertRaises(ValueError):
                launcher.desktop_transport()


if __name__ == '__main__':
    unittest.main()
