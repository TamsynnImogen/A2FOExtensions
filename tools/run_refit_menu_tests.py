#!/usr/bin/env python3
"""Console DLL fixtures; never runs game entrypoints or creates a game window."""
import argparse
import os
from pathlib import Path
import subprocess

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--prefix',required=True)
    p.add_argument('--fo',type=Path,action='append',help='FO DLL data input; default local and clean reference')
    p.add_argument('--armada',type=Path,help='engine data input; default local ArmadaL.exe')
    a=p.parse_args()
    prefix=Path(a.prefix)
    if not prefix.is_absolute() or not (prefix/'system.reg').is_file():
        p.error('explicit absolute initialized disposable Wine prefix required')
    root=Path(__file__).resolve().parents[1]
    env=dict(os.environ,WINEPREFIX=str(prefix),WINEDEBUG='-all',WINEDLLOVERRIDES='winemenubuilder.exe=d')
    for k in ('DISPLAY','WAYLAND_DISPLAY','STEAM_COMPAT_DATA_PATH'):env.pop(k,None)
    failures=[];total=0
    for fo in (a.fo or [root/'FleetOpsHook.dll',root/'reverse_engineering/FleetOpsHook/FleetOpsHook.dll']):
        for case in ('missing','disabled','invalid','api','identity','armada','profile','foreign','action','patch-fail','skin-signature','skin-patch-fail','success','loading','loading-signature','mission','mission-signature','mission-fo','mission-fo-gateway'):
            total+=1;print(f'{fo} {case}',flush=True)
            try:
                r=subprocess.run(['wine',str(root/'build/refit_menu_native.exe'),str(root/'build/modules/A2FORefitMenus.dll'),str((a.armada or root/'ArmadaL.exe').resolve()),str(fo.resolve()),case],env=env,timeout=90,check=False)
                if r.returncode:failures.append((str(fo),case,r.returncode))
            except (OSError,subprocess.TimeoutExpired) as e:failures.append((str(fo),case,str(e)))
    print(f'{total-len(failures)}/{total} console fixture processes passed',flush=True)
    for f in failures:print('FAIL',f,flush=True)
    return int(bool(failures))
if __name__=='__main__':raise SystemExit(main())
