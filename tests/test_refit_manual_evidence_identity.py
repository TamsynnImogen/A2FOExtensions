"""Session-local package identity checks; headless temporary fixtures only."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1] / "tools"
sys.path.insert(0, str(TOOLS))
import refit_manual_evidence as evidence


class ManualEvidenceIdentity(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        (self.root / "Data").mkdir()
        (self.root / "prefix/drive_c/users/tamsynn/AppData/Local/armada2-refit/logs").mkdir(parents=True)
        self.input = self.root / "Data/input.bin"
        self.input.write_bytes(b"pinned bytes")
        self.write_manifest(["Data/input.bin"])

    def tearDown(self):
        self.temp.cleanup()

    def write_manifest(self, paths):
        critical = {name: hashlib.sha256((self.root / name).read_bytes()).hexdigest()
                    for name in paths if (self.root / name).exists()}
        (self.root / "package.json").write_text(json.dumps({"critical_sha256": critical}), encoding="utf-8")

    def begin(self):
        return evidence.begin(self.root, "vulkan", "manual")

    def finish(self, session):
        return evidence.finish(self.root, session, 0)

    def test_unchanged_exact_input_set_matches(self):
        session = self.begin()
        report = self.finish(session)
        identity = report["input_identity_after"]
        self.assertTrue(identity["all_inputs_match"])
        self.assertEqual([item["path"] for item in identity["inputs"]], ["Data/input.bin"])
        self.assertEqual(identity["inputs"][0]["size"], len(b"pinned bytes"))

    def test_binary_change_is_reported_without_rejecting_finish(self):
        session = self.begin()
        self.input.write_bytes(b"changed")
        report = self.finish(session)
        self.assertFalse(report["input_identity_after"]["all_inputs_match"])
        self.assertEqual(report["input_identity_after"]["inputs"][0]["status"], "changed")

    def test_manifest_list_change_does_not_replace_pinned_paths(self):
        session = self.begin()
        other = self.root / "Data/new.bin"
        other.write_bytes(b"new")
        self.write_manifest(["Data/new.bin"])
        report = self.finish(session)
        identity = report["input_identity_after"]
        self.assertEqual([item["path"] for item in identity["inputs"]], ["Data/input.bin"])
        self.assertFalse(identity["manifest_matches"])
        self.assertFalse(identity["all_inputs_match"])

    def test_missing_input_at_finish_is_reported(self):
        session = self.begin()
        self.input.unlink()
        report = self.finish(session)
        self.assertFalse(report["input_identity_after"]["all_inputs_match"])
        self.assertEqual(report["input_identity_after"]["inputs"][0]["status"], "missing")

    def test_missing_manifest_at_finish_still_archives_new_logs(self):
        session = self.begin()
        log = self.root / "Data/A2FOExtensions.log"
        log.write_text("fresh log")
        (self.root / "package.json").unlink()
        report = self.finish(session)
        self.assertEqual(report["input_identity_after"]["manifest_status"], "missing")
        self.assertFalse(report["input_identity_after"]["all_inputs_match"])
        self.assertEqual((session / "after/data/A2FOExtensions.log").read_text(), "fresh log")

    def test_symlinked_manifest_at_finish_still_archives_new_logs(self):
        session = self.begin()
        log = self.root / "Data/A2FOExtensions.log"
        log.write_text("fresh log")
        outside = Path(self.temp.name).parent / (Path(self.temp.name).name + "-manifest")
        outside.write_text("{}")
        try:
            (self.root / "package.json").unlink()
            (self.root / "package.json").symlink_to(outside)
            report = self.finish(session)
            self.assertEqual(report["input_identity_after"]["manifest_status"], "invalid")
            self.assertFalse(report["input_identity_after"]["all_inputs_match"])
            self.assertEqual((session / "after/data/A2FOExtensions.log").read_text(), "fresh log")
        finally:
            outside.unlink(missing_ok=True)

    def test_begin_reads_manifest_bytes_once_for_hash_and_parse(self):
        manifest = self.root / "package.json"
        original = manifest.read_bytes()
        real_read_bytes = Path.read_bytes
        reads = []

        def read_once(path):
            if path == manifest:
                reads.append(path)
                if len(reads) == 1:
                    return original
                return b'{"critical_sha256": {}}'
            return real_read_bytes(path)

        with mock.patch.object(Path, "read_bytes", read_once):
            identity = evidence._input_identity(self.root)
        self.assertEqual(reads, [manifest])
        self.assertEqual(identity["manifest_sha256"], hashlib.sha256(original).hexdigest())
        self.assertEqual([item["path"] for item in identity["inputs"]], ["Data/input.bin"])

    def test_escaping_or_symlink_input_refuses_before_logs_move(self):
        log = self.root / "Data/A2FOExtensions.log"
        log.write_text("preserve")
        outside = Path(self.temp.name).parent / (Path(self.temp.name).name + "-outside")
        outside.write_text("outside")
        try:
            (self.root / "Data/link.bin").symlink_to(outside)
            manifest = self.root / "package.json"
            manifest.write_text(json.dumps({"critical_sha256": {"Data/link.bin": hashlib.sha256(b"outside").hexdigest()}}))
            with self.assertRaises(ValueError):
                self.begin()
            self.assertTrue(log.exists())
            self.assertEqual(log.read_text(), "preserve")
        finally:
            outside.unlink(missing_ok=True)

    def test_parent_escape_refuses_before_logs_move(self):
        log = self.root / "Data/A2FOExtensions.log"
        log.write_text("preserve")
        (self.root / "package.json").write_text(json.dumps({"critical_sha256": {"../escape.bin": "0" * 64}}))
        with self.assertRaises(ValueError):
            self.begin()
        self.assertTrue(log.exists())


if __name__ == "__main__":
    unittest.main()
