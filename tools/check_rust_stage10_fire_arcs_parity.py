#!/usr/bin/env python3
"""Compare the original FireArcs module with the stage-ten candidate under Wine."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time

ROOT = Path(__file__).resolve().parents[1]
SLICE = ROOT / 'build/rust-migration-stage10/fire-arcs'
STAGE8 = ROOT / 'build/rust-migration-stage8'
UNIT = Path('modules/A2FOFireArcs/module.cpp')
FROZEN = STAGE8 / 'before' / UNIT
FIXTURE = ROOT / 'tests/rust_stage10_fire_arcs_native.cpp'
ORIGINAL_ARCHIVE = ROOT / 'build/rust-migration-stage9/checkpoint-archive10/liba2fo_core-win.a'
KEEPALIVE = ROOT / 'build/rust-migration-stage9/batch5-regression-native-approved/keepalive.exe'
KEEPALIVE_SHA = '42767f0c5292f21753dc50d3cf9a17759672d62ff8a4dcbe3d2f95541bec3b45'
MODES = (
    'init-short init-no-module init-module-throw root-too-many root-disabled '
    'root-overlay init-events-missing init-legacy-trigger '
    'init-legacy-trigger-throw init-trigger-false init-class-false '
    'init-trigger-throw init-class-throw init-signature init-hook-first-false '
    'init-hook-third-false init-hook-throw init-success class-box class-cone '
    'class-alias class-invalid class-mode-invalid class-no-coverage '
    'class-box-angle class-cone-width class-pc24 class-pc53 class-pc64 '
    'trigger-allow trigger-reject trigger-auto trigger-short trigger-committed '
    'trigger-disabled canfire-allow canfire-reject canfire-native-false '
    'canfire-no-policy canfire-disabled canfire-mutate ui-ship ui-system '
    'ui-disabled ui-invalid-index '
    'trigger-owner-missing trigger-transform-missing trigger-null-target '
    'trigger-transform-mutate '
    'text-high text-nul text-normal float-direct query-missing query-empty '
    'query-trim query-259 query-260 query-nul root-missing root-null '
    'root-empty-path root-invalid '
    'root-empty root-exact-size root-oversize vector-empty vector-reversed '
    'vector-misaligned vector-256 vector-257 vector-unreadable '
    'numeric-pc24-rcnear numeric-pc24-rcdown numeric-pc24-rcup '
    'numeric-pc24-rcchop numeric-pc53-rcnear numeric-pc53-rcdown '
    'numeric-pc53-rcup numeric-pc53-rcchop numeric-pc64-rcnear '
    'numeric-pc64-rcdown numeric-pc64-rcup numeric-pc64-rcchop'
).split()
PRIORITY_MODES = ('root-exact-size', 'root-oversize')
MODES = tuple([mode for mode in MODES if mode.startswith('numeric-')] +
              list(PRIORITY_MODES) +
              [mode for mode in MODES if not mode.startswith('numeric-')
               and mode not in PRIORITY_MODES])
FLAVORS = ('standalone', 'builtin')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def run(command, output, label, env=None):
    with (output / f'{label}.stdout.log').open('wb') as stdout, \
         (output / f'{label}.stderr.log').open('wb') as stderr:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=stdout,
                                stderr=stderr, timeout=300, check=False)
    if result.returncode:
        raise RuntimeError(f'{label} exited {result.returncode}; see {output}')


def dependencies(source, flavor, output, label, pinned, translation_unit=FIXTURE):
    command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-MM',
               f'-DMODULE_SOURCE="{source}"']
    if flavor == 'builtin':
        command += ['-DA2FO_BUILTIN_FEATURE=a2fo::BuiltinFeature::fire_arcs']
    command += [str(translation_unit)]
    result = subprocess.run(command, cwd=ROOT, capture_output=True,
                            timeout=60, check=True)
    (output / f'{label}-dependencies.txt').write_bytes(result.stdout)
    entries = result.stdout.decode().replace('\\\n', ' ').split(':', 1)[1].split()
    project = sorted({Path(entry).resolve().relative_to(ROOT).as_posix()
                      for entry in entries if Path(entry).resolve().is_relative_to(ROOT)
                      and Path(entry).resolve() not in (translation_unit, source)})
    for path in project:
        archive_prefix = 'build/rust-migration-stage8/before/'
        manifest_key = path.removeprefix(archive_prefix)
        if manifest_key not in pinned or digest(ROOT / path) != pinned[manifest_key]:
            raise RuntimeError(f'unverified {label} project dependency: {path}')
    return project


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib', type=Path, required=True)
    parser.add_argument('--output-dir', type=Path, required=True)
    parser.add_argument('--wine-prefix', type=Path)
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    output = args.output_dir.resolve()
    if output.exists():
        parser.error(f'output directory already exists: {output}')
    if output.parent != SLICE or not output.name.startswith('fixtures'):
        parser.error('output directory must be a new fixtures* child of the FireArcs slice')
    archive = args.rust_lib.resolve(strict=True)
    frozen_pins = json.loads((STAGE8 / 'before-hashes.json').read_text())
    archive10_pins = json.loads((ROOT / 'build/rust-migration-stage9/'
                                 'checkpoint-archive10/source-hashes.json').read_text())
    original_sha = json.loads((SLICE / 'before-hashes.json').read_text())[
        'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a']
    if digest(FROZEN) != frozen_pins[str(UNIT)]:
        raise RuntimeError('original Stage8 FireArcs source drifted')
    if digest(ORIGINAL_ARCHIVE) != original_sha:
        raise RuntimeError('original archive10 Windows archive drifted')
    if digest(KEEPALIVE) != KEEPALIVE_SHA:
        raise RuntimeError('approved Wine keepalive drifted')
    for path in ('modules/A2FOFireArcs/fire_arc.cpp',
                 'modules/A2FOFireArcs/runtime_config.cpp'):
        if digest(ROOT / path) != frozen_pins[path]:
            raise RuntimeError(f'original auxiliary source drifted: {path}')
    output.mkdir(parents=True)
    paths = {'fixture': FIXTURE, 'checker': Path(__file__),
             'frozen_source': FROZEN, 'current_source': ROOT / UNIT,
             'frozen_manifest': STAGE8 / 'before-hashes.json',
             'archive10_manifest': ROOT / 'build/rust-migration-stage9/'
                                   'checkpoint-archive10/source-hashes.json',
             'original_archive': ORIGINAL_ARCHIVE, 'candidate_archive': archive,
             'keepalive': KEEPALIVE,
             'fire_arc_source': ROOT / 'modules/A2FOFireArcs/fire_arc.cpp',
             'runtime_config_source': ROOT / 'modules/A2FOFireArcs/runtime_config.cpp',
             'rust_lib_source': ROOT / 'rust/a2fo_core/src/lib.rs'}
    for rust_source in ROOT.glob('rust/a2fo_core/src/stage10_fire_arcs*.rs'):
        paths[rust_source.relative_to(ROOT).as_posix()] = rust_source
    for header in ROOT.glob('rust/stage10_fire_arcs*.h'):
        paths[header.relative_to(ROOT).as_posix()] = header
    source_hashes = {name: digest(path) for name, path in paths.items()}
    snapshots = output / 'source-used'
    snapshots.mkdir()
    for name in ('fixture', 'checker', 'current_source'):
        original = paths[name]
        destination = snapshots / name / original.name
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(original, destination)
        if digest(destination) != source_hashes[name]:
            raise RuntimeError(f'source snapshot mismatch: {name}')
    for name, original in paths.items():
        if name.startswith('rust/stage10_fire_arcs'):
            destination = snapshots / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(original, destination)
            if digest(destination) != source_hashes[name]:
                raise RuntimeError(f'source snapshot mismatch: {name}')
    dependencies_by_build = {}
    for auxiliary in ('fire_arc_source', 'runtime_config_source'):
        dependencies_by_build[f'aux-{auxiliary}'] = dependencies(
            paths[auxiliary], 'standalone', output, f'aux-{auxiliary}',
            frozen_pins, paths[auxiliary])
    binaries = {}
    for flavor in FLAVORS:
        for version, source, lib in (('before', FROZEN, ORIGINAL_ARCHIVE),
                                     ('current', ROOT / UNIT, archive)):
            label = f'{flavor}-{version}'
            # The current module may include a new stage-ten FFI header. Its
            # digest is still pinned below; older dependencies must match Stage8.
            pins = dict(frozen_pins if version == 'before' else archive10_pins)
            if version == 'current':
                for header in ROOT.glob('rust/stage10_fire_arcs*.h'):
                    pins[header.relative_to(ROOT).as_posix()] = digest(header)
            dependencies_by_build[label] = dependencies(source, flavor, output,
                                                        label, pins)
            exe = output / f'{label}.exe'
            command = ['i686-w64-mingw32-g++', '-std=gnu++17', '-O2',
                       '-Wall', '-Wextra', '-Wpedantic', '-Werror',
                       '-ffunction-sections', '-fdata-sections',
                       '-Wl,--gc-sections', '-static',
                       f'-DMODULE_SOURCE="{source}"']
            if flavor == 'builtin':
                command += ['-DA2FO_BUILTIN_FEATURE=a2fo::BuiltinFeature::fire_arcs']
            command += [str(FIXTURE), str(paths['fire_arc_source']),
                        str(paths['runtime_config_source']), str(lib),
                        '-lws2_32', '-lbcrypt', '-lole32', '-ladvapi32', '-o', str(exe)]
            run(command, output, f'build-{label}')
            binaries[label] = digest(exe)
    receipt = {'sources': source_hashes, 'dependencies': dependencies_by_build,
               'binaries': binaries, 'modes': MODES, 'flavors': FLAVORS,
               'native_invocations': len(MODES) * len(FLAVORS) * 2,
               'pairs': len(MODES) * len(FLAVORS)}
    receipt['dependency_outputs'] = {
        label: digest(output / f'{label}-dependencies.txt')
        for label in dependencies_by_build}
    def verify():
        for name, path in paths.items():
            if digest(path) != source_hashes[name]:
                raise RuntimeError(f'input drifted: {name}')
        for name, sha in binaries.items():
            if digest(output / f'{name}.exe') != sha:
                raise RuntimeError(f'binary drifted: {name}')
        for label, sha in receipt['dependency_outputs'].items():
            if digest(output / f'{label}-dependencies.txt') != sha:
                raise RuntimeError(f'dependency list drifted: {label}')
        for label, projects in dependencies_by_build.items():
            pins = (frozen_pins if label.endswith('before') or
                    label.startswith('aux-') else archive10_pins)
            for project in projects:
                key = project.removeprefix('build/rust-migration-stage8/before/')
                expected = pins.get(key, source_hashes.get(project))
                if expected is None or digest(ROOT / project) != expected:
                    raise RuntimeError(f'dependency drifted: {label}/{project}')
    verify()
    (output / 'inputs.json').write_text(json.dumps(receipt, indent=2) + '\n')
    if args.build_only:
        print(f'PASS built four FireArcs fixtures: {output}')
        return
    if args.wine_prefix is None:
        parser.error('--wine-prefix required for execution; lead serializes Wine')
    prefix = args.wine_prefix.resolve()
    if not (prefix / 'system.reg').is_file() or not (prefix / 'drive_c').is_dir():
        parser.error('initialized private Wine prefix required')
    env = dict(os.environ)
    env.pop('DISPLAY', None)
    env.pop('WAYLAND_DISPLAY', None)
    env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all',
               LIBGL_ALWAYS_SOFTWARE='1',
               WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;'
                                  'winewayland.drv=d;winex11.drv=d',
               A2FO_FIRE_ARCS_ROOT_DIR=str(output.relative_to(ROOT)).replace('/', '\\'))
    log_hashes = {}
    marker = prefix / 'drive_c' / f'a2fo-stage10-fire-arcs-stop-{os.getpid()}'
    marker.unlink(missing_ok=True)
    with (output / 'keepalive.log').open('wb') as keepalive_log:
        keeper = subprocess.Popen(['wine', str(KEEPALIVE), 'C:\\' + marker.name],
                                  cwd=ROOT, env=env, stdout=keepalive_log,
                                  stderr=subprocess.STDOUT)
        try:
            started = time.monotonic()
            while b'ready' not in (output / 'keepalive.log').read_bytes():
                if keeper.poll() is not None:
                    raise RuntimeError('Wine keepalive exited during startup')
                if time.monotonic() - started > 360:
                    raise RuntimeError('Wine keepalive startup timed out')
                time.sleep(0.5)
            for flavor in FLAVORS:
                for mode in MODES:
                    outputs = {}
                    for version in ('before', 'current'):
                        label = f'{flavor}-{version}'
                        verify()
                        for root in ('root0', 'root1'):
                            shutil.rmtree(output / root, ignore_errors=True)
                        log = f'{flavor}-{mode}-{version}'
                        run(['wine', str(output / f'{label}.exe'), mode], output, log, env)
                        raw = output / f'{log}.stdout.log'
                        outputs[version] = raw.read_bytes().replace(b'\r\n', b'\n')
                        if not outputs[version] or (
                                b'init:' not in outputs[version] and
                                b'throw:' not in outputs[version] and
                                b'terminate:' not in outputs[version]):
                            raise RuntimeError(f'empty or incomplete trace: {log}')
                        if mode != 'init-hook-throw' and b'terminate:' in outputs[version]:
                            raise RuntimeError(f'unexpected terminate trace: {log}')
                        if mode == 'init-hook-throw' and b'terminate:' not in outputs[version]:
                            raise RuntimeError(f'expected terminate trace absent: {log}')
                        active_case = (mode.startswith((
                            'class-', 'trigger-', 'canfire-', 'ui-', 'text-',
                            'query-', 'vector-', 'numeric-')) or
                            mode == 'float-direct')
                        if active_case and (f'completed:{mode}\n'.encode()
                                            not in outputs[version]):
                            raise RuntimeError(f'active mode did not complete: {log}')
                        normalized = output / f'{log}.normalized.txt'
                        normalized.write_bytes(outputs[version])
                        log_hashes[log] = {
                            'raw': digest(raw), 'normalized': digest(normalized),
                            'stderr': digest(output / f'{log}.stderr.log')}
                        verify()
                    if outputs['before'] != outputs['current']:
                        raise RuntimeError(f'FireArcs mismatch: {flavor}/{mode}')
                    print(f'PASS {flavor}/{mode}', flush=True)
        finally:
            marker.touch()
            try:
                keeper.wait(timeout=30)
            except subprocess.TimeoutExpired:
                keeper.terminate()
                keeper.wait(timeout=30)
            marker.unlink(missing_ok=True)
    receipt['logs'] = log_hashes
    receipt['equal'] = True
    (output / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'PASS {receipt["native_invocations"]} native invocations; '
          f'{receipt["pairs"]} matching pairs: {output}')


if __name__ == '__main__':
    main()
