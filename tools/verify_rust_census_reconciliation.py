#!/usr/bin/env python3
"""Verify a current portable-unit census without rewriting historical evidence.

This verifies source, archived completion, linked-image and regression evidence.
It does not certify complete native ownership or manual game acceptance.
"""

import hashlib
import json
from pathlib import Path
import re
import subprocess
import tarfile


ROOT = Path(__file__).resolve().parents[1]


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def resolve(name):
    path = Path(name)
    return path if path.is_absolute() else ROOT / path


def pinned(binding):
    path = resolve(binding['path'])
    require(path.is_file() and sha256(path) == binding['sha256'],
            f'missing or changed evidence: {path}')
    return path


def document(binding):
    return json.loads(pinned(binding).read_text())


def verify_membership(inventory, previous, historical, baseline):
    units = {unit['path']: unit for unit in inventory['units']}
    prior = {unit['path']: unit for unit in previous['units']}
    old = {unit['path']: unit for unit in historical['units']}
    frozen = {unit['path']: unit for unit in baseline['units']}
    require(len(units) == len(inventory['units']) == 151 and
            set(units) == set(prior) == set(old) == set(frozen),
            'duplicate or changed 151-unit census membership')
    for name, unit in units.items():
        for key in ('baseline_source_sha256', 'baseline_source_lines',
                    'legacy_census_paths', 'expanded_from_aggregation_wrapper'):
            require(unit.get(key) == frozen[name].get(key),
                    f'changed frozen provenance: {name}: {key}')
    inherited = {name for name, unit in old.items()
                 if unit['status'].startswith('verified_')}
    pending = set(units) - inherited
    require(len(inherited) == 116 and len(pending) == 35,
            'historical 116/35 partition changed')
    for name in inherited:
        require(units[name] == prior[name] == old[name],
                f'historical credit record changed: {name}')
    for name in pending:
        require(prior[name]['status'] == 'completed_pending_census',
                f'unit lacks held completion record: {name}')
        unit = dict(units[name])
        for key in ('stage', 'rust_source', 'rust_sources', 'rust_symbols',
                    'verified_cpp_sha256', 'census_verification'):
            unit.pop(key, None)
        unit['status'] = 'completed_pending_census'
        require(unit == prior[name],
                f'completion history changed during reconciliation: {name}')
        require(units[name]['status'] == 'verified_stage17' and
                units[name]['stage'] == 'stage17' and
                units[name]['component_completion']['strict_census_credit'] is False,
                f'new credit or historical completion status invalid: {name}')
    require(inventory['totals']['implementation_units'] == 151 and
            inventory['totals']['verified_migrated_units'] == 151 and
            inventory['totals']['pending_units'] == 0 and
            inventory['totals']['verified_percent'] == 100,
            'reconciled totals differ from unit statuses')
    return units, pending


def verify_completion(binding):
    path = pinned(binding)
    checkpoint = json.loads(path.read_text())
    require(checkpoint['source_and_headless_completion'] is True and
            checkpoint['strict_census_credit'] is False and
            checkpoint['census_status'] == 'completed_pending_census',
            f'checkpoint does not record held completion: {path}')
    source = path.with_name('source-hashes.json')
    archive = path.with_name('source-and-native-cache.tar.gz')
    pinned({'path': str(source), 'sha256': checkpoint['source_hashes_sha256']})
    pinned({'path': str(archive), 'sha256': checkpoint['source_archive_sha256']})
    counts = {}
    for category in ('checkpoint_artifacts', 'evidence'):
        require(checkpoint.get(category), f'empty checkpoint {category}: {path}')
        for name, digest in checkpoint[category].items():
            pinned({'path': str(path.parent / name), 'sha256': digest})
        counts[category] = len(checkpoint[category])
    return json.loads(source.read_text()), counts


def verify_archive_sources(archive, expected):
    remaining = dict(expected)
    with tarfile.open(archive, 'r|gz') as source:
        for member in source:
            name = member.name.removeprefix('./')
            if name in remaining:
                require(member.isfile(), f'non-file archived source: {name}')
                data = source.extractfile(member).read()
                require(hashlib.sha256(data).hexdigest() == remaining[name],
                        f'archive/manifest source mismatch: {name}')
                del remaining[name]
            if not remaining:
                break
    require(not remaining, f'completion archive omits sources: {sorted(remaining)}')


