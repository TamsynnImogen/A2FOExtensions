#!/usr/bin/env python3
"""Compare frozen/current SOL runtime behavior against the canonical Rust archive."""

import argparse
from datetime import datetime, timezone
import difflib
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "build/rust-migration-stage9"
WORK = STAGE / "sol-runtime-work"
UNIT = Path("modules/A2FOSOLMaps/module.cpp")
FROZEN = ROOT / "build/rust-migration-stage8/before" / UNIT
BASELINE_MANIFEST = ROOT / "build/rust-migration-stage8/before-hashes.json"
TEST = ROOT / "tests/rust_stage9_sol_runtime_native.cpp"
DISCOVERY_CASES = ROOT / "tests/rust_stage9_sol_discovery_cases.inl"
COMPLETION_CASES = ROOT / "tests/rust_stage9_sol_completion_cases.inl"
STAGED_FIXTURE = STAGE / "sol-runtime-staged/sol_runtime_supplement.cpp"
MOCK = ROOT / "tests/rust_stage8_c_solmaps_mock.cpp"
A1_MOCK = ROOT / "tests/rust_stage8_c_solmaps_a1_mock.cpp"
STAGED_RUNNER = STAGE / "run-sol-staged-native.py"
RUST_MANIFEST = ROOT / "rust/a2fo_core/Cargo.toml"
RUST_LOCK = ROOT / "rust/a2fo_core/Cargo.lock"
RUST_LIB = ROOT / "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a"
REQUIRED_EXPORTS = (
    "a2fo_rs_stage9_sol_root_directories",
    "a2fo_rs_stage9_sol_start_count",
    "a2fo_rs_stage9_sol_record_mission_add",
    "a2fo_rs_stage9_sol_publish_bounds",
    "a2fo_rs_stage9_sol_discovery_log_pass",
    "a2fo_rs_stage9_sol_parent_end",
    "a2fo_rs_stage9_sol_is_bzn_leaf",
    "a2fo_rs_stage9_sol_log_pass16",
    "a2fo_rs_stage9_sol_directory",
    "a2fo_rs_stage9_sol_search_mask",
    "a2fo_rs_stage9_sol_virtual_prefix",
    "a2fo_rs_stage9_sol_virtual_mask",
    "a2fo_rs_stage9_sol_selected_suffix",
    "a2fo_rs_stage9_sol_post_mission_message",
    "a2fo_rs_stage9_sol_mission_present",
    "a2fo_rs_stage9_sol_probe_candidate",
    "a2fo_rs_stage9_sol_probe_gate",
    "a2fo_rs_stage9_sol_caller_version",
    "a2fo_rs_stage9_sol_compatible_version",
    "a2fo_rs_stage9_sol_counter_changed",
    "a2fo_rs_stage9_sol_spawn_complete",
    "a2fo_rs_stage9_sol_file_size",
    "a2fo_rs_stage9_sol_version_fits",
    "a2fo_rs_stage9_sol_input_present",
    "a2fo_rs_stage9_sol_empty_metadata",
    "a2fo_rs_stage9_sol_direct_source_root",
)
OBJECT_POINTER = re.compile(
    rb"(log:SOL phase2j (?:constructed|spawned) \d+/\d+: .* object=)"
    rb"([0-9a-fA-F]{8})(; publishing to mission)?\n"
)
DETAIL_POINTER = re.compile(
    rb"(log:SOL phase2j probe synthesis #1: .* mapDetails=)"
    rb"([0-9a-fA-F]{8})( \(native\), earlyMission=.*\n)"
)
BOUNDS_ROW = re.compile(
    rb"^bounds:(pc24|pc53|pc64)/(near|down|up|chop):(\d+) "
    rb"in=.* out=.* ok=\d+ cw=[0-9a-f]{4} top=\d+ flags=[0-9a-f]{2}$"
)
DETAIL_ROWS = (
    b"supp-details:-1:0:0", b"supp-details:-1:3:3", b"supp-details:-1:70:8",
    b"supp-details:0:0:0", b"supp-details:0:3:3", b"supp-details:0:70:8",
    b"supp-details:1:0:0", b"supp-details:1:3:1", b"supp-details:1:70:1",
    b"supp-details:8:0:0", b"supp-details:8:3:3", b"supp-details:8:70:8",
    b"supp-details:64:0:0", b"supp-details:64:3:3", b"supp-details:64:70:64",
    b"supp-details:65:0:0", b"supp-details:65:3:3", b"supp-details:65:70:8",
)
DISCOVERY_SCENARIOS = (
    ("gate-no-armada", 0, 0, None, "native.bzn", False, 1),
    ("gate-null-knownmaps", 0, 0, None, "native.bzn", False, 1),
    ("seed-six", 6, 7, 7, "native.bzn", False, 0),
    ("seed-seven", 7, 8, 8, "native.bzn", False, 0),
    ("seed-eight", 8, 9, None, "native.bzn", False, 0),
    ("seed-max", 2147483647, -2147483648, -2147483648, "native.bzn", False, 0),
    ("null-mask", 0, 1, 1, None, True, 1),
    ("empty-mask", 0, 1, 1, "", True, 1),
    ("nonempty-mask", 0, 1, 1, "native.bzn", True, 1),
    ("lower-backslash", 0, 1, 1, "native.bzn", True, 1),
    ("upper-backslash", 0, 1, 1, "native.bzn", True, 1),
    ("forward-slash", 0, 1, 1, "native.bzn", True, 1),
    ("mixed-slashes", 0, 1, 1, "native.bzn", True, 1),
    ("sol-parent", 0, 1, 1, "native.bzn", False, 1),
    ("other-parent", 0, 1, 1, "native.bzn", False, 1),
    ("no-separator-file", 0, 1, 1, "native.bzn", False, 1),
    ("no-separator-bzn", 0, 1, 1, "native.bzn", True, 1),
    ("root-slash", 0, 1, 1, "native.bzn", False, 1),
    ("trailing-slash", 0, 1, 1, "native.bzn", True, 1),
    ("embedded-nul-before-sol", 0, 1, 1, "native.bzn", False, 1),
    ("embedded-nul-before-bzn", 0, 1, 1, "native.bzn", True, 1),
    ("ordered-first-bzn", 0, 1, 1, "native.bzn", True, 3),
    ("ordered-no-bzn", 0, 1, 1, "native.bzn", False, 3),
)
DISCOVERY_BEGIN = re.compile(rb"^discovery-begin:([^:]+):seed=(-?\d+):virtual=([01])$")
DISCOVERY_END = re.compile(rb"^discovery-end:([^:]+):count=(-?\d+)$")


