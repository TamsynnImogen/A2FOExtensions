#!/usr/bin/env python3
"""Rebuild frozen/current stage-5 worker fixtures against the integrated Rust archive.

All fixtures are headless. The Windows cases here use numeric/state transcripts or
mock D3D8 callbacks; no application window, input, or real graphics device is used.
Wine execution requires the explicit --run-wine switch and lead coordination.
"""
from __future__ import annotations

import argparse
import dataclasses
import difflib
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / "build/rust-migration-stage5"
BEFORE = STAGE / "before"
WORK = STAGE / "parity-runner"
HOST_ARCHIVE = ROOT / "build/rust/release/liba2fo_core.a"
WIN_ARCHIVE = ROOT / "build/rust/i686-pc-windows-gnu/release/liba2fo_core.a"


@dataclasses.dataclass(frozen=True)
class Case:
    name: str
    test: str
    sources: tuple[str, ...] = ()
    before_sources: tuple[str, ...] = ()
    current_sources: tuple[str, ...] = ()
    includes: tuple[str, ...] = ()
    defines: tuple[str, ...] = ()
    libs: tuple[str, ...] = ()
    win: bool = False
    win_sources: tuple[str, ...] = ()
    win_libs: tuple[str, ...] = ()
    baseline_archive: bool = False
    production_source: str = ""
    # Work shims are immutable fixture inputs, never production replacements.


def path(value: str) -> Path:
    return ROOT / value


def refit_win_mock_header() -> Path:
    original = STAGE / "refit-native-work/mock/d3d8.h"
    generated = WORK / "refit-native-win-shim/d3d8.h"
    lines = original.read_text().splitlines()
    removed = ("using BYTE=", "using HRESULT=", "#define TRUE", "#define FALSE",
               "#define SUCCEEDED", "#define FAILED")
    generated.parent.mkdir(parents=True, exist_ok=True)
    generated.write_text("\n".join(line for line in lines
                                   if not line.startswith(removed)) + "\n")
    return generated


CASES = (
    Case("m3d", "tests/rust_stage5_m3d_parity.cpp",
         ("modules/A2FOM3D/m3d_conversion.cpp",),
         includes=("modules/A2FOM3D",), win=True),
    Case("motion", "tests/rust_stage5_motion_parity.cpp",
         ("modules/A2FOFireArcs/fire_arc.cpp", "modules/A2FOSwarmSystem/swarm_motion.cpp",
          "modules/A2FOTurrets/turret_math.cpp"), win=True),
    Case("domain_state", "tests/rust_stage5_domain_state_parity.cpp",
         ("modules/A2FOCollectiveXP/collective_xp.cpp",
          "modules/A2FOSquadrons/squadron_state.cpp",
          "modules/A2FOSquadrons/squadron_config.cpp"),
         win=True, baseline_archive=True),
    Case("emissive", "tests/rust_stage5_emissive_parity.cpp",
         ("core/nebula_emissive.cpp",), win=True),
    Case("refit_present", "tests/rust_stage5_refit_present_parity.cpp",
         ("modules/A2FORefitMenus/loading_content.cpp",
          "modules/A2FORefitMenus/presentation.cpp",
          "modules/A2FORefitMenus/setup_presentation.cpp",
          "modules/A2FORefitMenus/loading_presentation.cpp"),
         includes=(".",), win=True,
         win_sources=("modules/A2FORefitMenus/setup_skin.cpp",),
         win_libs=("-lgdi32", "-lmsimg32", "-lgdiplus", "-lole32")),
    Case("refit_native", "build/rust-migration-stage5/refit-native-work/gpu_differential.cpp",
         ("modules/A2FORefitMenus/loading_gpu.cpp",),
         includes=("build/rust-migration-stage5/refit-native-work/mock",), win=True),
    # Legacy fake COM objects cover probe behavior only. Native SDK fixtures
    # platform_d3d11_resources_native and platform_d3d11_allocations_native
    # cover upload and handle behavior with the actual Win32 ABI.
    Case("d3d_resources", "tests/rust_stage5_d3d_resources_parity.cpp",
         ("renderer/d3d11/probe_bridge.cpp", "renderer/d3d11/probe_device.cpp"),
         includes=("build/rust-migration-stage5/d3d-resource-work/shim", "renderer/d3d11"),
         defines=("A2FO_PROBE_ONLY",),
         win=True),
    Case("render_cache_state", "tests/rust_stage5_render_cache_state_parity.cpp",
         before_sources=("build/rust-migration-stage5/render-cache-work/state_before.cpp",),
         current_sources=("build/rust-migration-stage5/render-cache-work/state_current.cpp",),
         includes=("build/rust-migration-stage5/render-cache-work",),
         defines=("A2FO_STATE_ONLY",), win=True,
         production_source="renderer/d3d8_compat/game/state_blocks.cpp"),
    Case("render_cache_shader", "tests/rust_stage5_render_cache_state_parity.cpp",
         ("renderer/shaders/bytecode_cache.cpp", "renderer/d3d8_compat/game/bc_catalog.cpp"),
         includes=("build/rust-migration-stage5/render-cache-work/shim", "renderer/shaders",
                   "renderer/d3d8_compat/game", "renderer/d3d11"), win=True),
    Case("render_draw", "tests/rust_stage5_render_draw_parity.cpp",
         before_sources=("build/rust-migration-stage5/render-draw-work/before_draw.cpp",),
         current_sources=("build/rust-migration-stage5/render-draw-work/current_draw.cpp",),
         includes=("build/rust-migration-stage5/render-draw-work",),
         defines=("A2FO_DRAW_FIXTURE",), win=True,
         production_source="renderer/d3d8_compat/game/draw.cpp"),
    Case("render_compressed", "tests/rust_stage5_render_draw_parity.cpp",
         before_sources=("build/rust-migration-stage5/render-draw-work/before_compressed.cpp",),
         current_sources=("build/rust-migration-stage5/render-draw-work/current_compressed.cpp",),
         includes=("build/rust-migration-stage5/render-draw-work",), win=True,
         production_source="renderer/d3d8_compat/resources/compressed_texture8.cpp"),
    Case("refraction", "tests/rust_stage5_refraction_parity.cpp",
         before_sources=("build/rust-migration-stage5/refraction-work/refraction_before.cpp",),
         current_sources=("build/rust-migration-stage5/refraction-work/refraction_current.cpp",),
         includes=("build/rust-migration-stage5/refraction-work/shim", "renderer/cloak"),
         win=True, production_source="renderer/cloak/refraction.cpp"),
)


