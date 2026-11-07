#!/usr/bin/env python3
"""Make a research-only placement plan from a legacy Menu/map.ini.

Usage: python3 tools/menu_stage_lab/scene_plan.py --map-ini PATH --output DIR
Writes JSON and CSV evidence, never a native map. Coordinates follow
menu-renderer/src/menu/mod.rs Group::place; no BZN coordinate contract is implied.
"""

import argparse
import csv
import hashlib
import json
import math
from pathlib import Path


class PlanError(ValueError):
    pass


def number(value, where):
    try:
        result = float(value.rstrip("f"))
    except ValueError as exc:
        raise PlanError(f"{where}: invalid number {value!r}") from exc
    if not math.isfinite(result):
        raise PlanError(f"{where}: non-finite number {value!r}")
    return result


def vector(value, where):
    parts = value.split(",")
    if len(parts) != 3:
        raise PlanError(f"{where}: expected three vector components")
    return [number(part.strip(), where) for part in parts]


def read_sections(data):
    sections = {}
    current = None
    try:
        lines = data.decode("utf-8-sig").splitlines()
    except UnicodeDecodeError as exc:
        raise PlanError("map.ini is not UTF-8 text") from exc
    for line_no, line in enumerate(lines, 1):
        line = line.split(";", 1)[0].strip()
        if not line:
            continue
        if line.startswith("[") and line.endswith("]"):
            label = line[1:-1].strip()
            key = label.casefold()
            if not label or key in sections:
                raise PlanError(f"line {line_no}: empty or duplicate section ID {label!r}")
            current = {"label": label, "settings": {}}
            sections[key] = current
            continue
        if "=" not in line or current is None:
            raise PlanError(f"line {line_no}: malformed setting or missing section")
        name, value = (part.strip() for part in line.split("=", 1))
        key = name.casefold()
        if not name or key in current["settings"]:
            raise PlanError(f"line {line_no}: empty or duplicate setting {name!r}")
        current["settings"][key] = value.strip('"')
    return sections


NUMERIC_KEYS = {
    "scene": "skyturn stars starsize starseed sundistance blend",
    "warp": "delay stagger ramp stretch speed turn follow tunnelat tunnelin dim tunnelspeed widen flash flashtime",
    "planet": "radius spin clouds orbit",
    "group": "yaw cloakdelay cloaktime cloakstagger",
    "ship": "yaw scale bob phase warp",
    "view": "fov swayperiod",
    "nebula": "spacing jitter seed radius keyshare scatter opacity brightness",
}
VECTOR_KEYS = {
    "scene": "keyaxis keycolour fillaxis fillcolour",
    "planet": "at axis",
    "group": "at warpcolour",
    "ship": "at team",
    "view": "eye target sway warpeye warptarget",
    "nebula": "at size",
}


def validate_settings(sections):
    for key, section in sections.items():
        kind = key.split(".", 1)[0]
        for name in NUMERIC_KEYS.get(kind, "").split():
            if name in section["settings"]:
                num(section, name)
        for name in VECTOR_KEYS.get(kind, "").split():
            if name in section["settings"]:
                vec(section, name)


def field(section, key, default=None):
    return section["settings"].get(key, default)


def num(section, key, default=0.0):
    value = field(section, key)
    return default if value is None else number(value, f'{section["label"]}.{key}')


def vec(section, key, default=(0.0, 0.0, 0.0)):
    value = field(section, key)
    return list(default) if value is None else vector(value, f'{section["label"]}.{key}')


def place(origin, yaw_deg, point):
    yaw = math.radians(yaw_deg)
    sine, cosine = math.sin(yaw), math.cos(yaw)
    x, y, z = point
    return [origin[0] + x * cosine + z * sine,
            origin[1] + y,
            origin[2] - x * sine + z * cosine]


