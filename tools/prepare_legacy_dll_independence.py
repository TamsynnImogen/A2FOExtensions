#!/usr/bin/env python3
"""Make an offline, reviewable Fleet Ops install copy for legacy DLL trials.

This copies files and supplied images only. It does not inspect or alter PE
contents, prove that imports are gone, deploy, or launch the game.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import shutil
import stat


REPO = Path(__file__).resolve().parents[1]
RETIRED = {"win2kdisabletaskswitch.original.dll", "dxver.dll", "imagehlp.dll"}


def check_path(path, *, missing=False):
    """Reject symlinks in a path, including its existing ancestors."""
    for part in (path, *path.parents):
        try:
            mode = part.lstat().st_mode
        except FileNotFoundError:
            if missing:
                continue
            raise ValueError(f"missing path: {part}") from None
        if stat.S_ISLNK(mode):
            raise ValueError(f"symlink path: {part}")
        if not (stat.S_ISREG(mode) or stat.S_ISDIR(mode)):
            raise ValueError(f"unsupported path: {part}")


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def inventory(root):
    """Validate every source entry and hash its regular files."""
    result = {}
    for current, dirs, files in os.walk(root, followlinks=False):
        for name in dirs + files:
            path = Path(current) / name
            check_path(path)
            mode = path.lstat().st_mode
            if name in files and not stat.S_ISREG(mode):
                raise ValueError(f"unsupported source file: {path}")
            if name in dirs and not stat.S_ISDIR(mode):
                raise ValueError(f"unsupported source directory: {path}")
            if name in files:
                result[path.relative_to(root).as_posix()] = sha256(path)
    return dict(sorted(result.items()))


def inside(path, root):
    return path == root or root in path.parents


def canonical(path, *, missing=False):
    raw = Path(path).absolute()
    check_path(raw, missing=missing)
    resolved = raw.resolve(strict=False)
    check_path(resolved, missing=missing)
    return resolved


def executable_key(key):
    if (not isinstance(key, str) or not key or "\\" in key or ":" in key
            or any(part in ("", ".", "..") for part in key.split("/"))):
        raise ValueError(f"unsafe patched executable path: {key!r}")
    relative = PurePosixPath(key)
    if relative.is_absolute() or relative.suffix.casefold() != ".exe":
        raise ValueError(f"patched image target must be a relative executable: {key!r}")
    if relative.name.casefold() in RETIRED:
        raise ValueError(f"retired DLL cannot be a patched target: {key!r}")
    return relative.as_posix()


def write_receipt(path, record):
    with path.open("x", encoding="utf-8") as stream:
        json.dump(record, stream, indent=2, sort_keys=True)
        stream.write("\n")


def prepare(source_install, output, proxy, patched_armada=None, receipt=None,
            patched_images=None, armada_profile=None, core=None):
    source = canonical(source_install)
    output = canonical(output, missing=True)
    proxy = canonical(proxy)
    patched = canonical(patched_armada) if patched_armada else None
    manifest = canonical(patched_images) if patched_images else None
    profile = canonical(armada_profile) if armada_profile else None
    core_image = canonical(core) if core else None
    quarantine = output.with_name(output.name + ".retired")
    receipt = canonical(receipt, missing=True) if receipt else canonical(output.with_name(output.name + ".receipt.json"), missing=True)
    build = canonical(REPO / "build", missing=True)

    for path in (source, output, proxy, quarantine, receipt, build):
        check_path(path, missing=True)
    if (not source.is_dir() or not (source / "Data" / "ArmadaL.exe").is_file()
            or not (source / "Data" / "Win2kDisableTaskSwitch.dll").is_file()
            or not (source / "FleetOpsHook.dll").is_file()):
        raise ValueError("source must contain Data/ArmadaL.exe, Data/Win2kDisableTaskSwitch.dll, and root FleetOpsHook.dll")
    if not proxy.is_file() or (patched and not patched.is_file()):
        raise ValueError("supplied proxy/patched image must be regular files")
    if manifest and not manifest.is_file():
        raise ValueError("patched-images manifest must be a regular file")
    if profile and not profile.is_file():
        raise ValueError("armada profile must be a regular file")
    if core_image and not core_image.is_file():
        raise ValueError("core image must be a regular file")
    if patched and manifest:
        raise ValueError("--patched-armada and --patched-images cannot be combined")
    if not inside(output, build) or output == build:
        raise ValueError("output must be a child of repository build/")
    if inside(output, source) or inside(source, output):
        raise ValueError("source and output overlap")
    if inside(quarantine, source) or inside(source, quarantine):
        raise ValueError("source and quarantine overlap")
    if output.exists() and (not output.is_dir() or any(output.iterdir())):
        raise ValueError("output already exists and is nonempty")
    if quarantine.exists():
        raise ValueError("quarantine already exists")
    if receipt.exists() or inside(receipt, source) or inside(receipt, output) or inside(receipt, quarantine):
        raise ValueError("receipt exists or overlaps installation paths")
    if inside(proxy, output) or inside(proxy, quarantine) or (patched and (inside(patched, output) or inside(patched, quarantine))):
        raise ValueError("supplied image overlaps staging paths")
    if profile and (inside(profile, output) or inside(profile, quarantine)):
        raise ValueError("supplied profile overlaps staging paths")
    if core_image and (inside(core_image, output) or inside(core_image, quarantine)):
        raise ValueError("supplied core image overlaps staging paths")

    patches = {}
    if manifest:
        mapping = json.loads(manifest.read_text(encoding="utf-8"))
        if not isinstance(mapping, dict):
            raise ValueError("patched-images manifest must be a JSON object")
        seen = set()
        for raw_key, raw_image in mapping.items():
            relative = executable_key(raw_key)
            if relative.casefold() in seen:
                raise ValueError(f"duplicate patched executable path: {relative}")
            seen.add(relative.casefold())
            if not isinstance(raw_image, str) or not raw_image:
                raise ValueError(f"patched image path must be a string: {relative}")
            image = Path(raw_image)
            image = canonical(image if image.is_absolute() else manifest.parent / image)
            if not image.is_file() or inside(image, output) or inside(image, quarantine):
                raise ValueError(f"invalid patched image: {image}")
            patches[relative] = image
    elif patched:
        patches["Data/ArmadaL.exe"] = patched

    before = inventory(source)
    for relative in patches:
        if relative not in before:
            raise ValueError(f"patched executable absent from source: {relative}")
    required_bytes = sum((source / relative).stat().st_size for relative in before) + 256 * 1024 * 1024
    disk_path = output.parent
    while not disk_path.exists():
        disk_path = disk_path.parent
    if shutil.disk_usage(disk_path).free < required_bytes:
        raise ValueError(f"insufficient staging space: need {required_bytes} bytes")
    proxy_hash = sha256(proxy)
    patch_hashes = {relative: sha256(image) for relative, image in patches.items()}
    profile_hash = sha256(profile) if profile else None
    core_hash = sha256(core_image) if core_image else None
    record = {
        "status": "failed", "source_install": str(source), "output": str(output),
        "quarantine": str(quarantine), "source_sha256_before": before,
        "supplied_images": {"proxy": {"path": str(proxy), "sha256": proxy_hash}},
        "replaced": {}, "quarantined": {},
        "ready_for_manual_launch": False,
        "validation_required": ["paired proxy/core contract", "all selected executable imports",
                                "runtime provider parity", "startup lifecycle", "manual game acceptance"],
        "runtime_dependencies": "retired files quarantined, runtime dependencies not proved",
    }
    if manifest:
        record["patched_images_manifest"] = {"path": str(manifest), "sha256": sha256(manifest)}
    for relative, image in patches.items():
        record["supplied_images"][relative] = {"path": str(image), "sha256": patch_hashes[relative]}
    if profile:
        record["supplied_images"]["armada_profile"] = {"path": str(profile), "sha256": profile_hash}
    if core_image:
        record["supplied_images"]["core"] = {"path": str(core_image), "sha256": core_hash}

    try:
        output.mkdir(parents=True, exist_ok=True)
        quarantine.mkdir()
        for current, dirs, _ in os.walk(source, followlinks=False):
            for name in dirs:
                relative_dir = (Path(current) / name).relative_to(source)
                (output / relative_dir).mkdir(parents=True, exist_ok=True)
        for relative, original_hash in before.items():
            src = source / relative
            check_path(src)
            dst = output / relative
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst, follow_symlinks=False)
            if sha256(dst) != original_hash:
                raise ValueError(f"copy hash mismatch: {relative}")

        data = output / "Data"
        for current, _, files in os.walk(data):
            for name in files:
                if name.casefold() not in RETIRED:
                    continue
                staged = Path(current) / name
                relative = staged.relative_to(output)
                retired = quarantine / relative
                retired.parent.mkdir(parents=True, exist_ok=True)
                staged.rename(retired)
                record["quarantined"][relative.as_posix()] = {
                    "source_sha256": before[relative.as_posix()],
                    "quarantine_sha256": sha256(retired),
                    "quarantine_path": str(retired),
                }

        replacements = [("Data/Win2kDisableTaskSwitch.dll", proxy, proxy_hash)]
        replacements.extend((relative, image, patch_hashes[relative]) for relative, image in patches.items())
        if profile:
            replacements.append(("Data/A2FOArmadaProfiles.txt", profile, profile_hash))
        if core_image:
            replacements.append(("Data/A2FOExtensions.dll", core_image, core_hash))
        for relative, image, image_hash in replacements:
            destination = output / relative
            previous_hash = before.get(relative)
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(image, destination, follow_symlinks=False)
            new_hash = sha256(destination)
            if new_hash != image_hash:
                raise ValueError(f"replacement hash mismatch: {relative}")
            record["replaced"][relative] = {
                "source_sha256": previous_hash, "supplied_sha256": image_hash,
                "staged_sha256": new_hash,
            }

        after = inventory(source)
        record["source_sha256_after"] = after
        if after != before:
            raise ValueError("source changed during staging")
        record["counts"] = {
            "source_files": len(before), "staged_files": len(inventory(output)),
            "quarantined_files": len(record["quarantined"]),
            "replaced_files": len(record["replaced"]),
        }
        record["status"] = "prepared_dependency_validation_required"
    except Exception as error:
        record["error"] = str(error)
        record["source_sha256_after"] = inventory(source)
        write_receipt(receipt, record)
        raise
    write_receipt(receipt, record)
    return record


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-install", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--proxy", required=True, type=Path)
    parser.add_argument("--patched-armada", type=Path)
    parser.add_argument("--patched-images", type=Path, help="JSON object mapping source-relative executable paths to supplied images")
    parser.add_argument("--armada-profile", type=Path, help="prepared UTF-8 profile file for staged Data/A2FOArmadaProfiles.txt")
    parser.add_argument("--core", type=Path, help="supplied main image for staged Data/A2FOExtensions.dll")
    parser.add_argument("--receipt", type=Path)
    args = parser.parse_args()
    result = prepare(args.source_install, args.output, args.proxy, args.patched_armada,
                     args.receipt, args.patched_images, args.armada_profile, args.core)
    print(f"{result['status']}: {result['output']}")


if __name__ == "__main__":
    main()
