#!/usr/bin/env python3
"""Prepare private DX11 BC transport files from an explicit child mod root.

Catalog v2 is ASCII, LF separated: A2FO_BC_CATALOG 2, COUNT N, then
basename|width|height|mips|DXGI(83/98)|role(raw/normal_xy)|top FNV64|file FNV64|mip FNV64s.
The last field has one comma-separated hash per supplied mip, top first; the
top hash is retained for compatibility with the existing metadata layout.
Hashes are lowercase sixteen-digit hexadecimal over exact bytes. Transport
DDS files are parser decoys only; their DXT4 header must never reach D3D8.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import stat
import struct

EXPECTED_COUNT = 407
NAME = re.compile(r"[A-Za-z0-9_. -]+\.dds\Z", re.IGNORECASE)
SHA = re.compile(r"[0-9a-f]{64}\Z")


def fnv(data):
    value = 0xcbf29ce484222325
    for byte in data:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    return f"{value:016x}"


def safe(path, missing=False):
    for part in (path, *path.parents):
        try:
            mode = part.lstat().st_mode
        except FileNotFoundError:
            if missing:
                continue
            raise ValueError(f"missing path: {part}") from None
        if stat.S_ISLNK(mode):
            raise ValueError(f"symlink path: {part}")
    if path.exists() and not (path.is_file() or path.is_dir()):
        raise ValueError(f"unsupported path: {path}")


def record(entry, root):
    raw = entry.get("relative_path")
    if not isinstance(raw, str) or not raw.startswith("Textures/"):
        raise ValueError(f"unsafe DDS path: {raw!r}")
    name = raw[len("Textures/"):]
    if not NAME.fullmatch(name) or name in (".", ".."):
        raise ValueError(f"unsafe DDS basename: {name!r}")
    path = root / "Textures" / name
    safe(path)
    data = path.read_bytes()
    expected = entry.get("output_sha256")
    if not isinstance(expected, str) or not SHA.fullmatch(expected) or hashlib.sha256(data).hexdigest() != expected:
        raise ValueError(f"canonical SHA256 mismatch: {name}")
    if len(data) != entry.get("output_bytes") or len(data) < 128 or data[:4] != b"DDS ":
        raise ValueError(f"canonical DDS size/header mismatch: {name}")
    if struct.unpack_from("<II", data, 4)[0] != 124 or struct.unpack_from("<I", data, 76)[0] != 32:
        raise ValueError(f"invalid DDS header: {name}")
    width, height, mips = entry.get("width"), entry.get("height"), entry.get("mips")
    if not all(type(x) is int and x > 0 for x in (width, height, mips)) or (height, width) != struct.unpack_from("<II", data, 12) or mips != struct.unpack_from("<I", data, 28)[0]:
        raise ValueError(f"DDS dimensions/mips mismatch: {name}")
    if width > 16384 or height > 16384 or mips > max(width, height).bit_length():
        raise ValueError(f"unsupported dimensions/mips: {name}")
    if struct.unpack_from("<I", data, 8)[0] & 0x800000 or struct.unpack_from("<I", data, 24)[0] > 1 or struct.unpack_from("<I", data, 112)[0] & (0xfe00 | 0x200000):
        raise ValueError(f"volume/cube DDS: {name}")
    if struct.unpack_from("<I", data, 80)[0] != 4:
        raise ValueError(f"unsupported pixel flags: {name}")
    kind = entry.get("target_format")
    role = entry.get("role")
    if (kind, role) not in (("BC5", "tangent_space_normal_xy_reconstruct_z"), ("BC7", "colour_alpha_or_scalar_preserve_rgba")):
        raise ValueError(f"unexpected format/role: {name}")
    fourcc = data[84:88]
    offset = 148 if fourcc == b"DX10" else 128
    if fourcc == b"DX10":
        if len(data) < 148 or struct.unpack_from("<IIII", data, 128)[:4] != ({"BC5": 83, "BC7": 98}[kind], 3, 0, 1):
            raise ValueError(f"unexpected DX10 header: {name}")
    elif not (kind == "BC5" and fourcc == b"ATI2"):
        raise ValueError(f"unexpected FourCC: {name}")
    payload = data[offset:]
    sizes = []
    w, h = width, height
    for _ in range(mips):
        sizes.append(((w + 3) // 4) * ((h + 3) // 4) * 16)
        w, h = max(1, w // 2), max(1, h // 2)
    if sum(sizes) != len(payload):
        raise ValueError(f"nonexact compressed payload: {name}")
    header = bytearray(data[:128])
    header[84:88] = b"DXT4"
    struct.pack_into("<I", header, 8, struct.unpack_from("<I", header, 8)[0] & ~0x800000)
    struct.pack_into("<I", header, 24, 0)
    transport = bytes(header) + payload
    full_hash = fnv(data)
    mip_hashes = []
    at = 0
    for size in sizes:
        mip_hashes.append(fnv(payload[at:at + size]))
        at += size
    line = f"{name}|{width}|{height}|{mips}|{83 if kind == 'BC5' else 98}|{'normal_xy' if kind == 'BC5' else 'raw'}|{mip_hashes[0]}|{full_hash}|{','.join(mip_hashes)}"
    return line, transport, (width, height, mips, payload[:sizes[0]], kind, role, full_hash)


def prepare(root, expected_count=EXPECTED_COUNT):
    root = Path(root).absolute()
    safe(root)
    if not root.is_dir():
        raise ValueError("--mod-root must be an existing child mod directory")
    manifest_path = root / "TextureConversionManifest.json"
    safe(manifest_path)
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    entries = manifest.get("entries")
    if manifest.get("status") != "success" or not isinstance(entries, list) or len(entries) != expected_count:
        raise ValueError("manifest status/count mismatch")
    if expected_count == EXPECTED_COUNT and manifest.get("counts") != {"BC5": 180, "BC7": 227}:
        raise ValueError("manifest format counts mismatch")
    names = set()
    matches = {}
    rows = []
    for entry in entries:
        line, transport, key = record(entry, root)
        name = line.split("|", 1)[0]
        if name.casefold() in names:
            raise ValueError(f"duplicate case-insensitive basename: {name}")
        names.add(name.casefold())
        identity = key[:4]
        prior = matches.get(identity)
        if prior and prior != key[4:]:
            raise ValueError(f"ambiguous top mip: {name}")
        matches[identity] = key[4:]
        rows.append((name, line, transport))
    if expected_count == EXPECTED_COUNT and (sum("|83|" in line for _, line, _ in rows) != 180 or sum("|98|" in line for _, line, _ in rows) != 227):
        raise ValueError("actual format counts mismatch")
    rows.sort(key=lambda item: item[0].casefold())
    catalog = (f"A2FO_BC_CATALOG 2\nCOUNT {len(rows)}\n" + "\n".join(row[1] for row in rows) + "\n").encode("ascii")
    base = root / ".a2fo-dx11"
    tex = base / "Textures"
    # Validate all preexisting output before creating anything.
    for path, expected in [(base / "catalog.txt", catalog), *[(tex / name, data) for name, _, data in rows]]:
        safe(path, missing=True)
        if path.exists() and (not path.is_file() or path.read_bytes() != expected):
            raise ValueError(f"existing transport differs: {path}")
    base.mkdir(exist_ok=True)
    tex.mkdir(exist_ok=True)
    for name, _, data in rows:
        path = tex / name
        if not path.exists():
            with path.open("xb") as stream:
                stream.write(data)
    path = base / "catalog.txt"
    if not path.exists():
        with path.open("xb") as stream:
            stream.write(catalog)
    return len(rows)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mod-root", required=True, type=Path)
    args = parser.parse_args()
    print(f"prepared {prepare(args.mod_root)} private BC transports")
