"""Archive owned Refit logs around one manually operated package session.

The caller owns the per-package lock and all process lifecycle decisions.
This module never starts a game or interacts with a desktop or input device.
"""

from __future__ import annotations

from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import secrets
import shutil
import stat


DATA_LOGS = (
    "A2FOExtensions.log", "A2FOStartupProxy.log", "A2FORenderer.log",
    "A2FORefitDisplay.log", "A2FORefitRenderer.log",
)
NATIVE_LOGS = Path("prefix/drive_c/users/tamsynn/AppData/Local/armada2-refit/logs")
FACT_PATTERNS = {
    "profile_admitted": ("child-only factory route armed",),
    "profile_rejected": ("exact executable or live mode ownership rejected",),
    "source_admitted": ("pre-workspace CPU sources admitted outside loader lock",),
    "source_refused": ("pre-workspace CPU source admission refused",),
    "factory_returned": ("Vulkan factory returned", "DX12 factory returned"),
    "factory_refused": ("native factory preparation refused", "native factory preparation failed"),
    "scoped_mesh_submitted": ("first owned single-group mesh submitted through native scene API",),
}
REFUSAL = re.compile(r"\b(refused|rejected|failed)\b", re.IGNORECASE)
MAX_MATCHING_LINES = 80
MAX_LINE_LENGTH = 1000
SHA256_RE = re.compile(r"[0-9a-fA-F]{64}")


def _utc() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


def _checked(path: Path, *, kind: str, optional: bool = False) -> Path | None:
    """Reject linked components, including parents, before any file operation."""
    path = Path(path).absolute()
    for part in (path, *path.parents):
        if part.is_symlink():
            raise ValueError(f"linked path refused: {part}")
    if not path.exists():
        if optional:
            return None
        raise ValueError(f"missing {kind}: {path}")
    mode = path.lstat().st_mode
    wanted = stat.S_ISDIR if kind == "directory" else stat.S_ISREG
    if not wanted(mode) or path.resolve(strict=True) != path:
        raise ValueError(f"ordinary canonical {kind} required: {path}")
    return path


def _package(package: Path) -> Path:
    root = _checked(package, kind="directory")
    if root is None:
        raise ValueError("missing package directory")
    _checked(root / "package.json", kind="file")
    return root


def _within(root: Path, child: Path) -> Path:
    child = Path(child).absolute()
    if not child.is_relative_to(root) or child == root:
        raise ValueError(f"path escapes package: {child}")
    return child


def _source_dirs(root: Path) -> tuple[tuple[str, Path | None], ...]:
    return (
        ("data", _checked(_within(root, root / "Data"), kind="directory", optional=True)),
        ("native", _checked(_within(root, root / NATIVE_LOGS), kind="directory", optional=True)),
    )


def _owned_logs(root: Path) -> list[tuple[str, Path]]:
    found: list[tuple[str, Path]] = []
    for label, directory in _source_dirs(root):
        if directory is None:
            continue
        paths = (directory / name for name in DATA_LOGS) if label == "data" else directory.glob("*.log")
        for path in paths:
            checked = _checked(_within(root, path), kind="file", optional=True)
            if checked is not None:
                found.append((label, checked))
    return found


def _write(path: Path, value: dict) -> None:
    _checked(path.parent, kind="directory")
    _checked(path, kind="file", optional=True)
    path.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def _digest(path: Path) -> dict:
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
            size += len(block)
    return {"sha256": digest.hexdigest(), "size": size}


def _input_identity(root: Path) -> dict:
    """Read the manifest and pin its critical inputs without retaining contents."""
    manifest = _checked(root / "package.json", kind="file")
    if manifest is None:
        raise ValueError("missing package manifest")
    manifest_bytes = manifest.read_bytes()
    manifest_hash = hashlib.sha256(manifest_bytes).hexdigest()
    try:
        data = json.loads(manifest_bytes.decode("utf-8"))
    except (UnicodeError, json.JSONDecodeError) as exc:
        raise ValueError("invalid package manifest") from exc
    critical = data.get("critical_sha256") if isinstance(data, dict) else None
    if not isinstance(critical, dict):
        raise ValueError("package manifest critical_sha256 must be an object")
    inputs = []
    for relative, expected in sorted(critical.items()):
        if (not isinstance(relative, str) or not relative or "\\" in relative or
                Path(relative).is_absolute() or ".." in Path(relative).parts or
                not isinstance(expected, str) or not SHA256_RE.fullmatch(expected)):
            raise ValueError("invalid critical package input entry")
        candidate = _within(root, root / relative)
        checked = _checked(candidate, kind="file")
        if checked is None:
            raise ValueError("missing package input: " + relative)
        actual = _digest(checked)
        if actual["sha256"].lower() != expected.lower():
            raise ValueError("package input hash mismatch: " + relative)
        inputs.append({"path": relative, "expected_sha256": expected.lower(),
                       "sha256": actual["sha256"], "size": actual["size"]})
    return {"manifest_sha256": manifest_hash, "inputs": inputs}


