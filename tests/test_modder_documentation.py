#!/usr/bin/env python3

from __future__ import annotations

from pathlib import Path
import unittest


ROOT = Path(__file__).parents[1]
REFERENCE_PATH = ROOT / "docs" / "modder-command-reference.md"
GUI_GUIDE_PATH = ROOT / "docs" / "odf-gui-integration-guide.md"


class ModderDocumentationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.reference = REFERENCE_PATH.read_text(encoding="utf-8")

    def test_root_readme_links_the_reference(self) -> None:
        readme = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertIn("docs/modder-command-reference.md", readme)
        self.assertIn("docs/odf-gui-integration-guide.md", readme)

    def test_gui_misc_guide_covers_every_integration_file(self) -> None:
        guide = GUI_GUIDE_PATH.read_text(encoding="utf-8")
        required_terms = (
            "info.ini", "Race ODF", "Weapon ODF", "gui_interface.cfg",
            "infoSingleCaptainTextArea", "infoSingleRegistryTextArea",
            "infoSinglePhotonTorpedoesTextArea",
            "infoSingleQuantumTorpedoesTextArea",
            "infoSingleDirectionalShieldsGraphicArea",
            "infoBuildName", "infoBuildClass",
            "infoSingleShieldBarArea", "infoSingleExperienceBarArea",
            "directionalShieldDisplayMode", "upgradePodMaximumTier",
            "Dynamic_Localized_Strings.h", "GUI_SD_SHIELD_TOOLTIP",
            "GUI_SD_EXPERIENCE_TOOLTIP", "GUI_SD_AMMO_WAITING",
            "GUI_SD_DIRSHIELD_FORWARD_TOOLTIP", "gui_global.spr",
            "dsf shield_forward", "all_interface_font40.tga",
            "`0x80`", "`0x8A`", "resource_6", "resource_9",
            "tritaniumCost", "normalCollectiveConnections",
            "photonTorpedoCost", "quantumTorpedoCost",
            "directionalShields", "forwardShieldPos",
        )
        for term in required_terms:
            with self.subTest(term=term):
                self.assertIn(term, guide)

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
            "| campaign | `background` |",
            # RTS_CFG and GUI configuration
            "SHOWMETHEMONEY_DILITHIUM", "SHOWMETHEMONEY_TRITANIUM",
            "SHOWMETHEMONEY_METAL", "SHOWMETHEMONEY_SUPPLIES",
            "SHOWMETHEMONEY_CREW", "upgradePodMaximumTier", "firearc",
            "fireArcBoundaryColor",
            "fireArcCenterColor", "fireArcValidTargetColor",
            "infoSingleCaptainTextArea", "infoSingleRegistryTextArea",
            "captainNameColor", "shipRegistryColor",
            "specialEnergyIconColor",
            "officerIconColor",
            "infoSingleShieldBarArea", "infoSingleExperienceBarArea",
            "experienceBarColor", "experienceBarBackgroundColor",
            "infoSingleDirectionalShieldsForwardAftTextArea",
            "infoSingleDirectionalShieldsPortStarboardTextArea",
            "infoSingleDirectionalShieldsGraphicArea",
            "infoBuildName", "infoBuildClass",
            "directionalShieldColor", "`dsf`", "`dsb`", "`dsl`", "`dsr`",
            # Race, object, identity, subsystem, and swarm commands
            "factionTextureSuffix", "alwaysShowShields",
            "possibleCaptainNames", "possibleCraftRegistry",
            "shieldTooltip", "shieldVerboseTooltip",
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
            "moduleXPseudoTechnology", "tierTBuildItemX",
            "tritaniumCost", "supplyCost",
            "creditsCost", "collectiveconnectionsCost",
            "normalTritanium", "lotsTritanium", "normalSupply",
            "lotsSupply", "normalCredits", "lotsCredits",
            "normalCollectiveConnections", "lotsCollectiveConnections",
            "resource_6", "resource_9",
            # Weapon commands
            "canDamageShields", "canDamageHull",
            "shieldDamageModifier", "hullDamageModifier", "fireArcMode",
            "fireArcYaw", "fireArcPitch", "fireArcYawAngle",
            "fireArcPitchAngle", "fireArcAngle", "fireArcCenter",
            "fireArcWidth", "shotDelay0", "shotDelay63",
            "saveFireCyclePoint", "shotCycleResetTime",
            "maxPhotonTorpedoes", "photonTorpedoRate",
            "photonTorpedoRechargeMode", "maxQuantumTorpedoes",
            "quantumTorpedoRate", "quantumTorpedoRechargeMode",
            "photonTorpedoCost", "quantumTorpedoCost",
            "torpedoResupply", "torpedoResupplyRange",
            "directionalShields", "forwardShieldStrength",
            "aftShieldStrength", "portShieldStrength",
            "starboardShieldStrength", "forwardShieldPos",
            "aftShieldPos", "portShieldPos", "starboardShieldPos",
            # Edit menu
            "menuNameX", "menuTitle", "itemX", "forceToNeutral",
            # Emissive materials
            "textureX", "emissiveWarp", "emissiveImpulse",
            "emissiveShields", "emissiveLifeSupport", "emissiveSensors",
            "emissiveWeapons", "emissiveXWarp", "emissiveXImpulse",
            "emissiveXShields", "emissiveXLifeSupport",
            "emissiveXSensors", "emissiveXWeapons",
            "A2FO_EMISSIVE_SUFFIX", "A2FO_BUMP_SUFFIX",
            "A2FO_SPECULAR_SUFFIX",
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
            # Tool entry points
            "--inspect",
            "--write", "--report", "--show-unsafe",
        )
        for term in required_terms:
            with self.subTest(term=term):
                self.assertIn(term, self.reference)


if __name__ == "__main__":
    unittest.main()
