#!/usr/bin/env python3
"""Pinned, headless frozen/current native regressions for the render/UI batch.

The runner creates only a new fixtures* directory in the stage-ten batch slice.
It never writes to historical checker work directories or project sources.
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
SLICE = ROOT / 'build/rust-migration-stage10/render-ui-batch'
BEFORE = SLICE / 'before'
ORIGINAL = ROOT / 'build/rust-migration-stage10/squadrons/checkpoint/liba2fo_core-win.a'
ORIGINAL_SHA = '08c75d5764c7d7f7094b4dcbcf32da99205f51b93f673291984cfc399ba8fe9a'
CANDIDATE = SLICE / 'candidate/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
CXX = 'i686-w64-mingw32-g++'
FLAGS = ['-std=gnu++17', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror',
         '-UNDEBUG', '-DWIN32_LEAN_AND_MEAN', '-DNOMINMAX',
         '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
         '-static', '-static-libgcc', '-static-libstdc++']
RUST_LIBS = ['-lntdll', '-luserenv', '-lws2_32', '-lbcrypt', '-ladvapi32']
WIN_LIBS = ['-lgdi32', '-luser32', '-lole32', '-lshell32', '-lshlwapi', '-lcomctl32']
SHARED_FIXTURES = frozenset({
    'tests/rust_stage10_bink_native.cpp',
    'tests/rust_stage10_canvas_traversal_native.cpp',
})


def sha(path):
    digest = hashlib.sha256()
    with path.open('rb') as source:
        for block in iter(lambda: source.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def path_for(version, relative):
    return (BEFORE if version == 'before' else ROOT) / relative


def fixture_for(version, relative):
    return ROOT / relative if relative in SHARED_FIXTURES else path_for(version, relative)


def source_macro(name, path):
    return f'-D{name}="{path}"'


def flavors(kind):
    return ('standalone', 'builtin') if kind in (
        'directional', 'feature', 'feature-upgrade', 'rgb', 'texture') else ('standalone',)


def replace_one(content, old, new):
    if content.count(old) != 1:
        raise RuntimeError(f'fixture marker absent or repeated: {old}')
    return content.replace(old, new)


def suite():
    from check_rust_stage8_a_directional_parity import MODES as directional_modes
    from check_rust_stage9_texture_visuals_parity import MODES as texture_modes
    from check_rust_stage8_texture_runtime_parity import MODES as faction_modes
    tree_modes = ('tree-found', 'tree-signed-left', 'tree-signed-right',
                  'tree-null', 'tree-head', 'tree-inaccessible', 'tree-cycle')
    return [
        ('craft-module', 'tests/rust_stage9_craft_runtime_native.cpp', None, (), 'craft'),
        ('craft-object-editor', 'tests/rust_stage8_b_object_editor_native.cpp',
         'modules/A2FOCraftIdentity/object_editor.cpp',
         ('inactive', 'entry', 'shields', 'load-event'), 'object-editor'),
        ('craft-object-editor-policy', 'tests/rust_stage8_b_object_editor_policy.cpp',
         None, (), 'policy'),
        ('directional', 'tests/rust_stage8_a_directional_native.cpp',
         'modules/A2FODirectionalShields/module.cpp',
         directional_modes + tree_modes, 'directional'),
        ('feature-odf', 'tests/rust_stage8_c_odf_mock.cpp',
         'modules/A2FOFeaturePack/odf_recursive.cpp', (), 'feature'),
        ('feature-buildyard', 'tests/rust_stage8_c_buildyard_mock.cpp',
         'modules/A2FOFeaturePack/buildyard_pseudo_technology.cpp', (), 'feature'),
        ('feature-queue', 'tests/rust_stage8_c_queue_mock.cpp',
         'modules/A2FOFeaturePack/queue_enhancement.cpp', (), 'feature'),
        ('feature-upgrade', 'tests/rust_stage8_c_upgrade_mock.cpp',
         'modules/A2FOFeaturePack/upgrade_pods.cpp', (), 'feature-upgrade'),
        ('shell-native-intro', 'tests/shell_native_intro_test.cpp',
         'modules/A2FOShellDisplay/native_intro.cpp', (), 'native-intro'),
        ('shell-bink-numeric', 'tests/rust_stage10_bink_native.cpp',
         'modules/A2FOShellDisplay/bink_video.cpp', (), 'bink-numeric'),
        ('shell-bink-policy', 'tests/rust_stage8_b_bink_policy.cpp',
         None, (), 'policy'),
        ('shell-canvas-policy', 'tests/rust_stage8_b_canvas_policy.cpp',
         None, (), 'policy'),
        ('shell-canvas-traversal', 'tests/rust_stage10_canvas_traversal_native.cpp',
         'modules/A2FOShellDisplay/canvas.cpp', (), 'canvas-traversal'),
        *[(f'shell-canvas-{name}', f'tests/shell_canvas_{name}_test.cpp',
           'modules/A2FOShellDisplay/canvas.cpp', (), 'canvas')
          for name in ('lifecycle', 'dialog', 'scroll', 'copy')],
        ('rgb', 'tests/rust_stage9_rgb_runtime_native.cpp',
         'modules/A2FORGBTextures/module.cpp', (), 'rgb'),
        ('texture-faction', 'tests/rust_stage8_texture_runtime_native.cpp',
         'modules/A2FOTextureVariants/module.cpp', faction_modes, 'texture'),
        ('texture-visuals', 'tests/rust_stage9_texture_visuals_native.cpp',
         'modules/A2FOTextureVariants/module.cpp', texture_modes, 'texture'),
    ]


def prepare(name, fixture, source, kind, version, work):
    test = fixture_for(version, fixture)
    content = test.read_text()
    extra = []
    macros = []
    source_path = path_for(version, source) if source else None
    if kind == 'craft':
        base = path_for(version, 'tests/craft_identity_ammunition_hover_test.cpp')
        base_text = replace_one(base.read_text(), 'int main() {',
                                'void stage9_existing_craft_hover_main() {')
        generated_base = work / f'{name}-{version}-base.cpp'
        generated_base.write_text(base_text)
        macros += [source_macro('A2FO_AMMUNITION_MODULE_SOURCE',
                                path_for(version, 'modules/A2FOCraftIdentity/module.cpp')),
                   source_macro('CRAFT_BASE_SOURCE', generated_base)]
        module_dir = path_for(version, 'modules/A2FOCraftIdentity')
        extra += [*sorted(p for p in module_dir.glob('*.cpp') if p.name != 'module.cpp'),
                  module_dir / 'thiscall_bridge.S']
    elif kind == 'object-editor':
        content = replace_one(content,
            '#include "../modules/A2FOCraftIdentity/object_editor.cpp"',
            f'#include "{source_path}"')
        extra += [path_for(version, 'modules/A2FOCraftIdentity/object_editor_state.cpp')]
    elif kind == 'native-intro':
        for filename in ('native_intro.hpp', 'native_intro.cpp'):
            content = replace_one(content,
                f'#include "../modules/A2FOShellDisplay/{filename}"',
                f'#include "{source_path.parent / filename}"')
    elif kind == 'canvas':
        for filename in ('canvas.hpp', 'canvas.cpp'):
            content = replace_one(content,
                f'#include "../modules/A2FOShellDisplay/{filename}"',
                f'#include "{source_path.parent / filename}"')
        content = content.replace('#include "../sdk/include/a2fo_shell_canvas.h"',
                                  f'#include "{path_for(version, "sdk/include/a2fo_shell_canvas.h")}"')
        extra += [path_for(version, 'modules/A2FOShellDisplay/delphi_bridge.S')]
    elif kind == 'directional':
        content = replace_one(content, 'int main(int argc, char** argv) {',
                              'int stage8_inherited_main(int argc, char** argv) {')
        content += r'''
int main(int argc, char** argv) {
    if (argc != 2 || std::strncmp(argv[1], "tree-", 5) != 0)
        return stage8_inherited_main(argc, argv);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    try {
        setup();
        std::array<std::uint8_t, 0x18> head{}, root{}, left{}, right{};
        auto put = [](void* node, std::size_t offset, const void* value) {
            std::memcpy(static_cast<std::uint8_t*>(node) + offset,
                        &value, sizeof(value));
        };
        auto key = [](void* node, std::int32_t value) {
            std::memcpy(static_cast<std::uint8_t*>(node) +
                        kShieldEffectNodeKeyOffset, &value, sizeof(value));
        };
        void* head_pointer = head.data();
        void* root_pointer = root.data();
        void* left_pointer = left.data();
        void* right_pointer = right.data();
        put(head.data(), 0x04, root_pointer);
        put(root.data(), 0x00, left_pointer);
        put(root.data(), 0x08, right_pointer);
        key(root.data(), -3);
        key(left.data(), -10);
        key(right.data(), 8);
        void* root_value = reinterpret_cast<void*>(0x1010);
        void* left_value = reinterpret_cast<void*>(0x2020);
        void* right_value = reinterpret_cast<void*>(0x3030);
        put(root.data(), kShieldEffectNodeValueOffset, root_value);
        put(left.data(), kShieldEffectNodeValueOffset, left_value);
        put(right.data(), kShieldEffectNodeValueOffset, right_value);
        const std::string which = argv[1];
        std::int32_t wanted = -3;
        void* expected = root_value;
        if (which == "tree-signed-left") {
            wanted = -10; expected = left_value;
        } else if (which == "tree-signed-right") {
            wanted = 8; expected = right_value;
        } else if (which == "tree-null") {
            head_pointer = nullptr; expected = nullptr;
        } else if (which == "tree-head") {
            put(head.data(), 0x04, head_pointer); expected = nullptr;
        } else if (which == "tree-inaccessible") {
            void* bad = reinterpret_cast<void*>(1);
            put(head.data(), 0x04, bad); expected = nullptr;
        } else if (which == "tree-cycle") {
            wanted = -11; expected = nullptr;
            put(left.data(), 0x00, left_pointer);
        } else if (which != "tree-found") {
            return 2;
        }
        put(image + kShieldEffectMapHeadRva, 0, head_pointer);
        if (find_shield_effect(wanted) != expected)
            throw std::runtime_error("tree result mismatch");
        std::printf("tree:%s:pass\n", argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "directional tree fixture: %s\n", error.what());
        return 1;
    }
}
'''
        macros += [source_macro('MODULE_SOURCE', source_path)]
        extra += [path_for(version, 'modules/A2FODirectionalShields/directional_shields.cpp')]
    elif kind == 'feature':
        macro = {'feature-odf': 'A2FO_STAGE8_ODF_SOURCE',
                 'feature-buildyard': 'A2FO_STAGE8_BUILDYARD_SOURCE',
                 'feature-queue': 'A2FO_STAGE8_QUEUE_SOURCE'}[name]
        macros += [source_macro(macro, source_path)]
    elif kind == 'feature-upgrade':
        macros += [source_macro('A2FO_STAGE8_UPGRADE_SOURCE', source_path)]
        extra += [path_for(version, 'modules/A2FOFeaturePack/upgrade_pod_config.cpp')]
    elif kind == 'policy':
        pass
    elif kind == 'bink-numeric':
        macros += [source_macro('MODULE_SOURCE', source_path)]
        extra += [path_for(version, 'modules/A2FOShellDisplay/native_intro.cpp'),
                  path_for(version, 'modules/A2FOShellDisplay/canvas.cpp'),
                  path_for(version, 'modules/A2FOShellDisplay/delphi_bridge.S')]
    elif kind == 'canvas-traversal':
        macros += [source_macro('MODULE_SOURCE', source_path)]
        extra += [path_for(version, 'modules/A2FOShellDisplay/delphi_bridge.S')]
    else:
        macros += [source_macro('MODULE_SOURCE', source_path)]
        if kind == 'texture':
            extra += [path_for(version, 'modules/A2FOTextureVariants/texture_variants.cpp')]
    if content != test.read_text():
        generated = work / f'{name}-{version}-fixture.cpp'
        generated.write_text(content)
        test = generated
    return test, extra, macros


def command_for(name, fixture, source, kind, version, flavor, archive, work):
    test, extra, macros = prepare(name, fixture, source, kind, version, work)
    exe = work / f'{name}-{flavor}-{version}.exe'
    flags = [*FLAGS, '-I', str(path_for(version, 'tests'))]
    if name == 'feature-buildyard':
        # Match its established GNU ABI fixture flags. GCC's extra pedantic
        # diagnostic rejects unchanged native thiscall function-pointer types.
        flags.remove('-Wpedantic')
    if name in ('rgb', 'texture-faction'):
        # Its fixture overrides new/delete for allocation-failure injection.
        # GCC diagnoses that established malloc/free pair after inlining.
        flags += ['-Wno-mismatched-new-delete']
    if name == 'texture-faction':
        # The same established native identity mock flag as its stage8 checker.
        flags += ['-Wno-array-bounds']
    if name == 'texture-visuals':
        # Match its existing full-source checker: the fixture intentionally
        # mocks native identities and overrides allocation for mutation tests.
        flags += ['-Wno-array-bounds', '-Wno-mismatched-new-delete']
    if kind == 'craft':
        flags += ['-Wl,--image-base,0x10000000']
    if flavor == 'builtin':
        flags += [f'-DA2FO_ModuleInit=A2FOBuiltin_{name.replace("-", "_")}_Init',
                  f'-DA2FO_ModuleShutdown=A2FOBuiltin_{name.replace("-", "_")}_Shutdown']
    command = [CXX, *flags, *macros, str(test), *(str(p) for p in extra),
               str(archive), *RUST_LIBS, *WIN_LIBS, '-o', str(exe)]
    return command, exe, test, extra


def run_logged(command, work, key, env=None, timeout=300, cwd=ROOT):
    stdout = work / f'{key}.stdout'
    stderr = work / f'{key}.stderr'
    with stdout.open('wb') as out, stderr.open('wb') as err:
        try:
            proc = subprocess.run(command, cwd=cwd, env=env, stdout=out,
                                  stderr=err, timeout=timeout, check=False)
            code = proc.returncode
        except subprocess.TimeoutExpired:
            code = 'timeout'
    result = {'command': command, 'cwd': str(cwd), 'returncode': code,
              'stdout': str(stdout), 'stderr': str(stderr),
              'stdout_sha256': sha(stdout), 'stderr_sha256': sha(stderr)}
    (work / f'{key}.json').write_text(json.dumps(result, indent=2) + '\n')
    return result


def dependencies(command, inputs, work, key):
    cxx = command[0]
    options = command[1:command.index(str(inputs[0]))]
    answer = []
    scans = []
    for index, unit in enumerate(inputs):
        if unit.suffix not in ('.cpp', '.cc', '.c', '.S'):
            answer.append(unit)
            continue
        dep_command = [cxx, *[x for x in options if x not in
                       ('-Wl,--gc-sections', '-Wl,--image-base,0x10000000')],
                       '-MM']
        if unit.suffix == '.S':
            dep_command += ['-x', 'assembler-with-cpp']
        dep_command.append(str(unit))
        result = run_logged(dep_command, work, f'{key}-dep-{index}', timeout=120)
        scans.append(result)
        if result['returncode'] != 0:
            raise RuntimeError(f'dependency scan failed: {key}/{index}; see {result["stderr"]}')
        content = Path(result['stdout']).read_text().replace('\\\n', ' ')
        if ':' not in content:
            raise RuntimeError(f'empty dependency output: {key}/{index}')
        answer.extend(Path(word).resolve() for word in
                      shlex.split(content.split(':', 1)[1]))
    return sorted(set(answer)), scans


def validate_trace(name, mode, stdout):
    """Require the existing fixture's own completion or state markers."""
    exact = {
        'craft-object-editor-policy': b'object editor policy: 34 branches PASS\n',
        'shell-bink-policy': b'bink policy: 22 branches PASS\n',
        'shell-canvas-policy': b'canvas policy: 33 branches PASS\n',
        'shell-canvas-lifecycle': b'Shell canvas lifecycle: PASS (synthetic USER32 host, no HWNDs)\n',
        'shell-canvas-dialog': b'shell native dialog nested input dispatch passed\n',
        'shell-canvas-scroll': b'shell canvas scroll, cached rows and dirty paint passed\n',
        'shell-canvas-copy': b'shell canvas copy export passed\n',
        'shell-native-intro': b'native intro memory-DIB fitting, scoped forwarding and fallback passed\n',
    }
    if name in exact:
        if stdout != exact[name]:
            raise RuntimeError(f'{name}: missing exact fixture completion marker')
    elif name == 'craft-object-editor':
        if stdout != f'object editor {mode}: PASS\n'.encode():
            raise RuntimeError(f'{name}/{mode}: missing exact fixture completion marker')
    elif name == 'directional':
        if mode.startswith('tree-'):
            if stdout != f'tree:{mode}:pass\n'.encode():
                raise RuntimeError(f'{name}/{mode}: missing tree completion marker')
        elif not stdout.strip() or len(stdout.splitlines()) < 1:
            raise RuntimeError(f'{name}/{mode}: empty directional trace')
    elif name == 'craft-module':
        markers = (b'hover:inactive-reset\n', b'render:aliased-name-reverse-restore\n',
                   b'hover:resampled-visibility-and-text-pointers\n',
                   b'sprite:retry-gate-and-missing-database\n',
                   b'sprite:icon-live-cache-slot\n', b'sprite:icon-crop-bounds\n',
                   b'sprite:icon-stale-quiet-then-retry\n',
                   b'sprite:bar-snapshot-and-width-before-colour\n',
                   b'sprite:bar-requested-fallback-and-foreground\n',
                   b'anchor:single-captain:', b'anchor:builder-captain:',
                   b'anchor:single-name-rebased:', b'anchor:builder-class-rebased:',
                   b'anchor:single-name-unconfigured:', b'anchor:hidden-captain-raw:',
                   b'anchor:mutation-read-order:', b'anchor:null-display\n',
                   b'join:empty-left:', b'join:empty-right:',
                   b'join:existing-separator:', b'join:insert-separator:')
        if any(marker not in stdout for marker in markers):
            raise RuntimeError('CraftIdentity module trace missing required state')
    elif name == 'feature-odf':
        if b'counters ' not in stdout or b'native ' not in stdout:
            raise RuntimeError('recursive ODF trace incomplete')
    elif name == 'feature-buildyard':
        if b'patch total ' not in stdout or b'match generated ' not in stdout:
            raise RuntimeError('BuildYard trace incomplete')
    elif name == 'feature-queue':
        if b'Queue workload export: unavailable, native, detached, deferred 25/3, and isolation PASS\n' not in stdout:
            raise RuntimeError('queue trace missing completion')
    elif name == 'feature-upgrade':
        if b'UpgradePods base fixture PASS\n' not in stdout:
            raise RuntimeError('upgrade trace missing completion')
    elif name == 'shell-bink-numeric':
        lines = stdout.splitlines()
        if (len(lines) != 278 or lines[0] != b'ready:stage10-bink-native' or
                lines[-1] != b'completed:stage10-bink-native:276' or
                sum(line.startswith(b'viewport:') for line in lines) != 144 or
                sum(line.startswith(b'rectangle:') for line in lines) != 60 or
                sum(line.startswith(b'dimensions:') for line in lines) != 72):
            raise RuntimeError('Bink numeric trace lacks 276 completed cases')
    elif name == 'shell-canvas-traversal':
        if stdout != (b'ready:canvas-traversal\n'
                      b'row:descendant-found-missing-self-bound64-bound65\n'
                      b'row:lease-reverse-scope-inactive-null\n'
                      b'row:map-source-native-target-zero-bypass-error\n'
                      b'completed:canvas-traversal\n'):
            raise RuntimeError('Canvas traversal trace missing ordered completion')
    elif name == 'rgb':
        if b'clear-state:' not in stdout or b'discovery-exception:' not in stdout:
            raise RuntimeError('RGB trace incomplete')
    elif name == 'texture-faction':
        if b'race-name:' not in stdout or b'node-flags:' not in stdout:
            raise RuntimeError('texture faction trace incomplete')
    elif name == 'texture-visuals':
        if ((b'policy:' not in stdout and b'shutdown-flags:' not in stdout) or
                (b'visual:' not in stdout and b'shutdown-maps:' not in stdout)):
            raise RuntimeError('texture visual trace incomplete')
    else:
        raise RuntimeError(f'no trace validator for {name}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--rust-lib', type=Path, default=CANDIDATE)
    parser.add_argument('--build-receipt', type=Path, required=True,
                        help='passed final batch build receipt with candidate archive hash')
    parser.add_argument('--wine-prefix', type=Path,
                        default=Path('/tmp/a2fo-shields-wine-prefix'))
    parser.add_argument('--build-only', action='store_true')
    parser.add_argument('--reuse-compiled-from', type=Path)
    args = parser.parse_args()
    work = args.work_dir.resolve()
    if work.exists() or work.parent != SLICE or not work.name.startswith('fixtures'):
        parser.error('--work-dir must be a fresh fixtures* child of the batch slice')
    candidate = args.rust_lib.resolve(strict=True)
    if candidate == ORIGINAL.resolve():
        parser.error('candidate archive must differ from original archive')
    if sha(ORIGINAL) != ORIGINAL_SHA:
        parser.error('original Rust archive drift')
    build_receipt = args.build_receipt.resolve(strict=True)
    build_data = json.loads(build_receipt.read_text())
    candidate_key = candidate.relative_to(ROOT).as_posix()
    if build_data.get('status') != 'passed' or \
            build_data.get('artifacts', {}).get(candidate_key) != sha(candidate):
        parser.error('final build receipt does not pin this candidate archive')
    build_inputs = build_data.get('inputs_after', {})
    provenance = [ROOT / 'Makefile', ROOT / 'rust/a2fo_core/Cargo.toml',
                  ROOT / 'rust/a2fo_core/Cargo.lock',
                  *ROOT.glob('rust/**/*.rs'), *ROOT.glob('rust/**/*.h')]
    missing_or_drifted = [path.relative_to(ROOT).as_posix() for path in provenance
                          if build_inputs.get(path.relative_to(ROOT).as_posix()) != sha(path)]
    if missing_or_drifted:
        parser.error('final build receipt lacks matching Rust/build inputs: ' +
                     ', '.join(missing_or_drifted[:8]))
    manifest = json.loads((SLICE / 'before-hashes.json').read_text())
    if not isinstance(manifest, dict):
        parser.error('unexpected frozen manifest format')
    work.mkdir(parents=True)
    receipt = {'status': 'preparing', 'original_archive': str(ORIGINAL),
               'candidate_archive': str(candidate), 'cases': {}, 'inputs': {},
               'dependency_scans': {}, 'build_only': args.build_only,
               'assertions_enabled': True,
               'build_receipt': str(build_receipt),
               'build_receipt_sha256': sha(build_receipt),
               'expected_trace_pairs': sum(len(modes or (None,)) * len(flavors(kind))
                                           for _, _, _, modes, kind in suite()),
               'trace_pairs': 0, 'executions': 0, 'stdout_lines': 0}

    def save():
        (work / 'receipt.json').write_text(json.dumps(receipt, indent=2,
                                                       sort_keys=True) + '\n')

    reuse = None
    reuse_work = None
    if args.reuse_compiled_from:
        reuse_work = args.reuse_compiled_from.resolve(strict=True)
        if reuse_work.parent != SLICE or reuse_work == work:
            parser.error('compile reuse requires a prior sibling fixture attempt')
        reuse_path = reuse_work / 'receipt.json'
        reuse = json.loads(reuse_path.read_text())
        assert reuse['build_receipt_sha256'] == sha(build_receipt)
        assert reuse['original_archive'] == str(ORIGINAL)
        assert reuse['candidate_archive'] == str(candidate)
        held = {}
        for name, expected in reuse['inputs'].items():
            item = Path(name)
            if item == Path(__file__).resolve():
                item = reuse_work / 'source-used' / item.relative_to(ROOT)
            if sha(item) != expected:
                raise RuntimeError(f'prior compile input drift: {item}')
            held[str(item)] = expected
        receipt['compile_reuse'] = {'receipt': str(reuse_path),
            'receipt_sha256': sha(reuse_path), 'held_inputs': held,
            'rule': 'Reuse successful compiles only with identical argv after work-directory normalization and all original source/log/exe hashes held'}

    try:
        plans = []
        paths = {ORIGINAL, candidate, build_receipt, Path(__file__).resolve(),
                 SLICE / 'before-hashes.json', ROOT / 'Makefile',
                 ROOT / 'rust/a2fo_core/Cargo.toml',
                 ROOT / 'rust/a2fo_core/Cargo.lock'}
        if reuse is not None:
            paths.add(reuse_work / 'receipt.json')
            paths.update(Path(name) for name in receipt['compile_reuse']['held_inputs'])
            for case in reuse['cases'].values():
                if case['compile']['returncode'] == 0 and 'exe_sha256' in case:
                    paths.add(Path(case['exe']))
                    paths.update(Path(case['compile'][c]) for c in ('stdout', 'stderr'))
        paths.update(ROOT.glob('rust/**/*.rs'))
        paths.update(ROOT.glob('rust/**/*.h'))
        for config in (ROOT / '.cargo/config.toml', ROOT / '.cargo/config'):
            if config.is_file():
                paths.add(config)
        for name, fixture, source, modes, kind in suite():
            for flavor in flavors(kind):
                for version, archive in (('before', ORIGINAL), ('current', candidate)):
                    key = f'{name}-{flavor}-{version}'
                    command, exe, test, extra = command_for(
                        name, fixture, source, kind, version, flavor, archive, work)
                    if test.is_relative_to(work):
                        paths.add(test)
                    if kind == 'craft':
                        paths.add(work / f'{name}-{version}-base.cpp')
                    original_test = fixture_for(version, fixture)
                    paths.add(original_test)
                    if (version == 'before' and fixture not in SHARED_FIXTURES and
                            manifest.get(fixture) != sha(original_test)):
                        raise RuntimeError(f'frozen fixture lacks matching pin: {fixture}')
                    if kind == 'craft':
                        base = path_for(version, 'tests/craft_identity_ammunition_hover_test.cpp')
                        paths.add(base)
                        if version == 'before' and manifest.get(base.relative_to(BEFORE).as_posix()) != sha(base):
                            raise RuntimeError('frozen CraftIdentity base fixture drift')
                    units = [test, *extra]
                    deps, scans = dependencies(command, units, work, key)
                    receipt['dependency_scans'][key] = [str(p) for p in deps]
                    receipt.setdefault('dependency_commands', {})[key] = scans
                    for path in deps:
                        if path.is_relative_to(work):
                            continue
                        if version == 'before' and path.is_relative_to(ROOT):
                            if not path.is_relative_to(BEFORE):
                                if path != original_test or fixture not in SHARED_FIXTURES:
                                    raise RuntimeError(f'frozen dependency escaped snapshot: {path}')
                            else:
                                logical = path.relative_to(BEFORE).as_posix()
                                if manifest.get(logical) != sha(path):
                                    raise RuntimeError(f'frozen dependency lacks matching pin: {logical}')
                        paths.add(path)
                    plans.append((key, name, modes, kind, flavor, version, command, exe))
        if any(not p.is_file() for p in paths):
            raise RuntimeError('one or more dependency inputs missing')
        initial = {str(path): sha(path) for path in sorted(paths)}
        receipt['inputs'] = initial
        def verify_inputs():
            changed = [str(path) for path in paths if sha(path) != initial[str(path)]]
            if changed:
                raise RuntimeError('input drift: ' + ', '.join(sorted(changed)))
        for path in sorted(paths):
            if path.is_relative_to(ROOT):
                target = work / 'source-used' / path.relative_to(ROOT)
            else:
                target = work / 'source-used/external' / sha(path) / path.name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            if sha(target) != initial[str(path)]:
                raise RuntimeError(f'source-used snapshot mismatch: {path}')
        save()
        for key, name, modes, kind, flavor, version, command, exe in plans:
            result = None
            old_case = reuse['cases'].get(key) if reuse is not None else None
            if old_case and old_case['compile']['returncode'] == 0 and 'exe_sha256' in old_case:
                normalize = lambda argv, directory: [value.replace(str(directory), '<WORK>') for value in argv]
                old_result = old_case['compile']
                if normalize(old_result['command'], reuse_work) == normalize(command, work):
                    # Generated inputs must match exactly after path normalization,
                    # in addition to all original frozen/current inputs held above.
                    equivalent = True
                    for item in paths:
                        if item.is_relative_to(work) and item.suffix == '.cpp':
                            previous = reuse_work / item.relative_to(work)
                            if not previous.is_file() or item.read_bytes().replace(str(work).encode(), b'<WORK>') != previous.read_bytes().replace(str(reuse_work).encode(), b'<WORK>'):
                                equivalent = False
                                break
                    if equivalent:
                        if sha(Path(old_case['exe'])) != old_case['exe_sha256']:
                            raise RuntimeError(f'prior executable drift: {key}')
                        for channel in ('stdout', 'stderr'):
                            if sha(Path(old_result[channel])) != old_result[f'{channel}_sha256']:
                                raise RuntimeError(f'prior compile log drift: {key}')
                        shutil.copy2(old_case['exe'], exe)
                        result = dict(old_result)
                        result.update(reused_from_receipt=str(reuse_work / 'receipt.json'),
                                      requested_equivalent_command=command,
                                      original_executable=old_case['exe'],
                                      original_executable_sha256=old_case['exe_sha256'])
                        for channel in ('stdout', 'stderr'):
                            destination = work / f'{key}-compile.{channel}'
                            shutil.copy2(old_result[channel], destination)
                            result[channel] = str(destination)
                        (work / f'{key}-compile.json').write_text(json.dumps(result, indent=2) + '\n')
            if result is None:
                result = run_logged(command, work, f'{key}-compile', timeout=360)
            receipt['cases'][key] = {'compile': result, 'exe': str(exe)}
            save()
            if result['returncode'] != 0:
                raise RuntimeError(f'compile failed: {key}; see {result["stderr"]}')
            receipt['cases'][key]['exe_sha256'] = sha(exe)
            verify_inputs()
        receipt['status'] = 'built'
        save()
        if args.build_only:
            return
        env = os.environ.copy()
        env.pop('DISPLAY', None)
        env.pop('WAYLAND_DISPLAY', None)
        env.update(WINEPREFIX=str(args.wine_prefix.resolve()), WINEDEBUG='-all',
                   WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;'
                   'winewayland.drv=d;winex11.drv=d', LIBGL_ALWAYS_SOFTWARE='1')
        for name, _, _, modes, kind in suite():
            for flavor in flavors(kind):
                traces = {}
                for version in ('before', 'current'):
                    exe = work / f'{name}-{flavor}-{version}.exe'
                    case_key = f'{name}-{flavor}-{version}'
                    for mode in modes or (None,):
                        verify_inputs()
                        if sha(exe) != receipt['cases'][case_key]['exe_sha256']:
                            raise RuntimeError(f'executable drift before run: {case_key}')
                        key = case_key + (f'-{mode}' if mode else '')
                        argv = ['wine', str(exe)] + ([mode] if mode else [])
                        # A fresh directory at the same absolute name for each
                        # side preserves path traces without sharing generated assets.
                        runtime = work / 'runtime' / f'{name}-{flavor}' / (mode or 'default')
                        runtime.mkdir(parents=True, exist_ok=False)
                        result = run_logged(argv, work, key, env=env, timeout=180, cwd=runtime)
                        captured = work / 'runtime-captured' / f'{name}-{flavor}' / (mode or 'default') / version
                        captured.parent.mkdir(parents=True, exist_ok=True)
                        runtime.rename(captured)
                        receipt.setdefault('runtime_files', {}).update({
                            str(path.relative_to(work)): sha(path)
                            for path in captured.rglob('*') if path.is_file()})
                        receipt['cases'][case_key].setdefault('runs', {})[mode or 'default'] = result
                        receipt['executions'] += 1
                        save()
                        if result['returncode'] != 0:
                            raise RuntimeError(f'native run failed: {key}; see {result["stderr"]}')
                        trace = Path(result['stdout']).read_bytes().replace(b'\r\n', b'\n')
                        errors = Path(result['stderr']).read_bytes().replace(b'\r\n', b'\n')
                        validate_trace(name, mode, trace)
                        traces[(version, mode)] = (result['returncode'], trace, errors)
                        if version == 'current':
                            receipt['stdout_lines'] += len(trace.splitlines())
                for mode in modes or (None,):
                    if traces[('before', mode)] != traces[('current', mode)]:
                        raise RuntimeError(f'native trace mismatch: {name}/{flavor}/{mode or "default"}')
                    receipt['trace_pairs'] += 1
                    save()
        verify_inputs()
        receipt['status'] = 'pass'
        save()
    except Exception as exc:
        receipt['status'] = 'failed'
        receipt['error'] = str(exc)
        save()
        raise


if __name__ == '__main__':
    main()
