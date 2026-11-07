#!/usr/bin/env python3
"""Whole-source frozen/current A2FOVideo fixture with mocked native observations."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'build/rust-migration-stage9/video-work'
FROZEN = ROOT / 'build/rust-migration-stage8/before/modules/A2FOVideo/module.cpp'
CURRENT = ROOT / 'modules/A2FOVideo/module.cpp'
RUST = ROOT / 'rust/a2fo_core/src/stage9_video_runtime.rs'
HEADER = ROOT / 'rust/stage9_video_runtime_ffi.h'
TEST = ROOT / 'tests/rust_stage9_video_runtime_native.cpp'
BASE_TEST = ROOT / 'tests/video_module_test.cpp'
ARCHIVE = ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
FROZEN_SHA = 'e6d71cd6b0b6672119e55bf83ec73a667cb174eae8c993e39d36602fd73043ae'
PINS = (FROZEN, CURRENT, RUST, HEADER, TEST, BASE_TEST,
        ROOT / 'modules/A2FOVideo/presentation.hpp',
        ROOT / 'sdk/include/a2fo_module_api.h',
        ROOT / 'sdk/include/a2fo_supported_armada.hpp',
        ROOT / 'sdk/include/a2fo_supported_fleetops.hpp',
        *(ROOT / f'third_party/ffmpeg-win32/lib/lib{name}.a'
          for name in ('avformat', 'avcodec', 'swscale', 'swresample', 'avutil')))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def fingerprint(archive):
    return {str(path.relative_to(ROOT)): digest(path) for path in (*PINS, archive)}


def fixture(source, version):
    original = BASE_TEST.read_text()
    original = original[:original.index('\nint main(int argc, char** argv) {')]
    marker = '#include "../modules/A2FOVideo/module.cpp"'
    assert original.count(marker) == 1
    original = original.replace(marker, '#define GetFileAttributesA fake_GetFileAttributesA\n'
                                f'#include "{source}"\n#undef GetFileAttributesA')
    prototype = 'DWORD WINAPI fake_GetFileAttributesA(LPCSTR);\n'
    original = original.replace('BOOL WINAPI fake_IsWindowVisible(HWND);',
                                prototype + 'BOOL WINAPI fake_IsWindowVisible(HWND);')
    generated = WORK / f'video-{version}-fixture.cpp'
    generated.write_text(original + '\n#include "' + str(TEST) + '"\n')
    return generated


def ffmpeg_libs():
    env = os.environ.copy()
    env['PKG_CONFIG_PATH'] = str(ROOT / 'third_party/ffmpeg-win32/lib/pkgconfig')
    result = subprocess.run(['pkg-config', '--static', '--libs', 'libavformat',
                             'libavcodec', 'libswscale', 'libswresample', 'libavutil'],
                            cwd=ROOT, env=env, check=True, capture_output=True, text=True)
    return shlex.split(result.stdout)


def build(version, archive, rust_object):
    source = FROZEN if version == 'frozen' else CURRENT
    generated = fixture(source, version)
    exe = WORK / f'video-{version}.exe'
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall', '-Wextra',
               '-Wpedantic', '-Werror', '-Wno-unused-function', '-UNDEBUG', '-DWIN32_LEAN_AND_MEAN',
               '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
               '-static', '-static-libgcc', '-static-libstdc++',
               '-I' + str(ROOT / 'third_party/ffmpeg-win32/include')]
    if version == 'current':
        command.append('-DA2FO_CURRENT_VIDEO=1')
    command += [str(generated)]
    if rust_object is not None:
        command.append(str(rust_object))
    command += [str(archive), '-lgdi32', '-luser32',
                '-lwinmm', *ffmpeg_libs(), '-o', str(exe)]
    subprocess.run(command, cwd=ROOT, check=True, timeout=240)
    return exe


def wine_run(exe, prefix):
    env = os.environ.copy()
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env['WINEPREFIX'] = str(prefix)
    env['WINEDEBUG'] = '-all'
    env['WINEDLLOVERRIDES'] = ('winemenubuilder.exe=d;winedbg.exe=d;'
                              'winewayland.drv=d;winex11.drv=d')
    with tempfile.TemporaryFile() as output, tempfile.TemporaryFile() as errors:
        proc = subprocess.run(['wine', str(exe)], cwd=ROOT, env=env,
                              stdout=output, stderr=errors, timeout=180)
        output.seek(0)
        errors.seek(0)
        trace = output.read().replace(b'\r\n', b'\n')
        err = errors.read()
    if proc.returncode or b'result:all-modes-pass\n' not in trace:
        raise RuntimeError(f'{exe.name}: exit={proc.returncode}; stdout={trace[-2000:]!r}; '
                           f'stderr={err[-2000:]!r}')
    return trace


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--rust-lib', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WIN_LIB', ARCHIVE)))
    parser.add_argument('--wine-prefix', type=Path,
                        default=Path(os.environ.get('A2FO_RUST_WINEPREFIX',
                                                    '/tmp/a2fo-video-wine-prefix')))
    args = parser.parse_args()
    archive = args.rust_lib.resolve()
    if digest(FROZEN) != FROZEN_SHA:
        parser.error('frozen A2FOVideo source drift')
    if not archive.is_file():
        parser.error(f'canonical Rust archive missing: {archive}')
    initial = fingerprint(archive)
    WORK.mkdir(parents=True, exist_ok=True)
    symbols = subprocess.check_output(
        ['i686-w64-mingw32-nm', '-g', '--defined-only', str(archive)], text=True)
    obj = None
    if '_a2fo_rs_stage9_video_resolve' not in symbols:
        obj = WORK / 'stage9_video_runtime.o'
        subprocess.run(['rustc', '--target=i686-pc-windows-gnu', '--edition=2024',
                        '--emit=obj', '--crate-type=lib', str(RUST), '-o', str(obj)],
                       cwd=ROOT, check=True, timeout=120)
    built = {version: build(version, archive, obj) for version in ('frozen', 'current')}
    if fingerprint(archive) != initial:
        parser.error('input changed during fixture build')
    receipt = {'pins': initial, 'rust_object': digest(obj) if obj else None,
               'fixtures': {key: digest(value) for key, value in built.items()},
               'build_only': args.build_only}
    if not args.build_only:
        traces = {key: wine_run(exe, args.wine_prefix) for key, exe in built.items()}
        if traces['frozen'] != traces['current']:
            raise RuntimeError('frozen/current trace mismatch')
        if fingerprint(archive) != initial:
            parser.error('input changed during native run')
        (WORK / 'trace.txt').write_bytes(traces['current'])
        receipt['trace_sha256'] = hashlib.sha256(traces['current']).hexdigest()
        receipt['trace_lines'] = len(traces['current'].splitlines())
    (WORK / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(json.dumps(receipt, indent=2))


if __name__ == '__main__':
    main()
