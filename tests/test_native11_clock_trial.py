"""Synthetic, temporary-directory checks for the Native11 clock trial."""

import hashlib
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
from contextlib import redirect_stdout

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import native11_clock_trial as trial


def digest(data):
    return hashlib.sha256(data).hexdigest()


class ClockTrialTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        self.core = root / "install" / "Data" / "A2FOExtensions.dll"
        self.config = root / "install" / "Mod" / "A2FORenderer.ini"
        self.other = root / "install" / "FleetOpsHook.dll"
        for path in (self.core, self.config, self.other):
            path.parent.mkdir(parents=True, exist_ok=True)
        self.core.write_bytes(b"old core")
        self.config.write_bytes(b"; caf\xe9\r\n[Native11]\r\nCaptureScene = 0 ; keep\r\nOther=7\r\n[Other]\r\nKeep=1\r\n")
        self.other.write_bytes(b"protected")
        self.candidate = root / "private" / "A2FOExtensions.dll"
        self.candidate.parent.mkdir()
        self.candidate.write_bytes(b"candidate core")
        self.receipt = root / "receipt.json"
        self.backup = root / "backup"
        self.refresh_receipt()

    def refresh_receipt(self):
        self.receipt.write_text(json.dumps({
            "core_dll": {"core_sha256": digest(self.candidate.read_bytes())},
            "protected_live_sha256_unchanged": {
                str(path): digest(path.read_bytes()) for path in (self.core, self.config, self.other)
            },
        }))

    def prepared(self):
        return trial.prepare(self.receipt, self.candidate, self.core, self.config)

    def test_dry_run_has_zero_effects_and_preserves_ini(self):
        before = {path: path.read_bytes() for path in (self.core, self.config, self.other, self.receipt)}
        result, originals, replacements = self.prepared()
        self.assertFalse(self.backup.exists())
        for path, data in before.items():
            self.assertEqual(path.read_bytes(), data)
        self.assertEqual(result["files"][0]["after_sha256"], digest(self.candidate.read_bytes()))
        self.assertEqual(result["files"][1]["after_sha256"], digest(replacements[1]))
        self.assertIn(b"; caf\xe9\r\n", replacements[1])
        self.assertIn(b"CaptureScene = 1 ; keep\r\n", replacements[1])
        self.assertIn(b"Other=7\r\n", replacements[1])
        self.assertTrue(replacements[1].endswith(b"[Other]\r\nKeep=1\r\n"))
        for key in trial.KEYS:
            self.assertEqual(replacements[1].lower().count(key.lower().encode() + b"="),
                             0 if key == "CaptureScene" else 1)
        self.assertEqual(originals[0], b"old core")

    def test_ambiguous_sections_and_duplicate_keys_refused(self):
        self.config.write_bytes(b"[Native11]\n[ native11 ]\n")
        self.refresh_receipt()
        with self.assertRaisesRegex(ValueError, "multiple"):
            self.prepared()
        self.config.write_bytes(b"[Native11]\nCaptureScene=0\ncapturescene=0\n")
        self.refresh_receipt()
        with self.assertRaisesRegex(ValueError, "duplicate"):
            self.prepared()

    def test_hash_tamper_refused(self):
        self.candidate.write_bytes(b"tamper")
        with self.assertRaisesRegex(ValueError, "candidate hash"):
            self.prepared()
        self.candidate.write_bytes(b"candidate core")
        self.other.write_bytes(b"changed")
        with self.assertRaisesRegex(ValueError, "protected file hash"):
            self.prepared()

    def test_cli_default_dry_run_and_running_game_refusal(self):
        args = ["--receipt", str(self.receipt), "--candidate", str(self.candidate),
                "--core-target", str(self.core), "--config-target", str(self.config)]
        out = io.StringIO()
        with redirect_stdout(out):
            self.assertEqual(trial.main(args), 0)
        self.assertEqual(json.loads(out.getvalue())["action"], "dry run")
        self.assertFalse(self.backup.exists())
        with patch.object(trial, "game_running", return_value=True):
            with self.assertRaisesRegex(ValueError, "Armada process"):
                trial.deploy(*self.prepared(), self.backup)
        self.assertFalse(self.backup.exists())

    def test_symlink_target_refused(self):
        actual = self.core.with_name("actual.dll")
        self.core.rename(actual)
        self.core.symlink_to(actual)
        with self.assertRaisesRegex(ValueError, "symlink"):
            self.prepared()

    def test_apply_backup_and_rollback(self):
        plan, originals, replacements = self.prepared()
        with patch.object(trial, "game_running", return_value=False):
            trial.deploy(plan, originals, replacements, self.backup)
            self.assertEqual(self.core.read_bytes(), replacements[0])
            self.assertEqual(self.config.read_bytes(), replacements[1])
            self.assertEqual((self.backup / "core.original").read_bytes(), originals[0])
            self.assertEqual((self.backup / "config.original").read_bytes(), originals[1])
            self.assertEqual(trial.rollback(self.backup)["action"], "rollback dry run")
            trial.rollback(self.backup, apply=True)
        self.assertEqual(self.core.read_bytes(), originals[0])
        self.assertEqual(self.config.read_bytes(), originals[1])

    def test_concurrent_edit_refuses_rollback(self):
        plan, originals, replacements = self.prepared()
        with patch.object(trial, "game_running", return_value=False):
            trial.deploy(plan, originals, replacements, self.backup)
            self.config.write_bytes(b"someone else's edit")
            with self.assertRaisesRegex(ValueError, "no longer matches"):
                trial.rollback(self.backup, apply=True)
        self.assertEqual(self.core.read_bytes(), replacements[0])

    def test_second_replace_failure_restores_first(self):
        plan, originals, replacements = self.prepared()
        real_replace = trial.atomic_replace
        calls = []

        def fail_second(path, data, mode):
            calls.append(path)
            if len(calls) == 2:
                raise OSError("injected second replace failure")
            return real_replace(path, data, mode)

        with patch.object(trial, "game_running", return_value=False), patch.object(trial, "atomic_replace", side_effect=fail_second):
            with self.assertRaisesRegex(OSError, "injected"):
                trial.deploy(plan, originals, replacements, self.backup)
        self.assertEqual(self.core.read_bytes(), originals[0])
        self.assertEqual(self.config.read_bytes(), originals[1])

    def test_rollback_retries_after_second_restore_failure(self):
        plan, originals, replacements = self.prepared()
        real_replace = trial.atomic_replace
        with patch.object(trial, "game_running", return_value=False):
            trial.deploy(plan, originals, replacements, self.backup)
            calls = []

            def fail_second(path, data, mode):
                calls.append(path)
                if len(calls) == 2:
                    raise OSError("injected rollback failure")
                return real_replace(path, data, mode)

            with patch.object(trial, "atomic_replace", side_effect=fail_second):
                with self.assertRaisesRegex(OSError, "injected rollback failure"):
                    trial.rollback(self.backup, apply=True)
            self.assertEqual(self.core.read_bytes(), originals[0])
            self.assertEqual(self.config.read_bytes(), replacements[1])
            trial.rollback(self.backup, apply=True)
            with patch.object(trial, "atomic_replace") as replace:
                trial.rollback(self.backup, apply=True)
                replace.assert_not_called()
        self.assertEqual(self.core.read_bytes(), originals[0])
        self.assertEqual(self.config.read_bytes(), originals[1])

    def test_missing_second_target_during_failed_deploy_still_restores_core(self):
        plan, originals, replacements = self.prepared()
        real_replace = trial.atomic_replace
        calls = []

        def remove_second(path, data, mode):
            calls.append(path)
            if len(calls) == 2:
                self.config.unlink()
                raise OSError("injected missing config")
            return real_replace(path, data, mode)

        with patch.object(trial, "game_running", return_value=False), patch.object(trial, "atomic_replace", side_effect=remove_second):
            with self.assertRaisesRegex(OSError, "incomplete recovery"):
                trial.deploy(plan, originals, replacements, self.backup)
        self.assertEqual(self.core.read_bytes(), originals[0])
        self.assertFalse(self.config.exists())
        self.assertTrue((self.backup / trial.MANIFEST).is_file())
        self.assertEqual((self.backup / "config.original").read_bytes(), originals[1])


if __name__ == "__main__":
    unittest.main()
