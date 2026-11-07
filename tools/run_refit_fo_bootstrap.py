#!/usr/bin/env python3
"""Stage a private FO4 Refit startup diagnostic; launch only with --launch.

The trial is always new. No source installation, desktop session, input device,
or network is made available to the sandboxed game process.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import select
import shutil
import stat
import subprocess
import sys
import tempfile
import time

from check_refit_fo_profile import inspect as inspect_fo_image

ROOT = Path(__file__).resolve().parents[1]
SOURCE = Path('/home/tamsynn/NVMeData/Projects/A2FO-Native11-Private/Data')
OUTPUT = Path('/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials')
XVFB = Path('/home/tamsynn/Applications/iStripper/tools/usr/bin/Xvfb')
WINE = Path('/usr/bin/wine').resolve()
WINESERVER = Path('/usr/bin/wineserver').resolve()
ICD = Path('/usr/share/vulkan/icd.d/lvp_icd.x86_64.json')
PROTON = Path('/home/tamsynn/.var/app/ru.linux_gaming.PortProton/data/dist/PROTON_LG_10-28')
RUNTIME = 'system'
HOOK_SHA = '3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11'
CORE_SHA = '8a88889ae4eb039ff67b092b006c684e5257c266c47893736261ebb06f258107'
MEDIA_SHA = 'a9dddb904a067ba25e90c2f09d3682c31eb2d64b8f92e61584a32b4b8b852f19'
DISPLAY_SHA = '6c09e505ea512fb763f1fe2ba08cdcd7210ba0aa266688820a16443e34b7b804'
RENDERER_SHA = 'd441ce6653ee392921bc1798ae0f48333dfb9a1163002bebd39cdfa026a34886'
EXE_SHA = {
    'ArmadaL.exe': '4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de',
    'Mods/Fleet Ops 4.0/ArmadaL.exe': 'c8846226048931134e07d48702309b2fec1ef0f67f5b4d82bec0eb5249257d8c',
}
LOG_NAMES = ('A2FOExtensions.log', 'A2FOStartupProxy.log', 'A2FORenderer.log',
             'A2FORefitDisplay.log')


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def regular(path):
    path = Path(path)
    if path.is_symlink() or not path.is_file() or not stat.S_ISREG(path.lstat().st_mode):
        raise ValueError(f'regular non-link file required: {path}')
    return path


def safe_source():
    if SOURCE.is_symlink() or not SOURCE.is_dir():
        raise ValueError('private Data source is missing or linked')
    real = SOURCE.resolve(strict=True)
    entries = files = bytes_total = links = 0
    for base, dirs, names in os.walk(SOURCE, followlinks=False):
        for name in dirs + names:
            item = Path(base) / name
            mode = item.lstat().st_mode
            entries += 1
            if stat.S_ISLNK(mode):
                raw = os.readlink(item)
                if os.path.isabs(raw):
                    raise ValueError(f'absolute source link refused: {item}')
                target = item.resolve(strict=True)
                if not target.is_relative_to(real) or not (target.is_file() or target.is_dir()):
                    raise ValueError(f'escaping or special source link refused: {item}')
                links += 1
            elif stat.S_ISREG(mode):
                files += 1
                bytes_total += item.stat().st_size
            elif not stat.S_ISDIR(mode):
                raise ValueError(f'special source node refused: {item}')
    return {'entries': entries, 'regular_files': files,
            'regular_bytes': bytes_total, 'internal_relative_links': links}


def checked_candidate(path):
    path = Path(path).absolute()
    if '..' in path.parts:
        raise ValueError(f'parent traversal in candidate: {path}')
    return regular(path)


def copy_verified(source, target):
    shutil.copy2(source, target)
    expected = sha(source)
    if sha(target) != expected:
        raise RuntimeError(f'copy hash mismatch: {target}')
    return expected


def sandbox(trial, display=None):
    args = ['bwrap', '--die-with-parent', '--new-session', '--unshare-user',
            '--unshare-pid', '--unshare-net', '--ro-bind', '/', '/',
            '--bind', str(trial), '/mnt', '--bind', str(trial / 'home'),
            '/home/tamsynn', '--tmpfs', '/tmp', '--dev', '/dev',
            '--proc', '/proc', '--tmpfs', '/run/user', '--tmpfs', '/run/dbus',
            '--chdir', '/mnt/Data']
    if RUNTIME == 'portproton':
        args += ['--ro-bind', str(PROTON), '/opt',
                 '--ro-bind', str(PROTON), str(PROTON)]
    if display is not None:
        args += ['--dir', '/tmp/.X11-unix', '--ro-bind',
                 f'/tmp/.X11-unix/X{display}', f'/tmp/.X11-unix/X{display}']
    return args


PROBE = r'''
import json,pathlib
p=pathlib.Path
assert p('/mnt/Data/ArmadaL.exe').is_file()
assert p('/mnt/FleetOpsHook.dll').is_file()
assert not p('/dev/input').exists() and not list(p('/dev').glob('hidraw*'))
assert not p('/dev/dri').exists() and not p('/dev/snd').exists()
assert not p('/home/tamsynn/NVMeData').exists()
assert not p('/home/tamsynn/.codex').exists()
assert not p('/run/user/1000/bus').exists()
assert len(p('/proc/net/route').read_text().splitlines()) == 1
assert any(x.split()[1]=='/' and 'ro' in x.split()[3].split(',')
           for x in p('/proc/mounts').read_text().splitlines())
print(json.dumps({'isolated':True,'root_read_only':True,'host_home_hidden':True,
 'input_devices_hidden':True,'hardware_graphics_hidden':True,
 'session_bus_hidden':True,'external_routes':False}))
'''

BOOT = r'''
import hashlib,json,os,pathlib,subprocess,time
p=pathlib.Path
progress=p('/mnt/logs/bootstrap-progress.json')
child=os.environ['A2FO_REFIT_CHILD']
state={'phase':'wineboot','launcher_invoked':False,'game_process_seen':False,
       'input_automation':False,'observations':[]}
def save():progress.write_text(json.dumps(state,indent=2)+'\n')
def digest(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for chunk in iter(lambda:f.read(1048576),b''):h.update(chunk)
 return h.hexdigest()
def logs():
 result={}
 for name in ('A2FOExtensions.log','A2FOStartupProxy.log','A2FORenderer.log','A2FORefitDisplay.log'):
  path=p('/mnt/Data')/name
  if path.is_file():
   try:
    with path.open('rb') as f:
     f.seek(max(0,path.stat().st_size-8192));tail=f.read().decode('utf-8','replace')
    result[name]={'size':path.stat().st_size,'sha256':digest(path),'tail':tail[-8192:]}
   except OSError as exc:result[name]={'read_error':str(exc)}
 return result
save()
try:
 with p('/mnt/logs/wineboot.log').open('wb') as out:
  if os.environ.get('A2FO_REFIT_PREFIX_SEEDED') == '1':
   state['wineboot_skipped_for_initialized_fixture_prefix']=True
   boot=subprocess.CompletedProcess([],0)
  else:
   try:boot=subprocess.run([os.environ['WINELOADER'],'wineboot.exe','-u'],stdin=subprocess.DEVNULL,
                           stdout=out,stderr=subprocess.STDOUT,timeout=120)
   except subprocess.TimeoutExpired:
    state['phase']='wineboot-timeout';save();raise SystemExit(2)
 state['wineboot_exit']=boot.returncode
 if boot.returncode:
  state['phase']='wineboot-failed';save();raise SystemExit(2)
 command=[os.environ['WINELOADER'],r'Z:\mnt\Data\ArmadaL.exe','/mod',child,'/nointro']
 state['game_command']=command;save()
 with p('/mnt/logs/game-process.log').open('wb') as out:
  game=subprocess.Popen(command,stdin=subprocess.DEVNULL,stdout=out,
                        stderr=subprocess.STDOUT,cwd='/mnt/Data')
  state['launcher_invoked']=True;state['phase']='game-observation';save()
  start=time.monotonic()
  observed_pids=set()
  while time.monotonic()-start<40:
   time.sleep(min(2,max(0,40-(time.monotonic()-start))))
   armada=[]
   for proc in p('/proc').iterdir():
    if not proc.name.isdigit():continue
    try:
     if (proc/'comm').read_text().strip().lower()=='armadal.exe':
      armada.append(int(proc.name))
    except OSError:pass
   observed_pids.update(armada)
   state['game_process_seen']=bool(observed_pids)
   state['observations'].append({'elapsed_seconds':round(time.monotonic()-start,1),
                                  'launcher_alive':game.poll() is None,
                                  'armada_pids':sorted(armada),'logs':logs()})
   save()
  try:
   capture=subprocess.run(['ffmpeg','-nostdin','-loglevel','error','-y',
        '-f','x11grab','-video_size','1280x1024','-i',os.environ['DISPLAY'],
        '-frames:v','1','-threads','1','/mnt/logs/private-root.png'],
        stdin=subprocess.DEVNULL,stdout=subprocess.DEVNULL,stderr=subprocess.PIPE,timeout=8)
   state['private_capture_exit']=capture.returncode
   state['private_capture_error']=capture.stderr.decode(errors='replace')[-1000:]
  except (OSError,subprocess.TimeoutExpired) as exc:
   state['private_capture_error']=str(exc)
  state['alive_at_limit']=game.poll() is None
  state['armada_pids_ever_seen']=sorted(observed_pids)
  state['armada_pids_at_limit']=state['observations'][-1]['armada_pids'] if state['observations'] else []
  state['armada_presence_at_limit']=bool(state['armada_pids_at_limit'])
  state['armada_pids_exited_before_limit']=sorted(observed_pids-set(state['armada_pids_at_limit']))
  if game.poll() is None:
   game.terminate()
   try:game.wait(timeout=5)
   except subprocess.TimeoutExpired:game.kill();game.wait(timeout=5)
  state['game_exit']=game.returncode
 state['phase']='finished';state['final_logs']=logs();save()
finally:
 try:
  state['wineserver_cleanup_exit']=subprocess.run([os.environ['WINESERVER'],'-k'],
       stdin=subprocess.DEVNULL,timeout=8,check=False).returncode
 except subprocess.TimeoutExpired:state['wineserver_cleanup_timeout']=True
 save()
'''


def stage(args):
    inputs = {name: checked_candidate(path) for name, path in (
        ('core', args.core), ('display_dll', args.display_dll),
        ('renderer_dll', args.renderer_dll))}
    if args.media:
        inputs['media'] = checked_candidate(args.media)
    if sha(inputs['core']) != CORE_SHA:
        raise ValueError('candidate Core hash differs from frozen checkpoint')
    if args.media and sha(inputs['media']) != MEDIA_SHA:
        raise ValueError('candidate Media hash differs from frozen checkpoint')
    if sha(inputs['display_dll']) != DISPLAY_SHA:
        raise ValueError('Display helper hash differs from frozen checkpoint')
    if sha(inputs['renderer_dll']) != RENDERER_SHA:
        raise ValueError('Renderer helper hash differs from frozen checkpoint')
    hook = regular(SOURCE.parent / 'FleetOpsHook.dll')
    if sha(hook) != HOOK_SHA:
        raise ValueError('parent FleetOpsHook.dll hash differs from pinned image')
    profiles = {name: inspect_fo_image(regular(SOURCE / name)) for name in EXE_SHA}
    for name, expected in EXE_SHA.items():
        if profiles[name]['sha256'] != expected:
            raise ValueError(f'unexpected reviewed FO executable: {name}')
    tree = safe_source()
    source_configs = {name:sha(regular(SOURCE/name)) for name in (
        'A2FOExtensions.dll','Mods/FO4 Nexus/info.ini',
        'Mods/FO4 Nexus/A2FORenderer.ini')}
    if OUTPUT.parent.is_symlink() or not OUTPUT.parent.is_dir():
        raise ValueError('trial parent must be an existing ordinary directory')
    if os.stat(SOURCE).st_dev != os.stat(OUTPUT.parent).st_dev:
        raise ValueError('trial destination must share source filesystem')
    free = shutil.disk_usage(OUTPUT.parent).free
    required = tree['regular_bytes'] + 16 * 1024**3
    if free < required:
        raise ValueError(f'insufficient trial capacity: {free} free, {required} required')
    filesystem = subprocess.run(['findmnt','-n','-o','FSTYPE','--target',str(OUTPUT.parent)],
                                capture_output=True,text=True,check=True).stdout.strip()
    if not filesystem:
        raise ValueError('cannot identify trial filesystem')
    OUTPUT.mkdir(parents=True, exist_ok=True)
    if OUTPUT.is_symlink() or OUTPUT.resolve() != OUTPUT:
        raise ValueError('output root must be a canonical directory')
    trial = OUTPUT / (time.strftime('%Y%m%dT%H%M%SZ', time.gmtime()) +
                      f'-{os.getpid()}-{time.time_ns()}')
    trial.mkdir(mode=0o700, exist_ok=False)
    manifest = {'status':'staging','trial':str(trial),'source':str(SOURCE),
                'source_tree':tree,'source_executables':profiles,
                'capacity':{'free_bytes_before':free,'required_bytes':required,
                            'headroom_bytes':16*1024**3,'filesystem':filesystem,
                            'source_device':os.stat(SOURCE).st_dev,
                            'trial_device':os.stat(OUTPUT.parent).st_dev},
                'source_parent_hook':{'path':str(hook),'sha256':HOOK_SHA},
                'source_config_sha256':source_configs,
                'inputs':{name:{'path':str(path),'sha256':sha(path)}
                          for name,path in inputs.items()},
                'backend':args.backend,'launch_requested':args.launch,
                'game_launched':False,'input_automation':False,
                'source_writes':False,'scope':'private startup diagnostic only',
                'runner_sha256':sha(Path(__file__)),
                'wine':{'path':str(WINE),'sha256':sha(WINE)}}
    receipt = trial / 'receipt.json'
    def save(): receipt.write_text(json.dumps(manifest,indent=2)+'\n')
    save()
    try:
        data = trial / 'Data'
        command=['cp','-a','--reflink=auto','--',str(SOURCE)+'/.',str(data)]
        manifest['copy_command']=command
        save()
        result = subprocess.run(command,stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,text=True)
        if result.returncode:
            manifest['status']='clone-failed'
            manifest['clone_error']=result.stderr[-4000:]
            save()
            raise RuntimeError('trial clone failed; partial new trial retained')
        manifest['clone_method']='ordinary-copy' if filesystem == 'ext4' else 'reflink-auto-unresolved'
        manifest['clone_filesystem']=filesystem
        manifest['no_source_target_hardlinks']=all(
            os.stat(SOURCE/name).st_ino != os.stat(data/name).st_ino
            for name in EXE_SHA)
        if not manifest['no_source_target_hardlinks']:
            raise RuntimeError('cloned executable unexpectedly shares source inode')
        for name, expected in EXE_SHA.items():
            if sha(data / name) != expected:
                raise RuntimeError(f'cloned FO executable mismatch: {name}')
        copy_verified(hook,trial/'FleetOpsHook.dll')
        (trial/'inputs').mkdir()
        (trial/'logs').mkdir()
        (trial/'home').mkdir()
        (trial/'prefix').mkdir()
        (trial/'run').mkdir()
        previous = regular(data/'A2FOExtensions.dll')
        previous_hash = copy_verified(previous,trial/'inputs/copied-core-prior.dll')
        manifest['copied_core_prior_sha256']=previous_hash
        mutations = []
        def replace(source,target,label):
            if target.is_symlink() or (target.exists() and not stat.S_ISREG(target.lstat().st_mode)):
                raise ValueError(f'unsafe trial replacement target: {target}')
            before = sha(target) if target.exists() else None
            descriptor,name = tempfile.mkstemp(prefix='.refit-stage-',dir=target.parent)
            os.close(descriptor)
            temporary=Path(name)
            try:
                after=copy_verified(source,temporary)
                os.replace(temporary,target)
                if sha(target) != after:
                    raise RuntimeError(f'atomic stage hash mismatch: {target}')
            finally:
                if temporary.exists(): temporary.unlink()
            mutations.append({'path':str(target.relative_to(trial)),
                              'kind':label,'before_sha256':before,'after_sha256':after})
        replace(inputs['core'],data/'A2FOExtensions.dll','candidate-core')
        if args.media:
            replace(inputs['media'],data/'A2FOMedia.dll','candidate-media')
        child_name = ('FO4 Refit Vulkan Test' if args.backend == 'vulkan'
                      else 'FO4 Refit DX12 Test')
        mods = data/'Mods'
        if mods.is_symlink() or not mods.is_dir():
            raise ValueError('copied Mods directory is unsafe')
        child = mods/child_name
        child.mkdir(exist_ok=False)
        helper = child/'modules/refit'
        helper.mkdir(parents=True)
        replace(inputs['display_dll'],helper/'A2FORefitDisplay.dll','display-helper')
        replace(inputs['renderer_dll'],helper/'A2FORefitRenderer.dll','renderer-helper')
        settings = 'A2FO Refit Diagnostic '+trial.name
        info = ('[mod]\r\nName='+child_name+'\r\n'
                'AssetVersion=50000\r\nVersion=0.1-test\r\n'
                'ParentMod="FO4 Nexus"\r\nstandalone=0\r\n'
                'SettingsDirectory="'+settings+'"\r\n\r\n'
                '[modules]\r\nreject0 = "A2FORefitMenus"\r\n').encode('ascii')
        config = ('[D3D8Compat]\r\nBackend=refit-'+
                  ('vulkan' if args.backend=='vulkan' else 'd3d12')+
                  '-test\r\n').encode('ascii')
        for name, contents in (('info.ini',info),('A2FORenderer.ini',config)):
            path=child/name
            path.write_bytes(contents)
            mutations.append({'path':str(path.relative_to(trial)),'kind':'new-child-config',
                              'before_sha256':None,'after_sha256':sha(path)})
        manifest['child_mod']=child_name
        manifest['mutations']=mutations
        staged = {'core':'Data/A2FOExtensions.dll',
                  'display_dll':str((helper/'A2FORefitDisplay.dll').relative_to(trial)),
                  'renderer_dll':str((helper/'A2FORefitRenderer.dll').relative_to(trial))}
        if args.media:
            staged['media']='Data/A2FOMedia.dll'
        manifest['staged_candidate_sha256']={name:sha(trial/rel) for name,rel in staged.items()}
        for name,digest in manifest['staged_candidate_sha256'].items():
            if digest != manifest['inputs'][name]['sha256']:
                raise RuntimeError(f'staged candidate differs from manifest input: {name}')
        manifest['source_critical_after_sha256']={name:sha(SOURCE/name) for name in EXE_SHA}
        manifest['source_parent_hook_after_sha256']=sha(hook)
        manifest['source_config_after_sha256']={name:sha(SOURCE/name) for name in source_configs}
        if (manifest['source_critical_after_sha256'] != EXE_SHA or sha(hook) != HOOK_SHA
                or manifest['source_config_after_sha256'] != source_configs):
            raise RuntimeError('source critical hash changed during staging')
        manifest['status']='staged'
        save()
        return trial,manifest,save
    except Exception as exc:
        manifest['status']='failed'
        manifest['error']=str(exc)
        save()
        raise


def launch(trial,manifest,save):
    if not all(path.is_file() for path in (XVFB,WINE,WINESERVER,ICD)):
        raise ValueError('private launch tools or software Vulkan ICD missing')
    if not shutil.which('bwrap'):
        raise ValueError('bubblewrap missing')
    env={'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','USER':'tamsynn',
         'LOGNAME':'tamsynn','HOME':'/home/tamsynn','WINEPREFIX':'/mnt/prefix',
         'WINEPATH':r'Z:\mnt;Z:\mnt\Data','WINEDEBUG':'-all',
         'WINELOADER':('/opt/bin-wow64/wine' if RUNTIME == 'portproton' else str(WINE)),
         'WINESERVER':('/opt/bin-wow64/wineserver' if RUNTIME == 'portproton' else str(WINESERVER)),
         'WINEESYNC':'0','WINEFSYNC':'0','XDG_RUNTIME_DIR':'/mnt/run',
         'LIBGL_ALWAYS_SOFTWARE':'1','GALLIUM_DRIVER':'llvmpipe',
         'VK_ICD_FILENAMES':str(ICD),'A2FO_REFIT_CHILD':manifest['child_mod'],
         'A2FO_REFIT_PREFIX_SEEDED':'1' if manifest.get('prefix_seed') else '0',
         'WINEDLLOVERRIDES':'winemenubuilder.exe=d;winedbg.exe=d;'
                            'winewayland.drv=d;mscoree=d;mshtml=d;'
                            'winepulse.drv=d;winealsa.drv=d;'
                            'd3d8=n,b;d3d9=n,b;D3DX81ab=n,b;d3dx9_43=n,b'}
    probe=subprocess.run(sandbox(trial)+['/usr/bin/python3','-c',PROBE],
                         env=env,capture_output=True,text=True,timeout=20)
    (trial/'logs/isolation-probe.log').write_text(probe.stdout+probe.stderr)
    if probe.returncode:
        manifest['status']='isolation-failed';save()
        raise RuntimeError('private isolation assertions failed')
    manifest['isolation']=json.loads(probe.stdout)
    with (trial/'logs/xvfb.log').open('wb') as out:
        server=subprocess.Popen([str(XVFB),'-displayfd','1','-screen','0',
             '1280x1024x24','-nolisten','tcp'],stdin=subprocess.DEVNULL,
             stdout=subprocess.PIPE,stderr=out,start_new_session=True)
    try:
        if not select.select([server.stdout],[],[],20)[0]:
            raise RuntimeError('private Xvfb did not allocate display')
        display=server.stdout.readline().decode().strip()
        if not display.isdigit() or server.poll() is not None:
            raise RuntimeError('private Xvfb allocation failed')
        env['DISPLAY']=':'+display
        manifest['private_display']=env['DISPLAY'];save()
        with (trial/'logs/sandbox-bootstrap.log').open('wb') as out:
            process=subprocess.Popen(sandbox(trial,display)+['/usr/bin/python3','-c',BOOT],
                                     env=env,stdin=subprocess.DEVNULL,
                                     stdout=out,stderr=subprocess.STDOUT,start_new_session=True)
            try:manifest['sandbox_exit']=process.wait(timeout=180)
            except subprocess.TimeoutExpired:
                process.kill();process.wait()
                manifest['sandbox_timeout']=True
        progress=trial/'logs/bootstrap-progress.json'
        if progress.is_file():
            manifest['progress']=json.loads(progress.read_text())
            manifest['game_launched']=manifest['progress'].get('launcher_invoked',False)
        progress_data=manifest.get('progress',{})
        if (manifest.get('sandbox_exit') != 0 or
                progress_data.get('wineboot_exit') != 0 or
                progress_data.get('phase') == 'wineboot-timeout' or
                manifest.get('sandbox_timeout')):
            manifest['status']='launch-failed'
        elif not progress_data.get('game_process_seen'):
            manifest['status']='launch-failed-no-armada-observed'
        else:
            manifest['status']='launch-diagnostic-complete'
    finally:
        server.terminate()
        try:server.wait(timeout=5)
        except subprocess.TimeoutExpired:server.kill();server.wait(timeout=5)
        manifest['xvfb_reaped']=server.poll() is not None
        save()
    if manifest.get('status', '').startswith('launch-failed'):
        raise RuntimeError('private launch failed; inspect trial receipt and logs')


def seed_fixture_prefix(trial, manifest, save, source):
    if RUNTIME != 'system':
        raise ValueError('the recorded fixture prefix belongs to system Wine')
    source=source.absolute()
    allowed=ROOT/'build/refit-host-checks/20261106T023433Z/2672721/vulkan/prefix'
    if source != allowed or source.resolve() != source or source.is_symlink():
        raise ValueError('only the recorded successful private fixture prefix is allowed')
    receipt=regular(source.parent.parent/'receipt.json')
    evidence=json.loads(receipt.read_text())
    if (evidence.get('status') != 'pass' or
            evidence['backends']['vulkan']['prefix'] != str(source)):
        raise ValueError('prefix does not have the passing recorded fixture receipt')
    target=trial/'prefix'
    if target.is_symlink() or any(target.iterdir()):
        raise ValueError('seed destination must be an empty owned prefix')
    required=['system.reg','user.reg','drive_c/windows/system32/kernel32.dll',
              'drive_c/windows/syswow64/kernel32.dll']
    for rel in required:regular(source/rel)
    hashes={}
    for base, dirs, files in os.walk(source,followlinks=False):
        for name in files:
            src=Path(base)/name
            if src.is_symlink():continue
            regular(src)
            hashes[str(src.relative_to(source))]=sha(src)
    shutil.copytree(source,target,symlinks=True,dirs_exist_ok=True)
    for rel,h in hashes.items():
        if sha(source/rel) != h or sha(target/rel) != h:
            raise ValueError('fixture prefix changed during independent copy: '+rel)
    manifest['prefix_seed']={'path':str(source),'receipt_sha256':sha(receipt),
                             'file_sha256':hashes,
                             'qualification':'Fresh private headless fixture prefix; no user game prefix'}
    manifest['runner_sha256']=sha(Path(__file__))
    save()


def retry_owned_trial(args):
    trial = args.retry_trial.absolute()
    if trial.resolve() != trial or trial.parent != OUTPUT or trial.is_symlink():
        raise ValueError('retry must name one canonical owned trial')
    manifest = json.loads(regular(trial/'receipt.json').read_text())
    if (manifest.get('trial') != str(trial) or manifest.get('source') != str(SOURCE)
            or manifest.get('backend') != args.backend
            or not manifest.get('xvfb_reaped') or manifest.get('status') != 'failed'):
        raise ValueError('retry requires a stopped failed trial of this backend')
    paths = {'core':trial/'Data/A2FOExtensions.dll',
             'media':trial/'Data/A2FOMedia.dll',
             'display_dll':trial/'Data/Mods'/manifest['child_mod']/'modules/refit/A2FORefitDisplay.dll',
             'renderer_dll':trial/'Data/Mods'/manifest['child_mod']/'modules/refit/A2FORefitRenderer.dll'}
    pins = {'core':CORE_SHA,'media':MEDIA_SHA,'display_dll':DISPLAY_SHA,'renderer_dll':RENDERER_SHA}
    for name, path in paths.items():
        if sha(regular(path)) != pins[name]:
            raise ValueError('staged candidate changed before retry: '+name)
    for name, expected in EXE_SHA.items():
        if sha(regular(trial/'Data'/name)) != expected:
            raise ValueError('staged executable changed before retry')
    if sha(regular(trial/'FleetOpsHook.dll')) != HOOK_SHA:
        raise ValueError('staged provider changed before retry')
    if shutil.disk_usage(trial).free < 16*1024**3:
        raise ValueError('retry would violate private trial reserve')
    archive = trial/'attempts'/str(time.time_ns())
    archive.mkdir(parents=True, exist_ok=False)
    shutil.copy2(trial/'receipt.json', archive/'receipt.json')
    for leaf in ('logs','prefix'):
        if (trial/leaf).is_symlink():
            raise ValueError('linked retry directory refused')
        (trial/leaf).rename(archive/leaf)
        (trial/leaf).mkdir()
    for key in ('progress','isolation','sandbox_exit','sandbox_timeout','error','xvfb_reaped','private_display','prefix_seed'):
        manifest.pop(key,None)
    manifest.update(status='staged',game_launched=False,launch_requested=True,
                    runtime=RUNTIME,runner_sha256=sha(Path(__file__)),
                    wine={'path':str(WINE),'sha256':sha(WINE)},
                    previous_attempt=str(archive))
    def save(): (trial/'receipt.json').write_text(json.dumps(manifest,indent=2)+'\n')
    save()
    return trial,manifest,save


def main():
    global WINE, WINESERVER, RUNTIME
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--core',type=Path,required=True)
    parser.add_argument('--display-dll',type=Path,required=True)
    parser.add_argument('--renderer-dll',type=Path,required=True)
    parser.add_argument('--media',type=Path)
    parser.add_argument('--backend',choices=('vulkan','d3d12'),required=True)
    parser.add_argument('--runtime',choices=('system','portproton'),default='system')
    parser.add_argument('--seed-prefix',type=Path,help='copy the recorded initialized private fixture prefix')
    parser.add_argument('--retry-trial',type=Path,help='retry an owned stopped failed trial, preserving its prior logs/prefix')
    parser.add_argument('--launch',action='store_true',help='run one 40-second private launch after staging')
    args=parser.parse_args()
    RUNTIME=args.runtime
    if RUNTIME == 'portproton':
        WINE=PROTON/'bin-wow64/wine'
        WINESERVER=PROTON/'bin-wow64/wineserver'
    if args.retry_trial and not args.launch:
        parser.error('--retry-trial requires --launch')
    trial=None
    try:
        trial,manifest,save=retry_owned_trial(args) if args.retry_trial else stage(args)
        if args.seed_prefix:
            seed_fixture_prefix(trial,manifest,save,args.seed_prefix)
        if args.launch:
            launch(trial,manifest,save)
        else:
            print('STAGED ONLY:',trial)
        print('Receipt:',trial/'receipt.json')
        return 0
    except (OSError,ValueError,RuntimeError,subprocess.SubprocessError) as exc:
        if trial is not None:
            manifest['status']='failed'
            manifest['error']=str(exc)
            save()
            print('FAIL; receipt:',trial/'receipt.json',file=sys.stderr)
        else:
            print('FAIL:',exc,file=sys.stderr)
        return 1

if __name__=='__main__':
    raise SystemExit(main())
