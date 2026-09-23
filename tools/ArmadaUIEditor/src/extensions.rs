use crate::model::UiRect;

#[derive(Debug, Clone, Copy)]
pub struct RectangleTemplate {
    pub section: &'static str,
    pub label: &'static str,
    pub key: &'static str,
    pub default: UiRect,
    pub description: &'static str,
}

#[derive(Debug, Clone, Copy)]
pub struct ColorTemplate {
    pub section: &'static str,
    pub label: &'static str,
    pub key: &'static str,
    pub default: [f32; 3],
}

pub const RECTANGLE_TEMPLATES: &[RectangleTemplate] = &[
    RectangleTemplate {
        section: "Additional resources",
        label: "Tritanium row",
        key: "resource_6",
        default: UiRect {
            x: 50,
            y: 30,
            width: 120,
            height: 18,
        },
        description: "A2FOResources second-row tritanium balance.",
    },
    RectangleTemplate {
        section: "Additional resources",
        label: "Supply row",
        key: "resource_7",
        default: UiRect {
            x: 250,
            y: 30,
            width: 120,
            height: 18,
        },
        description: "A2FOResources second-row supply balance.",
    },
    RectangleTemplate {
        section: "Additional resources",
        label: "Credits row",
        key: "resource_8",
        default: UiRect {
            x: 450,
            y: 30,
            width: 120,
            height: 18,
        },
        description: "A2FOResources second-row credits balance.",
    },
    RectangleTemplate {
        section: "Additional resources",
        label: "Collective connections row",
        key: "resource_9",
        default: UiRect {
            x: 650,
            y: 30,
            width: 120,
            height: 18,
        },
        description: "A2FOResources second-row collective-connections balance.",
    },
    RectangleTemplate {
        section: "Identity",
        label: "Captain row / A2FO anchor",
        key: "infoSingleCaptainTextArea",
        default: UiRect {
            x: 386,
            y: 130,
            width: 340,
            height: 20,
        },
        description:
            "Captain text and the coordinate anchor used by A2FO's selected-panel additions.",
    },
    RectangleTemplate {
        section: "Identity",
        label: "Registry row",
        key: "infoSingleRegistryTextArea",
        default: UiRect {
            x: 386,
            y: 154,
            width: 340,
            height: 20,
        },
        description: "Craft registry text from A2FOCraftIdentity.",
    },
    RectangleTemplate {
        section: "Ammunition",
        label: "Photon torpedoes",
        key: "infoSinglePhotonTorpedoesTextArea",
        default: UiRect {
            x: 386,
            y: 186,
            width: 340,
            height: 20,
        },
        description: "Photon-torpedo store label, icon/value, or capacity bar.",
    },
    RectangleTemplate {
        section: "Ammunition",
        label: "Quantum torpedoes",
        key: "infoSingleQuantumTorpedoesTextArea",
        default: UiRect {
            x: 386,
            y: 214,
            width: 340,
            height: 20,
        },
        description: "Quantum-torpedo store label, icon/value, or capacity bar.",
    },
    RectangleTemplate {
        section: "Ammunition",
        label: "Shuttle craft",
        key: "infoSingleShuttleCraftTextArea",
        default: UiRect {
            x: 386,
            y: 242,
            width: 340,
            height: 20,
        },
        description: "Shuttle-craft store label, icon/value, or capacity bar.",
    },
    RectangleTemplate {
        section: "Directional shields",
        label: "Forward / aft row",
        key: "infoSingleDirectionalShieldsForwardAftTextArea",
        default: UiRect {
            x: 386,
            y: 270,
            width: 340,
            height: 18,
        },
        description: "Numeric forward and aft shield fallback and tooltip text area.",
    },
    RectangleTemplate {
        section: "Directional shields",
        label: "Port / starboard row",
        key: "infoSingleDirectionalShieldsPortStarboardTextArea",
        default: UiRect {
            x: 386,
            y: 290,
            width: 340,
            height: 18,
        },
        description: "Numeric port and starboard shield fallback text area.",
    },
    RectangleTemplate {
        section: "Directional shields",
        label: "Directional shield graphic",
        key: "infoSingleDirectionalShieldsGraphicArea",
        default: UiRect {
            x: 26,
            y: 56,
            width: 128,
            height: 128,
        },
        description: "Origin for the dsf/dsb/dsl/dsr arc-ring sprites; keep this 128 by 128.",
    },
    RectangleTemplate {
        section: "Status",
        label: "Native shield hover area",
        key: "infoSingleShieldBarArea",
        default: UiRect {
            x: 26,
            y: 126,
            width: 103,
            height: 10,
        },
        description: "Native shield bar also used by A2FO's shield tooltip.",
    },
    RectangleTemplate {
        section: "Status",
        label: "Experience bar",
        key: "infoSingleExperienceBarArea",
        default: UiRect {
            x: 10,
            y: 148,
            width: 512,
            height: 8,
        },
        description: "Optional ranked-craft experience progress bar.",
    },
    RectangleTemplate {
        section: "Fleet Operations",
        label: "System background display",
        key: "infoSingleSystemsDisplay",
        default: UiRect {
            x: 10,
            y: 18,
            width: 512,
            height: 128,
        },
        description: "Destination for the selected craft's <model>_si system-background image.",
    },
    RectangleTemplate {
        section: "Fleet Operations",
        label: "System icon size",
        key: "infoSingleSystemsIcon",
        default: UiRect {
            x: 0,
            y: 0,
            width: 34,
            height: 34,
        },
        description: "Fleet Operations system/weapon icon reference rectangle.",
    },
];

