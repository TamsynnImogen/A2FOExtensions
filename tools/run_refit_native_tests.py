#!/usr/bin/env python3
"""Run console DLL/instruction fixtures only, with display variables removed.

Requires an explicit, already initialized disposable Wine prefix. Does not run
game entrypoints, sessions, input drivers or installers; never uses a default
prefix or installs modules. Fixture EXEs map inputs with DONT_RESOLVE_DLL_REFERENCES.
"""
import argparse
import os
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', required=True)
    parser.add_argument('--exe', type=Path, action='append', help='additional engine input, success case only')
    args = parser.parse_args()
    if not args.prefix or not Path(args.prefix).is_absolute():
        parser.error('--prefix must explicitly name an absolute disposable prefix')
    prefix = Path(args.prefix).resolve()
    if not (prefix / 'system.reg').is_file():
        parser.error('prefix must already be initialized; no automatic Wine setup')
    root = Path(__file__).resolve().parents[1]
    runner = root / 'build/refit_module_smoke.exe'
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all', WINEDLLOVERRIDES='winemenubuilder.exe=d')
    for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'STEAM_COMPAT_DATA_PATH'):
        env.pop(key, None)
    cases = ('missing', 'disabled', 'invalid', 'noop', 'api', 'foreign', 'context', 'identity', 'relocation', 'patch-fail', 'success')
    failed = []
    total = 0
    for name in ('A2FOCameraControls', 'A2FOViewDistance'):
        module = root / 'build/modules' / (name + '.dll')
        inputs = [(root / 'ArmadaL.exe', cases + (('consumer',) if 'Camera' in name else ()))]
        inputs += [(p.resolve(), ('success',)) for p in (args.exe or [])]
        for image, modes in inputs:
            for mode in modes:
                total += 1
                print(f'{name} input={image} case={mode}', flush=True)
                try:
                    result = subprocess.run(['wine', str(runner), str(module), str(image), mode], env=env, timeout=30, check=False)
                    if result.returncode:
                        failed.append((name, str(image), mode, result.returncode))
                except (OSError, subprocess.TimeoutExpired) as error:
                    failed.append((name, str(image), mode, str(error)))
    print(f'{total - len(failed)}/{total} console fixture processes passed', flush=True)
    for failure in failed:
        print(f'FAIL {failure}', file=sys.stderr)
    return 1 if failed else 0


if __name__ == '__main__':
    raise SystemExit(main())
