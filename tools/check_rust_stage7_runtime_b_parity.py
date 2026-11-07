#!/usr/bin/env python3
"""Frozen/current runtime-b parity. A2FO_RUST_WIN_LIB selects the canonical archive;
A2FO_RUST_WINEPREFIX selects a dedicated headless Wine prefix."""
from pathlib import Path
import json
import hashlib
import subprocess
import os
import shlex
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / "build/rust-migration-stage7/runtime-b-work"
BEFORE = ROOT / "build/rust-migration-stage7/before"
RUST = Path(os.environ.get("A2FO_RUST_WIN_LIB", WORK / "target/i686-pc-windows-gnu/release/liba2fo_core.a"))
WINEPREFIX = Path(os.environ.get("A2FO_RUST_WINEPREFIX", WORK / "wine")).resolve()

def run(*args):
    subprocess.run(args, cwd=ROOT, check=True)

def wine(exe, *arguments):
    env = os.environ.copy()
    env.update({"WINEPREFIX": str(WINEPREFIX), "WINEDEBUG": "-all"})
    env.pop("DISPLAY", None); env.pop("WAYLAND_DISPLAY", None)
    # Regular files prevent wineserver-held pipes from delaying completion.
    with tempfile.TemporaryFile(mode="w+t") as output, tempfile.TemporaryFile(mode="w+t") as error:
        result = subprocess.run(["wine", str(exe), *arguments], cwd=ROOT, env=env,
                                text=True, stdout=output, stderr=error, timeout=90)
        output.seek(0); error.seek(0)
        stdout, stderr = output.read(), error.read()
    assert result.returncode == 0, (exe, arguments, stdout, stderr)
    return stdout

def main():
    WORK.mkdir(parents=True, exist_ok=True)
    hashes = json.loads((ROOT / "build/rust-migration-stage7/before-hashes.json").read_text())
    for path in ("modules/A2FORadarDiagnostics/module.cpp", "modules/A2FORecoveryTests/module.cpp"):
        frozen = BEFORE / path
        actual = hashlib.sha256(frozen.read_bytes()).hexdigest()
        expected = hashes[path] if isinstance(hashes[path], str) else hashes[path]["sha256"]
        assert actual == expected, (path, actual, expected)
        print(f"frozen {path}: {actual}")
    archive = WORK / "libmodule_runtime_b.a"
    if not os.environ.get("A2FO_RUST_WIN_LIB"):
        exe = WORK / "runtime_b_parity"
        run("rustc", "--edition=2024", "--crate-type=staticlib", "-C", "opt-level=2",
            "rust/a2fo_core/src/module_runtime_b.rs", "-o", str(archive))
        run("g++", "-std=gnu++17", "-O2", "-Wall", "-Wextra", "-Werror",
            "tests/rust_stage7_runtime_b_parity.cpp", str(archive), "-ldl", "-lpthread", "-lm", "-o", str(exe))
        run(str(exe))
    else:
        print("canonical Windows archive selected; isolated host archive build skipped")
    for source in ("core/builtin_radar_diagnostics.cpp", "core/builtin_recovery_tests.cpp",
                   "modules/A2FORadarDiagnostics/module.cpp", "modules/A2FORecoveryTests/module.cpp"):
        run("i686-w64-mingw32-g++", "-std=gnu++17", "-fsyntax-only", source)
    print("stage7 runtime-b C++ syntax: PASS")
    native_transcripts(archive)
    radar_hook_transcript()
    receipt()