def digest(path):
    hasher = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            hasher.update(chunk)
    return hasher.hexdigest()


def rust_inputs():
    paths = {RUST_MANIFEST, RUST_LOCK}
    paths.update((ROOT / "rust/a2fo_core/src").rglob("*.rs"))
    paths.update((ROOT / "rust").glob("*.h"))
    return paths


def pinned_paths(archive):
    paths = {
        ROOT / "Makefile", archive, FROZEN, BASELINE_MANIFEST,
        ROOT / UNIT, TEST, DISCOVERY_CASES, COMPLETION_CASES, STAGED_FIXTURE,
        MOCK, A1_MOCK, STAGED_RUNNER,
        Path(__file__).resolve(),
        ROOT / "modules/A2FOSOLMaps/sol_format.cpp",
        ROOT / "modules/A2FOSOLMaps/sol_format.hpp",
        ROOT / "modules/A2FOSOLMaps/thiscall_bridge.S",
    }
    paths.update((ROOT / "sdk").rglob("*.h"))
    paths.update((ROOT / "sdk").rglob("*.hpp"))
    paths.update(rust_inputs())
    return paths


def snapshot(paths):
    missing = sorted(str(path) for path in paths if not path.is_file())
    if missing:
        raise FileNotFoundError("missing pinned inputs: " + ", ".join(missing))
    return {str(path): digest(path) for path in sorted(paths)}


def verify_inputs(archive):
    manifest = json.loads(BASELINE_MANIFEST.read_text(encoding="utf-8"))
    if manifest.get(str(UNIT)) != digest(FROZEN):
        raise RuntimeError("frozen A2FOSOLMaps source does not match before-hashes.json")
    newest = max(rust_inputs(), key=lambda path: path.stat().st_mtime_ns)
    if archive.stat().st_mtime_ns < newest.stat().st_mtime_ns:
        raise RuntimeError(f"Rust archive predates registered input {newest}")
    result = subprocess.run(["i686-w64-mingw32-nm", "-g", "--defined-only",
                             str(archive)], check=True, capture_output=True,
                            text=True, timeout=60)
    symbols = {line.split()[-1].lstrip("_") for line in result.stdout.splitlines()
               if line.split()}
    missing = sorted(set(REQUIRED_EXPORTS) - symbols)
    if missing:
        raise RuntimeError("Rust archive lacks required exports: " + ", ".join(missing))


