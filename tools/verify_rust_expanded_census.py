#!/usr/bin/env python3
"""Validate the frozen expanded implementation census and its linked Rust evidence."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from run_rust_stage7_parity import GROUPS

EXPECTED_GROUPS = {group[0] for group in GROUPS}
A1_UNIT = 'modules/A1Compat/module.cpp'
A1_BUILDER = ROOT / 'build/rust-migration-stage9/a1-final-fixtures/build_final.py'
A1_GROUP_NAMES = (
    'assets-v4', 'physics-v2', 'class', 'race-v2', 'geometry-v3',
    'layout-v2', 'gui-v5', 'numeric', 'speed', 'quarters', 'officer',
    'bzn-v2', 'sod', 'ui-numeric-v3', 'officer-workflow-v3', 'fofs-v2',
    'race-loaded-v1', 'crew-v3', 'officer-ui-v2', 'display-text-v1',
    'display-numeric-v3', 'gui-scale-v4', 'node-v3', 'aip-name-v1',
    'native-ui-v1', 'resource-limits-v2', 'state-policy-v1',
    'render-policy-v2',
)
A1_QUIET_GROUPS = frozenset({'geometry-v3', 'layout-v2'})
A1_MODULE_NAMES = (
    'stage8_a1', 'stage8_a1_lifecycle', 'stage8_a1_shutdown',
    'stage8_a1_relationships', 'stage8_a1_maps_runtime',
    'stage8_a1_physics', 'stage8_a1_officer',
    'stage9_a1_config', 'stage9_a1_mission_scan', 'stage9_a1_ui',
    'stage9_a1_assets', 'stage9_a1_bzn_runtime',
    'stage9_a1_class_defaults', 'stage9_a1_geometry',
    'stage9_a1_gui_config', 'stage9_a1_layout', 'stage9_a1_numeric',
    'stage9_a1_officer_policy', 'stage9_a1_physics_defaults',
    'stage9_a1_quarters', 'stage9_a1_race', 'stage9_a1_reporting',
    'stage9_a1_sod', 'stage9_a1_speed', 'stage9_a1_aip_name',
    'stage9_a1_crew', 'stage9_a1_display_numeric',
    'stage9_a1_display_text', 'stage9_a1_fofs', 'stage9_a1_gui_scale',
    'stage9_a1_native_ui', 'stage9_a1_node_policy',
    'stage9_a1_officer_ui', 'stage9_a1_officer_workflow',
    'stage9_a1_race_workflow', 'stage9_a1_render_policy',
    'stage9_a1_resource_limits', 'stage9_a1_state_policy',
    'stage9_a1_ui_numeric',
)
A1_REQUIRED_RUST_SOURCES = {
    f'rust/a2fo_core/src/{name}.rs' for name in A1_MODULE_NAMES
}
STAGE9_GROUPS = {
    'sol_runtime': {
        'unit': 'modules/A2FOSOLMaps/module.cpp',
        'rust_sources': {
            'rust/a2fo_core/src/stage8_c_solmaps.rs',
            'rust/a2fo_core/src/stage9_sol_runtime.rs',
            'rust/a2fo_core/src/stage9_sol_numeric.rs',
            'rust/a2fo_core/src/stage9_sol_discovery.rs',
            'rust/a2fo_core/src/stage9_sol_completion.rs',
        },
    },
    'a1_completion': {
        'unit': A1_UNIT,
        'rust_sources': A1_REQUIRED_RUST_SOURCES,
    },
}


def verify_parity_receipt(path):
    final = json.loads(path.read_text())
    results = final.get('results', [])
    names = [entry.get('group') for entry in results]
    if (len(EXPECTED_GROUPS) != 30 or final.get('status') != 'passed' or
            len(results) != 30 or len(set(names)) != 30 or
            set(names) != EXPECTED_GROUPS or
            len(final.get('selected_groups', [])) != 30 or
            set(final['selected_groups']) != EXPECTED_GROUPS):
        raise SystemExit(f'FAIL: incomplete 30-group native parity: {path}')
    for entry in results:
        if (entry.get('status') != 'passed' or entry.get('exit_code') != 0 or
                entry.get('changed_inputs') != [] or
                entry.get('child_receipt_missing') is not False):
            raise SystemExit(f"FAIL: native parity group did not pass: {entry.get('group')}")
    inputs = final.get('initial_sha256', {})
    if not inputs:
        raise SystemExit(f'FAIL: native parity input hashes missing: {path}')
    for name, expected in inputs.items():
        source = Path(name)
        if not source.is_absolute():
            source = ROOT / source
        if (not source.is_file() or
                hashlib.sha256(source.read_bytes()).hexdigest() != expected):
            raise SystemExit(f'FAIL: native parity input changed: {name}')
    return set(names), inputs


def resolve_input(name):
    path = Path(name)
    return path if path.is_absolute() else ROOT / path


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_a1_registry():
    if len(A1_GROUP_NAMES) != 28 or len(set(A1_GROUP_NAMES)) != 28:
        raise SystemExit('FAIL: A1 completion contract list is incomplete')
    builder_names = tuple(re.findall(
        r"^    Contract\('([^']+)'", A1_BUILDER.read_text(), re.MULTILINE))
    if builder_names != A1_GROUP_NAMES:
        raise SystemExit('FAIL: A1 completion groups differ from fixture builder')
    registered = set(re.findall(
        r'^mod ((?:stage8|stage9)_a1\w*);$',
        (ROOT / 'rust/a2fo_core/src/lib.rs').read_text(), re.MULTILINE))
    if len(A1_MODULE_NAMES) != 39 or registered != set(A1_MODULE_NAMES):
        raise SystemExit('FAIL: A1 Rust module registry differs from reviewed set')
    for name in A1_REQUIRED_RUST_SOURCES:
        if not (ROOT / name).is_file():
            raise SystemExit(f'FAIL: registered A1 Rust source missing: {name}')


def verify_a1_build_receipt(path, archive, current_source):
    build = json.loads(path.read_text())
    if (build.get('status') != 'build-only; no native fixture execution' or
            build.get('built_groups') != list(A1_GROUP_NAMES) or
            build.get('deferred_groups') != {} or
            build.get('standalone_staged_rust_objects_linked') is not False):
        raise SystemExit(f'FAIL: incomplete 28-group A1 full-source build: {path}')
    frozen = ROOT / 'build/rust-migration-stage8/before/modules/A1Compat/module.cpp'
    frozen_archive = ROOT / 'build/rust-migration-stage9/checkpoint-archive9/liba2fo_core-win.a'
    frozen_validation = frozen_archive.parent / 'validation.json'
    frozen_archive_sha = '9b09f732f8c30c15f0b2da72b6b1b96789df456d85f72658826e0a6d7f928bcf'
    if (build.get('frozen_current_rust_archive_linked') is not False or
            build.get('current_rust_archive_linked') is not True or
            build.get('frozen_rust_archive_linked') is not True or
            resolve_input(build.get('frozen_rust_archive', '')).resolve() != frozen_archive.resolve() or
            build.get('frozen_rust_archive_sha256') != frozen_archive_sha or
            sha256(frozen_archive) != frozen_archive_sha or
            json.loads(frozen_validation.read_text()).get('artifacts', {}).get('liba2fo_core-win.a') != frozen_archive_sha):
        raise SystemExit(f'FAIL: A1 frozen dependency is not the archived independent library: {path}')
    if (build.get('frozen_source_sha256') !=
            '223b70ae54f38202e8c623702a836708abe9f7c4c9c67ab31927951b812e2bdc' or
            resolve_input(build.get('frozen_source', '')).resolve() != frozen.resolve() or
            sha256(frozen) != build['frozen_source_sha256'] or
            resolve_input(build.get('current_source', '')).resolve() != current_source.resolve() or
            build.get('current_source_sha256') != sha256(current_source) or
            resolve_input(build.get('archive', '')).resolve() != archive.resolve() or
            build.get('archive_sha256') != sha256(archive)):
        raise SystemExit(f'FAIL: A1 source/archive build identity differs: {path}')
    before = build.get('inputs_sha256_before')
    after = build.get('inputs_sha256_after')
    if not isinstance(before, dict) or not before or before != after:
        raise SystemExit(f'FAIL: A1 build input pins missing or changed: {path}')
    pinned = {}
    for name, expected in before.items():
        source = resolve_input(name).resolve()
        if not source.is_file() or sha256(source) != expected:
            raise SystemExit(f'FAIL: A1 build input changed: {name}')
        pinned[str(source)] = expected
    frozen_module_dir = frozen.parent
    current_module_dir = current_source.parent
    native_sources = [
        *(p for p in frozen_module_dir.iterdir()
          if p.is_file() and p.suffix in {'.cpp', '.hpp', '.h', '.S'}),
        *(p for p in current_module_dir.iterdir()
          if p.is_file() and p.suffix in {'.hpp', '.h'}),
        current_module_dir / 'thiscall_bridge.S',
    ]
    for required in [frozen, current_source, archive, frozen_archive, frozen_validation, A1_BUILDER,
                     *native_sources,
                     *(ROOT / name for name in A1_REQUIRED_RUST_SOURCES)]:
        source = required.resolve()
        if pinned.get(str(source)) != sha256(source):
            raise SystemExit(f'FAIL: A1 build omits required input: {source}')
    artifacts = build.get('artifacts_sha256')
    generated = build.get('generated_module_source_sha256')
    if not isinstance(artifacts, dict) or not isinstance(generated, dict) or \
            set(generated) != set(A1_GROUP_NAMES):
        raise SystemExit(f'FAIL: A1 full-source artifacts incomplete: {path}')
    expected_exes = {f'{group}/{label}.exe' for group in A1_GROUP_NAMES
                     for label in ('frozen', 'current')}
    if {name for name in artifacts if name.endswith('.exe')} != expected_exes:
        raise SystemExit(f'FAIL: A1 full-source executable set differs: {path}')
    source_paths = [current_source]
    source_paths.extend(p for p in (ROOT / 'modules/A1Compat').iterdir()
                        if p.is_file() and p.suffix in {'.cpp', '.hpp', '.h'})
    required_exports = set()
    for source in source_paths:
        required_exports.update(re.findall(
            r'\b(a2fo_rs_stage9_a1_[A-Za-z0-9_]+)\s*\(', source.read_text()))
    if (not isinstance(build.get('required_current_stage9_a1_exports'), list) or
            set(build['required_current_stage9_a1_exports']) != required_exports):
        raise SystemExit(f'FAIL: A1 build required-export list differs: {path}')
    for name, expected in artifacts.items():
        artifact = path.parent / name
        if not artifact.is_file() or sha256(artifact) != expected:
            raise SystemExit(f'FAIL: A1 build artifact missing or changed: {artifact}')
    for group in A1_GROUP_NAMES:
        pair = generated.get(group)
        if not isinstance(pair, dict) or set(pair) != {'frozen', 'current'}:
            raise SystemExit(f'FAIL: A1 generated-source pair missing: {group}')
        for label in ('frozen', 'current'):
            name = f'{group}/{label}-module.cpp'
            exe = f'{group}/{label}.exe'
            if (not isinstance(pair[label], dict) or
                    pair[label].get('sha256') != artifacts.get(name) or
                    exe not in artifacts):
                raise SystemExit(f'FAIL: A1 fixture pair incomplete: {group}/{label}')
    return build


def verify_a1_completion_receipt(path, final, unit, before):
    verify_a1_registry()
    if final.get('groups_selected') != list(A1_GROUP_NAMES):
        raise SystemExit(f'FAIL: incomplete A1 native group selection: {path}')
    archive = resolve_input(final.get('rust_archive', '')).resolve()
    current_source = resolve_input(unit['path']).resolve()
    pinned = {str(resolve_input(name).resolve()): expected
              for name, expected in before.items()}
    for required in [current_source, archive, A1_BUILDER,
                     *(ROOT / name for name in A1_REQUIRED_RUST_SOURCES)]:
        source = required.resolve()
        if not source.is_file() or pinned.get(str(source)) != sha256(source):
            raise SystemExit(f'FAIL: A1 native receipt omits input: {source}')
    build_path = resolve_input(final.get('build_receipt', '')).resolve()
    if (not build_path.is_file() or
            final.get('build_receipt_sha256') != sha256(build_path) or
            pinned.get(str(build_path)) != sha256(build_path)):
        raise SystemExit(f'FAIL: A1 build receipt missing or unpinned: {path}')
    build = verify_a1_build_receipt(build_path, archive, current_source)
    frozen = final.get('frozen_results')
    current = final.get('current_results')
    if (not isinstance(frozen, dict) or not isinstance(current, dict) or
            set(frozen) != {'a1_completion'} or set(current) != {'a1_completion'} or
            not isinstance(frozen['a1_completion'], dict) or
            not isinstance(current['a1_completion'], dict)):
        raise SystemExit(f'FAIL: A1 full-unit semantic result missing: {path}')
    frozen_groups = frozen['a1_completion'].get('groups')
    current_groups = current['a1_completion'].get('groups')
    if (not isinstance(frozen_groups, dict) or
            not isinstance(current_groups, dict) or
            set(frozen_groups) != set(A1_GROUP_NAMES) or
            frozen_groups != current_groups):
        raise SystemExit(f'FAIL: A1 28-group semantic results incomplete: {path}')
    steps = final.get('steps')
    if not isinstance(steps, list) or len(steps) != 56:
        raise SystemExit(f'FAIL: A1 native step count differs from 56: {path}')
    expected_order = [(group, label) for group in A1_GROUP_NAMES
                      for label in ('frozen', 'current')]
    if [(step.get('group'), step.get('label')) if isinstance(step, dict)
        else (None, None) for step in steps] != expected_order:
        raise SystemExit(f'FAIL: A1 native step order differs from builder: {path}')
    expected_pairs = {(group, label) for group in A1_GROUP_NAMES
                      for label in ('frozen', 'current')}
    seen = set()
    for step in steps:
        if not isinstance(step, dict):
            raise SystemExit(f'FAIL: malformed A1 native step: {path}')
        group, label = step.get('group'), step.get('label')
        pair = (group, label)
        if pair not in expected_pairs or pair in seen or step.get('exit_code') != 0:
            raise SystemExit(f'FAIL: failed or duplicate A1 native step: {pair}')
        seen.add(pair)
        exe = (build_path.parent / group / f'{label}.exe').resolve()
        if (resolve_input(step.get('executable', '')).resolve() != exe or
                step.get('executable_sha256') != sha256(exe) or
                pinned.get(str(exe)) != sha256(exe) or
                build['artifacts_sha256'].get(f'{group}/{label}.exe') != sha256(exe)):
            raise SystemExit(f'FAIL: A1 step executable differs from build: {pair}')
        for field in ('stdout', 'stderr', 'normalized_stdout',
                      'normalized_stderr'):
            output = resolve_input(step.get(f'{field}_path', ''))
            if (not output.is_file() or
                    step.get(f'{field}_sha256') != sha256(output)):
                raise SystemExit(f'FAIL: A1 step output missing or changed: {pair}/{field}')
        if (step['stderr_sha256'] != step['normalized_stderr_sha256'] or
                not isinstance(step.get('line_count'), int) or
                (step['line_count'] == 0 and group not in A1_QUIET_GROUPS) or
                step['line_count'] < 0 or
                step['line_count'] != len(resolve_input(
                    step['normalized_stdout_path']).read_bytes().splitlines())):
            raise SystemExit(f'FAIL: A1 step stderr or line count differs: {pair}')
        semantic = frozen_groups[group]
        if (not isinstance(semantic, dict) or
                set(semantic) != {'normalized_stdout_sha256',
                                  'stderr_sha256', 'line_count'} or
                semantic['normalized_stdout_sha256'] !=
                    step['normalized_stdout_sha256'] or
                semantic['stderr_sha256'] != step['stderr_sha256'] or
                semantic['line_count'] != step['line_count']):
            raise SystemExit(f'FAIL: A1 group result differs from step: {pair}')
    if seen != expected_pairs:
        raise SystemExit(f'FAIL: A1 native steps omit a fixture pair: {path}')


def verify_stage9_parity_receipt(path, unit):
    final = json.loads(path.read_text())
    if (final.get('status') != 'passed' or final.get('build_only') is not False or
            final.get('changed_pins') != [] or final.get('unit') != unit['path']):
        raise SystemExit(f'FAIL: incomplete stage9 native parity receipt: {path}')
    before = final.get('pins_before')
    after = final.get('pins_after')
    if not isinstance(before, dict) or not before or before != after:
        raise SystemExit(f'FAIL: stage9 parity pins missing or changed: {path}')
    for name, expected in before.items():
        source = resolve_input(name)
        if (not source.is_file() or
                hashlib.sha256(source.read_bytes()).hexdigest() != expected):
            raise SystemExit(f'FAIL: stage9 parity input changed: {name}')
    frozen = final.get('frozen_results')
    current = final.get('current_results')
    if not isinstance(frozen, dict) or not frozen or frozen != current:
        raise SystemExit(f'FAIL: stage9 frozen/current semantic results differ: {path}')
    if unit['path'] == A1_UNIT:
        verify_a1_completion_receipt(path, final, unit, before)
    inputs = {str(resolve_input(name).resolve()): expected for name, expected in before.items()}
    return inputs


def verify_stage9_boundary_receipt(path, dll):
    boundary = json.loads(path.read_text())
    baseline = boundary.get('baseline')
    migrated = boundary.get('migrated')
    if (boundary.get('export_names_and_ordinals_equal') is not True or
            boundary.get('rust_implementations_linked') is not True or
            not isinstance(boundary.get('new_imports'), list) or
            not isinstance(baseline, dict) or
            not isinstance(migrated, dict) or
            baseline.get('exports') != migrated.get('exports')):
        raise SystemExit(f'FAIL: stage9 DLL boundary contract failed: {path}')
    baseline_imports = baseline.get('imports')
    migrated_imports = migrated.get('imports')
    if not isinstance(baseline_imports, list) or not isinstance(migrated_imports, list):
        raise SystemExit(f'FAIL: stage9 DLL boundary imports missing: {path}')
    imports_delta = sorted(set(migrated_imports) - set(baseline_imports))
    if imports_delta != boundary['new_imports']:
        raise SystemExit(f'FAIL: stage9 DLL boundary import delta is inconsistent: {path}')
    actual_sha = sha256(dll)
    if migrated.get('sha256') != actual_sha:
        raise SystemExit(f'FAIL: stage9 DLL boundary receipt does not pin --dll: {path}')


def verify_stage9_link_receipt(path, dll, child_receipt, required_inputs):
    link = json.loads(path.read_text())
    before = link.get('pins_before')
    after = link.get('pins_after')
    if (link.get('status') != 'passed' or link.get('exit_code') != 0 or
            link.get('changed_pins') != [] or not isinstance(before, dict) or
            not before or before != after):
        raise SystemExit(f'FAIL: incomplete stage9 DLL link receipt: {path}')
    pins = {}
    for name, expected in before.items():
        source = resolve_input(name)
        if not source.is_file() or sha256(source) != expected:
            raise SystemExit(f'FAIL: stage9 DLL link input changed: {name}')
        pins[str(source.resolve())] = expected

    link_dll = resolve_input(link.get('dll', ''))
    archive = resolve_input(link.get('rust_archive', ''))
    if not link_dll.is_file() or link_dll.resolve() != dll.resolve():
        raise SystemExit(f'FAIL: stage9 DLL link receipt does not identify --dll: {path}')
    if link.get('dll_sha256') != sha256(dll):
        raise SystemExit(f'FAIL: stage9 DLL link receipt does not pin --dll: {path}')
    if not archive.is_file() or link.get('rust_archive_sha256') != sha256(archive):
        raise SystemExit(f'FAIL: stage9 canonical Rust archive hash mismatch: {path}')
    child_archive = child_receipt.get('rust_archive')
    if not child_archive or resolve_input(child_archive).resolve() != archive.resolve():
        raise SystemExit(f'FAIL: stage9 child parity uses a different Rust archive: {path}')
    for name in required_inputs:
        source = resolve_input(name).resolve()
        if not source.is_file() or pins.get(str(source)) != sha256(source):
            raise SystemExit(f'FAIL: stage9 link receipt omits or mismatches input: {name}')
    if pins.get(str(archive.resolve())) != sha256(archive):
        raise SystemExit('FAIL: stage9 link receipt does not pin canonical Rust archive')

    command = link.get('command')
    target_rel = dll.resolve().relative_to(ROOT).as_posix()
    if (not isinstance(command, list) or not command or
            not all(isinstance(token, str) for token in command) or
            Path(command[0]).name not in {'make', 'gmake'} or
            (target_rel not in command and str(dll.resolve()) not in command)):
        raise SystemExit(f'FAIL: stage9 link command does not invoke make for --dll: {path}')

    log_path = resolve_input(link.get('build_log', ''))
    if not log_path.is_file() or link.get('build_log_sha256') != sha256(log_path):
        raise SystemExit(f'FAIL: stage9 DLL link log missing or changed: {path}')
    log_text = log_path.read_text(errors='replace').replace('\\\n', ' ')
    archive_paths = {str(archive.resolve()), archive.resolve().relative_to(ROOT).as_posix()}
    found_link_command = False
    for line in log_text.splitlines():
        tokens = line.split()
        if not tokens or 'g++' not in Path(tokens[0]).name:
            continue
        try:
            output = tokens[tokens.index('-o') + 1]
        except (ValueError, IndexError):
            continue
        if (resolve_input(output).resolve() == dll.resolve() and
                any(token in archive_paths for token in tokens)):
            found_link_command = True
            break
    if not found_link_command:
        raise SystemExit(f'FAIL: stage9 build log lacks actual DLL/archive link command: {path}')


def verify_stage9_unit(unit, dll, receipt_cache):
    receipts = unit.get('parity_receipts', [])
    groups = unit.get('parity_groups', [])
    if (not isinstance(receipts, list) or not receipts or
            not isinstance(groups, list) or not groups or len(set(groups)) != len(groups) or
            not all(isinstance(group, str) and group for group in groups)):
        raise SystemExit(f"FAIL: missing stage9 parity receipts/groups: {unit['path']}")
    for group in groups:
        spec = STAGE9_GROUPS.get(group)
        if spec is None or spec['unit'] != unit['path']:
            raise SystemExit(f"FAIL: unrecognized stage9 parity group for {unit['path']}: {group}")
    expected_groups = {name for name, spec in STAGE9_GROUPS.items()
                       if spec['unit'] == unit['path']}
    if set(groups) != expected_groups:
        raise SystemExit(f"FAIL: incomplete stage9 parity groups: {unit['path']}")
    rust_sources = unit.get('rust_sources') or [unit.get('rust_source')]
    primary_source = unit.get('rust_source')
    if (not isinstance(rust_sources, list) or not rust_sources or
            any(not isinstance(source, str) or not source
                                for source in rust_sources) or
            len(set(rust_sources)) != len(rust_sources) or
            primary_source not in rust_sources or
            not STAGE9_GROUPS[groups[0]]['rust_sources'] <= set(rust_sources)):
        raise SystemExit(f"FAIL: incomplete stage9 Rust source binding: {unit['path']}")
    verified_inputs = []
    child_receipts = []
    for evidence in receipts:
        receipt_path = resolve_input(evidence)
        if not receipt_path.is_file():
            raise SystemExit(f'FAIL: missing stage9 parity receipt: {evidence}')
        cache_key = ('stage9', unit['path'], receipt_path)
        if cache_key not in receipt_cache:
            receipt_cache[cache_key] = verify_stage9_parity_receipt(receipt_path, unit)
        verified_inputs.append(receipt_cache[cache_key])
        child_receipts.append(json.loads(receipt_path.read_text()))
    sources = [unit['path'], *rust_sources]
    for name in sources:
        source = resolve_input(name)
        if not source.is_file():
            raise SystemExit(f'FAIL: missing stage9 source: {name}')
        actual = hashlib.sha256(source.read_bytes()).hexdigest()
        if any(inputs.get(str(source.resolve())) != actual for inputs in verified_inputs):
            raise SystemExit(f'FAIL: source differs from stage9 native parity inputs: {name}')
    cpp = resolve_input(unit['path'])
    cpp_sha = hashlib.sha256(cpp.read_bytes()).hexdigest()
    if cpp_sha != unit.get('verified_cpp_sha256'):
        raise SystemExit(f"FAIL: source changed after verification: {unit['path']}")

    rust_source_paths = [resolve_input(name).resolve() for name in rust_sources]
    cpp_path = cpp.resolve()
    child_archive = None
    for child, inputs in zip(child_receipts, verified_inputs):
        archive_name = child.get('rust_archive')
        if not archive_name:
            raise SystemExit(f"FAIL: stage9 child parity archive missing: {unit['path']}")
        archive = resolve_input(archive_name).resolve()
        if child_archive is not None and archive != child_archive:
            raise SystemExit(f"FAIL: stage9 parity receipts use different archives: {unit['path']}")
        child_archive = archive
        for required in [cpp_path, *rust_source_paths, archive]:
            actual = sha256(required) if required.is_file() else None
            if actual is None or inputs.get(str(required)) != actual:
                raise SystemExit(f'FAIL: stage9 parity receipt omits or mismatches input: {required}')

    link_evidence = unit.get('link_receipt')
    if not link_evidence:
        raise SystemExit(f"FAIL: missing stage9 DLL link receipt: {unit['path']}")
    link_path = resolve_input(link_evidence)
    if not link_path.is_file():
        raise SystemExit(f'FAIL: missing stage9 DLL link receipt: {link_evidence}')
    for child in child_receipts:
        verify_stage9_link_receipt(
            link_path, dll, child, [unit['path'], *rust_sources])

    boundary_receipt = unit.get('dll_boundary_receipt')
    if not boundary_receipt:
        raise SystemExit(f"FAIL: missing stage9 DLL boundary receipt: {unit['path']}")
    boundary_path = resolve_input(boundary_receipt)
    if not boundary_path.is_file():
        raise SystemExit(f'FAIL: missing stage9 DLL boundary receipt: {boundary_receipt}')
    verify_stage9_boundary_receipt(boundary_path, dll)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inventory', type=Path, default=ROOT / 'rust/expanded_migration_inventory.json')
    parser.add_argument('--baseline', type=Path, default=ROOT / 'build/rust-migration-stage7/expanded-census-baseline.json')
    parser.add_argument('--dll', type=Path, required=True)
    parser.add_argument('--minimum-percent', type=float, default=75)
    parser.add_argument('--receipt', type=Path)
    args = parser.parse_args()
    inventory = json.loads(args.inventory.read_text())
    if inventory.get('current_reconciliation'):
        from verify_rust_census_reconciliation import verify
        try:
            result = verify(inventory, args.dll, inventory['current_reconciliation'])
        except (ValueError, KeyError, OSError) as error:
            raise SystemExit(f'FAIL: current census reconciliation: {error}') from error
        if result['verified_percent'] < args.minimum_percent:
            raise SystemExit('FAIL: current census below required percentage')
        result['inventory_sha256'] = sha256(args.inventory)
        if args.receipt:
            args.receipt.parent.mkdir(parents=True, exist_ok=True)
            args.receipt.write_text(json.dumps(result, indent=2) + '\n')
        print('Expanded Rust census: 151/151 portable implementation units, '
              f"{result['required_rust_symbols_linked']} linked symbols; PASS. "
              'Full native ownership and in-game acceptance remain separate.')
        return
    baseline = json.loads(args.baseline.read_text())
    units = {unit['path']: unit for unit in inventory['units']}
    frozen = {unit['path']: unit for unit in baseline['units']}
    if len(units) != len(inventory['units']) or set(units) != set(frozen):
        raise SystemExit('FAIL: expanded census contains duplicates or changed membership')
    if len(units) != inventory['totals']['implementation_units']:
        raise SystemExit('FAIL: census total disagrees with its members')
    for name, unit in units.items():
        for key in ['baseline_source_sha256', 'legacy_census_paths', 'expanded_from_aggregation_wrapper']:
            if unit.get(key) != frozen[name].get(key):
                raise SystemExit(f'FAIL: frozen census provenance changed: {name}: {key}')
    migrated = [unit for unit in units.values() if unit['status'].startswith('verified_')]
    count = len(migrated)
    percent = 100 * count / len(units)
    if count != inventory['totals']['verified_migrated_units']:
        raise SystemExit('FAIL: verified total disagrees with unit statuses')
    if len(units) - count != inventory['totals']['pending_units']:
        raise SystemExit('FAIL: pending total disagrees with unit statuses')
    if abs(percent - inventory['totals']['verified_percent']) > 1e-9:
        raise SystemExit('FAIL: verified percentage disagrees with unit statuses')
    symbols = subprocess.check_output(['i686-w64-mingw32-nm', '--defined-only', str(args.dll)], text=True)
    linked = set(re.findall(r'\b[Tt]\s+_?(a2fo_\w+)$', symbols, re.MULTILINE))
    required = set()
    receipt_cache = {}
    stage9_receipt_cache = {}
    for unit in migrated:
        if not (ROOT / unit['rust_source']).is_file() or not unit['rust_symbols']:
            raise SystemExit(f"FAIL: missing Rust implementation evidence: {unit['path']}")
        required.update(unit['rust_symbols'])
        if unit.get('stage') == 'stage7':
            receipts = unit.get('parity_receipts', [])
            groups = unit.get('parity_groups', [])
            if not receipts:
                raise SystemExit(f"FAIL: missing native parity evidence: {unit['path']}")
            if not groups or len(set(groups)) != len(groups) or not set(groups) <= EXPECTED_GROUPS:
                raise SystemExit(f"FAIL: incomplete native parity: {unit['path']}")
            for evidence in receipts:
                receipt_path = ROOT / evidence
                if not receipt_path.is_file():
                    raise SystemExit(f'FAIL: missing parity receipt: {evidence}')
                if receipt_path not in receipt_cache:
                    receipt_cache[receipt_path] = verify_parity_receipt(receipt_path)
                passed, _ = receipt_cache[receipt_path]
                if not set(groups) <= passed:
                    raise SystemExit(f"FAIL: incomplete native parity: {unit['path']}")
            inputs = receipt_cache[ROOT / receipts[0]][1]
            for path in [unit['path'], unit['rust_source']]:
                actual = hashlib.sha256((ROOT / path).read_bytes()).hexdigest()
                if inputs.get(path) != actual:
                    raise SystemExit(f'FAIL: source differs from native parity inputs: {path}')
            source = ROOT / unit['path']
            if hashlib.sha256(source.read_bytes()).hexdigest() != unit.get('verified_cpp_sha256'):
                raise SystemExit(f"FAIL: source changed after verification: {unit['path']}")
        elif unit.get('stage') == 'stage9':
            verify_stage9_unit(unit, args.dll, stage9_receipt_cache)
    missing = sorted(required - linked)
    if missing:
        raise SystemExit('FAIL: Rust implementation symbols absent from DLL: ' + ', '.join(missing))
    if percent < args.minimum_percent:
        raise SystemExit(f'FAIL: {count}/{len(units)} verified ({percent:.4f}%) is below {args.minimum_percent}%')
    result = {'implementation_units': len(units), 'verified_units': count, 'verified_percent': percent,
              'census_membership_unchanged': True, 'required_rust_symbols_linked': len(required),
              'dll_sha256': hashlib.sha256(args.dll.read_bytes()).hexdigest(),
              'inventory_sha256': hashlib.sha256(args.inventory.read_bytes()).hexdigest()}
    if args.receipt:
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(json.dumps(result, indent=2) + '\n')
    print(f'Expanded Rust census: {count}/{len(units)} ({percent:.4f}%), {len(required)} linked symbols; PASS')


if __name__ == '__main__':
    main()
