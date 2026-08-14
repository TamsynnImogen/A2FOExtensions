#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import unittest


ROOT = Path(__file__).parents[1]
REFERENCE_PATH = ROOT / "docs" / "modder-command-reference.md"


class ModderDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.reference = REFERENCE_PATH.read_text(encoding="utf-8")

    def test_root_readme_links_the_reference(self) -> None:
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("docs/modder-command-reference.md", readme)

    def test_every_native_module_readme_is_catalogued(self) -> None:
        readmes = sorted((ROOT / "modules").glob("*/README.md"))
        self.assertTrue(readmes)
        for readme in readmes:
            relative = readme.relative_to(ROOT).as_posix()
            with self.subTest(module=readme.parent.name):
                self.assertIn(readme.parent.name, self.reference)
                self.assertIn(f"../{relative}", self.reference)

    def test_every_public_command_family_is_indexed(self) -> None:
        required_terms = (
            # info.ini and extension configuration
            "SettingsDirectory", "DefaultGameSpeed", "requiredX",
            "rejectX", "activeX", "SafeMode", "nativeCampaign",
            "nativeMission", "thumbnail", "objectives", "unlocked",
            # RTS_CFG and GUI configuration
            "SHOWMETHEMONEY_DILITHIUM", "SHOWMETHEMONEY_TRITANIUM",
            "SHOWMETHEMONEY_METAL", "SHOWMETHEMONEY_SUPPLIES",
            "SHOWMETHEMONEY_CREW", "firearc", "fireArcBoundaryColor",
            "fireArcCenterColor", "fireArcValidTargetColor",
            "infoSingleCaptainTextArea", "infoSingleRegistryTextArea",
            "captainNameColor", "shipRegistryColor",
            # Race, object, identity, subsystem, and swarm commands
            "factionTextureSuffix", "alwaysShowShields",
            "possibleCaptainNames", "possibleCraftRegistry",
            "sensorMeshX", "engineMeshX", "weaponMeshX",
            "lifeSupportMeshX", "shieldGeneratorMeshX",
            "sensorMeshXexplosion", "engineMeshXexplosion",
            "weaponMeshXexplosion", "lifeSupportMeshXexplosion",
            "shieldGeneratorMeshXexplosion", "swarmX", "swarmXCount",
            "swarmXScale", "swarmXRadius", "swarmXMinRadius",
            "swarmXMaxRadius", "swarmXMinSpeed", "swarmXMaxSpeed",
            "swarmXTurnRate", "swarmXHardpoint",
            "swarmXHardpointCapacity", "swarmXInteraction",
            "swarmXInteractionCapacity", "swarmXInteractionChance",
            "swarmXInteractionTime", "swarmXInteractionRadius",
            "swarmXReturnToHardpoint", "swarmXAvoidHost",
            "swarmXHostClearance", "swarmXSeparation",
            # Turrets, replacements, A1, and production
            "turretX", "turretHardpointX", "turretYawMin",
            "turretYawMax", "turretPitchMin", "turretPitchMax",
            "turretYawRate", "turretPitchRate", "turretRestYaw",
            "turretRestPitch", "turretReturnToRest", "wreckage",
            "wreckageChance", "cocoon", "upgradeLevel",
            "maximumUpgrades", "officerGain", "constructItemX",
            "yardItemX", "researchItemX", "evolveItemX", "buildItemX",
            "tierTBuildItemX",
            # Weapon commands
            "canDamageShields", "canDamageHull",
            "shieldDamageModifier", "hullDamageModifier", "fireArcMode",
            "fireArcYaw", "fireArcPitch", "fireArcYawAngle",
            "fireArcPitchAngle", "fireArcAngle", "fireArcCenter",
            "fireArcWidth", "shotDelay0", "shotDelay63",
            "saveFireCyclePoint", "shotCycleResetTime",
            # Edit menu
            "menuNameX", "menuTitle", "itemX", "forceToNeutral",
            # Emissive materials
            "textureX", "emissiveWarp", "emissiveImpulse",
            "emissiveShields", "emissiveLifeSupport", "emissiveSensors",
            "emissiveWeapons", "emissiveXWarp", "emissiveXImpulse",
            "emissiveXShields", "emissiveXLifeSupport",
            "emissiveXSensors", "emissiveXWeapons",
            # Damage and logo decals
            "damageThreshold", "damageDecalPreview", "<prefix>ScorchX",
            "<prefix>ScorchXHardpoint", "<prefix>ScorchXOffset",
            "<prefix>ScorchXRotation", "<prefix>ScorchXSize",
            "scorchTextureX", "sensorsTargetHardpoints",
            "enginesTargetHardpoints", "weaponsTargetHardpoints",
            "lifeSupportTargetHardpoints",
            "shieldGeneratorTargetHardpoints", "hullTargetHardpoints",
            "logoFileNames", "logoDecalXHardpoint", "logoDecalXSuffix",
            "logoDecalXOffset", "logoDecalXRotation", "logoDecalXSize",
            "logoDecalXColourKey", "logoDecalXFlipU", "ScaleSOD",
            # Lua and tool entry points
            "a2fo.require_api", "a2fo.has_capability", "a2fo.log",
            "a2fo.configure_upgrade_pods", "a2fo.on_classlabel",
            "a2fo.on_evolver_cocoon", "a2fo.on_object_destroyed",
            "odf:get_string", "event:roll_percent", "--inspect",
            "--write", "--report", "--show-unsafe",
        )
        for term in required_terms:
            with self.subTest(term=term):
                self.assertIn(term, self.reference)


if __name__ == "__main__":
    unittest.main()