def _finish_input_identity(root: Path, pinned: dict) -> dict:
    """Rehash only paths captured by begin; never follow a changed manifest list."""
    manifest = {"status": "invalid"}
    manifest_hash = None
    try:
        current_manifest = _checked(root / "package.json", kind="file")
        if current_manifest is None:
            raise ValueError("missing file: " + str(root / "package.json"))
        manifest_bytes = current_manifest.read_bytes()
        manifest_hash = hashlib.sha256(manifest_bytes).hexdigest()
        manifest = {"status": "matched" if manifest_hash == pinned.get("manifest_sha256") else "changed"}
    except (ValueError, OSError) as exc:
        manifest["status"] = "missing" if isinstance(exc, ValueError) and str(exc).startswith("missing file:") else "invalid"
        manifest["error"] = str(exc)
    entries = []
    all_match = True
    for original in pinned.get("inputs", []):
        relative = original.get("path") if isinstance(original, dict) else None
        item = {"path": relative, "expected_sha256": original.get("expected_sha256") if isinstance(original, dict) else None,
                "status": "invalid"}
        try:
            if (not isinstance(relative, str) or not relative or "\\" in relative or
                    Path(relative).is_absolute() or ".." in Path(relative).parts):
                raise ValueError("invalid pinned path")
            path = _checked(_within(root, root / relative), kind="file")
            if path is None:
                raise ValueError("missing file: " + relative)
            measured = _digest(path)
            item.update(status="matched" if measured["sha256"] == item["expected_sha256"] else "changed",
                        sha256=measured["sha256"], size=measured["size"])
        except (ValueError, OSError) as exc:
            item["status"] = "missing" if isinstance(exc, ValueError) and str(exc).startswith("missing file:") else "invalid"
            item["error"] = str(exc)
        if item["status"] != "matched":
            all_match = False
        entries.append(item)
    manifest_matches = manifest["status"] == "matched"
    return {"manifest_sha256_before": pinned.get("manifest_sha256"),
            "manifest_sha256_after": manifest_hash, "manifest_matches": manifest_matches,
            "manifest_status": manifest["status"], "manifest_error": manifest.get("error"),
            "all_inputs_match": all_match and manifest_matches, "inputs": entries}


def begin(package: Path, backend: str, mode: str) -> Path:
    """Create a session and move only owned preexisting logs into before/."""
    root = _package(package)
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", backend) or not re.fullmatch(r"[A-Za-z0-9_-]{1,32}", mode):
        raise ValueError("backend and mode must be simple labels")
    identity = _input_identity(root)  # Fail closed before moving any preexisting logs.
    logs = _owned_logs(root)  # Validate all source paths before the first move.
    log_root = root / "logs"
    if _checked(log_root, kind="directory", optional=True) is None:
        log_root.mkdir(mode=0o700)
    session = log_root / (datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ") +
                          "-" + secrets.token_hex(4) + "-" + backend + "-" + mode)
    session.mkdir(mode=0o700)
    metadata = {"package": str(root), "backend": backend, "mode": mode,
                "start_utc": _utc(), "input_automation": False, "status": "begun",
                "input_identity": identity}
    _write(session / "begin.json", metadata)
    try:
        for label, source in logs:
            destination = session / "before" / label / source.name
            destination.parent.mkdir(parents=True, exist_ok=True)
            _checked(destination.parent, kind="directory")
            source.rename(destination)
    except Exception as exc:
        metadata.update(status="begin_failed", error=str(exc))
        _write(session / "begin.json", metadata)
        raise
    return session


def finish(package: Path, session: Path, returncode: int, error: str = "") -> dict:
    """Copy fresh owned logs into after/ and write a limited evidence report."""
    root = _checked(package, kind="directory")
    if root is None:
        raise ValueError("missing package directory")
    log_root = _checked(root / "logs", kind="directory")
    if log_root is None:
        raise ValueError("missing logs directory")
    session = _within(log_root, session)
    _checked(session, kind="directory")
    if session.parent != log_root:
        raise ValueError("session must be an immediate package logs child")
    begin_path = _checked(session / "begin.json", kind="file")
    if begin_path is None:
        raise ValueError("missing begin metadata")
    begin_data = json.loads(begin_path.read_text(encoding="utf-8"))
    if begin_data.get("package") != str(root) or begin_data.get("input_automation") is not False:
        raise ValueError("session metadata does not match package")
    report = {**begin_data, "end_utc": _utc(), "returncode": returncode,
              "error": error, "status": ("stopped" if returncode == 130 and not error else
                                           "failed" if error or returncode else "finished"),
              "copies": {}, "facts": {key: False for key in FACT_PATTERNS},
              "refusals_observed": False, "matching_lines": [],
              "acceptance": "Log evidence only; hardware, visual, and gameplay acceptance unverified."}
    report["input_identity_after"] = _finish_input_identity(root, begin_data.get("input_identity", {}))
    try:
        for label, source in _owned_logs(root):
            destination = session / "after" / label / source.name
            destination.parent.mkdir(parents=True, exist_ok=True)
            _checked(destination.parent, kind="directory")
            if destination.exists() or destination.is_symlink():
                raise ValueError(f"after log already exists: {destination}")
            shutil.copyfile(source, destination)
            report["copies"][str(destination.relative_to(session))] = _digest(destination)
            with destination.open("r", encoding="utf-8", errors="replace") as stream:
                for number, line in enumerate(stream, 1):
                    matches = [key for key, needles in FACT_PATTERNS.items()
                               if any(needle in line for needle in needles)]
                    refused = bool(REFUSAL.search(line))
                    if not matches and not refused:
                        continue
                    for key in matches:
                        report["facts"][key] = True
                    report["refusals_observed"] |= refused
                    if len(report["matching_lines"]) < MAX_MATCHING_LINES:
                        report["matching_lines"].append({"file": str(destination.relative_to(session)),
                                                         "line": number, "text": line.rstrip("\r\n")[:MAX_LINE_LENGTH]})
    except Exception as exc:
        report["status"] = "evidence_failed"
        report["evidence_error"] = str(exc)
        _write(session / "evidence.json", report)
        raise
    _write(session / "evidence.json", report)
    return report
