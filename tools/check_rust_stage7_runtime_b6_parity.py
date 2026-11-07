#!/usr/bin/env python3
"""B6 parity; A2FO_RUST_WIN_LIB selects archive, A2FO_RUST_WINEPREFIX selects Wine prefix."""
from pathlib import Path
import hashlib,json,os,shlex,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/rust-migration-stage7/runtime-b-work'
BEFORE=ROOT/'build/rust-migration-stage7/before'
RUST=Path(os.environ.get("A2FO_RUST_WIN_LIB", WORK/"target/i686-pc-windows-gnu/release/liba2fo_core.a"))
WINEPREFIX=Path(os.environ.get("A2FO_RUST_WINEPREFIX", WORK/"wine")).resolve()
TARGET='build/builtin_odf_variants_native_test.exe'
SOURCE='tests/builtin_odf_variants_native_test.cpp'
def run(args,**kw):return subprocess.run(args,cwd=ROOT,check=True,**kw)
def wine(exe,mode):
    env=os.environ.copy();env.update({'WINEPREFIX':str(WINEPREFIX),'WINEDEBUG':'-all',
        'WINEDLLOVERRIDES':'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d'})
    env.pop('DISPLAY',None);env.pop('WAYLAND_DISPLAY',None)
    # Regular files avoid inherited Wine server pipes delaying process exit.
    with tempfile.TemporaryFile(mode='w+t') as output, tempfile.TemporaryFile(mode='w+t') as error:
        p=subprocess.run(['wine',str(exe),mode],cwd=ROOT,env=env,text=True,
                         stdout=output,stderr=error,timeout=90)
        output.seek(0);error.seek(0)
        stdout,stderr=output.read(),error.read()
    assert p.returncode==0,(exe,mode,stdout,stderr)
    return stdout
def frozen_hash():
    path='modules/A2FOODFVariants/module.cpp'
    expected=json.loads((ROOT/'build/rust-migration-stage7/before-hashes.json').read_text())
    want=expected[path] if isinstance(expected[path],str) else expected[path]['sha256']
    got=hashlib.sha256((BEFORE/path).read_bytes()).hexdigest();assert got==want,(got,want)
    print(f'frozen {path}: {got}')
def linker():
    out=run(['make','-Bn',TARGET,f'RUST_WIN_LIB={RUST}'],capture_output=True,text=True).stdout
    command=shlex.split(next(line for line in out.splitlines() if f'-o {TARGET} ' in line))
    assert all(x in command for x in ('-std=gnu++17','-O2','-Werror'))
    return command
