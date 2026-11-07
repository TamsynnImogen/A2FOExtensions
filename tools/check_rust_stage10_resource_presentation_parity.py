#!/usr/bin/env python3
"""Build frozen/current full-module presentation fixtures and compare traces."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SLICE = ROOT / 'build/rust-migration-stage10/resource-presentation'
UNIT = Path('modules/A2FOResources/module.cpp')
FIXTURE = ROOT / 'tests/rust_stage10_resource_presentation_mock.cpp'
FROZEN = SLICE / 'before' / UNIT
ORIGINAL_ARCHIVE = ROOT / 'build/rust-migration-stage9/checkpoint-archive10/liba2fo_core-win.a'
ARCHIVE_MANIFEST = ROOT / 'build/rust-migration-stage9/checkpoint-archive10/source-hashes.json'
FROZEN_HEADERS = {
    'sdk/include/a2fo_module_api.h',
    'sdk/include/a2fo_module_provider.hpp',
    'modules/A2FOResources/additional_resources.hpp',
    'modules/A2FOResources/api.hpp',
    'modules/A2FOSquadrons/api.hpp',
    'rust/stage8_c_resources_ffi.h',
}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def outside_target_functions(path):
    source = path.read_text()
    for signature in ('void* local_team_object() noexcept {',
                      'bool localized_presentation_override(',
                      'const char* localize_presentation('):
        start = source.find(signature)
        if start < 0 or source.find(signature, start + 1) >= 0:
            raise RuntimeError(f'unique function not found: {signature} in {path}')
        opening = source.find('{', start)
        depth = 0
        for index in range(opening, len(source)):
            if source[index] == '{':
                depth += 1
            elif source[index] == '}':
                depth -= 1
                if depth == 0:
                    source = source[:start] + signature + '/* target */' + source[index + 1:]
                    break
        else:
            raise RuntimeError(f'unclosed function: {signature} in {path}')
    return re.sub(r'^#include (?:"../../rust/stage10_resource_presentation_ffi.h"|<optional>)\n',
                  '', source, flags=re.MULTILINE)


def run(command, output, label, env=None):
    with (output / f'{label}.stdout.log').open('wb') as out, \
         (output / f'{label}.stderr.log').open('wb') as err:
        result = subprocess.run(command, cwd=ROOT, env=env, stdout=out,
                                stderr=err, timeout=300, check=False)
    if result.returncode:
        raise RuntimeError(f'{label} exit {result.returncode}; see '
                           f'{output / (label + ".stderr.log")}')


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
    candidate_archive = args.rust_lib.resolve(strict=True)
    expected = json.loads((SLICE / 'before-hashes.json').read_text())
    if digest(FROZEN) != expected[str(UNIT)]:
        raise RuntimeError('frozen module source drifted')
    if outside_target_functions(FROZEN) != outside_target_functions(ROOT / UNIT):
        raise RuntimeError('current module has changes outside the three target '
                           'functions and required includes')
    original_hash = digest(ORIGINAL_ARCHIVE)
    if original_hash != expected['build/rust/i686-pc-windows-gnu/release/liba2fo_core.a']:
        raise RuntimeError('archive10 Windows archive drifted')
    if not FIXTURE.is_file():
        raise RuntimeError('fixture missing')
    output.mkdir(parents=True)
    dependency_command = [
        'i686-w64-mingw32-g++', '-std=gnu++17', '-MM',
        '-I', str(ROOT / 'modules/A2FOResources'),
        f'-DA2FO_STAGE10_RESOURCES_SOURCE="{FROZEN}"', str(FIXTURE)]
    dependencies = subprocess.run(dependency_command, cwd=ROOT,
                                  capture_output=True, timeout=30, check=True).stdout
    (output / 'frozen-dependencies.txt').write_bytes(dependencies)
    tokens = dependencies.decode().replace('\\\n', ' ').split(':', 1)[1].split()
    project_dependencies = {
        str((ROOT / token).resolve().relative_to(ROOT))
        for token in tokens if (ROOT / token).resolve().is_relative_to(ROOT)
        and (ROOT / token).resolve() not in (FIXTURE, FROZEN)
    }
    if project_dependencies != FROZEN_HEADERS:
        raise RuntimeError('frozen project dependencies changed: '
                           f'{sorted(project_dependencies ^ FROZEN_HEADERS)}')
    archived_hashes = json.loads(ARCHIVE_MANIFEST.read_text())
    for header in FROZEN_HEADERS:
        if header not in archived_hashes or digest(ROOT / header) != archived_hashes[header]:
            raise RuntimeError(f'frozen header differs from archive10 manifest: {header}')
    hashes = {
        'frozen_source': digest(FROZEN),
        'current_source': digest(ROOT / UNIT),
        'fixture': digest(FIXTURE),
        'checker': digest(Path(__file__)),
        'archive10_manifest': digest(ARCHIVE_MANIFEST),
        'frozen_dependencies': digest(output / 'frozen-dependencies.txt'),
        'stage8_header': digest(ROOT / 'rust/stage8_c_resources_ffi.h'),
        'stage10_header': digest(ROOT / 'rust/stage10_resource_presentation_ffi.h'),
        'resource_header': digest(ROOT / 'modules/A2FOResources/additional_resources.hpp'),
        'resource_api': digest(ROOT / 'modules/A2FOResources/api.hpp'),
        'squadrons_api': digest(ROOT / 'modules/A2FOSquadrons/api.hpp'),
        'module_api_header': digest(ROOT / 'sdk/include/a2fo_module_api.h'),
        'module_provider_header': digest(ROOT / 'sdk/include/a2fo_module_provider.hpp'),
        'original_archive': original_hash,
        'candidate_archive': digest(candidate_archive),
        'original_archive_path': str(ORIGINAL_ARCHIVE),
        'candidate_archive_path': str(candidate_archive),
    }
    pinned_paths = {
        'frozen_source': FROZEN, 'current_source': ROOT / UNIT,
        'fixture': FIXTURE, 'checker': Path(__file__),
        'archive10_manifest': ARCHIVE_MANIFEST,
        'frozen_dependencies': output / 'frozen-dependencies.txt',
        'stage8_header': ROOT / 'rust/stage8_c_resources_ffi.h',
        'stage10_header': ROOT / 'rust/stage10_resource_presentation_ffi.h',
        'resource_header': ROOT / 'modules/A2FOResources/additional_resources.hpp',
        'resource_api': ROOT / 'modules/A2FOResources/api.hpp',
        'squadrons_api': ROOT / 'modules/A2FOSquadrons/api.hpp',
        'module_api_header': ROOT / 'sdk/include/a2fo_module_api.h',
        'module_provider_header': ROOT / 'sdk/include/a2fo_module_provider.hpp',
        'original_archive': ORIGINAL_ARCHIVE,
        'candidate_archive': candidate_archive,
    }
    def verify_inputs():
        for name, path in pinned_paths.items():
            if digest(path) != hashes[name]:
                raise RuntimeError(f'input drifted during run: {name}')
    (output / 'inputs.json').write_text(json.dumps(hashes, indent=2) + '\n')
    for label, source, archive in (
            ('frozen', FROZEN, ORIGINAL_ARCHIVE),
            ('current', ROOT / UNIT, candidate_archive)):
        command = [
            'i686-w64-mingw32-g++', '-std=gnu++17', '-O2', '-Wall', '-Wextra',
            '-Werror', '-Wno-mismatched-new-delete',
            '-ffunction-sections', '-fdata-sections',
            '-Wl,--gc-sections', '-static',
            '-I', str(ROOT / 'modules/A2FOResources'),
            f'-DA2FO_STAGE10_RESOURCES_SOURCE="{source}"',
            str(FIXTURE), str(archive), '-o', str(output / f'{label}.exe')]
        run(command, output, f'build-{label}')
        verify_inputs()
        hashes[f'{label}_binary'] = digest(output / f'{label}.exe')
    (output / 'inputs.json').write_text(json.dumps(hashes, indent=2) + '\n')
    def verify_binaries():
        for label in ('frozen', 'current'):
            if digest(output / f'{label}.exe') != hashes[f'{label}_binary']:
                raise RuntimeError(f'{label} binary drifted during run')
    verify_binaries()
    if args.build_only:
        print(f'PASS build-only: {output}')
        return
    if args.wine_prefix:
        prefix = args.wine_prefix.resolve()
        context = None
    else:
        context = tempfile.TemporaryDirectory(prefix='a2fo-stage10-presentation-')
        prefix = Path(context.name) / 'wine'
    try:
        env = {**os.environ, 'WINEPREFIX': str(prefix), 'WINEDEBUG': '-all',
               'DISPLAY': '', 'WAYLAND_DISPLAY': '',
               'LIBGL_ALWAYS_SOFTWARE': '1',
               'WINEDLLOVERRIDES': 'winemenubuilder.exe=d;winedbg.exe=d;'
                                     'winewayland.drv=d;winex11.drv=d'}
        for label in ('frozen', 'current'):
            verify_inputs()
            verify_binaries()
            run(['wine', str(output / f'{label}.exe')], output,
                f'run-{label}', env)
            raw = (output / f'run-{label}.stdout.log').read_bytes()
            (output / f'{label}.trace').write_bytes(raw.replace(b'\r\n', b'\n'))
    finally:
        if context:
            context.cleanup()
    verify_inputs()
    verify_binaries()
    frozen = (output / 'frozen.trace').read_bytes()
    current = (output / 'current.trace').read_bytes()
    if frozen != current:
        first = next((i for i, pair in enumerate(zip(frozen.splitlines(),
                       current.splitlines()), 1) if pair[0] != pair[1]), None)
        raise RuntimeError(f'traces differ at line {first or "end"}')
    if frozen.count(b'PASS:') < 30 or b'FAIL:' in frozen:
        raise RuntimeError('fixture trace lacks expected passing assertions')
    receipt = {**hashes, 'trace_lines': len(frozen.splitlines()),
               'normalized_trace_sha256': hashlib.sha256(frozen).hexdigest(),
               'equal': True}
    (output / 'parity.json').write_text(json.dumps(receipt, indent=2) + '\n')
    print(f'PASS resource presentation: {receipt["trace_lines"]} assertions; {output}')


if __name__ == '__main__':
    main()
