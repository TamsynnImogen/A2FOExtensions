#!/usr/bin/env python3
"""Build full frozen/current MissionSelector headless fixtures and compare traces."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
UNIT = Path('modules/A2FOMissionSelector/module.cpp')
BASE = ROOT / 'build/rust-migration-stage8/before'
WORK = ROOT / 'build/rust-migration-stage8/mission-work'
FIXTURE = ROOT / 'tests/rust_stage8_c_mission_mock.cpp'
LAYOUT_FIXTURE = ROOT / 'tests/rust_stage8_c_mission_layout_mock.cpp'
RUST = ROOT / 'rust/a2fo_core/src/stage8_c_mission.rs'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command):
    result = subprocess.run(command, cwd=ROOT, capture_output=True, timeout=180)
    if result.returncode:
        raise RuntimeError(f'{command}: {result.returncode}\n'
                           f'{result.stderr[-6000:]!r}')
    return result.stdout.replace(b'\r\n', b'\n')


def run_wine(executable, env, label):
    stdout = WORK / f'{label}.stdout.log'
    stderr = WORK / f'{label}.stderr.log'
    with stdout.open('wb') as output, stderr.open('wb') as errors:
        result = subprocess.run(['wine', str(executable)], cwd=WORK, env=env,
                                stdout=output, stderr=errors, timeout=180)
    if result.returncode:
        raise RuntimeError(f'{label} Wine exit {result.returncode}: '
                           f'{stderr.read_bytes()[-5000:]!r}')
    return stdout.read_bytes().replace(b'\r\n', b'\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib', type=Path,
                        default=os.environ.get('A2FO_RUST_WIN_LIB'))
    parser.add_argument('--wine-prefix', type=Path,
                        default=os.environ.get('A2FO_RUST_WINEPREFIX'))
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    hashes = json.loads((ROOT/'build/rust-migration-stage8/before-hashes.json').read_text())
    if digest(BASE/UNIT) != hashes[str(UNIT)]:
        raise RuntimeError('frozen MissionSelector source hash changed')
    WORK.mkdir(parents=True, exist_ok=True)
    archive = args.rust_lib or WORK/'libstage8_c_mission.a'
    if args.rust_lib and not archive.exists():
        raise RuntimeError(f'Rust archive missing: {archive}')
    if not args.rust_lib:
        run(['rustc', '--edition=2024', '--target=i686-pc-windows-gnu',
             '--crate-type=staticlib', '-C', 'panic=abort', '-C', 'opt-level=2',
             '-D', 'warnings', str(RUST), '-o', str(archive)])
    frozen_dir = WORK/'frozen/modules/A2FOMissionSelector'
    frozen_dir.mkdir(parents=True, exist_ok=True)
    frozen_text = (BASE/UNIT).read_text()
    # The original source defined NOMINMAX after its provider header. The
    # equivalent guard is a test-only warning fix under -Werror.
    assert frozen_text.count('#define NOMINMAX') == 1
    frozen_text = frozen_text.replace('#define NOMINMAX',
                                      '#ifndef NOMINMAX\n#define NOMINMAX\n#endif', 1)
    frozen_source = frozen_dir/'module.cpp'
    frozen_source.write_text(frozen_text)
    shutil.copyfile(BASE/'modules/A2FOMissionSelector/standard_buttons.inl',
                    frozen_dir/'standard_buttons.inl')
    sdk_link = WORK/'frozen/sdk'
    if not sdk_link.exists():
        sdk_link.symlink_to(BASE/'sdk', target_is_directory=True)
    executables = {}
    for variant, fixture in [('main',FIXTURE),('layout',LAYOUT_FIXTURE)]:
        for label, source in [('before', frozen_source), ('current', ROOT/UNIT)]:
            executable = WORK/f'mission_{label}_{variant}.exe'
            command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2',
                       '-Wall', '-Wextra', '-Werror', '-ffunction-sections',
                       '-fdata-sections', '-Wl,--gc-sections', '-static',
                       '-DA2FO_STAGE8_MISSION_SOURCE="'+str(source)+'"', str(fixture)]
            if label == 'current':
                command.append(str(archive))
            command += ['-lgdiplus', '-lgdi32', '-lcomctl32', '-lole32', '-luuid',
                        '-lws2_32', '-lbcrypt', '-ladvapi32', '-luserenv', '-lntdll',
                        '-o', str(executable)]
            run(command)
            executables[label,variant] = executable
    if args.build_only:
        print('mission: full frozen/current native fixtures built')
        return
    if args.wine_prefix:
        prefix = args.wine_prefix
        context = None
    else:
        context = tempfile.TemporaryDirectory(prefix='a2fo-stage8-mission-')
        prefix = Path(context.name)/'wine'
    try:
        env = {**os.environ, 'WINEDEBUG':'-all', 'WINEPREFIX':str(prefix),
               'DISPLAY':'', 'WAYLAND_DISPLAY':'',
               'WINEDLLOVERRIDES':'winemenubuilder.exe=d;winedbg.exe=d;'
                                 'winewayland.drv=d;winex11.drv=d'}
        traces = []
        for label in ('before','current'):
            parts = []
            for variant in ('main','layout'):
                parts.append(b'fixture:'+variant.encode()+b'\n'+
                    run_wine(executables[label,variant],env,f'{label}_{variant}'))
            traces.append(b''.join(parts))
    finally:
        if context:
            context.cleanup()
    if traces[0] != traces[1]:
        before, current = (trace.splitlines() for trace in traces)
        for index in range(max(len(before),len(current))):
            if index >= len(before) or index >= len(current) or before[index] != current[index]:
                raise RuntimeError(f'first difference line {index+1}: '
                                   f'{before[index:index+1]} != {current[index:index+1]}')
    receipt = {'cases':len(traces[0].splitlines()),
               'sha256':hashlib.sha256(traces[0]).hexdigest(),
               'frozen_sha256':digest(BASE/UNIT),
               'current_sha256':digest(ROOT/UNIT),
               'rust_sha256':digest(RUST),
               'rust_archive':str(archive),'rust_archive_sha256':digest(archive),
               'equal':True}
    (WORK/'parity.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(f'mission: {receipt["cases"]} lines, {receipt["sha256"]}, PASS')

if __name__ == '__main__':
    main()
