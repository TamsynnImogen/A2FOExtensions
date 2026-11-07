#!/usr/bin/env python3
"""Whole-source info-default/provider cache fixture; game entrypoints stay uncalled."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,tempfile
from check_rust_stage8_core_lifecycle_parity import replace_body
ROOT=Path(__file__).resolve().parents[1];STAGE=ROOT/'build/rust-migration-stage8';WORK=STAGE/'core-settings-work'
SOURCE='core/dllmain.cpp'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args,env=None):
 with tempfile.TemporaryFile()as out,tempfile.TemporaryFile()as err:
  result=subprocess.run(args,cwd=ROOT,env=env,stdout=out,stderr=err,timeout=180)
  out.seek(0);err.seek(0);output,error=out.read(),err.read()
 if result.returncode:raise RuntimeError(f'{args}: {result.returncode}\n{output[-3000:]!r}\n{error[-5000:]!r}')
 return output.replace(b'\r\n',b'\n')
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--build-only',action='store_true');args=parser.parse_args();WORK.mkdir(parents=True,exist_ok=True)
 archive=Path(os.environ.get('A2FO_RUST_WIN_LIB',ROOT/'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a')).resolve();before=STAGE/'before'/SOURCE
 assert sha(before)==json.loads((STAGE/'before-hashes.json').read_text())[SOURCE]
 pinned=[ROOT/SOURCE,ROOT/'rust/a2fo_core/src/stage8_core_settings.rs',ROOT/'rust/stage8_core_settings_ffi.h',ROOT/'tools/check_rust_stage8_core_lifecycle_parity.py',ROOT/'tests/rust_stage8_core_settings_native.cpp',Path(__file__).resolve(),archive]
 initial={str(p):sha(p)for p in pinned}
 executables=[]
 for label,source in [('before',before),('current',ROOT/SOURCE)]:
  text=source.read_text().replace('__declspec(dllexport)','')
  text=replace_body(text,'void wait_for_deferred_initialization() {','return;')
  # Avoid CRT treating the production DLL entry as a live fixture entrypoint.
  text=re.sub(r'\bDllMain\b','a2fo_uninvoked_fixture_DllMain',text)
  text=re.sub(r'\bA2FO_Initialize\b','a2fo_uninvoked_fixture_Initialize',text)
  text=re.sub(r'#include "([^"\n]+)"',lambda m:'#include "'+str((source.parent/m[1]).resolve())+'"',text)
  generated=WORK/(label+'-core.cpp');generated.write_text(text);exe=WORK/(label+'.exe')
  command=['i686-w64-mingw32-g++','-std=gnu++17','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-DNDEBUG','-DWIN32_LEAN_AND_MEAN','-ffunction-sections','-fdata-sections','-static','-Wl,--gc-sections',f'-DCORE_SOURCE="{generated}"',str(ROOT/'tests/rust_stage8_core_settings_native.cpp'),str(archive),'-ladvapi32','-lgdi32','-lcomctl32','-lversion','-lws2_32','-lshell32','-lwinmm','-lole32','-lmsimg32','-lgdiplus','-ldxguid','-o',str(exe)]
  source_index=command.index(str(ROOT/'tests/rust_stage8_core_settings_native.cpp'))
  obj=WORK/(label+'.o')
  run(command[:source_index+1]+['-c','-o',str(obj)])
  undefined=subprocess.check_output(['i686-w64-mingw32-nm','--undefined-only',str(obj)],text=True)
  native=[]
  for line in undefined.splitlines():
   name=line.split()[-1]
   if (name.startswith('__ZN4a2fo') or name.startswith('__ZNK4a2fo') or name.startswith('_a2fo_')) and not name.startswith('_a2fo_rs_'):native.append(name)
  # Unused engine paths still require COFF relocation targets. Any accidental
  # execution aborts instead of calling a game/GUI primitive.
  stubs=WORK/(label+'-uninvoked.S')
  stubs.write_text('.text\n'+''.join('.globl '+n+'\n'+n+':\n jmp _a2fo_fixture_unexpected_native\n'for n in native))
  trap=WORK/'uninvoked.cpp';trap.write_text('#include <cstdlib>\n#include <cstdio>\nextern "C" [[noreturn]] void a2fo_fixture_unexpected_native(){std::fputs("unexpected native engine path\\n",stderr);std::abort();}\n')
  run([command[0],'-static','-Wl,--gc-sections',str(obj),str(stubs),str(trap),*command[source_index+1:]])
  executables.append(exe)
 if args.build_only:print('core settings whole-source fixtures compile: PASS');return
 env=os.environ.copy();env.update(WINEPREFIX=os.environ.get('A2FO_RUST_WINEPREFIX','/tmp/a2fo-shields-wine-prefix'),WINEDEBUG='-all',WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d');env.pop('DISPLAY',None);env.pop('WAYLAND_DISPLAY',None)
 outputs=[]
 for label,exe in zip(['before','current'],executables):
  output=run(['wine',str(exe)],env);(WORK/(label+'.txt')).write_bytes(output);outputs.append(output)
 assert outputs[0]==outputs[1],'core native transcript mismatch'
 assert all(sha(p)==initial[str(p)]for p in pinned),'core comparison input changed during run'
 result={'inputs':initial,'scope':'Info defaults provider, atomic lazy publication and immutable Delphi settings directory cache; inner startup wait mocked; unit remains partial','lines':len(outputs[0].splitlines()),'sha256':hashlib.sha256(outputs[0]).hexdigest(),'archive_sha256':sha(archive),'source_sha256':sha(ROOT/SOURCE)};(WORK/'parity.json').write_text(json.dumps(result,indent=2)+'\n');print('core settings native validation:',result['lines'],'equal traces: PASS')
if __name__=='__main__':main()