def run_logged(command, label, work, cwd=ROOT, timeout=300):
    stdout_path = work / f"{label}.stdout"
    stderr_path = work / f"{label}.stderr"
    with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
        result = subprocess.run(command, cwd=cwd, stdout=stdout, stderr=stderr,
                                timeout=timeout, check=False)
    if result.returncode:
        raise RuntimeError(f"{label} exited {result.returncode}; logs retained")
    return stdout_path, stderr_path


def compile_fixtures(archive, work, receipt):
    run_logged(["i686-w64-mingw32-g++", "-std=gnu++17", "-O2", "-Wall",
                "-Wextra", "-Werror", "-shared", "-static-libgcc",
                str(A1_MOCK), "-o", str(work / "A1Compat.dll")],
               "a1-mock-build", work)
    binaries = {}
    compile_logs = {}
    dlls = {}
    for label, source in (("frozen", FROZEN), ("current", ROOT / UNIT)):
        run_dir = work / label
        run_dir.mkdir()
        dll = run_dir / "A1Compat.dll"
        dll.write_bytes((work / "A1Compat.dll").read_bytes())
        dlls[label] = dll
        exe = run_dir / "sol-runtime.exe"
        command = [
            "i686-w64-mingw32-g++", "-std=gnu++17", "-O2", "-Wall", "-Wextra",
            "-Werror", "-ffunction-sections", "-fdata-sections", "-Wl,--gc-sections",
            "-static", f'-DA2FO_STAGE8_SOL_SOURCE="{source}"', str(TEST),
        ]
        if label == "current":
            command.append(str(archive))
        command += ["-o", str(exe)]
        out, err = run_logged(command, label + "-build", work)
        binaries[label] = exe
        compile_logs[label] = {"stdout": str(out), "stderr": str(err),
                               "a1_mock_sha256": digest(dll)}
    receipt["compile_logs"] = compile_logs
    receipt["binaries"] = {key: str(value) for key, value in binaries.items()}
    receipt["a1_mock_dlls"] = {key: {"path": str(value), "sha256": digest(value)}
                               for key, value in dlls.items()}
    return binaries


def wine_environment(prefix):
    env = os.environ.copy()
    env.pop("DISPLAY", None)
    env.pop("WAYLAND_DISPLAY", None)
    env.update(WINEPREFIX=str(prefix), WINEDEBUG="-all", LIBGL_ALWAYS_SOFTWARE="1",
               WINEDLLOVERRIDES="winemenubuilder.exe=d;winedbg.exe=d;"
               "winewayland.drv=d;winex11.drv=d")
    return env


def run_native(exe, prefix, work, label):
    stdout_path = work / f"{label}.stdout"
    stderr_path = work / f"{label}.stderr"
    with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
        try:
            result = subprocess.run(["wine", str(exe)], cwd=exe.parent,
                                    env=wine_environment(prefix), stdout=stdout,
                                    stderr=stderr, timeout=300, check=False)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError(f"{label} timed out; stdout/stderr retained") from error
    if result.returncode:
        raise RuntimeError(f"{label} exited {result.returncode}; traces retained")
    return stdout_path.read_bytes(), stderr_path.read_bytes()


