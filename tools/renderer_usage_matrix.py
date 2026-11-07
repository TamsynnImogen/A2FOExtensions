#!/usr/bin/env python3
"""Build a source + observed-call matrix without treating missing calls as unused.

Only pass traces collected from the named game to the game-specific switches.
Fixture traces must never be labelled as Armada or Fleet Operations evidence.
"""
import argparse
import csv
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

# These are backend-building-block checks, not implemented COM facade methods.
UI_SLICE = {
    'Clear': 'game facade translates rect/depth/stencil clears; headless pixel-tested; native game unverified',
    'DrawPrimitiveUP': 'game facade translates UP draws; transformed-FVF pixel-tested; native game unverified',
    'DrawIndexedPrimitiveUP': 'game facade translates indexed UP draws and fans; pixel-tested; native game unverified',
    'DrawPrimitive': 'game facade translates vertex-buffer draws; pixel-tested; native game unverified',
    'DrawIndexedPrimitive': 'game facade translates indexed vertex-buffer draws; pixel-tested; native game unverified',
    'CreateTexture': 'mutable legacy texture facade, BC5/BC7 catalog and D3DX loader implemented; native game unverified',
    'SetVertexShader': 'vertex shader handle/state facade uses limited ten-opcode VS1.0/1.1 including DP4 translator; pixel-tested; native game unverified',
    'CreateVertexShader': 'declaration/handle facade and limited ten-opcode VS1.0/1.1 including DP4 translation; pixel-tested; native game unverified',
    'SetRenderState': 'translated render-state subset; runtime parity unverified',
    'SetTextureStageState': 'multistage and DOT3 translation; disabled-alpha cases pixel-tested against installed provider; runtime parity unverified',
    'CreateStateBlock': 'ALL state blocks supported; typed PIXEL/VERTEX blocks unsupported',
    'BeginStateBlock': 'partial state-block recording for represented state setters',
    'EndStateBlock': 'partial state-block recording for represented state setters',
    'SetPixelShader': 'fixed-function handle 0 only; pixel shader programs unsupported',
    'SetViewport': 'game facade translates viewport and legacy pixel-centre convention; pixel-tested; native game unverified',
    'Present': 'D3D11 game facade presentation implemented; native game unverified',
    'Reset': 'windowed reset and managed-resource handling implemented; native game unverified',
}

UNSUPPORTED_DEVICE = set('''SetCursorProperties SetCursorPosition ShowCursor
CreateAdditionalSwapChain GetRasterStatus SetGammaRamp GetGammaRamp
CreateVolumeTexture CreateCubeTexture GetInfo SetPaletteEntries GetPaletteEntries
SetCurrentTexturePalette GetCurrentTexturePalette ProcessVertices CreatePixelShader
DeletePixelShader SetPixelShaderConstant GetPixelShaderConstant
GetPixelShaderFunction DrawRectPatch DrawTriPatch DeletePatch GetFrontBuffer'''.split())

def translation(interface, method):
    if interface == 'IDirect3DDevice8' and method in UNSUPPORTED_DEVICE:
        return 'unsupported by experimental game facade'
    if interface in ('IDirect3DCubeTexture8', 'IDirect3DVolumeTexture8', 'IDirect3DVolume8'):
        return 'unsupported: cube/volume resource facade absent'
    if interface == 'IDirect3DDevice8':
        return UI_SLICE.get(method, 'game facade source implementation; native runtime parity unknown')
    if interface == 'IDirect3D8':
        if method == 'CreateDevice':
            return 'experimental factory returns D3D11-backed IDirect3DDevice8 facade; native game unverified'
        return 'native factory adapter/display query pass-through; runtime parity unknown'
    if interface == 'IDirect3DSwapChain8':
        return 'additional swap chains unsupported; primary facade backbuffer only'
    return 'resource facade source implementation; native runtime parity unknown'


