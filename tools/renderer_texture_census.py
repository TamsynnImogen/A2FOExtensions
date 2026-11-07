#!/usr/bin/env python3
"""Read image headers and estimate texture payload sizes without decoding assets.

The byte estimates describe tightly packed texture data implied by headers;
they are not measurements of GPU residency, padding, or runtime format choice.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import struct
from collections import Counter
from pathlib import Path
from typing import Any


EXTENSIONS = {".dds", ".tga", ".png"}
MAX_HEADER = 148
FOURCC = {
    b"DXT1": ("BC1", 8), b"DXT2": ("BC2", 16), b"DXT3": ("BC2", 16),
    b"DXT4": ("BC3", 16), b"DXT5": ("BC3", 16),
    b"ATI1": ("BC4", 8), b"BC4U": ("BC4", 8), b"BC4S": ("BC4", 8),
    b"ATI2": ("BC5", 16), b"BC5U": ("BC5", 16), b"BC5S": ("BC5", 16),
}
DXGI = {
    71: ("BC1", 8), 72: ("BC1", 8), 74: ("BC2", 16), 75: ("BC2", 16),
    77: ("BC3", 16), 78: ("BC3", 16), 80: ("BC4", 8), 81: ("BC4", 8),
    83: ("BC5", 16), 84: ("BC5", 16), 98: ("BC7", 16), 99: ("BC7", 16),
    28: ("RGBA8", 4), 29: ("RGBA8", 4), 87: ("BGRA8", 4), 88: ("BGRA8", 4),
}


def chain_bytes(width: int, height: int, mips: int, block_bytes: int) -> int:
    total = 0
    for _ in range(mips):
        total += ((width + 3) // 4) * ((height + 3) // 4) * block_bytes
        width, height = max(1, width // 2), max(1, height // 2)
    return total


def rgba_bytes(width: int, height: int, mips: int) -> int:
    total = 0
    for _ in range(mips):
        total += width * height * 4
        width, height = max(1, width // 2), max(1, height // 2)
    return total


def pixel_chain_bytes(width: int, height: int, mips: int, bytes_per_pixel: int) -> int:
    total = 0
    for _ in range(mips):
        total += width * height * bytes_per_pixel
        width, height = max(1, width // 2), max(1, height // 2)
    return total


def full_mips(width: int, height: int) -> int:
    count = 1
    while width > 1 or height > 1:
        width, height = max(1, width // 2), max(1, height // 2)
        count += 1
    return count


def candidate_name(path: Path) -> bool:
    tokens = set(filter(None, re.split(r"[^a-z0-9]+", path.stem.lower())))
    return bool(tokens & {"normal", "normalmap", "nrm", "nm", "bump", "bumpmap", "n", "b"})


def record(path: Path, root: Path, size: int) -> dict[str, Any]:
    row: dict[str, Any] = {
        "path": str(path), "root": str(root), "file_bytes": size,
        "extension": path.suffix.lower(), "status": "invalid", "format": "unknown",
        "width": None, "height": None, "mips": None, "topology": "unknown",
        "payload_estimates": None, "normal_bump_name_candidate": candidate_name(path),
    }
    try:
        with path.open("rb") as stream:
            data = stream.read(MAX_HEADER)
    except OSError as exc:
        row.update(status="unreadable", error=str(exc))
        return row

    w = h = mips = None
    fmt = "unknown"
    source_block = None
    topology = "2d_single_image"
    if path.suffix.lower() == ".dds":
        if len(data) < 128 or data[:4] != b"DDS " or struct.unpack_from("<I", data, 4)[0] != 124:
            row["error"] = "truncated or invalid DDS base header"
            return row
        h, w = struct.unpack_from("<II", data, 12)
        depth, declared_mips = struct.unpack_from("<II", data, 24)
        # The count is useful header metadata even when DDSD_MIPMAPCOUNT is absent.
        mips = declared_mips or 1
        pf_size, pf_flags = struct.unpack_from("<II", data, 76)
        if pf_size != 32:
            row["error"] = "invalid DDS pixel format header size"
            return row
        if pf_flags & 0x4:
            code = data[84:88]
            if code == b"DX10":
                if len(data) < 148 or size < 148:
                    row["error"] = "truncated DDS DX10 header"
                    return row
                dxgi, resource_dimension, misc_flag, array_size = struct.unpack_from("<IIII", data, 128)
                fmt, source_block = DXGI.get(dxgi, (f"DXGI_{dxgi}", None))
                if resource_dimension != 3 or array_size != 1:
                    topology = "non_2d_or_array"
                if misc_flag & 0x4:
                    topology = "cube_or_volume"
            else:
                fmt, source_block = FOURCC.get(code, ("unknown_fourcc_" + code.decode("ascii", "replace"), None))
        elif pf_flags & 0x40:
            bits = struct.unpack_from("<I", data, 88)[0]
            masks = struct.unpack_from("<IIII", data, 92)
            if bits == 32:
                red, green, blue, alpha = masks
                if (red, green, blue, alpha) == (0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000):
                    fmt, source_block = "BGRA8", 4
                elif (red, green, blue, alpha) == (0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000):
                    fmt, source_block = "RGBA8", 4
                else:
                    fmt, source_block = "DDS_RGB32", 4
            else:
                fmt = f"DDS_RGB{bits}"
                source_block = (bits + 7) // 8 if bits in (8, 16, 24) else None
        caps2 = struct.unpack_from("<I", data, 112)[0]
        if caps2 & (0xFE00 | 0x200000):
            topology = "cube_or_volume"
        if depth > 1:
            topology = "cube_or_volume"
    elif path.suffix.lower() == ".tga":
        if len(data) < 18:
            row["error"] = "truncated TGA header"
            return row
        w, h = struct.unpack_from("<HH", data, 12)
        bits = data[16]
        fmt = f"unknown_TGA_{bits}bit"
        mips = 1
    else:
        png_signature = b"\x89PNG\r\n\x1a\n"
        if len(data) < 33 or data[:8] != png_signature or data[12:16] != b"IHDR":
            row["error"] = "truncated or invalid PNG IHDR"
            return row
        w, h = struct.unpack_from(">II", data, 16)
        fmt = "unknown_PNG"
        mips = 1

    if not w or not h or not mips or w > 131072 or h > 131072 or mips > full_mips(w, h):
        row.update(width=w, height=h, mips=mips, format=fmt, topology=topology,
                   error="invalid or implausible dimensions/mip count")
        return row
    if path.suffix.lower() == ".dds" and topology != "2d_single_image":
        row.update(status="valid_header_unestimated", width=w, height=h, mips=mips,
                   format=fmt, topology=topology)
        return row

    row.update(status="valid_header", width=w, height=h, mips=mips,
               format=fmt, topology=topology)
    estimate = {
        "basis": "theoretical tightly packed encoded payload; not actual VRAM/residency",
        "source_format_payload_bytes": (
            chain_bytes(w, h, mips, source_block) if fmt.startswith("BC") and source_block
            else pixel_chain_bytes(w, h, mips, source_block) if source_block else None
        ),
        "rgba8_reference_bytes": rgba_bytes(w, h, mips),
        "hypothetical_bc7_bytes_from_rgba8": chain_bytes(w, h, mips, 16),
        "hypothetical_bc5_bytes_from_rgba8": chain_bytes(w, h, mips, 16),
    }
    if path.suffix.lower() != ".dds":
        estimate["source_format_payload_bytes"] = None
    row["payload_estimates"] = estimate
    return row


def scan(root: Path) -> tuple[list[dict[str, Any]], list[str]]:
    found: list[dict[str, Any]] = []
    errors: list[str] = []
    if not root.is_dir():
        return found, [f"missing_or_not_directory: {root}"]
    stack = [root]
    while stack:
        current = stack.pop()
        try:
            with os.scandir(current) as entries:
                for entry in entries:
                    try:
                        if entry.is_symlink():
                            continue
                        if entry.is_dir(follow_symlinks=False):
                            stack.append(Path(entry.path))
                        elif entry.is_file(follow_symlinks=False) and Path(entry.name).suffix.lower() in EXTENSIONS:
                            path = Path(entry.path)
                            try:
                                size = entry.stat(follow_symlinks=False).st_size
                            except OSError as exc:
                                found.append({"path": str(path), "root": str(root), "status": "unreadable",
                                              "format": "unknown", "file_bytes": None, "error": str(exc)})
                                continue
                            found.append(record(path, root, size))
                    except OSError as exc:
                        errors.append(f"{entry.path}: {exc}")
        except OSError as exc:
            errors.append(f"{current}: {exc}")
    return found, errors


def summarize(rows: list[dict[str, Any]]) -> dict[str, Any]:
    counts = Counter(row.get("format", "unknown") for row in rows)
    format_bytes: Counter[str] = Counter()
    rgba_reference_bytes: Counter[str] = Counter()
    for row in rows:
        estimate = row.get("payload_estimates") or {}
        payload = estimate.get("source_format_payload_bytes")
        if payload is not None:
            format_bytes[row["format"]] += payload
        reference = estimate.get("rgba8_reference_bytes")
        if reference is not None and row.get("format"):
            rgba_reference_bytes[row["format"]] += reference
    valid = [row for row in rows if row.get("status") == "valid_header" and row.get("payload_estimates")]
    biggest = sorted(valid, key=lambda row: row["payload_estimates"]["rgba8_reference_bytes"], reverse=True)
    rgba_dds = [row for row in valid if row.get("extension") == ".dds" and row.get("format") in ("RGBA8", "BGRA8")]
    candidates = [row for row in valid if row.get("normal_bump_name_candidate")]
    rgba_dds_source_bytes = sum(row["payload_estimates"]["source_format_payload_bytes"] or 0 for row in rgba_dds)
    bc_bytes = sum(row["payload_estimates"]["hypothetical_bc7_bytes_from_rgba8"] for row in rgba_dds)
    return {
        "physical_file_counts_by_format": dict(sorted(counts.items())),
        "theoretical_source_payload_bytes_by_format": dict(sorted(format_bytes.items())),
        "rgba8_reference_payload_bytes_by_format": dict(sorted(rgba_reference_bytes.items())),
        "valid_2d_single_image_count": len(valid),
        "name_candidates_count_only_heuristic": len(candidates),
        "largest_rgba8_reference_examples": [
            {"path": row["path"], "format": row["format"], "dimensions": [row["width"], row["height"]],
             "rgba8_reference_bytes": row["payload_estimates"]["rgba8_reference_bytes"]}
            for row in biggest[:20]
        ],
        "largest_name_candidate_examples": [
            {"path": row["path"], "format": row["format"], "dimensions": [row["width"], row["height"]],
             "rgba8_reference_bytes": row["payload_estimates"]["rgba8_reference_bytes"]}
            for row in sorted(candidates, key=lambda row: row["payload_estimates"]["rgba8_reference_bytes"], reverse=True)[:20]
        ],
        "largest_rgba_bgra_dds_hypothetical_bc7_bc5_review_examples": [
            {"path": row["path"], "format": row["format"], "dimensions": [row["width"], row["height"]],
             "current_header_encoded_payload_bytes": row["payload_estimates"]["source_format_payload_bytes"],
             "hypothetical_bc7_bytes": row["payload_estimates"]["hypothetical_bc7_bytes_from_rgba8"],
             "hypothetical_bc5_bytes": row["payload_estimates"]["hypothetical_bc5_bytes_from_rgba8"],
             "note": "header-only review candidate; semantic suitability and visual quality are unknown"}
            for row in sorted(rgba_dds, key=lambda row: row["payload_estimates"]["rgba8_reference_bytes"], reverse=True)[:20]
        ],
        "rgba_bgra_dds_hypothetical_bc7_bc5_totals": {
            "file_count": len(rgba_dds),
            "current_header_encoded_payload_bytes": rgba_dds_source_bytes,
            "hypothetical_bc7_payload_bytes": bc_bytes,
            "hypothetical_bc5_payload_bytes": bc_bytes,
            "hypothetical_payload_reduction_bytes": rgba_dds_source_bytes - bc_bytes,
            "note": "estimate only; no claim that any texture is suitable for BC7 or BC5",
        },
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("roots", nargs="+", type=Path, help="texture directories to scan")
    parser.add_argument("--output", required=True, type=Path, help="JSON output path")
    args = parser.parse_args()
    rows: list[dict[str, Any]] = []
    errors: list[str] = []
    for root_arg in args.roots:
        root = root_arg.resolve(strict=False)
        scanned, scan_errors = scan(root)
        rows.extend(scanned)
        errors.extend(scan_errors)
    artifact = {
        "schema_version": 1,
        "scope": "loose DDS/TGA/PNG files under supplied roots; symlink directories/files skipped",
        "archives": {"inventoried": False, "note": "FPQ and other archives are excluded; no archive extraction performed"},
        "duplicate_policy": "physical files counted; no live asset/cache resolution or deduplication",
        "estimate_caveat": "Theoretical payload estimates are header-derived, tightly packed sizes, not actual runtime GPU format, VRAM, residency, row alignment, or driver allocation.",
        "roots": [str(path.resolve(strict=False)) for path in args.roots],
        "physical_file_count": len(rows),
        "summary": summarize(rows),
        "scan_errors": errors,
        "files": rows,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(artifact, indent=2) + "\n", encoding="utf-8")
    print(f"{len(rows)} physical texture files -> {args.output}; {len(errors)} scan errors")


if __name__ == "__main__":
    main()
