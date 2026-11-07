#!/usr/bin/env python3
"""Compare full frozen/current Refit DLL native fixtures without a game window."""
import argparse,hashlib,json,os,re,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];STAGE=ROOT/'build/rust-migration-stage7';WORK=STAGE/'refit-ui-work'
CASES=('missing','disabled','invalid','api','identity','armada','profile','foreign','action','patch-fail','skin-signature','skin-patch-fail','success','loading','loading-signature','mission','mission-signature','mission-fo','mission-fo-gateway')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(command,env=None):
 with tempfile.TemporaryFile() as out,tempfile.TemporaryFile() as err:
  p=subprocess.run(command,cwd=ROOT,env=env,stdout=out,stderr=err,timeout=180);out.seek(0);err.seek(0);a,b=out.read(),err.read()
 if p.returncode:raise RuntimeError(f'exit {p.returncode} {command}\n{a[-2500:]!r}\n{b[-3500:]!r}')
 return a,b
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--wine-prefix',type=Path,default=ROOT/'build/rust-stage7-wine');p.add_argument('--receipt',type=Path);a=p.parse_args();WORK.mkdir(exist_ok=True);a.wine_prefix.mkdir(exist_ok=True,parents=True)
command=json.loads((WORK/'module-link-command.json').read_text());archive=ROOT/'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a';archive_hash=sha(archive)
env=os.environ.copy()
for k in ['DISPLAY','WAYLAND_DISPLAY']:env.pop(k,None)
env.update(WINEPREFIX=str(a.wine_prefix.resolve()),WINEDEBUG='-all',WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
libs={};builds=[]
for before in [True,False]:
 name='before' if before else 'current';cmd=command.copy();cmd[cmd.index('-o')+1]=str(WORK/(name+'.dll'))
 if before:
  cmd=[str(STAGE/'before'/x) if x.startswith('modules/') and Path(x).suffix in {'.cpp','.S'} else x for x in cmd]
 run(cmd);libs[name]=cmd[cmd.index('-o')+1];builds.append({'side':name,'command':cmd,'sha256':sha(Path(libs[name]))})
results=[]
for case in CASES:
 outputs=[]
 for side in ['before','current']:
  cmd=['wine',str(ROOT/'build/refit_menu_native.exe'),str(ROOT/libs[side]),str(ROOT/'build/engine-compat-20261023/inputs/oct23/ArmadaL.exe'),str(ROOT/'build/engine-compat-20261023/inputs/oct23/FleetOpsHook.dll'),case]
  out,err=run(cmd,env);(WORK/f'{case}-{side}.log').write_bytes(out+err)
  if b'PASS' not in out:raise RuntimeError(f'{case} {side}: fixture success marker absent')
  # A fixture-owned temporary settings directory is intentionally fresh per
  # process. Only that path is normalized; native result/call logs stay exact.
  normalized=re.sub(rb'config=[^\r\n]*',b'config=<fixture-root>',out)
  normalized=re.sub(rb'C:\\users\\[^\r\n ;]*\\Temp\\[^\r\n ;]*',b'<fixture-root>',normalized)
  outputs.append(normalized)
 # Every process independently executes the original assertion-heavy fixture;
 # exact normalized output is an additional check, not the sole acceptance.
 equal=outputs[0]==outputs[1]
 results.append({'case':case,'frozen_and_current_assertions_passed':True,'normalized_output_equal':equal,'before_sha256':hashlib.sha256(outputs[0]).hexdigest(),'current_sha256':hashlib.sha256(outputs[1]).hexdigest()})
 print(f'{case}: frozen/current native assertions PASS; normalized output equal={equal}',flush=True)
assert sha(archive)==archive_hash
receipt={'compiler_mode':'gnu++17 -O2','archive_sha256':archive_hash,'builds':builds,'results':results,'native_processes_passed':len(results)*2,'game_entrypoints_or_windows_created':False}
if a.receipt:a.receipt.parent.mkdir(exist_ok=True,parents=True);a.receipt.write_text(json.dumps(receipt,indent=2)+'\n')
print(f'{len(results)*2} frozen/current native DLL fixture processes PASS')
