#!/usr/bin/env python3
"""B4 parity; A2FO_RUST_WIN_LIB selects archive, A2FO_RUST_WINEPREFIX selects Wine prefix."""
from pathlib import Path
import hashlib,json,os,shlex,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/rust-migration-stage7/runtime-b-work'
BEFORE=ROOT/'build/rust-migration-stage7/before'
RUST=Path(os.environ.get("A2FO_RUST_WIN_LIB", WORK/"target/i686-pc-windows-gnu/release/liba2fo_core.a"))
WINEPREFIX=Path(os.environ.get("A2FO_RUST_WINEPREFIX", WORK/"wine")).resolve()
FLAGS=['-DWIN32_LEAN_AND_MEAN','-std=gnu++17','-O2','-Wall','-Wextra','-Wpedantic','-Werror']
def run(args,**kw):return subprocess.run(args,cwd=ROOT,check=True,**kw)
def wine(exe,*arguments):
    env=os.environ.copy();env.update({'WINEPREFIX':str(WINEPREFIX),'WINEDEBUG':'-all',
        'WINEDLLOVERRIDES':'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d'})
    env.pop('DISPLAY',None);env.pop('WAYLAND_DISPLAY',None)
    # Regular files avoid inherited Wine server pipes delaying process exit.
    with tempfile.TemporaryFile(mode='w+t') as output, tempfile.TemporaryFile(mode='w+t') as error:
        p=subprocess.run(['wine',str(exe),*arguments],cwd=ROOT,env=env,text=True,
                         stdout=output,stderr=error,timeout=90)
        output.seek(0);error.seek(0)
        stdout,stderr=output.read(),error.read()
    assert p.returncode==0,(exe,arguments,stdout,stderr)
    return stdout
def check_hashes():
    expected=json.loads((ROOT/'build/rust-migration-stage7/before-hashes.json').read_text())
    for path in ('modules/A2FOWeaponDamageControls/module.cpp','modules/A2FOPointDefenseCycles/module.cpp'):
        want=expected[path] if isinstance(expected[path],str) else expected[path]['sha256']
        got=hashlib.sha256((BEFORE/path).read_bytes()).hexdigest();assert got==want,(path,got,want)
        print(f'frozen {path}: {got}')
def point(revision):
    module=BEFORE/'modules/A2FOPointDefenseCycles/module.cpp' if revision=='before' else ROOT/'modules/A2FOPointDefenseCycles/module.cpp'
    exe=WORK/f'point_b4_{revision}.exe'
    run(['i686-w64-mingw32-g++',*FLAGS,f'-DMODULE_SOURCE="{module}"',
        'tests/rust_stage7_runtime_b4_parity.cpp','modules/A2FOPointDefenseCycles/firing_cycle.cpp',str(RUST),
        '-static','-ladvapi32','-o',str(exe)])
    result=''.join(f'mode:{mode}\n'+wine(exe,mode) for mode in ('success','signature','partial'))
    (WORK/f'point_b4_{revision}.txt').write_text(result)
    return result
def combat_command():
    out=run(['make','-Bn','build/builtin_combat_native_test.exe',f'RUST_WIN_LIB={RUST}'],capture_output=True,text=True).stdout
    command=shlex.split(next(line for line in out.splitlines() if '-o build/builtin_combat_native_test.exe ' in line))
    assert all(x in command for x in ('-std=gnu++17','-O2','-Werror'))
    return command
def combat(revision,command):
    module=BEFORE/'modules/A2FOWeaponDamageControls/module.cpp' if revision=='before' else ROOT/'modules/A2FOWeaponDamageControls/module.cpp'
    wrapper=WORK/f'combat_b4_{revision}_wrapper.cpp'
    wrapper.write_text((ROOT/'core/builtin_weapon_damage_controls.cpp').read_text().replace(
        '"../modules/A2FOWeaponDamageControls/module.cpp"',f'"{module}"'))
    obj=WORK/f'combat_b4_{revision}.o'
    run(['i686-w64-mingw32-g++',*FLAGS,'-DNDEBUG','-c',str(wrapper),'-o',str(obj)])
    source=(ROOT/'tests/builtin_combat_native_test.cpp').read_text()
    source=source.replace('"../core/builtin_modules.hpp"',f'"{ROOT/"core/builtin_modules.hpp"}"')
    source=source.replace('"../core/module_loader.hpp"',f'"{ROOT/"core/module_loader.hpp"}"')
    source=source.replace('void A2FO_CALL log_message(const char*,const char*) {}',
        'void A2FO_CALL log_message(const char*,const char* text) {std::printf("log:%s\\n",text?text:"");}')
    source=source.replace('cleanup();std::puts("builtin combat native: PASS");',
        'std::printf("counts:%u,%u,%u,%u,%u,%u\\n",begins,ends,registrations,hooks,patches,damages);cleanup();std::puts("builtin combat native: PASS");')
    generated=WORK/f'combat_b4_{revision}.cpp';generated.write_text(source)
    exe=WORK/f'combat_b4_{revision}.exe'
    args=[str(generated) if a=='tests/builtin_combat_native_test.cpp' else str(exe) if a=='build/builtin_combat_native_test.exe'
          else str(obj) if a=='build/builtin-native/core/builtin_weapon_damage_controls.cpp.o' else a for a in command]
    run(args)
    directional=ROOT/'build/fixtures/builtin-directional/A2FODirectionalShields.dll'
    result=''.join(f'mode:{mode}\n'+wine(exe,mode,str(directional)) for mode in
        ('success','legacy','rejected','signature-fail','dispatch-fail','craft-dispatch-fail','registration-fail','hook-fail'))
    (WORK/f'combat_b4_{revision}.txt').write_text(result)
    return result
if __name__=='__main__':
    WORK.mkdir(parents=True,exist_ok=True);check_hashes();assert RUST.is_file()
    cases={}
    for name,fn in [('point',point),('combat',lambda rev:combat(rev,combat_command()))]:
        before=fn('before');current=fn('current');assert before==current,name
        digest=hashlib.sha256(before.encode()).hexdigest();cases[name]={'before_sha256':digest,'current_sha256':digest}
        print(f'{name} frozen/current native transcript: PASS {digest}')
    receipt=WORK/'runtime-b4-parity.json'
    receipt.write_text(json.dumps({'compiler':'i686-w64-mingw32-g++','native_flags':['-std=gnu++17','-O2','-Werror'],
        'frozen_current_transcripts':cases},indent=2,sort_keys=True)+'\n')
    print(f'receipt: {receipt}')