def seen(paths):
    result = {}
    for path in paths:
        for number, line in enumerate(path.read_text().splitlines(), 1):
            try:
                event = json.loads(line)
            except ValueError as error:
                raise SystemExit(f'{path}:{number}: invalid JSON: {error}')
            if event.get('event') == 'call':
                key = event['interface'] + '.' + event['method']
            elif event.get('event') == 'count' and event.get('calls', 0):
                key = event['method']
            else:
                continue
            result.setdefault(key, f'{path.name}:{number}')
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--armada-trace', action='append', type=Path, default=[])
    parser.add_argument('--fleetops-trace', action='append', type=Path, default=[])
    parser.add_argument('--output', type=Path, default=ROOT / 'docs/renderer/compatibility-matrix.csv')
    parser.add_argument('--translation-only', action='store_true',
                        help='update only A2FO_D3D11_translation in the existing matrix')
    args = parser.parse_args()
    if args.translation_only:
        with args.output.open(newline='') as source:
            rows = list(csv.DictReader(source))
            fields = list(rows[0]) if rows else []
        if 'interface' not in fields or 'method' not in fields or 'A2FO_D3D11_translation' not in fields:
            raise SystemExit(f'{args.output}: expected matrix columns are missing')
        for row in rows:
            row['A2FO_D3D11_translation'] = translation(row['interface'], row['method'])
            if row['interface'] == 'IDirect3D8':
                row['A2FO_D3D8_status'] = (
                    'experimental d3d11-test facade factory route; native game unverified'
                    if row['method'] == 'CreateDevice' else
                    'native factory retained for adapter/display queries; runtime parity unknown')
        with args.output.open('w', newline='') as out:
            writer = csv.DictWriter(out, fieldnames=fields)
            writer.writeheader()
            writer.writerows(rows)
        print(f'{len(rows)} translation cells -> {args.output}')
        return
    if not args.armada_trace and not args.fleetops_trace:
        raise SystemExit('refusing to regenerate observation fields without labelled trace input; use --translation-only for scoped translation updates')
    armada, fo = seen(args.armada_trace), seen(args.fleetops_trace)
    references = {}
    with (ROOT / 'docs/renderer/d3d8to11-method-status.csv').open() as source:
        for row in csv.DictReader(source):
            references[(row['interface'], row['method'])] = row
    # Explicit source evidence only, separate from native-game observations.
    sources = [*sorted((ROOT / 'core').glob('renderer_*.inl')),
               ROOT / 'core/nebula_renderer.cpp',
               ROOT / 'modules/A2FORefitMenus/loading_gpu.cpp']
    static = {}
    for source in sources:
        for number, line in enumerate(source.read_text().splitlines(), 1):
            for match in re.finditer(r'->(\w+)\s*\(', line):
                method = match[1]
                candidates = static.setdefault(method, [])
                if len(candidates) < 3:
                    candidates.append(f'{source.relative_to(ROOT)}:{number}')
    thunks = (ROOT / 'renderer/d3d8_compat/generated_thunks.inl').read_text()
    methods = re.findall(r'Call call\(self, Kind::(\w+), \d+, "\w+", "(\w+)"\)', thunks)
    fields = ['interface', 'method', 'ArmadaII_observed', 'FleetOps_observed',
              'A2FO_static_call_candidate', 'd3d8to11_source_status', 'reference_evidence',
              'A2FO_D3D8_status', 'A2FO_D3D11_translation', 'priority']
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open('w', newline='') as out:
        writer = csv.DictWriter(out, fieldnames=fields)
        writer.writeheader()
        for interface, method in methods:
            key = interface + '.' + method
            reference = references.get((interface.removeprefix('I'), method), {})
            evidence = '; '.join(static.get(method, [])) if interface == 'IDirect3DDevice8' else ''
            row = dict(interface=interface, method=method,
                       ArmadaII_observed=('yes: ' + armada[key]) if key in armada else 'unknown - no labelled runtime evidence',
                       FleetOps_observed=('yes: ' + fo[key]) if key in fo else 'unknown - no labelled runtime evidence',
                       A2FO_static_call_candidate=evidence or 'not established',
                       d3d8to11_source_status=reference.get('status', 'not inventoried'),
                       reference_evidence=reference.get('evidence', ''),
                       A2FO_D3D8_status='native-provider pass-through observer; game evidence only where labelled',
                       A2FO_D3D11_translation=translation(interface, method),
                       priority='runtime capture required')
            if key in fo or key in armada:
                row['priority']='observed: implement and validate parity; absence elsewhere remains unknown'
            if interface in ('IDirect3DCubeTexture8', 'IDirect3DVolumeTexture8'):
                row['d3d8to11_source_status'] = 'unavailable: implementation disabled by #if 0; ' + row['d3d8to11_source_status']
            if evidence and ('unsupported' in row['d3d8to11_source_status'] or method in ('CreateStateBlock', 'ApplyStateBlock', 'CreateVertexShader', 'CreatePixelShader')):
                row['priority'] = 'parity blocker for existing extensions; native coverage still required'
            writer.writerow(row)
    print(f'{len(methods)} method rows -> {args.output}')


if __name__ == '__main__':
    main()
