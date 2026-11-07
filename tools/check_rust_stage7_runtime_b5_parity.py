#!/usr/bin/env python3
"""B5 parity; A2FO_RUST_WIN_LIB selects archive, A2FO_RUST_WINEPREFIX selects Wine prefix."""
from pathlib import Path
import hashlib,json,os,shlex,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/rust-migration-stage7/runtime-b-work'
BEFORE=ROOT/'build/rust-migration-stage7/before'
RUST=Path(os.environ.get("A2FO_RUST_WIN_LIB", WORK/"target/i686-pc-windows-gnu/release/liba2fo_core.a"))
WINEPREFIX=Path(os.environ.get("A2FO_RUST_WINEPREFIX", WORK/"wine-b5")).resolve()
def run(args,**kw):return subprocess.run(args,cwd=ROOT,check=True,**kw)
def wine(exe,mode):
    env=os.environ.copy();env.update({'WINEPREFIX':str(WINEPREFIX),'WINEDEBUG':'-all',
        'WINEDLLOVERRIDES':'winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d'})
    env.pop('DISPLAY',None);env.pop('WAYLAND_DISPLAY',None)
    # The persistent wineserver can inherit pipes and delay communicate()
    # until its keepalive expires. Regular files keep each fixture bounded.
    with tempfile.TemporaryFile(mode='w+t') as output, tempfile.TemporaryFile(mode='w+t') as error:
        p=subprocess.run(['wine',str(exe),mode],cwd=ROOT,env=env,text=True,
                         stdout=output,stderr=error,timeout=90)
        output.seek(0);error.seek(0)
        stdout,stderr=output.read(),error.read()
    assert p.returncode==0,(exe,mode,stdout,stderr)
    return stdout
def hashes():
    expected=json.loads((ROOT/'build/rust-migration-stage7/before-hashes.json').read_text())
    for path in ('modules/A2FOInstantActionSettings/module.cpp','modules/A2FOEditMenu/module.cpp'):
        want=expected[path] if isinstance(expected[path],str) else expected[path]['sha256']
        got=hashlib.sha256((BEFORE/path).read_bytes()).hexdigest();assert got==want,(path,got,want)
        print(f'frozen {path}: {got}')
def linker(target):
    output=run(['make','-Bn',target,f'RUST_WIN_LIB={RUST}'],capture_output=True,text=True).stdout
    command=shlex.split(next(line for line in output.splitlines() if f'-o {target} ' in line))
    assert all(flag in command for flag in ('-std=gnu++17','-O2','-Werror'))
    return command
def edit_extra(source):
    source=source.replace('namespace {\nusing namespace a2fo;',
        'namespace {\nextern "C" void* __cdecl fail_native_alloc(std::size_t) {return nullptr;}\nusing namespace a2fo;',1)
    marker='    check(!g_scratch_installed && g_breadcrumbs.empty() && installs == 1,\n          "off/on inside native Update invalidates pending custom navigation");'
    extra='''
    // Exercise the production navigation path with an injected engine allocator
    // failure; only synthetic memory and a private mock image are touched.
    a2fo::edit_menu::MenuNode child{};
    child.source_name="child.odf"; child.build_items[0]="leaf.odf";
    g_nodes["child.odf"]=std::make_unique<a2fo::edit_menu::MenuNode>(child);
    native_root->build_menus[0].source_name=const_cast<char*>("child.odf");
    const std::uintptr_t alloc_address=reinterpret_cast<std::uintptr_t>(&fail_native_alloc);
    std::uint8_t jump[]{0xb8,0,0,0,0,0xff,0xe0};
    std::memcpy(jump+1,&alloc_address,sizeof(alloc_address));
    DWORD old_protection=0;
    check(VirtualProtect(image+kEngineOperatorNewRva,sizeof(jump),PAGE_EXECUTE_READWRITE,
                         &old_protection),"mock allocator page");
    std::memcpy(image+kEngineOperatorNewRva,jump,sizeof(jump));
    std::int32_t root_index=0,build_index=0;
    std::memcpy(menu.data()+kEditMenuSelectedRootOffset,&root_index,sizeof(root_index));
    std::memcpy(menu.data()+kEditMenuSelectedBuildItemOffset,&build_index,sizeof(build_index));
    enter_selected_submenu(menu.data());
    check(!g_scratch_installed && g_breadcrumbs.empty(),"allocation failure rolls back breadcrumb");
    std::printf("injected-allocation-failure:%u,%u\\n",g_scratch_installed?1u:0u,
                static_cast<unsigned>(g_breadcrumbs.size()));
    native_root->build_menus[0].source_name=nullptr;
    g_nodes.clear();
'''
    assert marker in source
    return source.replace(marker,marker+extra)
