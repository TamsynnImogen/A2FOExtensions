#!/usr/bin/env python3
"""Actual frozen/current native MultiYard and Muzzle module fixtures."""
from pathlib import Path
import difflib
import os
import subprocess

ROOT=Path(__file__).resolve().parent.parent
WORK=ROOT/'build/rust-migration-stage7/runtime-a-work'
BEFORE=ROOT/'build/rust-migration-stage7/before'
LIB=Path(os.environ.get('A2FO_RUST_WIN_LIB',WORK/'cargo/i686-pc-windows-gnu/release/liba2fo_core.a'))
CXX='i686-w64-mingw32-g++'
FLAGS=['-DWIN32_LEAN_AND_MEAN','-std=gnu++17','-O2','-Wall','-Wextra',
       '-Wpedantic','-static','-static-libgcc','-static-libstdc++']
LINK=['-lws2_32','-lbcrypt','-lole32','-ladvapi32']
def run(args,**kwargs):
    print('+',' '.join(map(str,args)),flush=True)
    return subprocess.run(args,cwd=ROOT,check=True,**kwargs)
def environment():
    env=os.environ.copy();env.pop('DISPLAY',None);env.pop('WAYLAND_DISPLAY',None);env['WINEPREFIX']=str(os.environ.get('A2FO_RUST_WINEPREFIX', WORK/'wine-prefix'));env['WINEDEBUG']='-all'
    env['WINEDLLOVERRIDES']='winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d'
    return env
def multi_fixture():
    original=(ROOT/'tests/multi_yard_native_test.cpp').read_text()
    changed=original.replace('#include "../modules/A2FOMultiYardBuild/module.cpp"',
                             '#include MODULE_SOURCE',1)
    provider='''
    HMODULE pack=LoadLibraryA("build/rust-migration-stage7/runtime-a-work/A2FOFeaturePack.dll");
    assert(pack);using Mode=void (__cdecl*)(int);Mode set_mode=nullptr;
    FARPROC address=GetProcAddress(pack,"FixtureQueueMode");
    std::memcpy(&set_mode,&address,sizeof(address));assert(set_mode);
    for(int i=0;i<3;++i)put(objects[i],0x40,yard_class);
    void* provider_selected[3]{objects[0],objects[1],objects[2]};
    commands=0;g_last_built=0;set_mode(0);
    multi_yard_build_route(provider_selected,3,yard_class);
    assert(commands==1&&command_target==objects[1]);
    set_mode(1);g_last_built=0;
    put(objects[0],0x200,std::uint32_t{0});
    put(objects[1],0x200,std::uint32_t{2});
    put(objects[2],0x200,std::uint32_t{3});
    commands=0;multi_yard_build_route(provider_selected,3,yard_class);
    assert(commands==1&&command_target==objects[0]);
    FreeLibrary(pack);std::puts("provider queue and native fallback passed");
'''
    changed=changed.replace('    A2FO_ModuleShutdown(); assert(!g_active);',
        provider+'    A2FO_ModuleShutdown(); assert(!g_active);',1)
    fixture=WORK/'multi_yard_stage7_fixture.cpp';fixture.write_text(changed)
    return fixture
def compare(name,flavor,fixture,bridges,images=()):
    transcripts=[]
    for version in ('before','current'):
        source=(BEFORE if version=='before' else ROOT)/'modules'/name/'module.cpp'
        executable=WORK/f'{name}-{flavor}-{version}.exe'
        command=[CXX,*FLAGS,f'-DMODULE_SOURCE="{source}"']
        if flavor=='builtin':
            command+=['-DTEST_BUILTIN','-DA2FO_BUILTIN_FEATURE=a2fo::BuiltinFeature::muzzle_flashes']
        command+=['-o',str(executable),str(fixture),*map(str,bridges),str(LIB),*LINK]
        run(command)
        result=wine_capture(['wine',str(executable),*map(str,images)],env=environment(),
                   text=True,timeout=120)
        transcript=result.stdout.replace('\r\n','\n')
        (WORK/f'{name}-{flavor}-{version}.txt').write_text(transcript)
        transcripts.append(transcript)
    if transcripts[0]!=transcripts[1]:
        print(''.join(difflib.unified_diff(transcripts[0].splitlines(True),
            transcripts[1].splitlines(True),fromfile='frozen',tofile='current')))
        raise RuntimeError(f'{name} {flavor} trace mismatch')
    print(f'{name} {flavor}: {len(transcripts[0].splitlines())} matching trace lines',flush=True)
def wine_capture(args, env, timeout=120, text=False, check=True):
    import tempfile
    print('+', ' '.join(map(str, args)), flush=True)
    mode = 'w+t' if text else 'w+b'
    with tempfile.TemporaryFile(mode=mode) as stdout, tempfile.TemporaryFile(mode=mode) as stderr:
        result = subprocess.run(args, cwd=ROOT, env=env, stdout=stdout, stderr=stderr,
                                timeout=timeout)
        stdout.seek(0); stderr.seek(0)
        output, errors = stdout.read(), stderr.read()
    if check and result.returncode:
        raise RuntimeError(f'Wine fixture exited {result.returncode}: {args}\n{output}\n{errors}')
    return subprocess.CompletedProcess(args, result.returncode, output, errors)

def main():
    # This source includes the original assertion-heavy fixture, so NDEBUG is
    # deliberately absent. The module has no NDEBUG branches; GNU++17/O2 match
    # production code generation while keeping the fixture assertions active.
    run([CXX,*FLAGS,'-shared','-DNDEBUG','-o',str(WORK/'A2FOFeaturePack.dll'),
        str(ROOT/'tests/rust_stage7_runtime_a4_yard_provider.cpp')])
    compare('A2FOMultiYardBuild','standalone',multi_fixture(),
        [ROOT/'modules/A2FOMultiYardBuild/bridges.S',ROOT/'tests/multi_yard_bridge_fixture.S'],
        [ROOT/'ArmadaL.exe',ROOT/'FleetOpsHook.dll'])
    fixture=ROOT/'tests/rust_stage7_runtime_a4_muzzle_parity.cpp'
    for flavor in ('standalone','builtin'):
        compare('A2FOMuzzleFlashes',flavor,fixture,
            [ROOT/'modules/A2FOMuzzleFlashes/thiscall_bridge.S'])
if __name__=='__main__':main()
