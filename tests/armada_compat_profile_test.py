"""Focused tests for the read-only Armada PE compatibility profile tool."""

from pathlib import Path
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest
import zlib


ROOT = Path(__file__).resolve().parents[1]
SCRIPT = ROOT / "tools" / "armada_compat_profile.py"
spec = importlib.util.spec_from_file_location("armada_compat_profile", SCRIPT)
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)


def synthetic_pe(raw_size=4, raw_offset=0x200):
    """Return a minimal PE32 image with one section."""
    data = bytearray(0x200 + raw_size)
    data[:2] = b"MZ"
    data[0x3C:0x40] = (0x80).to_bytes(4, "little")
    data[0x80:0x84] = b"PE\0\0"
    data[0x84:0x98] = (
        (0x14C).to_bytes(2, "little") + (1).to_bytes(2, "little") +
        (0x12345678).to_bytes(4, "little") + bytes(8) + (0xE0).to_bytes(2, "little") +
        (0x0102).to_bytes(2, "little")
    )
    optional = 0x98
    data[optional:optional + 2] = (0x10B).to_bytes(2, "little")
    data[optional + 16:optional + 20] = (0x1000).to_bytes(4, "little")
    data[optional + 28:optional + 32] = (0x400000).to_bytes(4, "little")
    data[optional + 32:optional + 36] = (0x1000).to_bytes(4, "little")
    data[optional + 56:optional + 60] = (0x2000).to_bytes(4, "little")
    section = optional + 0xE0
    data[section:section + 8] = b".text\0\0\0"
    data[section + 8:section + 24] = (
        (4).to_bytes(4, "little") + (0x1000).to_bytes(4, "little") +
        raw_size.to_bytes(4, "little") + raw_offset.to_bytes(4, "little")
    )
    data[section + 36:section + 40] = (0x60000020).to_bytes(4, "little")
    return bytes(data)


class ParserTests(unittest.TestCase):
    def test_parses_pe32_fields_and_section_crc(self):
        data = synthetic_pe()
        info = profile.parse_image(data)
        self.assertEqual(info["machine"], 0x14C)
        self.assertEqual(info["file_characteristics"], 0x0102)
        self.assertEqual(info["optional_magic"], 0x10B)
        self.assertEqual(info["sections"][0]["name"], ".text")
        self.assertEqual(info["sections"][0]["characteristics"], 0x60000020)
        self.assertEqual(info["sections"][0]["raw_crc32"], "2144DF1C")
        self.assertEqual(info["sections"][0]["virtual_crc32"], "2144DF1C")
        self.assertEqual(info["file_crc32"], f"{zlib.crc32(data):08X}")

    def test_rejects_malformed_and_truncated_inputs(self):
        for data in (b"", b"not PE" + bytes(100)):
            with self.subTest(data_len=len(data)), self.assertRaises(profile.PEError):
                profile.parse_image(data)
        broken_signature = bytearray(synthetic_pe())
        broken_signature[0x80] = 0
        with self.assertRaisesRegex(profile.PEError, "PE signature"):
            profile.parse_image(bytes(broken_signature))
        truncated_span = synthetic_pe(raw_size=0x100, raw_offset=0x200)[:-0xFF]
        with self.assertRaisesRegex(profile.PEError, "raw span exceeds"):
            profile.parse_image(truncated_span)

    def test_compare_groups_changed_bytes_with_file_offsets_and_rva(self):
        old_data = synthetic_pe()
        new = bytearray(old_data)
        new[0x201:0x203] = b"XY"
        old_info = profile.parse_image(old_data)
        new_info = profile.parse_image(bytes(new))
        result = profile.compare(old_data, old_info, bytes(new), new_info)
        self.assertEqual(result["byte_differences"]["differing_bytes"], 2)
        self.assertEqual(result["byte_differences"]["groups"],
                         [{"file_offset": 0x201, "length": 2, "rva": 0x1001}])
        self.assertIn(".text", result["section_layout_differences"])


