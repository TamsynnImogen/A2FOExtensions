#!/usr/bin/env python3
"""Sample read-only process and whole-GPU metrics during a manual game test."""

import argparse
import csv
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import platform
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[1]
GAME_NAMES = {"ArmadaL.exe", "Armada2.exe"}
GPU_SCOPE = "whole GPU, not per game VRAM"


def read_identity(pid):
    proc = Path("/proc") / str(pid)
    comm = (proc / "comm").read_text().strip()
    stat = (proc / "stat").read_text()
    # comm is parenthesized and may itself contain spaces or parentheses.
    fields = stat[stat.rfind(")") + 2 :].split()
    return comm, int(fields[19])


def find_game_pid():
    matches = []
    for entry in Path("/proc").iterdir():
        if entry.name.isdigit():
            try:
                comm, starttime = read_identity(int(entry.name))
            except (FileNotFoundError, PermissionError, ProcessLookupError, ValueError, IndexError):
                continue
            if comm in GAME_NAMES:
                matches.append((int(entry.name), comm, starttime))
    if len(matches) != 1:
        raise RuntimeError(
            "expected exactly one ArmadaL.exe or Armada2.exe process; "
            f"found {len(matches)}. Launch the game manually and retry."
        )
    return matches[0]


def select_process(pid):
    if pid is None:
        return find_game_pid()
    comm, starttime = read_identity(pid)
    if comm not in GAME_NAMES:
        raise RuntimeError(f"PID {pid} comm is {comm!r}, expected ArmadaL.exe or Armada2.exe")
    return pid, comm, starttime


def verify_process(pid, expected_comm, expected_starttime):
    comm, starttime = read_identity(pid)
    if comm != expected_comm or starttime != expected_starttime:
        raise RuntimeError(f"PID {pid} identity changed; stopping sampler")


def read_status(pid):
    status = (Path("/proc") / str(pid) / "status").read_text()
    values = {}
    for line in status.splitlines():
        if line.startswith(("VmRSS:", "State:")):
            key, value = line.split(":", 1)
            if key == "VmRSS":
                values["rss_kib"] = int(value.split()[0])
            else:
                values["process_state"] = value.strip()
    return values


def read_pss(pid):
    try:
        text = (Path("/proc") / str(pid) / "smaps_rollup").read_text()
    except (FileNotFoundError, PermissionError, ProcessLookupError):
        return None
    for line in text.splitlines():
        if line.startswith("Pss:"):
            return int(line.split()[1])
    return None


def read_io(pid):
    result = {"read_bytes": None, "write_bytes": None}
    try:
        lines = (Path("/proc") / str(pid) / "io").read_text().splitlines()
    except (FileNotFoundError, PermissionError, ProcessLookupError):
        return result
    for line in lines:
        key, _, value = line.partition(":")
        if key in result:
            try:
                result[key] = int(value.strip())
            except ValueError:
                pass
    return result


def read_cpu_ticks(pid):
    stat = (Path("/proc") / str(pid) / "stat").read_text()
    fields = stat[stat.rfind(")") + 2 :].split()
    return int(fields[11]) + int(fields[12])


def read_gpu():
    command = [
        "nvidia-smi",
        "--query-gpu=uuid,name,memory.used,utilization.gpu",
        "--format=csv,noheader,nounits",
    ]
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=2, check=False)
        if result.returncode:
            return None, f"nvidia-smi exited {result.returncode}: {result.stderr.strip()}"
        gpus = []
        for parts in csv.reader(result.stdout.splitlines(), skipinitialspace=True):
            if len(parts) != 4:
                continue
            parts = [part.strip() for part in parts]
            try:
                memory = int(parts[2])
                utilization = int(parts[3])
            except ValueError:
                memory = utilization = None
            gpus.append({"uuid": parts[0], "name": parts[1],
                         "memory_used_mib": memory, "utilization_percent": utilization})
        if not gpus:
            return None, "nvidia-smi returned no GPU rows"
        return gpus, None
    except (FileNotFoundError, subprocess.TimeoutExpired, OSError) as error:
        return None, str(error)


def utc_now():
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds").replace("+00:00", "Z")


def write_summary(path, summary):
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(summary, indent=2) + "\n")
    os.replace(temporary, path)


