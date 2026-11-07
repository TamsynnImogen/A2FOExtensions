"""Focused headless tests for the static ODF corpus comparator."""
from pathlib import Path
import importlib.util
import json
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "audit_stl_odf_commands.py"
spec = importlib.util.spec_from_file_location("audit_stl_odf_commands", SCRIPT)
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class ParserTests(unittest.TestCase):
    def test_comments_multiline_sections_includes_and_duplicates(self):
        parsed = audit.parse_odf('''// weapon9 = 1
[Ship]
#include "base.odf"
weapon1 = "a // literal /* literal
continued" // useful
weapon1 = 2 // duplicate
cost = 1,
  2, 3
// weapon2 = 8
/broken = 4
?? unknown
''', "odf/test.odf")
        self.assertEqual([a["key"] for a in parsed["assignments"]], ["weapon1", "weapon1", "cost"])
        self.assertIn("continued", parsed["assignments"][0]["value"])
        self.assertIn("// literal", parsed["assignments"][0]["value"])
        self.assertEqual(parsed["assignments"][0]["section"], "Ship")
        self.assertEqual(parsed["assignments"][0]["inline_comments"], ["useful"])
        self.assertEqual(parsed["assignments"][2]["value"], "1,\n2, 3")
        self.assertEqual(len(parsed["includes"]), 1)
        self.assertEqual([a["key"] for a in parsed["commented"]], ["weapon9", "weapon2"])
        self.assertEqual([a["reason"] for a in parsed["anomalies"]], ["malformed_assignment", "unknown_syntax"])

    def test_crlf_cp1252_and_empty(self):
        data = b'classLabel = "frigate"\r\nname = "Caf\xe9"\r\n'
        text, encoding = audit.decode_odf(data)
        self.assertEqual(encoding, "cp1252")
        parsed = audit.parse_odf(text, "odf/a.odf")
        self.assertEqual([a["line"] for a in parsed["assignments"]], [1, 2])
        self.assertEqual(parsed["classlabels"][0]["value"], '"frigate"')
        self.assertEqual(audit.parse_odf("", "empty.odf")["assignments"], [])

    def test_section_after_assignment_and_undefined_cp1252_byte(self):
        parsed = audit.parse_odf('first = 1\n[Next]\nsecond = 2\n/* hidden = 3 */\n', "odf/a.odf")
        self.assertEqual([a["value"] for a in parsed["assignments"]], ["1", "2"])
        self.assertEqual(parsed["assignments"][1]["section"], "Next")
        self.assertEqual([a["key"] for a in parsed["assignments"]], ["first", "second"])
        text, encoding = audit.decode_odf(b"name = \x81\r\n")
        self.assertEqual(encoding, "latin-1-fallback")
        self.assertEqual(audit.parse_odf(text, "odf/b.odf")["assignments"][0]["line"], 1)

    def test_family_index_is_not_new(self):
        self.assertEqual(audit.family("weapon99"), audit.family("weapon1"))
        self.assertEqual(audit.family("weapon12Damage"), audit.family("weapon1Damage"))
        self.assertNotEqual(audit.family("foo1Bar"), audit.family("foo2Bar"))

    def test_malformed_quote_recovers_next_assignment(self):
        parsed = audit.parse_odf('weapon10 = "broken\nweaponHardpoints10 = "hp26"\n'
                                 'PhysicsFile = "k_bop_physics.odf""\n'
                                 'buildTime = 50.0\n', "odf/broken.odf")
        self.assertEqual([a["key"] for a in parsed["assignments"]],
                         ["weapon10", "weaponHardpoints10", "PhysicsFile", "buildTime"])
        self.assertEqual(parsed["assignments"][0]["parse_status"], "unclosed_quote_recovered")
        self.assertEqual(parsed["assignments"][1]["parse_status"], "after_unclosed_quote")
        self.assertEqual(sum(a["reason"] == "recovered_after_unclosed_quote" for a in parsed["anomalies"]), 2)
        valid = audit.parse_odf('name = "first line\nsecond line"\nnext = 1\n', "odf/valid.odf")
        self.assertEqual(len(valid["assignments"]), 2)
        self.assertFalse(valid["anomalies"])

    def test_classlabel_canonical_token(self):
        parsed = audit.parse_odf('classLabel = "ship"; // note\nclassLabel = frigate;\n', "odf/c.odf")
        self.assertEqual([a["canonical_value"] for a in parsed["classlabels"]], ["ship", "frigate"])
        self.assertEqual(parsed["classlabels"][0]["value"], '"ship";')


