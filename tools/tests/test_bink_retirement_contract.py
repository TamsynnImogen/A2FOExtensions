"""Regression checks for the isolated Bink export/import retirement contracts.

Local PE inputs are proprietary fixtures and are never copied into source.
"""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import retarget_bink_imports as retarget  # noqa: E402
import rust_dll_linker as linker  # noqa: E402
from armada_compat_profile import parse_image  # noqa: E402
from retarget_imagehlp import BytesImage, normal_imports, sha  # noqa: E402


class ExportContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.base_198 = (ROOT / "core/A2FOExtensions.def").read_bytes()
        cls.private_210 = (ROOT / "core/A2FOExtensions.bink.def").read_bytes()
        cls.base_197 = b"".join(
            line for line in cls.base_198.splitlines(keepends=True)
            if not line.split() or line.split()[0] != b"A2FO_InitializeNoUnwind"
        )

    def test_exact_197_198_210_contracts(self):
        for data, count in ((self.base_197, 197), (self.base_198, 198), (self.private_210, 210)):
            with self.subTest(count=count):
                names = linker.validate_contract(data)
                self.assertEqual(len(names), count)
        names = linker.validate_contract(self.private_210)
        self.assertEqual(names["_BinkSetVolume@8"].split(),
                         ['"_BinkSetVolume@8"', '=', 'a2fo_bink_set_volume@12', '@207'])

    def test_old_export_name_ordinal_and_forwarder_changes_rejected(self):
        original = b"A2FOAlwaysShowShields_CleanupCraft @1"
        self.assertIn(original, self.base_197)
        for replacement in (
            b"A2FOAlwaysShowShields_CleanupCraft @999",
            b"A2FOAlwaysShowShields_CleanupCraftChanged @1",
            b"A2FOAlwaysShowShields_CleanupCraft = other_dll.target @1",
        ):
            with self.subTest(replacement=replacement):
                tampered = self.base_197.replace(original, replacement, 1)
                with self.assertRaises(ValueError):
                    linker.validate_contract(tampered)

    def test_wrong_volume_alias_and_incomplete_private_contract_rejected(self):
        original = b'"_BinkSetVolume@8" = a2fo_bink_set_volume@12 @207'
        self.assertIn(original, self.private_210)
        for replacement in (
            b'"_BinkSetVolume@8" = a2fo_bink_set_volume@8 @207',
            b'"_BinkSetVolume@8" = a2fo_bink_set_volume@12 @211',
        ):
            with self.subTest(replacement=replacement):
                with self.assertRaises(ValueError):
                    linker.validate_contract(self.private_210.replace(original, replacement, 1))
        incomplete = b"".join(line for line in self.private_210.splitlines(keepends=True)
                              if b'"_BinkGetError@0"' not in line)
        with self.assertRaises(ValueError):
            linker.validate_contract(incomplete)
        # A superficially 210-entry file must still reject a missing alias.
        substituted = incomplete + b'"_BinkUnreviewed@0" = other@0 @210\n'
        with self.assertRaises(ValueError):
            linker.validate_contract(substituted)

    def test_duplicate_export_rejected(self):
        with self.assertRaisesRegex(ValueError, "duplicate export"):
            linker.entries("EXPORTS\nfoo @1\nfoo @2\n")


