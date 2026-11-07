#!/usr/bin/env python3
"""Verify the narrow FO factory/mode boundary offline; never execute/patch an image."""
import argparse
import hashlib
import json
from pathlib import Path
import struct

PROFILES = {
    "4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de":
        ("fo-roots", 0x3C4C76BD, 0x405000, 0xCA07C55A),
    "c8846226048931134e07d48702309b2fec1ef0f67f5b4d82bec0eb5249257d8c":
        ("fo4-mod", 0x6A7D02B1, 0x404000, 0x3CD46234),
}
FINDER_RVA = 0x226BF0
CALL_RVAS = (0x22C4E0, 0x22C643, 0x13E7C9, 0x13E7E7, 0x861DA, 0x861F2)
FINDER = bytes.fromhex("55 8b ec 8b 51 14 53 56 57 8b 0a 3b ca 74 20 8b 7d 10 8b 5d 0c 8b 75 08 8b 41 08 39 30 7c 0a 39 58 04 7c 05 39 78 08 74 08 8b 09 3b ca 75 e9 33 c0 5f 5e 5b 5d c2 0c 00")

class PE:
    def __init__(self, data):
        self.data = data
        if data[:2] != b"MZ":
            raise ValueError("not MZ")
        nt = self.u32(0x3C)
        if self.raw(nt, 4) != b"PE\0\0" or self.u16(nt + 4) != 0x14C:
            raise ValueError("not i386 PE")
        self.opt = nt + 24
        opt_size = self.u16(nt + 20)
        if self.u16(self.opt) != 0x10B or opt_size < 224:
            raise ValueError("not complete PE32")
        self.raw(self.opt, opt_size)
        self.base = self.u32(self.opt + 28)
        self.timestamp = self.u32(nt + 8)
        self.image_size = self.u32(self.opt + 56)
        count = self.u16(nt + 6)
        if not 1 <= count <= 96:
            raise ValueError("section count")
        self.sections = []
        value = 0x811C9DC5
        for i in range(count):
            s = self.raw(self.opt + opt_size + i * 40, 40)
            virtual_size, rva, raw_size, offset = struct.unpack_from("<IIII", s, 8)
            for b in s[:8] + struct.pack("<III", rva, virtual_size, raw_size):
                value = ((value ^ b) * 0x01000193) & 0xFFFFFFFF
            self.sections.append((rva, raw_size, offset))
        self.section_fnv = value

    def raw(self, offset, count):
        if offset < 0 or count < 0 or offset + count > len(self.data):
            raise ValueError("file span")
        return self.data[offset:offset + count]

    def u16(self, at):
        return struct.unpack("<H", self.raw(at, 2))[0]

    def u32(self, at):
        return struct.unpack("<I", self.raw(at, 4))[0]

    def read(self, rva, count):
        for start, size, offset in self.sections:
            if start <= rva and rva + count <= start + size:
                return self.raw(offset + rva - start, count)
        raise ValueError("RVA not file backed")

    def string(self, rva):
        result = bytearray()
        for i in range(260):
            b = self.read(rva + i, 1)[0]
            if not b:
                return result.decode("ascii")
            result.append(b)
        raise ValueError("unterminated import name")

    def factory_iat(self):
        rva, size = struct.unpack("<II", self.raw(self.opt + 96 + 8, 8))
        found = []
        for at in range(0, min(size, 4096 * 20), 20):
            desc = self.read(rva + at, 20)
            if desc == bytes(20):
                break
            original, _, _, dll, iat = struct.unpack("<IIIII", desc)
            if self.string(dll).lower() != "d3d8.dll":
                continue
            if not original:
                raise ValueError("missing import names")
            for i in range(4096):
                thunk = struct.unpack("<I", self.read(original + i * 4, 4))[0]
                if not thunk:
                    break
                if not thunk & 0x80000000 and self.string(thunk + 2) == "Direct3DCreate8":
                    found.append(iat + i * 4)
        if len(found) != 1:
            raise ValueError("factory import count")
        return found[0]