def normalize_trace(data):
    objects = []
    details = []
    output = []
    for line in data.replace(b"\r\n", b"\n").splitlines(keepends=True):
        match = OBJECT_POINTER.fullmatch(line)
        if match:
            objects.append(match.group(2))
            line = match.group(1) + b"<fixture-object>" + (match.group(3) or b"") + b"\n"
        else:
            match = DETAIL_POINTER.fullmatch(line)
            if match:
                details.append(match.group(2))
                line = match.group(1) + b"<fixture-details>" + match.group(3)
        output.append(line)
    if len(objects) != 130 or len(set(objects)) != 1 or objects[0] == b"00000000":
        raise RuntimeError("expected 130 SOL object-pointer rows sharing one nonzero address")
    if len(details) != 1 or details[0] == b"00000000":
        raise RuntimeError("expected one nonzero native MapDetails pointer row")
    normalized = b"".join(output)
    role_declaration = re.compile(
        rb"^fixture-role:(game-object|nebula|mission|list)=([0-9a-f]{8})$", re.M)
    declarations = list(role_declaration.finditer(normalized))
    role_names = (b"game-object", b"nebula", b"mission", b"list")
    if len(declarations) != len(role_names):
        raise RuntimeError("expected exactly four fixture-role declarations")
    roles = {}
    for match in declarations:
        role, address = match.group(1), int(match.group(2), 16)
        if role in roles or address == 0:
            raise RuntimeError("fixture-role declarations must be unique and nonzero")
        roles[role] = address
    if set(roles) != set(role_names) or len(set(roles.values())) != 4:
        raise RuntimeError("fixture-role declarations must name four distinct arrays")
    normalized = role_declaration.sub(
        lambda match: b"fixture-role:" + match.group(1) + b"=<fixture-" +
        match.group(1) + b">", normalized)
    role_usages = {
        b"game-object": (re.compile(rb"(GameObject=)([0-9a-f]{8})( team=3;)"), 1),
        b"nebula": (re.compile(rb"(nebula=)([0-9a-f]{8})( serialized companion)"), 1),
        b"mission": (re.compile(
            rb"((?:mission|earlyMission)=)([0-9a-f]{8})([;,\n])"), 4),
        b"list": (re.compile(rb"(gameObjectList=)([0-9a-f]{8})(, callerVersion=)"), 1),
    }
    for role, (pattern, expected_count) in role_usages.items():
        matches = list(pattern.finditer(normalized))
        if len(matches) != expected_count:
            raise RuntimeError(
                f"expected {expected_count} {role.decode()} pointer usages, found {len(matches)}")
        if any(int(match.group(2), 16) != roles[role] for match in matches):
            raise RuntimeError(f"{role.decode()} pointer usage did not match its declaration")
        normalized = pattern.sub(
            lambda match: match.group(1) + b"<fixture-" + role + b">" + match.group(3),
            normalized)
    return normalized


def validate_trace(data):
    lines = data.splitlines()
    bounds = [line for line in lines if line.startswith(b"bounds:")]
    if len(bounds) != 6228:
        raise RuntimeError(f"expected 6,228 SOL bounds rows, found {len(bounds)}")
    counts = {(precision, rounding): 0 for precision in (b"pc24", b"pc53", b"pc64")
              for rounding in (b"near", b"down", b"up", b"chop")}
    next_index = dict.fromkeys(counts, 0)
    for row in bounds:
        match = BOUNDS_ROW.fullmatch(row)
        if not match:
            raise RuntimeError(f"invalid SOL bounds row: {row!r}")
        key = match.group(1), match.group(2)
        if int(match.group(3)) != next_index[key]:
            raise RuntimeError(f"SOL bounds row order changed for {key!r}")
        next_index[key] += 1
        counts[key] += 1
    if any(value != 519 for value in counts.values()):
        raise RuntimeError("each SOL precision/rounding mode must have 519 rows")
    for row in DETAIL_ROWS:
        if lines.count(row) != 1:
            raise RuntimeError(f"missing or duplicated SOL slot/fallback row: {row!r}")
    required = (
        b"supp-alloc-fail:1", b"supp-winner:foo.sol:1:1",
        b"supp-winner:bar.sol:0:1", b"supp-spawn:1",
        b"supp-summary:67:33:17:17:0", b"supp-repeat:1",
        b"supp-clamped:0:0", b"supp-no-mission:1:67:33:17:17:33",
        b"bounds-guard:disabled:0:deadbeef:cafef00d",
        b"bounds-guard:null-armada:0:deadbeef:cafef00d",
        b"bounds-guard:readonly:0:deadbeef:cafef00d:flags=00",
    )
    for row in required:
        if lines.count(row) != 1:
            raise RuntimeError(f"missing or duplicated SOL summary/guard row: {row!r}")
    if sum(line.startswith(b"supp-details:") for line in lines) != 18:
        raise RuntimeError("expected exactly 18 SOL MapDetails slot rows")
    expected_probes = []
    for bits in range(32):
        reader = int(bool(bits & 1))
        context = 2 if bits & 2 else 0
        details = 3 if bits & 4 else 0
        mission = int(bool(bits & 8))
        object_list = int(bool(bits & 16))
        complete = bits == 31
        expected_probes.append((
            f"completion-probe:{bits:02d}:flags={bits:02x}:header=3021,{reader},"
            f"{context},{details},mission={mission},list={object_list}:"
            f"out={2177 if complete else 3021}:logs={0 if complete else 1}/0"
        ).encode())
    actual_probes = [line for line in lines if line.startswith(b"completion-probe:")]
    if actual_probes != expected_probes:
        raise RuntimeError("SOL completion probe matrix rows differ from the 32 expected cases")
    cap_tags = (b"ai-guard", b"nebula-guard", b"a1-craft-table",
                b"post-mission-first", b"post-mission-second",
                b"map-details-discovery")
    cap_cases = ((15, 16, 1), (16, 17, 0), (2147483647, -2147483648, 1))
    expected_caps = [
        (f"completion-cap:{tag.decode()}:seed={seed}:after={after}:logs={emit}/"
         f"{int(tag == b'map-details-discovery')}/0").encode()
        for tag in cap_tags for seed, after, emit in cap_cases
    ]
    actual_caps = [line for line in lines if line.startswith(b"completion-cap:")]
    if actual_caps != expected_caps:
        raise RuntimeError("SOL completion counter boundary rows differ from expected cases")
    validate_discovery_trace(lines)
    return {"bounds_rows": len(bounds), "precision_rounding_modes": len(counts),
            "mapdetails_rows": 18, "discovery_cases": 23,
            "completion_probe_cases": 32, "completion_cap_cases": 18,
            "output_lines": len(lines)}