class RetargetedImageTests(unittest.TestCase):
    ENGINE_RECEIPTS = (
        "ArmadaL.exe.receipt.json",
        "ArmadaL-fo4.exe.receipt.json",
    )
    FLEETOPS_RECEIPT = ROOT / "build/bink-corrupt-investigation-20261101/FleetOpsHook.dll.receipt.json"

    @classmethod
    def setUpClass(cls):
        cls.directory = ROOT / "build/bink-rust-stage2/retargeted"

    def fixtures(self):
        receipt_paths = [self.directory / filename for filename in self.ENGINE_RECEIPTS]
        receipt_paths.append(self.FLEETOPS_RECEIPT)
        for receipt_path in receipt_paths:
            if not receipt_path.is_file():
                self.skipTest(f"local PE receipt missing: {receipt_path}")
            receipt = json.loads(receipt_path.read_text())
            source = Path(receipt["source"])
            output = Path(receipt["output"])
            if not source.is_file() or not output.is_file():
                self.skipTest(f"local PE fixture missing: {source} or {output}")
            yield receipt, source, output

    def test_rewrite_preserves_thunks_geometry_and_executable_bytes(self):
        checked = 0
        for receipt, source, output in self.fixtures():
            with self.subTest(kind=receipt["kind"]):
                before = source.read_bytes()
                self.assertEqual(sha(before), receipt["before_sha256"])
                self.assertIn(sha(before), retarget.KNOWN)
                after, record = retarget.rewrite(before)
                self.assertEqual(after, output.read_bytes())
                self.assertEqual(sha(after), receipt["after_sha256"])
                self.assertEqual(record["changed_file_offsets"], receipt["changed_file_offsets"])
                original_imports = normal_imports(before)
                rewritten_imports = normal_imports(after)
                self.assertEqual(len(original_imports), len(rewritten_imports))
                selected = [i for i, item in enumerate(original_imports)
                            if item["dll"].lower() == "binkw32.dll"]
                self.assertEqual(len(selected), record["descriptor_count"])
                for i, (old, new) in enumerate(zip(original_imports, rewritten_imports)):
                    self.assertEqual(old["entries"], new["entries"])
                    if i in selected:
                        self.assertEqual(new["dll"], "A2FOExtensions.dll")
                        self.assertEqual(new["name_offset"], record["replacement_name_offset"])
                    else:
                        self.assertEqual(old, new)
                before_sections = parse_image(before)["sections"]
                after_sections = parse_image(after)["sections"]
                self.assertEqual(len(before_sections), len(after_sections))
                for old, new in zip(before_sections, after_sections):
                    geometry = lambda section: {key: value for key, value in section.items()
                                                if key not in ("raw_crc32", "virtual_crc32")}
                    self.assertEqual(geometry(old), geometry(new))
                    if old["characteristics"] & 0x20000000:  # IMAGE_SCN_MEM_EXECUTE
                        start = old["raw_offset"]
                        end = start + old["raw_size"]
                        self.assertEqual(before[start:end], after[start:end])
                im = BytesImage(before)
                descriptor_start = im.offset(im.u32(im.optional + 104))
                name_fields = {descriptor_start + i * 20 + 12 + byte
                               for i in selected for byte in range(4)}
                checksum_field = {im.optional + 64 + byte for byte in range(4)}
                name_offset = record["replacement_name_offset"]
                new_name = retarget.NEW
                self.assertEqual(before[name_offset:name_offset + len(new_name)],
                                 b"\0" * len(new_name))
                self.assertEqual(after[name_offset:name_offset + len(new_name)], new_name)
                padding = set(range(name_offset, name_offset + len(new_name)))
                if receipt["kind"] == "roots-fleetops":
                    self.assertEqual(record["checksum_policy"], "preserve_pinned_fleetops_legacy_field")
                    self.assertEqual(record["stored_checksum_after"], record["stored_checksum_before"])
                    allowed = name_fields | padding
                else:
                    self.assertEqual(record["checksum_policy"], "recalculate_engine_pe_checksum")
                    allowed = name_fields | checksum_field | padding
                actual = {i for i, (old, new) in enumerate(zip(before, after)) if old != new}
                self.assertEqual(len(before), len(after))
                self.assertTrue(actual)
                self.assertLessEqual(actual, allowed)
                for i in selected:
                    name_rva = struct.unpack_from("<I", after, descriptor_start + i * 20 + 12)[0]
                    self.assertEqual(name_rva, record["replacement_name_rva"])
                checked += 1
        self.assertEqual(checked, 3)

    def fleetops_fixture(self):
        if not self.FLEETOPS_RECEIPT.is_file():
            self.skipTest(f"local PE receipt missing: {self.FLEETOPS_RECEIPT}")
        receipt = json.loads(self.FLEETOPS_RECEIPT.read_text())
        source, output = Path(receipt["source"]), Path(receipt["output"])
        if not source.is_file() or not output.is_file():
            self.skipTest(f"local PE fixture missing: {source} or {output}")
        return receipt, source, output

    def test_fleetops_retarget_preserves_legacy_checksum_and_image_contract(self):
        receipt, source, output = self.fleetops_fixture()
        before, after = source.read_bytes(), output.read_bytes()
        self.assertEqual(sha(before), receipt["before_sha256"])
        self.assertEqual(sha(after), receipt["after_sha256"])
        rebuilt, record = retarget.rewrite(before)
        self.assertEqual(rebuilt, after)
        self.assertEqual(record["changed_file_offsets"], receipt["changed_file_offsets"])
        self.assertEqual(record["checksum_policy"], "preserve_pinned_fleetops_legacy_field")
        self.assertEqual(record["stored_checksum_after"], record["stored_checksum_before"])

        old_imports, new_imports = normal_imports(before), normal_imports(after)
        selected = [index for index, entry in enumerate(old_imports)
                    if entry["dll"].lower() == "binkw32.dll"]
        self.assertEqual(len(selected), 2)
        for index, (old, new) in enumerate(zip(old_imports, new_imports)):
            self.assertEqual(old["entries"], new["entries"])
            if index in selected:
                self.assertEqual(new["dll"], "A2FOExtensions.dll")
            else:
                self.assertEqual(old, new)

        before_image, after_image = parse_image(before), parse_image(after)
        geometry = lambda info: [
            (section["name"], section["virtual_address"], section["virtual_size"],
             section["raw_offset"], section["raw_size"], section["characteristics"])
            for section in info["sections"]
        ]
        self.assertEqual(geometry(before_image), geometry(after_image))
        text_section = next(section for section in before_image["sections"]
                            if section["name"] == ".text")
        start, end = text_section["raw_offset"], text_section["raw_offset"] + text_section["raw_size"]
        self.assertEqual(before[start:end], after[start:end])
        optional = BytesImage(before).optional
        export_rva, export_size = struct.unpack_from("<II", before, optional + 96)
        start = BytesImage(before).offset(export_rva)
        end = start + export_size
        self.assertEqual(before[start:end], after[start:end])

        image = BytesImage(before)
        field = image.optional + 64
        checksum_bytes = set(range(field, field + 4))
        descriptor_start = image.offset(image.u32(image.optional + 104))
        name_fields = {descriptor_start + index * 20 + 12 + byte
                       for index in selected for byte in range(4)}
        name_offset = record["replacement_name_offset"]
        self.assertEqual(before[name_offset:name_offset + len(retarget.NEW)], b"\0" * len(retarget.NEW))
        self.assertEqual(after[name_offset:name_offset + len(retarget.NEW)], retarget.NEW)
        changed = {offset for offset, (left, right) in
                   enumerate(zip(before, after)) if left != right}
        self.assertTrue(changed)
        self.assertTrue(checksum_bytes.isdisjoint(changed))
        self.assertLessEqual(changed, name_fields | set(range(name_offset, name_offset + len(retarget.NEW))))

    def test_recomputed_fleetops_checksum_is_legacy_error_path_positive_control(self):
        receipt, source, output = self.fleetops_fixture()
        before, after = source.read_bytes(), output.read_bytes()
        field = BytesImage(before).optional + 64

        # Independent fold copied from the checksum routine shape at RVA 6c14c
        # in the saved CheckFileCrc disassembly. The PE header's four checksum
        # bytes are omitted while summing, then file length is added.
        def independent_pe_checksum(data):
            total = 0
            for offset in range(0, len(data), 2):
                word = 0 if field <= offset < field + 4 else int.from_bytes(data[offset:offset + 2], "little")
                total += word
                total = (total & 0xFFFF) + (total >> 16)
            total = (total & 0xFFFF) + (total >> 16)
            return (total & 0xFFFF) + len(data)

        original_stored = struct.unpack_from("<I", before, field)[0]
        self.assertEqual(original_stored, 0x2E94FF)
        self.assertEqual(independent_pe_checksum(before), 0x2F096A)
        self.assertNotEqual(original_stored, independent_pe_checksum(before))

        old_policy_output = bytearray(after)
        struct.pack_into("<I", old_policy_output, field, independent_pe_checksum(after))
        old_policy_stored = struct.unpack_from("<I", old_policy_output, field)[0]
        self.assertEqual(old_policy_stored, independent_pe_checksum(old_policy_output))

        disassembly = (ROOT / "build/bink-corrupt-investigation-20261101/check-file-crc.asm").read_text()
        self.assertIn("cmp    (%edx),%eax", disassembly)
        self.assertIn("mov    $0x1,%bl", disassembly)
        self.assertIn("test   %bl,%bl", disassembly)
        self.assertIn("jne    0x5a86c3e6", disassembly)
        # Equal computed/header values avoid the BL=mismatch bypass and enter
        # the observed legacy error route. The preceding assertions establish
        # that old checksum-recalculation policy creates that equality.
        self.assertEqual(old_policy_stored, independent_pe_checksum(old_policy_output))

    def test_unknown_single_byte_mutation_rejected_before_output(self):
        receipt_path = self.directory / self.ENGINE_RECEIPTS[0]
        if not receipt_path.is_file():
            self.skipTest("local PE receipt missing")
        receipt = json.loads(receipt_path.read_text())
        source = Path(receipt["source"])
        if not source.is_file():
            self.skipTest("local PE fixture missing")
        data = bytearray(source.read_bytes())
        self.assertEqual(sha(data), receipt["before_sha256"])
        data[0x200] ^= 1
        with self.assertRaisesRegex(ValueError, "unknown image"):
            retarget.rewrite(bytes(data))
        with tempfile.TemporaryDirectory(dir=ROOT / "build") as directory:
            modified = Path(directory) / "modified.exe"
            output = Path(directory) / "must-not-exist.exe"
            modified.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "unknown image"):
                retarget.patch(modified, output)
            self.assertFalse(output.exists())
            self.assertFalse(output.with_name(output.name + ".receipt.json").exists())


if __name__ == "__main__":
    unittest.main()