class RootsIntegrationTests(unittest.TestCase):
    latest = Path("/home/tamsynn/NVMeData/Fleet Ops Roots/Data/ArmadaL.exe")
    september = ROOT / ".build/engine-compat-20261003/september30/ArmadaL.exe"
    october = ROOT / "build/engine-compat-20261023/inputs/oct23/ArmadaL.exe"

    @staticmethod
    def synthetic_roots_info():
        info = {
            "machine": profile.FIXED_ROOTS["machine"],
            "optional_magic": profile.FIXED_ROOTS["optional_magic"],
            "image_base": profile.FIXED_ROOTS["image_base"],
            "entry_point": profile.FIXED_ROOTS["entry_point"],
            "timestamp": 0x3C4C76BD,
            "image_size": profile.FIXED_ROOTS["image_size"],
            "optional_header_size": profile.FIXED_ROOTS["optional_header_size"],
            "section_alignment": profile.FIXED_ROOTS["section_alignment"],
            "header_crc32": "01234567",
            "sha256": "synthetic",
            "sections": [],
        }
        for geometry, flags in zip(profile.FIXED_ROOTS["sections"],
                                   profile.FIXED_ROOTS["section_characteristics"]):
            name, va, virtual_size, raw_offset, raw_size = geometry
            info["sections"].append({
                "name": name, "virtual_address": va, "virtual_size": virtual_size,
                "raw_offset": raw_offset, "raw_size": raw_size,
                "characteristics": flags, "raw_crc32": "89ABCDEF",
                "virtual_crc32": "CD47B19C" if name == ".rdata" else "89ABCDEF",
            })
        return info

    def test_profile_rejects_geometry_flags_base_entry_size_and_truncation(self):
        for label, mutate in (
                ("geometry", lambda info: info["sections"][0].update(raw_offset=0x1001)),
                ("section flags", lambda info: info["sections"][0].update(characteristics=0x40000040)),
                ("section alignment", lambda info: info.update(section_alignment=0x2000)),
                ("optional header size", lambda info: info.update(optional_header_size=0xE8)),
                ("base", lambda info: info.update(image_base=0x500000)),
                ("entry", lambda info: info.update(entry_point=0x1000)),
                ("image size", lambda info: info.update(image_size=0x400000)),
                ("short header", lambda info: info.update(header_crc32=None))):
            with self.subTest(label=label):
                info = self.synthetic_roots_info()
                mutate(info)
                with self.assertRaises(profile.PEError):
                    profile.make_profile(info)

    def test_synthetic_fixed_roots_profile_keeps_raw_fingerprints(self):
        info = self.synthetic_roots_info()
        info["timestamp"] = 0x11223344
        info["sections"][1]["virtual_crc32"] = "76543210"
        info["sections"][1]["raw_crc32"] = "AABBCCDD"
        generated = profile.make_profile(info)
        self.assertIn("# .rdata virtual CRC32: 76543210", generated)
        content = [line for line in generated.splitlines() if not line.startswith("#")]
        self.assertEqual(content[0], "A2FO_ARMADA_PROFILES_V1")
        self.assertEqual(len(content[1].split()), 11)
        self.assertEqual(content[1].split()[1:4], ["11223344", "00405000", "01234567"])
        self.assertEqual(content[1].split()[4:], ["89ABCDEF", "AABBCCDD"] + ["89ABCDEF"] * 5)
        self.assertIn("manual binary/hook audit", generated)

    @unittest.skipUnless(october.is_file(), "Oct 23 Armada fixture unavailable")
    def test_oct23_and_changed_timestamp_rdata_remain_exactly_fingerprinted(self):
        original = self.october.read_bytes()
        before = profile.parse_image(original)
        proposal = profile.make_profile(before)
        self.assertIn(before["header_crc32"], proposal)
        self.assertIn(before["sections"][1]["raw_crc32"], proposal)
        modified = bytearray(original)
        pe_offset = int.from_bytes(modified[0x3C:0x40], "little")
        modified[pe_offset + 8:pe_offset + 12] = (0x11223344).to_bytes(4, "little")
        modified[before["sections"][1]["raw_offset"]] ^= 1
        after = profile.parse_image(bytes(modified))
        changed = profile.make_profile(after)
        self.assertIn("roots_native_rotation 11223344", changed)
        self.assertNotEqual(before["header_crc32"], after["header_crc32"])
        self.assertNotEqual(before["sections"][1]["raw_crc32"], after["sections"][1]["raw_crc32"])
        self.assertIn(after["header_crc32"], changed)
        self.assertIn(after["sections"][1]["raw_crc32"], changed)

    @unittest.skipUnless(latest.is_file() and september.is_file(), "Roots comparison fixtures unavailable")
    def test_latest_and_september30_inspect_compare_and_profile(self):
        latest_info = profile.parse_image(self.latest.read_bytes())
        self.assertEqual(latest_info["timestamp"], 0x3C4C76BD)
        self.assertEqual(latest_info["image_size"], 0x405000)
        self.assertEqual(len(latest_info["sections"]), 7)
        self.assertEqual(latest_info["sections"][-1]["raw_size"], 0xA999)

        compared = subprocess.run(
            [sys.executable, str(SCRIPT), "compare", str(self.september), str(self.latest)],
            check=True, capture_output=True, text=True)
        result = json.loads(compared.stdout)
        self.assertNotEqual(result["old_sha256"], result["new_sha256"])
        self.assertGreater(result["byte_differences"]["differing_bytes"], 0)
        self.assertTrue(result["byte_differences"]["groups"])

        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "proposed.profile"
            subprocess.run([sys.executable, str(SCRIPT), "profile", str(self.latest), "--output", str(output)],
                           check=True, capture_output=True, text=True)
            lines = output.read_text(encoding="ascii").splitlines()
            content = [line for line in lines if not line.startswith("#")]
            self.assertEqual(content[0], "A2FO_ARMADA_PROFILES_V1")
            tokens = content[1].split()
            self.assertEqual(tokens[0], "roots_native_rotation")
            self.assertEqual(len(tokens), 11)
            self.assertIn("manual binary/hook audit before installation", " ".join(lines))
            self.assertIn(f"# .rdata virtual CRC32: {latest_info['sections'][1]['virtual_crc32']}", lines)