pub const COLOR_TEMPLATES: &[ColorTemplate] = &[
    ColorTemplate {
        section: "Identity",
        label: "Captain name",
        key: "captainNameColor",
        default: [1.0, 0.0, 1.0],
    },
    ColorTemplate {
        section: "Identity",
        label: "Ship registry",
        key: "shipRegistryColor",
        default: [1.0, 0.0, 1.0],
    },
    ColorTemplate {
        section: "Identity",
        label: "Ship name",
        key: "shipNameColor",
        default: [1.0, 1.0, 1.0],
    },
    ColorTemplate {
        section: "Identity",
        label: "Selected-panel fallback text",
        key: "infoTextColor",
        default: [1.0, 1.0, 1.0],
    },
    ColorTemplate {
        section: "Photon torpedoes",
        label: "Healthy",
        key: "photonTorpedoColor",
        default: [0.0, 1.0, 0.0],
    },
    ColorTemplate {
        section: "Photon torpedoes",
        label: "Low",
        key: "photonTorpedoLowColor",
        default: [1.0, 1.0, 0.0],
    },
    ColorTemplate {
        section: "Photon torpedoes",
        label: "Critical",
        key: "photonTorpedoCriticalColor",
        default: [1.0, 0.0, 0.0],
    },
    ColorTemplate {
        section: "Quantum torpedoes",
        label: "Healthy",
        key: "quantumTorpedoColor",
        default: [0.0, 1.0, 0.0],
    },
    ColorTemplate {
        section: "Quantum torpedoes",
        label: "Low",
        key: "quantumTorpedoLowColor",
        default: [1.0, 1.0, 0.0],
    },
    ColorTemplate {
        section: "Quantum torpedoes",
        label: "Critical",
        key: "quantumTorpedoCriticalColor",
        default: [1.0, 0.0, 0.0],
    },
    ColorTemplate {
        section: "Shuttle craft",
        label: "Healthy",
        key: "shuttleCraftColor",
        default: [0.0, 1.0, 0.0],
    },
    ColorTemplate {
        section: "Shuttle craft",
        label: "Low",
        key: "shuttleCraftLowColor",
        default: [1.0, 1.0, 0.0],
    },
    ColorTemplate {
        section: "Shuttle craft",
        label: "Critical",
        key: "shuttleCraftCriticalColor",
        default: [1.0, 0.0, 0.0],
    },
    ColorTemplate {
        section: "Directional shields",
        label: "Healthy",
        key: "directionalShieldColor",
        default: [0.1, 1.0, 0.1],
    },
    ColorTemplate {
        section: "Directional shields",
        label: "Low",
        key: "directionalShieldLowColor",
        default: [1.0, 0.5, 0.0],
    },
    ColorTemplate {
        section: "Directional shields",
        label: "Critical",
        key: "directionalShieldCriticalColor",
        default: [1.0, 0.05, 0.02],
    },
    ColorTemplate {
        section: "Experience",
        label: "Experience fill",
        key: "experienceBarColor",
        default: [0.2, 0.65, 1.0],
    },
    ColorTemplate {
        section: "Experience",
        label: "Experience background",
        key: "experienceBarBackgroundColor",
        default: [0.25, 0.25, 0.25],
    },
    ColorTemplate {
        section: "System icons",
        label: "Healthy",
        key: "systemIconHealthyColor",
        default: [0.2, 1.0, 0.2],
    },
    ColorTemplate {
        section: "System icons",
        label: "Low",
        key: "systemIconLowColor",
        default: [1.0, 0.9, 0.0],
    },
    ColorTemplate {
        section: "System icons",
        label: "Critical",
        key: "systemIconCriticalColor",
        default: [1.0, 0.15, 0.0],
    },
    ColorTemplate {
        section: "System icons",
        label: "Disabled",
        key: "systemIconDisabledColor",
        default: [0.25, 0.55, 1.0],
    },
    ColorTemplate {
        section: "System icons",
        label: "Destroyed",
        key: "systemIconDestroyedColor",
        default: [1.0, 0.0, 1.0],
    },
    ColorTemplate {
        section: "Native values",
        label: "Special energy",
        key: "specialEnergyIconColor",
        default: [1.0, 1.0, 0.0],
    },
    ColorTemplate {
        section: "Native values",
        label: "Officer icon/value",
        key: "officerIconColor",
        default: [1.0, 0.5, 0.0],
    },
];