class IntegrationTests(unittest.TestCase):
    def test_synthetic_corpora_and_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            base = Path(tmp)
            mods = base / "Mods"
            for name in audit.CORPORA:
                (mods / name / "odf").mkdir(parents=True)
            (mods / "STL" / "odf" / "stl.ODF").write_text('weapon99 = 1\nnovelX = 2\nclassLabel = "stltype"\n// commentOnly = 5\n')
            (mods / "STA1 Classic Mod" / "odf" / "a.odf").write_text("weapon1 = 2\n")
            (mods / "STA2 Classic Mod" / "odf" / "old_chainp.odf").write_text("oldCommand = 1\n")
            (mods / "STL" / "addon").mkdir()
            (mods / "STL" / "addon" / "empty.odf").write_bytes(b"")
            (mods / "STL" / "odf" / "ODF.fpq").write_bytes(b"archive")
            (mods / "STL" / "odf" / "system_BACKUP.rar").write_bytes(b"backup")
            (mods / "STL" / "odf" / "backup").mkdir()
            (mods / "STL" / "odf" / "backup" / "no.odf").write_text("ignore = 1")
            output = base / "out"
            result = subprocess.run([sys.executable, str(SCRIPT), "--mods-root", str(mods), "--output", str(output)],
                                    capture_output=True, text=True, check=True)
            self.assertIn('"files": 2', result.stdout)
            candidates = json.loads((output / "candidates.json").read_text())["occurrences"]
            self.assertEqual({a["key"] for a in candidates}, {"novelX", "classLabel"})
            matrix = json.loads((output / "command-matrix.json").read_text())["rows"]
            weapon = next(r for r in matrix if r["key"] == "weapon99")
            self.assertEqual(weapon["family_comparison"]["STA1 Classic Mod"]["assignment_count"], 1)
            manifest = json.loads((output / "manifest.json").read_text())
            self.assertEqual(len(manifest["corpora"]["STL"]["files"]), 2)
            self.assertEqual(len(manifest["corpora"]["STL"]["archives"]), 1)
            self.assertEqual(len(manifest["corpora"]["STA2 Classic Mod"]["files"]), 1)
            self.assertEqual(manifest["corpora"]["STL"]["excluded_paths"][0]["reason"], "backup_directory")
            self.assertEqual({x["path"] for x in manifest["corpora"]["STL"]["excluded_paths"]},
                             {"odf/backup", "odf/system_BACKUP.rar"})
            self.assertIn("FO_ROOT", weapon["corpora"])
            self.assertIn("FO_ROOT", weapon["family_comparison"])
            summary = json.loads((output / "summary.json").read_text())
            self.assertEqual(summary["corpora"]["STA1 Classic Mod"]["exact_keys"], 1)
            self.assertEqual(summary["corpora"]["STA1 Classic Mod"]["families"], 1)
            self.assertEqual(summary["corpora"]["FO_ROOT"]["exact_keys"], 0)
            self.assertTrue(json.loads((output / "classlabel-matrix.json").read_text())["rows"][0]["absent_from_baselines"])
            self.assertEqual(json.loads((output / "comments.json").read_text())["comment_only_occurrences"][0]["key"], "commentOnly")


if __name__ == "__main__":
    unittest.main()
