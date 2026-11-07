#!/usr/bin/env python3
"""Inspect PE32 engine images and propose audited compatibility profiles."""

import argparse
import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path


FIXED_ROOTS = {
    "machine": 0x014C,
    "optional_magic": 0x010B,
    "image_base": 0x00400000,
    "entry_point": 0x002733C0,
    "image_size": 0x00405000,
    "optional_header_size": 0x00E0,
    "section_alignment": 0x00001000,
    "sections": (
        (".text", 0x001000, 0x2AC6C4, 0x001000, 0x2AD000),
        (".rdata", 0x2AE000, 0x03DE90, 0x2AE000, 0x03E000),
        (".data", 0x2EC000, 0x0CA1B8, 0x2EC000, 0x049000),
        (".idata", 0x3B7000, 0x0055FA, 0x335000, 0x006000),
        (".rsrc", 0x3BD000, 0x006BB0, 0x33B000, 0x007000),
        (".reloc", 0x3C4000, 0x036000, 0x342000, 0x036000),
        (".test", 0x3FA000, 0x00A999, 0x378000, 0x00A999),
    ),
    "section_characteristics": (
        0x60000020, 0x40000040, 0xC0000040, 0xC0000040,
        0x40000040, 0x42000040, 0x60000020,
    ),
}
FIXED_FLEETOPS = {
    "machine": 0x014C,
    "optional_magic": 0x010B,
    "image_base": 0x5A800000,
    "entry_point": 0x00201988,
    "image_size": 0x00322000,
    "file_size": 3043840,
    "file_characteristics": 0xA3AE,
    "optional_header_size": 0x00E0,
    "section_alignment": 0x00001000,
    "sections": (
        (".text", 0x001000, 0x1FD054, 0x000400, 0x1FD200),
        (".itext", 0x1FF000, 0x002D84, 0x1FD600, 0x002E00),
        (".data", 0x202000, 0x011288, 0x200400, 0x011400),
        (".bss", 0x214000, 0x036420, 0x211800, 0x000000),
        (".idata", 0x24B000, 0x005000, 0x211800, 0x005000),
        (".edata", 0x250000, 0x000053, 0x216800, 0x000200),
        (".reloc", 0x251000, 0x020F30, 0x216A00, 0x021000),
        (".rsrc", 0x272000, 0x0AF648, 0x237A00, 0x0AF800),
    ),
    "section_characteristics": (
        0x60000020, 0x60000020, 0xC0000040, 0xC0000000,
        0xC0000040, 0x40000040, 0x42000040, 0x40000040,
    ),
}
PROFILE_SECTIONS = (
    (".text", "TEXTRAWCRC"),
    (".rdata", "RDATARAWCRC"),
    (".data", "DATARAWCRC"),
    (".idata", "IDATARAWCRC"),
    (".rsrc", "RSRCRAWCRC"),
    (".reloc", "RELOCRAWCRC"),
    (".test", "TESTRAWCRC8"),
)


class PEError(ValueError):
    """The input is not a safely readable PE image."""


def _unpack(fmt, data, offset, label):
    size = struct.calcsize(fmt)
    if offset < 0 or offset + size > len(data):
        raise PEError(f"truncated {label}")
    return struct.unpack_from(fmt, data, offset)


