#!/usr/bin/env python3
"""Revise an owned stopped startup trial for LiveParts diagnostics.

Preserves prior binaries, prefix and logs. Only the existing private trial is
writable inside the GUI-free network/device-isolated sandbox. No user input or
live installation is accessed. A positional map is experimental on Fleet Ops.
"""
import argparse
import json
import os
from pathlib import Path
import re
import shutil
import time
import run_refit_fo_bootstrap as base


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--trial',type=Path,required=True)
    p.add_argument('--core',type=Path,required=True)
    p.add_argument('--helpers',type=Path,required=True)
    p.add_argument('--map',dest='map_name',default='')
    p.add_argument('--launch',action='store_true')
    args=p.parse_args()
    trial=args.trial.absolute()
    if trial.resolve()!=trial or trial.parent!=base.OUTPUT or trial.is_symlink():
        raise ValueError('only an existing canonical private startup trial is allowed')
    receipt=base.regular(trial/'receipt.json')
    old=json.loads(receipt.read_text())
    if old.get('trial')!=str(trial) or old.get('status')!='launch-diagnostic-complete':
        raise ValueError('requires a completed owned startup diagnostic')
    # Detect surviving namespace children independently of their argv: Wine
    # may have outlived its launcher and still hold this bind at /mnt.
    identity=trial.stat()
    for process in Path('/proc').glob('[0-9]*'):
        if int(process.name)==os.getpid():continue
        try:
            mount=(process/'root/mnt').stat()
            if (mount.st_dev,mount.st_ino)==(identity.st_dev,identity.st_ino):
                raise ValueError('trial is still bound by process '+process.name)
        except (FileNotFoundError,PermissionError,ProcessLookupError):pass
        try:
            cwd=os.readlink(process/'cwd')
            if cwd==str(trial) or cwd.startswith(str(trial)+'/'):
                raise ValueError('process still uses trial '+process.name)
        except (FileNotFoundError,PermissionError,ProcessLookupError):pass
    for directory in (trial/'Data',trial/'Data/Mods',trial/'home',trial/'inputs',trial/'run'):
        if directory.is_symlink() or not directory.is_dir() or directory.resolve()!=directory:
            raise ValueError('unsafe private destination parent: '+str(directory))
    if (trial/'revisions').exists() and ((trial/'revisions').is_symlink() or not (trial/'revisions').is_dir()):
        raise ValueError('unsafe revision directory')
    if args.map_name and not re.fullmatch(r'[A-Za-z0-9_-]{1,64}',args.map_name):
        raise ValueError('map must be a simple name')
    inputs={'core':base.checked_candidate(args.core),
        'display_dll':base.checked_candidate(args.helpers/'A2FORefitDisplay.dll'),
        'renderer_dll':base.checked_candidate(args.helpers/'A2FORefitRenderer.dll')}
    if not all(path.is_relative_to(base.ROOT/'build') for path in inputs.values()):
        raise ValueError('candidate inputs must be staged beneath this checkout build directory')
    for name,want in base.EXE_SHA.items():
        if base.sha(base.regular(trial/'Data'/name))!=want:raise ValueError('private executable drift')
    if base.sha(base.regular(trial/'FleetOpsHook.dll'))!=base.HOOK_SHA:raise ValueError('private provider drift')
    old_child=old['child_mod']
    if old_child not in ('FO4 Refit Vulkan Test','FO4 Refit DX12 Test'):
        raise ValueError('unexpected startup child')
    old_paths={'core':'Data/A2FOExtensions.dll','media':'Data/A2FOMedia.dll',
        'display_dll':'Data/Mods/'+old_child+'/modules/refit/A2FORefitDisplay.dll',
        'renderer_dll':'Data/Mods/'+old_child+'/modules/refit/A2FORefitRenderer.dll'}
    for key,want in old['staged_candidate_sha256'].items():
        if base.sha(base.regular(trial/old_paths[key]))!=want:
            raise ValueError('old candidate changed: '+key)
    if shutil.disk_usage(trial).free<8*1024**3:raise ValueError('private prefix backup reserve unavailable')
    revision=trial/'revisions'/('live-'+time.strftime('%Y%m%dT%H%M%SZ',time.gmtime())+'-'+str(os.getpid()))
    revision.mkdir(parents=True,exist_ok=False)
    state={'status':'preparing','trial':str(trial),'revision':str(revision),
        'previous_receipt_sha256':base.sha(receipt),'source_qualification':'Private diagnostic only; no gameplay acceptance',
        'inputs':{k:{'path':str(v),'sha256':base.sha(v)} for k,v in inputs.items()},
        'input_automation':False,'map_argument_experimental':args.map_name}
    def save():(revision/'receipt.json').write_text(json.dumps(state,indent=2)+'\n')
    save()
    try:
        base.copy_verified(receipt,revision/'startup-receipt.json')
        # Preserve every prior diagnostic and initialized private prefix.
        for name in ('logs','prefix'):
            target=trial/name
            if target.is_symlink() or not target.is_dir():raise ValueError('unsafe private directory')
            target.rename(revision/name)
            target.mkdir(mode=0o700)
        core=base.regular(trial/'Data/A2FOExtensions.dll')
        state['old_core_sha256']=base.copy_verified(core,revision/'A2FOExtensions.dll')
        base.copy_verified(inputs['core'],core)
        name='FO4 Refit Live '+('Vulkan' if 'Vulkan' in old['child_mod'] else 'DX12')+' Test'
        child=trial/'Data/Mods'/name
        child.mkdir(exist_ok=False)
        helpers=child/'modules/refit';helpers.mkdir(parents=True)
        for key,filename in [('display_dll','A2FORefitDisplay.dll'),('renderer_dll','A2FORefitRenderer.dll')]:
            base.copy_verified(inputs[key],helpers/filename)
        backend='vulkan' if 'Vulkan' in name else 'd3d12'
        (child/'info.ini').write_bytes(('[mod]\r\nName='+name+'\r\nAssetVersion=50000\r\nVersion=0.2-test\r\n'
            'ParentMod="FO4 Nexus"\r\nstandalone=0\r\nSettingsDirectory="Refit Live '+revision.name+'"\r\n'
            '\r\n[modules]\r\nreject0="A2FORefitMenus"\r\n').encode('ascii'))
        (child/'A2FORenderer.ini').write_bytes(('[D3D8Compat]\r\nBackend=refit-'+backend+'-test\r\n'
            '\r\n[Refit]\r\nLiveParts=1\r\nCaptureScene=1\r\nCaptureAssets=1\r\nCaptureTextures=1\r\n'
            'CaptureMaterials=1\r\nCaptureNodes=1\r\n').encode('ascii'))
        state['child_mod']=name
        # Fresh Data logs: retain any original bytes in the revision.
        for log in (*base.LOG_NAMES,'A2FORefitRenderer.log'):
            path=trial/'Data'/log
            if path.exists():
                base.regular(path).rename(revision/log)
        base.seed_fixture_prefix(trial,state,save,
            base.ROOT/'build/refit-host-checks/20261106T023433Z/2672721/vulkan/prefix')
        if args.map_name:
            old_boot=base.BOOT
            token="'/nointro']"
            if old_boot.count(token)!=1:raise ValueError('bootstrap command drift')
            base.BOOT=old_boot.replace(token,"'/nointro',"+repr(args.map_name)+"]")
        state['status']='staged';save()
        if args.launch:
            base.launch(trial,state,save)
        state['final_core_sha256']=base.sha(core)
        state['status']='diagnostic-complete' if args.launch else 'staged';save()
        print('Private LiveParts diagnostic:',revision/'receipt.json')
    except Exception as error:
        state['status']='failed';state['error']=str(error);save();raise

if __name__=='__main__':main()
