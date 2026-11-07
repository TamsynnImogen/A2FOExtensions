#!/usr/bin/env python3
"""Full frozen/current headless DrawPipeline native differential oracle."""
import argparse
from contextlib import nullcontext
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'build/rust-migration-stage7/before'
WORK=ROOT/'build/rust-migration-stage7/draw-work'
SOURCE=Path('renderer/d3d11/game/draw_pipeline.cpp')
HEADER=Path('renderer/d3d11/game/contracts.hpp')
HASHES=json.loads((ROOT/'build/rust-migration-stage7/before-hashes.json').read_text())

def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(command,env=None):
    p=subprocess.run(command,cwd=ROOT,capture_output=True,env=env,timeout=180)
    if p.returncode:raise RuntimeError(f'{command}: {p.returncode}\n{p.stderr[-4000:]!r}\n{p.stdout[-4000:]!r}')
    return p.stdout.replace(b'\r\n',b'\n')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--rust-lib',type=Path)
    parser.add_argument('--isolated',action='store_true')
    parser.add_argument('--wine-prefix',type=Path)
    parser.add_argument('--compile-only',action='store_true')
    args=parser.parse_args()
    for path in (SOURCE,HEADER):
        if sha(BASE/path)!=HASHES[str(path)]:raise RuntimeError(f'frozen hash mismatch: {path}')
    WORK.mkdir(parents=True,exist_ok=True)
    freeze=WORK/'frozen/renderer/d3d11/game'
    freeze.mkdir(parents=True,exist_ok=True)
    shutil.copy2(BASE/SOURCE,freeze/'draw_pipeline.cpp')
    source=(BASE/HEADER).read_text()
    marker='private:struct Impl;std::unique_ptr<Impl> impl_;\n};\nclass Target'
    if marker not in source:raise RuntimeError('frozen DrawPipeline declaration changed')
    source=source.replace(marker,'private:struct Impl;std::unique_ptr<Impl> impl_;\n'
        '#if defined(A2FO_HEADLESS_NATIVE_TEST)\n    friend struct DrawFixture;\n#endif\n};\nclass Target')
    (freeze/'contracts.hpp').write_text(source)
    shaders=freeze.parent.parent/'shaders'
    if not shaders.exists():shaders.symlink_to(BASE/'renderer/shaders',target_is_directory=True)
    wrapper=WORK/'standalone_draw.rs'
    wrapper.write_text('#![no_std]\nunsafe extern "C" { fn abort() -> !; }\n'
      '#[panic_handler] fn panic(_: &core::panic::PanicInfo) -> ! { unsafe { abort() } }\n'
      '#[unsafe(no_mangle)] extern "C" fn rust_eh_personality(_:i32,_:i32,_:u64,_:*mut core::ffi::c_void,_:*mut core::ffi::c_void)->i32 { unsafe { abort() } }\n'
      'mod draw { include!("'+str(ROOT/'rust/a2fo_core/src/game_draw.rs')+'"); }\n')
    canonical=ROOT/'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a'
    selected=args.rust_lib or (canonical if canonical.exists() and not args.isolated else None)
    archive=selected or WORK/'libgame_draw.a'
    if selected:
        if not archive.is_file():raise RuntimeError(f'Rust archive missing: {archive}')
    else:
        run(['rustc','--edition=2024','--target','i686-pc-windows-gnu','-C','panic=abort',
             '-D','unsafe-op-in-unsafe-fn','--crate-type','staticlib',str(wrapper),'-o',str(archive)])
    outputs=[]
    for label,path in [('before',freeze/'draw_pipeline.cpp'),('current',ROOT/SOURCE)]:
        exe=WORK/f'draw-{label}.exe'
        command=['i686-w64-mingw32-g++','-std=gnu++17','-O2','-Wall','-Wextra','-Werror',
            '-DA2FO_HEADLESS_NATIVE_TEST','-DA2FO_DRAW_SOURCE="'+str(path)+'"',
            '-ffunction-sections','-fdata-sections','-Wl,--gc-sections','-static',
            str(ROOT/'tests/rust_stage7_draw_mock.cpp')]
        if label=='current':command.append(str(archive))
        command+=['-o',str(exe)]
        run(command);outputs.append(exe)
    original=(BASE/SOURCE).read_text()
    inverse=original[original.index('bool inverse3('):original.index('struct alignas(16) FfConstants')]
    extracted=WORK/'frozen_inverse.cpp'
    extracted.write_text('#include <cmath>\nnamespace frozen {\n'+inverse+'}\n')
    math_exe=WORK/'draw-math.exe'
    run(['i686-w64-mingw32-g++','-std=gnu++17','-O2','-Wall','-Wextra','-Werror',
        '-static',str(ROOT/'tests/rust_stage7_draw_math.cpp'),str(extracted),
        str(archive),'-o',str(math_exe)])
    if args.compile_only:
        print('draw: frozen/current full-source fixtures compile, execution skipped')
        return
    with (nullcontext(args.wine_prefix) if args.wine_prefix else tempfile.TemporaryDirectory(prefix='a2fo-stage7-draw-wine-')) as temp:
        prefix=Path(temp) if args.wine_prefix else Path(temp)/'wine'
        env=os.environ.copy();env.update(WINEPREFIX=str(prefix),WINEDEBUG='-all',DISPLAY='',WAYLAND_DISPLAY='',
            WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
        before,current=[run(['wine',str(exe)],env) for exe in outputs]
        math=run(['wine',str(math_exe)],env)
    if before!=current:
        a,b=before.splitlines(),current.splitlines()
        for i in range(max(len(a),len(b))):
            if i>=len(a) or i>=len(b) or a[i]!=b[i]:
                raise RuntimeError(f'draw line {i+1}: {a[i:i+1]} != {b[i:i+1]}')
    receipt={'frozen':{str(p):HASHES[str(p)] for p in (SOURCE,HEADER)},
        'current':{str(p):sha(ROOT/p) for p in (SOURCE,HEADER)},
        'rust_archive':str(archive),'rust_archive_sha256':sha(archive),
        'fixture_sha256':sha(ROOT/'tests/rust_stage7_draw_mock.cpp'),
        'lines':len(before.splitlines()),'sha256':hashlib.sha256(before).hexdigest(),
        'math_lines':len(math.splitlines()),'math_sha256':hashlib.sha256(math).hexdigest()}
    (WORK/'parity.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print(f"draw: {receipt['lines']} traces, {receipt['sha256']}, PASS")
    print(f"draw inverse/camera: {receipt['math_lines']} mode summaries, {receipt['math_sha256']}, PASS")

if __name__=='__main__':main()
