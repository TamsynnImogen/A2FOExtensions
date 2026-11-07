#!/usr/bin/env python3
"""B3 parity; A2FO_RUST_WIN_LIB selects archive, A2FO_RUST_WINEPREFIX selects Wine prefix."""
from pathlib import Path
import hashlib,json,os,shlex,subprocess,sys,tempfile

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/"build/rust-migration-stage7/runtime-b-work"
BEFORE=ROOT/"build/rust-migration-stage7/before"
RUST=Path(os.environ.get("A2FO_RUST_WIN_LIB", WORK/"target/i686-pc-windows-gnu/release/liba2fo_core.a"))
WINEPREFIX=Path(os.environ.get("A2FO_RUST_WINEPREFIX", WORK/"wine")).resolve()

def run(args,**kwargs):return subprocess.run(args,cwd=ROOT,check=True,**kwargs)
def linker(target):
    output=run(["make","-Bn",target,f"RUST_WIN_LIB={RUST}"],capture_output=True,text=True).stdout
    command=shlex.split(next(line for line in output.splitlines() if f"-o {target} " in line))
    assert all(flag in command for flag in ("-std=gnu++17","-O2","-Werror"))
    return command
def wine(exe,mode):
    env=os.environ.copy();env.update({"WINEPREFIX":str(WINEPREFIX),"WINEDEBUG":"-all"})
    env.pop("DISPLAY",None);env.pop("WAYLAND_DISPLAY",None)
    # Capture to regular files so wineserver cannot keep communicate() open.
    with tempfile.TemporaryFile(mode="w+t") as output, tempfile.TemporaryFile(mode="w+t") as error:
        p=subprocess.run(["wine",str(exe),mode],cwd=ROOT,env=env,text=True,
                         stdout=output,stderr=error,timeout=90)
        output.seek(0);error.seek(0)
        stdout,stderr=output.read(),error.read()
    assert p.returncode==0,(exe.name,mode,stdout,stderr)
    return stdout
def hashes():
    expected=json.loads((ROOT/"build/rust-migration-stage7/before-hashes.json").read_text())
    for path in ("modules/A2FOAnimatedHardpoints/module.cpp","modules/A2FOAnimations/module.cpp"):
        want=expected[path] if isinstance(expected[path],str) else expected[path]["sha256"]
        actual=hashlib.sha256((BEFORE/path).read_bytes()).hexdigest()
        assert actual==want,(path,actual,want)
        print(f"frozen {path}: {actual}")
def operation_order():
    archive=WORK/"libmodule_runtime_b3_host.a"
    exe=WORK/"runtime_b3_operation_order"
    run(["rustc","--edition=2024","--crate-type=staticlib","-C","opt-level=2",
        "rust/a2fo_core/src/module_runtime_b3.rs","-o",str(archive)])
    run(["g++","-std=gnu++17","-O2","-Wall","-Wextra","-Werror",
        "tests/rust_stage7_runtime_b3_parity.cpp",str(archive),"-ldl","-lpthread","-lm","-o",str(exe)])
    run([str(exe)])
def ensure_rust(command):
    if str(RUST) in command:return command
    index=next((i for i,a in enumerate(command) if a.startswith("-ladvapi32")),len(command))
    return command[:index]+[str(RUST)]+command[index:]
def direct(name,target,source,wrapper,module,modes):
    command=linker(target);outputs=[]
    for revision in ("before","current"):
        included=WORK/f"{name}_{revision}_wrapper.cpp"
        text=(ROOT/f"core/{wrapper}.cpp").read_text()
        chosen=BEFORE/f"modules/{module}/module.cpp" if revision=="before" else ROOT/f"modules/{module}/module.cpp"
        included.write_text(text.replace(f'"../modules/{module}/module.cpp"',f'"{chosen}"'))
        content=(ROOT/source).read_text()
        content=content.replace(f'"../core/{wrapper}.cpp"',f'"{included}"')
        content=content.replace('"../core/builtin_modules.hpp"',f'"{ROOT/"core/builtin_modules.hpp"}"')
        generated=WORK/f"{name}_{revision}.cpp";generated.write_text(content)
        exe=WORK/f"{name}_{revision}.exe"
        args=[str(generated) if a==source else str(exe) if a==target else a for a in command]
        run(ensure_rust(args))
        transcript="".join(f"mode:{mode}\n"+wine(exe,mode) for mode in modes)
        (WORK/f"{name}_{revision}.txt").write_text(transcript);outputs.append(transcript)
    assert outputs[0]==outputs[1],name
    print(f"{name} frozen/current native transcript: PASS")
