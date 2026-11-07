#!/usr/bin/env python3
"""Exercise manual launcher lifecycle on owned Xvfb/software GPU, never desktop UI."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import time

import run_refit_fo_bootstrap as base


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--package', type=Path, required=True)
    p.add_argument('--backend', choices=('vulkan', 'dx12'), required=True)
    p.add_argument('--seconds', type=int, choices=(100, 150), default=100)
    a = p.parse_args(); root = a.package.resolve(strict=True)
    output = base.ROOT / 'build/refit-manual-package-checks' / (a.backend+'-'+str(time.time_ns()))
    output.mkdir(parents=True)
    record = {'package': str(root), 'backend': a.backend, 'scope': 'Manual launcher lifecycle and normal game config under owned Xvfb/software GPU',
              'hardware_gameplay': False, 'input_automation': False, 'observation_seconds': a.seconds}
    inputs = [root/'launcher.py', root/'refit_manual_evidence.py', root/'Data/A2FOExtensions.dll']
    def hashes():
        return {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in inputs}
    record['inputs_before'] = hashes()
    with (output/'xvfb.log').open('wb') as log:
        server = subprocess.Popen([str(base.XVFB), '-displayfd', '1', '-screen', '0', '1280x1024x24', '-nolisten', 'tcp'],
                                  stdout=subprocess.PIPE, stderr=log, stdin=subprocess.DEVNULL, start_new_session=True)
    wrapper = None
    try:
        assert select.select([server.stdout], [], [], 20)[0]
        display = server.stdout.readline().decode().strip(); assert display.isdigit() and server.poll() is None
        record['owned_display'] = ':'+display
        code = r'''
import sys,signal
from pathlib import Path
sys.path.insert(0,sys.argv[2]);sys.path.insert(0,str(Path(sys.argv[1]).parent))
import launcher as m
import run_refit_fo_bootstrap as base
root=Path(sys.argv[1]).parent;display=sys.argv[3];backend=sys.argv[4]
original=m.environment
def software(data,desktop=False):
 e=original(data,False)
 e.update(DISPLAY=':'+display,VK_DRIVER_FILES=str(base.ICD),VK_ICD_FILENAMES=str(base.ICD),LIBGL_ALWAYS_SOFTWARE='1',GALLIUM_DRIVER='llvmpipe')
 for k in ('__NV_PRIME_RENDER_OFFLOAD','__GLX_VENDOR_LIBRARY_NAME','__VK_LAYER_NV_optimus'):e.pop(k,None)
 return e
m.environment=software
m.sandbox=lambda root,desktop=False:base.sandbox(root,display)
assert m.BOOT.count("'/nointro']")==1
m.BOOT=m.BOOT.replace("'/nointro']","'/nointro','Duel']")
signal.signal(signal.SIGTERM,m.stopped);signal.signal(signal.SIGINT,m.stopped)
sys.argv=[sys.argv[1],'--package',str(root),'--backend',backend]
m.main()
'''
        with (output/'wrapper.log').open('wb') as log:
            wrapper = subprocess.Popen([sys.executable, '-c', code, str(root/'launcher.py'), str(base.ROOT/'tools'), display, a.backend],
                stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT)
            time.sleep(2)
            # A second launch must fail on the package lock before any desktop access.
            duplicate = subprocess.run([sys.executable, str(root/'launcher.py'), '--probe-gpu'], capture_output=True, text=True, timeout=20)
            record['concurrent_launch_rejected'] = duplicate.returncode != 0 and 'already using this package' in duplicate.stderr
            deadline = time.monotonic()+a.seconds
            while time.monotonic()<deadline and wrapper.poll() is None:
                time.sleep(min(1, max(0, deadline-time.monotonic())))
            record['wrapper_alive_at_limit'] = wrapper.poll() is None
            if wrapper.poll() is None:
                capture = subprocess.run(['ffmpeg','-nostdin','-loglevel','error','-y','-f','x11grab','-video_size','1280x1024',
                    '-i',':'+display,'-frames:v','1','-threads','1',str(output/'owned-headless.png')],capture_output=True,timeout=30)
                record['owned_capture_exit'] = capture.returncode
                stop = subprocess.run([sys.executable,str(root/'launcher.py'),'--stop'],capture_output=True,text=True,timeout=20)
                record['owned_stop_exit'] = stop.returncode; record['owned_stop_output'] = stop.stdout.strip()
            record['wrapper_exit'] = wrapper.wait(timeout=40)
        active = json.loads((root/'run/active-session.json').read_text())
        assert active['wrapper_pid'] == wrapper.pid
        session = Path(active['session']);record['session'] = str(session)
        record['process'] = json.loads((session/'process.json').read_text())
        report = json.loads((session/'evidence.json').read_text());record['facts'] = report['facts']
        record['session_input_identity'] = report.get('input_identity_after', {})
        record['session_inputs_unchanged'] = record['session_input_identity'].get('all_inputs_match') is True
        core = session/'after/data/A2FOExtensions.log'
        text = core.read_text(errors='replace') if core.exists() else ''
        record['fixture_absent'] = '[RefitTestScene]' not in text
        record['native_factory_seen'] = 'factory returned' in text
        record['inputs_after'] = hashes()
        record['inputs_unchanged'] = record['inputs_before'] == record['inputs_after']
        record['status'] = 'pass' if (record['wrapper_alive_at_limit'] and record['concurrent_launch_rejected'] and
             record.get('owned_stop_exit') == 0 and record['wrapper_exit'] == 0 and record['process']['game_seen'] and
             record['fixture_absent'] and record['native_factory_seen'] and record['inputs_unchanged'] and record['session_inputs_unchanged']) else 'failed'
    finally:
        if wrapper and wrapper.poll() is None:
            wrapper.terminate()
            try:wrapper.wait(timeout=35)
            except subprocess.TimeoutExpired:wrapper.kill();wrapper.wait(timeout=5)
        server.terminate()
        try:server.wait(timeout=5)
        except subprocess.TimeoutExpired:server.kill();server.wait(timeout=5)
        record['xvfb_reaped'] = server.poll() is not None
        (output/'receipt.json').write_text(json.dumps(record,indent=2)+'\n')
        print(output/'receipt.json',flush=True)
    if record['status'] != 'pass':raise SystemExit(1)


if __name__ == '__main__':main()
