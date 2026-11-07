#!/usr/bin/env python3
"""Generate the transparent Refit setup menu skin.

Usage: python3 tools/generate_refit_menu_skin.py [--check]
       python3 tools/generate_refit_menu_skin.py --pack [--check]
       python3 tools/generate_refit_menu_skin.py [--output-build DIR] [--check]

The source PNGs, atlas, and manifest live in assets/refit-menus/ui. The default
binary output is build/refit-menu-ui/skin.bgra. --check compares expected bytes
with existing files and never writes them. --pack reads the existing source
PNGs, preserving edits to those tiles.
"""

import argparse
import io
import json
import struct
from pathlib import Path

from PIL import Image, ImageDraw


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "assets/refit-menus/ui"
DEFAULT_BUILD = ROOT / "build/refit-menu-ui"
SIZE = 64
CUT = 8
TILES = (
    ("panel", "#101820", 180, "#8c9aa4"),
    ("header", "#27343f", 210, None),
    ("button_normal", "#263744", 220, "#8999a3"),
    ("button_hot", "#365064", 220, "#e9be73"),
    ("button_pressed", "#7a5630", 220, "#e9be73"),
    ("button_disabled", "#202933", 180, "#53616a"),
    ("popup", "#101820", 255, "#8c9aa4"),
    ("popup_selected", "#365064", 255, "#e9be73"),
)


def rgba(hex_rgb: str, alpha: int) -> tuple[int, int, int, int]:
    return (*bytes.fromhex(hex_rgb[1:]), alpha)


def make_tile(name: str, fill: str, alpha: int, edge: str | None) -> Image.Image:
    image = Image.new("RGBA", (SIZE, SIZE))
    draw = ImageDraw.Draw(image)
    if name == "header":
        # Only the top corners are cut; the lower corners stay square.
        draw.polygon([(CUT, 0), (SIZE - CUT - 1, 0), (SIZE - 1, CUT),
                      (SIZE - 1, SIZE - 1), (0, SIZE - 1), (0, CUT)],
                     fill=rgba(fill, alpha))
        draw.line([(0, SIZE - 1), (SIZE - 1, SIZE - 1)],
                  fill=rgba("#b38a4d", 255), width=1)
    else:
        outer = [(CUT, 0), (SIZE - CUT - 1, 0), (SIZE - 1, CUT),
                 (SIZE - 1, SIZE - CUT - 1), (SIZE - CUT - 1, SIZE - 1),
                 (CUT, SIZE - 1), (0, SIZE - CUT - 1), (0, CUT)]
        inner = [(CUT, 1), (SIZE - CUT - 1, 1), (SIZE - 2, CUT),
                 (SIZE - 2, SIZE - CUT - 1), (SIZE - CUT - 1, SIZE - 2),
                 (CUT, SIZE - 2), (1, SIZE - CUT - 1), (1, CUT)]
        draw.polygon(outer, fill=rgba(edge, 255))
        draw.polygon(inner, fill=rgba(fill, alpha))
    return image


def png_bytes(image: Image.Image) -> bytes:
    output = io.BytesIO()
    image.save(output, format="PNG", optimize=False)
    return output.getvalue()


def binary_bytes(atlas: Image.Image) -> bytes:
    header = struct.pack("<8I", 0x31534955, 128, 256, SIZE, SIZE, len(TILES), 0, 0)
    pixels = bytearray()
    rgba_pixels = atlas.tobytes()
    for offset in range(0, len(rgba_pixels), 4):
        red, green, blue, alpha = rgba_pixels[offset:offset + 4]
        pixels.extend(((blue * alpha + 127) // 255,
                       (green * alpha + 127) // 255,
                       (red * alpha + 127) // 255, alpha))
    return header + pixels


def read_tile(path: Path) -> Image.Image:
    if not path.is_file():
        raise ValueError(f"Missing source tile: {path}")
    with Image.open(path) as tile:
        if tile.format != "PNG" or tile.mode != "RGBA" or tile.size != (SIZE, SIZE):
            raise ValueError(f"Source tile must be RGBA {SIZE}x{SIZE} PNG: {path}")
        return tile.copy()


def outputs(build_dir: Path, pack: bool) -> dict[Path, bytes]:
    atlas = Image.new("RGBA", (128, 256))
    files = {}
    entries = []
    if pack:
        source_tiles = {path.name for path in SOURCE.glob("*.png") if path.name != "atlas.png"}
        expected_tiles = {f"{tile[0]}.png" for tile in TILES}
        if source_tiles != expected_tiles:
            raise ValueError(f"Expected exactly {len(TILES)} source tile PNGs; "
                             f"missing={sorted(expected_tiles-source_tiles)}, "
                             f"unexpected={sorted(source_tiles-expected_tiles)}")
    for index, (name, fill, alpha, edge) in enumerate(TILES):
        path = SOURCE / f"{name}.png"
        tile = read_tile(path) if pack else make_tile(name, fill, alpha, edge)
        if not pack:
            files[path] = png_bytes(tile)
        x, y = (index % 2) * SIZE, (index // 2) * SIZE
        atlas.paste(tile, (x, y))
        entries.append({"name": name, "file": f"{name}.png", "index": index,
                        "source": {"x": x, "y": y, "width": SIZE, "height": SIZE},
                        "nine_slice": {"left": CUT, "top": CUT,
                                       "right": CUT, "bottom": CUT},
                        "interior_opacity": tile.getpixel((SIZE // 2, SIZE // 2))[3],
                        "frame_border_opacity": 0 if edge is None else tile.getpixel((SIZE // 2, 0))[3],
                        "bottom_edge_opacity": tile.getpixel((SIZE // 2, SIZE - 1))[3]})
    files[SOURCE / "atlas.png"] = png_bytes(atlas)
    manifest = {"format": "RGBA PNG; binary resource is top-down premultiplied BGRA",
                "atlas": "atlas.png", "atlas_width": 128, "atlas_height": 256,
                "cell_width": SIZE, "cell_height": SIZE, "tiles": entries}
    files[SOURCE / "atlas.json"] = (json.dumps(manifest, indent=2) + "\n").encode()
    files[build_dir / "skin.bgra"] = binary_bytes(atlas)
    return files


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify files without writing")
    parser.add_argument("--pack", action="store_true",
                        help="pack existing source tile PNGs without replacing them")
    parser.add_argument("--output-build", type=Path, default=DEFAULT_BUILD,
                        metavar="DIR", help="binary output directory")
    args = parser.parse_args()
    build_dir = args.output_build.resolve()
    try:
        expected = outputs(build_dir, args.pack)
    except ValueError as error:
        parser.exit(1, f"{error}\n")
    if args.check:
        mismatches = [str(path) for path, data in expected.items()
                      if not path.is_file() or path.read_bytes() != data]
        if mismatches:
            parser.exit(1, "Missing or stale generated files:\n" + "\n".join(mismatches) + "\n")
        print(f"Refit menu skin verified: {len(expected)} files")
    else:
        for path, data in expected.items():
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
        print(f"Refit menu skin generated: {len(expected)} files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
