"""Offline tests for the narrow FleetOps Refit executable profile checker."""

import hashlib
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "tools/check_refit_fo_profile.py"
SPEC = importlib.util.spec_from_file_location("check_refit_fo_profile", SCRIPT)
checker = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(checker)


BASE = 0x400000
TIMESTAMP = 0x12345678
IMAGE_SIZE = 0x404000
SECTION_RVA = 0x1000
SECTION_OFFSET = 0x400
SECTION_SIZE = 0x3B9000
IMPORT_RVA = 0x280000


def _put(data, offset, value):
    data[offset:offset + len(value)] = value


def synthetic_pe(*, wrong_factory=False, mutated_finder=False, bad_call=False):
    """Build a minimal file-backed PE32 carrying the exact admitted code shape."""
    data = bytearray(SECTION_OFFSET + SECTION_SIZE)
    _put(data, 0, b"MZ")
    struct.pack_into("<I", data, 0x3C, 0x80)
    _put(data, 0x80, b"PE\0\0")
    nt = 0x80
    struct.pack_into("<HHIIIHH", data, nt + 4, 0x14C, 1, TIMESTAMP, 0, 0, 224, 0x210E)
    opt = nt + 24
    struct.pack_into("<H", data, opt, 0x10B)
    struct.pack_into("<I", data, opt + 28, BASE)
    struct.pack_into("<I", data, opt + 56, IMAGE_SIZE)
    struct.pack_into("<I", data, opt + 60, SECTION_OFFSET)
    struct.pack_into("<I", data, opt + 92, 16)
    struct.pack_into("<II", data, opt + 96 + 8, IMPORT_RVA, 40)
    section = opt + 224
    _put(data, section, b".all\0\0\0\0")
    struct.pack_into("<IIII", data, section + 8, SECTION_SIZE, SECTION_RVA,
                     SECTION_SIZE, SECTION_OFFSET)

    def file_at(rva):
        return SECTION_OFFSET + rva - SECTION_RVA

    finder = bytearray(checker.FINDER)
    if mutated_finder:
        finder[10] ^= 1
    _put(data, file_at(checker.FINDER_RVA), finder)
    for index, rva in enumerate(checker.CALL_RVAS):
        target = BASE + checker.FINDER_RVA
        displacement = target - (BASE + rva + 5)
        call = bytearray(b"\xE8" + struct.pack("<i", displacement))
        if bad_call and index == 4:
            call[1] ^= 1
        _put(data, file_at(rva - 20), bytes(range(20)))
        _put(data, file_at(rva), call)

    desc = file_at(IMPORT_RVA)
    struct.pack_into("<IIIII", data, desc, IMPORT_RVA + 0x300, 0, 0,
                      IMPORT_RVA + 0x100, 0x3B82CC)
    _put(data, file_at(IMPORT_RVA + 0x100), b"d3d8.dll\0")
    struct.pack_into("<II", data, file_at(IMPORT_RVA + 0x300), IMPORT_RVA + 0x200, 0)
    struct.pack_into("<I", data, file_at(0x3B82CC), 0)
    _put(data, file_at(IMPORT_RVA + 0x200), b"\0\0" +
         (b"OtherD3DCall" if wrong_factory else b"Direct3DCreate8") + b"\0")
    return bytes(data)


def admitted(data):
    """Return a one-image profile matching fixture headers, solely for this test."""
    parsed = checker.PE(data)
    digest = hashlib.sha256(data).hexdigest()
    return digest, ("synthetic-test", parsed.timestamp, parsed.image_size, parsed.section_fnv)


class RefitProfileTests(unittest.TestCase):
    def inspect_fixture(self, data):
        digest, profile = admitted(data)
        with tempfile.TemporaryDirectory() as tmp, mock.patch.dict(checker.PROFILES, {digest: profile}, clear=True):
            image = Path(tmp) / "synthetic.exe"
            image.write_bytes(data)
            return checker.inspect(image)

    def assert_fixture_rejected(self, data, reason):
        digest, profile = admitted(data)
        with tempfile.TemporaryDirectory() as tmp, mock.patch.dict(checker.PROFILES, {digest: profile}, clear=True):
            image = Path(tmp) / "synthetic.exe"
            image.write_bytes(data)
            with self.assertRaisesRegex(ValueError, reason):
                checker.inspect(image)

    def test_complete_synthetic_pe_passes_all_factory_and_mode_checks(self):
        result = self.inspect_fixture(synthetic_pe())
        self.assertEqual(result["profile"], "synthetic-test")
        self.assertEqual([call["rva"] for call in result["mode_calls"]], list(checker.CALL_RVAS))
        self.assertEqual(result["factory_iat_rva"], 0x3B82CC)

    def test_unreviewed_sha_is_rejected_before_pe_inspection(self):
        data = synthetic_pe()
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / "synthetic.exe"
            image.write_bytes(data)
            with self.assertRaisesRegex(ValueError, "unreviewed executable SHA-256"):
                checker.inspect(image)

    def test_known_synthetic_hash_still_requires_complete_finder_body(self):
        self.assert_fixture_rejected(synthetic_pe(mutated_finder=True), "complete finder body mismatch")

    def test_known_synthetic_hash_still_requires_each_call_target(self):
        self.assert_fixture_rejected(synthetic_pe(bad_call=True), "finder call target mismatch")

    def test_wrong_direct3d_factory_import_is_rejected(self):
        self.assert_fixture_rejected(synthetic_pe(wrong_factory=True), "factory import count")

    def test_truncated_file_backed_section_is_rejected(self):
        data = synthetic_pe()
        cut = SECTION_OFFSET + checker.FINDER_RVA - SECTION_RVA - 1
        self.assert_fixture_rejected(data[:cut], "file span")


if __name__ == "__main__":
    unittest.main()
