"""Contract checks for current and legacy Bink surface result generations."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
import rust_dll_linker as linker  # noqa: E402


class NativeSurfaceOracleTests(unittest.TestCase):
    ORACLES = {
        "native-current": ROOT / "build/bink-legacy-stage3/native-current.json",
        "native-legacy": ROOT / "build/bink-legacy-stage3/native-legacy.json",
        "rust-current": ROOT / "build/bink-legacy-stage3/rust-current.json",
        "rust-legacy": ROOT / "build/bink-legacy-stage3/rust-legacy.json",
    }

    def load_oracles(self):
        missing = [str(path) for path in self.ORACLES.values() if not path.is_file()]
        if missing:
            self.skipTest(f"local native/Rust surface oracle fixtures missing: {missing}")
        return {name: json.loads(path.read_text()) for name, path in self.ORACLES.items()}

    def test_both_generations_match_all_208_native_cases_exactly(self):
        reports = self.load_oracles()
        for generation in ("current", "legacy"):
            native = reports[f"native-{generation}"]
            rust = reports[f"rust-{generation}"]
            with self.subTest(generation=generation):
                self.assertEqual(native, rust)
                for report in (native, rust):
                    self.assertEqual(report["count"], 208)
                    self.assertEqual(len(report["cases"]), 208)
                    for case in report["cases"]:
                        self.assertEqual(case["calls"], 1)
                        self.assertEqual(case["initial_size"], 32)

    def test_current_and_legacy_generations_differ_on_known_rgb_cases(self):
        reports = self.load_oracles()
        current = {case["i"]: case for case in reports["native-current"]["cases"]}
        legacy = {case["i"]: case for case in reports["native-legacy"]["cases"]}
        signatures = (
            (130, "RGB24"),
            (162, "RGB32"),
            (106, "RGB565"),
        )
        for case_index, label in signatures:
            with self.subTest(format=label):
                current_case, legacy_case = current[case_index], legacy[case_index]
                self.assertEqual(
                    tuple(current_case[key] for key in ("bits", "fourcc", "r", "g", "b", "a")),
                    tuple(legacy_case[key] for key in ("bits", "fourcc", "r", "g", "b", "a")),
                )
                self.assertNotEqual(current_case["result"], legacy_case["result"])


class SurfaceExportContractTests(unittest.TestCase):
    BASE_197 = ROOT / "core/A2FOExtensions.def"
    PRIVATE_210 = ROOT / "core/A2FOExtensions.bink.def"
    SURFACE_212 = ROOT / "core/A2FOExtensions.bink-surface.def"

    def test_197_198_210_and_212_export_contracts_validate(self):
        base_198 = self.BASE_197.read_bytes()
        base_197 = b"".join(
            line for line in base_198.splitlines(keepends=True)
            if not line.split() or line.split()[0] != b"A2FO_InitializeNoUnwind"
        )
        for data, expected_count in (
            (base_197, 197),
            (base_198, 198),
            (self.PRIVATE_210.read_bytes(), 210),
            (self.SURFACE_212.read_bytes(), 212),
        ):
            with self.subTest(count=expected_count):
                self.assertEqual(len(linker.validate_contract(data)), expected_count)

    def test_surface_contract_keeps_the_210_export_prefix_and_adds_two_exact_aliases(self):
        private = self.PRIVATE_210.read_bytes()
        surface = self.SURFACE_212.read_bytes()
        self.assertTrue(surface.startswith(private))
        self.assertEqual(
            surface[len(private):],
            b'    "_BinkDDSurfaceType@4" = a2fo_bink_dd_surface_type@4 @211\n'
            b'    "_BinkDDSurfaceTypeLegacy@4" = a2fo_bink_dd_surface_type_legacy@4 @212\n',
        )
        exports = linker.validate_contract(surface)
        self.assertEqual(
            exports["_BinkDDSurfaceType@4"].split(),
            ['"_BinkDDSurfaceType@4"', "=", "a2fo_bink_dd_surface_type@4", "@211"],
        )
        self.assertEqual(
            exports["_BinkDDSurfaceTypeLegacy@4"].split(),
            ['"_BinkDDSurfaceTypeLegacy@4"', "=", "a2fo_bink_dd_surface_type_legacy@4", "@212"],
        )

    def test_wrong_surface_alias_ordinal_or_body_and_incomplete_211_contract_rejected(self):
        surface = self.SURFACE_212.read_bytes()
        mutations = (
            surface.replace(b"a2fo_bink_dd_surface_type@4 @211", b"a2fo_bink_dd_surface_type@4 @210", 1),
            surface.replace(b"a2fo_bink_dd_surface_type_legacy@4 @212",
                            b"a2fo_bink_dd_surface_type@4 @212", 1),
            surface.replace(b"a2fo_bink_dd_surface_type_legacy@4 @212",
                            b"a2fo_bink_dd_surface_type_legacy@4 @211", 1),
        )
        for mutation in mutations:
            with self.subTest(mutation=mutation[-80:]):
                with self.assertRaises(ValueError):
                    linker.validate_contract(mutation)

        incomplete_211 = surface.replace(
            b'    "_BinkDDSurfaceTypeLegacy@4" = a2fo_bink_dd_surface_type_legacy@4 @212\n',
            b"",
            1,
        )
        with self.assertRaises(ValueError):
            linker.validate_contract(incomplete_211)


if __name__ == "__main__":
    unittest.main()
