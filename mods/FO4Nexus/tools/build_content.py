#!/usr/bin/env python3
"""Build the FO4Nexus ODF overlay from an unmodified Fleet Operations parent."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
from collections import defaultdict
from pathlib import Path


FAMILIES = ("saber", "intrepid", "akira", "sovereign")
STORES = {
    "intrepid": ("photon", 48, "0.15", 1),
    "akira": ("photon", 96, "3", 2),
    "sovereign": ("quantum", 64, "2", 2),
    "saber": ("photon", 48, "0.15", 1),
}
CAPTAINS = (
    "Avery Morgan", "Jordan Vale", "Morgan Hale", "Casey Rowan",
    "Riley Quinn", "Taylor Arden", "Alex Mercer", "Samira Ellis",
    "Dara Bennett", "Robin Shaw", "Nadia Clarke", "Leah Winters",
)
FIELD = re.compile(r'^\s*([A-Za-z][A-Za-z0-9]*)\s*=\s*(.*?)\s*$', re.M)
QUOTED = re.compile(r'"([^"\r\n]*)"')


def fail(message: str) -> None:
    raise ValueError(message)


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read(path: Path) -> tuple[bytes, str]:
    raw = path.read_bytes()
    text = raw.decode("cp1252")
    if re.search(r'^\s*#\s*include\b', text, re.I | re.M):
        fail(f"unsupported include: {path}")
    return raw, text.replace("\r\n", "\n").replace("\r", "\n")


def fields(text: str) -> dict[str, list[str]]:
    found: dict[str, list[str]] = defaultdict(list)
    for key, value in FIELD.findall(text):
        found[key.lower()].append(value)
    return found


def one(found: dict[str, list[str]], key: str, path: Path, *, required: bool = True) -> str | None:
    values = found.get(key.lower(), [])
    if len(values) > 1 or (required and not values):
        fail(f"expected one {key} in {path}, found {len(values)}")
    return values[0] if values else None


def scalar(value: str, path: Path) -> str:
    match = re.fullmatch(r'\s*"([^"\r\n]*)"\s*(?://.*)?', value)
    if not match:
        fail(f"unsupported scalar in {path}: {value!r}")
    return match.group(1)


def append(text: str, assignments: list[tuple[str, str]], path: Path) -> str:
    existing = fields(text)
    for key, _ in assignments:
        if key.lower() in existing:
            fail(f"existing {key} in {path}")
    return text + ("" if text.endswith("\n") else "\n") + "\n// FO4Nexus overlay\n" + "\n".join(
        f"{key} = {value}" for key, value in assignments
    ) + "\n"


def replace_scalar(text: str, key: str, value: str, path: Path) -> str:
    pattern = re.compile(rf'^(\s*{re.escape(key)}\s*=\s*)"([^"\r\n]*)"([^\n]*)$', re.I | re.M)
    matches = list(pattern.finditer(text))
    if len(matches) != 1:
        fail(f"expected one quoted {key} in {path}, found {len(matches)}")
    old = matches[0]
    if old.group(3).strip() and not old.group(3).lstrip().startswith("//"):
        fail(f"unsupported {key} suffix in {path}")
    return text[:old.start()] + old.group(1) + '"' + value + '"' + old.group(3) + text[old.end():]


def names(value: str, path: Path) -> list[str]:
    parsed = QUOTED.findall(value)
    if not parsed or QUOTED.sub("", value).strip():
        fail(f"unsupported possibleCraftNames in {path}")
    return parsed


def registry(family: str, name: str) -> str:
    # The identity depends only on class and native craft name, never rank/avatar.
    number = int.from_bytes(hashlib.sha256(f"{family}\0{name}".encode("utf-8")).digest()[:8], "big") % 1_000_000_000
    return f"NCC-{number:09d}"


def output_bytes(text: str) -> bytes:
    return text.replace("\n", "\r\n").encode("cp1252")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--parent", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    parent = args.parent.resolve()
    odf = parent / "odf"
    output = args.output.resolve()
    if not odf.is_dir():
        fail(f"missing parent odf tree: {odf}")
    if output == parent or parent in output.parents:
        fail("output must not be inside the parent")
    source_paths = sorted(odf.rglob("*.odf"), key=lambda p: str(p.relative_to(odf)).lower())
    by_stem: dict[str, list[Path]] = defaultdict(list)
    by_name: dict[str, list[Path]] = defaultdict(list)
    for path in source_paths:
        by_stem[path.stem.lower()].append(path)
        by_name[path.name.lower()].append(path)
    planned: dict[Path, tuple[Path, bytes, str, dict]] = {}
    clone_consumers: dict[str, list[str]] = defaultdict(list)
    clone_kinds: dict[str, str] = {}
    identities: dict[str, tuple[str, str]] = {}
    counts = {family: 0 for family in FAMILIES}
    store_count = 0

    def put(destination: Path, source: Path, source_raw: bytes, generated: str, role: str, settings: dict) -> None:
        if destination in planned:
            fail(f"duplicate generated destination: {destination}")
        planned[destination] = (source, source_raw, generated, {"role": role, "settings": settings})

    for path in source_paths:
        family = next((family for family in FAMILIES if path.stem.lower().startswith("fed_" + family)), None)
        if family is None:
            continue
        raw, text = read(path)
        data = fields(text)
        if scalar(one(data, "classLabel", path), path).lower() != "craft":
            continue
        if len(by_name[path.name.lower()]) != 1:
            fail(f"duplicate craft basename: {path.name}")
        maximum_text = one(data, "maxShields", path)
        if not re.fullmatch(r'\d+', maximum_text or "") or int(maximum_text) < 4:
            fail(f"missing/unsupported numeric maxShields in {path}")
        maximum = int(maximum_text)
        forward = maximum * 35 // 100
        aft = maximum * 15 // 100
        port = maximum * 25 // 100
        starboard = maximum - forward - aft - port
        craft_names = names(one(data, "possibleCraftNames", path), path)
        captain_rows = []
        registry_rows = []
        for name in craft_names:
            captain_rows.append(CAPTAINS[int.from_bytes(hashlib.sha256(name.encode("utf-8")).digest()[:2], "big") % len(CAPTAINS)])
            ident = registry(family, name)
            existing = identities.setdefault(ident, (family, name))
            if existing != (family, name):
                fail(f"registry collision: {ident}: {existing} versus {(family, name)}")
            registry_rows.append(ident)
        tooltip = scalar(one(data, "tooltip", path), path)
        if len(tooltip) + len(" [Nexus]") > 255:
            fail(f"tooltip too long: {path}")
        modified = replace_scalar(text, "tooltip", tooltip + " [Nexus]", path)
        additions = [
            ("directionalShields", "1"),
            ("directionalShieldValueDisplayMode", "1"),
            ("forwardShieldStrength", str(forward)),
            ("aftShieldStrength", str(aft)),
            ("portShieldStrength", str(port)),
            ("starboardShieldStrength", str(starboard)),
            ("shieldTooltip", '"Directional shields"'),
            ("shieldVerboseTooltip", '"Four shield facings: forward 35%, aft 15%, port 25%, starboard 25%. Face incoming fire with the strongest shield."'),
            ("possibleCaptainNames", " ".join(json.dumps(name, ensure_ascii=False) for name in captain_rows)),
            ("possibleCraftRegistry", " ".join(json.dumps(name) for name in registry_rows)),
        ]
        weapon_refs = []
        for key, values in data.items():
            if not re.fullmatch(r'weapon\d+', key):
                continue
            ref = scalar(one(data, key, path), path)
            sources = by_stem.get(ref.lower(), [])
            if len(sources) != 1:
                fail(f"weapon {ref} in {path} resolves to {len(sources)} source files")
            weapon_path = sources[0]
            _, weapon_text = read(weapon_path)
            weapon_fields = fields(weapon_text)
            weapon_name_value = one(weapon_fields, "wpnName", weapon_path, required=False)
            if weapon_name_value is None:
                continue
            weapon_name = scalar(weapon_name_value, weapon_path).lower()
            kind = "quantum" if "quantum torpedo launcher" in weapon_name else "photon" if "photon torpedo launcher" in weapon_name else None
            if kind is None:
                continue
            if one(weapon_fields, "special", weapon_path) != "0":
                fail(f"torpedo launcher is not a normal weapon: {weapon_path}")
            expected_kind = STORES[family][0]
            if kind != expected_kind:
                fail(f"{path} uses {kind} launcher but {family} store is {expected_kind}")
            clone = "nx_" + ref
            if len(clone) > 27 or clone.lower() in by_stem:
                fail(f"invalid/colliding clone ID {clone} from {path}")
            if clone in clone_kinds and clone_kinds[clone] != kind:
                fail(f"launcher {clone} has incompatible store types")
            clone_kinds[clone] = kind
            weapon_refs.append((key, ref, clone, weapon_path, kind, weapon_text))
        if weapon_refs:
            kind, capacity, rate, mode = STORES[family]
            title = kind.title() + " Torpedoes"
            reason = "automatically reloads" if mode == 1 else "reloads near a same-team shipyard"
            prefix = kind + "Torpedo"
            additions.extend([
                ("max" + kind.title() + "Torpedoes", str(capacity)),
                (prefix + "Rate", rate),
                (prefix + "RechargeMode", str(mode)),
                (prefix + "DisplayMode", "1"),
                (prefix + "ValueDisplayMode", "1"),
                (prefix + "Tooltip", json.dumps(title)),
                (prefix + "VerboseTooltip", json.dumps(f"{title}: one round per projectile; {reason}.")),
            ])
            store_count += 1
        modified = append(modified, additions, path)
        for key, ref, clone, weapon_path, kind, weapon_text in weapon_refs:
            modified = replace_scalar(modified, key, clone, path)
            clone_consumers[clone].append(path.name)
            destination = Path("odf") / weapon_path.relative_to(odf).parent / (clone + ".odf")
            if destination not in planned:
                clone_raw, _ = read(weapon_path)
                clone_text = append(weapon_text, [
                    (kind + "TorpedoCost", "1"),
                    ("fireArcMode", '"box"'),
                    ("fireArcYaw", "0"),
                    ("fireArcPitch", "0"),
                    ("fireArcYawAngle", "120"),
                    ("fireArcPitchAngle", "90"),
                ], weapon_path)
                put(destination, weapon_path, clone_raw, clone_text, "cloned_launcher", {"original_id": ref, "clone_id": clone, "store": kind, "projectile_cost": 1})
        put(Path("odf") / path.relative_to(odf), path, raw, modified, "craft", {"family": family, "max_shields": maximum, "facing_strengths": [forward, aft, port, starboard], "normal_torpedo_launchers": len(weapon_refs), "store": STORES[family][0] if weapon_refs else None})
        counts[family] += 1

    if counts != {"saber": 48, "intrepid": 18, "akira": 36, "sovereign": 12}:
        fail(f"unexpected craft census: {counts}")
    identity_only = {"venture": 0, "mediterranean": 0}
    identity_classes = {"venture": "craft", "mediterranean": "constructionrig"}
    for path in source_paths:
        family = next((name for name in identity_only if path.stem.lower().startswith("fed_" + name)), None)
        if family is None:
            continue
        if path.parent != odf / "system" or len(by_name[path.name.lower()]) != 1:
            fail(f"unsupported identity-only source path or duplicate basename: {path}")
        raw, text = read(path)
        data = fields(text)
        class_label = scalar(one(data, "classLabel", path), path).lower()
        if class_label != identity_classes[family]:
            fail(f"unexpected classLabel for {path}: {class_label}")
        craft_names = names(one(data, "possibleCraftNames", path), path)
        captain_rows = []
        registry_rows = []
        for name in craft_names:
            captain_rows.append(CAPTAINS[int.from_bytes(hashlib.sha256(name.encode("utf-8")).digest()[:2], "big") % len(CAPTAINS)])
            ident = registry(family, name)
            existing = identities.setdefault(ident, (family, name))
            if existing != (family, name):
                fail(f"registry collision: {ident}: {existing} versus {(family, name)}")
            registry_rows.append(ident)
        tooltip = scalar(one(data, "tooltip", path), path)
        if len(tooltip) + len(" [Nexus]") > 255:
            fail(f"tooltip too long: {path}")
        modified = replace_scalar(text, "tooltip", tooltip + " [Nexus]", path)
        modified = append(modified, [
            ("possibleCaptainNames", " ".join(json.dumps(name, ensure_ascii=False) for name in captain_rows)),
            ("possibleCraftRegistry", " ".join(json.dumps(name) for name in registry_rows)),
        ], path)
        put(Path("odf") / path.relative_to(odf), path, raw, modified, "identity_only", {"family": family, "class_label": class_label, "name_rows": len(craft_names)})
        identity_only[family] += 1
    if identity_only != {"venture": 5, "mediterranean": 4}:
        fail(f"unexpected identity-only census: {identity_only}")
    yards = 0
    for path in source_paths:
        if not re.fullmatch(r'fed_.*yard.*', path.stem, re.I):
            continue
        raw, text = read(path)
        if scalar(one(fields(text), "classLabel", path), path).lower() != "shipyard":
            continue
        if len(by_name[path.name.lower()]) != 1:
            fail(f"duplicate yard basename: {path.name}")
        modified = append(text, [("torpedoResupply", "1"), ("torpedoResupplyRange", "350")], path)
        put(Path("odf") / path.relative_to(odf), path, raw, modified, "shipyard", {"torpedo_resupply": 1, "range": 350})
        yards += 1
    if not yards:
        fail("no Federation shipyards found")
    entries = []
    for destination, (source, raw, generated, metadata) in sorted(planned.items(), key=lambda row: str(row[0]).lower()):
        encoded = output_bytes(generated)
        target = output / destination
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(encoded)
        entry = {"source": str(source.relative_to(parent)), "output": str(destination), "source_sha256": digest(raw), "generated_sha256": digest(encoded), **metadata}
        if metadata["role"] == "cloned_launcher":
            entry["consumers"] = sorted(clone_consumers[metadata["settings"]["clone_id"]])
        entries.append(entry)
    manifest = {"schema": 1, "parent": str(parent), "counts": {"craft": counts, "identity_only": identity_only, "stores": store_count, "shipyards": yards, "cloned_launchers": len(clone_consumers), "files": len(entries)}, "files": entries}
    manifest_path = output.parent / "content-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(manifest["counts"], sort_keys=True))


if __name__ == "__main__":
    main()
