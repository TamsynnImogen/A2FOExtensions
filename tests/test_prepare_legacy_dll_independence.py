"""Synthetic, headless checks for the offline legacy DLL staging tool."""

import json
from collections import namedtuple
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import prepare_legacy_dll_independence as stage


class StageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(dir=stage.REPO / "build")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.source = self.root / "install"
        self.data = self.source / "Data"
        self.data.mkdir(parents=True)
        self.put("FleetOpsHook.dll", b"hook")
        self.put("Data/ArmadaL.exe", b"armada")
        self.put("Data/Win2kDisableTaskSwitch.dll", b"old-proxy")
        self.put("Data/Win2kDisableTaskSwitch.original.dll", b"original")
        self.put("Data/nested/DxVer.DLL", b"dxver")
        self.put("Data/nested/IMAGEHLP.dll", b"imagehlp")
        self.put("Data/config.ini", b"config")
        self.put("Data/Mods/Parent/ArmadaL.exe", b"parent-armada")
        (self.data / "empty").mkdir()
        self.proxy = self.root / "proxy.dll"
        self.proxy.write_bytes(b"new-proxy")
        self.patched = self.root / "patched.exe"
        self.patched.write_bytes(b"patched")
        self.output = self.root / "candidate"

    def put(self, relative, data):
        path = self.source / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def test_full_copy_replacement_quarantine_and_source_unchanged(self):
        before = stage.inventory(self.source)
        result = stage.prepare(self.source, self.output, self.proxy, self.patched)
        receipt = json.loads((self.root / "candidate.receipt.json").read_text())
        retired = self.root / "candidate.retired"
        self.assertEqual(result, receipt)
        self.assertEqual(receipt["status"], "prepared_dependency_validation_required")
        self.assertFalse(receipt["ready_for_manual_launch"])
        self.assertEqual(receipt["validation_required"], [
            "paired proxy/core contract", "all selected executable imports",
            "runtime provider parity", "startup lifecycle", "manual game acceptance"])
        self.assertEqual(receipt["source_sha256_before"], before)
        self.assertEqual(receipt["source_sha256_after"], before)
        self.assertEqual(stage.inventory(self.source), before)
        self.assertEqual((self.output / "Data/Win2kDisableTaskSwitch.dll").read_bytes(), b"new-proxy")
        self.assertEqual((self.output / "Data/ArmadaL.exe").read_bytes(), b"patched")
        self.assertEqual((self.output / "Data/config.ini").read_bytes(), b"config")
        self.assertTrue((self.output / "Data/empty").is_dir())
        self.assertEqual((retired / "Data/nested/DxVer.DLL").read_bytes(), b"dxver")
        self.assertEqual(len(receipt["quarantined"]), 3)
        self.assertEqual(receipt["counts"], {"source_files": 8, "staged_files": 5,
                                              "quarantined_files": 3, "replaced_files": 2})
        for relative, row in receipt["quarantined"].items():
            self.assertFalse((self.output / relative).exists())
            self.assertEqual(row["source_sha256"], row["quarantine_sha256"])
            self.assertEqual(stage.sha256(Path(row["quarantine_path"])), row["source_sha256"])
        for relative, row in receipt["replaced"].items():
            self.assertEqual(row["source_sha256"], before[relative])
            self.assertEqual(row["staged_sha256"], stage.sha256(self.output / relative))
            self.assertNotEqual((self.source / relative).stat().st_ino,
                                (self.output / relative).stat().st_ino)
        (self.output / "Data/config.ini").write_bytes(b"changed")
        self.assertEqual((self.source / "Data/config.ini").read_bytes(), b"config")

    def test_optional_patch_is_not_required(self):
        receipt = stage.prepare(self.source, self.output, self.proxy)
        self.assertEqual((self.output / "Data/ArmadaL.exe").read_bytes(), b"armada")
        self.assertEqual(list(receipt["replaced"]), ["Data/Win2kDisableTaskSwitch.dll"])

    def test_supplied_profile_replaces_only_staged_copy(self):
        original = "[roots]\ncrc=old\n".encode("utf-8")
        prepared = "[roots]\ncrc=new\n".encode("utf-8")
        self.put("Data/A2FOArmadaProfiles.txt", original)
        profile = self.root / "prepared-profiles.txt"
        profile.write_bytes(prepared)
        before = stage.inventory(self.source)
        receipt = stage.prepare(self.source, self.output, self.proxy, armada_profile=profile)
        relative = "Data/A2FOArmadaProfiles.txt"
        row = receipt["replaced"][relative]
        self.assertEqual((self.output / relative).read_bytes(), prepared)
        self.assertEqual((self.source / relative).read_bytes(), original)
        self.assertEqual(stage.inventory(self.source), before)
        self.assertEqual(row["source_sha256"], before[relative])
        self.assertEqual(row["supplied_sha256"], stage.sha256(profile))
        self.assertEqual(row["staged_sha256"], stage.sha256(self.output / relative))
        self.assertEqual(receipt["supplied_images"]["armada_profile"]["sha256"],
                         row["supplied_sha256"])

    def test_supplied_core_replaces_only_staged_copy(self):
        self.put("Data/A2FOExtensions.dll", b"old-core")
        core = self.root / "new-core.dll"
        core.write_bytes(b"new-core")
        before = stage.inventory(self.source)
        receipt = stage.prepare(self.source, self.output, self.proxy, core=core)
        relative = "Data/A2FOExtensions.dll"
        row = receipt["replaced"][relative]
        self.assertEqual((self.output / relative).read_bytes(), b"new-core")
        self.assertEqual((self.source / relative).read_bytes(), b"old-core")
        self.assertEqual(stage.inventory(self.source), before)
        self.assertEqual(row["source_sha256"], before[relative])
        self.assertEqual(row["supplied_sha256"], stage.sha256(core))
        self.assertEqual(row["staged_sha256"], stage.sha256(self.output / relative))
        self.assertFalse(receipt["ready_for_manual_launch"])

    def test_profile_rejects_symlink(self):
        link = self.root / "profile-link.txt"
        link.symlink_to(self.proxy)
        with self.assertRaisesRegex(ValueError, "symlink"):
            stage.prepare(self.source, self.output, self.proxy, armada_profile=link)

    def test_core_rejects_symlink(self):
        link = self.root / "core-link.dll"
        link.symlink_to(self.proxy)
        with self.assertRaisesRegex(ValueError, "symlink"):
            stage.prepare(self.source, self.output, self.proxy, core=link)

    def test_rejects_overlap_and_paths_outside_build(self):
        for target in (self.source, self.data / "child", self.root, Path("/tmp/legacy-candidate"),
                       stage.REPO / "build/../../outside-candidate",
                       self.root / "candidate/../install/Data/child"):
            with self.subTest(target=target), self.assertRaises(ValueError):
                stage.prepare(self.source, target, self.proxy)

    def test_rejects_receipt_traversal_into_source(self):
        receipt = self.root / "candidate/../install/new-receipt.json"
        with self.assertRaisesRegex(ValueError, "receipt"):
            stage.prepare(self.source, self.output, self.proxy, receipt=receipt)
        self.assertFalse((self.source / "new-receipt.json").exists())

    def test_rejects_symlink_in_dot_dot_supplied_path(self):
        link = self.root / "linked"
        link.symlink_to(self.source, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            stage.prepare(self.source, self.output, link / "../proxy.dll")

    def test_manifest_replaces_base_and_parent_executables(self):
        parent_image = self.root / "parent-patched.exe"
        parent_image.write_bytes(b"parent-patched")
        manifest = self.root / "images.json"
        manifest.write_text(json.dumps({"Data/ArmadaL.exe": self.patched.name,
                                        "Data/Mods/Parent/ArmadaL.exe": parent_image.name}))
        before = stage.inventory(self.source)
        receipt = stage.prepare(self.source, self.output, self.proxy, patched_images=manifest)
        self.assertEqual((self.output / "Data/ArmadaL.exe").read_bytes(), b"patched")
        self.assertEqual((self.output / "Data/Mods/Parent/ArmadaL.exe").read_bytes(), b"parent-patched")
        self.assertEqual(stage.inventory(self.source), before)
        self.assertEqual(receipt["counts"]["replaced_files"], 3)
        for relative in ("Data/ArmadaL.exe", "Data/Mods/Parent/ArmadaL.exe"):
            row = receipt["replaced"][relative]
            self.assertEqual(row["source_sha256"], before[relative])
            self.assertEqual(row["supplied_sha256"], row["staged_sha256"])
            self.assertEqual(row["staged_sha256"], stage.sha256(self.output / relative))

    def test_manifest_rejects_unsafe_targets_and_alias_conflict(self):
        manifest = self.root / "images.json"
        for key in ("../evil.exe", "/absolute.exe", "Data/../ArmadaL.exe",
                    "Data/Win2kDisableTaskSwitch.original.dll"):
            manifest.write_text(json.dumps({key: self.patched.name}))
            with self.subTest(key=key), self.assertRaises(ValueError):
                stage.prepare(self.source, self.output, self.proxy, patched_images=manifest)
        manifest.write_text(json.dumps({"Data/ArmadaL.exe": self.patched.name}))
        with self.assertRaisesRegex(ValueError, "cannot be combined"):
            stage.prepare(self.source, self.output, self.proxy, self.patched, patched_images=manifest)

    def test_rejects_insufficient_space_before_copy(self):
        usage = namedtuple("usage", "total used free")
        with patch.object(stage.shutil, "disk_usage", return_value=usage(1, 1, 0)):
            with self.assertRaisesRegex(ValueError, "insufficient staging space"):
                stage.prepare(self.source, self.output, self.proxy)
        self.assertFalse(self.output.exists())

    def test_rejects_nonempty_output_or_existing_quarantine(self):
        self.output.mkdir()
        (self.output / "untouched").write_bytes(b"keep")
        with self.assertRaisesRegex(ValueError, "nonempty"):
            stage.prepare(self.source, self.output, self.proxy)
        self.assertEqual((self.output / "untouched").read_bytes(), b"keep")
        (self.output / "untouched").unlink()
        retired = self.root / "candidate.retired"
        retired.mkdir()
        with self.assertRaisesRegex(ValueError, "quarantine"):
            stage.prepare(self.source, self.output, self.proxy)

    def test_rejects_symlink_in_source_and_roots(self):
        (self.data / "link.dll").symlink_to(self.proxy)
        with self.assertRaisesRegex(ValueError, "symlink"):
            stage.prepare(self.source, self.output, self.proxy)
        (self.data / "link.dll").unlink()
        alias = self.root / "alias"
        alias.symlink_to(self.source, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            stage.prepare(alias, self.output, self.proxy)
        self.output.symlink_to(self.root / "missing", target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "symlink"):
            stage.prepare(self.source, self.output, self.proxy)

    def test_copy_failure_leaves_partial_and_failed_receipt(self):
        before = stage.inventory(self.source)
        with patch.object(stage.shutil, "copy2", side_effect=OSError("synthetic copy failure")):
            with self.assertRaisesRegex(OSError, "synthetic copy failure"):
                stage.prepare(self.source, self.output, self.proxy)
        self.assertTrue(self.output.is_dir())
        self.assertTrue((self.root / "candidate.retired").is_dir())
        receipt = json.loads((self.root / "candidate.receipt.json").read_text())
        self.assertEqual(receipt["status"], "failed")
        self.assertEqual(receipt["source_sha256_before"], before)
        self.assertEqual(receipt["source_sha256_after"], before)


if __name__ == "__main__":
    unittest.main()
