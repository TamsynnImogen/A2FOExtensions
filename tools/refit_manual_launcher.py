#!/usr/bin/env python3
"""User-invoked hardware playtest launcher; --check/--probe-gpu never start Wine."""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import stat
import subprocess
import sys
import tempfile
import time

import refit_manual_evidence as evidence

STOP = False


def stopped(_signal, _frame):
    global STOP
    STOP = True


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for data in iter(lambda: f.read(1048576), b''):
            h.update(data)
    return h.hexdigest()


def ordinary(path, directory=False):
    if path.is_symlink() or path.resolve() != path or not (path.is_dir() if directory else path.is_file()):
        raise ValueError('ordinary canonical path required: ' + str(path))
    return path


def check(root):
    ordinary(root, True)
    data = json.loads(ordinary(root / 'package.json').read_text())
    if data.get('schema') != 'a2fo-manual-test-v1' or data.get('package') != str(root):
        raise ValueError('manual package identity mismatch')
    for name in ('Data', 'prefix', 'home', 'logs', 'run'):
        ordinary(root / name, True)
    for relative, expected in data['critical_sha256'].items():
        p = root / relative
        if not p.resolve().is_relative_to(root) or digest(ordinary(p)) != expected:
            raise ValueError('package input changed: ' + relative)
    for name in ('wine', 'wineserver'):
        p = ordinary(Path(data[name]))
        if digest(p) != data[name + '_sha256']:
            raise ValueError('Wine runtime changed: ' + str(p))
    if digest(ordinary(Path(data['nvidia_icd']))) != data['nvidia_icd_sha256']:
        raise ValueError('NVIDIA driver manifest changed; refresh package validation')
    return data


def process_start(pid):
    text = Path(f'/proc/{pid}/stat').read_text()
    return text.rsplit(')', 1)[1].split()[19]


def write_text_atomic(path, text):
    if path.exists() or path.is_symlink():
        ordinary(path)
    with tempfile.NamedTemporaryFile(mode='w', dir=path.parent, prefix='.active-', delete=False) as f:
        f.write(text)
        temporary = Path(f.name)
    os.replace(temporary, path)


def write_active(path, data):
    write_text_atomic(path, json.dumps(data, indent=2)+'\n')


def stop_session(root):
    p = root / 'run/active-session.json'
    if not p.exists():
        print('No recorded test session.'); return
    d = json.loads(ordinary(p).read_text())
    pid = d['wrapper_pid']
    if not isinstance(pid, int) or pid <= 1:
        raise ValueError('invalid recorded process identity; nothing signalled')
    handle = None
    try:
        handle = os.pidfd_open(pid)
        args = Path(f'/proc/{pid}/cmdline').read_bytes().split(b'\0')
        if (d.get('package') != str(root) or d.get('status') != 'running' or
                process_start(pid) != d['process_start'] or
                str(root / 'launcher.py').encode() not in args):
            raise ValueError('recorded process identity does not match this launcher; nothing signalled')
        signal.pidfd_send_signal(handle, signal.SIGTERM)
        print('Requested shutdown of this package session only.')
    except (FileNotFoundError, ProcessLookupError):
        print('The recorded test session has already exited.')
    finally:
        if handle is not None:
            os.close(handle)


def desktop_authority():
    value = os.environ.get('XAUTHORITY')
    if value:
        if not Path(value).is_file():
            raise ValueError('desktop XAUTHORITY file unavailable')
        return value
    default = Path.home() / '.Xauthority'
    return str(default) if default.is_file() else None


def abstract_x11_listener(number):
    endpoint = '@/tmp/.X11-unix/X' + number
    return any(len(parts := line.split()) >= 8 and parts[3] == '00010000' and parts[-1] == endpoint
               for line in Path('/proc/net/unix').read_text().splitlines())


def desktop_transport():
    match = re.fullmatch(r':(\d+)(?:\.\d+)?', os.environ.get('DISPLAY', ''))
    if not match:
        raise ValueError('Run from your desktop terminal; a local X11/XWayland DISPLAY is required.')
    socket = Path('/tmp/.X11-unix/X' + match[1])
    if socket.is_socket():
        return 'filesystem', str(socket)
    if abstract_x11_listener(match[1]):
        return 'abstract-shared-network', None
    raise ValueError('No local X11/XWayland endpoint is available for DISPLAY=' + os.environ['DISPLAY'])