def parse_image(data):
    """Parse a PE32 image and compute file and section CRC32 values."""
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise PEError("invalid or truncated DOS header")
    (pe_offset,) = _unpack("<I", data, 0x3C, "DOS e_lfanew")
    if pe_offset + 24 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise PEError("invalid or truncated PE signature/file header")
    machine, section_count, timestamp, _, _, optional_size, file_characteristics = _unpack(
        "<HHIIIHH", data, pe_offset + 4, "COFF header")
    optional = pe_offset + 24
    if optional + optional_size > len(data):
        raise PEError("truncated optional header")
    if optional_size < 60:
        raise PEError("optional header too short for PE32 fields")
    (magic,) = _unpack("<H", data, optional, "optional-header magic")
    if magic != 0x10B:
        raise PEError(f"unsupported optional-header magic 0x{magic:04x}; expected PE32")
    entry_point = _unpack("<I", data, optional + 16, "entry point")[0]
    section_alignment = _unpack("<I", data, optional + 32, "section alignment")[0]
    image_base = _unpack("<I", data, optional + 28, "image base")[0]
    image_size = _unpack("<I", data, optional + 56, "image size")[0]
    table = optional + optional_size
    if table + section_count * 40 > len(data):
        raise PEError("truncated section table")
    sections = []
    for index in range(section_count):
        pos = table + index * 40
        name_raw = data[pos:pos + 8].split(b"\0", 1)[0]
        try:
            name = name_raw.decode("ascii")
        except UnicodeDecodeError as error:
            raise PEError(f"section {index} has a non-ASCII name") from error
        virtual_size, virtual_address, raw_size, raw_offset = _unpack(
            "<IIII", data, pos + 8, f"section {index} geometry")
        section_characteristics = _unpack("<I", data, pos + 36, f"section {index} characteristics")[0]
        if raw_size and (raw_offset > len(data) or raw_size > len(data) - raw_offset):
            raise PEError(f"section {name!r} raw span exceeds file bounds")
        raw = data[raw_offset:raw_offset + raw_size]
        sections.append({
            "name": name,
            "virtual_address": virtual_address,
            "virtual_size": virtual_size,
            "raw_offset": raw_offset,
            "raw_size": raw_size,
            "characteristics": section_characteristics,
            "raw_crc32": f"{zlib.crc32(raw) & 0xFFFFFFFF:08X}",
            "virtual_crc32": (f"{zlib.crc32(raw[:virtual_size]) & 0xFFFFFFFF:08X}"
                              if virtual_size <= raw_size else None),
        })
    return {
        "machine": machine,
        "section_count": section_count,
        "timestamp": timestamp,
        "file_characteristics": file_characteristics,
        "optional_header_size": optional_size,
        "optional_magic": magic,
        "entry_point": entry_point,
        "image_base": image_base,
        "image_size": image_size,
        "section_alignment": section_alignment,
        "file_size": len(data),
        "file_crc32": f"{zlib.crc32(data) & 0xFFFFFFFF:08X}",
        "header_crc32": f"{zlib.crc32(data[:0x1000]) & 0xFFFFFFFF:08X}" if len(data) >= 0x1000 else None,
        "sha256": hashlib.sha256(data).hexdigest(),
        "sections": sections,
    }


def inspect(path):
    data = Path(path).read_bytes()
    return data, parse_image(data)


def _geometry(info):
    return [(s["name"], s["virtual_address"], s["virtual_size"], s["raw_offset"], s["raw_size"])
            for s in info["sections"]]


def _rva_for_offset(offset, sections):
    for section in sections:
        start = section["raw_offset"]
        if start <= offset < start + section["raw_size"]:
            return section["virtual_address"] + offset - start
    return None


def compare(old_data, old, new_data, new):
    fields = ("machine", "section_count", "timestamp", "optional_magic", "entry_point",
              "image_base", "image_size", "file_size")
    header_diffs = {key: {"old": old[key], "new": new[key]}
                    for key in fields if old[key] != new[key]}
    old_sections = {s["name"]: s for s in old["sections"]}
    new_sections = {s["name"]: s for s in new["sections"]}
    layout_diffs = {}
    for name in sorted(set(old_sections) | set(new_sections)):
        before, after = old_sections.get(name), new_sections.get(name)
        if before is None or after is None:
            layout_diffs[name] = {"old": before, "new": after}
            continue
        keys = ("virtual_address", "virtual_size", "raw_offset", "raw_size", "characteristics", "raw_crc32")
        delta = {key: {"old": before[key], "new": after[key]}
                 for key in keys if before[key] != after[key]}
        if delta:
            layout_diffs[name] = delta

    spans = []
    differing = 0
    common = min(len(old_data), len(new_data))
    i = 0
    while i < common:
        if old_data[i] == new_data[i]:
            i += 1
            continue
        start = i
        while i < common and old_data[i] != new_data[i]:
            i += 1
        length = i - start
        differing += length
        spans.append({"file_offset": start, "length": length,
                      "rva": _rva_for_offset(start, new["sections"])})
    if len(old_data) != len(new_data):
        start = common
        length = abs(len(old_data) - len(new_data))
        differing += length
        spans.append({"file_offset": start, "length": length,
                      "rva": _rva_for_offset(start, new["sections"]) if start < len(new_data) else None})
    return {
        "old_sha256": old["sha256"],
        "new_sha256": new["sha256"],
        "header_differences": header_diffs,
        "section_layout_differences": layout_diffs,
        "byte_differences": {"differing_bytes": differing, "group_count": len(spans), "groups": spans},
    }


