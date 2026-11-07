#!/usr/bin/env python3
"""Pinned frozen/current native presentation regressions for stage ten.

Writes only a fresh fixtures* child of presentation-runtime-batch. Wine runs
serially with synthetic fixtures and an already initialized private prefix.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess


ROOT = Path(__file__).resolve().parents[1]
SLICE = ROOT / 'build/rust-migration-stage10/presentation-runtime-batch'
BEFORE = SLICE / 'before'
ORIGINAL = (ROOT / 'build/rust-migration-stage10/core-completion/checkpoint/'
            'artifacts/rust/i686-pc-windows-gnu/release/liba2fo_core.a')
CANDIDATE = SLICE / 'candidate/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
CXX = 'i686-w64-mingw32-g++'
BASE_FLAGS = ['-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
              '-UNDEBUG', '-DWIN32_LEAN_AND_MEAN', '-DNOMINMAX', '-static',
              '-static-libgcc', '-static-libstdc++']
RUST_LIBS = ['-lntdll', '-luserenv', '-lws2_32', '-lbcrypt', '-ladvapi32']
REFIT_UNITS = tuple('modules/A2FORefitMenus/' + name + '.cpp' for name in (
    'module', 'loading_native', 'setup_native', 'menu_scene', 'presentation',
    'skin', 'skin_native', 'scene_catalogue', 'mission_backdrop',
    'scene_composition', 'setup_presentation', 'loading_gpu',
    'loading_content', 'loading_presentation', 'loading_theme', 'setup_skin'))
REFIT_OBJECTS = ('build/refit_menu_ui_skin.o', 'build/refit_menu_fonts.o')
SHARED_VIDEO_FIXTURE = ROOT / 'tests/rust_stage10_video_completion_native.cpp'
SHARED_YARDS_FIXTURE = ROOT / 'tests/rust_stage10_refit_yards_traversal_native.cpp'
REFIT_MODES = ('missing', 'disabled', 'invalid', 'api', 'identity', 'armada',
               'profile', 'foreign', 'action', 'patch-fail', 'skin-signature',
               'skin-patch-fail', 'success', 'loading', 'loading-signature',
               'mission', 'mission-signature', 'mission-fo', 'mission-fo-gateway')
YARDS_MODES = ('travel', 'queue', 'cancel', 'cleanup', 'nearest', 'numeric24',
               'numeric53', 'numeric64', 'init-signature', 'init-hook-false',
               'init-hook-throw', 'init-class-false', 'init-class-throw',
               'init-craft-false', 'init-craft-throw')
from check_rust_stage8_a_turrets_parity import MODES as TURRET_MODES, EXPECTED_MARKERS


def sha(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def file_at(version, logical):
    return (BEFORE if version == 'before' else ROOT) / logical


def logged(command, work, name, cwd=ROOT, env=None, timeout=360):
    out, err = work / (name + '.stdout'), work / (name + '.stderr')
    with out.open('wb') as stdout, err.open('wb') as stderr:
        try:
            result = subprocess.run(command, cwd=cwd, env=env, stdout=stdout,
                                    stderr=stderr, timeout=timeout, check=False)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = 'timeout'
    record = {'command': list(map(str, command)), 'cwd': str(cwd),
              'returncode': code, 'stdout': str(out), 'stderr': str(err),
              'stdout_sha256': sha(out), 'stderr_sha256': sha(err)}
    (work / (name + '.json')).write_text(json.dumps(record, indent=2) + '\n')
    if code != 0:
        raise RuntimeError(f'{name}: exit {code}; see {err}')
    return record


def deps(command, units, work, name):
    result = set()
    for index, unit in enumerate(units):
        if unit.suffix not in ('.cpp', '.cc', '.c', '.S'):
            result.add(unit)
            continue
        flags = command[1:command.index(str(units[0]))]
        flags = [flag for flag in flags if not flag.startswith('-Wl,')]
        scan = [CXX, *flags, '-MM']
        if unit.suffix == '.S':
            scan += ['-x', 'assembler-with-cpp']
        scan.append(str(unit))
        record = logged(scan, work, f'{name}-dep-{index}', timeout=120)
        output = Path(record['stdout']).read_text().replace('\\\n', ' ')
        if ':' not in output:
            raise RuntimeError(f'{name}: empty dependency scan')
        result.update(Path(word).resolve() for word in
                      shlex.split(output.split(':', 1)[1]))
    return result


def video_fixture(version, work):
    base = file_at(version, 'tests/video_module_test.cpp')
    extra = file_at(version, 'tests/rust_stage9_video_runtime_native.cpp')
    extra_text = extra.read_text()
    tail = '    VirtualFree(g_armada, 0, MEM_RELEASE);\n}'
    if extra_text.count(tail) != 1:
        raise RuntimeError('Video stage9 fixture tail marker changed')
    extra_copy = work / f'video-stage9-{version}.cpp'
    extra_copy.write_text(extra_text.replace(tail,
                          '    VirtualFree(g_armada, 0, MEM_RELEASE);\n'
                          '    return 0;\n}'))
    content = base.read_text()
    marker = '\nint main(int argc, char** argv) {'
    if content.count(marker) != 1:
        raise RuntimeError('Video fixture main marker changed')
    content = content[:content.index(marker)]
    sleep_marker = '#define Sleep fake_Sleep'
    if content.count(sleep_marker) != 1:
        raise RuntimeError('Video Sleep mock marker changed')
    content = content.replace(sleep_marker, '#define Sleep stage10_fake_Sleep')
    include = '#include "../modules/A2FOVideo/module.cpp"'
    if content.count(include) != 1:
        raise RuntimeError('Video module include marker changed')
    content = content.replace(include,
        'static BOOL (WINAPI *stage10_real_qpc)(LARGE_INTEGER*) = '
        '&QueryPerformanceCounter;\n'
        'BOOL WINAPI stage10_fake_QueryPerformanceCounter(LARGE_INTEGER*);\n'
        '#define QueryPerformanceCounter stage10_fake_QueryPerformanceCounter\n'
        'VOID WINAPI stage10_fake_Sleep(DWORD);\n'
        '#define GetFileAttributesA fake_GetFileAttributesA\n'
        f'#include "{file_at(version, "modules/A2FOVideo/module.cpp")}"\n'
        '#undef GetFileAttributesA')
    prototype = 'BOOL WINAPI fake_IsWindowVisible(HWND);'
    if content.count(prototype) != 1:
        raise RuntimeError('Video prototype marker changed')
    content = content.replace(prototype,
        'DWORD WINAPI fake_GetFileAttributesA(LPCSTR);\n' + prototype)
    target = work / f'video-{version}-fixture.cpp'
    target.write_text(content +
        f'\n#include "{SHARED_VIDEO_FIXTURE}"\n'
        f'#define main stage9_video_fixture_main\n#include "{extra_copy}"\n'
        '#undef main\n'
        'int main() {\n'
        '    const int result = stage9_video_fixture_main();\n'
        '    if (result != 0) return result;\n'
        '    stage10_video_completion_native_tests();\n'
        '    return 0;\n}\n')
    return target, [base, extra, extra_copy, SHARED_VIDEO_FIXTURE]


def mission_source(version, work):
    source = file_at(version, 'modules/A2FOMissionSelector/module.cpp')
    if version == 'current':
        return source, []
    content = source.read_text()
    marker = '#define NOMINMAX'
    if content.count(marker) != 1:
        raise RuntimeError('frozen mission NOMINMAX marker changed')
    content = content.replace(marker, '#ifndef NOMINMAX\n#define NOMINMAX\n#endif', 1)
    folder = work / 'frozen-mission/modules/A2FOMissionSelector'
    folder.mkdir(parents=True)
    target = folder / 'module.cpp'
    target.write_text(content)
    buttons = file_at(version, 'modules/A2FOMissionSelector/standard_buttons.inl')
    shutil.copy2(buttons, folder / buttons.name)
    for name in ('sdk', 'rust'):
        (work / 'frozen-mission' / name).symlink_to(BEFORE / name, target_is_directory=True)
    return target, [source, buttons]


def yards_fixture(version, work):
    original = file_at(version, 'tests/rust_stage8_lead_refit_native.cpp')
    target = work / f'refit-yards-{version}-fixture.cpp'
    target.write_text(
        '#define main stage8_yards_fixture_main\n'
        f'#include "{original}"\n'
        '#undef main\n'
        f'#include "{SHARED_YARDS_FIXTURE}"\n'
        'int main(int argc, char** argv) {\n'
        '    const int result = stage8_yards_fixture_main(argc, argv);\n'
        '    if (result == 0 && argc == 2 && std::strcmp(argv[1], "nearest") == 0)\n'
        '        stage10_refit_yards_traversal_tests();\n'
        '    return result;\n}\n')
    return target, [original, SHARED_YARDS_FIXTURE]


def ffmpeg_flags(work):
    env = os.environ.copy()
    env['PKG_CONFIG_PATH'] = str(ROOT / 'third_party/ffmpeg-win32/lib/pkgconfig')
    record = logged(['pkg-config', '--static', '--libs', 'libavformat',
                     'libavcodec', 'libswscale', 'libswresample', 'libavutil'],
                    work, 'ffmpeg-pkg-config', env=env)
    return shlex.split(Path(record['stdout']).read_text())


def plans(work, ffmpeg):
    result = []
    for version, archive in (('before', ORIGINAL), ('current', CANDIDATE)):
        flags = BASE_FLAGS + ['-ffunction-sections', '-fdata-sections',
                              '-Wl,--gc-sections']
        video, video_inputs = video_fixture(version, work)
        video_flags = [x for x in flags if x != '-DNOMINMAX'] + [
            '-Wno-unused-function', '-I' + str(ROOT / 'third_party/ffmpeg-win32/include')]
        if version == 'current':
            video_flags.append('-DA2FO_CURRENT_VIDEO=1')
        video_exe = work / f'video-{version}.exe'
        result.append(('video', version, 'default', video_exe,
            [CXX, *video_flags, str(video), str(archive), '-lgdi32', '-luser32',
             '-lwinmm', *ffmpeg, *RUST_LIBS, '-o', str(video_exe)],
            [video], video_inputs))
        mission, mission_inputs = mission_source(version, work)
        for variant in ('main', 'layout'):
            fixture = file_at(version, 'tests/rust_stage8_c_mission_' +
                              ('mock.cpp' if variant == 'main' else 'layout_mock.cpp'))
            exe = work / f'mission-{variant}-{version}.exe'
            cmd = [CXX, *[x for x in flags if x not in
                         ('-Wpedantic', '-DWIN32_LEAN_AND_MEAN')],
                   f'-DA2FO_STAGE8_MISSION_SOURCE="{mission}"', str(fixture),
                   str(archive), '-lgdiplus', '-lgdi32', '-lcomctl32', '-lole32',
                   '-luuid', *RUST_LIBS, '-o', str(exe)]
            result.append((f'mission-{variant}', version, 'default', exe,
                           cmd, [fixture], [fixture, *mission_inputs]))
        yard_fixture, yard_inputs = yards_fixture(version, work)
        yard_module = file_at(version, 'modules/A2FORefitYards/module.cpp')
        yard_policy = file_at(version, 'modules/A2FORefitYards/refit_policy.cpp')
        exe = work / f'refit-yards-{version}.exe'
        cmd = [CXX, *flags, f'-DMODULE_SOURCE="{yard_module}"',
               str(yard_fixture), str(yard_policy), str(archive),
               '-lws2_32', '-lbcrypt', '-lole32', '-ladvapi32',
               *RUST_LIBS, '-o', str(exe)]
        result.append(('refit-yards', version, 'default', exe, cmd,
                       [yard_fixture, yard_policy], yard_inputs))
        turret_fixture = file_at(version, 'tests/rust_stage8_a_turrets_native.cpp')
        turret_module = file_at(version, 'modules/A2FOTurrets/module.cpp')
        turret_math = file_at(version, 'modules/A2FOTurrets/turret_math.cpp')
        turret_combat = file_at(version, 'modules/A2FOTurrets/turret_combat.cpp')
        for flavor in ('standalone', 'builtin'):
            exe = work / f'turrets-{flavor}-{version}.exe'
            defs = ([] if flavor == 'standalone' else [
                '-DA2FO_ModuleInit=A2FOBuiltinTurrets_Init',
                '-DA2FO_ModuleShutdown=A2FOBuiltinTurrets_Shutdown'])
            cmd = [CXX, *flags, *defs, f'-DMODULE_SOURCE="{turret_module}"',
                   str(turret_fixture), str(turret_math), str(turret_combat),
                   str(archive), '-ladvapi32', *RUST_LIBS, '-o', str(exe)]
            result.append(('turrets', version, flavor, exe, cmd,
                           [turret_fixture, turret_math, turret_combat], []))
        dll = work / f'refit-menus-{version}.dll'
        units = [file_at(version, name) for name in REFIT_UNITS]
        units.append(file_at(version, 'modules/A2FORefitMenus/native_bridge.S'))
        objects = [ROOT / name for name in REFIT_OBJECTS]
        dll_cmd = [CXX, *BASE_FLAGS, '-shared', '-Wl,--enable-stdcall-fixup',
                   '-I', str(file_at(version, 'modules/A2FORefitMenus')),
                   *map(str, units), *map(str, objects), str(archive),
                   '-lcomctl32', '-lgdi32', '-lmsimg32', '-ladvapi32',
                   '-lgdiplus', *RUST_LIBS, '-o', str(dll)]
        result.append(('refit-menus-dll', version, 'default', dll, dll_cmd,
                       units, objects))
        fixture = file_at(version, 'tests/rust_stage8_b_refit_native.cpp')
        bridge = file_at(version, 'tests/refit_menu_native_bridge.S')
        hook = file_at(version, 'core/hook.cpp') if version == 'before' else ROOT / 'tests/native_reference/adapters/core/hook.cpp'
        exe = work / f'refit-menus-fixture-{version}.exe'
        fixture_cmd = [CXX, *BASE_FLAGS, '-Wl,--image-base,0x10000000',
                       str(fixture), str(bridge), str(hook), str(archive),
                       '-lgdi32', *RUST_LIBS, '-o', str(exe)]
        result.append(('refit-menus-fixture', version, 'default', exe,
                       fixture_cmd, [fixture, bridge, hook], []))
    return result


def validate(name, mode, trace):
    if name == 'video':
        if (b'result:all-modes-pass\n' not in trace or
                b'stage10:wait-and-audio-frame-pass\n' not in trace):
            raise RuntimeError('Video completion marker absent')
    elif name == 'mission-main':
        if any(marker not in trace for marker in
               (b'mission_label:', b'campaign_label:', b'ini:campaign',
                b'ini:mission', b'native:campaign', b'launch_rejected:')):
            raise RuntimeError('Mission main trace missing catalog or launch coverage')
    elif name == 'mission-layout':
        if not trace.strip():
            raise RuntimeError('Mission layout trace empty')
    elif name == 'refit-menus':
        reported = 'success' if mode.startswith(('loading', 'mission')) else mode
        if trace != f'refit menu {reported}: PASS\n'.encode():
            raise RuntimeError(f'RefitMenus {mode} completion marker absent')
    elif name == 'refit-yards':
        if not trace.strip():
            raise RuntimeError(f'RefitYards {mode} trace empty')
        if mode == 'nearest' and \
                b'stage10 refit yards: eight live-list mutation/guard cases PASS\n' not in trace:
            raise RuntimeError('RefitYards nearest traversal marker absent')
    elif name == 'turrets':
        if b'state:classes=' not in trace:
            raise RuntimeError(f'Turrets {mode} state marker absent')
        marker = EXPECTED_MARKERS.get(mode)
        if marker and marker not in trace:
            raise RuntimeError(f'Turrets {mode} marker absent')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', required=True, type=Path)
    parser.add_argument('--rust-lib', type=Path, default=CANDIDATE)
    parser.add_argument('--build-receipt', type=Path, required=True)
    parser.add_argument('--wine-prefix', type=Path,
                        default=Path('/tmp/a2fo-shields-wine-prefix'))
    parser.add_argument('--build-only', action='store_true')
    args = parser.parse_args()
    work = args.work_dir.resolve()
    if work.exists() or work.parent != SLICE or not work.name.startswith('fixtures'):
        parser.error('work directory must be a fresh fixtures* child of this batch')
    if args.rust_lib.resolve(strict=True) != CANDIDATE.resolve(strict=True):
        parser.error('candidate archive must be the batch candidate')
    receipt_path = args.build_receipt.resolve(strict=True)
    build = json.loads(receipt_path.read_text())
    candidate_key = CANDIDATE.relative_to(ROOT).as_posix()
    if build.get('status') != 'passed' or \
            build.get('artifacts', {}).get(candidate_key) != sha(CANDIDATE):
        parser.error('passed final build receipt does not pin candidate archive')
    predecessor = json.loads((SLICE / 'predecessor-artifacts.json').read_text())
    original_key = ('build/rust-migration-stage10/core-completion/candidate/rust/'
                    'i686-pc-windows-gnu/release/liba2fo_core.a')
    if predecessor.get(original_key) != sha(ORIGINAL):
        parser.error('predecessor archive differs from frozen checkpoint pin')
    frozen = json.loads((SLICE / 'before-hashes.json').read_text())
    if len(frozen) != 1241:
        parser.error('frozen snapshot inventory differs from 1241-file baseline')
    current_inputs = build.get('inputs_after', {})
    if not current_inputs:
        parser.error('final build receipt has no current product input pins')
    required_build = {'Makefile', 'rust/a2fo_core/Cargo.toml',
                      'rust/a2fo_core/Cargo.lock'}
    if not required_build.issubset(current_inputs):
        parser.error('final build receipt lacks core product input pins')
    work.mkdir()
    receipt = {'status': 'preparing', 'build_only': args.build_only,
               'assertions_enabled': True, 'expected_trace_pairs': 101,
               'trace_pairs': 0, 'executions': 0, 'cases': {}, 'inputs': {},
               'dependency_scans': {}, 'build_receipt': str(receipt_path)}
    def save():
        (work / 'receipt.json').write_text(json.dumps(receipt, indent=2,
                                                       sort_keys=True) + '\n')
    try:
        pins = {}
        def pin(path, expected=None):
            path = Path(path).resolve(strict=True)
            actual = sha(path)
            if expected is not None and expected != actual:
                raise RuntimeError(f'authoritative input pin drift: {path}')
            if path in pins and pins[path] != actual:
                raise RuntimeError(f'input changed during preparation: {path}')
            pins[path] = actual
        for logical, expected in frozen.items():
            pin(BEFORE / logical, expected)
        for logical, expected in current_inputs.items():
            pin(ROOT / logical, expected)
        for path in (ORIGINAL, CANDIDATE, receipt_path, Path(__file__),
                     SLICE / 'before-hashes.json',
                     SLICE / 'predecessor-artifacts.json',
                     ROOT / 'ArmadaL.exe', ROOT / 'FleetOpsHook.dll'):
            pin(path)
        for name in REFIT_OBJECTS:
            pin(ROOT / name)
        pin(ROOT / 'tools/check_rust_stage8_a_turrets_parity.py')
        for path in (ROOT / 'tests').rglob('*'):
            if path.is_file():
                pin(path)
        ffmpeg = ffmpeg_flags(work)
        for path in (ROOT / 'third_party/ffmpeg-win32').rglob('*'):
            if path.is_file():
                pin(path)
        plan = plans(work, ffmpeg)
        for name, version, flavor, exe, command, units, extra in plan:
            key = f'{name}-{flavor}-{version}'
            for path in [*units, *extra]:
                pin(path)
            if version == 'current':
                if name == 'refit-menus-dll':
                    for path in units:
                        pin(path)
                elif name in ('refit-yards', 'turrets'):
                    macro = next(value for value in command
                                 if value.startswith('-DMODULE_SOURCE='))
                    pin(Path(macro.split('=', 1)[1].strip('"')))
            found = deps(command, units, work, key)
            for path in found:
                if path.is_relative_to(BEFORE):
                    logical = path.relative_to(BEFORE).as_posix()
                    pin(path, frozen.get(logical))
                    if logical not in frozen:
                        raise RuntimeError(f'unpinned frozen dependency: {logical}')
                elif path.is_relative_to(ROOT) and version == 'before' and \
                        not path.is_relative_to(work) and path not in {
                            *(ROOT / item for item in REFIT_OBJECTS),
                            SHARED_VIDEO_FIXTURE, SHARED_YARDS_FIXTURE} and \
                        not path.is_relative_to(ROOT / 'third_party/ffmpeg-win32'):
                    raise RuntimeError(f'frozen dependency escaped snapshot: {path}')
                else:
                    pin(path)
            receipt['dependency_scans'][key] = sorted(map(str, found))
        def verify():
            changed = [str(path) for path, expected in pins.items()
                       if sha(path) != expected]
            if changed:
                raise RuntimeError('input drift: ' + ', '.join(changed[:8]))
        verify()
        receipt['inputs'] = {str(path): digest for path, digest in pins.items()}
        for path, digest in pins.items():
            target = (work / 'source-used' / path.relative_to(ROOT) if
                      path.is_relative_to(ROOT) else
                      work / 'source-used/external' / digest / path.name)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            if sha(target) != digest:
                raise RuntimeError(f'source-used copy mismatch: {path}')
        save()
        for name, version, flavor, exe, command, _, _ in plan:
            verify()
            key = f'{name}-{flavor}-{version}'
            record = logged(command, work, key + '-compile')
            receipt['cases'][key] = {'compile': record, 'artifact': str(exe),
                                     'artifact_sha256': sha(exe)}
            save()
        verify()
        receipt['status'] = 'built'
        save()
        if args.build_only:
            return
        prefix = args.wine_prefix.resolve(strict=True)
        if not (prefix / 'system.reg').is_file():
            raise RuntimeError('initialized absolute disposable Wine prefix required')
        env = os.environ.copy()
        env.pop('DISPLAY', None)
        env.pop('WAYLAND_DISPLAY', None)
        env.update(WINEPREFIX=str(prefix), WINEDEBUG='-all',
                   WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;'
                   'winewayland.drv=d;winex11.drv=d', LIBGL_ALWAYS_SOFTWARE='1')
        suites = [('video', 'default', (None,)),
                  ('mission-main', 'default', (None,)),
                  ('mission-layout', 'default', (None,)),
                  ('refit-menus', 'default', REFIT_MODES),
                  ('refit-yards', 'default', YARDS_MODES),
                  *[('turrets', flavor, TURRET_MODES) for flavor in
                    ('standalone', 'builtin')]]
        for name, flavor, modes in suites:
            for mode in modes:
                traces = {}
                for version in ('before', 'current'):
                    verify()
                    artifact_name = ('refit-menus-fixture' if name == 'refit-menus'
                                     else name)
                    case = receipt['cases'][f'{artifact_name}-{flavor}-{version}']
                    exe = Path(case['artifact'])
                    if sha(exe) != case['artifact_sha256']:
                        raise RuntimeError(f'executable drift: {exe}')
                    argv = ['wine', str(exe)]
                    if name == 'refit-menus':
                        dll_case = receipt['cases'][f'refit-menus-dll-default-{version}']
                        dll = Path(dll_case['artifact'])
                        if sha(dll) != dll_case['artifact_sha256']:
                            raise RuntimeError(f'DLL drift: {dll}')
                        argv += [str(dll), str(ROOT / 'ArmadaL.exe'),
                                 str(ROOT / 'FleetOpsHook.dll'), mode]
                    elif mode:
                        argv.append(mode)
                    logical = f'{name}-{flavor}-{mode or "default"}'
                    runtime = work / 'runtime' / logical
                    runtime.mkdir(parents=True, exist_ok=False)
                    key = f'{logical}-{version}'
                    record = logged(argv, work, key, cwd=runtime,
                                    env=env, timeout=180)
                    captured = work / 'runtime-captured' / logical / version
                    captured.parent.mkdir(parents=True, exist_ok=True)
                    runtime.rename(captured)
                    assets = {str(path.relative_to(captured)): sha(path)
                              for path in captured.rglob('*') if path.is_file()}
                    receipt.setdefault('runtime_files', {}).update({
                        str(captured.relative_to(work) / relative): digest
                        for relative, digest in assets.items()})
                    stdout = Path(record['stdout']).read_bytes().replace(b'\r\n', b'\n')
                    stderr = Path(record['stderr']).read_bytes().replace(b'\r\n', b'\n')
                    validate(name, mode, stdout)
                    receipt['cases'][f'{artifact_name}-{flavor}-{version}'].setdefault(
                        'runs', {})[mode or 'default'] = {
                            **record, 'assets': assets}
                    traces[version] = (record['returncode'], stdout, stderr,
                                       sorted(assets.items()))
                    receipt['executions'] += 1
                    save()
                if traces['before'] != traces['current']:
                    raise RuntimeError(f'native trace mismatch: {logical}')
                receipt['trace_pairs'] += 1
                save()
        if receipt['trace_pairs'] != receipt['expected_trace_pairs']:
            raise RuntimeError('incomplete trace matrix')
        verify()
        receipt['status'] = 'pass'
        save()
    except Exception as error:
        receipt['status'] = 'failed'
        receipt['error'] = str(error)
        save()
        raise


if __name__ == '__main__':
    main()