def sandbox(root, desktop=False):
    args = ['bwrap', '--die-with-parent', '--new-session', '--unshare-user', '--unshare-pid',
            '--unshare-net', '--ro-bind', '/', '/', '--bind', str(root), '/mnt',
            '--bind', str(root / 'home'), '/home/tamsynn', '--tmpfs', '/tmp',
            '--dev', '/dev', '--proc', '/proc', '--tmpfs', '/run/user',
            '--tmpfs', '/run/dbus', '--chdir', '/mnt/Data']
    if Path('/dev/dri').is_dir():
        args += ['--dev-bind', '/dev/dri', '/dev/dri']
    for p in sorted(Path('/dev').glob('nvidia*')):
        if re.fullmatch(r'nvidia(?:\d+|ctl|-modeset|-uvm|-uvm-tools)', p.name) and stat.S_ISCHR(p.stat().st_mode):
            args += ['--dev-bind', str(p), str(p)]
    if desktop:
        transport, socket = desktop_transport()
        if transport == 'filesystem':
            args += ['--dir', '/tmp/.X11-unix', '--ro-bind', socket, socket]
        else:
            # Linux abstract Unix sockets belong to the network namespace.
            # Keep the desktop namespace only when no filesystem endpoint exists.
            args.remove('--unshare-net')
        pulse = Path('/run/user') / str(os.getuid()) / 'pulse/native'
        if pulse.is_socket():
            args += ['--dir', str(pulse.parent), '--ro-bind', str(pulse), str(pulse)]
        auth = desktop_authority()
        if auth:
            args += ['--ro-bind', auth, '/mnt/run/xauthority']
    return args


def environment(data, desktop=False):
    env = {'PATH': '/usr/bin:/bin', 'LANG': 'C.UTF-8', 'USER': 'tamsynn', 'LOGNAME': 'tamsynn',
           'HOME': '/home/tamsynn', 'WINEPREFIX': '/mnt/prefix', 'WINEPATH': r'Z:\mnt;Z:\mnt\Data',
           'WINELOADER': data['wine'], 'WINESERVER': data['wineserver'], 'WINEDEBUG': '-all',
           'WINEESYNC': '0', 'WINEFSYNC': '0', 'XDG_RUNTIME_DIR': '/mnt/run',
           'VK_DRIVER_FILES': data['nvidia_icd'], 'VK_ICD_FILENAMES': data['nvidia_icd'],
           '__NV_PRIME_RENDER_OFFLOAD': '1', '__GLX_VENDOR_LIBRARY_NAME': 'nvidia',
           '__VK_LAYER_NV_optimus': 'NVIDIA_only',
           'WINEDLLOVERRIDES': 'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;'
                              'mscoree=d;mshtml=d;d3d8=n,b;d3d9=n,b;D3DX81ab=n,b;d3dx9_43=n,b'}
    if desktop:
        env['DISPLAY'] = os.environ['DISPLAY']
        if desktop_authority():
            env['XAUTHORITY'] = '/mnt/run/xauthority'
        env['PULSE_SERVER'] = f'unix:/run/user/{os.getuid()}/pulse/native'
    return env


