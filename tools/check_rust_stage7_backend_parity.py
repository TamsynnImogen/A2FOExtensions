#!/usr/bin/env python3
"""Headless frozen/current native backend differential fixture."""
import hashlib
import argparse
from contextlib import nullcontext
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/rust-migration-stage7/before'
WORK=ROOT/'build/rust-migration-stage7/backend-work'
HASHES=json.loads((ROOT/'build/rust-migration-stage7/before-hashes.json').read_text())
BACK=Path('renderer/d3d11/game/backend.cpp')
CONTRACT=Path('renderer/d3d11/game/contracts.hpp')
OBSERVER=Path('renderer/d3d8_compat/observer.cpp')

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd,env=None):
    p=subprocess.run(cmd,cwd=ROOT,capture_output=True,env=env,timeout=180)
    if p.returncode:raise RuntimeError(f'{cmd}: {p.returncode}\n{p.stderr[-3000:]!r}\n{p.stdout[-1000:]!r}')
    return p.stdout.replace(b'\r\n',b'\n')
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib',type=Path,help='link current fixtures against this Rust archive')
    parser.add_argument('--isolated',action='store_true',help='build the standalone Rust module archive')
    parser.add_argument('--wine-prefix',type=Path,help='reuse this prewarmed isolated Wine prefix')
    parser.add_argument('--compile-only',action='store_true',help='build frozen/current native fixtures without Wine')
    args=parser.parse_args()
    canonical=ROOT/'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
    selected=args.rust_lib or (canonical if canonical.exists() and not args.isolated else None)
    for path in (BACK,CONTRACT,OBSERVER):
        if sha(BASE/path)!=HASHES[str(path)]:raise RuntimeError(f'frozen hash changed: {path}')
    WORK.mkdir(parents=True,exist_ok=True)
    freeze=WORK/'frozen/renderer/d3d11/game'
    freeze.mkdir(parents=True,exist_ok=True)
    shutil.copy2(BASE/BACK,freeze/'backend.cpp')
    current_header=(ROOT/CONTRACT).read_text()
    original_header=(BASE/CONTRACT).read_text()
    for snippet in ('#if defined(A2FO_HEADLESS_NATIVE_TEST)\n    friend struct BackendFixture;\n#endif',):
        if snippet not in current_header:raise RuntimeError('test seam missing')
    freeze.joinpath('contracts.hpp').write_text(original_header.replace(
        'friend class Backend;', 'friend class Backend;\n#if defined(A2FO_HEADLESS_NATIVE_TEST)\n    friend struct BackendFixture;\n#endif').replace(
        'private:struct Impl;std::unique_ptr<Impl> impl_;\n};\n}',
        'private:struct Impl;std::unique_ptr<Impl> impl_;\n#if defined(A2FO_HEADLESS_NATIVE_TEST)\n    friend struct BackendFixture;\n#endif\n};\n}'))
    for folder in ('textures','shaders'):
        dst=freeze.parent.parent/folder
        if not dst.exists():dst.symlink_to(BASE/'renderer'/folder,target_is_directory=True)
    wrapper=WORK/'standalone_backend.rs'
    wrapper.write_text('#![no_std]\nunsafe extern "C" { fn abort() -> !; }\n'
      '#[panic_handler] fn panic(_: &core::panic::PanicInfo) -> ! { unsafe { abort() } }\n'
      '#[unsafe(no_mangle)] extern "C" fn rust_eh_personality(_:i32,_:i32,_:u64,_:*mut core::ffi::c_void,_:*mut core::ffi::c_void)->i32 { unsafe { abort() } }\n'
      'mod backend { include!("'+str(ROOT/'rust/a2fo_core/src/game_backend.rs')+'"); }\n')
    archive=selected or WORK/'libgame_backend.a'
    if selected:
        if not archive.is_file():raise RuntimeError(f'Rust archive missing: {archive}')
    else:
        run(['rustc','--edition=2024','--target','i686-pc-windows-gnu','-C','panic=abort',
             '-D','unsafe-op-in-unsafe-fn','--crate-type','staticlib',str(wrapper),'-o',str(archive)])
    outputs=[]
    for label,path in [('before',freeze/'backend.cpp'),('current',ROOT/BACK)]:
        exe=WORK/f'backend-{label}.exe'
        cmd=['i686-w64-mingw32-g++','-std=gnu++17','-O2','-DA2FO_HEADLESS_NATIVE_TEST',
             '-DA2FO_BACKEND_SOURCE="'+str(path)+'"','-ffunction-sections','-fdata-sections',
             '-Wl,--gc-sections','-static',str(ROOT/'tests/rust_stage7_backend_mock.cpp')]
        if label=='current':cmd.append(str(archive))
        cmd+=['-ldxguid','-luuid','-o',str(exe)]
        run(cmd);outputs.append(exe)
    observer_outputs=[]
    for label,path in [('before',BASE/OBSERVER),('current',ROOT/OBSERVER)]:
        exe=WORK/f'observer-{label}.exe'
        cmd=['i686-w64-mingw32-g++','-std=gnu++17','-O2',
             '-DA2FO_OBSERVER_SOURCE="'+str(path)+'"','-ffunction-sections','-fdata-sections',
             '-Wl,--gc-sections','-static',str(ROOT/'tests/rust_stage7_observer_mock.cpp')]
        if label=='current':cmd.append(str(archive))
        cmd+=['-ldxguid','-luuid','-o',str(exe)]
        run(cmd);observer_outputs.append(exe)
    if args.compile_only:
        print('backend/observer: full frozen/current native fixtures compile, execution skipped')
        return
    with (nullcontext(args.wine_prefix) if args.wine_prefix else tempfile.TemporaryDirectory(prefix='a2fo-stage7-backend-wine-')) as temp:
        prefix=Path(temp) if args.wine_prefix else Path(temp)/'wine'
        env=os.environ.copy();env.update(WINEPREFIX=str(prefix),WINEDEBUG='-all',DISPLAY='',WAYLAND_DISPLAY='',WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
        before,current=[run(['wine',str(exe)],env) for exe in outputs]
        observer_before,observer_current=[run(['wine',str(exe)],env) for exe in observer_outputs]
    if observer_before!=observer_current:
        a,b=observer_before.splitlines(),observer_current.splitlines()
        for i in range(max(len(a),len(b))):
            if i>=len(a) or i>=len(b) or a[i]!=b[i]:raise RuntimeError(f'observer line {i+1}: {a[i:i+1]} != {b[i:i+1]}')
    if before!=current:
        a,b=before.splitlines(),current.splitlines()
        for i in range(max(len(a),len(b))):
            if i>=len(a) or i>=len(b) or a[i]!=b[i]:raise RuntimeError(f'line {i+1}: {a[i:i+1]} != {b[i:i+1]}')
    receipt={'frozen':{str(p):HASHES[str(p)] for p in (BACK,CONTRACT,OBSERVER)},
      'current':{str(p):sha(ROOT/p) for p in (BACK,CONTRACT,OBSERVER)},
      'lines':len(before.splitlines()),'sha256':hashlib.sha256(before).hexdigest(),
      'observer_lines':len(observer_before.splitlines()),
      'observer_sha256':hashlib.sha256(observer_before).hexdigest(),
      'rust_archive':str(archive),'rust_archive_sha256':sha(archive),
      'fixture_sha256':sha(ROOT/'tests/rust_stage7_backend_mock.cpp')}
    (WORK/'parity.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(f"backend: {receipt['lines']} traces, {receipt['sha256']}, PASS")
    print(f"observer: {receipt['observer_lines']} traces, {receipt['observer_sha256']}, PASS")
if __name__=='__main__':main()
