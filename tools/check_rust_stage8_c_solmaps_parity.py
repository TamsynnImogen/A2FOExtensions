#!/usr/bin/env python3
"""Build full frozen/current A2FOSOLMaps fixtures and compare native traces."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
UNIT = Path('modules/A2FOSOLMaps/module.cpp')
BASE = ROOT / 'build/rust-migration-stage8/before'
WORK = ROOT / 'build/rust-migration-stage8/solmaps-work'
FIXTURE = ROOT / 'tests/rust_stage8_c_solmaps_mock.cpp'


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, *, env=None):
    result = subprocess.run(command, cwd=ROOT, env=env, capture_output=True,
                            timeout=180)
    if result.returncode:
        raise RuntimeError(f'{command}: {result.returncode}\n'
                           f'{result.stderr[-5000:]!r}')
    return result.stdout.replace(b'\r\n', b'\n')


def run_wine(executable, env, label):
    stdout = WORK/f'{label}.stdout.log'
    stderr = WORK/f'{label}.stderr.log'
    with stdout.open('wb') as output, stderr.open('wb') as errors:
        result = subprocess.run(['wine',str(executable)],cwd=ROOT,env=env,
                                stdout=output,stderr=errors,timeout=180)
    if result.returncode:
        raise RuntimeError(f'{label} Wine exit {result.returncode}: '
                           f'{stderr.read_bytes()[-5000:]!r}')
    return stdout.read_bytes().replace(b'\r\n',b'\n')


def normalize_fixture_addresses(trace):
    """Normalize only the mock object and heap MapDetails address fields."""
    addresses = []
    normalized = []
    object_pattern = re.compile(
        rb'^(log:SOL phase2j (?:constructed|spawned) 3/3: .* object=)'
        rb'([0-9a-fA-F]{8})(; publishing to mission)?$')
    details_pattern = re.compile(
        rb'^(log:SOL phase2j probe synthesis #1: nativeVersion=3021, '
        rb'reader=00000001, context=00000002 \(native\), mapDetails=)'
        rb'([0-9a-fA-F]{8})( \(native\), earlyMission=.*)$')
    details_addresses = []
    for line in trace.splitlines(keepends=True):
        body = line.rstrip(b'\n')
        match = object_pattern.fullmatch(body)
        if match:
            addresses.append(match.group(2))
            line = match.group(1) + b'<fixture-object>' + (
                match.group(3) or b'') + b'\n'
        else:
            match = details_pattern.fullmatch(body)
            if match:
                details_addresses.append(match.group(2))
                line = match.group(1) + b'<fixture-details>' + match.group(3) + b'\n'
        normalized.append(line)
    if len(addresses) != 2 or addresses[0] != addresses[1]:
        raise RuntimeError(f'expected one mock object reused in two SOL logs: '
                           f'{addresses!r}')
    if len(details_addresses) != 1 or details_addresses[0] == b'00000000':
        raise RuntimeError(f'expected one nonnull mock MapDetails address: '
                           f'{details_addresses!r}')
    return b''.join(normalized)


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
        raise RuntimeError('frozen SOL source hash changed')
    WORK.mkdir(parents=True, exist_ok=True)
    archive = args.rust_lib or WORK/'libstage8_c_solmaps.a'
    if args.rust_lib and not archive.exists():
        raise RuntimeError(f'Rust archive missing: {archive}')
    if not args.rust_lib:
        run(['rustc', '--edition=2024', '--target=i686-pc-windows-gnu',
             '--crate-type=staticlib', '-C', 'panic=abort', '-C', 'opt-level=2',
             '-D', 'warnings', str(ROOT/'rust/a2fo_core/src/stage8_c_solmaps.rs'),
             '-o', str(archive)])
    run(['i686-w64-mingw32-g++','-std=gnu++17','-O2','-Wall','-Wextra',
         '-Werror','-shared','-static-libgcc',
         str(ROOT/'tests/rust_stage8_c_solmaps_a1_mock.cpp'),
         '-o',str(WORK/'A1Compat.dll')])
    executables = []
    for label, source in [('before',BASE/UNIT),('current',ROOT/UNIT)]:
        executable = WORK/f'solmaps_{label}.exe'
        command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2',
                   '-Wall', '-Wextra', '-Werror', '-ffunction-sections',
                   '-fdata-sections', '-Wl,--gc-sections', '-static',
                   '-DA2FO_STAGE8_SOL_SOURCE="'+str(source)+'"',
                   str(FIXTURE)]
        if label == 'current':
            command.append(str(archive))
        command += ['-o',str(executable)]
        run(command)
        executables.append(executable)
    if args.build_only:
        print('solmaps: full frozen/current native fixtures built')
        return
    if args.wine_prefix:
        prefix = args.wine_prefix
        context = None
    else:
        context = tempfile.TemporaryDirectory(prefix='a2fo-stage8-solmaps-')
        prefix = Path(context.name)/'wine'
    try:
        env = {**os.environ,'WINEDEBUG':'-all','WINEPREFIX':str(prefix),
               'DISPLAY':'','WAYLAND_DISPLAY':'',
               'WINEDLLOVERRIDES':'winemenubuilder.exe=d;winedbg.exe=d;'
                                 'winewayland.drv=d;winex11.drv=d'}
        traces = [run_wine(executable,env,label)
                  for label,executable in zip(('before','current'),executables)]
    finally:
        if context:
            context.cleanup()
    normalized = [normalize_fixture_addresses(trace) for trace in traces]
    if normalized[0] != normalized[1]:
        before, current = (trace.splitlines() for trace in normalized)
        for index in range(max(len(before),len(current))):
            if index >= len(before) or index >= len(current) or before[index] != current[index]:
                raise RuntimeError(f'first difference line {index+1}: '
                                   f'{before[index:index+1]} != '
                                   f'{current[index:index+1]}')
    receipt = {'cases':len(normalized[0].splitlines()),
               'sha256':hashlib.sha256(normalized[0]).hexdigest(),
               'raw_frozen_sha256':hashlib.sha256(traces[0]).hexdigest(),
               'raw_current_sha256':hashlib.sha256(traces[1]).hexdigest(),
               'frozen_sha256':digest(BASE/UNIT),
               'current_sha256':digest(ROOT/UNIT),
               'rust_sha256':digest(ROOT/'rust/a2fo_core/src/stage8_c_solmaps.rs'),
               'rust_archive':str(archive),'rust_archive_sha256':digest(archive),
               'equal':True}
    (WORK/'parity.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(f'solmaps: {receipt["cases"]} lines, {receipt["sha256"]}, PASS')


if __name__ == '__main__':
    main()