def extend(source):
    source=source.replace('unsigned registrations = 0;',
        'unsigned registrations = 0;\nunsigned constructs=0,expires=0,adds=0,swaps=0;\n'
        'bool provide_transform=false,have_mission=false;\n'
        'std::array<float,12> transform_matrix{};\nstd::array<std::uint8_t,0x300> replacement_storage{};\n'
        'void* mission_vtable[7]{};void** mission_object=mission_vtable;')
    source=source.replace('void* __fastcall native_transform(void*,void*) { return nullptr; }',
        '''void* __fastcall native_transform(void*,void*) {
            return provide_transform?transform_matrix.data():nullptr;
        }
        void* __fastcall native_construct(void*,void*,const void*,int,int,void*) {
            ++constructs;return replacement_storage.data();
        }
        std::uintptr_t __fastcall native_expire(void*,void*) {++expires;return 1;}
        void* __cdecl native_mission() {return have_mission?&mission_object:nullptr;}
        std::uintptr_t __fastcall native_add(void*,void*,void*) {++adds;return 1;}
        std::uintptr_t __fastcall native_swap(void*,void*,void*) {++swaps;return 1;}''')
    marker='''    require(g_base_odfs[craft.data()]=="ship" &&
            std::find(queried_classes.begin(),queried_classes.end(),"ship_r")!=queried_classes.end() &&
            std::find(queried_classes.begin(),queried_classes.end(),"ship_b_r")==queried_classes.end(),
            "first simulation infers canonical base without stacking owner suffixes");'''
    extra='''
    provide_transform=true;
    jump(kGameObjectClassConstructRva,reinterpret_cast<void*>(&native_construct));
    jump(kCraftDoExpireRva,reinterpret_cast<void*>(&native_expire));
    jump(kAiMissionGetCurrentRva,reinterpret_cast<void*>(&native_mission));
    jump(kEvolverSwapObjectsRva,reinterpret_cast<void*>(&native_swap));
    mission_vtable[6]=reinterpret_cast<void*>(&native_add);
    require(!swap_craft_odf(craft.data(),new_race.data()) &&
            constructs==1 && expires==1 && adds==0 && swaps==0,
            "missing mission expires replacement after construction");
    have_mission=true;
    require(swap_craft_odf(craft.data(),new_race.data()) &&
            constructs==2 && expires==2 && adds==1 && swaps==1 &&
            g_base_odfs[replacement_storage.data()]=="ship" &&
            g_last_races[replacement_storage.data()]==new_race.data() &&
            g_base_odfs.count(craft.data())==0,
            "native add/swap/state handoff and source expiry");
    std::printf("swap:%u,%u,%u,%u\\n",constructs,expires,adds,swaps);
'''
    assert marker in source
    source=source.replace(marker,marker+extra)
    source=source.replace('void A2FO_CALL log_message(const char*, const char*) {}',
        'void A2FO_CALL log_message(const char*,const char* text){std::printf("log:%s\\n",text?text:"");}')
    source=source.replace('try { run(argv[1]); std::printf("builtin ODF variants %s: PASS\\n", argv[1]); return 0; }',
        'try { run(argv[1]); std::printf("state:%u,%u\\n",registrations,static_cast<unsigned>(queried_classes.size())); '
        'std::printf("builtin ODF variants %s: PASS\\n", argv[1]); return 0; }')
    return source
def fixture(revision,command):
    module=BEFORE/'modules/A2FOODFVariants/module.cpp' if revision=='before' else ROOT/'modules/A2FOODFVariants/module.cpp'
    wrapper=WORK/f'odf_b6_{revision}_wrapper.cpp'
    wrapper.write_text((ROOT/'core/builtin_odf_variants.cpp').read_text().replace(
        '"../modules/A2FOODFVariants/module.cpp"',f'"{module}"'))
    source=(ROOT/SOURCE).read_text().replace('"../core/builtin_odf_variants.cpp"',f'"{wrapper}"')
    source=source.replace('"../core/builtin_modules.hpp"',f'"{ROOT/"core/builtin_modules.hpp"}"')
    generated=WORK/f'odf_b6_{revision}.cpp';generated.write_text(extend(source))
    exe=WORK/f'odf_b6_{revision}.exe'
    args=[str(generated) if a==SOURCE else str(exe) if a==TARGET else a for a in command]
    run(args)
    transcript=''.join(f'mode:{mode}\n'+wine(exe,mode) for mode in ('success','signature','registration','dispatch'))
    (WORK/f'odf_b6_{revision}.txt').write_text(transcript)
    return transcript
if __name__=='__main__':
    WORK.mkdir(parents=True,exist_ok=True);frozen_hash()
    assert RUST.is_file(),RUST
    command=linker();before=fixture('before',command);current=fixture('current',command)
    assert before==current,'ODF frozen/current native transcript'
    digest=hashlib.sha256(before.encode()).hexdigest()
    receipt=WORK/'runtime-b6-parity.json'
    receipt.write_text(json.dumps({'compiler':'i686-w64-mingw32-g++',
        'native_flags':['-std=gnu++17','-O2','-Werror'],
        'frozen_current_transcripts':{'odf':{'before_sha256':digest,'current_sha256':digest}}},
        indent=2,sort_keys=True)+'\n')
    print(f'ODF frozen/current native transcript: PASS {digest}')
    print(f'receipt: {receipt}')