def sources(case: Case, side: str, platform: str) -> list[Path]:
    special = case.before_sources if side == "before" else case.current_sources
    if special:
        result = [path(x) for x in special]
    else:
        base = BEFORE if side == "before" else ROOT
        result = [base / x for x in case.sources]
    if platform == "win":
        base = BEFORE if side == "before" else ROOT
        result.extend(base / x for x in case.win_sources)
    return result


def build_command(case: Case, side: str, platform: str, executable: Path) -> list[str]:
    compiler = "g++" if platform == "host" else "i686-w64-mingw32-g++"
    command = [compiler, "-std=gnu++17", "-O2", "-Wall", "-Wextra", "-Wpedantic"]
    if platform == "win":
        command += ["-DNOMINMAX", "-static", "-static-libgcc", "-static-libstdc++"]
        if case.name == "refit_native":
            command += ["-I", str(refit_win_mock_header().parent)]
    for define in case.defines:
        command.append("-D" + define)
    for directory in case.includes:
        command += ["-I", str(path(directory))]
    command += [str(path(case.test)), *map(str, sources(case, side, platform))]
    if side == "current" or case.baseline_archive:
        command.append(str(HOST_ARCHIVE if platform == "host" else WIN_ARCHIVE))
    command += list(case.libs)
    if platform == "win":
        command += list(case.win_libs)
    command += ["-o", str(executable)]
    return command


def call(command: list[str], cwd: Path, env: dict[str, str] | None = None,
         timeout: int = 180) -> subprocess.CompletedProcess[str]:
    return subprocess.run(command, cwd=cwd, env=env, text=True,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          timeout=timeout, check=False)


def file_hash(filename: Path) -> str:
    digest = hashlib.sha256()
    with filename.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def fixture_body(filename: Path) -> str:
    # Worker mock copies replace only native include lines. The two token
    # parser assertions in production draw.cpp describe a native enum absent
    # from its test shim; neither changes executable behavior.
    return "\n".join(
        line for line in filename.read_text().splitlines()
        if not line.lstrip().startswith("#include ")
        and not line.lstrip().startswith(
            ("static_assert(static_cast<UINT>(shaders::TokenParseStatus::complete)",
             "static_assert(static_cast<UINT>(shaders::TokenParseStatus::truncated)"))
    )


