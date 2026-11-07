#!/usr/bin/env python3
"""Actual-source deferred entrypoint/worker oracle; unsafe inner startup mocked."""
from pathlib import Path
import argparse,hashlib,json,os,re,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1];STAGE=ROOT/'build/rust-migration-stage8';WORK=STAGE/'core-lifecycle-work';SOURCE='core/dllmain.cpp'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(args,env=None):
 with tempfile.TemporaryFile()as out,tempfile.TemporaryFile()as err:
  result=subprocess.run(args,cwd=ROOT,env=env,stdout=out,stderr=err,timeout=300);out.seek(0);err.seek(0);output,error=out.read(),err.read()
 if result.returncode:raise RuntimeError(f'{args}: {result.returncode}\n{output[-3000:]!r}\n{error[-5000:]!r}')
 return output.replace(b'\r\n',b'\n')
def replace_body(text,signature,body):
 start=text.index(signature)+len(signature);level=1;index=start;quote=None;comment=None
 while level:
  ch=text[index];nxt=text[index+1:index+2]
  if comment=='line':
   if ch=='\n':comment=None
  elif comment=='block':
   if ch=='*'and nxt=='/':comment=None;index+=1
  elif quote:
   if ch=='\\':index+=1
   elif ch==quote:quote=None
  elif ch=='/'and nxt in ('/','*'):comment='line'if nxt=='/'else'block';index+=1
  elif ch in ('"',"'"):quote=ch
  elif ch=='{':level+=1
  elif ch=='}':level-=1
  index+=1
 return text[:start]+'\n'+body+'\n'+text[index-1:]
def main():
 parser=argparse.ArgumentParser();parser.add_argument('--build-only',action='store_true');args=parser.parse_args();WORK.mkdir(parents=True,exist_ok=True)
 archive=Path(os.environ.get('A2FO_RUST_WIN_LIB',ROOT/'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a')).resolve();before=STAGE/'before'/SOURCE
 assert sha(before)==json.loads((STAGE/'before-hashes.json').read_text())[SOURCE]
 pinned=[ROOT/SOURCE,ROOT/'rust/a2fo_core/src/stage8_core_lifecycle.rs',ROOT/'rust/stage8_core_lifecycle_ffi.h',ROOT/'tests/rust_stage8_core_lifecycle_native.cpp',Path(__file__).resolve(),archive];initial={str(p):sha(p)for p in pinned};executables=[]
 for label,source in [('before',before),('current',ROOT/SOURCE)]:
  text=source.read_text().replace('__declspec(dllexport)','')
  for signature,body in [('void open_log() {','fixture_open_log();'),('void log_line(const std::string& text) {','fixture_log(text);'),('std::string module_directory(HMODULE module) {','(void)module;return fixture_directory();'),('DWORD WINAPI initialize(void*) {','return fixture_initialize();'),('void run_shell_display_monitor() {','fixture_monitor();')]:text=replace_body(text,signature,body)
  text=re.sub(r'\bDllMain\b','a2fo_uninvoked_fixture_DllMain',text);text=re.sub(r'\bA2FO_Initialize\b','a2fo_uninvoked_fixture_Initialize',text)
  text=re.sub(r'#include "([^"\n]+)"',lambda m:'#include "'+str((source.parent/m[1]).resolve())+'"',text);generated=WORK/(label+'-core.cpp');generated.write_text(text);obj=WORK/(label+'.o');exe=WORK/(label+'.exe')
  flags=['i686-w64-mingw32-g++','-std=gnu++17','-O2','-Wall','-Wextra','-Wpedantic','-Werror','-Wno-unused-function','-Wno-unused-variable','-DNDEBUG','-DWIN32_LEAN_AND_MEAN','-ffunction-sections','-fdata-sections','-static']
  run(flags+[f'-DCORE_SOURCE="{generated}"',str(ROOT/'tests/rust_stage8_core_lifecycle_native.cpp'),'-c','-o',str(obj)])
  undefined=subprocess.check_output(['i686-w64-mingw32-nm','--undefined-only',str(obj)],text=True);native=[]
  for line in undefined.splitlines():
   name=line.split()[-1]
   if name.startswith(('__ZN4a2fo','__ZNK4a2fo','_a2fo_'))and not name.startswith('_a2fo_rs_'):native.append(name)
  stubs=WORK/(label+'-uninvoked.S');stubs.write_text('.text\n'+''.join('.globl '+n+'\n'+n+':\n jmp _a2fo_fixture_unexpected_native\n'for n in native));trap=WORK/'uninvoked.cpp';trap.write_text('#include <cstdlib>\nextern "C" [[noreturn]] void a2fo_fixture_unexpected_native(){std::abort();}\n')
  run([flags[0],'-static','-Wl,--gc-sections',str(obj),str(stubs),str(trap),str(archive),'-ladvapi32','-lgdi32','-lcomctl32','-lversion','-lws2_32','-lshell32','-lwinmm','-lole32','-lmsimg32','-lgdiplus','-ldxguid','-o',str(exe)]);executables.append(exe)
 if args.build_only:print('core deferred lifecycle whole-source fixture builds: PASS');return
 env=os.environ.copy();env.update(WINEPREFIX=os.environ.get('A2FO_RUST_WINEPREFIX','/tmp/a2fo-shields-wine-prefix'),WINEDEBUG='-all',WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d');env.pop('DISPLAY',None);env.pop('WAYLAND_DISPLAY',None);outputs=[]
 for label,exe in zip(['before','current'],executables):
  output=run(['wine',str(exe)],env);(WORK/(label+'.txt')).write_bytes(output);outputs.append(output)
 if outputs[0]!=outputs[1]:
  old,new=(x.splitlines()for x in outputs)
  for i in range(max(len(old),len(new))):
   if old[i:i+1]!=new[i:i+1]:raise RuntimeError(f'first difference {i+1}: {old[i:i+1]} != {new[i:i+1]}')
 assert all(sha(p)==initial[str(p)]for p in pinned),'inputs changed'
 result={'scope':'Deferred export and worker only; native inner startup/Win32 primitives mocked','inputs':initial,'lines':len(outputs[0].splitlines()),'sha256':hashlib.sha256(outputs[0]).hexdigest()};(WORK/'parity.json').write_text(json.dumps(result,indent=2)+'\n');print('core deferred lifecycle:',result['lines'],'matching traces PASS')
if __name__=='__main__':main()
