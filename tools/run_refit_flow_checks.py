#!/usr/bin/env python3
"""Console-only Refit flow checks; no game entrypoints, windows, or input."""
import argparse
import json
import os
from pathlib import Path
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix', required=True, type=Path)
    parser.add_argument('--evidence', required=True, type=Path)
    parser.add_argument('--armada', type=Path, help='engine data mapping; defaults to repository image')
    parser.add_argument('--fo', type=Path, help='Fleet Ops data mapping; defaults to repository image')
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[1]
    prefix = args.prefix.resolve()
    if not prefix.name.startswith('a2fo-refit-native-prefix-') or not (prefix / 'system.reg').is_file():
        parser.error('an initialized dedicated a2fo-refit-native-prefix-* prefix is required')
    evidence = args.evidence.resolve()
    if not evidence.is_relative_to(repo / 'build'):
        parser.error('evidence must be beneath repository build/')
    evidence.mkdir(parents=True, exist_ok=True)
    env = dict(os.environ, WINEPREFIX=str(prefix), WINEDEBUG='-all',
               WINEDLLOVERRIDES='winemenubuilder.exe=d')
    for name in ('DISPLAY', 'WAYLAND_DISPLAY', 'STEAM_COMPAT_DATA_PATH'):
        env.pop(name, None)
    commands = []
    for case in ('success', 'refuse', 'changed', 'players-changed', 'status-changed',
                 'animation', 'animation-show-changed', 'animation-kill-changed',
                 'animation-fraction-changed', 'animation-status-changed',
                 'animation-kill-refuse', 'animation-show-refuse',
                 'animation-inner-changed', 'animation-inner-refuse',
                 'animation-cleanup-changed', 'animation-cleanup-refuse',
                 'known-race-detour', 'foreign-race-detour',
                 'core-race-detour', 'core-foreign-race-detour', 'core-race-tail-changed',
                 'animation-fo', 'animation-fo-core', 'animation-fo-release-refuse',
                 'animation-fo-foreign', 'animation-fo-pixels-foreign'):
        commands.append(('loading-' + case, [repo / 'build/refit_loading_native.exe',
                         (args.armada or repo / 'ArmadaL.exe').resolve(),
                         (args.fo or repo / 'FleetOpsHook.dll').resolve(), case]))
    commands.append(('provider', [repo / 'build/refit_scene_provider.exe',
                     repo / 'build/modules/refit/A2FORefitScene.dll',
                     evidence / 'host/RefitScene', repo]))
    commands.append(('campaign-bridge', [repo / 'build/mission_selector_scene_bridge_test.exe']))
    results = []
    for name, command in commands:
        command = ['wine', *map(str, command)]
        with (evidence / (name + '.log')).open('w') as log:
            try:
                code = subprocess.run(command, cwd=repo, env=env, stdout=log,
                                      stderr=subprocess.STDOUT, timeout=60, check=False).returncode
            except (OSError, subprocess.TimeoutExpired) as error:
                log.write(str(error) + '\n')
                code = 1
        results.append(dict(name=name, command=command, exit_code=code))
        print(('PASS' if code == 0 else 'FAIL') + ': ' + name, flush=True)
    (evidence / 'console-results.json').write_text(json.dumps(results, indent=2) + '\n')
    return int(any(item['exit_code'] for item in results))


if __name__ == '__main__':
    raise SystemExit(main())