// Match A2FOCraftIdentity's panel element names. The two panel keys are
// deliberately separate entries so resizing tall does not modify medium.
macro_rules! panel_catalogue {
    ($($name:literal => [$x:literal, $y:literal, $w:literal, $h:literal]),* $(,)?) => {
        const PANEL_RECTANGLES: &[RectangleTemplate] = &[
            $(RectangleTemplate {
                section: "Medium panel parts", label: $name,
                key: concat!("infoSingle", $name, "Area"),
                default: UiRect { x: $x, y: $y, width: $w, height: $h },
                description: "Independent medium-panel rectangle. Text keeps the native font.",
            },)*
            $(RectangleTemplate {
                section: "Tall panel parts", label: $name,
                key: concat!("infoBuild", $name, "Area"),
                default: UiRect { x: $x, y: $y, width: $w, height: $h },
                description: "Independent tall-panel rectangle; absent properties inherit medium.",
            },)*
        ];
        const PANEL_COLORS: &[ColorTemplate] = &[
            $(ColorTemplate {
                section: "Medium panel parts", label: $name,
                key: concat!("infoSingle", $name, "Color"), default: [1.0, 1.0, 1.0],
            },)*
            $(ColorTemplate {
                section: "Tall panel parts", label: $name,
                key: concat!("infoBuild", $name, "Color"), default: [1.0, 1.0, 1.0],
            },)*
        ];
    };
}
panel_catalogue! {
    "CaptainText" => [386, 130, 200, 20],
    "RegistryText" => [386, 154, 200, 20],
    "PhotonTorpedoesText" => [386, 186, 200, 20],
    "PhotonTorpedoesLabelText" => [386, 186, 180, 20],
    "PhotonTorpedoesValueText" => [580, 186, 100, 20],
    "PhotonTorpedoesIcon" => [350, 186, 20, 20],
    "PhotonTorpedoesBar" => [580, 186, 140, 12],
    "QuantumTorpedoesText" => [386, 210, 200, 20],
    "QuantumTorpedoesLabelText" => [386, 210, 180, 20],
    "QuantumTorpedoesValueText" => [580, 210, 100, 20],
    "QuantumTorpedoesIcon" => [350, 210, 20, 20],
    "QuantumTorpedoesBar" => [580, 210, 140, 12],
    "ShuttleCraftText" => [386, 234, 200, 20],
    "ShuttleCraftLabelText" => [386, 234, 180, 20],
    "ShuttleCraftValueText" => [580, 234, 100, 20],
    "ShuttleCraftIcon" => [350, 234, 20, 20],
    "ShuttleCraftBar" => [580, 234, 140, 12],
    "DirectionalShieldsGraphic" => [26, 26, 128, 128],
    "DirectionalShieldsForwardAftText" => [386, 266, 340, 20],
    "DirectionalShieldsPortStarboardText" => [386, 288, 340, 20],
    "DirectionalShieldsForward" => [52, 26, 76, 20],
    "DirectionalShieldsForwardValueText" => [58, 48, 64, 18],
    "DirectionalShieldsAft" => [52, 134, 76, 20],
    "DirectionalShieldsAftValueText" => [58, 114, 64, 18],
    "DirectionalShieldsPort" => [26, 52, 20, 76],
    "DirectionalShieldsPortValueText" => [46, 81, 44, 18],
    "DirectionalShieldsStarboard" => [134, 52, 20, 76],
    "DirectionalShieldsStarboardValueText" => [90, 81, 44, 18],
    "ExperienceBar" => [386, 260, 334, 10],
}