def implementation_symbols(unit):
    required = set(re.findall(r'\b(a2fo_rs_\w+)\s*\(',
                             resolve(unit['path']).read_text()))
    for name in unit['component_completion']['rust_sources']:
        required.update(re.findall(
            r'#\[(?:unsafe\()?no_mangle\)?\]\s*(?:pub\s+)?(?:unsafe\s+)?'
            r'extern\s+"C"\s+fn\s+(a2fo_\w+)', resolve(name).read_text()))
    return sorted(required)


def verify_focused_checks(contract):
    checks = contract['focused_checks']
    require(set(checks) == {'runtime_ownership', 'patch_profiles',
                            'media_playback', 'adapter_cleanup'},
            'focused regression plan differs')
    runtime = document(checks['runtime_ownership'])
    require(runtime['status'] == 'pass-bounded' and
            runtime['selected_core_suites'] ==
            ['settings', 'lifecycle', 'dispatch', 'classload', 'lookup'] and
            runtime['core_pairs'] == 5 and
            runtime['dispatch_allocation_section']['equal'] is True and
            runtime['executions'] == runtime['expected_executions'] and
            runtime['build_only'] is False, 'runtime ownership gate incomplete')
    patch = document(checks['patch_profiles'])
    require(patch['status'] == 'passed' and len(patch['runs']) == 2 and
            patch['host_windows_profile_transcript_exact'] is True and
            all(row['exit'] == 0 for row in patch['runs']),
            'checked patch profile gate incomplete')
    media = document(checks['media_playback'])
    required_media = {'wine-unit-accepted', 'facade-accepted',
                      'backend-loader-accepted', 'playback-accepted',
                      'pipeline-720p-accepted', 'pipeline-1080p-paced-accepted'}
    require(media['status'] == 'passed-headless-memory-host-not-game-acceptance' and
            required_media <= set(media['runs']) and
            all(media['runs'][name]['exit'] == 0 for name in required_media),
            'media lifecycle/playback gate incomplete')
    cleanup = document(checks['adapter_cleanup'])
    require(cleanup['status'] == 'passed-headless-binding-removal-and-contracts' and
            cleanup['pair_count'] == len(cleanup['pairs']) == 17 and
            cleanup['pair_lines'] == 18690 and
            {'root-smoke', 'radar', 'decal'} <= set(cleanup['runs']) and
            all(row['exit'] == 0 for row in cleanup['runs'].values()),
            'direct binding/linked DLL gate incomplete')
    needed = {'runtime_ownership', 'adapter_cleanup'}
    bindings = {
        'core/dllmain.cpp': needed,
        'core/module_menu.cpp': {'adapter_cleanup'},
        'core/nebula_renderer.cpp': {'patch_profiles', 'adapter_cleanup'},
        'core/renderer_options.cpp': {'adapter_cleanup'},
        'modules/A2FOCraftIdentity/module.cpp': {'patch_profiles'},
        'modules/A2FOVideo/module.cpp': {'patch_profiles', 'media_playback'},
    }
    changed = {name for name, row in contract['units'].items()
               if row['current_cpp_sha256'] != row['completion_cpp_sha256']}
    require(changed == set(bindings), 'post-completion source change set differs')
    for name, required in bindings.items():
        require(set(contract['units'][name]['reviewed_change']['focused_checks']) == required,
                f'focused change evidence differs: {name}')
    return checks


def verify_parity(receipt):
    from run_rust_stage7_parity import GROUPS
    names = {group[0] for group in GROUPS}
    results = receipt.get('results', [])
    require(receipt.get('status') == 'passed' and len(names) == 30 and
            len(results) == 30 and len({row['group'] for row in results}) == 30 and
            {row['group'] for row in results} == names and
            len(receipt.get('selected_groups', [])) == 30 and
            set(receipt['selected_groups']) == names,
            'incomplete current 30-group parity')
    for row in results:
        require(row['status'] == 'passed' and row['exit_code'] == 0 and
                row['changed_inputs'] == [] and
                row['child_receipt_missing'] is False,
                f'failed current native group: {row["group"]}')


