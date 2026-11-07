#!/usr/bin/env python3
"""Reviewable, backup-first Native11 clock trial for exactly two live files."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import stat
import tempfile


KEYS = ("CaptureScene", "CaptureAssets", "CaptureTextures", "CaptureMaterials",
        "CaptureNodes", "CaptureAnimationClock")
MANIFEST = "native11-clock-trial.json"


def sha256_bytes(data):
    return hashlib.sha256(data).hexdigest()


def checked_file(path, name=None):
    if name and path.name != name:
        raise ValueError(f"target must be named {name}")
    if not path.is_absolute() or ".." in path.parts:
        raise ValueError("paths must be absolute without parent traversal")
    for part in (path, *path.parents):
        mode = part.lstat().st_mode
        if stat.S_ISLNK(mode):
            raise ValueError(f"symlink refused: {part}")
        if part == path and not stat.S_ISREG(mode):
            raise ValueError(f"regular file required: {part}")
    return path


def checked_new_dir(path):
    if not path.is_absolute() or ".." in path.parts:
        raise ValueError("backup directory must be an absolute path without parent traversal")
    if path.exists() or path.is_symlink():
        raise ValueError("backup directory already exists")
    for part in path.parents:
        mode = part.lstat().st_mode
        if stat.S_ISLNK(mode) or not stat.S_ISDIR(mode):
            raise ValueError(f"unsafe backup ancestor: {part}")


def read_file(path, name=None):
    checked_file(path, name)
    return path.read_bytes()


def update_ini(data):
    # Latin-1 is a lossless byte mapping and also preserves cp1252 text.
    text = data.decode("latin-1")
    if "\r\n" in text:
        eol = "\r\n"
        if "\n" in text.replace("\r\n", ""):
            raise ValueError("mixed INI line endings")
    else:
        eol = "\n"
    lines = text.splitlines(keepends=True)
    sections = []
    for index, line in enumerate(lines):
        match = re.match(r"^\s*\[([^\]\r\n]+)\]\s*(?:[;#].*)?(?:\r?\n)?$", line)
        if match and match.group(1).strip().casefold() == "native11":
            sections.append(index)
    if len(sections) > 1:
        raise ValueError("multiple Native11 sections")
    if not sections:
        if text and not text.endswith(("\n", "\r")):
            lines.append(eol)
        lines.extend(["[Native11]" + eol] + [f"{key}=1{eol}" for key in KEYS])
        return "".join(lines).encode("latin-1")
    start = sections[0] + 1
    end = len(lines)
    for index in range(start, len(lines)):
        if re.match(r"^\s*\[[^\]\r\n]+\]", lines[index]):
            end = index
            break
    seen = set()
    for index in range(start, end):
        body = lines[index].rstrip("\r\n")
        ending = lines[index][len(body):]
        match = re.match(r"^([ \t]*([^=;#\s]+)[ \t]*=[ \t]*)([^;#]*?)([ \t]*(?:[;#].*)?)$", body)
        if not match:
            continue
        key = match.group(2).casefold()
        if key in (item.casefold() for item in KEYS):
            if key in seen:
                raise ValueError("duplicate selected Native11 key")
            seen.add(key)
            lines[index] = match.group(1) + "1" + match.group(4) + ending
    missing = [key for key in KEYS if key.casefold() not in seen]
    if missing:
        if end and not lines[end - 1].endswith(("\n", "\r")):
            lines[end - 1] += eol
        lines[end:end] = [f"{key}=1{eol}" for key in missing]
    return "".join(lines).encode("latin-1")


def game_running():
    for entry in Path("/proc").iterdir():
        if not entry.name.isdigit():
            continue
        try:
            if (entry / "comm").read_text().strip().casefold() in ("armadal.exe", "armada2.exe"):
                return True
        except (OSError, UnicodeError):
            continue
    return False


def atomic_replace(path, data, mode):
    fd, temp_name = tempfile.mkstemp(prefix=".native11-clock-", dir=path.parent)
    try:
        with os.fdopen(fd, "wb") as stream:
            os.fchmod(stream.fileno(), mode)
            stream.write(data)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temp_name, path)
        directory_fd = os.open(path.parent, os.O_RDONLY)
        try:
            os.fsync(directory_fd)
        finally:
            os.close(directory_fd)
    finally:
        if os.path.exists(temp_name):
            os.unlink(temp_name)


def write_manifest(backup, manifest):
    path = backup / MANIFEST
    with path.open("x", encoding="utf-8") as stream:
        json.dump(manifest, stream, indent=2, sort_keys=True)
        stream.write("\n")
        stream.flush()
        os.fsync(stream.fileno())
    directory_fd = os.open(backup, os.O_RDONLY)
    try:
        os.fsync(directory_fd)
    finally:
        os.close(directory_fd)


def prepare(receipt_path, candidate_path, core_path, config_path):
    for raw in (receipt_path, candidate_path, core_path, config_path):
        if str(raw) != os.path.abspath(str(raw)) or ".." in raw.parts:
            raise ValueError("exact absolute lexical paths required")
    receipt = json.loads(read_file(receipt_path).decode("utf-8"))
    protected = receipt["protected_live_sha256_unchanged"]
    if not isinstance(protected, dict) or not protected:
        raise ValueError("protected path map required")
    if str(core_path) not in protected or str(config_path) not in protected:
        raise ValueError("exact targets absent from receipt protected path map")
    if core_path == config_path or candidate_path in (core_path, config_path):
        raise ValueError("candidate and targets must be distinct")
    candidate = read_file(candidate_path, "A2FOExtensions.dll")
    if not candidate:
        raise ValueError("candidate is empty")
    candidate_hash = sha256_bytes(candidate)
    if receipt["core_dll"]["core_sha256"] != candidate_hash:
        raise ValueError("candidate hash differs from receipt core DLL")
    original = {}
    for raw_path, expected in protected.items():
        if not isinstance(raw_path, str) or not isinstance(expected, str) or not re.fullmatch(r"[0-9a-f]{64}", expected):
            raise ValueError("invalid protected path or hash")
        current = read_file(Path(raw_path))
        if sha256_bytes(current) != expected:
            raise ValueError(f"protected file hash changed: {raw_path}")
        if raw_path in (str(core_path), str(config_path)):
            original[raw_path] = current
    core = original[str(core_path)]
    config = original[str(config_path)]
    updated = update_ini(config)
    rows = []
    for path, before, after, backup_name in ((core_path, core, candidate, "core.original"),
                                              (config_path, config, updated, "config.original")):
        checked_file(path, "A2FOExtensions.dll" if path == core_path else "A2FORenderer.ini")
        rows.append({"target": str(path), "backup": backup_name, "before_sha256": sha256_bytes(before),
                     "after_sha256": sha256_bytes(after), "mode": stat.S_IMODE(path.stat().st_mode)})
    return {"candidate": str(candidate_path), "candidate_sha256": candidate_hash,
            "keys": {key: "1" for key in KEYS}, "files": rows}, (core, config), (candidate, updated)


def deploy(plan, originals, replacements, backup):
    checked_new_dir(backup)
    if game_running():
        raise ValueError("Armada process is running")
    backup.mkdir(mode=0o700, exist_ok=False)
    rows = plan["files"]
    for row, original in zip(rows, originals):
        dest = backup / row["backup"]
        with dest.open("xb") as stream:
            stream.write(original)
            stream.flush()
            os.fsync(stream.fileno())
        if sha256_bytes(read_file(dest)) != row["before_sha256"]:
            raise ValueError("backup verification failed")
    write_manifest(backup, plan)
    for row in rows:
        if sha256_bytes(read_file(Path(row["target"]))) != row["before_sha256"]:
            raise ValueError("target changed before deployment")
    try:
        for row, after in zip(rows, replacements):
            target = Path(row["target"])
            if sha256_bytes(read_file(target)) != row["before_sha256"]:
                raise ValueError("target changed during deployment")
            atomic_replace(target, after, row["mode"])
            if sha256_bytes(read_file(target)) != row["after_sha256"]:
                raise ValueError("deployed hash mismatch")
    except Exception as failure:
        # A replace may succeed before a later fsync reports failure.
        incomplete = []
        for row in reversed(rows):
            target = Path(row["target"])
            try:
                current = sha256_bytes(read_file(target))
                if current == row["after_sha256"] and current != row["before_sha256"]:
                    atomic_replace(target, read_file(backup / row["backup"]), row["mode"])
                    if sha256_bytes(read_file(target)) != row["before_sha256"]:
                        raise OSError("recovery hash mismatch")
                elif current != row["before_sha256"]:
                    incomplete.append(str(target))
            except (OSError, ValueError):
                incomplete.append(str(target))
        if incomplete:
            raise OSError(f"deployment failed; incomplete recovery for {incomplete}; "
                          f"inspect backup manifest {backup / MANIFEST}") from failure
        raise


def rollback(backup, apply=False):
    checked_file(backup / MANIFEST)
    plan = json.loads(read_file(backup / MANIFEST).decode("utf-8"))
    rows = plan["files"]
    if len(rows) != 2 or [row["backup"] for row in rows] != ["core.original", "config.original"]:
        raise ValueError("invalid rollback manifest")
    originals = []
    for row, name in zip(rows, ("A2FOExtensions.dll", "A2FORenderer.ini")):
        target = Path(row["target"])
        checked_file(target, name)
        original = read_file(backup / row["backup"])
        if sha256_bytes(original) != row["before_sha256"]:
            raise ValueError("backup hash mismatch")
        if sha256_bytes(read_file(target)) not in (row["before_sha256"], row["after_sha256"]):
            raise ValueError("target no longer matches original or deployed hash")
        originals.append(original)
    result = {"action": "rollback" if apply else "rollback dry run", "files": rows}
    if apply:
        if game_running():
            raise ValueError("Armada process is running")
        for row, original in zip(rows, originals):
            target = Path(row["target"])
            current = sha256_bytes(read_file(target))
            if current not in (row["before_sha256"], row["after_sha256"]):
                raise ValueError("target changed during rollback")
            if current == row["before_sha256"]:
                continue
            atomic_replace(target, original, row["mode"])
            if sha256_bytes(read_file(target)) != row["before_sha256"]:
                raise ValueError("restored hash mismatch")
    return result


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--receipt", type=Path)
    parser.add_argument("--candidate", type=Path)
    parser.add_argument("--core-target", type=Path)
    parser.add_argument("--config-target", type=Path)
    parser.add_argument("--backup-dir", type=Path)
    parser.add_argument("--rollback", type=Path, metavar="BACKUPDIR")
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args(argv)
    try:
        if args.rollback:
            if any((args.receipt, args.candidate, args.core_target, args.config_target, args.backup_dir)):
                parser.error("--rollback cannot be combined with deployment paths")
            result = rollback(args.rollback, args.apply)
        else:
            if not all((args.receipt, args.candidate, args.core_target, args.config_target)):
                parser.error("deployment requires --receipt, --candidate, --core-target, --config-target")
            if args.apply and not args.backup_dir:
                parser.error("--apply requires --backup-dir")
            result, originals, replacements = prepare(args.receipt, args.candidate, args.core_target, args.config_target)
            result["action"] = "apply" if args.apply else "dry run"
            if args.apply:
                deploy(result, originals, replacements, args.backup_dir)
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        parser.exit(1, f"refused: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
