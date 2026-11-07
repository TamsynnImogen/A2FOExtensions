#!/usr/bin/env python3
"""Run the final consolidation's headless fixtures; never start the game."""
import argparse
import json
import os
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--wine-prefix', required=True)
parser.add_argument('--output-dir', default='build/builtin-stage10',
                    help='directory for this run; use a new path to preserve prior receipts')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
os.chdir(root)
prefix = Path(args.wine_prefix).resolve()
prefix.mkdir(parents=True, exist_ok=True)
output = root / args.output_dir
output.mkdir(parents=True, exist_ok=True)
env = os.environ.copy()
for key in ('DISPLAY', 'WAYLAND_DISPLAY'):
    env.pop(key, None)
env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all',
           WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
jobs = []
def add(name, executable, *values):
    jobs.append((name, ['wine', str(executable), *map(str, values)]))

add('startup-off', 'build/builtin_startup_policy_test.exe', 'off')
add('startup-on', 'build/builtin_startup_policy_test.exe', 'on')
for index in range(24):
    add(f'startup-fail-{index}', 'build/builtin_startup_policy_test.exe', 'fail', index)
for mode in ('producer_fail', 'producer_ready', 'reject'):
    add('loader-' + mode, 'build/builtin_final_loader_test.exe', mode)
add('production-adapters', 'build/builtin_batch10_native_test.exe')
add('stage9-production-adapters', 'build/builtin_batch9_native_test.exe')
add('feature-pack-bridge', 'build/builtin_feature_pack_bridge_native_test.exe')
base = Path('build/fixtures/builtin-final')
for mode in ('full', 'old', 'partial', 'no-core'):
    add('provider-' + mode, base / 'startup' / mode / 'provider_test.exe', mode)
for mode in ('active', 'disabled', 'old', 'partial'):
    add('missions-' + mode, base / 'missions' / mode / 'missions_test.exe', mode)
add('shell-owner', base / 'shell/shell_owner_test.exe')
movies = ('aac.mp4', 'opus.webm', 'vorbis.webm', 'silent.mp4', 'short.mp4', 'reference.mp4')
add('video-shared-core', 'build/builtin_video_native_test.exe',
    *(Path('build/video-fixtures') / name for name in movies))
inputs = Path('build/engine-compat-20261023/inputs/oct23')
add('engine-profiles', 'build/engine_profiles_native_test.exe', inputs / 'ArmadaL.exe', inputs / 'FleetOpsHook.dll')
for mode in ('success', 'players-changed', 'known-race-detour', 'foreign-race-detour',
             'core-race-detour', 'core-foreign-race-detour', 'core-race-tail-changed'):
    add('refit-' + mode, 'build/refit_loading_native.exe', inputs / 'ArmadaL.exe', inputs / 'FleetOpsHook.dll', mode)
add('dll-load', 'build/roots_dll_load_smoke.exe', 'build/A2FOExtensions.dll')
results = []
for name, command in jobs:
    log = output / (name + '.log')
    # Wine services can inherit console pipes. A regular log file lets the
    # fixture's process exit independently of those services' output lifetime.
    with log.open('w') as stream:
        try:
            result = subprocess.run(command, env=env, stdout=stream,
                                    stderr=subprocess.STDOUT, timeout=45)
            code = result.returncode
        except subprocess.TimeoutExpired as error:
            code = 124
            stream.write(str(error) + '\n')
    text = log.read_text(encoding='utf-8', errors='replace')
    results.append({'case': name, 'command': command, 'exit': code, 'log': str(log.relative_to(root))})
    (output / 'native-results.json').write_text(json.dumps(results, indent=2) + '\n')
    print(f'{len(results)}/{len(jobs)} {name}: ' + ('PASS' if code == 0 else f'FAIL ({code})'), flush=True)
    if code:
        print(text[-3000:], flush=True)
        raise SystemExit(1)
print(f'{len(results)} final consolidation fixture invocations: PASS')