class FleetOpsProfileTests(unittest.TestCase):
    fixture = ROOT / "build/engine-compat-20261023/inputs/oct23/FleetOpsHook.dll"

    @staticmethod
    def synthetic_info():
        fixed = profile.FIXED_FLEETOPS
        info = {key: fixed[key] for key in ("machine", "optional_magic", "image_base", "entry_point",
                                             "image_size", "file_size", "file_characteristics",
                                             "optional_header_size", "section_alignment")}
        info.update(timestamp=0x12345678, file_crc32="ABCDEF01", sha256="synthetic", sections=[])
        for geometry, flags in zip(fixed["sections"], fixed["section_characteristics"]):
            name, va, virtual_size, raw_offset, raw_size = geometry
            info["sections"].append(dict(name=name, virtual_address=va, virtual_size=virtual_size,
                                         raw_offset=raw_offset, raw_size=raw_size, characteristics=flags))
        return info

    def test_synthetic_profile_records_changed_timestamp_and_crc(self):
        result = profile.make_fleetops_profile(self.synthetic_info())
        self.assertIn("A2FO_FLEETOPS_PROFILES_V1\n", result)
        self.assertIn("roots_fleetops 12345678 00322000 002E7200 ABCDEF01\n", result)
        self.assertIn("manual binary/hook audit", result)

    def test_rejects_changed_fixed_fields(self):
        mutations = (
            ("geometry", lambda i: i["sections"][0].update(raw_offset=0x600)),
            ("flags", lambda i: i["sections"][0].update(characteristics=0x40000040)),
            ("base", lambda i: i.update(image_base=0x400000)),
            ("entry", lambda i: i.update(entry_point=0x1000)),
            ("image size", lambda i: i.update(image_size=0x400000)),
            ("file size", lambda i: i.update(file_size=100)),
            ("file flags", lambda i: i.update(file_characteristics=0)),
        )
        for label, mutate in mutations:
            with self.subTest(label=label):
                info = self.synthetic_info()
                mutate(info)
                with self.assertRaises(profile.PEError):
                    profile.make_fleetops_profile(info)

    @unittest.skipUnless(fixture.is_file(), "Oct 23 FleetOps fixture unavailable")
    def test_oct23_fixture_and_cli(self):
        data = self.fixture.read_bytes()
        info = profile.parse_image(data)
        self.assertEqual(info["file_size"], 3043840)
        self.assertEqual(info["file_crc32"], f"{zlib.crc32(data):08X}")
        expected = profile.make_fleetops_profile(info)
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / "fleetops.profile"
            subprocess.run([sys.executable, str(SCRIPT), "fleetops-profile", str(self.fixture),
                            "--output", str(output)], check=True, capture_output=True, text=True)
            self.assertEqual(output.read_text(encoding="ascii"), expected)
            tokens = expected.splitlines()[-1].split()
            self.assertEqual(tokens, ["roots_fleetops", f"{info['timestamp']:08X}",
                                      "00322000", "002E7200", info["file_crc32"]])
            modified = bytearray(data)
            pe_offset = int.from_bytes(modified[0x3C:0x40], "little")
            modified[pe_offset + 8:pe_offset + 12] = (0x11223344).to_bytes(4, "little")
            changed = profile.parse_image(bytes(modified))
            changed_row = profile.make_fleetops_profile(changed).splitlines()[-1]
            self.assertEqual(changed_row.split()[1], "11223344")
            self.assertNotEqual(changed["file_crc32"], info["file_crc32"])
            self.assertEqual(changed_row.split()[-1], changed["file_crc32"])
            truncated = Path(tmp) / "truncated.dll"
            truncated.write_bytes(data[:-1])
            failed = subprocess.run([sys.executable, str(SCRIPT), "fleetops-profile", str(truncated),
                                     "--output", str(output)], capture_output=True, text=True)
            self.assertEqual(failed.returncode, 1)
            self.assertIn("raw span exceeds", failed.stderr)


if __name__ == "__main__":
    unittest.main()
