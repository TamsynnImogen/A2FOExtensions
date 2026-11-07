#!/usr/bin/env python3
"""Build the Nexus shield UI sprite overlay from locally verified assets."""

import argparse
import hashlib
import json
from pathlib import Path
import struct


EXPECTED = {
    "shield_quad.tga": "80454ea3b78cf7c4c13717c44dbb3afdf8dd050213850d339073dd94d579b58b",
    "shield_quad_side.tga": "02f94126ad6b2e68d411bc6184ac0233ea9f6b178d0016a83295bf7b9f3bb374",
}
OUTPUT_NAMES = {
    "shield_quad.tga": "nx_ui_shield_quad.tga",
    "shield_quad_side.tga": "nx_ui_shield_quad_side.tga",
}
INCLUDE = b"@include nx_ui.spr\r\n"
INITIAL_INCLUDES = (
    b"@include tex_anim_gui.spr",
    b"@include miniimages.spr",
    b"@include systembackgrounds.spr",
    b"@include systemimages.spr",
)
NX_SPR = (
    b"sprite_table\r\n"
    b"@reference=128\r\n"
    b"@tmaterial=interface\r\n"
    b"@skip=off\r\n"
    b"dsq nx_ui_shield_quad 0 0 128 128\r\n"
    b"dsqs nx_ui_shield_quad_side 0 0 128 128\r\n"
)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def validate_tga(path, data, expected_hash):
    digest = sha256(data)
    if digest != expected_hash:
        raise ValueError(f"{path}: SHA-256 {digest}, expected {expected_hash}")
    if len(data) < 18:
        raise ValueError(f"{path}: truncated TGA header")
    image_type = data[2]
    width, height = struct.unpack_from("<HH", data, 12)
    bpp = data[16]
    if (image_type, width, height, bpp) != (2, 128, 128, 32):
        raise ValueError(
            f"{path}: expected type 2, 128x128, 32bpp; got "
            f"type {image_type}, {width}x{height}, {bpp}bpp"
        )
    return digest


def update_global(source, data):
    if data.count(INCLUDE) or b"nx_ui_shield_quad" in data or b"nx_ui_shield_quad_side" in data:
        raise ValueError(f"{source}: duplicate Nexus include or sprite definition")
    lines = data.splitlines(keepends=True)
    if len(lines) < 6 or not lines[0].startswith(b"sprite_table"):
        raise ValueError(f"{source}: missing expected opening sprite_table structure")
    for index, prefix in enumerate(INITIAL_INCLUDES, start=1):
        if not lines[index].startswith(prefix) or not lines[index].endswith(b"\r\n"):
            raise ValueError(f"{source}: missing expected opening include line {index}")
    skip_indices = [i for i, line in enumerate(lines[1:], start=1)
                    if line.startswith(b"@skip=off")]
    if not skip_indices or skip_indices[0] <= 4:
        raise ValueError(f"{source}: expected @skip=off after four opening includes")
    return b"".join(lines[:5] + [INCLUDE] + lines[5:])


def ensure_output_separate(output, parent, asset_source):
    for label, root in (("parent", parent), ("asset source", asset_source)):
        if output == root or root in output.parents:
            raise ValueError(f"output must not be inside {label}: {root}")


def build(parent, output, asset_source):
    global_source = parent / "Sprites/gui_global.spr"
    if not global_source.is_file():
        raise ValueError(f"missing parent sprite table: {global_source}")
    original_global = global_source.read_bytes()
    updated_global = update_global(global_source, original_global)

    textures = {}
    for source_name, output_name in OUTPUT_NAMES.items():
        source = asset_source / source_name
        data = source.read_bytes()
        digest = validate_tga(source, data, EXPECTED[source_name])
        textures[source_name] = (source, output_name, data, digest)

    sprite_dir = output / "Sprites"
    texture_dir = output / "Textures"
    sprite_dir.mkdir(parents=True, exist_ok=True)
    texture_dir.mkdir(parents=True, exist_ok=True)
    (sprite_dir / "gui_global.spr").write_bytes(updated_global)
    (sprite_dir / "nx_ui.spr").write_bytes(NX_SPR)
    for _source_name, output_name, data, _digest in textures.values():
        (texture_dir / output_name).write_bytes(data)

    files = [
        {
            "source_path": str(global_source),
            "source_sha256": sha256(original_global),
            "output_path": "Sprites/gui_global.spr",
            "output_sha256": sha256(updated_global),
        },
        {
            "source_path": "generated: nx_ui.spr definition",
            "source_sha256": None,
            "output_path": "Sprites/nx_ui.spr",
            "output_sha256": sha256(NX_SPR),
        },
    ]
    for source_name, (_source, output_name, data, digest) in textures.items():
        files.append({
            "source_path": str(asset_source / source_name),
            "source_sha256": digest,
            "output_path": f"Textures/{output_name}",
            "output_sha256": sha256(data),
        })
    manifest = {
        "parent": str(parent),
        "asset_source": str(asset_source),
        "artwork_provenance": {
            "local_reference_test": True,
            "distribution_provenance": "pending",
        },
        "files": files,
    }
    (output.parent / "ui-assets-manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--asset-source", type=Path, required=True)
    args = parser.parse_args()
    parent, output, asset_source = (
        args.parent.resolve(), args.output.resolve(), args.asset_source.resolve()
    )
    try:
        ensure_output_separate(output, parent, asset_source)
        build(parent, output, asset_source)
    except (OSError, ValueError) as exc:
        parser.error(str(exc))
    print(f"Built UI assets at {output}")


if __name__ == "__main__":
    main()
