#!/usr/bin/env python3
"""Install a validated Nexus payload as a new sibling mod, with a hash receipt."""
import argparse
import datetime
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--parent', required=True, type=Path)
    parser.add_argument('--target', required=True, type=Path)
    args = parser.parse_args()
    parent = args.parent.resolve()
    target = args.target.absolute()
    if target.is_symlink() or target.exists():
        parser.error('target already exists; refusing to overwrite an installed mod')
    if target.parent.resolve() != parent.parent or target.name == parent.name:
        parser.error('target must be a new sibling of the Fleet Ops 4.0 parent')
    package = ROOT / 'package'
    subprocess.run([sys.executable, str(ROOT / 'tools/validate.py'),
                    '--parent', str(parent), '--package', str(package),
                    '--report', str(ROOT / 'validation.json')], check=True)
    inputs = {}
    content = json.loads((ROOT / 'content-manifest.json').read_text())
    integration = json.loads((ROOT / 'integration-manifest.json').read_text())
    for item in content['files']:
        inputs[parent / item['source']] = item['source_sha256']
    for item in integration['files']:
        inputs[parent / item['path']] = item['source_sha256']
    install_root = parent.parents[2]
    for relative, expected in json.loads((ROOT / 'protected-before.json').read_text()).items():
        inputs[install_root / relative] = expected
    for path, expected in inputs.items():
        if sha(path) != expected:
            raise RuntimeError(f'protected input changed before deployment: {path}')
    payload = {str(p.relative_to(package)): sha(p)
               for p in sorted(package.rglob('*')) if p.is_file()}
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%SZ')
    stage = target.parent / ('.' + target.name + '.staging-' + stamp)
    shutil.copytree(package, stage)
    for relative, expected in payload.items():
        if sha(stage / relative) != expected:
            raise RuntimeError(f'staged hash mismatch: {relative}; retained {stage}')
    if target.exists():
        raise RuntimeError(f'target appeared during deployment; retained {stage}')
    stage.rename(target)
    drift = [str(path) for path, expected in inputs.items() if sha(path) != expected]
    installed = {str(p.relative_to(target)): sha(p)
                 for p in sorted(target.rglob('*')) if p.is_file()}
    if installed != payload:
        raise RuntimeError('installed payload differs from staged payload')
    receipt = {
        'installed_utc': stamp, 'target': str(target), 'parent': str(parent),
        'files': installed, 'file_count': len(installed),
        'protected_inputs': {str(p): value for p, value in inputs.items()},
        'protected_drift': drift, 'game_launched': False,
        'rollback': 'With the game closed, move the entire target directory outside Data/Mods. No existing files were replaced.',
    }
    destination = ROOT / ('deployment-' + stamp + '.json')
    destination.write_text(json.dumps(receipt, indent=2, sort_keys=True) + '\n')
    if drift:
        raise RuntimeError(f'protected inputs changed; inspect {destination}')
    print(f'Installed {len(installed)} files; {len(inputs)} protected hashes unchanged; receipt {destination}')


if __name__ == '__main__':
    main()
