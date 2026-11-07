#!/usr/bin/env python3
"""B2 parity; A2FO_RUST_WIN_LIB selects archive, A2FO_RUST_WINEPREFIX selects Wine prefix."""
from pathlib import Path
import hashlib
import json
import os
import shlex
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/"build/rust-migration-stage7/runtime-b-work"
BEFORE=ROOT/"build/rust-migration-stage7/before"
RUST=Path(os.environ.get("A2FO_RUST_WIN_LIB", WORK/"target/i686-pc-windows-gnu/release/liba2fo_core.a"))
WINEPREFIX=Path(os.environ.get("A2FO_RUST_WINEPREFIX", WORK/"wine")).resolve()

def execute(args,**kwargs):
    return subprocess.run(args,cwd=ROOT,check=True,**kwargs)

def linker(target):
    output=execute(["make","-Bn",target,f"RUST_WIN_LIB={RUST}"],text=True,capture_output=True).stdout
    command=shlex.split(next(line for line in output.splitlines() if f"-o {target} " in line))
    assert "-std=gnu++17" in command and "-O2" in command and "-Werror" in command
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
def frozen_hashes():
    hashes=json.loads((ROOT/"build/rust-migration-stage7/before-hashes.json").read_text())
    for path in ("modules/A1Fallbacks/module.cpp","modules/A2FOStationRotation/module.cpp"):
        expected=hashes[path] if isinstance(hashes[path],str) else hashes[path]["sha256"]
        actual=hashlib.sha256((BEFORE/path).read_bytes()).hexdigest()
        assert actual==expected,(path,actual,expected)
        print(f"frozen {path}: {actual}")

def operation_order():
    archive=WORK/"libmodule_runtime_b2_host.a"
    exe=WORK/"runtime_b2_operation_order"
    execute(["rustc","--edition=2024","--crate-type=staticlib","-C","opt-level=2",
        "rust/a2fo_core/src/module_runtime_b2.rs","-o",str(archive)])
    execute(["g++","-std=gnu++17","-O2","-Wall","-Wextra","-Werror",
        "tests/rust_stage7_runtime_b2_parity.cpp",str(archive),"-ldl","-lpthread","-lm","-o",str(exe)])
    execute([str(exe)])

def station():
    target="build/builtin_station_rotation_native_test.exe"
    command=linker(target)
    outputs=[]
    for revision in ("before","current"):
        wrapper=WORK/f"station_{revision}_wrapper.cpp"
        code=(ROOT/"core/builtin_station_rotation.cpp").read_text()
        module=BEFORE/"modules/A2FOStationRotation/module.cpp" if revision=="before" else ROOT/"modules/A2FOStationRotation/module.cpp"
        code=code.replace('"../modules/A2FOStationRotation/module.cpp"',f'"{module}"')
        wrapper.write_text(code)
        source=(ROOT/"tests/builtin_station_rotation_native_test.cpp").read_text()
        source=source.replace('"../core/builtin_station_rotation.cpp"',f'"{wrapper}"')
        source=source.replace('"../core/module_loader.hpp"',f'"{ROOT/"core/module_loader.hpp"}"')
        source=source.replace('void A2FO_CALL logger(const char*, const char*) {}',
            'void A2FO_CALL logger(const char*,const char* s){std::printf("log:%c\\n",s&&*s?*s:\'?\');}')
        source=source.replace('++installs;', '++installs;std::printf("hook:%u\\n",installs);')
        source=source.replace('++patches;return true;', '++patches;std::printf("patch:%u\\n",patches);return true;')
        source=source.replace('    std::puts("builtin station rotation fixture: PASS");',
            '    std::printf("state:%u,%u,%u,%u\\n",installs,patches,native_queues,native_sets);\n'
            '    std::puts("builtin station rotation fixture: PASS");')
        generated=WORK/f"station_{revision}.cpp";generated.write_text(source)
        exe=WORK/f"station_{revision}.exe"
        args=[str(generated) if a=="tests/builtin_station_rotation_native_test.cpp" else str(exe) if a==target else a for a in command]
        execute(args)
        transcript="".join(f"mode:{m}\n"+wine(exe,m) for m in ("success","signature","partial"))
        (WORK/f"station_{revision}.txt").write_text(transcript)
        outputs.append(transcript)
    assert outputs[0]==outputs[1],"station transcript"
    print("station frozen/current native transcript: PASS")

