#!/usr/bin/env python3
"""Validate the FO4Nexus child package against its installed FO4 parent."""

import argparse
import hashlib
import json
import re
from pathlib import Path


FAMILIES = ("fed_saber", "fed_intrepid", "fed_akira", "fed_sovereign")
IDENTITY_ONLY = {
    "fed_venture", "fed_venturem", "fed_venturemm", "fed_venturemmm", "fed_ventures",
    "fed_mediterraneany", "fed_mediterraneanys", "fed_mediterraneanz", "fed_mediterraneanzs",
}
IDENTITY_ADDITIONS = {"possiblecaptainnames", "possiblecraftregistry"}
TALL_IDENTITY = {"infoBuildCaptainTextArea": (627, 10, 270, 18),
                 "infoBuildRegistryTextArea": (915, 10, 290, 18)}
REQUIRED = {"A2FO" + name for name in (
    "InfoIni", "FeaturePack", "BuildTooltips", "CraftIdentity",
    "DirectionalShields", "EnergySystems", "FireArcs", "Resources",
    "WeaponDamageControls")}
SHIELDS = ("forwardshieldstrength", "aftshieldstrength", "portshieldstrength", "starboardshieldstrength")
ARC = ("firearcmode", "firearcyaw", "firearcpitch", "firearcyawangle", "firearcpitchangle")
NATIVE_MEDIUM = {"infoProgressBar", "infoSingleConstructionArea", "infoSingleConstructionBarArea",
                 "infoSingleDilithiumBarArea", "infoSinglePlanetMetalBarArea"}
MEDIUM_TELEMETRY = {
    "infoSingleCaptainTextArea": (627, 18, 360, 20),
    "infoSingleRegistryTextArea": (627, 42, 360, 20),
    "infoSinglePhotonTorpedoesTextArea": (627, 72, 360, 20),
    "infoSingleQuantumTorpedoesTextArea": (627, 72, 360, 20),
    "infoSingleExperienceBarArea": (627, 101, 360, 7),
    "infoSingleDirectionalShieldsGraphicArea": (1050, 14, 96, 96),
    "infoSingleDirectionalShieldsForwardValueTextArea": (1070, 14, 56, 18),
    "infoSingleDirectionalShieldsAftValueTextArea": (1070, 91, 56, 18),
    "infoSingleDirectionalShieldsPortValueTextArea": (1005, 54, 40, 18),
    "infoSingleDirectionalShieldsStarboardValueTextArea": (1152, 54, 40, 18),
}
RING = "infoSingleDirectionalShieldsGraphicArea"
FACES = {key for key in MEDIUM_TELEMETRY if key.endswith("ValueTextArea")}
VALUE = re.compile(r'^\s*([A-Za-z_][A-Za-z_0-9]*)\s*=\s*(.*?)\s*$')
INCLUDE = re.compile(r'^\s*#include\s+"([^"]+)"\s*$')
QUOTED = re.compile(r'"([^"]*)"')
WEAPON = re.compile(r'weapon\d+$')


def files(directory):
    return {p.stem.lower(): p for p in directory.glob("*.odf")}


def parse(path, roots, errors, chain=()):
    if path in chain:
        errors.append(f"include cycle: {path}")
        return {}
    if not path.is_file():
        errors.append(f"missing ODF: {path}")
        return {}
    result = {}
    for number, raw in enumerate(path.read_text(encoding="cp1252").splitlines(), 1):
        line = raw.split("//", 1)[0].strip()
        if not line or line.startswith(";"):
            continue
        if line.startswith("#include"):
            match = INCLUDE.fullmatch(line)
            if not match:
                errors.append(f"unknown include syntax: {path}:{number}")
                continue
            name = match.group(1)
            candidates = [path.parent / name] + [root / name for root in roots]
            target = next((p for p in candidates if p.is_file()), None)
            if target is None:
                errors.append(f"unresolved include: {path}:{number}: {name}")
            else:
                result.update(parse(target, roots, errors, chain + (path,)))
            continue
        match = VALUE.match(line)
        if match:
            result[match.group(1).lower()] = match.group(2).strip()
    return result


def atom(value):
    return value.strip().strip('"').lower() if value is not None else ""


def number(value):
    try:
        return float(atom(value).rstrip("f"))
    except ValueError:
        return None