def native_transcripts(archive):
    # Link both source versions to the same existing native mocks/objects.
    # The Makefile dry run supplies the established linker inputs without
    # rebuilding shared objects while other stage-seven work is in progress.
    for name, wrapper, module in (
        ("radar_diagnostics", "builtin_radar_diagnostics", "A2FORadarDiagnostics"),
        ("recovery_tests", "builtin_recovery_tests", "A2FORecoveryTests"),
    ):
        if len(sys.argv) > 1 and sys.argv[1] != name:
            continue
        target = f"build/builtin_{name}_native_test.exe"
        dry = subprocess.run(["make", "-Bn", target,
            f"RUST_WIN_LIB={RUST}"],
            cwd=ROOT, check=True, text=True, capture_output=True).stdout
        lines = [line for line in dry.splitlines() if f"-o {target} " in line]
        assert lines, target
        command = shlex.split(lines[-1])
        assert "-std=gnu++17" in command and "-O2" in command and "-Werror" in command
        transcripts = []
        for revision in ("before", "current"):
            original = (ROOT / f"tests/builtin_{name}_native_test.cpp").read_text()
            if revision == "before":
                included = WORK / f"{name}_frozen_wrapper.cpp"
                wrapper_text = (ROOT / f"core/{wrapper}.cpp").read_text()
                wrapper_text = wrapper_text.replace(f'"../modules/{module}/module.cpp"',
                    f'"{BEFORE / f"modules/{module}/module.cpp"}"')
                included.write_text(wrapper_text)
            else:
                included = ROOT / f"core/{wrapper}.cpp"
            original = original.replace(f'"../core/{wrapper}.cpp"', f'"{included}"')
            original = original.replace('"../core/module_loader.hpp"', f'"{ROOT / "core/module_loader.hpp"}"')
            original = original.replace('void A2FO_CALL logger(const char*,const char*){++logs;}',
                'void A2FO_CALL logger(const char*,const char* message){++logs; std::printf("log:%c\\n",message&&*message?*message:\'?\');}')
            original = original.replace('++patches;return', '++patches;std::printf("patch:%u\\n",patches);return')
            original = original.replace('++originals;', '++originals;std::printf("original:%u\\n",originals);')
            source = WORK / f"{name}_{revision}.cpp"
            source.write_text(original)
            exe = WORK / f"{name}_{revision}.exe"
            changed = [str(source) if arg == f"tests/builtin_{name}_native_test.cpp" else
                       str(exe) if arg == target else arg for arg in command]
            # The current Rust unit is available from the isolated full crate.
            run(*changed)
            modes = ("passive", "success", "signature", "partial") if name == "radar_diagnostics" else ("passive", "unsupported", "success")
            events = []
            for mode in modes:
                stdout = wine(exe, mode)
                events.append(f"mode:{mode}\n{stdout}")
            transcript = "".join(events)
            (WORK / f"{name}_{revision}.txt").write_text(transcript)
            transcripts.append(transcript)
        assert transcripts[0] == transcripts[1], name
        print(f"{name} frozen/current native transcript: PASS")

def radar_hook_transcript():
    if len(sys.argv) > 1 and sys.argv[1] == "recovery_tests":
        return
    target = "build/radar_diagnostics_test.exe"
    rustlib = RUST
    dry = subprocess.run(["make","-Bn",target,f"RUST_WIN_LIB={rustlib}"],
        cwd=ROOT,check=True,text=True,capture_output=True).stdout
    command = shlex.split(next(line for line in dry.splitlines() if f"-o {target} " in line))
    assert "-std=gnu++17" in command and "-O2" in command and "-Werror" in command
    transcripts = []
    for revision in ("before","current"):
        source = (ROOT / "tests/radar_diagnostics_test.cpp").read_text()
        module = BEFORE / "modules/A2FORadarDiagnostics/module.cpp" if revision == "before" else ROOT / "modules/A2FORadarDiagnostics/module.cpp"
        source = source.replace('"../modules/A2FORadarDiagnostics/module.cpp"',f'"{module}"')
        source = source.replace('"../rust/native_hooks_ffi.h"',f'"{ROOT / "rust/native_hooks_ffi.h"}"')
        if revision == "before":
            source = '#define A2FO_LEGACY_RADAR_PROBE 1\n' + source
        source = source.replace('    std::puts("PASS: radar diagnostics checked patches, rollback, forwarding and lifetime");',
            '    std::printf("state:%u,%u,%u,%u,%ld,%llu,%llu,%llu\\n",patch_count,clear_count,viewport_count,original_count,samples,objects[0].load(),objects[1].load(),dots.load());\n'
            '    std::puts("PASS: radar diagnostics checked patches, rollback, forwarding and lifetime");')
        generated = WORK / f"radar_hook_{revision}.cpp"
        generated.write_text(source)
        exe = WORK / f"radar_hook_{revision}.exe"
        changed = [str(generated) if arg == "tests/radar_diagnostics_test.cpp" else
                   str(exe) if arg == target else arg for arg in command]
        run(*changed)
        stdout = wine(exe)
        (WORK / f"radar_hook_{revision}.txt").write_text(stdout)
        transcripts.append(stdout)
    assert transcripts[0] == transcripts[1], "radar hook transcript"
    print("radar fake-D3D hook frozen/current transcript: PASS")

def receipt():
    cases={}
    for name in ("radar_diagnostics","recovery_tests","radar_hook"):
        before=WORK/f"{name}_before.txt";current=WORK/f"{name}_current.txt"
        if before.is_file() and current.is_file():
            a=hashlib.sha256(before.read_bytes()).hexdigest()
            b=hashlib.sha256(current.read_bytes()).hexdigest()
            assert a==b,name
            cases[name]={"before_sha256":a,"current_sha256":b}
    value={"compiler":"i686-w64-mingw32-g++","native_flags":["-std=gnu++17","-O2","-Werror"],
           "frozen_current_transcripts":cases}
    path=WORK/"runtime-b-parity.json"
    path.write_text(json.dumps(value,indent=2,sort_keys=True)+"\n")
    print(f"receipt: {path}")

if __name__ == "__main__":
    main()
