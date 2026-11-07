#!/usr/bin/env python3
"""Build an isolated BC5/BC7 DDS texture tree from an explicit, checked plan.

No installed assets are changed. A receipt is required before an existing output
can be reused; output headers and complete block extents are checked each run.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import re
import signal
import stat
import struct
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath


REPO = Path(__file__).resolve().parent.parent
DEFAULT_TIMEOUT = 1800
MIP_POLICY = "preserve-supplied-v1"
SHA256 = re.compile(r"[0-9a-f]{64}\Z")


def digest(path: Path) -> str:
    sha = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            sha.update(block)
    return sha.hexdigest()


def plain_path(path: Path, *, allow_missing: bool = False) -> None:
    """Reject links at every existing path component, including the root."""
    for component in (path, *path.parents):
        try:
            mode = component.lstat().st_mode
        except FileNotFoundError:
            if allow_missing:
                continue
            raise ValueError(f"missing path: {component}") from None
        if stat.S_ISLNK(mode):
            raise ValueError(f"symlink path: {component}")


def inside(child: Path, parent: Path) -> bool:
    return child == parent or parent in child.parents


def relative_name(raw: object) -> PurePosixPath:
    if not isinstance(raw, str) or not raw or "\\" in raw:
        raise ValueError("relative_path must be a nonempty POSIX path")
    path = PurePosixPath(raw)
    if path.is_absolute() or any(part in ("", ".", "..") for part in raw.split("/")):
        raise ValueError(f"unsafe relative_path: {raw!r}")
    if path.suffix.casefold() != ".dds":
        raise ValueError(f"expected DDS relative_path: {raw!r}")
    return path


def full_mips(width: int, height: int) -> int:
    levels = 1
    while width > 1 or height > 1:
        width, height = max(1, width // 2), max(1, height // 2)
        levels += 1
    return levels


def dds_header(path: Path) -> tuple[int, int, int, bytes, int]:
    with path.open("rb") as stream:
        head = stream.read(148)
    if len(head) < 128 or head[:4] != b"DDS ":
        raise ValueError("missing/truncated DDS header")
    if struct.unpack_from("<I", head, 4)[0] != 124 or struct.unpack_from("<I", head, 76)[0] != 32:
        raise ValueError("invalid DDS header sizes")
    height, width = struct.unpack_from("<II", head, 12)
    depth, mips = struct.unpack_from("<II", head, 24)
    mips = mips or 1
    if not (0 < width <= 131072 and 0 < height <= 131072 and 0 < mips <= full_mips(width, height)):
        raise ValueError("invalid DDS dimensions/mips")
    if depth > 1 or struct.unpack_from("<I", head, 112)[0] & (0xFE00 | 0x200000):
        raise ValueError("DDS is not a single 2D texture")
    fourcc = head[84:88] if struct.unpack_from("<I", head, 80)[0] & 4 else b""
    offset = 128
    if fourcc == b"DX10":
        if len(head) < 148:
            raise ValueError("truncated DX10 DDS header")
        _, dimension, misc, array = struct.unpack_from("<IIII", head, 128)
        if dimension != 3 or array != 1 or misc & 4:
            raise ValueError("DX10 DDS is not a single 2D texture")
        offset = 148
    return width, height, mips, fourcc, offset


def canonical_bgra8_source(path: Path, width: int, height: int,
                           mips: int) -> tuple[str, bytes] | None:
    """Expand observed packed DDS layouts and RGB24 into temporary BGRA8."""
    with path.open("rb") as stream:
        header = stream.read(128)
    pf_flags = struct.unpack_from("<I", header, 80)[0]
    bits = struct.unpack_from("<I", header, 88)[0]
    masks = struct.unpack_from("<IIII", header, 92)
    layout = (pf_flags, header[84:88], bits, masks)
    if layout == (0x20000, b"\0" * 4, 8, (0xFF, 0, 0, 0)):
        transform, pixel_bytes = "l8opaque-bgra8-v1", 1
    elif layout == (64, b"\0" * 4, 24, (0xFF0000, 0xFF00, 0xFF, 0)):
        # This codec reads 24-bit payload bytes as RGB despite the BGR DDS masks.
        transform, pixel_bytes = "rgb24-bgra8-v2", 3
    elif layout == (65, b"\0" * 4, 16, (0x7C00, 0x03E0, 0x001F, 0x8000)):
        transform, pixel_bytes = "argb1555-bgra8-v1", 2
    elif layout == (64, b"\0" * 4, 16, (0xF800, 0x07E0, 0x001F, 0)):
        transform, pixel_bytes = "rgb565-bgra8-v1", 2
    elif pf_flags == 0x20000:
        raise ValueError("unsupported luminance DDS layout")
    else:
        return None
    pixels = 0
    w, h = width, height
    for _ in range(mips):
        pixels += w * h
        w, h = max(1, w // 2), max(1, h // 2)
    data = path.read_bytes()
    if len(data) != 128 + pixels * pixel_bytes:
        raise ValueError(f"{transform} requires exact tightly packed mip payload")
    canonical = bytearray(header)
    flags = struct.unpack_from("<I", canonical, 8)[0]
    struct.pack_into("<I", canonical, 8, (flags | 0x8) & ~0x80000)  # pitch, not linear size
    struct.pack_into("<I", canonical, 20, width * 4)
    struct.pack_into("<I", canonical, 80, 0x41)  # RGB | alpha pixels
    struct.pack_into("<I", canonical, 88, 32)
    struct.pack_into("<IIII", canonical, 92, 0x00FF0000, 0x0000FF00,
                     0x000000FF, 0xFF000000)
    if pixel_bytes == 1:
        for value in data[128:]:
            canonical.extend((value, value, value, 255))
    elif pixel_bytes == 3:
        for offset in range(128, len(data), 3):
            canonical.extend(data[offset:offset + 3])
            canonical.append(255)
    else:
        for offset in range(128, len(data), 2):
            packed = struct.unpack_from("<H", data, offset)[0]
            blue = packed & 31
            if transform == "argb1555-bgra8-v1":
                green, red = (packed >> 5) & 31, (packed >> 10) & 31
                alpha = 255 if packed & 0x8000 else 0
                green_max = 31
            else:
                green, red = (packed >> 5) & 63, (packed >> 11) & 31
                alpha = 255
                green_max = 63
            canonical.extend(((blue * 255 + 15) // 31,
                              (green * 255 + green_max // 2) // green_max,
                              (red * 255 + 15) // 31, alpha))
    return transform, bytes(canonical)


def validate_output(path: Path, entry: dict) -> str:
    width, height, mips, fourcc, offset = dds_header(path)
    if (width, height, mips) != (entry["width"], entry["height"], entry["mips"]):
        raise ValueError("output dimensions/mips differ from plan")
    target = entry["target_format"]
    if fourcc == b"DX10":
        with path.open("rb") as stream:
            stream.seek(128)
            dxgi = struct.unpack("<I", stream.read(4))[0]
        if dxgi != {"BC5": 83, "BC7": 98}[target]:
            raise ValueError(f"output DXGI {dxgi} differs from {target}")
    elif not (target == "BC5" and fourcc == b"ATI2"):
        raise ValueError(f"output FourCC {fourcc!r} differs from {target}")
    payload = 0
    for _ in range(mips):
        payload += ((width + 3) // 4) * ((height + 3) // 4) * 16
        width, height = max(1, width // 2), max(1, height // 2)
    if path.stat().st_size != offset + payload:
        raise ValueError("output block payload is truncated or has trailing data")
    return digest(path)


def source_mips(data: bytes, width: int, height: int, count: int) -> list[tuple[int, int, bytes]]:
    """Slice only the seven established FO4 layouts; ignore bytes after known mips."""
    if len(data) < 128 or data[:4] != b"DDS ":
        raise ValueError("invalid source DDS")
    flags, fourcc, bits = struct.unpack_from("<I4sI", data, 80)
    masks = struct.unpack_from("<IIII", data, 92)
    layout = flags, fourcc, bits, masks
    if layout == (4, b"DXT1", 0, (0, 0, 0, 0)):
        block_bytes, pixel_bytes = 8, 0
    elif layout == (4, b"DXT5", 0, (0, 0, 0, 0)):
        block_bytes, pixel_bytes = 16, 0
    elif layout == (64, b"\0" * 4, 24, (0xFF0000, 0xFF00, 0xFF, 0)):
        block_bytes, pixel_bytes = 0, 3
    elif layout == (65, b"\0" * 4, 32, (0xFF0000, 0xFF00, 0xFF, 0xFF000000)):
        block_bytes, pixel_bytes = 0, 4
    else:
        raise ValueError(f"unsupported source DDS pixel layout: {layout!r}")
    result = []
    offset = 128
    for _ in range(count):
        size = (((width + 3) // 4) * ((height + 3) // 4) * block_bytes
                if block_bytes else width * height * pixel_bytes)
        if len(data) < offset + size:
            raise ValueError("source DDS mip payload truncated")
        result.append((width, height, data[offset:offset + size]))
        offset += size
        width, height = max(1, width // 2), max(1, height // 2)
    return result


def rgb24_bc7_needs_padding(width: int, height: int, count: int) -> bool:
    for _ in range(count):
        if width % 4 or height % 4:
            return True
        width, height = max(1, width // 2), max(1, height // 2)
    return False


def pad_bgra8_to_bc7_blocks(width: int, height: int, pixels: bytes) -> tuple[int, int, bytes]:
    """Replicate border pixels in an affected RGB24 mip before BC7 encoding."""
    if len(pixels) != width * height * 4:
        raise ValueError("BGRA8 mip payload size differs from dimensions")
    if width % 4 == 0 and height % 4 == 0:
        return width, height, pixels
    padded_width, padded_height = (width + 3) // 4 * 4, (height + 3) // 4 * 4
    rows = []
    for y in range(padded_height):
        row = pixels[min(y, height - 1) * width * 4:][:width * 4]
        rows.append(row + row[-4:] * (padded_width - width))
    return padded_width, padded_height, b"".join(rows)


def restore_encoded_mip_dimensions(encoded: bytes, width: int, height: int) -> bytes:
    """Restore authored dimensions after the padded codec output was validated."""
    header = bytearray(encoded)
    struct.pack_into("<II", header, 12, height, width)
    struct.pack_into("<I", header, 20, ((width + 3) // 4) * ((height + 3) // 4) * 16)
    return bytes(header)


def single_mip_dds(header: bytes, width: int, height: int, payload: bytes) -> bytes:
    single = bytearray(header[:128])
    compressed = bool(struct.unpack_from("<I", single, 80)[0] & 4)
    struct.pack_into("<I", single, 8, 0x81007 if compressed else 0x100F)
    struct.pack_into("<II", single, 12, height, width)
    struct.pack_into("<I", single, 20, len(payload) if compressed else width * (struct.unpack_from("<I", single, 88)[0] // 8))
    struct.pack_into("<II", single, 24, 0, 1)
    struct.pack_into("<IIIII", single, 108, 0x1000, 0, 0, 0, 0)
    return bytes(single) + payload


def assembled_dds(encoded: list[bytes], entry: dict) -> bytes:
    first = encoded[0]
    width, height, count, fourcc, offset = dds_header_bytes(first)
    if (width, height, count) != (entry["width"], entry["height"], 1):
        raise ValueError("encoded top mip mismatch")
    header = bytearray(first[:offset])
    struct.pack_into("<I", header, 8, 0xA1007 if len(encoded) > 1 else 0x81007)
    struct.pack_into("<I", header, 20,
                     ((entry["width"] + 3) // 4) * ((entry["height"] + 3) // 4) * 16)
    struct.pack_into("<II", header, 24, 0, len(encoded))
    struct.pack_into("<IIIII", header, 108,
                     0x1000 | (0x8 | 0x400000 if len(encoded) > 1 else 0), 0, 0, 0, 0)
    payloads = []
    for chunk in encoded:
        _, _, mip_count, kind, start = dds_header_bytes(chunk)
        if mip_count != 1 or kind != fourcc or start != offset or chunk[128:offset] != first[128:offset]:
            raise ValueError("encoded mip DDS headers differ")
        payloads.append(chunk[start:])
    return bytes(header) + b"".join(payloads)


def dds_header_bytes(data: bytes) -> tuple[int, int, int, bytes, int]:
    """Use the existing on-disk parser for encoded in-memory buffers."""
    # Encoded mip files are short; inspect their header without a second temp file.
    if len(data) < 128 or data[:4] != b"DDS " or struct.unpack_from("<I", data, 4)[0] != 124:
        raise ValueError("invalid encoded DDS header")
    height, width = struct.unpack_from("<II", data, 12)
    mips = struct.unpack_from("<I", data, 28)[0] or 1
    fourcc = data[84:88] if struct.unpack_from("<I", data, 80)[0] & 4 else b""
    offset = 148 if fourcc == b"DX10" else 128
    if len(data) < offset:
        raise ValueError("truncated encoded DDS header")
    return width, height, mips, fourcc, offset


def atomic_json(path: Path, value: object) -> None:
    plain_path(path, allow_missing=True)
    fd, name = tempfile.mkstemp(prefix=".bc-json-", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(value, stream, indent=2, sort_keys=True)
            stream.write("\n")
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)


def read_plan(path: Path) -> dict:
    plain_path(path)
    with path.open(encoding="utf-8") as stream:
        plan = json.load(stream)
    if not isinstance(plan, dict):
        raise ValueError("plan must be an object")
    for key in ("source_root", "output_root", "codec"):
        if not isinstance(plan.get(key), str) or not Path(plan[key]).is_absolute():
            raise ValueError(f"{key} must be an absolute path")
    source, output, codec = (Path(plan[key]) for key in ("source_root", "output_root", "codec"))
    if any(".." in path.parts for path in (source, output, codec)):
        raise ValueError("root and codec paths must not contain '..'")
    if not source.is_dir() or not codec.is_file():
        raise ValueError("source root or codec missing")
    for root in (source, codec):
        plain_path(root)
    plain_path(output, allow_missing=True)
    build = REPO / "build"
    if not inside(output, build) or output == build:
        raise ValueError("output_root must be strictly under repository build/")
    if inside(output, source) or inside(source, output):
        raise ValueError("source/output roots overlap")
    reuse = plan.get("reuse_base_root")
    if reuse is not None:
        if not isinstance(reuse, str) or not Path(reuse).is_absolute():
            raise ValueError("reuse_base_root must be an absolute path")
        reuse_path = Path(reuse)
        if ".." in reuse_path.parts or not inside(reuse_path, build) or reuse_path == build:
            raise ValueError("reuse_base_root must be strictly under repository build/ without '..'")
        if (not reuse_path.is_dir() or inside(reuse_path, output) or inside(output, reuse_path)
                or inside(reuse_path, source) or inside(source, reuse_path)):
            raise ValueError("reuse_base_root missing or overlaps source/output")
        plain_path(reuse_path)
    quality = plan.get("quality")
    if type(quality) not in (int, float) or not math.isfinite(quality) or not 0 < quality <= 1:
        raise ValueError("quality must be finite and greater than 0 through 1")
    timeout = plan.get("timeout_seconds", DEFAULT_TIMEOUT)
    if type(timeout) not in (int, float) or not math.isfinite(timeout) or not 0 < timeout <= 7200:
        raise ValueError("timeout_seconds must be finite and greater than 0 through 7200")
    entries = plan.get("entries")
    if not isinstance(entries, list):
        raise ValueError("entries must be a list")
    seen = set()
    for entry in entries:
        if not isinstance(entry, dict):
            raise ValueError("entry must be an object")
        name = relative_name(entry.get("relative_path"))
        if str(name).casefold() in seen:
            raise ValueError(f"duplicate path (case-insensitive): {name}")
        seen.add(str(name).casefold())
        if entry.get("target_format") not in ("BC5", "BC7"):
            raise ValueError(f"invalid target_format for {name}")
        if not isinstance(entry.get("source_sha256"), str) or not SHA256.fullmatch(entry["source_sha256"]):
            raise ValueError(f"invalid source_sha256 for {name}")
        if not isinstance(entry.get("role"), str) or not entry["role"]:
            raise ValueError(f"missing role for {name}")
        for field in ("width", "height", "mips"):
            if type(entry.get(field)) is not int or entry[field] < 1:
                raise ValueError(f"invalid {field} for {name}")
        if entry["mips"] > full_mips(entry["width"], entry["height"]):
            raise ValueError(f"too many mips for {name}")
        plain_path(source / name)
        plain_path(output / name, allow_missing=True)
    return plan


def run(plan: dict, *, dry_run: bool) -> int:
    source = Path(plan["source_root"])
    output = Path(plan["output_root"])
    codec = Path(plan["codec"])
    codec_hash = digest(codec)
    # AMD ships a shell launcher with a blank line before its shebang.
    # Invoke that script explicitly without shell interpolation.
    codec_command = ["/bin/bash", str(codec)] if codec.read_bytes()[:256].lstrip().startswith(b"#!/bin/bash") else [str(codec)]
    companion = codec.with_name("compressonatorcli-bin")
    codec_binary_hash = digest(companion) if companion.is_file() else None
    quality = str(plan["quality"])
    timeout = plan.get("timeout_seconds", DEFAULT_TIMEOUT)
    reuse_root = Path(plan["reuse_base_root"]) if plan.get("reuse_base_root") else None
    entries = plan["entries"]
    if dry_run:
        print(json.dumps({"valid": True, "entries": len(entries), "output_root": str(output)}))
        return 0
    output.mkdir(parents=True, exist_ok=True)
    metadata = output / ".bc-build"
    plain_path(metadata, allow_missing=True)
    metadata.mkdir(exist_ok=True)
    status = {"state": "running", "total": len(entries), "converted": 0, "resumed": 0,
              "failed": 0, "failures": [], "updated_utc": None}
    status_path = metadata / "status.json"
    events = metadata / "events.jsonl"
    plain_path(events, allow_missing=True)

    def update(event: dict) -> None:
        status["updated_utc"] = datetime.now(timezone.utc).isoformat()
        atomic_json(status_path, status)
        with events.open("a", encoding="utf-8") as stream:
            stream.write(json.dumps(event, sort_keys=True) + "\n")

    update({"event": "start", "total": len(entries)})
    manifest = []
    for entry in entries:
        name = relative_name(entry["relative_path"])
        src = source / name
        dest = output / name
        key = hashlib.sha256(str(name).encode()).hexdigest()
        receipt_path = metadata / f"{key}.json"
        log_path = metadata / f"{key}.log"
        temp = None
        owned_mip_files: list[Path] = []
        try:
            plain_path(src)
            plain_path(dest, allow_missing=True)
            plain_path(receipt_path, allow_missing=True)
            plain_path(log_path, allow_missing=True)
            before = digest(src)
            if before != entry["source_sha256"]:
                raise ValueError("source SHA-256 differs from plan")
            sw, sh, sm, _, _ = dds_header(src)
            if (sw, sh, sm) != (entry["width"], entry["height"], entry["mips"]):
                raise ValueError("source dimensions/mips differ from plan")
            converted_source = canonical_bgra8_source(src, sw, sh, sm)
            transform = converted_source[0] if converted_source is not None else "none"
            # RGB24 has no alpha; opaque BC7 modes keep decoded alpha exactly 255.
            bc7_mode_mask = (15 if transform == "rgb24-bgra8-v2"
                             and entry["target_format"] == "BC7" else None)
            edge_padding = (bc7_mode_mask is not None
                            and rgb24_bc7_needs_padding(sw, sh, sm))
            identity = {"relative_path": str(name), "source_sha256": before,
                        "codec_sha256": codec_hash, "codec_binary_sha256": codec_binary_hash,
                        "target_format": entry["target_format"],
                        "quality": quality, "timeout_seconds": timeout,
                        "mips": entry["mips"], "encode_with": "CPU",
                        "bc7_threads": 2 if entry["target_format"] == "BC7" else None,
                        "source_transform": transform, "mip_policy": MIP_POLICY}
            if bc7_mode_mask is not None:
                identity["bc7_mode_mask"] = bc7_mode_mask
            if edge_padding:
                identity["rgb24_edge_padding"] = "replicate-to-bc7-blocks-v1"
            if dest.exists() and receipt_path.exists():
                with receipt_path.open(encoding="utf-8") as stream:
                    receipt = json.load(stream)
                if all(receipt.get(k) == v for k, v in identity.items()):
                    if validate_output(dest, entry) == receipt.get("output_sha256"):
                        if digest(src) != before:
                            raise ValueError("source changed during validation")
                        status["resumed"] += 1
                        manifest.append(receipt)
                        update({"event": "resumed", "path": str(name)})
                        continue
            if dest.exists():
                raise ValueError("existing output cannot be resumed; refusing overwrite")
            dest.parent.mkdir(parents=True, exist_ok=True)
            plain_path(dest.parent)
            fd, temp_name = tempfile.mkstemp(prefix=".bc-", suffix=".dds", dir=dest.parent)
            os.close(fd)
            temp = Path(temp_name)
            source_data = converted_source[1] if converted_source is not None else src.read_bytes()
            mips = source_mips(source_data, sw, sh, sm)
            log_parts: list[str] = []
            encoded: list[bytes] = []
            base_reused = False
            if reuse_root is not None:
                old_path = reuse_root / name
                old_receipt = reuse_root / ".bc-build" / f"{key}.json"
                try:
                    plain_path(old_path)
                    plain_path(old_receipt)
                    with old_receipt.open(encoding="utf-8") as stream:
                        previous = json.load(stream)
                    old_identity = {k: v for k, v in identity.items() if k != "mip_policy"}
                    if edge_padding and sw % 4 == 0 and sh % 4 == 0:
                        # An aligned top mip was encoded identically by the old v2 path.
                        old_identity.pop("rgb24_edge_padding")
                    if (all(previous.get(k) == v for k, v in old_identity.items())
                            and validate_output(old_path, entry) == previous.get("output_sha256")):
                        previous_data = old_path.read_bytes()
                        _, _, _, _, old_offset = dds_header_bytes(previous_data)
                        top_bytes = ((sw + 3) // 4) * ((sh + 3) // 4) * 16
                        top_header = single_mip_dds(previous_data[:128], sw, sh, b"")[:128]
                        if previous_data[84:88] == b"DX10":
                            top_header += previous_data[128:148]
                        top_header = bytearray(top_header)
                        struct.pack_into("<I", top_header, 20, top_bytes)
                        top = bytes(top_header) + previous_data[old_offset:old_offset + top_bytes]
                        # The legacy full-chain receipt validates the cached top block extent.
                        encoded.append(top)
                        base_reused = True
                        log_parts.append("mip=0 reused validated legacy top blocks\n")
                except (OSError, ValueError, json.JSONDecodeError):
                    pass  # Invalid old cache is read-only; encode the base mip afresh.
            deadline = time.monotonic() + timeout
            for level, (mw, mh, pixels) in enumerate(mips):
                if level == 0 and base_reused:
                    continue
                encode_width, encode_height = mw, mh
                if edge_padding:
                    encode_width, encode_height, pixels = pad_bgra8_to_bc7_blocks(mw, mh, pixels)
                fd, mip_source_name = tempfile.mkstemp(prefix=".bc-source-", suffix=".dds", dir=metadata)
                mip_source = Path(mip_source_name)
                owned_mip_files.append(mip_source)
                with os.fdopen(fd, "wb") as stream:
                    stream.write(single_mip_dds(source_data, encode_width, encode_height, pixels))
                fd, mip_output_name = tempfile.mkstemp(prefix=".bc-mip-", suffix=".dds", dir=metadata)
                os.close(fd)
                mip_output = Path(mip_output_name)
                owned_mip_files.append(mip_output)
                cmd = [*codec_command, "-fd", entry["target_format"], "-EncodeWith", "CPU",
                       "-Quality", quality, "-nomipmap"]
                if entry["target_format"] == "BC7":
                    cmd += ["-NumThreads", "2"]
                    if bc7_mode_mask is not None:
                        cmd += ["-ModeMask", str(bc7_mode_mask)]
                cmd += [str(mip_source), str(mip_output)]
                remaining = deadline - time.monotonic()
                if remaining <= 0:
                    raise ValueError(f"codec exceeded total {timeout}s before mip {level}")
                try:
                    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                               text=True, errors="replace", start_new_session=True)
                    mip_log, _ = process.communicate(timeout=remaining)
                except subprocess.TimeoutExpired as exc:
                    try:
                        os.killpg(process.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                    drained, _ = process.communicate()
                    mip_log = (drained.decode("utf-8", "replace") if isinstance(drained, bytes) else drained) or ""
                    log_parts.append(f"mip={level}\n{mip_log}\nTIMEOUT after total {timeout}s; owned codec process group killed\n")
                    raise ValueError(f"codec exceeded total {timeout}s at mip {level}") from exc
                finally:
                    log_path.write_text("".join(log_parts), encoding="utf-8")
                log_parts.append(f"mip={level}\n{mip_log}\n")
                log_path.write_text("".join(log_parts), encoding="utf-8")
                if process.returncode:
                    raise ValueError(f"codec exit {process.returncode} at mip {level}")
                one = dict(entry, width=encode_width, height=encode_height, mips=1)
                validate_output(mip_output, one)
                mip_bytes = mip_output.read_bytes()
                if (encode_width, encode_height) != (mw, mh):
                    mip_bytes = restore_encoded_mip_dimensions(mip_bytes, mw, mh)
                encoded.append(mip_bytes)
                mip_source.unlink()
                mip_output.unlink()
                owned_mip_files.clear()
            if digest(src) != before:
                raise ValueError("source changed during conversion")
            temp.write_bytes(assembled_dds(encoded, entry))
            output_hash = validate_output(temp, entry)
            os.replace(temp, dest)
            temp = None
            receipt = {**identity, "output_sha256": output_hash, "width": entry["width"],
                       "height": entry["height"], "role": entry["role"],
                       "base_reused": base_reused}
            atomic_json(receipt_path, receipt)
            status["converted"] += 1
            manifest.append(receipt)
            update({"event": "converted", "path": str(name), "output_sha256": output_hash})
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            status["failed"] += 1
            failure = {"path": str(name), "error": str(exc)}
            status["failures"].append(failure)
            update({"event": "failed", **failure})
        finally:
            if temp is not None:
                temp.unlink(missing_ok=True)
            for owned in owned_mip_files:
                owned.unlink(missing_ok=True)
    status["state"] = "failed" if status["failed"] else "success"
    atomic_json(metadata / "manifest.json", {"entries": manifest, "status": status["state"]})
    update({"event": "finish", "state": status["state"]})
    print(json.dumps(status, sort_keys=True))
    return 1 if status["failed"] else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("plan", type=Path, help="explicit JSON conversion plan")
    parser.add_argument("--validate-plan", "--dry-run", action="store_true", dest="dry_run")
    args = parser.parse_args()
    try:
        return run(read_plan(args.plan), dry_run=args.dry_run)
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f"plan error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