def list_values(value):
    return QUOTED.findall(value or "")


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def intersects(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return ax < bx + bw and bx < ax + aw and ay < by + bh and by < ay + ah


def check_header_includes(path, roots, errors, seen=None):
    seen = set() if seen is None else seen
    if path in seen:
        return
    seen.add(path)
    for line_no, raw in enumerate(path.read_text(encoding="cp1252").splitlines(), 1):
        line = raw.split("//", 1)[0].strip()
        if not line.startswith("#include"):
            continue
        match = INCLUDE.fullmatch(line)
        if match is None:
            errors.append(f"unknown header include syntax: {path}:{line_no}")
            continue
        name = match.group(1)
        target = next((candidate for candidate in (path.parent / name, *(root / name for root in roots)) if candidate.is_file()), None)
        if target is None:
            errors.append(f"unresolved header include: {path}:{line_no}: {name}")
        else:
            check_header_includes(target, roots, errors, seen)


def validate(parent, package, report):
    errors = []
    counts = {}
    checks = {}

    def check(name, condition, message):
        checks[name] = checks.get(name, 0) + 1
        if not condition:
            errors.append(message)

    podf, codf = parent / "odf/system", package / "odf/system"
    native, child = files(podf), files(codf)
    roots = (codf, podf)
    parsed = {}

    def data(name, prefer_child=True):
        key = name.lower().removesuffix(".odf")
        path = child.get(key) if prefer_child else None
        path = path or native.get(key)
        if path is None:
            errors.append(f"unresolved ODF reference: {name}")
            return {}
        if path not in parsed:
            parsed[path] = parse(path, roots, errors)
        return parsed[path]

    family = {name: path for name, path in native.items() if name.startswith(FAMILIES) and atom(data(name, False).get("classlabel")) == "craft"}
    counts["parent_family_craft"] = len(family)
    check("family_count", len(family) == 114, f"expected 114 parent family craft, found {len(family)}")
    for name in sorted(family):
        check("craft_present", name in child, f"missing craft override: {name}")
        if name not in child:
            continue
        original, overlay = data(name, False), data(name)
        for key, value in original.items():
            if WEAPON.fullmatch(key):
                old, new = atom(value), atom(overlay.get(key))
                check("weapon_reference", new in (old, "nx_" + old), f"{name}: changed {key}: {old} -> {new}")
                if new.startswith("nx_"):
                    check("cloned_weapon_present", new in child, f"{name}: missing cloned weapon {new}")
            elif key == "tooltip":
                check("native_tooltip", overlay.get(key) == value[:-1] + ' [Nexus]"' if value.endswith('"') else False, f"{name}: tooltip differs beyond Nexus suffix")
            else:
                check("native_parity", overlay.get(key) == value, f"{name}: native {key} differs")
        check("directional_shields", atom(overlay.get("directionalshields")) == "1", f"{name}: directionalShields must be 1")
        check("shield_value_display", atom(overlay.get("directionalshieldvaluedisplaymode")) == "1", f"{name}: directionalShieldValueDisplayMode must be 1")
        strength = [number(overlay.get(key)) for key in SHIELDS]
        check("shield_sum", None not in strength and sum(strength) == number(original.get("maxshields")), f"{name}: shield facing sum differs from native maxshields")
        names = list_values(overlay.get("possiblecraftnames"))
        captains = list_values(overlay.get("possiblecaptainnames"))
        registries = list_values(overlay.get("possiblecraftregistry"))
        check("identity_rows", bool(names) and len(names) == len(captains) == len(registries), f"{name}: captain/registry/name rows differ")
        kind = "photon" if name.startswith(("fed_intrepid", "fed_akira")) else "quantum" if name.startswith("fed_sovereign") else None
        if kind:
            quantity, rate, mode = ((48, 0.15, 1) if name.startswith("fed_intrepid") else (96, 3, 2) if name.startswith("fed_akira") else (64, 2, 2))
            check("torpedo_store_settings", number(overlay.get("max" + kind + "torpedoes")) == quantity and number(overlay.get(kind + "torpedorate")) == rate and number(overlay.get(kind + "torpedorechargemode")) == mode, f"{name}: {kind} store settings differ")
        for key, value in original.items():
            if key.startswith("nextrank") or "replace" in key or "rankclass" in key:
                for ref in list_values(value):
                    stem = ref.lower().removesuffix(".odf")
                    if stem in family:
                        check("progression_closure", stem in child, f"{name}: {key} target lacks override: {stem}")
        for key, value in overlay.items():
            if not WEAPON.fullmatch(key):
                continue
            launcher = atom(value)
            if not launcher.startswith("nx_"):
                continue
            weapon = data(launcher)
            orig_name = launcher[3:]
            original_weapon = data(orig_name, False)
            for field, old in original_weapon.items():
                check("launcher_native_parity", weapon.get(field) == old, f"{launcher}: native {field} differs")
            ordname = atom(weapon.get("ordname"))
            check("ordnance_reference", bool(ordname) and ordname in native, f"{launcher}: original ordname does not resolve parent: {ordname}")
            if ordname in native:
                original_ord = data(ordname, False)
                check("ordnance_damage", "damage" not in original_ord or original_ord.get("damage") == data(ordname).get("damage"), f"{launcher}: ordnance damage differs")
            kind = "photon" if "photontorpedocost" in weapon else "quantum" if "quantumtorpedocost" in weapon else None
            if kind:
                cost = number(weapon.get(kind + "torpedocost"))
                check("torpedo_store", number(overlay.get("max" + kind + "torpedoes")) is not None and number(overlay.get("max" + kind + "torpedoes")) > 0, f"{name}: {kind} launcher lacks store")
                check("torpedo_cost", cost is not None and cost > 0 and cost == 1, f"{launcher}: {kind} projectile cost must be one")
                expected_arc = {"firearcmode": "box", "firearcyaw": "0", "firearcpitch": "0", "firearcyawangle": "120", "firearcpitchangle": "90"}
                for field in ARC:
                    check("launcher_arc", atom(weapon.get(field)) == expected_arc[field], f"{launcher}: {field} differs")

    counts["parent_identity_only"] = sum(name in native for name in IDENTITY_ONLY)
    check("identity_only_parent_count", counts["parent_identity_only"] == 9, "parent lacks one of nine Venture/Mediterranean ODFs")
    for name in sorted(IDENTITY_ONLY):
        check("identity_only_present", name in child, f"missing identity-only override: {name}")
        if name not in child or name not in native:
            continue
        original, overlay = data(name, False), data(name)
        expected_class = "constructionrig" if name.startswith("fed_mediterranean") else "craft"
        check("identity_only_class", atom(original.get("classlabel")) == expected_class, f"{name}: unexpected parent classlabel")
        for key, value in original.items():
            if key == "tooltip":
                check("identity_only_tooltip", overlay.get(key) == value[:-1] + ' [Nexus]"' if value.endswith('"') else False, f"{name}: tooltip differs beyond Nexus suffix")
            else:
                check("identity_only_native_parity", overlay.get(key) == value, f"{name}: native {key} differs")
        added = set(overlay) - set(original)
        check("identity_only_added_fields", added == IDENTITY_ADDITIONS, f"{name}: unexpected added fields: {sorted(added)}")
        names = list_values(overlay.get("possiblecraftnames"))
        captains = list_values(overlay.get("possiblecaptainnames"))
        registries = list_values(overlay.get("possiblecraftregistry"))
        check("identity_only_rows", bool(names) and len(names) == len(captains) == len(registries), f"{name}: identity rows differ")

    clones = {name for name in child if name.startswith("nx_")}
    counts["cloned_launchers"] = len(clones)
    for name in sorted(clones):
        check("clone_id_length", len(name) <= 27, f"cloned launcher ID exceeds 27 characters: {name}")
        check("clone_origin", name[3:] in native, f"cloned launcher lacks parent origin: {name}")
    for name in child:
        if name in family or name in clones:
            continue
        fields = data(name)
        if atom(fields.get("classlabel")) == "craft":
            for key, value in fields.items():
                if WEAPON.fullmatch(key):
                    check("unmodified_craft_reference", atom(value) not in clones, f"unmodified craft {name} references Nexus clone")

    yards = {name for name in native if name.startswith("fed_") and "yard" in name and atom(data(name, False).get("classlabel")) == "shipyard"}
    counts["parent_federation_shipyards"] = len(yards)
    check("yard_count", len(yards) == 13, f"expected 13 Federation shipyards, found {len(yards)}")
    for name in sorted(yards):
        check("yard_present", name in child, f"missing shipyard override: {name}")
        if name in child:
            yard = data(name)
            check("yard_resupply", atom(yard.get("torpedoresupply")) == "1" and number(yard.get("torpedoresupplyrange")) == 350, f"{name}: expected torpedoResupply=1 and range=350")

    ini = package / "info.ini"
    if ini.is_file():
        sections = {}
        current = ""
        for line in ini.read_text(encoding="utf-8-sig").splitlines():
            line = line.split(";", 1)[0].strip()
            if line.startswith("[") and line.endswith("]"):
                current = line[1:-1].lower()
            elif "=" in line:
                key, value = line.split("=", 1)
                sections.setdefault(current, {})[key.strip().lower()] = atom(value)
        mod, modules = sections.get("mod", {}), sections.get("modules", {})
        check("parent_mod", mod.get("parentmod") == "fleet ops 4.0", "info.ini: ParentMod must be Fleet Ops 4.0")
        check("standalone", mod.get("standalone") == "0", "info.ini: standalone must be 0")
        check("settings_directory", bool(mod.get("settingsdirectory")) and mod.get("settingsdirectory") != "fleet ops 4.0", "info.ini: settings directory is missing or shared")
        required = {value for key, value in modules.items() if re.fullmatch(r"required\d+", key)}
        check("required_modules", required == {name.lower() for name in REQUIRED}, f"info.ini: required modules differ: {sorted(required)}")
    else:
        errors.append("missing info.ini")

    rts = package / "RTS_CFG.h"
    check("rts_firearc", rts.is_file() and re.search(r"(?m)^\s*int\s+firearc\s*=\s*1\s*;", rts.read_text(errors="replace")) is not None, "RTS_CFG.h: firearc=1 missing")
    if rts.is_file():
        check_header_includes(rts, (package, parent), errors)

    gui = package / "misc/gui_interface_misc.cfg"
    if gui.is_file():
        rectangles = {}
        for line in gui.read_text(encoding="cp1252").splitlines():
            match = re.match(r"\s*(info\w+)\s*=\s*(-?\d+)\s+(-?\d+)\s+(\d+)\s+(\d+)\s*$", line)
            if match:
                rectangles[match.group(1)] = tuple(map(int, match.groups()[1:]))
        expected = ("Captain", "Registry", "PhotonTorpedoes", "QuantumTorpedoes", "ShuttleCraft", "DirectionalShieldsGraphic", "DirectionalShieldsForwardAft", "DirectionalShieldsPortStarboard", "DirectionalShieldsForwardValue", "DirectionalShieldsAftValue", "DirectionalShieldsPortValue", "DirectionalShieldsStarboardValue", "ExperienceBar")
        for field in expected:
            single = rectangles.get("infoSingle" + field + ("Area" if field.endswith("Graphic") or field == "ExperienceBar" else "TextArea"))
            build = rectangles.get("infoBuild" + field + ("Area" if field.endswith("Graphic") or field == "ExperienceBar" else "TextArea"))
            check("ui_rectangles_present", single is not None and build is not None, f"UI rectangles missing: {field}")
            if single:
                x, y, w, h = single
                check("ui_medium_bounds", single == (0, 0, 0, 0) or (x >= 627 and y >= 0 and x + w <= 1249 and y + h <= 163), f"UI medium rectangle out of bounds: {field}")
            if single is not None:
                single_key = "infoSingle" + field + ("Area" if field.endswith("Graphic") or field == "ExperienceBar" else "TextArea")
                expected_rect = MEDIUM_TELEMETRY.get(single_key, (0, 0, 0, 0))
                check("ui_medium_layout", single == expected_rect, f"UI medium rectangle differs: {single_key}")
                check("ui_medium_frame_inset", single == (0, 0, 0, 0) or y >= 14, f"UI medium text/bar crosses the observed upper frame inset: {field}")
            build_key = "infoBuild" + field + ("Area" if field.endswith("Graphic") or field == "ExperienceBar" else "TextArea")
            if build_key in TALL_IDENTITY:
                check("ui_tall_identity", build == TALL_IDENTITY[build_key], f"UI tall identity rectangle differs: {build_key}")
            else:
                check("ui_tall_hidden", build == (0, 0, 0, 0), f"UI tall rectangle not hidden: {field}")
        telemetry = {"infoSingle" + field + ("Area" if field.endswith("Graphic") or field == "ExperienceBar" else "TextArea") for field in expected}
        visible = [(key, rect) for key, rect in rectangles.items() if key in telemetry and rect != (0, 0, 0, 0)]
        for native_key in sorted(NATIVE_MEDIUM):
            native_rect = rectangles.get(native_key)
            check("native_medium_present", native_rect is not None, f"native medium rectangle missing: {native_key}")
            if native_rect is None:
                continue
            bx, by, bw, bh = native_rect
            for key, (ax, ay, aw, ah) in visible:
                check("ui_native_clearance", not intersects((ax, ay, aw, ah), native_rect), f"UI telemetry overlaps native medium {native_key}: {key}")
        tall = [(key, rectangles[key]) for key in TALL_IDENTITY if key in rectangles]
        if len(tall) == 2:
            check("ui_tall_no_overlap", not intersects(tall[0][1], tall[1][1]), "UI tall identity rectangles overlap")
        for native_key in [*(f"infoBuildQueue_{index}" for index in range(10)), "infoProgressBar"]:
            native_rect = rectangles.get(native_key)
            check("native_tall_present", native_rect is not None, f"native tall rectangle missing: {native_key}")
            if native_rect is not None:
                for key, rect in tall:
                    check("ui_tall_native_clearance", not intersects(rect, native_rect), f"UI tall identity overlaps {native_key}: {key}")
        for i, (a, (ax, ay, aw, ah)) in enumerate(visible):
            for b, (bx, by, bw, bh) in visible[i + 1:]:
                if intersects((ax, ay, aw, ah), (bx, by, bw, bh)):
                    allowed = ({a, b} == {"infoSinglePhotonTorpedoesTextArea", "infoSingleQuantumTorpedoesTextArea"}
                               or (a == RING and b in FACES) or (b == RING and a in FACES))
                    check("ui_no_overlap", allowed, f"UI overlap: {a}, {b}")
        counts["ui_rectangles"] = len(rectangles)
    else:
        errors.append("missing misc/gui_interface_misc.cfg")

    parent_spr = parent / "Sprites/gui_global.spr"
    child_spr = package / "Sprites/gui_global.spr"
    nexus_spr = package / "Sprites/nx_ui.spr"
    if parent_spr.is_file() and child_spr.is_file():
        raw_lines = child_spr.read_bytes().splitlines(keepends=True)
        include_lines = [index for index, line in enumerate(raw_lines) if line.strip().lower() == b"@include nx_ui.spr"]
        skip_lines = [index for index, line in enumerate(raw_lines) if line.strip().lower().startswith(b"@skip")]
        check("sprite_include_once", len(include_lines) == 1, "gui_global.spr must include nx_ui.spr exactly once")
        check("sprite_include_early", bool(include_lines and skip_lines and include_lines[0] < skip_lines[0]), "nx_ui.spr include must precede first @skip")
        if len(include_lines) == 1:
            remainder = b"".join(line for index, line in enumerate(raw_lines) if index != include_lines[0])
            check("parent_sprite_bytes", remainder == parent_spr.read_bytes(), "gui_global.spr differs beyond inserted nx_ui.spr include")
    else:
        errors.append("missing parent or child Sprites/gui_global.spr")

    if nexus_spr.is_file():
        sprite_bytes = nexus_spr.read_bytes()
        check("sprite_crlf", b"\r\n" in sprite_bytes and b"\n" not in sprite_bytes.replace(b"\r\n", b""), "nx_ui.spr must use CRLF")
        sprite_text = sprite_bytes.decode("cp1252")
        defs = [line.strip().split() for line in sprite_text.splitlines() if line.strip().startswith(("dsq ", "dsqs "))]
        check("sprite_definitions", defs == [["dsq", "nx_ui_shield_quad", "0", "0", "128", "128"],
                                             ["dsqs", "nx_ui_shield_quad_side", "0", "0", "128", "128"]], "nx_ui.spr must define only dsq and dsqs on native TGA copies")
        check("sprite_interface", "@tmaterial=interface" in sprite_text.lower() and "@reference=128" in sprite_text.lower(), "nx_ui.spr lacks interface material/reference")
        check("no_world_sprite_node", "sprite_node" not in sprite_text.lower(), "nx_ui.spr contains world sprite_node")
    else:
        errors.append("missing Sprites/nx_ui.spr")

    texture_names = ("nx_ui_shield_quad.tga", "nx_ui_shield_quad_side.tga")
    for name in texture_names:
        texture = package / "Textures" / name
        if not texture.is_file():
            errors.append(f"missing texture: {name}")
            continue
        raw = texture.read_bytes()
        header = raw[:18]
        valid = (len(header) == 18 and header[0] == 0 and header[1] == 0 and header[2] == 2
                 and int.from_bytes(header[12:14], "little") == 128 and int.from_bytes(header[14:16], "little") == 128
                 and header[16] == 32 and header[17] & 15 == 8 and len(raw) >= 18 + 128 * 128 * 4)
        check("shield_tga_header", valid, f"{name}: expected 128x128 uncompressed RGBA TGA type 2")

    asset_manifest = package.parent / "ui-assets-manifest.json"
    if asset_manifest.is_file():
        try:
            assets = json.loads(asset_manifest.read_text())
            entries = assets.get("files", [])
            expected_outputs = {"Sprites/gui_global.spr", "Sprites/nx_ui.spr", *("Textures/" + name for name in texture_names)}
            check("ui_asset_manifest_coverage", {entry.get("output_path") for entry in entries} == expected_outputs and len(entries) == 4, "UI asset manifest must list exactly four outputs")
            for entry in entries:
                output_path = package / entry.get("output_path", "")
                check("ui_asset_output_hash", output_path.is_file() and sha(output_path) == entry.get("output_sha256"), f"UI asset output hash differs: {output_path}")
                source_path = entry.get("source_path", "")
                if source_path.startswith("generated:"):
                    check("ui_asset_generated_source", entry.get("source_sha256") is None, f"generated UI asset has source hash: {source_path}")
                else:
                    source = Path(source_path)
                    check("ui_asset_source_hash", source.is_file() and sha(source) == entry.get("source_sha256"), f"UI asset source hash differs: {source_path}")
            counts["ui_asset_manifest_entries"] = len(entries)
        except (ValueError, TypeError) as exc:
            errors.append(f"invalid UI asset manifest: {exc}")
    else:
        errors.append("missing ui-assets-manifest.json")

    manifest = package.parent / "content-manifest.json"
    if manifest.is_file():
        try:
            doc = json.loads(manifest.read_text())
            for entry in doc.get("files", []):
                rel = entry.get("output", "")
                target = package / rel
                source = parent / entry.get("source", "")
                check("manifest_hash", target.is_file() and sha(target) == entry.get("generated_sha256") and source.is_file() and sha(source) == entry.get("source_sha256"), f"content manifest hash mismatch: {rel}")
            counts["manifest_entries"] = len(doc.get("files", []))
            roles = {entry.get("output", "").lower(): entry.get("role") for entry in doc.get("files", [])}
            identity_manifest = {Path(path).stem for path, role in roles.items() if role == "identity_only"}
            identity_counts = doc.get("counts", {}).get("identity_only")
            check("manifest_identity_only", identity_manifest == IDENTITY_ONLY and identity_counts == {"mediterranean": 4, "venture": 5}, f"content manifest identity-only coverage/count differs: {sorted(identity_manifest)}, {identity_counts}")
        except (ValueError, TypeError) as exc:
            errors.append(f"invalid content manifest: {exc}")

    result = {"ok": not errors, "checks": checks, "counts": counts, "errors": errors}
    report.parent.mkdir(parents=True, exist_ok=True)
    report.write_text(json.dumps(result, indent=2) + "\n")
    print(f"FO4Nexus validation: {'PASS' if result['ok'] else 'FAIL'}; {sum(checks.values())} checks; {len(errors)} errors; report {report}")
    return result["ok"]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--package", type=Path, required=True)
    parser.add_argument("--report", type=Path, required=True)
    args = parser.parse_args()
    raise SystemExit(0 if validate(args.parent, args.package, args.report) else 1)


if __name__ == "__main__":
    main()
