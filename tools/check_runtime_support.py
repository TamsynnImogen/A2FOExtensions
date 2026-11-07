#!/usr/bin/env python3
"""Check companion files needed by the built-in menu and reporting modules.

This only reads files. It never loads a DLL, starts the helper, or sends reports.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct

from check_rust_media_contract import Image


SCENE_EXPORTS = {
    'A2FOScene_ApiVersion', 'A2FOScene_Initialize', 'A2FOScene_SetDevice',
    'Render3D_MenuScene', 'Render3D_MenuWarp', 'A2FOScene_SetFrameTime',
}


def check(data):
    issues = []
    files = {}
    for relative, exports in (
            ('modules/refit/A2FORefitScene.dll', SCENE_EXPORTS),
            ('modules/A2FOLogMailerHelper.exe', set())):
        path = data / relative
        if not path.is_file():
            issues.append(f'missing runtime companion: {relative}')
            continue
        try:
            raw = path.read_bytes()
            header = struct.unpack_from('<I', raw, 0x3c)[0]
            machine = struct.unpack_from('<H', raw, header + 4)[0]
            image = Image(path)
            names = {row[1] for row in image.exports}
            if machine != 0x14c:
                issues.append(f'runtime companion is not Win32 x86: {relative}')
            missing = exports - names
            if missing:
                issues.append(f'scene provider exports missing: {sorted(missing)}')
            files[relative] = {'sha256': hashlib.sha256(raw).hexdigest(),
                               'size': len(raw), 'imports': image.imports}
        except (OSError, ValueError, struct.error, IndexError) as error:
            issues.append(f'invalid runtime companion: {relative}: {error}')
    for relative in ('RefitScene/Render3D.ini', 'RefitScene/Menu/map.ini'):
        path = data / relative
        if not path.is_file():
            issues.append(f'missing scene configuration: {relative}')
        else:
            files[relative] = {'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
    return {'status': 'passed-runtime-support-preflight' if not issues else 'failed',
            'data_directory': str(data), 'issues': issues, 'files': files,
            'scope': 'refit scene provider/configuration and module-window reporting helper',
            'visual_acceptance': False, 'reports_sent': False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--data', type=Path, required=True)
    parser.add_argument('--receipt', type=Path)
    args = parser.parse_args()
    result = check(args.data.resolve())
    output = json.dumps(result, indent=2) + '\n'
    if args.receipt:
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(output)
    print(output, end='')
    return 0 if not result['issues'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