def validate_discovery_trace(lines):
    starts = [index for index, line in enumerate(lines)
              if line.startswith(b"discovery-begin:")]
    ends = [line for line in lines if line.startswith(b"discovery-end:")]
    if len(starts) != len(DISCOVERY_SCENARIOS) or len(ends) != len(DISCOVERY_SCENARIOS):
        raise RuntimeError("expected exactly 23 SOL discovery begin/end cases")
    for index, (name, seed, count, log_pass, mask, virtual, indexed) in enumerate(
            DISCOVERY_SCENARIOS):
        begin = DISCOVERY_BEGIN.fullmatch(lines[starts[index]])
        if (not begin or begin.group(1) != name.encode() or int(begin.group(2)) != seed or
                int(begin.group(3)) != int(virtual)):
            raise RuntimeError(f"invalid SOL discovery begin row for {name}")
        end_index = next((cursor for cursor in range(starts[index] + 1, len(lines))
                          if lines[cursor].startswith(b"discovery-end:")), None)
        if end_index is None:
            raise RuntimeError(f"missing SOL discovery end row for {name}")
        block = lines[starts[index] + 1:end_index]
        if any(line.startswith(b"discovery-begin:") for line in block):
            raise RuntimeError(f"nested SOL discovery block before end for {name}")
        end = DISCOVERY_END.fullmatch(lines[end_index])
        if (not end or end.group(1) != name.encode() or int(end.group(2)) != count):
            raise RuntimeError(f"invalid SOL discovery end row for {name}")
        expected_logs = []
        if log_pass is not None:
            mask_text = mask if mask is not None else "<null>"
            expected_logs.append(
                f"log:SOL KnownMaps discovery pass #{log_pass}: nativeMask='"
                f"{mask_text}', indexedSOL={indexed}".encode("ascii"))
        found_logs = [line for line in block
                      if line.startswith(b"log:SOL KnownMaps discovery pass #")]
        if found_logs != expected_logs:
            raise RuntimeError(f"SOL discovery pass log mismatch for {name}")
        expected_virtual = ([b"log:SOL KnownMaps virtual discovery mask: 'bzn\\*.sol'"]
                            if virtual and log_pass is not None else [])
        found_virtual = [line for line in block
                         if line.startswith(b"log:SOL KnownMaps virtual discovery mask:")]
        if found_virtual != expected_virtual:
            raise RuntimeError(f"SOL discovery virtual-mask log mismatch for {name}")
        expected_masks = []
        if name not in {"gate-no-armada", "gate-null-knownmaps"} and mask:
            expected_masks.append(mask.encode("ascii"))
        if name not in {"gate-no-armada", "gate-null-knownmaps"} and virtual:
            expected_masks.append(b"bzn\\*.sol")
        native_masks = [line[len(b"native-mask:"):] for line in block
                        if line.startswith(b"native-mask:")]
        if native_masks != expected_masks:
            raise RuntimeError(f"SOL discovery native-mask order mismatch for {name}")
    final_rows = [line for line in lines if line.startswith(b"discovery-final:")]
    if len(final_rows) != 1 or final_rows[0] != b"discovery-final:count=1":
        raise RuntimeError("missing or invalid SOL discovery final count")


