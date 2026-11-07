#!/usr/bin/env python3
"""Private legacy-game bootstrap diagnostics; no input, presentation takeover or deployment."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import select
import shutil
import subprocess
import tempfile
import time

WINE = Path('/home/tamsynn/.var/app/ru.linux_gaming.PortProton/data/dist/PROTON_LG_10-28')
XVFB = Path('/home/tamsynn/Applications/iStripper/tools/usr/bin/Xvfb')
USER_LAUNCH_BATCH = (b'@echo off\r\nsetlocal\r\ncd /d "%~dp0Data"\r\n'
                     b'set "PATH=%~dp0;%PATH%"\r\n'
                     b'start "" /b /wait "ArmadaL.exe" /mod "FO4 Nexus"\r\n')

def capture_private_root(path,display,screen):
    # Read the explicitly selected private root window. No window selection,
    # focus, pointer/keyboard operation, or host-display capture is performed.
    attempts=[]
    commands=[]
    if shutil.which('magick'):
        commands.append(['magick','import','-display',display,'-window','root','-silent',str(path)])
    commands.append(['ffmpeg','-nostdin','-loglevel','error','-y','-analyzeduration','0',
        '-probesize','32','-f','x11grab','-framerate','1','-video_size',screen,
        '-i',display,'-frames:v','1','-threads','1',str(path)])
    for command in commands:
        try:
            cap=subprocess.run(command,stdin=subprocess.DEVNULL,capture_output=True,timeout=15)
            good=cap.returncode==0 and path.is_file() and path.stat().st_size>0
            attempts.append(dict(tool=command[0],exit=cap.returncode,
                stderr=cap.stderr.decode(errors='replace')[-2000:]))
            if good:return 0,attempts
        except (subprocess.TimeoutExpired,OSError) as exc:
            attempts.append(dict(tool=command[0],exit=124,error=str(exc)))
    return 1,attempts

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def checked_private(path, root):
    path=Path(path)
    if not path.is_absolute() or ".." in path.parts or not path.is_relative_to(root):
        raise ValueError("write outside the private trial")
    for part in (path,*path.parents):
        if part.is_symlink():raise ValueError("symlink in private writable path: "+str(part))
        if part==root:break
    return path

def save(path, value):
    path.write_text(json.dumps(value, indent=2)+'\n')

def stage_private_core(source, target, trial, expected):
    """Preserve the previous private image, then replace it with a verified copy."""
    checked_private(target,trial)
    previous=sha(target)
    backup=checked_private(trial/'logs'/('copied-core-prior-'+previous+'.dll'),trial)
    if not backup.exists():shutil.copy2(target,backup)
    if sha(backup)!=previous:raise ValueError('private prior core backup mismatch')
    descriptor,name=tempfile.mkstemp(prefix='.native11-core-',suffix='.dll',dir=target.parent)
    temporary=Path(name)
    try:
        with os.fdopen(descriptor,'wb') as out, Path(source).open('rb') as inp:
            shutil.copyfileobj(inp,out)
            out.flush();os.fsync(out.fileno())
        shutil.copystat(source,temporary)
        if sha(temporary)!=expected:raise ValueError('private staged core hash mismatch')
        checked_private(target,trial)
        if sha(target)!=previous:raise ValueError('private core changed during staging')
        os.replace(temporary,target)
        if sha(target)!=expected:raise ValueError('private installed core hash mismatch')
    finally:
        if temporary.exists():temporary.unlink()

def stage_private_helper(source, target, trial, expected):
    """Keep the previous private helper before installing the gate-matched copy."""
    checked_private(target,trial)
    previous=sha(target) if target.exists() else None
    if previous is not None:
        backup=checked_private(trial/'logs'/('copied-helper-prior-'+previous+'.dll'),trial)
        if not backup.exists():shutil.copy2(target,backup)
        if sha(backup)!=previous:raise ValueError('private prior helper backup mismatch')
    descriptor,name=tempfile.mkstemp(prefix='.native11-helper-',suffix='.dll',dir=target.parent)
    temporary=Path(name)
    try:
        with os.fdopen(descriptor,'wb') as out, Path(source).open('rb') as inp:
            shutil.copyfileobj(inp,out)
            out.flush();os.fsync(out.fileno())
        shutil.copystat(source,temporary)
        if sha(temporary)!=expected:raise ValueError('private staged helper hash mismatch')
        checked_private(target,trial)
        if (sha(target) if target.exists() else None)!=previous:
            raise ValueError('private helper changed during staging')
        os.replace(temporary,target)
        if sha(target)!=expected:raise ValueError('private installed helper hash mismatch')
    finally:
        if temporary.exists():temporary.unlink()

def set_native11_options(data, options):
    """Set one effective value per task option in the existing Native11 section."""
    text=data.decode('latin1')
    eol='\r\n' if '\r\n' in text else '\n'
    lines=text.splitlines(keepends=True)
    sections=[i for i,line in enumerate(lines)
              if re.match(r'^\s*\[Native11\]\s*(?:[;#].*)?(?:\r?\n)?$',line,re.IGNORECASE)]
    if len(sections)!=1:raise ValueError('one Native11 section required')
    start=sections[0]+1
    end=next((i for i in range(start,len(lines))
              if re.match(r'^\s*\[[^\]\r\n]+\]',lines[i])),len(lines))
    keys={key.casefold() for key in options}
    kept=[line for line in lines[start:end]
          if not (match:=re.match(r'^\s*([^=;#\s]+)\s*=',line)) or match.group(1).casefold() not in keys]
    if kept and not kept[-1].endswith(('\n','\r')):kept[-1]+=eol
    lines[start:end]=kept+[f'{key}={value}{eol}' for key,value in options.items()]
    return ''.join(lines).encode('latin1')

LOG_NAMES=('A2FOExtensions.log','A2FOStartupProxy.log','A2FORenderer.log',
           'A2FOMissions-fault.log')
ATTEMPT_FILES=(
    'bootstrap-receipt.json','isolation-receipt.json',
    'captures/bootstrap.png','captures/bootstrap-final.png',
    'logs/bootstrap-progress.json','logs/game-process.log','logs/wineboot.log',
    'logs/read-profiles.json',
    'logs/isolation-probe.log','logs/xvfb.log','logs/sandbox-bootstrap.log',
    *(f'Data/{name}' for name in LOG_NAMES),
    *(f'logs/new-{name}' for name in LOG_NAMES),
)

def archive_previous(trial):
    existing=[]
    for name in ATTEMPT_FILES:
        source=checked_private(trial/name,trial)
        if source.is_file():existing.append((name,source))
    if not existing:return None
    parent=checked_private(trial/'attempts',trial)
    parent.mkdir(exist_ok=True)
    attempt=checked_private(parent/(str(time.time_ns())+'-'+str(os.getpid())),trial)
    attempt.mkdir()
    for name,source in existing:
        destination=checked_private(attempt/name,trial)
        destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,destination)
        if sha(source)!=sha(destination):raise RuntimeError('previous attempt archive mismatch: '+name)
    return attempt

def extract_log(previous,current):
    if current is None:return 'missing',b''
    if previous is None:return 'new',current
    if current.startswith(previous):return 'append',current[len(previous):]
    return 'rewrite',current

def sandbox(trial, display=None):
    args = ['bwrap', '--die-with-parent', '--new-session', '--unshare-user',
            '--unshare-pid', '--unshare-net', '--ro-bind', '/', '/',
            '--bind', str(trial), '/mnt', '--ro-bind', str(WINE), '/opt',
            '--bind', str(trial/'home'), '/home/tamsynn',
            '--ro-bind', str(WINE), str(WINE), '--tmpfs', '/tmp',
            '--dev', '/dev', '--proc', '/proc', '--tmpfs', '/run/user',
            '--tmpfs', '/run/dbus', '--chdir', '/mnt/Data']
    if display is not None:
        args += ['--dir', '/tmp/.X11-unix', '--ro-bind',
                 f'/tmp/.X11-unix/X{display}', f'/tmp/.X11-unix/X{display}']
    return args

PROBE = r'''
import os,json,pathlib
p=pathlib.Path
assert p('/mnt/Data/ArmadaL.exe').is_file()
assert p('/opt/bin-wow64/wine').is_file()
assert not p('/dev/input').exists() and not list(p('/dev').glob('hidraw*'))
assert not p('/dev/dri').exists() and not p('/dev/snd').exists()
assert not p('/home/tamsynn/NVMeData/Fleet Ops Roots').exists()
assert not p('/home/tamsynn/.codex').exists()
assert not p('/run/user/1000/bus').exists()
assert len(p('/proc/net/route').read_text().splitlines())==1
mounts=p('/proc/mounts').read_text().splitlines()
assert any(x.split()[1]=='/' and 'ro' in x.split()[3].split(',') for x in mounts)
print(json.dumps({'isolated':True,'physical_input_devices':False,'hardware_gpu':False,
                 'host_home_visible':False,'host_session_bus':False,'external_routes':False}))
'''

# The sandbox's Python remains the parent of the Wine processes. It performs
# no GUI commands. A PID namespace and die-with-parent contain every descendant.
BOOT = r'''
import json,os,pathlib,subprocess,time,struct
p=pathlib.Path
progress=p('/mnt/logs/bootstrap-progress.json')
state={'phase':'wineboot','wineboot_limit_seconds':120,'input_automation':False,'launcher_invoked':False,
       'game_process_seen':False,'hook_bytes_observed':False,'snapshots':[],
       'read_errors':[],'pid_namespace':os.readlink('/proc/self/ns/pid')}
def mark():progress.write_text(json.dumps(state,indent=2)+'\n')
mark()
try:
 setup_start=time.monotonic()
 with p('/mnt/logs/wineboot.log').open('w') as log:
  try:r=subprocess.run(['/opt/bin-wow64/wine','wineboot.exe','-u'],stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,timeout=120)
  except subprocess.TimeoutExpired:state['phase']='wineboot-timeout';mark();raise SystemExit(2)
 state['wineboot_elapsed_seconds']=round(time.monotonic()-setup_start,1)
 state['wineboot_exit']=r.returncode
 if r.returncode:state['phase']='wineboot-failed';mark();raise SystemExit(2)
 state['phase']='starting-game';mark()
 with p('/mnt/logs/game-process.log').open('w') as log:
  command=(['/opt/bin-wow64/wine','cmd','/c','Z:\\mnt\\Native11 Inspection.bat']
           if os.environ.get('A2FO_PRIVATE_USE_BATCH')=='1' else
           ['/opt/bin-wow64/wine','Z:\\mnt\\Data\\ArmadaL.exe','/mod','FO4 Nexus'])
  game=subprocess.Popen(command,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,cwd='/mnt/Data')
  state['launcher_invoked']=True;start=time.monotonic()
  limit=int(os.environ.get('A2FO_PRIVATE_BOOT_SECONDS','40'))
  assert limit in (40,90)
  while game.poll() is None and time.monotonic()-start<limit:
   time.sleep(2)
   for proc in p('/proc').iterdir():
    if not proc.name.isdigit():continue
    try:
     if (proc/'comm').read_text().strip().lower()!='armadal.exe':continue
     state['game_process_seen']=True
     with (proc/'mem').open('rb',buffering=0) as mem:
      def read(at,n):return os.pread(mem.fileno(),n,at)
      def word(at):return struct.unpack('<I',read(at,4))[0]
      v={'elapsed':round(time.monotonic()-start,1),'pid':int(proc.name),
         'sprite_call':read(0x63b486,6).hex(),'end_scene_call':read(0x624719,6).hex(),
         'database_destructor':read(0x620690,190).hex(),
         'texture_destructor':read(0x6427d0,146).hex()}
      if (len(v['sprite_call'])!=12 or len(v['end_scene_call'])!=12 or
          len(v['database_destructor'])!=380 or len(v['texture_destructor'])!=292):
       raise ValueError('short hook-byte or destructor read')
      state['hook_bytes_observed']=True
      db_entry=bytes.fromhex(v['database_destructor'])
      if db_entry[0]==0x68 and db_entry[5]==0xc3:
       fleet_base=struct.unpack_from('<I',db_entry,1)[0]-0x1d4bb4
       if 0x10000<=fleet_base<0xff000000:
        v['fleet_database_wrapper_base']=hex(fleet_base)
        original=word(fleet_base+0x246a1c)
        v['fleet_database_original']=hex(original)
        if original:v['fleet_database_original_bytes']=read(original,32).hex()
        if os.environ.get('A2FO_PRIVATE_PROFILE_DIAGNOSTICS')=='1':
         ctor={'root_entry':read(0x620540,6).hex(),
               'parser_block':read(0x63faac,20).hex(),
               'inner_wrapper':read(fleet_base+0x1d4b48,52).hex(),
               'outer_wrapper':read(fleet_base+0x1d4b7c,56).hex()}
         cell=word(fleet_base+0x212870);ctor['table_cell']=hex(cell)
         if cell==fleet_base+0x210b38:ctor['table_target']=hex(word(cell))
         saved_ctor=word(fleet_base+0x246a18);ctor['saved_original']=hex(saved_ctor)
         if saved_ctor:ctor['saved_original_bytes']=read(saved_ctor,16).hex()
         v['fleet_constructor']=ctor
         prep={'setup_entry':read(0x4cb390,8).hex(),'entity_entry':read(0x4d54e0,6).hex(),
               'setup_callback':read(fleet_base+0x1fb250,99).hex(),
               'variant_decoder':read(fleet_base+0x1fb18c,66).hex(),
               'class_extension':read(fleet_base+0x10c704,30).hex(),
               'class_decoder':read(fleet_base+0x10bc84,66).hex(),
               'array_length':read(fleet_base+0x6e8c,8).hex()}
         saved_setup=word(fleet_base+0x24a1f0)
         prep['saved_setup']=hex(saved_setup)
         if saved_setup:prep['saved_setup_bytes']=read(saved_setup,32).hex()
         saved_entity=word(fleet_base+0x246e14)
         prep['saved_entity']=hex(saved_entity)
         if saved_entity:prep['saved_entity_bytes']=read(saved_entity,32).hex()
         v['fleet_prepare']=prep
      engine=word(0x7ad508);v['engine']=hex(engine)
      if engine:
       index=word(engine+0xc0);v['device_index']=index
       if index<16:
        dev=word(engine+0xcc+index*4);v['device']=hex(dev)
        if dev:
         v['device_vtable']=hex(word(dev));v['native_target_size']=list(struct.unpack('<ff',read(dev+0x10,8)))
         v['device_dimensions_84_88']=list(struct.unpack('<ff',read(dev+0x84,8)))
         v['native_interface']=hex(word(dev+0x90))
      state['snapshots'].append(v)
      diagnostic_key='profile_diagnostics' if 'profile_diagnostics' not in state else 'profile_diagnostics_late'
      diagnostic_time=12 if diagnostic_key=='profile_diagnostics' else 60
      if (os.environ.get('A2FO_PRIVATE_PROFILE_DIAGNOSTICS')=='1' and
          time.monotonic()-start>=diagnostic_time and diagnostic_key not in state):
       recipes=json.loads(p('/mnt/logs/read-profiles.json').read_text())
       observations={}
       for name,recipe in recipes.items():
        observed={'regions':[],'slots':[]}
        for region in recipe['regions']:
         item=dict(region)
         try:item['actual_hex']=read(0x400000+region['rva'],region['size']).hex()
         except OSError as e:item['read_error']=str(e)
         observed['regions'].append(item)
        for slot in recipe['slots']:
         item=dict(slot)
         try:item['actual']=word(0x400000+slot['rva'])
         except (OSError,struct.error) as e:item['read_error']=str(e)
         observed['slots'].append(item)
        observations[name]=observed
       state[diagnostic_key]=observations
    except (OSError,ValueError,struct.error) as e:
     if len(state['read_errors'])<16:state['read_errors'].append({'pid':proc.name,'error':str(e)})
   state['phase']='game-running';state['elapsed']=round(time.monotonic()-start,1);mark()
  alive=game.poll() is None
  if alive:
   game.terminate()
   try:game.wait(timeout=5)
   except subprocess.TimeoutExpired:game.kill();game.wait(timeout=5)
  state.update(phase='finished' if state['snapshots'] else 'finished-without-runtime-snapshot',
               ran_for_seconds=round(time.monotonic()-start,1),alive_at_probe_limit=alive,game_exit=game.returncode,
               scope='Legacy bootstrap with diagnostic core; no native DX11 gameplay claim')
finally:
 try:
  state['wineserver_cleanup_exit']=subprocess.run(['/opt/bin-wow64/wineserver','-k'],stdin=subprocess.DEVNULL,timeout=8,check=False).returncode
 except subprocess.TimeoutExpired:state['wineserver_cleanup_timeout']=True
 mark()
if not state['snapshots']:raise SystemExit(3)
'''

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--trial-root',required=True,type=Path)
    ap.add_argument('--receipt',required=True,type=Path)
    source=ap.add_mutually_exclusive_group(required=True)
    source.add_argument('--candidate',type=Path)
    source.add_argument('--baseline',action='store_true')
    ap.add_argument('--renderer-helper',type=Path,
                    help='Gate-matched API11 helper to stage in the private trial on launch')
    ap.add_argument('--live-preview-export',action='store_true',
                    help='Enable the observed-preview file exporter inside the private trial')
    ap.add_argument('--screen',choices=('1280x720','800x600','1280x1024'),default='1280x1024')
    ap.add_argument('--seconds',type=int,choices=(40,90),default=40)
    ap.add_argument('--profile-diagnostics',action='store_true',help='Read bounded construction, playback, lifecycle, asset and sprite regions in the private process')
    ap.add_argument('--launch',action='store_true')
    args=ap.parse_args();trial=args.trial_root
    if not trial.is_absolute() or trial.is_symlink():raise ValueError('exact private trial path required')
    if trial.resolve()!=trial:raise ValueError('canonical private trial path required')
    clone=json.loads((trial/'clone-receipt.json').read_text())
    if clone.get('root')!=str(trial) or clone.get('clone')!=str(trial/'Data'):
        raise ValueError('trial does not match its clone receipt')
    writable=['Data/A2FOExtensions.dll','Data/A2FONative11Renderer.dll','Data/Mods/FO4 Nexus/A2FORenderer.ini',
              'Data/Mods/FO4 Nexus/info.ini','logs','captures','home','prefix','run','attempts',
              'logs/copied-core-original.dll','logs/copied-renderer-original.ini',
              'logs/isolation-probe.log','logs/xvfb.log','logs/sandbox-bootstrap.log',
              'logs/bootstrap-progress.json','captures/bootstrap.png','captures/bootstrap-final.png',
              'logs/read-profiles.json',
              'isolation-receipt.json','bootstrap-receipt.json',
              *(f'Data/{name}' for name in LOG_NAMES)]
    if args.live_preview_export:
        writable+=['Native11 Inspection.bat','Data/Native11Preview']+['Data/Native11Preview/'+name for name in
            ('.a2fo-preview-v1','capture.lock','status.json','frame-a.bmp','frame-b.bmp','write.tmp')]
    writable += ['logs/new-'+name for name in LOG_NAMES]
    for name in writable:checked_private(trial/name,trial)
    receipt=json.loads(args.receipt.read_text());protected=receipt['protected_live_sha256_unchanged']
    assert protected==clone['protected_original_sha256']
    assert all(sha(p)==h for p,h in protected.items())
    for name in ['home/Documents','home/.config','run','logs','prefix','captures']:(trial/name).mkdir(parents=True,exist_ok=True)
    originals=[value for path,value in protected.items()
               if Path(path).name=='A2FOExtensions.dll' and Path(path).parent.name=='Data']
    if len(originals)!=1:raise ValueError('one protected original core hash required')
    original_sha=originals[0]
    target=trial/'Data/A2FOExtensions.dll'
    backup=trial/'logs/copied-core-original.dll'
    if backup.exists():
        if sha(backup)!=original_sha:raise ValueError('private original core backup hash mismatch')
    elif args.baseline or args.launch:
        if sha(target)!=original_sha:raise ValueError('cannot preserve original core from changed trial target')
        shutil.copy2(target,backup)
        if sha(backup)!=original_sha:raise ValueError('private original core backup copy mismatch')
    if args.baseline:
        if not backup.exists():raise ValueError('baseline requires preserved private original core')
        tested_core=backup;mode='baseline'
    else:
        if sha(args.candidate)!=receipt['core_dll']['core_sha256']:
            raise ValueError('candidate does not match milestone core')
        tested_core=args.candidate;mode='candidate'
    tested_sha=sha(tested_core)
    helper_target=trial/'Data/A2FONative11Renderer.dll'
    helper_expected=receipt['core_dll'].get('helper_sha256')
    if args.renderer_helper is not None:
        if not helper_expected or sha(args.renderer_helper)!=helper_expected:
            raise ValueError('renderer helper does not match milestone helper')
    tested_helper_sha=helper_expected if args.renderer_helper is not None and args.launch else (
        sha(helper_target) if helper_target.exists() else None)
    assert len(receipt['runtime_cases'])>=117 and all(x['exit_code']==0 for x in receipt['runtime_cases'])
    archived=archive_previous(trial) if args.launch else None
    if args.launch:
        for name in ('captures/bootstrap.png','captures/bootstrap-final.png','logs/bootstrap-progress.json',
                     *(f'logs/new-{item}' for item in LOG_NAMES)):
            path=checked_private(trial/name,trial)
            if path.exists():path.unlink()
    env={'PATH':'/usr/bin:/bin','LANG':'C.UTF-8','USER':'tamsynn','LOGNAME':'tamsynn',
         'HOME':os.environ.get('HOME','/home/tamsynn'),'WINEPREFIX':'/mnt/prefix',
         'A2FO_PRIVATE_BOOT_SECONDS':str(args.seconds),
         'A2FO_PRIVATE_PROFILE_DIAGNOSTICS':'1' if args.profile_diagnostics else '0',
         'A2FO_PRIVATE_USE_BATCH':'1' if args.live_preview_export else '0',
         'WINELOADER':'/opt/bin-wow64/wine','WINESERVER':'/opt/bin-wow64/wineserver',
         'WINEPATH':r'Z:\mnt;Z:\mnt\Data','WINEDEBUG':'-all','WINEESYNC':'0','WINEFSYNC':'0','XDG_RUNTIME_DIR':'/mnt/run',
         'LIBGL_ALWAYS_SOFTWARE':'1','GALLIUM_DRIVER':'llvmpipe',
         'WINEDLLOVERRIDES':'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;mscoree=d;mshtml=d;winepulse.drv=d;winealsa.drv=d;d3d8=n,b;d3d9=n,b;D3DX81ab=n,b;d3dx9_43=n,b'}
    icds=sorted(Path('/usr/share/vulkan/icd.d').glob('lvp_icd*.json'))
    if icds:env['VK_ICD_FILENAMES']=':'.join(map(str,icds))
    probe=subprocess.run(sandbox(trial)+['/usr/bin/python3','-c',PROBE],env=env,text=True,capture_output=True,timeout=20)
    (trial/'logs/isolation-probe.log').write_text(probe.stdout+probe.stderr)
    if probe.returncode:raise RuntimeError('private isolation assertions failed')
    result={'isolation':json.loads(probe.stdout),'game_launched':False,'input_automation':False,
            'core_sha256':tested_sha,'tested_core_sha256':tested_sha,
            'tested_helper_sha256':tested_helper_sha,
            'observed_preview_export_enabled':args.live_preview_export,
            'user_launch_batch_sha256':hashlib.sha256(USER_LAUNCH_BATCH).hexdigest() if args.live_preview_export else None,
            'tested_core_mode':mode,'candidate_sha256':tested_sha if mode=='candidate' else None,
            'previous_attempt_archive':str(archived) if archived else None,
            'private_screen':args.screen,
            'private_probe_seconds':args.seconds,
            'protected_original_sha256':protected,
            'legacy_graphics_allowed':True,'native_dx11_gameplay_claim':False,
            'tool_sha256':sha(Path(__file__)), 'limited_host_tmp_writes':'Task Xvfb socket/lock only'}
    if not args.launch:
        save(trial/'isolation-receipt.json',result);print('PASS private isolation only; no game launched');return
    if args.profile_diagnostics:
        from check_native11_engine_inventory import PE32,EXPECTED_SHA256
        native=trial/'Data/ArmadaL.exe'
        if sha(native)!=EXPECTED_SHA256:raise ValueError('unsupported private diagnostic image')
        pe=PE32(native.read_bytes());recipes={};total=0
        for name in ('asset-engine-profile.json','tga-engine-profile.json','sprite-dimensions-profile.json',
                     'model-template-profile.json','node-engine-profile.json','lifecycle-profile.json','capture-scale-engine-profile.json','craft-view-profile.json','craft-preparation-profile.json','craft-visibility-base-profile.json','borg-material-profile.json'):
            source=Path(__file__).resolve().parent.parent/'build/native11'/name
            profile=json.loads(source.read_text())
            if profile.get('image_sha256',profile.get('sha256'))!=EXPECTED_SHA256:
                raise ValueError('unsupported diagnostic profile input')
            regions=profile['regions']
            if name=='capture-scale-engine-profile.json':
                regions=[regions[key] for key in ('craft-instance-frustum-scale','apply-scale-effects',
                    'instance-scale-copy','plain-database-basis-scale')]
            regions=regions.values() if isinstance(regions,dict) else regions
            recipe={'regions':[],'slots':[]}
            for region in regions:
                rva=region.get('rva',region.get('rva_start'));rva=int(rva,0) if isinstance(rva,str) else rva
                size=region.get('size')
                if size is None:
                    end=region['rva_end'];end=int(end,0) if isinstance(end,str) else end;size=end-rva
                if not 0<size<=8192:raise ValueError('diagnostic region exceeds read bound')
                total+=size
                item=dict(rva=rva,size=size,expected_hex=pe.read(rva,size).hex())
                if region.get('runtime_input'):
                    if name!='craft-view-profile.json' or region['runtime_input']!='near_fade' or rva!=0x30444c or size!=12:
                        raise ValueError('unsupported mutable diagnostic input')
                    item['runtime_input']='near_fade'
                recipe['regions'].append(item)
            for slot in profile.get('slots',[]):
                rva,target_rva=(slot['rva'],slot['target_rva']) if isinstance(slot,dict) else slot
                recipe['slots'].append(dict(rva=rva,expected=0x400000+target_rva))
            if name=='capture-scale-engine-profile.json':
                for rva,slot_target in ((0x2b3e38,0xcb6b0),(0x2b3e40,0xcb0e0),(0x2bc56c,0x2209b0)):
                    if int.from_bytes(pe.read(rva,4),'little')!=0x400000+slot_target:
                        raise ValueError('diagnostic placement slot differs from pinned Root image')
                    recipe['slots'].append(dict(rva=rva,expected=0x400000+slot_target))
            recipes[name]=recipe
        clock=json.loads((Path(__file__).resolve().parent.parent/'build/native11/clock-profile-regeneration.json').read_text())
        recipe={'regions':[],'slots':[{'rva':0x2bc3ac,'expected':0x6146a0},
                                      {'rva':0x2bc43c,'expected':0x6161c0}]}
        for key,region in clock['regions'].items():
            rva=int(key,0);expected=bytes.fromhex(region['bytes_hex'])
            if not 0<len(expected)<=512 or pe.read(rva,len(expected))!=expected:
                raise ValueError('diagnostic clock region differs from pinned Root image')
            total+=len(expected)
            recipe['regions'].append(dict(rva=rva,size=len(expected),expected_hex=expected.hex()))
        recipes['animation-clock-live-profile']=recipe
        if total>65536 or sum(len(x['regions'])+len(x['slots']) for x in recipes.values())>128:
            raise ValueError('diagnostic manifest exceeds read bound')
        save(trial/'logs/read-profiles.json',recipes)
    # Only the task-owned copy is changed. Preserve its copied original core.
    stage_private_core(tested_core,target,trial,tested_sha)
    if args.renderer_helper is not None:
        stage_private_helper(args.renderer_helper,helper_target,trial,helper_expected)
    if args.live_preview_export:
        batch=checked_private(trial/'Native11 Inspection.bat',trial)
        if batch.exists() and batch.read_bytes()!=USER_LAUNCH_BATCH:
            raise ValueError('private inspection launcher differs from expected bytes')
        if not batch.exists():batch.write_bytes(USER_LAUNCH_BATCH)
    # Optional observed-preview drawing/export never grants world completeness
    # or presentation takeover; legacy presentation owns startup.
    from native11_clock_trial import update_ini
    ini=trial/'Data/Mods/FO4 Nexus/A2FORenderer.ini'
    before=ini.read_bytes();ini_backup=trial/'logs/copied-renderer-original.ini'
    if not ini_backup.exists():ini_backup.write_bytes(before)
    updated=set_native11_options(update_ini(ini_backup.read_bytes()),{
        'CaptureSprites':'1','CaptureWorldLights':'1','OffscreenWorld':'0',
        'LiveMaterials':'1','LiveMaterialPreview':'1' if args.live_preview_export else '0',
        'LivePreviewExport':'1' if args.live_preview_export else '0','LivePreviewIntervalMs':'1000'})
    ini.write_bytes(updated)
    info=trial/'Data/Mods/FO4 Nexus/info.ini'
    info.write_bytes(b''.join(line for line in info.read_bytes().splitlines(keepends=True) if b'A2FOLogMailer' not in line))
    previous_logs={name:(trial/'Data'/name).read_bytes() if (trial/'Data'/name).exists() else None
                   for name in LOG_NAMES}
    server=subprocess.Popen([str(XVFB),'-displayfd','1','-screen','0',args.screen+'x24','-nolisten','tcp','-ac'],stdout=subprocess.PIPE,stderr=(trial/'logs/xvfb.log').open('wb'))
    try:
        if not select.select([server.stdout],[],[],20)[0]:raise RuntimeError('private X server timeout')
        display=server.stdout.readline().decode().strip()
        if not display.isdigit() or server.poll() is not None:raise RuntimeError('private display allocation failed')
        env['DISPLAY']=':'+display;result['private_display']=env['DISPLAY']
        with (trial/'logs/sandbox-bootstrap.log').open('w') as log:
            boot=subprocess.Popen(sandbox(trial,display)+['/usr/bin/python3','-c',BOOT],env=env,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT)
            for _ in range(12):
                time.sleep(2)
                if boot.poll() is not None:break
            if boot.poll() is None:
                result['screenshot_exit'],result['screenshot_attempts']=capture_private_root(
                    trial/'captures/bootstrap.png',':'+display,args.screen)
            # Obtain a late read-only capture before the owned child reaches
            # its time limit. No input or window activation is performed.
            # Allow the bounded 120-second WoW64 prefix update as well as the
            # separately bounded game observation and cleanup.
            deadline=time.monotonic()+args.seconds+130
            while boot.poll() is None and time.monotonic()<deadline:
                progress=trial/'logs/bootstrap-progress.json'
                try:
                    state=json.loads(progress.read_text())
                    ready=state.get('elapsed',0)>=args.seconds-8
                except (OSError,ValueError):ready=False
                if ready and 'final_screenshot_exit' not in result:
                    result['final_screenshot_exit'],result['final_screenshot_attempts']=capture_private_root(
                        trial/'captures/bootstrap-final.png',':'+display,args.screen)
                try:boot.wait(timeout=1)
                except subprocess.TimeoutExpired:pass
            try:result['sandbox_exit']=boot.wait(timeout=1)
            except subprocess.TimeoutExpired:boot.kill();boot.wait();result['sandbox_timeout']=True
        if (trial/'logs/bootstrap-progress.json').exists():
            result['progress']=json.loads((trial/'logs/bootstrap-progress.json').read_text())
            result['game_launched']=result['progress'].get('game_process_seen',False)
            result['launcher_invoked']=result['progress'].get('launcher_invoked',False)
            result['runtime_snapshot_observed']=bool(result['progress'].get('snapshots'))
    finally:
        if 'boot' in locals() and boot.poll() is None:
            boot.terminate()
            try:boot.wait(timeout=5)
            except subprocess.TimeoutExpired:boot.kill();boot.wait()
        result['bootstrap_reaped']='boot' in locals() and boot.poll() is not None
        # Preserve game/namespace evidence even if screenshot or host-side
        # collection fails before the normal progress extraction above.
        try:
            progress=json.loads((trial/'logs/bootstrap-progress.json').read_text())
            result['progress']=progress
            result['game_launched']=progress.get('game_process_seen',False)
            result['launcher_invoked']=progress.get('launcher_invoked',False)
            result['runtime_snapshot_observed']=bool(progress.get('snapshots'))
        except (OSError,ValueError):pass
        server.terminate()
        try:server.wait(timeout=5)
        except subprocess.TimeoutExpired:server.kill();server.wait()
        result['xvfb_reaped']=server.poll() is not None
        result['xvfb_socket_remaining']=Path('/tmp/.X11-unix/X'+display).exists() if 'display' in locals() else None
        token=result.get('progress',{}).get('pid_namespace')
        remaining=[]
        if token:
            for proc in Path('/proc').iterdir():
                if not proc.name.isdigit():continue
                try:
                    if os.readlink(proc/'ns/pid')==token:remaining.append(int(proc.name))
                except OSError:pass
        result['private_namespace_remaining_pids']=remaining
        result['namespace_cleanup_verified']=bool(token) and not remaining
        result['log_extraction']={}
        for name,previous in previous_logs.items():
            path=checked_private(trial/'Data'/name,trial)
            current=path.read_bytes() if path.exists() else None
            extraction,delta=extract_log(previous,current)
            if current is not None:
                checked_private(trial/'logs'/('new-'+name),trial).write_bytes(delta)
            result['log_extraction'][name]={'mode':extraction,
                'before_bytes':len(previous) if previous is not None else None,
                'after_bytes':len(current) if current is not None else None,
                'emitted_bytes':len(delta)}
        assert all(sha(p)==h for p,h in protected.items()),'protected installation changed'
        result['protected_originals_unchanged']=True
        save(trial/'bootstrap-receipt.json',result)
    print('Private bootstrap finished; see',trial/'bootstrap-receipt.json')
    if (result.get('sandbox_exit')!=0 or not result.get('runtime_snapshot_observed') or
        not result.get('namespace_cleanup_verified') or not result.get('xvfb_reaped') or
        result.get('xvfb_socket_remaining') or not result.get('bootstrap_reaped')):
        raise SystemExit(1)

if __name__=='__main__':main()