BOOT = r'''
import json,os,pathlib,signal,subprocess,sys,time
root=pathlib.Path('/mnt');session=pathlib.Path(sys.argv[1]);child=sys.argv[2]
stop=False
def request(*a):
 global stop
 stop=True
signal.signal(signal.SIGTERM,request);signal.signal(signal.SIGINT,request)
state={'input_automation':False,'child':child,'game_seen':False,'status':'starting'}
def save(): (session/'process.json').write_text(json.dumps(state,indent=2)+'\n')
save();game=None;code=1
try:
 command=[os.environ['WINELOADER'],r'Z:\mnt\Data\ArmadaL.exe','/mod',child,'/nointro']
 state['command']=command
 with (session/'wine-output.log').open('wb') as output:
  game=subprocess.Popen(command,cwd='/mnt/Data',stdin=subprocess.DEVNULL,stdout=output,stderr=subprocess.STDOUT)
  start=time.monotonic()
  while not stop:
   if (session/'stop-requested').exists(): stop=True;break
   pids=[]
   for p in pathlib.Path('/proc').glob('[0-9]*'):
    try:
     if (p/'comm').read_text().strip().lower()=='armadal.exe': pids.append(int(p.name))
    except OSError: pass
   state['game_seen'] |= bool(pids);state['armada_pids']=pids;state['elapsed_seconds']=round(time.monotonic()-start,1)
   state['status']='running' if pids else 'waiting';save()
   if state['game_seen'] and not pids:
    try: code=game.wait(timeout=5)
    except subprocess.TimeoutExpired: state['error']='Launcher remained active after Armada exited';code=1
    break
   if not state['game_seen'] and time.monotonic()-start>180: state['error']='No Armada process observed within180 seconds';break
   time.sleep(1)
 if stop: code=130
finally:
 try:
  state['wineserver_cleanup_exit']=subprocess.run([os.environ['WINESERVER'],'-k'],stdin=subprocess.DEVNULL,timeout=10).returncode
 except subprocess.TimeoutExpired: state['wineserver_cleanup_timeout']=True
 if game:
  try: game.wait(timeout=10)
  except subprocess.TimeoutExpired: game.kill();game.wait(timeout=5)
 state['wine_launcher_returncode']=game.returncode if game else None
 state['status']='stopped' if stop else 'exited';state['exit_code']=code;save()
sys.exit(code)
'''


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--package', type=Path, default=Path(__file__).resolve().parent)
    p.add_argument('--backend', choices=('vulkan', 'dx12'), default='vulkan')
    p.add_argument('--mode', choices=('replacement', 'comparison'), default='replacement')
    action = p.add_mutually_exclusive_group()
    action.add_argument('--check', action='store_true')
    action.add_argument('--probe-gpu', action='store_true')
    action.add_argument('--check-desktop', action='store_true')
    action.add_argument('--stop', action='store_true')
    a = p.parse_args(); root = a.package.absolute()
    if a.stop:
        ordinary(root, True)
        marker = json.loads(ordinary(root / 'package.json').read_text())
        if marker.get('schema') != 'a2fo-manual-test-v1' or marker.get('package') != str(root):
            raise ValueError('manual package identity mismatch')
        stop_session(root); return
    data = check(root)
    if a.check:
        print('Package inputs verified. No game launched. Hardware/gameplay acceptance pending.'); return
    if a.check_desktop:
        transport, _ = desktop_transport()
        sandbox(root, True); environment(data, True)
        print('Desktop transport:', transport, '; endpoint/auth paths checked only. No connection or game launched.')
        return
    descriptor = os.open(root / 'run/session.lock', os.O_CREAT | os.O_RDWR | os.O_NOFOLLOW | os.O_CLOEXEC, 0o600)
    if not stat.S_ISREG(os.fstat(descriptor).st_mode):
        os.close(descriptor)
        raise ValueError('session lock must be an ordinary file')
    with os.fdopen(descriptor, 'a+') as lock:
        try: fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError: raise ValueError('A test is already using this package; close it or use Stop-Test.sh.')
        if a.probe_gpu:
            result = subprocess.run(sandbox(root) + ['/usr/bin/vulkaninfo', '--summary'],
                                    env=environment(data), capture_output=True, text=True, timeout=60)
            write_text_atomic(root / 'logs/gpu-preflight.log', result.stdout + result.stderr)
            passed = result.returncode == 0 and 'NVIDIA GeForce GTX 1660 Ti' in result.stdout and 'deviceType' in result.stdout
            write_active(root / 'logs/gpu-preflight.json', {'exit_code': result.returncode,
                'physical_device_enumerated': passed, 'game_launched': False, 'input_automation': False})
            if not passed: raise ValueError('NVIDIA Vulkan preflight failed; see logs/gpu-preflight.log')
            print('NVIDIA Vulkan device enumerated headlessly; no game or desktop window launched.'); return
        # Validate desktop access before moving any existing session logs.
        command = sandbox(root, True); env = environment(data, True)
        session = evidence.begin(root, a.backend, a.mode)
        child = data['children'][a.backend][a.mode]
        active_path = root / 'run/active-session.json'
        active = {'package': str(root), 'wrapper_pid': os.getpid(), 'process_start': process_start(os.getpid()),
                  'session': str(session), 'backend': a.backend, 'mode': a.mode, 'status': 'running',
                  'desktop_transport': desktop_transport()[0]}
        write_active(active_path, active)
        print('Starting', a.backend, a.mode, 'test.', flush=True)
        print('Session logs:', session, flush=True)
        command += ['/usr/bin/python3', '-c', BOOT, '/mnt/' + str(session.relative_to(root)), child]
        code = 1; error = ''; process = None
        try:
            with (session / 'launcher.log').open('wb') as output:
                process = subprocess.Popen(command, env=env, stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT)
                deadline = None
                while process.poll() is None:
                    if STOP and deadline is None:
                        write_text_atomic(session / 'stop-requested', 'User requested this session to stop.\n')
                        deadline = time.monotonic()+25
                    if deadline is not None and time.monotonic()>deadline: process.kill()
                    time.sleep(.25)
                code = process.returncode
        except Exception as exc:
            error = str(exc)
            if process and process.poll() is None: process.terminate(); process.wait(timeout=30)
            raise
        finally:
            active['status'] = 'exited'; active['returncode'] = code
            write_active(active_path, active)
            evidence.finish(root, session, code, error)
            print('Session evidence:', session, flush=True)
        if code not in (0, 130): raise SystemExit(code)


if __name__ == '__main__':
    signal.signal(signal.SIGTERM, stopped)
    signal.signal(signal.SIGINT, stopped)
    try:
        main()
    except Exception as exc:
        # Errors before evidence.begin() must remain visible after a double-click.
        try:
            default = Path(__file__).resolve().parent
            marker = json.loads((default / 'package.json').read_text())
            if marker.get('schema') == 'a2fo-manual-test-v1' and marker.get('package') == str(default):
                ordinary(default / 'logs', True)
                record = {'error': str(exc), 'type': type(exc).__name__,
                          'time_unix': time.time(), 'game_acceptance': False}
                write_active(default / 'logs' / ('prelaunch-error-'+str(time.time_ns())+'.json'), record)
                write_active(default / 'logs/prelaunch-error.json', record)
        except Exception:
            pass
        print('Test launcher failed:', exc, file=sys.stderr, flush=True)
        raise SystemExit(1)
