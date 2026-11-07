#!/usr/bin/env python3
"""Console-only mission-backdrop/FO-hook replay tests; no game entrypoints/input."""
import argparse
import hashlib
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', required=True)
    parser.add_argument('--armada', required=True, type=Path)
    parser.add_argument('--fo', required=True, type=Path)
    parser.add_argument('--log', required=True, type=Path)
    args = parser.parse_args()
    prefix = Path(args.prefix)
    if not prefix.is_absolute() or not (prefix / 'system.reg').is_file():
        parser.error('explicit absolute initialized disposable Wine prefix required')
    repo = Path(__file__).resolve().parents[1]
    fixture = repo / 'build/refit_mission_backdrop_native.exe'
    module = repo / 'build/modules/A2FORefitMenus.dll'
    for path in (args.armada, args.fo, fixture, module):
        if not path.is_file():
            parser.error(f'missing test input: {path}')
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all',
               WINEDLLOVERRIDES='winemenubuilder.exe=d')
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'STEAM_COMPAT_DATA_PATH'):
        env.pop(key, None)
    digest = hashlib.sha256(module.read_bytes()).hexdigest()
    args.log.parent.mkdir(parents=True, exist_ok=True)
    with args.log.open('w') as log:
        def write(text):
            print(text, flush=True)
            log.write(text + '\n')
            log.flush()

        def run(command, timeout):
            try:
                result = subprocess.run(command, cwd=repo, env=env, timeout=timeout,
                                        text=True, errors='replace', stdout=subprocess.PIPE,
                                        stderr=subprocess.STDOUT, check=False)
            except (OSError, subprocess.TimeoutExpired) as error:
                write(f'FAIL: {error}')
                return False
            write(result.stdout)
            return result.returncode == 0

        write(f'Tested module SHA-256: {digest}')
        for mode in ('changed', 'refuse', 'success', 'fo-success', 'fo-target',
                     'fo-gateway', 'fo-callback', 'fo-noexec'):
            write(f'Observer case: {mode}')
            command = ['wine', str(fixture), str(args.armada.resolve())]
            if mode.startswith('fo-'):
                command.append(str(args.fo.resolve()))
            if not run(command + [mode], 60):
                return 1
        write('PASS: 8 observer/FO replay cases')
        if not run(['python3', str(repo / 'tools/run_refit_menu_tests.py'),
                    '--prefix', str(prefix), '--armada', str(args.armada.resolve()),
                    '--fo', str(args.fo.resolve())], 900):
            return 1
        if hashlib.sha256(module.read_bytes()).hexdigest() != digest:
            write('FAIL: module changed while tests ran; results invalid')
            return 1
        write('PASS: observer and real-DLL console suites; native visual acceptance pending')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
