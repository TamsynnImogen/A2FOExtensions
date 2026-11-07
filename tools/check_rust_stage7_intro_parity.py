#!/usr/bin/env python3
"""Frozen/current native intro workflow and offscreen pixel assertions; no HWND."""
import argparse,hashlib,json,os,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];STAGE=ROOT/'build/rust-migration-stage7';UNIT='modules/A2FOShellDisplay/native_intro.cpp'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd,env=None):
 with tempfile.TemporaryFile() as out,tempfile.TemporaryFile() as err:
  p=subprocess.run(cmd,cwd=ROOT,env=env,stdout=out,stderr=err,timeout=180);out.seek(0);err.seek(0);a,b=out.read(),err.read()
 if p.returncode:raise RuntimeError(f'exit {p.returncode} {cmd}\n{a[-1000:]!r}\n{b[-3000:]!r}')
 return a
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--wine-prefix',type=Path,default=ROOT/'build/rust-stage7-wine');p.add_argument('--receipt',type=Path);a=p.parse_args();hashes=json.loads((STAGE/'before-hashes.json').read_text());assert sha(STAGE/'before'/UNIT)==hashes[UNIT]
archive=ROOT/'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a';assert (ROOT/'rust/a2fo_core/src/intro_runtime.rs').stat().st_mtime_ns<=archive.stat().st_mtime_ns,'stale Rust intro archive';archive_hash=sha(archive)
a.wine_prefix.mkdir(parents=True,exist_ok=True);env=os.environ.copy()
for k in ['DISPLAY','WAYLAND_DISPLAY']:env.pop(k,None)
env.update(WINEPREFIX=str(a.wine_prefix.resolve()),WINEDEBUG='-all',WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
with tempfile.TemporaryDirectory(prefix='a2fo-intro-parity-') as tmp:
 outputs=[]
 for before in [True,False]:
  base=STAGE/'before' if before else ROOT;tmp=Path(tmp);name='before' if before else 'rust';fixture=tmp/(name+'.cpp');exe=tmp/(name+'.exe')
  fixture.write_text((ROOT/'tests/rust_stage7_intro_parity.cpp').read_text().replace('../modules/',str(base/'modules')+'/'))
  cmd=['i686-w64-mingw32-g++','-std=gnu++17','-O2','-Wall','-Wextra','-Werror','-static','-UNDEBUG',str(fixture),'-lgdi32','-luser32']
  if not before:cmd+=[str(archive)]
  cmd+=['-o',str(exe)];run(cmd);outputs.append(run(['wine',str(exe)],env))
 if outputs[0]!=outputs[1]:
  x,y=outputs[0].splitlines(),outputs[1].splitlines()
  for i in range(max(len(x),len(y))):
   aa=x[i] if i<len(x) else b'<missing>';bb=y[i] if i<len(y) else b'<missing>'
   if aa!=bb:raise RuntimeError(f'mismatch row {i+1}: {aa!r} != {bb!r}')
assert sha(archive)==archive_hash
receipt={'windows':True,'equal':True,'cases':len(outputs[1].splitlines()),'sha256':hashlib.sha256(outputs[1]).hexdigest(),'archive_sha256':archive_hash,'before_cpp_sha256':sha(STAGE/'before'/UNIT),'current_cpp_sha256':sha(ROOT/UNIT),'fixture_sha256':sha(ROOT/'tests/rust_stage7_intro_parity.cpp'),'offscreen_pixel_assertions_passed':True,'game_or_window_created':False}
if a.receipt:a.receipt.parent.mkdir(parents=True,exist_ok=True);a.receipt.write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt))