def source_receipt(case: Case, side: str, platform: str) -> list[dict[str, str]]:
    result = []
    for filename in [path(case.test), *sources(case, side, platform)]:
        if not filename.is_file():
            raise FileNotFoundError(f"fixture source missing: {filename}")
        result.append({"path": str(filename.relative_to(ROOT)),
                       "sha256": file_hash(filename)})
    if case.production_source:
        actual = (BEFORE if side == "before" else ROOT) / case.production_source
        copied = sources(case, side, platform)[0]
        if fixture_body(actual) != fixture_body(copied):
            raise RuntimeError(f"stale mock copy: {copied} differs from {actual} beyond includes")
        result.append({"path": str(actual.relative_to(ROOT)),
                       "sha256": file_hash(actual),
                       "role": "production body asserted equal to mock copy"})
    return result


def run_case(case: Case, platform: str, args: argparse.Namespace) -> dict[str, object] | None:
    if platform == "win" and not case.win:
        raise ValueError(f"{case.name} has no approved headless Windows fixture")
    archive = HOST_ARCHIVE if platform == "host" else WIN_ARCHIVE
    if not archive.is_file():
        raise FileNotFoundError(f"fresh integrated archive missing: {archive}")
    folder = WORK / case.name / platform
    folder.mkdir(parents=True, exist_ok=True)
    archive_hash = file_hash(archive) if not args.dry_run else ""
    receipt: dict[str, object] = {"case": case.name, "platform": platform,
                                  "archive": str(archive.relative_to(ROOT)),
                                  "archive_sha256": archive_hash,
                                  "sources": {}}
    if case.name == "d3d_resources":
        receipt["scope"] = "probe only; legacy fake COM fixture"
        receipt["native_upload_handle_fixtures"] = [
            "tests/platform_d3d11_resources_native.cpp",
            "tests/platform_d3d11_allocations_native.cpp",
        ]
    if platform == "win" and case.name == "refit_native":
        generated = refit_win_mock_header()
        original = STAGE / "refit-native-work/mock/d3d8.h"
        receipt["mock_headers"] = [
            {"path": str(original.relative_to(ROOT)), "sha256": file_hash(original)},
            {"path": str(generated.relative_to(ROOT)), "sha256": file_hash(generated)},
        ]
    if archive_hash:
        (folder / "archive.sha256").write_text(f"{archive_hash}  {archive}\n")
    outputs: dict[str, str] = {}
    for side in ("before", "current"):
        exe = folder / (side + (".exe" if platform == "win" else ".bin"))
        command = build_command(case, side, platform, exe)
        (folder / f"{side}.command").write_text(shlex.join(command) + "\n")
        if args.dry_run:
            print(shlex.join(command))
            continue
        receipt["sources"][side] = source_receipt(case, side, platform)
        result = call(command, ROOT, timeout=240)
        (folder / f"{side}.build.stderr").write_text(result.stderr)
        if result.returncode:
            raise RuntimeError(f"{case.name}/{platform}/{side} compile exit {result.returncode}\n"
                               + result.stderr[-4000:] + "\ncommand: " + shlex.join(command))
        run_dir = folder / (side + "-run")
        run_dir.mkdir(exist_ok=True)
        if platform == "win":
            env = os.environ.copy()
            env.pop("DISPLAY", None)
            env.pop("WAYLAND_DISPLAY", None)
            env["WINEPREFIX"] = args.wineprefix
            env["WINEDLLOVERRIDES"] = (
                "winemenubuilder.exe=d;winedbg.exe=d;winewayland.drv=d;winex11.drv=d")
            invocation = ["wine", str(exe)]
        else:
            env = None
            invocation = [str(exe)]
        (folder / f"{side}.run.command").write_text(shlex.join(invocation) + "\n")
        if platform == "win":
            # Wine helper processes can inherit pipes. Regular files avoid a
            # false timeout after the headless fixture itself has exited.
            with (folder / f"{side}.stdout").open("w") as output, \
                 (folder / f"{side}.stderr").open("w") as errors:
                result = subprocess.run(invocation, cwd=run_dir, env=env,
                                        stdout=output, stderr=errors,
                                        timeout=180, check=False)
            stdout = (folder / f"{side}.stdout").read_text()
            stderr = (folder / f"{side}.stderr").read_text()
        else:
            result = call(invocation, run_dir, env, timeout=180)
            stdout, stderr = result.stdout, result.stderr
            (folder / f"{side}.stdout").write_text(stdout)
            (folder / f"{side}.stderr").write_text(stderr)
        if result.returncode:
            raise RuntimeError(f"{case.name}/{platform}/{side} run exit {result.returncode}\n"
                               + stderr[-4000:])
        outputs[side] = stdout
    if args.dry_run:
        return
    if file_hash(archive) != archive_hash:
        raise RuntimeError(f"integrated archive changed during {case.name}/{platform}")
    before, current = outputs["before"], outputs["current"]
    receipt["rows"] = len(current.splitlines())
    receipt["before_sha256"] = hashlib.sha256(before.encode()).hexdigest()
    receipt["current_sha256"] = hashlib.sha256(current.encode()).hexdigest()
    receipt["equal"] = before == current
    (folder / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    if before != current:
        diff = list(difflib.unified_diff(before.splitlines(), current.splitlines(),
                                         fromfile="before", tofile="current", lineterm=""))
        raise RuntimeError(f"{case.name}/{platform} parity mismatch\n" + "\n".join(diff[:36]))
    digest = hashlib.sha256(current.encode()).hexdigest()
    rows = len(current.splitlines())
    print(f"PASS {case.name:21} {platform:4} rows={rows:4} sha256={digest}")
    return receipt


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--group", action="append", choices=[x.name for x in CASES],
                        help="repeat to select groups; default is all")
    parser.add_argument("--platform", choices=("host", "win", "both"), default="host")
    parser.add_argument("--run-wine", action="store_true",
                        help="required for Wine execution after coordination with the lead")
    parser.add_argument("--wineprefix", default="/tmp/a2fo-engine-compat-wine")
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--receipt", type=Path,
                        help="optional aggregate JSON receipt for selected cases")
    parser.add_argument("--assemble-receipt", action="store_true",
                        help="merge existing per-case receipts after checking archive/source hashes")
    args = parser.parse_args()
    if args.list:
        for case in CASES:
            print(case.name + (" host,win" if case.win else " host"))
        return 0
    selected = [x for x in CASES if not args.group or x.name in args.group]
    platforms = [args.platform] if args.platform != "both" else ["host", "win"]
    if args.assemble_receipt:
        if not args.receipt:
            parser.error("--assemble-receipt requires --receipt")
        results = []
        try:
            for case in selected:
                for platform in platforms:
                    if platform == "win" and not case.win:
                        continue
                    filename = WORK / case.name / platform / "receipt.json"
                    entry = json.loads(filename.read_text())
                    archive = HOST_ARCHIVE if platform == "host" else WIN_ARCHIVE
                    if not entry["equal"] or entry["archive_sha256"] != file_hash(archive):
                        raise RuntimeError(f"stale or unequal receipt: {filename}")
                    for side in ("before", "current"):
                        for item in entry["sources"][side]:
                            if item["sha256"] != file_hash(ROOT / item["path"]):
                                raise RuntimeError(f"source changed after receipt: {item['path']}")
                    results.append(entry)
        except (FileNotFoundError, KeyError, RuntimeError, ValueError) as error:
            print(f"FAIL {error}", file=sys.stderr)
            return 1
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(json.dumps({"results": results}, indent=2) + "\n")
        print(f"PASS assembled {len(results)} frozen/current receipts: {args.receipt}")
        return 0
    if "win" in platforms and not args.run_wine and not args.dry_run:
        parser.error("Wine requires --run-wine and lead coordination")
    results: list[dict[str, object]] = []
    try:
        for case in selected:
            for platform in platforms:
                if platform == "win" and not case.win:
                    continue
                result = run_case(case, platform, args)
                if result is not None:
                    results.append(result)
    except (FileNotFoundError, RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
        print(f"FAIL {error}", file=sys.stderr)
        if args.receipt:
            args.receipt.parent.mkdir(parents=True, exist_ok=True)
            args.receipt.write_text(json.dumps({"results": results,
                                                "error": str(error)}, indent=2) + "\n")
        return 1
    if args.receipt and not args.dry_run:
        args.receipt.parent.mkdir(parents=True, exist_ok=True)
        args.receipt.write_text(json.dumps({"results": results}, indent=2) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