def inspect(path):
    data = path.read_bytes()
    sha = hashlib.sha256(data).hexdigest()
    profile = PROFILES.get(sha)
    if profile is None:
        raise ValueError("unreviewed executable SHA-256")
    pe = PE(data)
    key, timestamp, size, fnv = profile
    if (pe.base, pe.timestamp, pe.image_size, pe.section_fnv) != (0x400000, timestamp, size, fnv):
        raise ValueError("profile/header mismatch")
    if pe.read(FINDER_RVA, len(FINDER)) != FINDER:
        raise ValueError("complete finder body mismatch")
    calls = []
    for rva in CALL_RVAS:
        call = pe.read(rva, 5)
        target = pe.base + rva + 5 + struct.unpack("<i", call[1:])[0]
        if call[0] != 0xE8 or target != pe.base + FINDER_RVA:
            raise ValueError("finder call target mismatch")
        calls.append({"rva": rva, "bytes": call.hex(), "target_rva": target - pe.base,
                      "context_before": pe.read(rva - 20, 20).hex()})
    iat = pe.factory_iat()
    if iat != 0x3B82CC:
        raise ValueError("factory IAT mismatch")
    return {"image": str(path.resolve()), "sha256": sha, "profile": key,
            "image_base": pe.base, "timestamp": pe.timestamp, "image_size": pe.image_size,
            "section_fnv": pe.section_fnv, "finder_rva": FINDER_RVA,
            "finder_bytes": FINDER.hex(), "finder_calling_convention": "thiscall(device,w,h,bpp); ret12",
            "mode_calls": calls, "factory_iat_rva": iat,
            "scope": "Factory import and mode alias only; no other Refit GOG symbols admitted",
            "shell_request": [1200, 900, 16], "intro_request": [640, 480, 32]}

def header():
    rows = ["// Generated by tools/check_refit_fo_profile.py after exact-image validation.",
            "#pragma once", "#include <array>", "#include <cstdint>", "namespace a2fo::refit::profile {",
            "struct Image {const char* key;std::array<unsigned char,32> sha;std::uint32_t timestamp,size,sections;};",
            "inline constexpr std::array<Image,2> images{{"]
    for sha, (key, timestamp, size, fnv) in PROFILES.items():
        digest = ",".join("0x" + sha[i:i + 2] for i in range(0, 64, 2))
        rows.append('{"%s",{{%s}},0x%x,0x%x,0x%x},' % (key, digest, timestamp, size, fnv))
    rows.extend(["}};", "inline constexpr std::uint32_t preferred_base=0x400000;",
                 f"inline constexpr std::uint32_t finder_rva=0x{FINDER_RVA:x}, factory_iat_rva=0x3b82cc;",
                 "inline constexpr std::array<unsigned char,%d> finder_bytes{{%s}};" %
                 (len(FINDER), ",".join("0x%02x" % b for b in FINDER)),
                 "inline constexpr std::array<std::uint32_t,6> mode_calls{{%s}};" %
                 ",".join("0x%x" % rva for rva in CALL_RVAS), "}"])
    return "\n".join(rows) + "\n"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--image", type=Path, action="append", required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--write-header", type=Path)
    parser.add_argument("--check-header", type=Path)
    args = parser.parse_args()
    result = [inspect(path) for path in args.image]
    if args.write_header and set(r["sha256"] for r in result) != set(PROFILES):
        raise ValueError("header requires both reviewed images")
    if args.check_header and args.check_header.read_text() != header():
        raise ValueError("header drift")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps({"status": "offline verified; no game execution or binary edits",
                                      "images": result}, indent=2) + "\n")
    if args.write_header:
        args.write_header.parent.mkdir(parents=True, exist_ok=True)
        args.write_header.write_text(header())
    print("PASS:", len(result), "exact FO factory/mode profiles; other engine layouts unqualified")

if __name__ == "__main__":
    main()