def verify(inventory, dll, binding):
    contract_path = pinned(binding)
    contract = json.loads(contract_path.read_text())
    require(contract['schema'] == 1 and
            contract['scope'] == 'portable-implementation-unit-credit' and
            contract['full_rust_rewrite'] is False and
            contract['manual_game_acceptance'] is False and
            contract['deployment'] is False,
            'unsupported reconciliation scope')
    previous = document(contract['previous_inventory'])
    historical = document(contract['historical_inventory'])
    baseline = document(contract['membership_baseline'])
    units, pending = verify_membership(inventory, previous, historical, baseline)
    require(set(contract['units']) == pending,
            'reconciliation does not cover exactly the 35 held units')

    # Current identities are checked before expensive historical archives.
    require(contract['source_pins'], 'current source pins missing')
    for name, digest in contract['source_pins'].items():
        pinned({'path': name, 'sha256': digest})
    retired = document(contract['retirement_ledger'])
    retired_sources = {name: row for row in retired['retired']
                       for name in row['removed'] if name.endswith('.cpp')}
    absent = {name for name in units if not resolve(name).is_file()}
    require(absent == set(retired_sources) == {'core/hook.cpp', 'core/decal_math.cpp'},
            'unexplained missing census source')
    for name, unit in units.items():
        if name in absent:
            owner = retired_sources[name]['rust_owner']
            require(owner == unit['rust_source'] and owner in contract['source_pins'],
                    f'retired source lacks its current Rust owner: {name}')
            pinned({'path': 'tests/native_reference/adapters/' + name,
                    'sha256': retired['reference_hashes'][name]})
        else:
            require(name in contract['source_pins'],
                    f'current census source is not pinned: {name}')
    require(Path(dll).resolve() == pinned(contract['dll']).resolve(),
            'DLL differs from reconciled candidate')
    historical_receipt = document(contract['historical_census'])
    require(historical_receipt['verified_units'] == 116 and
            historical_receipt['implementation_units'] == 151 and
            historical_receipt['inventory_sha256'] ==
            contract['historical_inventory']['sha256'] and
            historical_receipt['census_membership_unchanged'] is True,
            'historical 116-unit credit is not bound to its strict receipt')
    pinned(contract['historical_dll'])
    require(contract['historical_dll']['sha256'] ==
            historical_receipt['dll_sha256'], 'historical DLL receipt mismatch')

    host = document(contract['host_regressions'])
    require(host['status'] == 'passed' and host['exit_code'] == 0 and
            not host['changed_source_pins'] and not host['archive_drift'] and
            not host['private_tests_drift'] and not host['private_tools_drift'],
            'current combined host regressions did not pass')
    host_root = resolve(contract['host_root'])
    pinned({'path': str(pinned(contract['host_regressions']).with_name('make.log')),
            'sha256': host['log_sha256']})
    require(len(host['artifacts']) == 41 and host['recovered'],
            'combined host artifact/test set is incomplete')
    for category in ('artifacts', 'private_tests', 'private_tools'):
        for name, digest in host[category].items():
            pinned({'path': str(host_root / name), 'sha256': digest})
    native = document(contract['native_regressions'])
    verify_parity(native)
    for row in native['results']:
        # These log/child bytes are independently included in the sealed output
        # list, so a later overwritten runner directory cannot inherit credit.
        for key in ('stdout', 'stderr', 'child_receipt'):
            if row.get(key):
                require(any(resolve(item['path']).resolve() == resolve(row[key]).resolve()
                            for item in contract['regression_evidence']),
                        f'native group output is not sealed: {row["group"]}: {key}')
    rust = document(contract['rust_regressions'])
    required_steps = {'core-host-test', 'core-windows-test-build', 'core-fmt',
                      'core-host-clippy', 'core-windows-clippy',
                      'media-host-test', 'media-windows-test-build', 'media-fmt',
                      'mmm-loader-windows-build', 'mmm-loader-rustfmt',
                      'dll-fmt', 'dll-windows-lib-clippy-abort'}
    require(rust['status'] == 'passed-required-gates-with-advisories' and
            rust['changed_inputs'] == [] and not rust['required_failures'] and
            len(rust['required_steps']) == len(required_steps) and
            {row['name'] for row in rust['required_steps']} == required_steps and
            all(row['exit_code'] == 0 for row in rust['required_steps']),
            'combined Rust gates did not pass')
    for row in [*rust['required_steps'], *rust['advisory_steps']]:
        pinned({'path': row['log'], 'sha256': row['log_sha256']})
    rust_root = pinned(contract['rust_regressions']).parent
    for name, digest in rust['artifacts'].items():
        pinned({'path': str(rust_root / name), 'sha256': digest})
    require(rust['inputs_before'] == rust['inputs_after'] and
            host['source_pins_before'] == host['source_pins_after'],
            'regression source identity changed')
    for name, digest in rust['inputs_before'].items():
        pinned({'path': name, 'sha256': digest})
        if name in contract['source_pins']:
            require(digest == contract['source_pins'][name],
                    f'Rust gate used different product input: {name}')
    for name, row in contract['units'].items():
        if row['current_cpp_sha256'] != row['completion_cpp_sha256']:
            require(host['source_pins_before'].get(name) == row['current_cpp_sha256'],
                    f'combined host gate omits changed current source: {name}')
    for name, digest in contract['source_pins'].items():
        if (name.startswith(('rust/a2fo_core/', 'rust/a2fo_dll/', 'rust/a2fo_media/')) or
                name == rust['standalone_loader']) and name.endswith(('.rs', 'Cargo.toml', 'Cargo.lock')):
            require(rust['inputs_before'].get(name) == digest,
                    f'Rust gate omits current Rust implementation: {name}')
    bridges = document(contract['bridge_manifest'])
    parity = document(contract['bridge_parity'])
    require(len(bridges['bridges']) == 48 and parity['status'] == 'passed' and
            len(parity['cases']) == 48 and
            {row['legacy'] for row in bridges['bridges']} == set(parity['cases']),
            'standalone bridge source/object parity incomplete')
    for row in bridges['bridges']:
        case = parity['cases'][row['legacy']]
        require(case['differences'] == [] and case['exits'] == [0, 0] and
                case['inputs'].get(str(resolve(row['rust']))) ==
                contract['source_pins'].get(row['rust']),
                f'standalone bridge differs from verified source: {row["rust"]}')
    # Pin all logs, executable outputs, fixture inputs, archives and subsidiary
    # receipts used by these regression gates, not only their summary JSON.
    require(contract['regression_evidence'], 'regression output pins missing')
    for item in contract['regression_evidence']:
        pinned(item)
    for name, digest in native['initial_sha256'].items():
        pinned({'path': str(resolve(name)) if Path(name).is_absolute() else
                str(resolve(contract['native_root']) / name), 'sha256': digest})

    retirement = document(contract['retirement_contract'])
    require(retirement['status'] == 'passed-retirement-and-dll-contracts' and
            retirement['images']['A2FOExtensions.dll']['sha256'] == sha256(dll) and
            retirement['images']['A2FOExtensions.dll']['exports'] == 197,
            'candidate DLL/export contract differs')
    link = document(contract['link_receipt'])
    require(link['exit_code'] == 0 and link['changed_inputs'] == [] and
            link['inputs'] == link['inputs_after'] and
            resolve(link['output']).resolve() == Path(dll).resolve() and
            link['output_sha256'] == sha256(dll),
            'candidate link receipt did not pass')
    for field in ('log', 'manifest', 'export_contract'):
        pinned({'path': link[field], 'sha256': link[field + '_sha256']})
    for name, digest in link['inputs'].items():
        pinned({'path': name, 'sha256': digest})
    # Cargo builds core again as an rlib dependency of the Rust-owned DLL.
    # The fixture static archive is a separate build, not a native linker input.
    for name, digest in contract['source_pins'].items():
        if name.startswith('rust/a2fo_core/') and name.endswith(('.rs', 'Cargo.toml', 'Cargo.lock')):
            require(link['inputs'].get(str(resolve(name))) == digest,
                    f'DLL Cargo dependency differs from tested core source: {name}')
    current_archive = pinned(contract['windows_archive'])
    require(sha256(current_archive) == sha256(resolve(native['rust_lib'])),
            'native regression archive differs from designated fixture archive')
    archive_sources = document(contract['archive_sources'])
    archive_artifacts = document(contract['archive_artifacts'])
    archive_build = document(contract['archive_build'])
    require(archive_build['windows-archive']['exit'] == 0 and
            archive_build['host-archive']['exit'] == 0 and
            archive_artifacts['rust/i686-pc-windows-gnu/release/liba2fo_core.a'] ==
            sha256(current_archive), 'fixture archive build/snapshot differs')
    for name, digest in contract['source_pins'].items():
        if name.startswith('rust/a2fo_core/'):
            require(archive_sources.get(name) == digest,
                    f'fixture archive source snapshot differs: {name}')
    for name, digest in host['archives'].items():
        pinned({'path': name, 'sha256': digest})
    require(sha256(current_archive) in host['archives'].values(),
            'host regression did not use the current integrated core archive')
    focused_checks = verify_focused_checks(contract)

    snapshots = {}
    checkpoint_counts = {}
    for item in contract['completion_checkpoints']:
        pins, counts = verify_completion(item)
        snapshots[item['path']] = pins
        checkpoint_counts[item['path']] = counts
        required_sources = {source for name in pending
                            if units[name]['component_completion']['checkpoint'] == item['path']
                            for source in [name, *units[name]['component_completion']['rust_sources']]}
        verify_archive_sources(resolve(item['path']).with_name('source-and-native-cache.tar.gz'),
                               {name: pins[name] for name in required_sources})
    require(set(snapshots) == {units[name]['component_completion']['checkpoint']
                               for name in pending},
            'completion checkpoint membership differs')
    for name in pending:
        unit = units[name]
        completion = unit['component_completion']
        proof = contract['units'][name]
        old = snapshots[completion['checkpoint']]
        require(old[name] == completion['cpp_sha256'],
                f'completion source pin differs: {name}')
        sources = completion['rust_sources']
        require(unit['rust_sources'] == sources and unit['rust_source'] in sources,
                f'Rust implementation binding differs: {name}')
        require(unit['rust_symbols'] == implementation_symbols(unit),
                f'Rust symbol set does not match implementation sources: {name}')
        for source in sources:
            require(old[source] == contract['source_pins'].get(source),
                    f'Rust algorithm changed since completion: {source}')
        current = contract['source_pins'].get(name)
        require(current and unit['verified_cpp_sha256'] == current and
                proof['current_cpp_sha256'] == current and
                proof['completion_cpp_sha256'] == old[name],
                f'current native source binding differs: {name}')
        if current != old[name]:
            change = proof.get('reviewed_change')
            require(change and change['summary'] and change['focused_checks'],
                    f'unreviewed post-completion native source change: {name}')
            source_manifest = document(change['source_manifest'])
            require(source_manifest[name] == current,
                    f'later checkpoint does not pin current source: {name}')
            require(set(change['focused_checks']) <= set(focused_checks),
                    f'unknown focused check: {name}')
        require(unit['census_verification'] == binding,
                f'unit points to different reconciliation: {name}')

    symbols = subprocess.check_output(
        ['i686-w64-mingw32-nm', '--defined-only', str(dll)], text=True)
    linked = set(re.findall(r'\b[Tt]\s+_?(a2fo_\w+)$', symbols, re.MULTILINE))
    required = set()
    for unit in units.values():
        require(unit.get('rust_symbols') and unit.get('rust_source') and
                resolve(unit['rust_source']).is_file(),
                f'missing implementation symbols: {unit["path"]}')
        required.update(unit['rust_symbols'])
        for source in unit.get('rust_sources', [unit['rust_source']]):
            require(source in contract['source_pins'],
                    f'implementation source not pinned: {source}')
    require(not required - linked,
            'required Rust symbols absent from DLL: ' + ', '.join(sorted(required - linked)))
    return {'implementation_units': 151, 'verified_units': 151,
            'verified_percent': 100, 'newly_credited_units': 35,
            'historical_credit_preserved': 116,
            'census_membership_unchanged': True,
            'required_rust_symbols_linked': len(required),
            'dll_sha256': sha256(dll),
            'core_cross_build_binding': 'same pinned sources; separate Cargo DLL and static archive builds',
            'reconciliation_sha256': sha256(contract_path),
            'scope': contract['scope'], 'full_rust_rewrite': False,
            'manual_game_acceptance': False, 'deployment': False,
            'completion_checkpoints': checkpoint_counts}
