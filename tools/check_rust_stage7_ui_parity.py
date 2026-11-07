#!/usr/bin/env python3
"""Compare full frozen/current UI renderer against headless COM ABI mocks."""
import argparse,hashlib,json,os,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
STAGE=ROOT/'build/rust-migration-stage7';WORK=STAGE/'ui-work'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--rust-lib',type=Path,default=ROOT/'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a');p.add_argument('--wine-prefix',type=Path,default=Path('/tmp/a2fo-shields-wine-prefix'));p.add_argument('--compile-only',action='store_true');p.add_argument('--receipt',type=Path);a=p.parse_args();WORK.mkdir(exist_ok=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
unit='renderer/d3d11/ui_renderer.cpp';frozen=STAGE/'before'/unit;assert sha(frozen)==json.loads((STAGE/'before-hashes.json').read_text())[unit]
outputs=[];commands=[]
for before in [True,False]:
 source=frozen if before else ROOT/unit;exe=WORK/('before.exe'if before else'current.exe')
 cmd=['i686-w64-mingw32-g++','-std=gnu++17','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-DWIN32_LEAN_AND_MEAN','-static','-UNDEBUG',f'-DUNIT_SOURCE="{source}"',str(ROOT/'tests/rust_stage7_ui_parity.cpp'),str(ROOT/'tests/rust_stage7_ui_slots.cpp'),str(ROOT/'renderer/state/fvf.cpp'),str(a.rust_lib.resolve()),'-luuid','-o',str(exe)]
 commands.append(cmd)
 with (WORK/('before-build.log'if before else'current-build.log')).open('w')as log:subprocess.run(cmd,cwd=ROOT,stdout=log,stderr=log,check=True)
 if a.compile_only:continue
 env=os.environ.copy();env.pop('DISPLAY',None);env.pop('WAYLAND_DISPLAY',None);env.update(WINEPREFIX=str(a.wine_prefix.resolve()),WINEDEBUG='-all',WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
 output=WORK/('before.stdout'if before else'current.stdout')
 with output.open('wb')as log,(WORK/('before.stderr'if before else'current.stderr')).open('wb')as err:subprocess.run(['wine',str(exe)],cwd=WORK,env=env,stdout=log,stderr=err,check=True,timeout=180)
 outputs.append(output.read_bytes())
if not a.compile_only:
 before,current=outputs
 if before!=current:
  import difflib
  diff='\n'.join(difflib.unified_diff(before.decode(errors='replace').splitlines(),current.decode(errors='replace').splitlines()));(WORK/'native.diff').write_text(diff);raise SystemExit('FAIL full UI native output differs; see ui-work/native.diff')
 assert b'PASS full native UI mock' in current
 receipt={'compiler_mode':'gnu++17 -O2','equal':True,'traces':len(current.splitlines()),'sha256':hashlib.sha256(current).hexdigest(),'archive_sha256':sha(a.rust_lib),'before_cpp_sha256':sha(frozen),'current_cpp_sha256':sha(ROOT/unit),'fixture_sha256':sha(ROOT/'tests/rust_stage7_ui_parity.cpp'),'commands':commands,'creates_hwnd_or_device':False}
 if a.receipt:a.receipt.write_text(json.dumps(receipt,indent=2)+'\n')
 print(f'PASS full UI native: {receipt["traces"]} matching trace lines')
else:print('PASS full frozen/current UI native fixture compile')