def visual():
    name="hardpoints_visual";target="build/builtin_visual_native_test.exe"
    command=linker(target);outputs=[]
    for revision in ("before","current"):
        wrapper=WORK/f"{name}_{revision}_wrapper.cpp"
        text=(ROOT/"core/builtin_animated_hardpoints.cpp").read_text()
        module=BEFORE/"modules/A2FOAnimatedHardpoints/module.cpp" if revision=="before" else ROOT/"modules/A2FOAnimatedHardpoints/module.cpp"
        wrapper.write_text(text.replace('"../modules/A2FOAnimatedHardpoints/module.cpp"',f'"{module}"'))
        obj=WORK/f"{name}_{revision}.o"
        run(["i686-w64-mingw32-g++","-DWIN32_LEAN_AND_MEAN","-std=gnu++17","-O2","-Wall","-Wextra","-Wpedantic","-DNDEBUG","-Werror","-c",str(wrapper),"-o",str(obj)])
        source=(ROOT/"tests/builtin_visual_native_test.cpp").read_text()
        source=source.replace('"../core/builtin_modules.hpp"',f'"{ROOT/"core/builtin_modules.hpp"}"')
        source=source.replace('"../core/module_loader.hpp"',f'"{ROOT/"core/module_loader.hpp"}"')
        generated=WORK/f"{name}_{revision}.cpp";generated.write_text(source)
        exe=WORK/f"{name}_{revision}.exe"
        args=[str(generated) if a=="tests/builtin_visual_native_test.cpp" else str(exe) if a==target else
            str(obj) if a=="build/builtin-native/core/builtin_animated_hardpoints.cpp.o" else a for a in command]
        run(ensure_rust(args))
        modes=("success","legacy","rejected","hook-fail","partial","signature-fail","identity-fail")
        transcript="".join(f"mode:{mode}\n"+wine(exe,mode) for mode in modes)
        (WORK/f"{name}_{revision}.txt").write_text(transcript);outputs.append(transcript)
    assert outputs[0]==outputs[1],name
    print("hardpoints visual frozen/current native transcript: PASS")
def receipt():
    cases={}
    for name in ("animation_bridge","animations","hardpoints_visual"):
        before=WORK/f"{name}_before.txt";current=WORK/f"{name}_current.txt"
        if before.is_file() and current.is_file():
            a=hashlib.sha256(before.read_bytes()).hexdigest();b=hashlib.sha256(current.read_bytes()).hexdigest()
            assert a==b,name
            cases[name]={"before_sha256":a,"current_sha256":b}
    path=WORK/"runtime-b3-parity.json"
    path.write_text(json.dumps({"compiler":"i686-w64-mingw32-g++",
        "native_flags":["-std=gnu++17","-O2","-Werror"],"frozen_current_transcripts":cases},indent=2,sort_keys=True)+"\n")
    print(f"receipt: {path}")
if __name__=="__main__":
    WORK.mkdir(parents=True,exist_ok=True);hashes()
    if not os.environ.get("A2FO_RUST_WIN_LIB"):operation_order()
    else:print("canonical Windows archive selected; isolated host archive build skipped")
    assert RUST.is_file(),RUST
    choice=sys.argv[1] if len(sys.argv)>1 else "all"
    if choice in ("all","bridge"):
        direct("animation_bridge","build/builtin_animation_bridge_native_test.exe",
            "tests/builtin_animation_bridge_native_test.cpp","builtin_animated_hardpoints",
            "A2FOAnimatedHardpoints",("core","fallback","incomplete"))
    if choice in ("all","animations"):
        direct("animations","build/builtin_animations_native_test.exe",
            "tests/builtin_animations_native_test.cpp","builtin_animations","A2FOAnimations",
            ("success","signature","partial","dispatch","throw-hook",
             "throw-class-first","throw-class-later","false-class-first",
             "throw-producer"))
    if choice in ("all","visual"):visual()
    receipt()