def parse_args(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--label", required=True, help="manual test context label; not used to infer a mod")
    parser.add_argument("--output", required=True, type=Path, help="JSONL output path beneath repository build/")
    parser.add_argument("--seconds", type=float, default=120)
    parser.add_argument("--interval", type=float, default=1)
    parser.add_argument("--pid", type=int, help="game PID; otherwise require exactly one matching process")
    parser.add_argument("--validate-only", action="store_true", help="validate options without inspecting a game process")
    args = parser.parse_args(argv)
    output = args.output.resolve()
    build_root = (ROOT / "build").resolve()
    if output == build_root or not output.is_relative_to(build_root):
        parser.error("--output must be beneath repository build/")
    if not math.isfinite(args.seconds) or args.seconds <= 0:
        parser.error("--seconds must be finite and positive")
    if not math.isfinite(args.interval) or args.interval < 0.1:
        parser.error("--interval must be finite and at least 0.1 seconds")
    if args.pid is not None and args.pid <= 0:
        parser.error("--pid must be a positive integer")
    args.output = output
    summary_path = output.with_suffix(output.suffix + ".summary.json")
    temporary_summary_path = summary_path.with_name(summary_path.name + ".tmp")
    existing = [path for path in (output, summary_path, temporary_summary_path) if path.exists()]
    if existing:
        parser.error("refusing to overwrite existing output artifact(s): " + ", ".join(map(str, existing)))
    return args


def main(argv=None):
    args = parse_args(argv)
    if args.validate_only:
        print(f"options valid; output={args.output}")
        return 0
    try:
        pid, comm, starttime = select_process(args.pid)
    except (FileNotFoundError, PermissionError, ProcessLookupError, RuntimeError, ValueError) as error:
        print(f"monitor: {error}", file=sys.stderr)
        return 1
    args.output.parent.mkdir(parents=True, exist_ok=True)
    summary_path = args.output.with_suffix(args.output.suffix + ".summary.json")
    started_wall = utc_now()
    started = time.monotonic()
    sample_count = 0
    summary = {
        "label": args.label,
        "pid": pid,
        "proc_comm": comm,
        "proc_starttime_ticks": starttime,
        "started_utc": started_wall,
        "python": platform.python_version(),
        "platform": platform.platform(),
        "sampler": Path(__file__).name,
        "proc_status_fields": ["VmRSS", "State"],
        "gpu_memory_scope": GPU_SCOPE,
        "sample_count": 0,
        "stopped_reason": "duration elapsed",
    }
    try:
        with args.output.open("x", encoding="utf-8") as output:
            while True:
                elapsed = time.monotonic() - started
                if elapsed > args.seconds:
                    break
                verify_process(pid, comm, starttime)
                status = read_status(pid)
                gpu, gpu_error = read_gpu()
                sample = {
                    "timestamp_utc": utc_now(),
                    "elapsed_monotonic_seconds": round(time.monotonic() - started, 3),
                    "label": args.label,
                    "pid": pid,
                    **status,
                    "pss_kib": read_pss(pid),
                    "proc_cpu_ticks": read_cpu_ticks(pid),
                    **read_io(pid),
                    "gpu_memory_scope": GPU_SCOPE,
                    "gpus": gpu,
                    "gpu_error": gpu_error,
                }
                verify_process(pid, comm, starttime)
                output.write(json.dumps(sample, separators=(",", ":")) + "\n")
                output.flush()
                sample_count += 1
                remaining = args.interval - (time.monotonic() - started - elapsed)
                if remaining > 0:
                    time.sleep(min(remaining, max(0, args.seconds - (time.monotonic() - started))))
    except (FileNotFoundError, PermissionError, ProcessLookupError, RuntimeError, ValueError) as error:
        summary["stopped_reason"] = str(error)
    finally:
        summary["sample_count"] = sample_count
        summary["finished_utc"] = utc_now()
        summary["elapsed_seconds"] = round(time.monotonic() - started, 3)
        write_summary(summary_path, summary)
    print(f"{sample_count} samples -> {args.output}; summary -> {summary_path}")
    return 0 if summary["stopped_reason"] == "duration elapsed" else 1


if __name__ == "__main__":
    sys.exit(main())
