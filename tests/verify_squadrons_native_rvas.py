#!/usr/bin/env python3
"""Check the adapter's ShipDisplay RVAs against a supplied ArmadaL.exe.

Reads only PE headers and bytes; does not execute or modify the game.
"""
import re
import struct
import sys
from pathlib import Path


def pe_reader(executable: Path):
    image = executable.read_bytes()
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    assert image[pe:pe + 4] == b"PE\0\0", "not a PE image"
    count = struct.unpack_from("<H", image, pe + 6)[0]
    optional_size = struct.unpack_from("<H", image, pe + 20)[0]
    image_base = struct.unpack_from("<I", image, pe + 24 + 28)[0]
    sections = []
    for i in range(count):
        offset = pe + 24 + optional_size + i * 40
        _, rva, size, raw = struct.unpack_from("<IIII", image, offset + 8)
        sections.append((rva, size, raw))

    def at(rva: int, size: int) -> bytes:
        for start, length, raw in sections:
            if start <= rva and rva + size <= start + length:
                return image[raw + rva - start:raw + rva - start + size]
        raise AssertionError(f"unmapped RVA {rva:#x}")

    return at, image_base


def verify(executable: Path) -> None:
    at, image_base = pe_reader(executable)
    source = (Path(__file__).resolve().parents[1] /
              "modules/A2FOSquadrons/module.cpp").read_text()
    for name, signature in (
        ("kShipDisplayCountReturns", bytes.fromhex("ff 90 94 00 00 00")),
        ("kShipDisplayHandlesReturns", bytes.fromhex("ff 92 98 00 00 00")),
    ):
        body = re.search(rf"{name}\{{\{{(.*?)\}}\}}", source, re.S).group(1)
        addresses = [int(value, 16) for value in re.findall(r"0x[0-9a-f]+", body)]
        assert len(addresses) == 3
        for address in addresses:
            assert at(address - 6, 6) == signature, f"{name}: incorrect RVA {address:#x}"
            print(f"PASS {name}: image RVA {address:#010x}")

    render_slot = struct.unpack("<I", at(0x2B4BCC + 0x58, 4))[0]
    assert render_slot == image_base + 0xF2BB0, "unexpected ShipDisplay Render slot"
    assert at(0x11B160, 6) == bytes.fromhex("55 8b ec 83 ec 10"), "unexpected text renderer"
    print("PASS ShipDisplay Render vtable and text renderer")
    for index, getter in enumerate((0xce170, 0xce190, 0xce180, 0xce1b0, 0xce1a0, 0xce1c0)):
        assert at(getter, 7) == bytes([0x8b, 0x81, 0x80 + index * 4, 0, 0, 0, 0xc3])
    assert at(0xce29f, 3) == bytes.fromhex("8b 47 68")
    print("PASS six native class cost fields and raw build-time field")
    repair_slot = struct.unpack("<I", at(0x2B3744 + 0x2C, 4))[0]
    assert repair_slot == image_base + 0x13A6E0, "unexpected repair Activate vtable"
    assert at(0x13A6E0, 5) == bytes.fromhex("55 8b ec 51 53")
    assert at(0x13A6EA, 8) == bytes.fromhex("8b 7b 44 8b f1 8b 47 3c")
    assert at(0x13A6F2, 3) == bytes.fromhex("83 f8 11")
    assert at(0x13A6FB, 3) == bytes.fromhex("83 f8 35")
    assert at(0x13A704, 3) == bytes.fromhex("83 f8 14")
    assert at(0x13A7E8, 3) == bytes.fromhex("8b 46 1c")
    assert at(0xCFD50, 7) == bytes.fromhex("8b 41 04 83 c0 44 c3")
    assert at(0xBBDE1, 11) == bytes.fromhex("56 6a 0a 8b cf e8 05 5d 01 00 8b")
    assert at(0xBBE17, 6) == bytes.fromhex("5f 8b c3 5e 5b c3")
    print("PASS repair queue owner, repair/recycle commands, geometry and native build result/launch command")


def verify_selection(executable: Path) -> None:
    at, _ = pe_reader(executable)
    # The fourth stack argument, [ebp+0x14], gates the clear-selection
    # virtual call. The sixth, [ebp+0x1c], gates compatibility checks later.
    assert at(0x1DAD58, 26) == bytes.fromhex(
        "80 7d 14 00 74 14 8b 45 08 8b 00 ba a0 00 00 00 "
        "e8 07 6f 00 00 8b 4d 08 ff d0"), "unexpected selection-clear argument"
    assert at(0x1DAF9A, 4) == bytes.fromhex("80 7d 1c 00"), "unexpected compatibility argument"
    print("PASS Fleet Ops selection: fourth argument clears; sixth checks compatibility")
    assert at(0x1DD0B8, 6) == bytes.fromhex("55 8b ec 83 c4 cc")
    assert at(0x1DD29D, 3) == bytes.fromhex("c2 0c 00"), "NeedsRepair must pop three arguments"
    assert at(0x122C8C, 6) == bytes.fromhex("55 8b ec 83 c4 c8")
    print("PASS Fleet Ops NeedsRepair three-argument ABI and paid job deletion entry")


if __name__ == "__main__":
    if len(sys.argv) not in (2, 3):
        raise SystemExit("usage: verify_squadrons_native_rvas.py /path/to/ArmadaL.exe [/path/to/FleetOpsHook.dll]")
    verify(Path(sys.argv[1]))
    if len(sys.argv) == 3:
        verify_selection(Path(sys.argv[2]))
