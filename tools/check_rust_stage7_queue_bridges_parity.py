#!/usr/bin/env python3
"""Compare complete queue-bridge registration and dispatch against frozen sources."""
import argparse, hashlib, json, os, subprocess, tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
UNITS=['modules/A2FOFeaturePack/refit_queue_bridge_client.cpp','modules/A2FOFeaturePack/hybrid_bridge_client.cpp']
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd,env=None):
 with tempfile.TemporaryFile() as out,tempfile.TemporaryFile() as err:
  p=subprocess.run(cmd,cwd=ROOT,env=env,stdout=out,stderr=err,timeout=180)
  out.seek(0);err.seek(0);a,b=out.read(),err.read()
 if p.returncode:raise RuntimeError(f'{cmd}: exit {p.returncode}\n{a[-1000:]!r}\n{b[-3000:]!r}')
 return a
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--windows',action='store_true');p.add_argument('--wine-prefix',type=Path,default=ROOT/'build/rust-stage7-wine');p.add_argument('--receipt',type=Path);a=p.parse_args()
stage=ROOT/'build/rust-migration-stage7';hashes=json.loads((stage/'before-hashes.json').read_text());archive=ROOT/'build/rust'/('i686-pc-windows-gnu/release' if a.windows else 'release')/'liba2fo_core.a'
for source in UNITS:assert digest(stage/'before'/source)==hashes[source],source
for source in [ROOT/'rust/a2fo_core/src/queue_bridges.rs']:assert source.stat().st_mtime_ns<=archive.stat().st_mtime_ns,'stale queue-bridge implementation in archive'
archive_hash=digest(archive);env=os.environ.copy();runner=[]
if a.windows:
 a.wine_prefix.mkdir(parents=True,exist_ok=True)
 for k in ['DISPLAY','WAYLAND_DISPLAY']:env.pop(k,None)
 env.update(WINEPREFIX=str(a.wine_prefix.resolve()),WINEDEBUG='-all',WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d');runner=['wine']
with tempfile.TemporaryDirectory(prefix='a2fo-queue-bridges-') as tmp:
 tmp=Path(tmp);outputs=[]
 if not a.windows:
  (tmp/'windows.h').write_text('#pragma once\n#include <stdint.h>\nusing LONG=int32_t;\ninline LONG InterlockedCompareExchange(volatile LONG* p,LONG x,LONG c){return __sync_val_compare_and_swap(p,c,x);}\ninline LONG InterlockedExchange(volatile LONG* p,LONG x){return __sync_lock_test_and_set(p,x);}\n')
 for before in [True,False]:
  base=stage/'before' if before else ROOT;exe=tmp/(('before' if before else 'rust')+('.exe' if a.windows else ''));cmd=['i686-w64-mingw32-g++' if a.windows else 'g++','-std=gnu++17','-O2','-Wall','-Wextra','-Werror','-I',str(base/'modules/A2FOFeaturePack')]
  if a.windows:cmd+=['-static']
  else:cmd+=['-D__cdecl=','-D__declspec(x)=','-I',str(tmp)]
  cmd+=[str(ROOT/'tests/rust_stage7_queue_bridges_parity.cpp'),*[str(base/u) for u in UNITS]]
  if not before:cmd+=[str(archive)]
  cmd+=['-o',str(exe)];run(cmd);outputs.append(run(runner+[str(exe)],env))
 if outputs[0]!=outputs[1]:
  for i,(x,y) in enumerate(zip(outputs[0].splitlines(),outputs[1].splitlines())):
   if x!=y:raise RuntimeError(f'mismatch row {i}: {x!r} != {y!r}')
  raise RuntimeError('output lengths differ')
assert digest(archive)==archive_hash
receipt={'windows':a.windows,'cases':len(outputs[1].splitlines()),'equal':True,'sha256':hashlib.sha256(outputs[1]).hexdigest(),'archive_sha256':archive_hash,'units':UNITS,'fixture_sha256':digest(ROOT/'tests/rust_stage7_queue_bridges_parity.cpp')}
if a.receipt:a.receipt.parent.mkdir(parents=True,exist_ok=True);a.receipt.write_text(json.dumps(receipt,indent=2)+'\n')
print(json.dumps(receipt))
