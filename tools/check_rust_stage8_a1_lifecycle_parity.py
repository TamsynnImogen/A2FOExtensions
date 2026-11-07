#!/usr/bin/env python3
"""Compare actual frozen/current A1 ModuleInit with native hook primitives mocked."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
STAGE=ROOT/'build/rust-migration-stage8'
WORK=STAGE/'a1-lifecycle-work'
SOURCE='modules/A1Compat/module.cpp'
FIXTURE=ROOT/'tests/rust_stage8_a1_lifecycle_native.cpp'
MOCKS={
    'enabled_in_mod_chain':34,
    'apply_legacy_native_resource_limits':55,
    'register_starting_resource_defaults':59,
    'reset_legacy_race_menu_state':63,
    'install_essential_gui_sprite_loader':70,
    'install_legacy_default_cursor_fallback':80,
    'install_legacy_tactical_cursor_fallback':82,
    'install_legacy_gameplay_ui_scaling':84,
    'install_legacy_tooltip_adapter':86,
    'install_legacy_cinematic_view_adapter':88,
    'install_legacy_control_panel_adapter':90,
    'install_legacy_resource_panel_adapter':92,
    'install_legacy_explore_command_bridge':105,
    'install_legacy_team_color_translation':107,
    'install_legacy_virtual_directory_normalizer':109,
    'install_legacy_moon_resource_defaults':111,
    'install_legacy_physics_defaults':113,
    'install_legacy_neutral_race_registry':115,
    'install_nebula_node_guard':122,
    'install_legacy_aip_name_bridge':124,
    'install_legacy_aip_missing_unit_guard':126,
    'install_a1_bzn_map_bounds_bridge':128,
    'install_a1_bzn_ai_mission_bridge':130,
    'install_a1_bzn_runtime_class_bridge':132,
    'install_a1_relationship_bridge':134,
    'install_craft_level_up_race_default':136,
    'install_starbase_officer_quarter_compatibility':138,
    'install_officer_upgrade_system':140,
    'install_producer_palette_race_filter':142,
    'install_legacy_ship_display_adapter':145,
}
def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(command,env=None,timeout=300):
    with tempfile.TemporaryFile() as out,tempfile.TemporaryFile() as err:
        try:result=subprocess.run(command,cwd=ROOT,env=env,stdout=out,stderr=err,timeout=timeout)
        except subprocess.TimeoutExpired as exc:
            out.seek(0);err.seek(0)
            raise RuntimeError(f'timeout: {command}\n{out.read()[-2000:]!r}\n{err.read()[-4000:]!r}') from exc
        out.seek(0);err.seek(0);output,error=out.read(),err.read()
    if result.returncode:
        raise RuntimeError(f'{command}: exit {result.returncode}\n{output[-2000:]!r}\n{error[-5000:]!r}')
    return output.replace(b'\r\n',b'\n')
def closing(text,start,opening,closing):
    depth=0;mode='normal';index=start
    while index<len(text):
        ch=text[index];nxt=text[index+1:index+2]
        if mode=='line':
            if ch=='\n':mode='normal'
        elif mode=='block':
            if ch=='*' and nxt=='/':mode='normal';index+=1
        elif mode in ('single','double'):
            if ch=='\\':index+=1
            elif (mode=='single' and ch=="'") or (mode=='double' and ch=='"'):mode='normal'
        elif ch=='/' and nxt=='/':mode='line';index+=1
        elif ch=='/' and nxt=='*':mode='block';index+=1
        elif ch=="'":mode='single'
        elif ch=='"':mode='double'
        elif ch==opening:depth+=1
        elif ch==closing:
            depth-=1
            if depth==0:return index
        index+=1
    raise RuntimeError('unbalanced native function')
def replace_function(text,name,body):
    pattern=re.compile(r'(?m)^(?:bool|void|LegacyStartingResourceDefaults)\s+'+re.escape(name)+r'\s*\(')
    definitions=[]
    for match in pattern.finditer(text):
        opened=text.index('(',match.start());ended=closing(text,opened,'(',')')
        next_open=text.find('{',ended+1)
        next_semi=text.find(';',ended+1)
        if next_open<0 or (0<=next_semi<next_open):continue
        definitions.append((next_open,closing(text,next_open,'{','}')))
    if len(definitions)!=1:raise RuntimeError(f'{name}: expected one definition, got {len(definitions)}')
    opened,ended=definitions[0]
    return text[:opened]+'{'+body+'}'+text[ended+1:]
def fixture_source(source):
    text=source.read_text().replace('__declspec(dllexport)','')
    text=re.sub(r'#include "([^"\n]+)"',
        lambda match:'#include "'+str((source.parent/match[1]).resolve())+'"',text)
    text=('extern "C" bool a2fo_a1_fixture_step(unsigned);\n'
        'extern "C" bool a2fo_a1_fixture_safe();\n'
        'extern "C" bool a2fo_a1_fixture_tactical();\n'
        'extern "C" bool a2fo_a1_fixture_mutate_resource();\n'+text)
    for name,id in MOCKS.items():
        body=f'return a2fo_a1_fixture_step({id});'
        if name=='reset_legacy_race_menu_state':body=f'(void)a2fo_a1_fixture_step({id});'
        if name=='install_legacy_ship_display_adapter':
            body=('const bool installed=a2fo_a1_fixture_step(145);'
                  'if(a2fo_a1_fixture_mutate_resource())g_legacy_resource_panel_runtime_ready=false;'
                  'return installed;')
        text=replace_function(text,name,body)
    text=replace_function(text,'load_starting_resource_defaults',
        '(void)a2fo_a1_fixture_step(58);return {};')
    text=replace_function(text,'read_a1compat_settings',
        'settings={};settings.safe_mode=a2fo_a1_fixture_safe();return a2fo_a1_fixture_step(65);')
    text=replace_function(text,'detect_incompatible_legacy_cursor_table',
        'source_path="fixture-cursor.spr";if(tactical_override_required)*tactical_override_required=a2fo_a1_fixture_tactical();return a2fo_a1_fixture_step(77);')
    return text
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-only',action='store_true')
    parser.add_argument('--lifecycle-lib',type=Path,help='isolated new-symbol Rust rlib')
    args=parser.parse_args();WORK.mkdir(parents=True,exist_ok=True)
    archive=Path(os.environ.get('A2FO_RUST_WIN_LIB',ROOT/'build/rust/i686-pc-windows-gnu/release/liba2fo_core.a')).resolve()
    before=STAGE/'before'/SOURCE
    if sha(before)!=json.loads((STAGE/'before-hashes.json').read_text())[SOURCE]:
        raise RuntimeError('frozen A1 source changed')
    pinned=[ROOT/SOURCE,ROOT/'rust/a2fo_core/src/stage8_a1_lifecycle.rs',
        ROOT/'rust/stage8_a1_lifecycle_ffi.h',FIXTURE,Path(__file__).resolve(),archive]
    if args.lifecycle_lib:pinned.append(args.lifecycle_lib.resolve())
    initial={str(path):sha(path) for path in pinned}
    executables=[]
    for label,source in [('before',before),('current',ROOT/SOURCE)]:
        generated=WORK/(label+'-lifecycle.cpp');generated.write_text(fixture_source(source))
        obj=WORK/(label+'.o');exe=WORK/(label+'.exe')
        flags=['i686-w64-mingw32-g++','-std=gnu++17','-O2','-Wall','-Wextra','-Wpedantic',
            '-Werror','-Wno-error=cast-function-type','-Wno-unused-function',
            '-Wno-unused-variable','-Wno-unused-parameter',
            '-DNDEBUG','-DWIN32_LEAN_AND_MEAN',
            '-ffunction-sections','-fdata-sections','-static']
        run(flags+[f'-DA1_SOURCE="{generated}"',str(FIXTURE),'-c','-o',str(obj)])
        undefined=subprocess.check_output(['i686-w64-mingw32-nm','--undefined-only',str(obj)],text=True)
        native=[]
        for line in undefined.splitlines():
            name=line.split()[-1]
            if name.startswith(('_a2fo_a1_','__ZN8a1compat','__ZNK8a1compat')):native.append(name)
        stubs=WORK/(label+'-uninvoked.S')
        stubs.write_text('.text\n'+''.join('.globl '+name+'\n'+name+':\n jmp _a2fo_a1_fixture_unexpected_native\n' for name in native))
        trap=WORK/'uninvoked.cpp'
        trap.write_text('#include <cstdlib>\n#include <cstdio>\nextern "C" [[noreturn]] void a2fo_a1_fixture_unexpected_native(){std::fputs("unexpected native A1 engine path\\n",stderr);std::abort();}\n')
        libs=[str(args.lifecycle_lib.resolve())] if args.lifecycle_lib else []
        libs.append(str(archive))
        run(['i686-w64-mingw32-g++','-static','-Wl,--gc-sections',str(obj),str(stubs),str(trap),*libs,
            '-ladvapi32','-lgdi32','-lcomctl32','-lversion','-lws2_32','-lshell32','-lwinmm',
            '-lole32','-lmsimg32','-lgdiplus','-ldxguid','-o',str(exe)])
        executables.append(exe)
    if any(sha(path)!=initial[str(path)] for path in pinned):raise RuntimeError('A1 lifecycle input drift')
    if args.build_only:
        print('A1 lifecycle frozen/current actual-source GNU++17/O2/Werror fixtures build: PASS')
        return
    prefix=Path(os.environ.get('A2FO_RUST_WINEPREFIX','/tmp/a2fo-shields-wine-prefix'))
    if not prefix.is_absolute() or not (prefix/'system.reg').is_file():
        raise RuntimeError('initialized absolute private Wine prefix required')
    env=os.environ.copy();env.update(WINEPREFIX=str(prefix),WINEDEBUG='-all',DISPLAY='',WAYLAND_DISPLAY='',
        WINEDLLOVERRIDES='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d')
    outputs=[]
    for label,exe in zip(('before','current'),executables):
        output=run(['wine',str(exe)],env=env,timeout=180)
        (WORK/(label+'.txt')).write_bytes(output);outputs.append(output)
    if outputs[0]!=outputs[1]:
        old,new=(output.splitlines() for output in outputs)
        for index in range(max(len(old),len(new))):
            if old[index:index+1]!=new[index:index+1]:
                raise RuntimeError(f'first difference line {index+1}: {old[index:index+1]} != {new[index:index+1]}')
        raise RuntimeError('A1 lifecycle transcript mismatch')
    if any(sha(path)!=initial[str(path)] for path in pinned):raise RuntimeError('A1 lifecycle input drift')
    receipt={'scope':'A1_ModuleInit only; unsafe native hook helpers mocked in both actual sources',
        'inputs':initial,'lines':len(outputs[0].splitlines()),
        'sha256':hashlib.sha256(outputs[0]).hexdigest(),'compiler':'GNU++17/O2/Werror'}
    (WORK/'parity.json').write_text(json.dumps(receipt,indent=2)+'\n')
    print('A1 lifecycle frozen/current native traces:',receipt['lines'],'PASS')
if __name__=='__main__':main()