def fixture(name,revision):
    module_name='A2FOInstantActionSettings' if name=='instant' else 'A2FOEditMenu'
    wrapper_name='builtin_instant_action_settings' if name=='instant' else 'builtin_edit_menu'
    target=f'build/builtin_{"instant_action" if name=="instant" else "edit_menu"}_native_test.exe'
    source_name=f'tests/builtin_{"instant_action" if name=="instant" else "edit_menu"}_native_test.cpp'
    module=BEFORE/f'modules/{module_name}/module.cpp' if revision=='before' else ROOT/f'modules/{module_name}/module.cpp'
    wrapper=WORK/f'{name}_b5_{revision}_wrapper.cpp'
    wrapper.write_text((ROOT/f'core/{wrapper_name}.cpp').read_text().replace(
        f'"../modules/{module_name}/module.cpp"',f'"{module}"'))
    source=(ROOT/source_name).read_text().replace(f'"../core/{wrapper_name}.cpp"',f'"{wrapper}"')
    source=source.replace('"../core/builtin_modules.hpp"',f'"{ROOT/"core/builtin_modules.hpp"}"')
    source=source.replace('"../core/module_loader.hpp"',f'"{ROOT/"core/module_loader.hpp"}"')
    if name=='edit':
        source=edit_extra(source)
        source=source.replace('void A2FO_CALL logger(const char*, const char*) {}',
            'void A2FO_CALL logger(const char*,const char* text){std::printf("log:%s\\n",text?text:"");}')
        source=source.replace('        std::puts("builtin EditMenu native fixture: PASS");',
            '        std::printf("state:%u,%u\\n",installs,native_updates);\n        std::puts("builtin EditMenu native fixture: PASS");')
    else:
        source=source.replace('        std::printf("builtin Instant Action %s: PASS\\n", argv[1]);',
            '        std::printf("state:%u,%u,%u,%u\\n",hooks,native_loads,native_reads,native_openings);\n'
            '        std::printf("builtin Instant Action %s: PASS\\n", argv[1]);')
    generated=WORK/f'{name}_b5_{revision}.cpp';generated.write_text(source)
    exe=WORK/f'{name}_b5_{revision}.exe'
    command=linker(target)
    args=[str(generated) if a==source_name else str(exe) if a==target else a for a in command]
    run(args)
    modes=('success','identity','signature','api','fleet','fail1','fail2','fail3','fail4','fail5','required','rejected','legacy') if name=='instant' else ('success','signature','hook-fail')
    transcript=''.join(f'mode:{mode}\n'+wine(exe,mode) for mode in modes)
    (WORK/f'{name}_b5_{revision}.txt').write_text(transcript)
    return transcript
if __name__=='__main__':
    WORK.mkdir(parents=True,exist_ok=True);hashes()
    assert RUST.is_file(),RUST
    cases={}
    for name in ('edit','instant'):
        before=fixture(name,'before');current=fixture(name,'current');assert before==current,name
        digest=hashlib.sha256(before.encode()).hexdigest()
        cases[name]={'before_sha256':digest,'current_sha256':digest}
        print(f'{name} frozen/current native transcript: PASS {digest}')
    receipt=WORK/'runtime-b5-parity.json'
    receipt.write_text(json.dumps({'compiler':'i686-w64-mingw32-g++',
        'native_flags':['-std=gnu++17','-O2','-Werror'],
        'frozen_current_transcripts':cases},indent=2,sort_keys=True)+'\n')
    print(f'receipt: {receipt}')
