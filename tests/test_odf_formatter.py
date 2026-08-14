#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
from pathlib import Path
import sys
import unittest


FORMATTER_PATH = Path(__file__).parents[1] / "tools" / "format_odfs.py"
SPEC = importlib.util.spec_from_file_location("format_odfs", FORMATTER_PATH)
assert SPEC is not None and SPEC.loader is not None
format_odfs = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = format_odfs
SPEC.loader.exec_module(format_odfs)


class OdfFormatterTests(unittest.TestCase):
    def format(self, text: str, encoding: str = "ascii") -> str:
        output, before, after, _ = format_odfs.format_bytes(text.encode(encoding))
        self.assertFalse(before.unsafe_reasons)
        self.assertEqual(
            format_odfs.semantic_signature(before),
            format_odfs.semantic_signature(after),
        )
        return output.decode(encoding)

    def test_groups_commands_and_puts_runtime_last(self) -> None:
        source = (
            'classlabel = "craft"\r\n'
            'damagebase = 10\r\n'
            'maxhealth = 100\r\n'
            'unitname = "Test Ship"\r\n'
        )
        output = self.format(source)
        self.assertLess(output.index("Display and localization"), output.index("unitname"))
        self.assertLess(output.index("unitname"), output.index("maxhealth"))
        self.assertLess(output.index("maxhealth"), output.index("damagebase"))
        self.assertLess(output.index("damagebase"), output.index("classlabel"))
        self.assertIn("\r\n", output)

    def test_keeps_continuation_table_with_its_command(self) -> None:
        source = (
            'classlabel = "cannonimp"\n'
            'damagebase = 20\n'
            '\t"first.odf" 10\n'
            '\t"second.odf" 15\n'
            'wpnname = "Test Weapon"\n'
            'maxhealth = 1\n'
        )
        output = self.format(source)
        damage = output.index("damagebase = 20")
        first = output.index('"first.odf" 10')
        second = output.index('"second.odf" 15')
        runtime = output.index('classlabel = "cannonimp"')
        self.assertLess(damage, first)
        self.assertLess(first, second)
        self.assertLess(second, runtime)

    def test_keeps_subsystem_mesh_and_explosion_in_damage_section(self) -> None:
        source = (
            'classlabel = "craft"\n'
            'engineMesh1explosion = "xfirebsm"\n'
            'engineMesh1 = "nacelle_l"\n'
            'sensorMesh4 = "sensor_dish"\n'
            'unitname = "Damage Mesh Test"\n'
        )
        output = self.format(source)
        section = output.index("Subsystems, damage, and repair")
        engine = output.index('engineMesh1 = "nacelle_l"')
        explosion = output.index('engineMesh1explosion = "xfirebsm"')
        sensor = output.index('sensorMesh4 = "sensor_dish"')
        runtime = output.index('classlabel = "craft"')
        self.assertLess(section, engine)
        self.assertLess(section, explosion)
        self.assertLess(section, sensor)
        self.assertLess(max(engine, explosion, sensor), runtime)

    def test_preserves_repeated_key_order(self) -> None:
        source = (
            'unitname = "Duplicate Test"\n'
            'canrepairshiprepair = 0\n'
            'canrepairshiprepair = 1\n'
            'classlabel = "craft"\n'
        )
        output = self.format(source)
        self.assertLess(
            output.index("canrepairshiprepair = 0"),
            output.index("canrepairshiprepair = 1"),
        )

    def test_keeps_existing_comment_with_command(self) -> None:
        source = (
            'unitname = "Comment Test"\n\n'
            '// This explains the collision command.\n'
            'collisionRadius = 2\n'
            'classlabel = "craft"\n'
            'maxhealth = 5\n'
        )
        output = self.format(source)
        comment = output.index("// This explains")
        command = output.index("collisionRadius = 2")
        self.assertLess(comment, command)
        self.assertLess(command - comment, 100)

    def test_pairs_disabled_original_value_with_replacement(self) -> None:
        source = (
            'unitname = "UI Test"\n'
            '//topbary = 64\n'
            '// Keep this command unscaled.\n'
            'topbary = 96\n'
            'buttonslot = 1\n'
            'classlabel = "replaceweapon"\n'
        )
        output = self.format(source)
        old = output.index("//topbary = 64")
        note = output.index("// Keep this command")
        replacement = output.index("topbary = 96")
        self.assertLess(old, note)
        self.assertLess(note, replacement)

    def test_accepts_dotted_star_keys(self) -> None:
        source = "bstar.0 = 36\r\nbstar.0.scale = 250.0\r\n"
        output = self.format(source)
        self.assertIn("bstar.0 = 36", output)
        self.assertIn("bstar.0.scale = 250.0", output)

    def test_preserves_cp1252(self) -> None:
        source = 'unitname = "Café"\r\nclasslabel = "craft"\r\n'
        output = self.format(source, "cp1252")
        self.assertIn("Café", output)

    def test_is_idempotent(self) -> None:
        source = (
            'classlabel = "craft"\r\n'
            'maxhealth = 100\r\n'
            'unitname = "Test Ship"\r\n'
            'weapon1 = "test_weapon"\r\n'
        )
        first = self.format(source)
        second = self.format(first)
        self.assertEqual(first, second)

    def test_gameobject_follows_fleet_ops_ship_template_sections(self) -> None:
        source = (
            'classlabel = "craft"\n'
            'shieldgeneratorhitpoints = 20\n'
            'weaponhardpoints1 = "hp01"\n'
            'weapon1 = "test_weapon"\n'
            'buildtime = 10\n'
            'maxhealth = 100\n'
            'unitname = "Template Ship"\n'
            'eventselect = "ShipSelect"\n'
            'physicsfile = "smooth_physics.odf"\n'
            'xplevel = 1\n'
            'recycletime = 5\n'
            'shieldhit = "shield_hit"\n'
            'has_crew = 1\n'
        )
        output = self.format(source)
        headings = (
            "Display and localization",
            "Construction and costs",
            "Hull, shields, crew, and energy",
            "Subsystems, damage, and repair",
            "Weapons and ordnance",
            "Accuracy, hardpoints, and targeting",
            "Audio and events",
            "Movement and physics",
            "Progression and variants",
            "Resource handling and recycling",
            "Art, animation, and effects",
            "Capabilities and object behavior",
            "Runtime class",
        )
        positions = [output.index(heading) for heading in headings]
        self.assertEqual(positions, sorted(positions))

    def test_weapon_follows_fleet_ops_weapon_template_sections(self) -> None:
        source = (
            'classlabel = "cannonimp"\n'
            'firesound = "test.wav"\n'
            'hitchance = 1\n'
            'shotdelay0 = 1.0\n'
            'ordname = "test_ordnance"\n'
            'wpnname = "Template Weapon"\n'
        )
        output = self.format(source)
        headings = (
            "Display and localization",
            "Weapons and ordnance",
            "Accuracy, hardpoints, and targeting",
            "Audio and events",
            "Runtime class",
        )
        positions = [output.index(heading) for heading in headings]
        self.assertEqual(positions, sorted(positions))

    def test_ordnance_follows_fleet_ops_ordnance_template_sections(self) -> None:
        source = (
            'classlabel = "phaser"\n'
            'shieldcrewmodifier = 0.1\n'
            'damagebase = 12\n'
            'shotspeed = 1000\n'
            'lifespan = 2\n'
            'shieldduration = 0.5\n'
            'sprite = "test_beam"\n'
        )
        output = self.format(source)
        headings = (
            "Art, animation, and effects",
            "Movement and physics",
            "Weapons and ordnance",
            "Runtime class",
        )
        positions = [output.index(heading) for heading in headings]
        self.assertEqual(positions, sorted(positions))
        self.assertNotIn("Additional parameters", output)

    def test_faction_follows_fleet_ops_faction_template_sections(self) -> None:
        source = (
            'preloadunits0 = "test_ship"\n'
            'normaldilithium = 1000\n'
            'repairstrength = 5\n'
            'singleplayermusic = "test.mp3"\n'
            'minimalunits1 = "test_const.odf"\n'
            'interfaceconfiguration = "gui_test.cfg"\n'
            'instantactionslot = 1\n'
            'displaykey = "Test"\n'
            'name = "test"\n'
        )
        output = self.format(source)
        headings = (
            "Display and localization",
            "Interface and controls",
            "Faction rules and starting units",
            "Audio and events",
            "Hull, shields, crew, and energy",
            "Resource handling and recycling",
            "Progression and variants",
        )
        positions = [output.index(heading) for heading in headings]
        self.assertEqual(positions, sorted(positions))

    def test_interface_command_routing_does_not_look_like_movement(self) -> None:
        source = (
            'tooltip = "Attack"\n'
            'preferredposition = 0 0 0 0\n'
            'buttonname = "attack"\n'
            'commandname = "ATTACK"\n'
            'needstarget = 1\n'
            'source = "combat"\n'
            'dest = "has_hitpoints"\n'
        )
        output = self.format(source)
        self.assertIn("Interface and controls", output)
        self.assertIn("Accuracy, hardpoints, and targeting", output)
        self.assertNotIn("Movement and physics", output)
        self.assertNotIn("Additional parameters", output)


if __name__ == "__main__":
    unittest.main()