def write_first_diff(path, before, after):
    sections = []
    for name, left, right in (("stdout", before[0], after[0]),
                              ("stderr", before[1], after[1])):
        left_lines = left.decode("utf-8", errors="replace").splitlines(keepends=True)
        right_lines = right.decode("utf-8", errors="replace").splitlines(keepends=True)
        sections.append(f"=== {name} ===\n")
        sections.extend(difflib.unified_diff(left_lines, right_lines,
                       fromfile=f"frozen/{name}", tofile=f"current/{name}"))
    path.write_text("".join(sections), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rust-lib", type=Path,
                        default=os.environ.get("A2FO_RUST_WIN_LIB", RUST_LIB))
    parser.add_argument("--wine-prefix", type=Path,
                        default=os.environ.get("A2FO_RUST_WINEPREFIX",
                            os.environ.get("WINEPREFIX", "/tmp/a2fo-shields-wine-prefix")))
    parser.add_argument("--work-dir", type=Path, default=WORK)
    parser.add_argument("--build-only", action="store_true")
    args = parser.parse_args()
    archive = args.rust_lib.resolve()
    work = args.work_dir.resolve()
    if work.exists() and any(work.iterdir()):
        print(f"refusing nonempty work directory: {work}", file=sys.stderr)
        return 1
    work.mkdir(parents=True, exist_ok=True)
    receipt = {"unit": str(UNIT), "status": "running", "rust_archive": str(archive),
               "work_dir": str(work), "build_only": args.build_only,
               "started_at": datetime.now(timezone.utc).isoformat()}
    before = {}
    return_code = 1
    try:
        paths = pinned_paths(archive)
        before = snapshot(paths)
        verify_inputs(archive)
        binaries = compile_fixtures(archive, work, receipt)
        if args.build_only:
            receipt["status"] = "built"
        else:
            traces = {}
            for label, exe in binaries.items():
                traces[label] = run_native(exe, args.wine_prefix.resolve(), work, label)
                (work / f"{label}.normalized.stdout").write_bytes(
                    normalize_trace(traces[label][0]))
            normalized = {label: (work / f"{label}.normalized.stdout").read_bytes()
                          for label in traces}
            if normalized["frozen"] != normalized["current"]:
                write_first_diff(work / "firstdiff.txt",
                                 (normalized["frozen"], traces["frozen"][1]),
                                 (normalized["current"], traces["current"][1]))
                raise RuntimeError("SOL runtime frozen/current trace mismatch")
            for label, trace in traces.items():
                receipt[f"{label}_results"] = validate_trace(trace[0])
            for label, artifact in receipt["a1_mock_dlls"].items():
                if digest(Path(artifact["path"])) != artifact["sha256"]:
                    raise RuntimeError(f"generated {label} A1Compat.dll changed during run")
            receipt.update(status="passed", bounds_rows=6228, mapdetails_rows=18,
                           frozen_sha256=digest(FROZEN), current_sha256=digest(ROOT / UNIT))
        after = snapshot(paths)
        changed = sorted(path for path in before.keys() | after.keys()
                         if before.get(path) != after.get(path))
        receipt["changed_pins"] = changed
        if changed:
            raise RuntimeError("pinned inputs changed during run: " + ", ".join(changed))
        return_code = 0
    except Exception as error:
        receipt["status"] = "failed"
        receipt["error"] = str(error)
        print(error, file=sys.stderr)
    finally:
        receipt["pins_before"] = before
        try:
            receipt["pins_after"] = snapshot(pinned_paths(archive))
        except Exception as error:
            receipt["snapshot_error"] = str(error)
            receipt["status"] = "failed"
            return_code = 1
        if receipt["status"] == "failed":
            return_code = 1
        receipt["finished_at"] = datetime.now(timezone.utc).isoformat()
        (work / "receipt.json").write_text(
            json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"SOL runtime parity: {receipt['status']}")
    return return_code


if __name__ == "__main__":
    sys.exit(main())
