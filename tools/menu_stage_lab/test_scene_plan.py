"""Focused synthetic fixtures for the research-only legacy conversion planner."""

import math
import csv
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from scene_plan import PlanError, plan


def make_plan(text):
    return plan(text.encode(), Path("synthetic.ini"))


class ScenePlanTests(unittest.TestCase):
    def test_asymmetric_group_rotation_and_combined_yaw(self):
        result = make_plan("""
[Group.arbitrary]
At=13,17,-23
Yaw=37
[Ship.craft]
Group=arbitrary
Odf=custom_ship
At=5,-7,11
Yaw=-68
[View.preview]
Group=arbitrary
Eye=-3,2,19
Target=8,-4,-9
Fov=51
[Planet]
Class=custom_planet
At=2,3,4
Radius=6
[Nebula.field]
Group=arbitrary
Odf=custom_gas
At=2,0,-4
Size=7,0,3
""")
        a = result["actors"][0]
        s, c = math.sin(math.radians(37)), math.cos(math.radians(37))
        self.assertAlmostEqual(a["position"][0], 13 + 5*c + 11*s)
        self.assertEqual(a["position"][1], 10)
        self.assertAlmostEqual(a["position"][2], -23 - 5*s + 11*c)
        self.assertEqual(a["yaw_degrees"], -31)
        self.assertAlmostEqual(result["views"][0]["eye"][0], 13 - 3*c + 19*s)
        self.assertEqual(result["views"][0]["eye_marker"], "menu_camera_preview")
        self.assertEqual(result["planet"]["radius"], 6)
        self.assertEqual(len(result["nebula_regions"][0]["rotated_corners"]), 4)
        self.assertIsNone(result["nebula_regions"][0]["resolved_actor_centres"])
        self.assertFalse(result["native_map"])

    def test_unknown_group(self):
        with self.assertRaisesRegex(PlanError, "unknown group"):
            make_plan("[Ship.x]\nOdf=x\nGroup=absent\n")

    def test_nonfinite_and_malformed(self):
        for setting in ("At=1,nan,3", "At=1,2", "Yaw=inf", "Bob=nan"):
            with self.subTest(setting=setting), self.assertRaises(PlanError):
                make_plan("[Ship.x]\nOdf=x\n" + setting + "\n")
        with self.assertRaises(PlanError):
            make_plan("[Ship.x]\nOdf=x\nbroken line\n")

    def test_duplicate_section_and_setting(self):
        with self.assertRaisesRegex(PlanError, "duplicate section"):
            make_plan("[Ship.x]\nOdf=x\n[ship.X]\nOdf=y\n")
        with self.assertRaisesRegex(PlanError, "duplicate setting"):
            make_plan("[Ship.x]\nOdf=x\nodf=y\n")

    def test_cli_outputs_are_research_artifacts(self):
        with tempfile.TemporaryDirectory() as temporary:
            source = Path(temporary) / "map.ini"
            source.write_text("[Ship.x]\nOdf=custom\nAt=1,2,3\n")
            output = Path(temporary) / "out"
            subprocess.run([sys.executable, "-B", str(Path(__file__).with_name("scene_plan.py")),
                            "--map-ini", str(source), "--output", str(output)],
                           check=True, capture_output=True, text=True)
            result = json.loads((output / "conversion-plan.json").read_text())
            self.assertEqual(result["actors"][0]["position"], [1, 2, 3])
            self.assertEqual(len(result["source"]["sha256"]), 64)
            with (output / "actors.csv").open() as stream:
                self.assertEqual(len(list(csv.DictReader(stream))), 1)
            with (output / "camera-markers.csv").open() as stream:
                self.assertEqual(len(list(csv.DictReader(stream))), 0)
            self.assertFalse(list(output.glob("*.bzn")))


if __name__ == "__main__":
    unittest.main()