pub fn rectangle_templates() -> impl Iterator<Item = &'static RectangleTemplate> {
    RECTANGLE_TEMPLATES
        .iter()
        .chain(PANEL_RECTANGLES.iter().filter(|candidate| {
            !RECTANGLE_TEMPLATES
                .iter()
                .any(|base| base.key == candidate.key)
        }))
}
pub fn color_templates() -> impl Iterator<Item = &'static ColorTemplate> {
    COLOR_TEMPLATES.iter().chain(PANEL_COLORS.iter())
}

pub fn is_known_rectangle(key: &str) -> bool {
    rectangle_templates().any(|template| template.key.eq_ignore_ascii_case(key))
}

pub fn preview_text(key: &str) -> Option<(&'static str, Option<&'static str>)> {
    let key = key.to_ascii_lowercase();
    let key = key
        .strip_prefix("infobuild")
        .map(|suffix| format!("infosingle{suffix}"))
        .unwrap_or(key);
    match key.as_str() {
        "resource_6" => Some(("Tritanium 1200", None)),
        "resource_7" => Some(("Supply 80", None)),
        "resource_8" => Some(("Credits 450", None)),
        "resource_9" => Some(("Connections 12", None)),
        "infosinglecaptaintextarea" => Some(("Captain Picard", Some("captainNameColor"))),
        "infosingleregistrytextarea" => Some(("NCC-1701-D", Some("shipRegistryColor"))),
        "infosinglephotontorpedoestextarea" => {
            Some(("Photon Torpedoes 12/16", Some("photonTorpedoColor")))
        }
        "infosinglequantumtorpedoestextarea" => {
            Some(("Quantum Torpedoes 6/8", Some("quantumTorpedoColor")))
        }
        "infosingleshuttlecrafttextarea" => Some(("Shuttle Craft 4/6", Some("shuttleCraftColor"))),
        "infosingledirectionalshieldsforwardafttextarea" => {
            Some(("Forward 100%   Aft 100%", Some("directionalShieldColor")))
        }
        "infosingledirectionalshieldsportstarboardtextarea" => {
            Some(("Port 100%   Starboard 100%", Some("directionalShieldColor")))
        }
        _ => None,
    }
}

#[cfg(test)]
mod panel_tests {
    use super::*;
    #[test]
    fn every_extension_part_has_distinct_medium_and_tall_keys() {
        let keys: std::collections::HashSet<_> = rectangle_templates().map(|t| t.key).collect();
        assert_eq!(keys.len(), rectangle_templates().count());
        for entry in PANEL_RECTANGLES {
            assert!(keys.contains(entry.key));
        }
        assert!(is_known_rectangle("infoBuildPhotonTorpedoesBarArea"));
        assert!(is_known_rectangle(
            "infoSingleDirectionalShieldsAftValueTextArea"
        ));
        assert!(color_templates().any(|t| t.key == "infoBuildExperienceBarColor"));
    }
}
