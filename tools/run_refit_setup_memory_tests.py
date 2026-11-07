#!/usr/bin/env python3
"""Memory DC/USER32-stub checks; no game, desktop windows or input automation."""
import argparse
import os
from pathlib import Path
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prefix',type=Path,required=True)
    args=parser.parse_args()
    prefix=args.prefix.resolve()
    if not prefix.name.startswith('a2fo-refit-native-prefix-') or not (prefix/'system.reg').is_file():
        parser.error('initialized dedicated a2fo-refit-native-prefix-* prefix required')
    root=Path(__file__).resolve().parents[1]
    env=dict(os.environ,WINEPREFIX=str(prefix),WINEDEBUG='-all',WINEDLLOVERRIDES='winemenubuilder.exe=d')
    for name in ('DISPLAY','WAYLAND_DISPLAY','STEAM_COMPAT_DATA_PATH'):
        env.pop(name,None)
    log=root/'build/refit-instant-action-memory-tests.log'
    with log.open('w') as out:
        for name in ('shell_canvas_copy_test','shell_canvas_lifecycle_test','shell_canvas_dialog_test','shell_canvas_scroll_test','refit_setup_render'):
            command=['wine',str(root/'build'/f'{name}.exe')]
            if name=='refit_setup_render':
                command.extend(str(root/'build'/f'refit-setup-{width}.bmp') for width in (1920,1280))
            out.write(name+'\n');out.flush()
            result=subprocess.run(command,env=env,stdout=out,stderr=subprocess.STDOUT,timeout=90)
            if result.returncode:
                print(f'FAIL: {name}: {result.returncode}; evidence: {log}')
                return 1
    print(f'PASS: canvas copy/presenter, lifecycle, dialog, scrolling and setup offscreen rendering; {log}')
    return 0


if __name__=='__main__':
    raise SystemExit(main())
