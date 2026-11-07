#!/usr/bin/env python3
"""Prepare an isolated, user-launched Native11 gameplay inspection copy."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import stat
import subprocess

from native11_private_bootstrap import USER_LAUNCH_BATCH, set_native11_options

DEFAULT_SOURCE = Path('/home/tamsynn/NVMeData/Projects/A2FO-Native11-Private/Data')
DEFAULT_TARGET = Path('/home/tamsynn/NVMeData/Projects/A2FO-Native11-Gameplay-Test')
OPTIONS = {
    'CaptureScene': '1', 'CaptureAssets': '1', 'CaptureTextures': '1',
    'CaptureMaterials': '1', 'CaptureNodes': '1', 'CaptureSprites': '1',
    'CaptureWorldLights': '1', 'LiveMaterials': '1', 'LiveMaterialPreview': '1',
    'LivePreviewExport': '1', 'LivePreviewIntervalMs': '1000', 'OffscreenWorld': '0',
    'WorldAmbient': '0.7 0.7 0.7',
}
CRITICAL = ('ArmadaL.exe', 'Mods/Fleet Ops 4.0/ArmadaL.exe',
            'A2FOExtensions.dll', 'A2FORenderer.ini',
            'Mods/FO4 Nexus/A2FORenderer.ini', 'Mods/FO4 Nexus/info.ini', 'd3d9.dll')
HEX = re.compile(r'^[0-9a-f]{64}$')
REQUIRED_CASES = frozenset(('preview-export-codec', 'live-preview-export',
                            'live-preview-export-mount', 'live-material-pipeline', 'world-session'))


def checked_path(path, *, existing=True, directory=False):
    path = Path(path)
    if not path.is_absolute() or '..' in path.parts:
        raise ValueError('absolute lexical path without parent traversal required')
    for part in (path, *path.parents):
        if part.is_symlink():
            raise ValueError(f'symlink path refused: {part}')
        if part.exists() and part == path and existing:
            mode = part.lstat().st_mode
            if not (stat.S_ISDIR(mode) if directory else stat.S_ISREG(mode)):
                raise ValueError(f'unexpected file type: {part}')
        if part != path and part.exists() and not part.is_dir():
            raise ValueError(f'unsafe ancestor: {part}')
    if existing and not path.exists():
        raise ValueError(f'missing path: {path}')
    return path


def sha(path):
    checked_path(path)
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(chunk)
    return h.hexdigest()


def audit_tree(source):
    """Only regular files, directories and links that resolve within source."""
    size = 0
    entries = 0
    for base, dirs, files in os.walk(source, followlinks=False):
        for name in dirs + files:
            item = Path(base) / name
            entries += 1
            mode = item.lstat().st_mode
            if stat.S_ISLNK(mode):
                if os.path.isabs(os.readlink(item)):
                    raise ValueError(f'absolute asset link refused: {item}')
                target = item.resolve(strict=True)
                if not target.is_relative_to(source) or not target.is_file():
                    raise ValueError(f'asset link escapes private Data or is not a regular file: {item}')
            elif stat.S_ISREG(mode):
                size += item.stat().st_size
            elif not stat.S_ISDIR(mode):
                raise ValueError(f'special source node refused: {item}')
    return {'entries': entries, 'regular_bytes': size}


def read_gate(receipt, candidate, helper):
    gate = json.loads(checked_path(receipt).read_text())
    core = gate.get('core_dll', {})
    for name, path in (('core_sha256', candidate), ('helper_sha256', helper)):
        expected = core.get(name)
        if not isinstance(expected, str) or not HEX.fullmatch(expected) or sha(path) != expected:
            raise ValueError(f'{name} candidate mismatch')
    cases = gate.get('runtime_cases')
    if not isinstance(cases, list) or len(cases) < 160:
        raise ValueError('runtime gate requires at least 160 named cases')
    names = set()
    for row in cases:
        if not isinstance(row, dict) or not isinstance(row.get('case'), str) or not row['case'].strip():
            raise ValueError('runtime case name required')
        name = row['case']
        if name in names:
            raise ValueError(f'duplicate runtime case: {name}')
        names.add(name)
        if type(row.get('exit_code')) is not int or row['exit_code'] != 0:
            raise ValueError(f'runtime case failed: {name}')
        raw_exe = row.get('executable')
        if not isinstance(raw_exe, str) or not raw_exe or Path(raw_exe).is_absolute() or '..' in Path(raw_exe).parts or Path(raw_exe) == Path('.'):
            raise ValueError(f'invalid runtime executable path: {name}')
        executable_hash = row.get('executable_sha256')
        if not isinstance(executable_hash, str) or not HEX.fullmatch(executable_hash):
            raise ValueError(f'invalid runtime executable hash: {name}')
        if sha(receipt.parent / raw_exe) != executable_hash:
            raise ValueError(f'runtime executable hash mismatch: {name}')
        inputs = row.get('input_file_sha256')
        if not isinstance(inputs, dict):
            raise ValueError(f'runtime input hash map required: {name}')
        for raw_input, expected in inputs.items():
            if not isinstance(raw_input, str) or not raw_input or not isinstance(expected, str) or not HEX.fullmatch(expected):
                raise ValueError(f'invalid runtime input hash: {name}')
            input_path = Path(raw_input)
            if '..' in input_path.parts or input_path == Path('.'):
                raise ValueError(f'invalid runtime input path: {name}')
            if not input_path.is_absolute():
                input_path = receipt.parent / input_path
            if sha(input_path) != expected:
                raise ValueError(f'runtime input hash mismatch: {name}: {raw_input}')
    if not REQUIRED_CASES.issubset(names):
        raise ValueError(f'missing required runtime cases: {sorted(REQUIRED_CASES - names)}')
    protected = gate.get('protected_live_sha256_unchanged')
    if not isinstance(protected, dict) or len(protected) != 7:
        raise ValueError('runtime gate requires exactly seven protected live hashes')
    for raw, expected in protected.items():
        if not isinstance(raw, str) or not isinstance(expected, str) or not HEX.fullmatch(expected):
            raise ValueError('invalid protected live entry')
        if sha(Path(raw)) != expected:
            raise ValueError(f'protected live hash mismatch: {raw}')
    return gate


def snapshot_critical(source):
    return {name: sha(source / name) for name in CRITICAL}


def clone_data(source, destination, required_bytes):
    """Reflink when possible; ordinary copy only after a free-space check."""
    destination.mkdir(mode=0o700, exist_ok=False)
    try:
        subprocess.run(['cp', '-a', '--reflink=always', '--', str(source) + '/.', str(destination)],
                       check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        return 'reflink'
    except subprocess.CalledProcessError:
        # The destination is task-owned and new. Clear a partial reflink attempt.
        for child in destination.iterdir():
            if child.is_dir() and not child.is_symlink():
                shutil.rmtree(child)
            else:
                child.unlink()
        free = shutil.disk_usage(destination).free
        if free < required_bytes + max(1024 * 1024 * 1024, required_bytes // 20):
            raise ValueError(f'insufficient free space for fallback copy: {free} free, {required_bytes} required')
        subprocess.run(['cp', '-a', '--reflink=never', '--', str(source) + '/.', str(destination)],
                       check=True)
        return 'copy'


def write_new(path, data, mode=0o600):
    checked_path(path, existing=False)
    with path.open('xb') as stream:
        os.fchmod(stream.fileno(), mode)
        stream.write(data)
        stream.flush()
        os.fsync(stream.fileno())


def prepare(receipt, candidate, helper, source=DEFAULT_SOURCE, target=DEFAULT_TARGET, *, apply=False):
    source = checked_path(source, directory=True)
    candidate, helper, receipt = (checked_path(p) for p in (candidate, helper, receipt))
    target = checked_path(target, existing=False)
    if target.exists() or target.is_symlink():
        raise ValueError('target already exists; existing test copies are never overwritten')
    if source.is_relative_to(target) or target.is_relative_to(source):
        raise ValueError('source and target must be separate trees')
    if target.parent.exists():
        checked_path(target.parent, directory=True)
    elif not target.parent.parent.exists():
        raise ValueError('target parent must be one level below an existing directory')
    gate = read_gate(receipt, candidate, helper)
    tree = audit_tree(source)
    original = snapshot_critical(source)
    sibling_hook = checked_path(source.parent / 'FleetOpsHook.dll')
    sibling_hook_hash = sha(sibling_hook)
    config = source / 'Mods/FO4 Nexus/A2FORenderer.ini'
    updated = set_native11_options(config.read_bytes(), OPTIONS)
    result = {
        'scope': 'observed DX11 inspection; no full renderer/presentation/shadow claim',
        'source_data': str(source), 'target_root': str(target),
        'target_data': str(target / 'Data'), 'tree': tree,
        'gate_receipt_sha256': sha(receipt),
        'candidate_core_sha256': gate['core_dll']['core_sha256'],
        'renderer_helper_sha256': gate['core_dll']['helper_sha256'],
        'source_critical_before_sha256': original,
        'source_sibling_hook_sha256': sibling_hook_hash,
        'source_config_after_sha256': original['Mods/FO4 Nexus/A2FORenderer.ini'],
        'options': OPTIONS, 'applied': apply,
        'launch': 'User opens Launch Game.sh; the batch launcher selects FO4 Nexus.',
        'input_automation': False,
    }
    if not apply:
        return result
    target.mkdir(mode=0o700, exist_ok=False)
    data = target / 'Data'
    result['clone_method'] = clone_data(source, data, tree['regular_bytes'])
    shutil.copy2(sibling_hook, target / 'FleetOpsHook.dll')
    if sha(target / 'FleetOpsHook.dll') != sibling_hook_hash:
        raise ValueError('copied sibling FleetOpsHook.dll hash mismatch')
    # A copied source should match before any trial-only changes.
    for name, expected in original.items():
        if sha(data / name) != expected:
            raise ValueError(f'copied critical hash mismatch: {name}')
    for from_path, to_path in ((candidate, data / 'A2FOExtensions.dll'),
                               (helper, data / 'A2FONative11Renderer.dll')):
        checked_path(to_path, existing=to_path.exists())
        if to_path.exists():
            to_path.unlink()
        shutil.copy2(from_path, to_path)
        if sha(to_path) != sha(from_path):
            raise ValueError(f'staged binary hash mismatch: {to_path}')
    config_copy = data / 'Mods/FO4 Nexus/A2FORenderer.ini'
    checked_path(config_copy)
    config_copy.write_bytes(updated)
    if sha(config_copy) != hashlib.sha256(updated).hexdigest():
        raise ValueError('trial config hash mismatch')
    preview = data / 'Native11Preview'
    checked_path(preview, existing=False)
    if preview.exists():
        checked_path(preview, directory=True)
        seed = target / 'Seed Preview'
        checked_path(seed, existing=False)
        preview.rename(seed)
        result['seed_preview_archive'] = str(seed)
    preview.mkdir()
    inspector = target / 'Inspector'
    inspector.mkdir()
    viewer_source = checked_path(Path(__file__).with_name('native11_live_view.py').absolute())
    viewer_bytes = viewer_source.read_bytes()
    viewer_hash = hashlib.sha256(viewer_bytes).hexdigest()
    viewer_copy = inspector / 'native11_live_view.py'
    write_new(viewer_copy, viewer_bytes)
    if sha(viewer_copy) != viewer_hash:
        raise ValueError('copied viewer hash mismatch')
    batch=target/'Native11 Inspection.bat'
    write_new(batch,USER_LAUNCH_BATCH)
    result['user_launch_batch_sha256']=sha(batch)
    launcher = ('#!/bin/sh\nset -eu\n'
                '# The batch launcher selects FO4 Nexus explicitly.\n'
                'exec flatpak run ru.linux_gaming.PortProton ' + shlex.quote(str(batch)) + '\n')
    viewer = ('#!/bin/sh\nset -eu\n'
              'exec python3 ' + shlex.quote(str(viewer_copy)) +
              ' --directory ' + shlex.quote(str(preview)) + ' --port 18711\n')
    write_new(target / 'Launch Game.sh', launcher.encode(), 0o700)
    write_new(target / 'View Renderer.sh', viewer.encode(), 0o700)
    guide = (
        '# Native11 gameplay trial\n\n'
        'This is an isolated copy for user-driven observed DX11 inspection. '
        'It can show captured ships, poses, and materials; it does not establish '
        'full world rendering, shadows, or native presentation.\n\n'
        '1. Run `./Launch Game.sh` from this directory. PortProton opens the batch launcher, '
        'which starts the copied `Data/ArmadaL.exe` with `FO4 Nexus` selected.\n'
        '2. In FO4 Nexus, start a skirmish.\n'
        '3. Run `./View Renderer.sh` separately. Open `http://127.0.0.1:18711/` manually in a browser.\n'
        '4. Inspect the observed ships, poses, and materials. The default is one snapshot per second '
        'with neutral ambient lighting. Stop both programs normally when done.\n\n'
        'The viewer reads `Data/Native11Preview/status.json` and `frame-a.bmp` or `frame-b.bmp`. '
        'Game logs include `Data/A2FOExtensions.log`, `Data/A2FORenderer.log`, '
        'and `Data/A2FOStartupProxy.log`. Preparation hashes are in `gameplay-trial-receipt.json`.\n'
    )
    write_new(target / 'README.md', guide.encode())
    after = snapshot_critical(source)
    if after != original:
        raise ValueError('private source critical files changed during preparation')
    if sha(sibling_hook) != sibling_hook_hash:
        raise ValueError('private source sibling hook changed during preparation')
    read_gate(receipt, candidate, helper)
    result['source_critical_after_sha256'] = after
    result['trial_core_sha256'] = sha(data / 'A2FOExtensions.dll')
    result['trial_helper_sha256'] = sha(data / 'A2FONative11Renderer.dll')
    result['trial_config_sha256'] = sha(config_copy)
    result['trial_exe_sha256'] = sha(data / 'ArmadaL.exe')
    result['trial_sibling_hook_sha256'] = sha(target / 'FleetOpsHook.dll')
    result['inspector_viewer_sha256'] = sha(viewer_copy)
    result['inspector_viewer_source_sha256'] = viewer_hash
    if sha(viewer_source) != viewer_hash:
        raise ValueError('viewer source changed during preparation')
    write_new(target / 'gameplay-trial-receipt.json', (json.dumps(result, indent=2, sort_keys=True) + '\n').encode())
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--candidate', type=Path, required=True)
    parser.add_argument('--renderer-helper', type=Path, required=True)
    parser.add_argument('--source-data', type=Path, default=DEFAULT_SOURCE)
    parser.add_argument('--target-root', type=Path, default=DEFAULT_TARGET)
    parser.add_argument('--apply', action='store_true', help='create the new isolated copy')
    args = parser.parse_args(argv)
    print(json.dumps(prepare(args.receipt, args.candidate, args.renderer_helper,
                             args.source_data, args.target_root, apply=args.apply), indent=2, sort_keys=True))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
