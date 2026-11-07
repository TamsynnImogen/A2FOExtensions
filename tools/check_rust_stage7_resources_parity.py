#!/usr/bin/env python3
"""Compare frozen resource algorithms/COM private-data traces with Rust adapters."""
import hashlib
import argparse
from contextlib import nullcontext
import json
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
UNIT = Path('renderer/d3d8_compat/game/resources.cpp')
BASE = ROOT / 'build/rust-migration-stage7/before'
WORK = ROOT / 'build/rust-migration-stage7/resources-work'
FIXTURE = ROOT / 'tests/rust_stage7_resources_parity.cpp'
HASHES = ROOT / 'build/rust-migration-stage7/before-hashes.json'


def hash_file(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def slice_unit(source):
    text = source.read_text()
    key = text[text.index('std::array<BYTE, 16> key('):text.index('template <class T>')]
    private = text[text.index('HRESULT PrivateData::set('):text.index('Resource::Resource(')]
    formats = text[text.index('bool block_format('):text.index('#define RESOURCE_COMMON')]
    return ('#include "'+str(ROOT / 'renderer/d3d8_compat/game/private.hpp')+'"\n'
            + '#include "'+str(ROOT / 'rust/game_resources_ffi.h')+'"\n'
            + 'namespace a2fo::renderer::game {\nnamespace {\n'+key+'}\n'+private+formats+'}\n')


def run(cmd, **kwargs):
    result = subprocess.run(cmd, cwd=ROOT, capture_output=True, timeout=120, **kwargs)
    if result.returncode:
        raise RuntimeError(f'{cmd}: {result.returncode}\n{result.stderr[-4000:]!r}')
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib', type=Path, help='link current fixtures against this Rust archive')
    parser.add_argument('--isolated', action='store_true', help='build the standalone Rust module archive')
    parser.add_argument('--wine-prefix', type=Path, help='reuse this prewarmed isolated Wine prefix')
    args = parser.parse_args()
    canonical = ROOT / 'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
    selected = args.rust_lib or (canonical if canonical.exists() and not args.isolated else None)
    hashes = json.loads(HASHES.read_text())
    if hash_file(BASE / UNIT) != hashes[str(UNIT)]:
        raise RuntimeError('frozen resources.cpp hash mismatch')
    header = Path('renderer/d3d8_compat/game/private.hpp')
    if hash_file(ROOT / header) != hashes[str(header)]:
        raise RuntimeError('public/private header changed after freeze')
    WORK.mkdir(parents=True, exist_ok=True)
    archive = selected or WORK / 'libgame_resources.a'
    wrapper = WORK / 'standalone_resources.rs'
    wrapper.write_text(
        '#![no_std]\nunsafe extern "C" { fn abort() -> !; }\n'
        '#[panic_handler] fn panic(_: &core::panic::PanicInfo) -> ! '
        '{ unsafe { abort() } }\n'
        '#[unsafe(no_mangle)] extern "C" fn rust_eh_personality('
        '_version:i32,_actions:i32,_class:u64,_object:*mut core::ffi::c_void,'
        '_context:*mut core::ffi::c_void)->i32 { unsafe { abort() } }\n'
        'include!("'+str(ROOT/'rust/a2fo_core/src/game_resources.rs')+'");\n')
    if selected:
        if not archive.is_file(): raise RuntimeError(f'Rust archive missing: {archive}')
    else:
        run(['rustc','--edition=2024','--target','i686-pc-windows-gnu',
             '-C','panic=abort','-D','unsafe-op-in-unsafe-fn','--crate-type',
             'staticlib',str(wrapper),'-o',str(archive)])
    traces = []
    full_traces = []
    with (nullcontext(args.wine_prefix) if args.wine_prefix else tempfile.TemporaryDirectory(prefix='a2fo-resource-parity-')) as temp:
        prefix=Path(temp) if args.wine_prefix else Path(temp)/'wine'
        for label, source in [('before',BASE/UNIT),('rust',ROOT/UNIT)]:
            extract = Path(temp)/f'{label}.cpp'
            extract.write_text(slice_unit(source))
            executable = Path(temp)/f'{label}.exe'
            cmd = ['i686-w64-mingw32-g++','-std=gnu++17','-O2','-static',str(FIXTURE),str(extract)]
            if label == 'rust': cmd.insert(1, '-DRESOURCE_RUST'); cmd.append(str(archive))
            cmd += ['-o',str(executable)]
            run(cmd)
            traces.append(run(['wine',str(executable)],env={**os.environ,'WINEDEBUG':'-all','WINEPREFIX':str(prefix),'DISPLAY':'','WAYLAND_DISPLAY':'','WINEDLLOVERRIDES':'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d'}).replace(b'\r\n',b'\n'))
            full_exe = Path(temp)/f'full-{label}.exe'
            full_command = [
                'i686-w64-mingw32-g++','-std=gnu++17','-O2','-static',
                str(ROOT/'tests/rust_stage7_resources_mock.cpp'),
                str(WORK/'full_support.cpp'),str(WORK/'device_virtuals.cpp'),
                str(source)]
            if label == 'rust':
                full_command.append(str(archive))
            full_command += ['-ldxguid','-luuid','-o',str(full_exe)]
            run(full_command)
            full_traces.append(run(['wine',str(full_exe)],env={
                **os.environ,'WINEDEBUG':'-all',
                'WINEPREFIX':str(prefix),'DISPLAY':'',
                'WAYLAND_DISPLAY':'',
                'WINEDLLOVERRIDES':'winemenubuilder.exe=d;winedbg.exe=d;'
                'winewayland.drv=d;winex11.drv=d'}).replace(b'\r\n',b'\n'))
    if full_traces[0] != full_traces[1]:
        a,b=full_traces[0].splitlines(),full_traces[1].splitlines()
        for i in range(max(len(a),len(b))):
            if i >= len(a) or i >= len(b) or a[i]!=b[i]:
                raise RuntimeError(f'full resource difference line {i+1}: '
                                   f'{a[i:i+1]} != {b[i:i+1]}')
    if traces[0] != traces[1]:
        a,b=traces[0].splitlines(),traces[1].splitlines()
        for i in range(max(len(a),len(b))):
            if i >= len(a) or i >= len(b) or a[i]!=b[i]:
                raise RuntimeError(f'first difference line {i+1}: {a[i:i+1]} != {b[i:i+1]}')
    receipt={'cases':len(traces[0].splitlines()),'native_cases':len(full_traces[0].splitlines()),'native_sha256':hashlib.sha256(full_traces[0]).hexdigest(),'sha256':hashlib.sha256(traces[0]).hexdigest(),
             'before_cpp_sha256':hash_file(BASE/UNIT),'current_cpp_sha256':hash_file(ROOT/UNIT),
             'rust_sha256':hash_file(ROOT/'rust/a2fo_core/src/game_resources.rs'),'equal':True}
    receipt['rust_archive']=str(archive)
    receipt['rust_archive_sha256']=hash_file(archive)
    (WORK/'parity.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(f"resources: {receipt['cases']} traces, {receipt['sha256']}, PASS")

if __name__ == '__main__': main()