def make_profile(info):
    expected_sections = [tuple(section) for section in FIXED_ROOTS["sections"]]
    checks = ("machine", "optional_magic", "image_base", "entry_point", "image_size",
              "optional_header_size", "section_alignment")
    for field in checks:
        if info[field] != FIXED_ROOTS[field]:
            raise PEError(f"not the fixed Roots image: {field} is 0x{info[field]:X}")
    if len(info["sections"]) != 7 or _geometry(info) != expected_sections:
        raise PEError("not the fixed Roots image: section geometry differs")
    if tuple(section["characteristics"] for section in info["sections"]) != FIXED_ROOTS["section_characteristics"]:
        raise PEError("not the fixed Roots image: section characteristics differ")
    if info["header_crc32"] is None:
        raise PEError("not the fixed Roots image: file is shorter than the 0x1000-byte header span")
    if len(info["sections"]) != 7 or info["sections"][-1]["name"] != ".test" or info["sections"][-1]["raw_size"] != 0xA999:
        raise PEError("not the fixed Roots image: expected seven sections and .test raw size 0xA999")
    by_name = {section["name"]: section for section in info["sections"]}
    values = [f"{info['timestamp']:08X}", f"{info['image_size']:08X}", info["header_crc32"]]
    values.extend(by_name[name]["raw_crc32"] for name, _ in PROFILE_SECTIONS)
    return ("# SHA-256: " + info["sha256"] + "\n"
            "# .rdata virtual CRC32: " + by_name[".rdata"]["virtual_crc32"] + "\n"
            "# Proposed profile; requires manual binary/hook audit before installation.\n"
            "A2FO_ARMADA_PROFILES_V1\n"
            "roots_native_rotation " + " ".join(values) + "\n")


def make_fleetops_profile(info):
    """Propose a row for the exact Oct 23 FleetOps layout, never an audit approval."""
    checks = ("machine", "optional_magic", "image_base", "entry_point", "image_size",
              "file_size", "file_characteristics", "optional_header_size", "section_alignment")
    for field in checks:
        if info[field] != FIXED_FLEETOPS[field]:
            raise PEError(f"not the fixed FleetOps image: {field} is 0x{info[field]:X}")
    if len(info["sections"]) != 8 or _geometry(info) != list(FIXED_FLEETOPS["sections"]):
        raise PEError("not the fixed FleetOps image: section geometry differs")
    if tuple(section["characteristics"] for section in info["sections"]) != FIXED_FLEETOPS["section_characteristics"]:
        raise PEError("not the fixed FleetOps image: section characteristics differ")
    values = (info["timestamp"], info["image_size"], info["file_size"])
    return ("# SHA-256: " + info["sha256"] + "\n"
            "# Proposed profile; requires manual binary/hook audit before installation.\n"
            "A2FO_FLEETOPS_PROFILES_V1\n"
            "roots_fleetops " + " ".join(f"{value:08X}" for value in values) +
            " " + info["file_crc32"] + "\n")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    inspect_parser = sub.add_parser("inspect", help="inspect one PE image as JSON")
    inspect_parser.add_argument("image", type=Path)
    compare_parser = sub.add_parser("compare", help="compare two PE images as JSON")
    compare_parser.add_argument("old", type=Path)
    compare_parser.add_argument("new", type=Path)
    profile_parser = sub.add_parser("profile", help="write a proposed fixed Roots profile")
    profile_parser.add_argument("image", type=Path)
    profile_parser.add_argument("--output", required=True, type=Path)
    fleetops_parser = sub.add_parser("fleetops-profile", help="write a proposed fixed FleetOps profile")
    fleetops_parser.add_argument("image", type=Path)
    fleetops_parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "inspect":
            _, result = inspect(args.image)
            print(json.dumps(result, indent=2, sort_keys=True))
        elif args.command == "compare":
            old_data, old = inspect(args.old)
            new_data, new = inspect(args.new)
            print(json.dumps(compare(old_data, old, new_data, new), indent=2, sort_keys=True))
        elif args.command == "profile":
            _, info = inspect(args.image)
            args.output.write_text(make_profile(info), encoding="ascii")
        else:
            _, info = inspect(args.image)
            args.output.write_text(make_fleetops_profile(info), encoding="ascii")
    except (OSError, PEError) as error:
        parser.exit(1, f"armada compatibility profile: {error}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