def plan(data, source):
    sections = read_sections(data)
    validate_settings(sections)
    groups = {"": {"at": [0.0, 0.0, 0.0], "yaw_degrees": 0.0}}
    raw = {s["label"]: dict(s["settings"]) for s in sections.values()}
    for key, section in sections.items():
        if key.startswith("group."):
            groups[key[6:]] = {"at": vec(section, "at"),
                                "yaw_degrees": num(section, "yaw")}

    def group_for(section):
        name = field(section, "group", "").casefold()
        if name not in groups:
            raise PlanError(f'{section["label"]}: unknown group {name!r}')
        return name, groups[name]

    bounds_points = []
    actors, views, regions = [], [], []
    for key, section in sections.items():
        kind, dot, ident = key.partition(".")
        if kind in {"ship", "view", "nebula", "group"} and (not dot or not ident):
            raise PlanError(f'{section["label"]}: missing named ID')
        if kind == "ship":
            name, group = group_for(section)
            odf = field(section, "odf")
            if not odf:
                raise PlanError(f'{section["label"]}: missing Odf')
            local = vec(section, "at")
            position = place(group["at"], group["yaw_degrees"], local)
            bounds_points.append(position)
            actors.append({"id": ident, "odf": odf, "group": name,
                           "local_position": local, "position": position,
                           "local_yaw_degrees": num(section, "yaw"),
                           "yaw_degrees": group["yaw_degrees"] + num(section, "yaw")})
        elif kind == "view":
            name, group = group_for(section)
            eye = place(group["at"], group["yaw_degrees"], vec(section, "eye", (0, 50, 400)))
            target = place(group["at"], group["yaw_degrees"], vec(section, "target"))
            bounds_points.extend((eye, target))
            views.append({"id": ident, "group": name,
                          "eye_marker": f"menu_camera_{ident}",
                          "target_marker": f"menu_target_{ident}",
                          "eye": eye, "target": target,
                          "fov_degrees": num(section, "fov", 40),
                          "sway": vec(section, "sway", (8, 4, 6)),
                          "sway_period": num(section, "swayperiod", 40)})
        elif kind == "nebula":
            name, group = group_for(section)
            odf = field(section, "odf")
            if not odf:
                raise PlanError(f'{section["label"]}: missing Odf')
            at = vec(section, "at")
            size = vec(section, "size", (1500, 0, 1500))
            if size[0] < 0 or size[2] < 0:
                raise PlanError(f'{section["label"]}: negative region extent')
            centre = place(group["at"], group["yaw_degrees"], at)
            corners = [place(group["at"], group["yaw_degrees"],
                             (at[0] + x, at[1], at[2] + z))
                       for x in (-size[0], size[0]) for z in (-size[2], size[2])]
            bounds_points.extend(corners)
            regions.append({"id": ident, "odf": odf, "group": name,
                            "centre": centre, "local_half_extents": size,
                            "rotated_corners": corners, "resolved_actor_centres": None,
                            "unresolved": ["ODF effectRadius", "spacing and seeded placement"]})

    planet = None
    section = sections.get("planet")
    if section and field(section, "class"):
        centre = vec(section, "at")
        radius = num(section, "radius", 3000)
        if radius <= 0:
            raise PlanError("Planet.Radius: expected positive radius")
        planet = {"odf": field(section, "class"), "centre": centre,
                  "radius": radius, "axis": vec(section, "axis", (1, 0, 0))}
        bounds_points.extend([[centre[i] + sign * radius if j == i else centre[j]
                               for j in range(3)] for i in range(3) for sign in (-1, 1)])
    if not bounds_points:
        raise PlanError("no spatial content")
    bounds = {"min": [min(p[i] for p in bounds_points) for i in range(3)],
              "max": [max(p[i] for p in bounds_points) for i in range(3)],
              "includes": ["actor origins", "camera eye and target anchors",
                           "planet radius", "rotated nebula region corners"],
              "native_editor_bounds_verification_required": True,
              "nebula_actor_extents_unresolved": bool(regions)}
    return {"kind": "legacy-menu-research-conversion-plan", "native_map": False,
            "source": {"path": str(source), "sha256": hashlib.sha256(data).hexdigest(),
                       "raw_settings": raw},
            "actors": actors, "views": views, "groups": groups,
            "planet": planet, "nebula_regions": regions, "bounds": bounds,
            "legacy_choreography": {s["label"]: dict(s["settings"])
                                    for key, s in sections.items()
                                    if key in {"scene", "warp"} or key.startswith(("group.", "ship.", "view.", "nebula."))},
            "limitations": ["No BZN records written or coordinate import contract established",
                            "Static transforms omit runtime bob, sway, cloak, warp, orbit and gas jitter",
                            "Nebula actor centres require resolved effectRadius and spacing"]}


def write_csv(path, columns, rows):
    with path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--map-ini", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = plan(args.map_ini.read_bytes(), args.map_ini.resolve())
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "conversion-plan.json").write_text(json.dumps(result, indent=2) + "\n")
    write_csv(args.output / "actors.csv", ["id", "odf", "group", "x", "y", "z", "yaw_degrees"],
              [dict(id=a["id"], odf=a["odf"], group=a["group"],
                    x=a["position"][0], y=a["position"][1], z=a["position"][2],
                    yaw_degrees=a["yaw_degrees"]) for a in result["actors"]])
    write_csv(args.output / "camera-markers.csv", ["view", "marker", "kind", "x", "y", "z"],
              [dict(view=v["id"], marker=v[f"{kind}_marker"], kind=kind,
                    x=v[kind][0], y=v[kind][1], z=v[kind][2])
               for v in result["views"] for kind in ("eye", "target")])
    print(f'{len(result["actors"])} actors, {len(result["views"])} views, '
          f'{len(result["nebula_regions"])} unresolved nebula regions')


if __name__ == "__main__":
    main()
