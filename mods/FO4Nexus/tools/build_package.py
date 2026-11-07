#!/usr/bin/env python3
"""Build the Nexus data overlay without writing to the parent installation."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
REQUIRED = [
    'A2FOInfoIni', 'A2FOFeaturePack', 'A2FOBuildTooltips',
    'A2FOCraftIdentity', 'A2FODirectionalShields', 'A2FOEnergySystems',
    'A2FOFireArcs', 'A2FOResources', 'A2FOWeaponDamageControls',
]


def write_cfg(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(text.replace('\r\n', '\n').replace('\n', '\r\n').encode('cp1252'))


def integrate(parent, output):
    parent_info = (parent / 'info.ini').read_text(encoding='cp1252')
    active = [name for name in re.findall(
        r'(?im)^active\d+\s*=\s*"([^"\r\n]+)"', parent_info)
        if name not in REQUIRED and name != 'A1Compat']
    info = '''[mod]
Name=Fleet Operations: Nexus
Description=A2FOExtensions showcase. Federation tactical systems: directional shields, torpedo logistics, launcher arcs, crew identities and selected-ship telemetry. Prototype 0.2.
Author=Blade/Apoclaydon; original Fleet Operations by DOCa Cola, Optec, Activision, Maddoc, JanB
Version=0.2.0
AssetVersion=50000
ParentMod="Fleet Ops 4.0"
standalone=0
ignoreParentMaps=0
Screenshot=splash.png
SettingsDirectory="A2FO Nexus"

[modules]
; Mechanics below are mandatory for this mod. Restart after changes.
'''
    info += '\n'.join(f'required{i} = "{name}"' for i, name in enumerate(REQUIRED))
    info += '\n\n; Retain the other applicable FO4 parent selections.\n'
    info += '\n'.join(f'active{i} = "{name}"' for i, name in enumerate(active)) + '\n'
    write_cfg(output / 'info.ini', info)
    # Existing native controls remain authoritative. These rows use vacant
    # space on the right of the 1249x163 medium selected-craft panel.
    layout = {
        'infoSingleCaptainTextArea': '627 18 360 20',
        'infoSingleRegistryTextArea': '627 42 360 20',
        'infoSinglePhotonTorpedoesTextArea': '627 72 360 20',
        'infoSingleQuantumTorpedoesTextArea': '627 72 360 20',
        'infoSingleShuttleCraftTextArea': '0 0 0 0',
        'infoSingleDirectionalShieldsGraphicArea': '1050 14 96 96',
        'infoSingleDirectionalShieldsForwardAftTextArea': '0 0 0 0',
        'infoSingleDirectionalShieldsPortStarboardTextArea': '0 0 0 0',
        'infoSingleDirectionalShieldsForwardValueTextArea': '1070 14 56 18',
        'infoSingleDirectionalShieldsAftValueTextArea': '1070 91 56 18',
        'infoSingleDirectionalShieldsPortValueTextArea': '1005 54 40 18',
        'infoSingleDirectionalShieldsStarboardValueTextArea': '1152 54 40 18',
        'infoSingleExperienceBarArea': '627 101 360 7',
        'captainNameColor': '0.85 0.92 1.0',
        'shipRegistryColor': '0.55 0.8 1.0',
        'photonTorpedoColor': '0.4 0.9 1.0',
        'quantumTorpedoColor': '0.4 0.9 1.0',
        'directionalShieldColor': '0.5 0.9 1.0',
        'directionalShieldLowColor': '1.0 0.65 0.15',
        'directionalShieldCriticalColor': '1.0 0.12 0.08',
        'directionalShieldValueColor': '0.85 0.95 1.0',
        'directionalShieldValueLowColor': '1.0 0.75 0.25',
        'directionalShieldValueCriticalColor': '1.0 0.2 0.15',
        'experienceBarColor': '0.35 0.7 1.0',
        'experienceBarBackgroundColor': '0.08 0.12 0.2',
    }
    for key in list(layout):
        if key.startswith('infoSingle') and key.endswith('Area'):
            layout[key.replace('infoSingle', 'infoBuild', 1)] = '0 0 0 0'
    # Native producer/station panels can use the tall panel. Its first queue tile
    # starts at y31, leaving a top band for two independent identity fields.
    layout['infoBuildCaptainTextArea'] = '627 10 270 18'
    layout['infoBuildRegistryTextArea'] = '915 10 290 18'
    text = (parent / 'misc/gui_interface_misc.cfg').read_text(encoding='cp1252')
    for key in layout:
        text = re.sub(r'(?im)^[ \t]*' + re.escape(key) + r'[ \t]*=.*(?:\n|$)', '', text)
    text += '\n// Nexus tactical readout: identity/status column and shield quadrant display.\n'
    text += '\n'.join(f'{key} = {value}' for key, value in layout.items()) + '\n'
    write_cfg(output / 'misc/gui_interface_misc.cfg', text)
    text = (parent / 'RTS_CFG.h').read_text(encoding='cp1252')
    text = re.sub(r'(?im)^[ \t]*int[ \t]+firearc[ \t]*=.*(?:\n|$)', '', text)
    write_cfg(output / 'RTS_CFG.h', text + '\n// Nexus launcher arcs and weapon hover previews.\nint firearc = 1;\n')
    manifest = {'parent': str(parent), 'required_modules': REQUIRED,
                'active_modules': active, 'files': [],
                'ui_rectangles': {k: list(map(int, v.split())) for k, v in layout.items() if k.endswith('Area')}}
    for relative in ['info.ini', 'misc/gui_interface_misc.cfg', 'RTS_CFG.h']:
        manifest['files'].append({
            'path': relative,
            'source_sha256': hashlib.sha256((parent / relative).read_bytes()).hexdigest(),
            'sha256': hashlib.sha256((output / relative).read_bytes()).hexdigest(),
        })
    (ROOT / 'integration-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent', type=Path, required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'package')
    parser.add_argument('--ui-asset-source', type=Path,
                        default=ROOT / 'assets/Textures')
    args = parser.parse_args()
    parent, output = args.parent.resolve(), args.output.resolve()
    # Both containment directions are forbidden: output must be a separate tree.
    if parent == output or parent in output.parents or output in parent.parents:
        parser.error('output and parent must be separate trees')
    integrate(parent, output)
    subprocess.run([sys.executable, str(ROOT / 'tools/build_content.py'),
                    '--parent', str(parent), '--output', str(output)], check=True)
    subprocess.run([sys.executable, str(ROOT / 'tools/build_ui_assets.py'),
                    '--parent', str(parent), '--output', str(output),
                    '--asset-source', str(args.ui_asset_source.resolve())], check=True)
    for name in ['README.md', 'ACCEPTANCE.md', 'AVATAR_CANDIDATES.md', 'UI_LAYOUT.md']:
        (output / name).write_bytes((ROOT / name).read_bytes())
    print(f'Built Nexus at {output}')


if __name__ == '__main__':
    main()