def a1():
    target="build/builtin_visual_native_test.exe"
    command=linker(target)
    outputs=[]
    for revision in ("before","current"):
        wrapper=WORK/f"a1_{revision}_wrapper.cpp"
        code=(ROOT/"core/builtin_a1_fallbacks.cpp").read_text()
        module=BEFORE/"modules/A1Fallbacks/module.cpp" if revision=="before" else ROOT/"modules/A1Fallbacks/module.cpp"
        wrapper.write_text(code.replace('"../modules/A1Fallbacks/module.cpp"',f'"{module}"'))
        obj=WORK/f"a1_{revision}.o"
        execute(["i686-w64-mingw32-g++","-DWIN32_LEAN_AND_MEAN","-std=gnu++17","-O2","-Wall","-Wextra","-Wpedantic","-DNDEBUG","-Werror","-c",str(wrapper),"-o",str(obj)])
        source=(ROOT/"tests/builtin_visual_native_test.cpp").read_text()
        source=source.replace('"../core/builtin_modules.hpp"',f'"{ROOT/"core/builtin_modules.hpp"}"')
        source=source.replace('"../core/module_loader.hpp"',f'"{ROOT/"core/module_loader.hpp"}"')
        source=source.replace('void A2FO_CALL log_message(const char*, const char*) {}',
            'void A2FO_CALL log_message(const char*,const char* s){std::printf("log:%c\\n",s&&*s?*s:\'?\');}')
        source=source.replace('          std::puts("builtin visual native: PASS");',
            '          std::printf("state:%u,%u,%u,%u,%u\\n",wire_installs,world_installs,wire_native_calls,lookups,world_native_calls);\n'
            '          std::puts("builtin visual native: PASS");')
        generated=WORK/f"a1_{revision}.cpp";generated.write_text(source)
        exe=WORK/f"a1_{revision}.exe"
        args=[str(generated) if a=="tests/builtin_visual_native_test.cpp" else str(exe) if a==target else str(obj) if a=="build/builtin-native/core/builtin_a1_fallbacks.cpp.o" else a for a in command]
        execute(args)
        modes=("success","legacy","rejected","hook-fail","partial","signature-fail","identity-fail")
        transcript="".join(f"mode:{m}\n"+wine(exe,m) for m in modes)
        (WORK/f"a1_{revision}.txt").write_text(transcript)
        outputs.append(transcript)
    assert outputs[0]==outputs[1],"A1 transcript"
    print("A1 fallback frozen/current native transcript: PASS")

def receipt():
    cases={}
    for name in ("station","a1"):
        before=WORK/f"{name}_before.txt";current=WORK/f"{name}_current.txt"
        if before.is_file() and current.is_file():
            a=hashlib.sha256(before.read_bytes()).hexdigest()
            b=hashlib.sha256(current.read_bytes()).hexdigest()
            assert a==b,name
            cases[name]={"before_sha256":a,"current_sha256":b}
    value={"compiler":"i686-w64-mingw32-g++","native_flags":["-std=gnu++17","-O2","-Werror"],
           "frozen_current_transcripts":cases}
    path=WORK/"runtime-b2-parity.json"
    path.write_text(json.dumps(value,indent=2,sort_keys=True)+"\n")
    print(f"receipt: {path}")

if __name__=="__main__":
    WORK.mkdir(parents=True,exist_ok=True)
    frozen_hashes()
    if not os.environ.get("A2FO_RUST_WIN_LIB"):operation_order()
    else:print("canonical Windows archive selected; isolated host archive build skipped")
    assert RUST.is_file(),RUST
    if len(sys.argv)==1 or sys.argv[1]=="station":station()
    if len(sys.argv)==1 or sys.argv[1]=="a1":a1()
    receipt()
