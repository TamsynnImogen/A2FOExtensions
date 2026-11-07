#!/usr/bin/env python3
"""Summarize bounded shader payloads from D3D8 observer JSONL traces."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
import sys
from pathlib import Path
from typing import Any


STATUSES = {
    "complete", "absent", "unreadable", "unsupported", "invalid",
    "word_limit", "wrong_stage",
}
SHADER_METHODS = {
    "CreateVertexShader", "CreatePixelShader", "DeleteVertexShader",
    "DeletePixelShader", "SetVertexShader", "SetPixelShader",
    "GetVertexShader", "GetPixelShader", "SetVertexShaderConstant",
    "SetPixelShaderConstant", "GetVertexShaderConstant",
    "GetPixelShaderConstant",
}
MAX_WORDS = 4096


class TraceError(ValueError):
    pass


def fail(path: Path, line: int, message: str) -> TraceError:
    return TraceError(f"{path}:{line}: {message}")


def parse_word(value: Any, path: Path, line: int) -> int:
    if not isinstance(value, str) or re.fullmatch(r"0x[0-9a-fA-F]{8}", value) is None:
        raise fail(path, line, f"invalid DWORD {value!r}; expected 0x plus 8 hex digits")
    try:
        return int(value[2:], 16)
    except ValueError as exc:
        raise fail(path, line, f"invalid DWORD {value!r}; expected 0x plus 8 hex digits") from exc


def payload_report(payload: Any, label: str, path: Path, line: int) -> dict[str, Any]:
    if not isinstance(payload, dict):
        raise fail(path, line, f"shader {label} must be an object")
    status = payload.get("status")
    if not isinstance(status, str) or status not in STATUSES:
        raise fail(path, line, f"shader {label} has invalid status {status!r}")
    words = payload.get("words")
    if not isinstance(words, list):
        raise fail(path, line, f"shader {label}.words must be an array")
    if len(words) > MAX_WORDS:
        raise fail(path, line, f"shader {label} exceeds {MAX_WORDS} words")
    parsed = [parse_word(word, path, line) for word in words]
    if status == "complete" and not parsed:
        raise fail(path, line, f"complete shader {label} has no words")
    if status != "complete" and parsed:
        raise fail(path, line, f"incomplete shader {label} must not contain words")
    report: dict[str, Any] = {"status": status, "word_count": len(parsed)}
    if status == "complete":
        raw = b"".join(struct.pack("<I", word) for word in parsed)
        report["sha256"] = hashlib.sha256(raw).hexdigest()
    return report


def shader_event(row: dict[str, Any], path: Path, line: int) -> dict[str, Any]:
    stage = row.get("stage")
    if stage not in ("vertex", "pixel"):
        raise fail(path, line, f"shader stage must be vertex or pixel, got {stage!r}")
    for key in ("capture", "frame", "object", "handle", "usage"):
        if not isinstance(row.get(key), int) or isinstance(row.get(key), bool):
            raise fail(path, line, f"shader {key} must be an integer")
    declaration = payload_report(row.get("declaration"), "declaration", path, line)
    function = payload_report(row.get("function"), "function", path, line)
    if function["status"] == "complete":
        first = parse_word(row["function"]["words"][0], path, line)
        expected = 0xFFFE if stage == "vertex" else 0xFFFF
        if first >> 16 != expected:
            raise fail(path, line, "complete shader function version does not match stage")
        function["version_token"] = f"0x{first:08x}"
        function["version"] = f"{(first >> 8) & 0xff}.{first & 0xff}"
    return {
        "capture": row["capture"], "frame": row["frame"], "object": row["object"],
        "stage": stage, "handle": row["handle"], "usage": row["usage"],
        "declaration": declaration, "function": function,
    }


def summarize_trace(path: Path) -> dict[str, Any]:
    try:
        raw = path.read_bytes()
    except OSError as exc:
        raise TraceError(f"{path}: {exc}") from exc
    digest = hashlib.sha256(raw).hexdigest()
    session: dict[str, Any] | None = None
    markers: list[dict[str, Any]] = []
    shaders: list[dict[str, Any]] = []
    call_rows: dict[str, int] = {}
    method_latest: dict[str, int] = {}
    method_max: dict[str, int] = {}
    signature_counts: dict[str, dict[str, dict[str, int]]] = {}
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        raise TraceError(f"{path}:{exc.start}: trace is not UTF-8") from exc
    for number, text_line in enumerate(text.splitlines(), 1):
        try:
            row = json.loads(text_line)
        except json.JSONDecodeError as exc:
            raise fail(path, number, f"malformed JSON: {exc.msg}") from exc
        if not isinstance(row, dict):
            raise fail(path, number, "JSONL record must be an object")
        event = row.get("event")
        if event == "session":
            if session is None:
                session = {k: row[k] for k in ("schema", "pid", "backend", "trace", "shader_capture_limit", "shader_word_limit") if k in row}
        elif event in ("file_limit", "shader_capture_limit"):
            markers.append({k: v for k, v in row.items() if k != "event"} | {"event": event})
        elif event == "shader":
            shaders.append(shader_event(row, path, number))
        elif event == "call":
            method = row.get("method")
            if isinstance(method, str) and method in SHADER_METHODS:
                call_rows[method] = call_rows.get(method, 0) + 1
        elif event == "count":
            qualified_method, calls = row.get("method"), row.get("calls")
            if isinstance(qualified_method, str) and isinstance(calls, int) and not isinstance(calls, bool) and calls >= 0:
                method = qualified_method.rsplit(".", 1)[-1]
                if method in SHADER_METHODS:
                    method_latest[method] = calls
                    method_max[method] = max(calls, method_max.get(method, 0))
        elif event == "signature":
            key, calls = row.get("key"), row.get("calls")
            if isinstance(key, str) and isinstance(calls, int) and not isinstance(calls, bool) and calls >= 0:
                method = key.split("|", 1)[0].rsplit(".", 1)[-1]
                if method in SHADER_METHODS:
                    per_key = signature_counts.setdefault(method, {}).setdefault(
                        key, {"latest_calls": calls, "max_calls": calls}
                    )
                    per_key["latest_calls"] = calls
                    per_key["max_calls"] = max(calls, per_key["max_calls"])
    method_counts = {
        method: {
            "call_rows": call_rows.get(method, 0),
            "latest_method_calls": method_latest.get(method),
            "max_method_calls": method_max.get(method),
            "signature_snapshots": signature_counts.get(method, {}),
        }
        for method in sorted(SHADER_METHODS)
        if method in call_rows or method in method_latest or method in signature_counts
    }
    return {
        "input": str(path), "input_sha256": digest, "input_evidence": "unlabelled; caller must distinguish fixtures from games",
        "session": session, "markers": markers, "shader_method_calls": method_counts,
        "shader_capture_count": len(shaders),
        "shader_contents": "some complete function payloads captured" if any(
            shader["function"]["status"] == "complete" for shader in shaders
        ) else "unknown",
        "complete_declaration_count": sum(shader["declaration"]["status"] == "complete" for shader in shaders),
        "shaders": shaders,
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("traces", nargs="+", type=Path, help="one or more JSONL trace paths")
    parser.add_argument("--output", required=True, type=Path, help="output JSON report path")
    args = parser.parse_args(argv)
    try:
        report = {"report_schema": 1, "inputs": [summarize_trace(path) for path in args.traces]}
        args.output.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    except (TraceError, OSError) as exc:
        print(str(exc), file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
