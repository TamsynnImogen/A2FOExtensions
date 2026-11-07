#!/usr/bin/env python3
"""Real scene test inside a private headless compositor; no engine or input automation."""
import argparse
import os
from pathlib import Path
import signal
import subprocess
import tempfile
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixture', choices=('scene', 'setup', 'loading'), default='scene')
    parser.add_argument('--prefix', required=True, type=Path)
    parser.add_argument('--host-root', required=True, type=Path)
    parser.add_argument('--assets', required=True, type=Path)
    parser.add_argument('--log', required=True, type=Path)
    parser.add_argument('--startup-only', action='store_true',help='profile only the first real scene frame and clean shutdown')
    args = parser.parse_args()
    if args.startup_only and args.fixture!='scene':parser.error('--startup-only requires --fixture scene')
    repo = Path(__file__).resolve().parents[1]
    host = args.host_root.resolve()
    prefix = args.prefix.resolve()
    if not host.is_relative_to(repo / 'build') or not host.is_dir():
        parser.error('isolated host root must already exist beneath this repository build/ directory')
    if not prefix.name.startswith('a2fo-refit-native-prefix-') or not (prefix / 'system.reg').is_file():
        parser.error('an initialized dedicated a2fo-refit-native-prefix-* Wine prefix is required')
    if not args.assets.is_dir():
        parser.error('asset root missing')
    executable = repo / {'scene':'build/refit_scene_worker_native.exe',
                         'setup':'build/refit_setup_native.exe',
                         'loading':'build/refit_loading_gpu.exe'}[args.fixture]
    if args.fixture=='loading':
        staged=host/executable.name
        if not staged.is_file() or staged.read_bytes()!=executable.read_bytes():
            parser.error('stage the current loading fixture executable in the private host root')
        executable=staged
    if not executable.is_file() or (args.fixture=='scene' and not (host / 'd3d9.dll').is_file()):
        parser.error('build the worker fixture and stage its renderer/provider/config first')
    if args.fixture=='loading' and not (host/'d3d8.dll').is_file():
        parser.error('stage the private loading fixture d3d8 wrapper first')
    if args.fixture=='loading' and b'DxWrapper Stub' in (host/'d3d8.dll').read_bytes() and not (host/'dxwrapper.dll').is_file():
        parser.error('the staged D3D8 proxy requires its private dxwrapper.dll dependency')
    marker = ('PASS: real D3D frames (main/Klingon/Borg), 1920x1080, retained cache, shutdown;'
              if args.fixture=='scene' else 'PASS: loading D3D8 image, native-overlay order, state restoration and resource teardown'
              if args.fixture=='loading' else 'refit setup native widget decode and constructor passed')
    fixture_args = ([str(host),str(args.assets.resolve())] if args.fixture=='scene' else
                    [str(host)] if args.fixture=='loading' else [str(args.assets.resolve()/'ArmadaL.exe')])
    if args.startup_only:
        marker='PASS: all-fleet startup profile';fixture_args.append('--startup-only')
    args.log.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='a2fo-refit-gpu-') as runtime:
        env = dict(os.environ)
        for key in ('DISPLAY', 'WAYLAND_DISPLAY', 'STEAM_COMPAT_DATA_PATH', 'LD_PRELOAD'):
            env.pop(key, None)
        env.update(XDG_RUNTIME_DIR=runtime, WINEPREFIX=str(prefix), WINEDEBUG='-all',
                   WINEDLLOVERRIDES='d3d9=n,b;winemenubuilder.exe=d',
                   DXVK_LOG_PATH=str(host), DXVK_STATE_CACHE_PATH=str(host / 'cache'))
        if args.fixture=='loading':
            env['WINEDLLOVERRIDES']='d3d8=n,b;d3d9=n,b;winemenubuilder.exe=d'
        (host / 'cache').mkdir(exist_ok=True)
        command = ['gamescope', '--backend', 'headless', '-W', '1920', '-H', '1080', '--',
                   'env', '-u', 'WAYLAND_DISPLAY', 'wine', str(executable), *fixture_args]
        passed = False
        with args.log.open('w') as output:
            process = subprocess.Popen(command, cwd=repo, env=env, stdout=output,
                                       stderr=subprocess.STDOUT, start_new_session=True)
            try:
                deadline = time.monotonic() + 240
                while time.monotonic() < deadline:
                    data = args.log.read_text(errors='replace')
                    if marker in data:
                        passed = True
                        break
                    if 'FAIL: ' in data or process.poll() is not None:
                        break
                    time.sleep(0.25)
            finally:
                # Wine can leave a service below gamescopereaper after its
                # console program exits. Terminate only this private process
                # group, never a desktop compositor or an unrelated Wine server.
                if process.poll() is None:
                    os.killpg(process.pid, signal.SIGTERM)
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    os.killpg(process.pid, signal.SIGKILL)
                    process.wait(timeout=10)
        print(('PASS' if passed else 'FAIL') + ': isolated '+args.fixture+' fixture; evidence: ' + str(args.log))
        return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
