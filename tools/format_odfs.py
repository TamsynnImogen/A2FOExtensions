#!/usr/bin/env python3
"""Semantics-preserving formatter for Armada and Fleet Operations ODF files.

The formatter keeps each assignment together with its continuation/table rows,
preserves the relative order of repeated assignments with the same key, and
groups command blocks into compact human-readable sections. It validates a
per-key semantic signature before writing and is idempotent by design.
"""

from __future__ import annotations

import argparse
import collections
import dataclasses
import json
import os
from pathlib import Path
import re
import sys
import tempfile
from typing import Iterable


ACTIVE_ASSIGNMENT_RE = re.compile(
    r"^\s*([A-Za-z_][A-Za-z0-9_.]*)\s*=\s*(.*?)\s*$"
)
DISABLED_ASSIGNMENT_RE = re.compile(
    r"^\s*(?://|#)\s*([A-Za-z_][A-Za-z0-9_.]*)\s*=\s*(.*?)\s*$"
)
GENERATED_SECTION_RE = re.compile(r"^// --- [^-].* ---$")


@dataclasses.dataclass
class Entry:
    key: str | None
    value: str | None
    continuation: list[str]
    prefix: list[str]
    original_index: int
    active: bool
    source_line: str | None = None


@dataclasses.dataclass
class ParsedOdf:
    entries: list[Entry]
    unsafe_reasons: list[str]


@dataclasses.dataclass
class FormatResult:
    path: str
    changed: bool
    safe: bool
    reason: str = ""
    assignments: int = 0
    continuation_lines: int = 0
    section_count: int = 0
    duplicate_keys: int = 0
    input_bytes: int = 0
    output_bytes: int = 0


CATEGORY_TITLES = (
    "File identity and inheritance",
    "Display and localization",
    "Construction and costs",
    "Hull, shields, crew, and energy",
    "Subsystems, damage, and repair",
    "Movement and physics",
    "Weapons and ordnance",
    "Accuracy, hardpoints, and targeting",
    "Art, animation, and effects",
    "Audio and events",
    "Artificial intelligence and tactical behavior",
    "Interface and controls",
    "Progression and variants",
    "Resource handling and recycling",
    "Faction rules and starting units",
    "Additional parameters",
    "Capabilities and object behavior",
    "Runtime class",
)


# Fleet Operations' published templates use different reading orders for
# GameObjects, weapons, ordnance, and factions. Keep the same category IDs for
# classification, but render them in the order appropriate to the detected ODF
# profile. Categories absent from a file are simply skipped.
PROFILE_SECTION_ORDERS = {
    "gameobject": (0, 1, 2, 3, 4, 6, 7, 10, 11, 9, 5, 12, 13, 8, 14, 15, 16, 17),
    "weapon": (0, 1, 6, 7, 3, 9, 8, 11, 10, 5, 12, 13, 14, 4, 2, 15, 16, 17),
    "ordnance": (0, 8, 5, 6, 7, 9, 3, 4, 10, 11, 12, 13, 14, 2, 15, 16, 17),
    "faction": (0, 1, 11, 14, 9, 3, 4, 10, 13, 12, 8, 2, 5, 6, 7, 15, 16, 17),
    "interface": (0, 1, 11, 8, 9, 14, 12, 2, 13, 3, 4, 5, 6, 7, 10, 15, 16, 17),
    "generic": (0, 1, 12, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 14, 15, 16, 17),
}


TEMPLATE_FIELD_ORDERS = {
    "gameobject": (
        "unitname", "tooltip", "verbosetooltip", "race",
        "buildtime", "officercost", "crewcost", "dilithiumcost",
        "latinumcost", "metalcost", "biomattercost",
        "maxhealth", "curhealth", "healthrate", "maxshield",
        "maxshields", "curshield", "curshields", "shieldrate",
        "maxspecialenergy", "specialenergyrate", "specialenergydisplaymode",
        "shieldgeneratorhitpoints", "engineshitpoints", "weaponshitpoints",
        "lifesupporthitpoints", "sensorshitpoints", "possiblecraftnames",
        "weapon1", "weaponhardpoints1", "weapon1iconpos",
        "hulltargethardpoints", "lifesupporttargethardpoints",
        "weaponstargethardpoints", "shieldgeneratortargethardpoints",
        "sensorstargethardpoints", "enginestargethardpoints",
        "criticaltargethardpoints", "attackpower", "intrinsicvalue",
        "weaponyellow", "weaponred", "hotkeylabel", "scalesod", "shipclass",
        "eventselect", "eventacknowledge", "eventattack", "eventstop",
        "eventmove", "eventrepair", "physicsfile", "trekphysicsfile",
        "avoidanceclass", "shiptype", "shielddelay", "rangescan", "damagedscan",
        "enginesrepairtime", "lifesupportrepairtime", "weaponsrepairtime",
        "shieldgeneratorrepairtime", "sensorsrepairtime", "enginescrewloss",
        "lifesupportcrewloss", "weaponscrewloss", "shieldgeneratorcrewloss",
        "sensorscrewloss", "lifesupportloss", "engineshitpercent",
        "lifesupporthitpercent", "weaponshitpercent",
        "shieldgeneratorhitpercent", "sensorshitpercent", "crewhitpercent",
        "hullhitpercent", "ainame", "fireball", "basename", "xplevel",
        "nextrankclass", "nextrankxp", "worthxp", "recycledilithium",
        "recycletritanium", "recyclesupply", "recycletime", "maxextraweapons",
        "extraweaponhardpoints", "shieldhit", "shieldhitcritical", "shielddown",
        "ship", "has_hitpoints", "has_crew", "transporter", "alert",
        "show_sw_autonomy", "show_movement_autonomy", "can_explore", "combat",
        "can_sandd", "buildablebyownerraceonly", "classlabel",
    ),
    "weapon": (
        "wpnname", "tooltip", "verbosetooltip", "ordname",
        "candamageshields", "candamagehull", "shielddamagemodifier",
        "hulldamagemodifier", "shotdelay0",
        "firesound", "hitchance", "impulsetohitmodifier", "stoptohitmodifier",
        "tohitimpulsemodifier", "tohitstopmodifier", "range", "classlabel",
    ),
    "ordnance": (
        "sprite", "radius", "lifespan", "shotspeed", "spriteduration",
        "shieldduration", "damagebase", "damagevariance", "damagethreshold",
        "shieldcrewmodifier", "hullcrewmodifier", "flareradiusmultiplier",
        "flaresprite", "explosionhit", "explosionshieldhit", "classlabel",
    ),
    "faction": (
        "name", "displaykey", "instantactionslot", "interfaceconfiguration",
        "interfacesprites", "canplaycolonizeplanets", "canplaycollectlatinum",
        "canplaycapturetheflag", "affectedbypsionicinsanity", "cancommandeer",
        "canbecommandeered", "officerres", "officertooltip",
        "officerverbosetooltip", "creationlabel", "recycledlabel",
        "singleplayermusic", "multiplayermusic", "transportsprite",
        "repairstrength", "boardingstrength", "retreatstrength",
        "crewretreatratio", "crewaccumulationrate",
        "planetcrewaccumulationmodifier", "citytexturename",
        "atmospheretexturename", "atmospheretint", "numcities",
        "crewyellowstatus", "crewredstatus", "repairyellow", "repairred",
        "weaponyellow", "weaponred", "officerupgradeodf", "minimalunits1",
        "standardunits1", "superunits1", "ctfunits1",
        "transporttofriendsonnoteasy", "transporttoenemyonhard",
        "recycledilithiumfraction", "recyclemetalfraction",
        "miningratemodifier", "insufficientofficersevent", "normaldilithium",
        "normalmetal", "normallatinum", "normalcrew", "normalbiomatter",
        "lotsdilithium", "lotsmetal", "lotslatinum", "lotscrew",
        "lotsbiomatter", "cantrade", "allowgivedilithium",
        "allowgivelatinum", "allowgivemetal", "allowgivebiomatter",
        "allowgivecrew", "cangainxp", "preloadunits0", "classlabel",
    ),
}


TEMPLATE_FIELD_RANKS = {
    profile: {key: rank for rank, key in enumerate(fields)}
    for profile, fields in TEMPLATE_FIELD_ORDERS.items()
}


def contains_any(value: str, needles: Iterable[str]) -> bool:
    return any(needle in value for needle in needles)


def category_for(key: str | None, profile: str = "generic") -> int:
    if key is None:
        return 15

    key = key.lower()

    # The runtime constructor is kept alone at the end. Fleet Operations'
    # templates place it first in weapons/ordnance and late in GameObjects, but
    # separating it makes the engine-facing identity unambiguous in every ODF.
    if key == "classlabel":
        return 17

    if key in {
        "basename", "originalclass", "baseclass", "baseodf", "parentclass",
    } or contains_any(key, ("inherit", "include")):
        return 0

    if key in {
        "name", "unitname", "wpnname", "tooltip", "verbosetooltip",
        "displaykey", "hotkeylabel", "race", "shipclass",
        "possiblecraftnames", "description", "objectname", "dbname",
        "officerres", "officertooltip", "officerverbosetooltip",
        "creationlabel", "recycledlabel", "freightername", "keymaplabel",
    } or contains_any(key, ("tooltip", "displayname", "localiz", "verbose")):
        return 1

    if profile == "interface" and (
        re.match(r"^item\d+$", key)
        or key in {"source", "sourcenot", "sourcetypeor", "dest", "param", "preferredposition"}
        or key.endswith(("data", "label", "image", "height", "width", "positionx", "positiony"))
        or key.startswith(("player", "tally", "timeline", "battle", "loadbar", "tip"))
    ):
        return 11

    # UI geometry and controls must precede broad resource/movement matching:
    # fields such as dilithiumHeaderRect and scrollButton are interface data.
    if contains_any(
        key,
        (
            "button", "interface", "menu", "panel", "cursor", "window",
            "scroll", "tabrect", "headerrect", "datarect", "textarea",
            "iconarea", "mapicon", "preferredselect", "tabbmp", "bordername",
            "mapsize", "adlog", "commandbar", "wireframe", "raceicon", "overlay",
            "commandnames", "commandhotkeys", "loadbar", "numberoftips", "tipposition",
        ),
    ) or key.endswith(("rect", "area", "bmp", "bmph")) or key in {
        "commandname", "selectmode", "istoggledon", "istoggle", "toggle",
        "allowbuildqueuepop", "source", "sourcenot", "sourcetypeor", "dest",
        "param", "topbary", "bottomy", "buildtopy", "classheaderx",
        "stardateheaderx", "buildheaderx", "builtheaderx", "lostheaderx", "betweenplayerspace",
        "betweentabspace", "individualbarthin",
    } or key.startswith(("show", "hide", "tip")):
        return 11

    if profile == "faction" and (
        key in {
            "instantactionslot", "affectedbypsionicinsanity", "cancommandeer",
            "canbecommandeered", "officerupgradeodf",
        }
        or key.startswith(("canplay", "minimalunits", "standardunits", "superunits", "ctfunits"))
    ):
        return 14

    if key == "numberofraces" or re.match(r"^race\d+$", key):
        return 14

    if key in {
        "buildablebyownerraceonly", "has_hitpoints", "has_crew",
    } or key.startswith("has_"):
        return 16

    if key in {
        "worthxp", "xplevel", "nextrankclass", "nextrankxp", "prevrankclass",
        "isresearchedbyyard", "isshipupgrade", "shipisbuiltbyevolution",
        "destroymewithresearchstation", "requiredalliedrace", "beneficialbuild",
        "replacemeninstantcycle", "cangainxp", "ownergainsxp",
    } or key.startswith(
        (
            "rank", "xp", "veteran", "upgrade", "replacement", "avatar",
            "mixedtech", "fleetcap", "supplyclasses", "research", "evolve",
            "alliedrepairupgrade", "module", "preloadunits",
        )
    ) or key in {
        "numberofmodules", "maxcap", "simplecapmode", "fusedclass",
    } or key.startswith(("multireq", "techfile")):
        return 12

    if key == "crewcost" or contains_any(
        key,
        (
            "builditem", "buildtime", "buildable", "construction",
            "officercost", "supplycost", "workerbee",
        ),
    ) or key.endswith("cost"):
        return 2

    if key in {"cantrade", "isbuy"} or key.startswith(
        ("normal", "lots", "allowgive")
    ) or contains_any(
        key,
        (
            "dilithium", "tritanium", "latinum", "biomatter", "metal",
            "resource", "recycle", "mining", "harvest", "cargo", "trade",
        ),
    ) or key == "freightersize":
        return 13


    if contains_any(key, ("targethardpoint", "hardpoint", "iconpos")) or key in {
        "basetargets", "forceinvalidatetarget", "turntofire", "registertargethit",
        "targetrange", "maxtargets", "maxhits", "needstarget", "targetterrainonly",
        "targetself", "hitinvalid", "rangefactor",
    } or contains_any(
        key,
        (
            "hitchance", "tohit", "needtarget", "firearc", "useprimarytarget",
            "validtargets", "hitmode", "targetlocation", "targetloop", "autotarget",
            "hitcloaked", "targetallied", "targetenemy", "hitcondition",
            "maxextratargets", "attackrange",
        ),
    ):
        return 7

    if key in {
        "maxhealth", "curhealth", "healthrate", "maxshield", "maxshields",
        "curshield", "curshields", "shieldrate", "shielddelay",
        "maxspecialenergy", "specialenergyrate", "specialenergydisplaymode",
        "weaponyellow", "weaponred", "crewyellowstatus", "crewredstatus",
        "repairyellow", "repairred", "repairstrength", "boardingstrength",
        "boardingpartystrength", "maximumcrew", "maxcrew", "curcrew",
        "boardingdelay", "boardinginterval", "boardingsize", "boardingpercent",
        "boardingrate", "boardingtime", "repairside", "regenmodifier",
        "repairmodifier", "regenvalue", "repairvalue", "maxcrewgain",
        "decloakshielddelay", "repairsshields", "shieldamount",
        "crewaffectsweapondelay",
    } or contains_any(key, ("crewaccumulation", "energydrain", "drainspecialenergy")):
        return 3

    if key.startswith(("shieldgenerator", "lifesupport", "engines", "sensors")) or contains_any(
        key,
        (
            "hitpoints", "hitpercent", "crewloss", "repairtime", "hullhit",
            "crewhit", "lifesupportloss", "repairrate", "disablesensors",
            "disablelifesupport", "disablecrew",
        ),
    ):
        return 4

    if re.match(
        r"^(sensor|engine|weapon|lifesupport|shieldgenerator)mesh\d+"
        r"(explosion)?$",
        key,
    ):
        return 4

    if key.endswith(".scale") or key.startswith(
        ("shieldhit", "shielddown", "explosion", "bstar.")
    ) or contains_any(
        key,
        (
            "animation", "sprite", "texture", "model", "overlaysod",
            "scalesod", "fireball", "particle", "geometry", "colour", "color",
            "light", "alpha", "emit", "render", "flare", "glow", "fade",
            "lodshift", "scalevalue", "scalecount", "lengthscale", "lensflare",
            "wave_effect", "waveeffect", "animateowner", "rootnode", "lifetimer",
            "effecttimer", "autoexpire", "closedscale", "openscale",
            "scaleeffect", "scaling", "markshrink", "marktime", "green_flow",
        ),
    ) or key.endswith("sod") or (profile == "ordnance" and key in {"radius", "shieldduration"}) or key in {
        "atmospheretint", "numcities", "length", "duration", "rays", "scalesize",
        "maxscalefactor", "background_0", "background_1", "background_2",
        "background_3", "background_4", "background_5", "background_6",
        "background_7", "ghostambient", "ghostdiffuse", "ghostspecular",
        "minasteroids", "maxasteroids", "rstar.0", "empty", "dominion",
        "klingon", "federation", "borg", "ciadan", "romulan", "iconian",
        "noxter",
    }:
        return 8

    if key == "damagedscan":
        return 10

    if contains_any(
        key,
        (
            "event", "sound", "music", "voice", "speech", "speak",
            "acknowledge", "chant", "wavename", "distancefactor",
            "dopplerfactor", "samplerate", "bitspersample", "audiovisual",
            "rumble", "minimumdistance3d", "maximumdistance3d",
        ),
    ) or re.match(r"^track\d+$", key) or key in {
        "mintime", "maxtime", "startreturntime",
    }:
        return 9

    if contains_any(
        key,
        (
            "physics", "velocity", "acceleration", "deceleration", "speed",
            "turnrate", "movement", "impulse", "warp", "collision", "radius",
            "buffer", "footprint", "welding", "mass", "path", "rotate",
            "rotation", "pitch", "yaw", "trek", "nebula", "omega", "seektime",
            "belowgrid", "forwardaccel", "backwardaccel", "inertia",
            "controldistance", "controlstiffness", "tooclosetoturn",
            "tooslowtoturn", "preferredposition", "traveltime", "takeoff",
            "closure", "repel", "initialdistance", "finaldistance",
        ),
    ) or key.startswith("roll") or key in {
        "shotspeed", "maxroll", "turncontrolsquared", "turncontrolangle",
    } or (profile == "ordnance" and key == "lifespan"):
        return 5

    if key.startswith(("weapon", "ord", "shot", "specialweapon", "cloak")) or contains_any(
        key,
        (
            "extraweapon", "damage", "projectile", "torpedo", "phaser", "pulse",
            "beam", "lifespan", "shieldduration", "disableweapons",
            "disableengines", "specialvalue", "drainrate", "draintime",
            "initialshotdelay", "firemodifier", "successchance", "repulsionforce",
            "shockwave", "slowpercent", "disableshield", "destructionprobability",
            "disableweapon", "disabletimer", "disabletime", "percentreduction",
        ),
    ) or key in {
        "range", "affectedsystem", "affectedsystems", "ordname",
        "blametheshooter", "disablecloak", "shieldcrewmodifier",
        "hullcrewmodifier", "affectortype", "turnmodifier", "hitmodifier",
        "rate", "percentage", "timebetween", "standbytime",
    }:
        return 6

    if key in {
        "attackpower", "intrinsicvalue", "avoidanceclass", "shiptype", "scout",
        "assault", "miner", "detectcloak", "alwaysactiveai", "alwaysactiveplayer",
        "visiblerange", "affectsai",
    } or contains_any(
        key,
        (
            "aip", "artificial", "priority", "formation", "threat", "retreat",
            "rangescan", "damagedscan", "combat", "alert", "explore", "autonomy",
            "avoidme", "clearsfog", "switchtoattack", "transporttofriends",
            "transporttoenemy",
        ),
    ) or key == "ainame":
        return 10

    if key in {
        "facility", "ship", "station", "transporter", "builder_ship",
        "builder_station", "builder_facility", "giveisallowed", "invincible",
        "invalidastarget", "perceivedneutral", "hidden", "planet",
        "repairfacility", "decommissionfacility", "iamgametothedeathcondition",
        "beneficial", "infinite", "repair_ship", "selfdestruct", "forcetoneutral",
        "createobject", "preservestatus", "transferstats", "special",
        "onetimemode", "isinfinite", "initialcloak", "expireonclose",
        "hasnegativeeffectonowner",
    } or key.startswith(("has_", "can", "is_", "ignore", "override", "concurrent")) or re.match(
        r"^[a-z]typeallowed$", key
    ):
        return 16

    return 15


def profile_for(parsed: ParsedOdf) -> str:
    keys = {
        entry.key.lower()
        for entry in parsed.entries
        if entry.active and entry.key is not None
    }
    classlabels = {
        (entry.value or "").strip().strip('"').lower()
        for entry in parsed.entries
        if entry.active and entry.key is not None and entry.key.lower() == "classlabel"
    }

    if "instantactionslot" in keys or any(
        key.startswith(("minimalunits", "standardunits", "superunits", "ctfunits"))
        for key in keys
    ):
        return "faction"

    if "damagebase" in keys and (
        {"lifespan", "shotspeed", "damagevariance", "damagethreshold"} & keys
        or classlabels & {
            "phaser", "unitorpedo", "utribeam", "pphaser", "unibeam",
            "photontorpedo", "probetorpedo", "plasmacannon", "nanitesordnance",
        }
    ):
        return "ordnance"

    if {"ordname", "wpnname", "shotdelay0"} & keys or any(
        contains_any(label, ("weapon", "cannon", "launcher", "artillery", "minelayer"))
        for label in classlabels
    ):
        return "weapon"

    if {"unitname", "maxhealth", "curhealth", "weapon1"} & keys or classlabels & {
        "craft", "shipyard", "pod", "evolver", "constructionrig", "freighter",
        "fighter", "repairship", "mining", "research", "buildyard",
    }:
        return "gameobject"

    if not classlabels and (
        {"buttonname", "commandname", "item1"} & keys
        or any(
            contains_any(key, ("buttonrect", "buttonsprite", "headerrect", "tabbmp", "datarect"))
            for key in keys
        )
    ):
        return "interface"

    return "generic"


def is_comment(line: str) -> bool:
    stripped = line.lstrip()
    return stripped.startswith("//") or stripped.startswith("#")


def disabled_key(line: str) -> str | None:
    match = DISABLED_ASSIGNMENT_RE.match(line)
    return match.group(1) if match else None


def parse_lines(lines: list[str]) -> ParsedOdf:
    entries: list[Entry] = []
    unsafe: list[str] = []
    pending: list[str] = []
    current: Entry | None = None
    next_index = 0

    def add_entry(entry: Entry) -> None:
        nonlocal next_index
        entry.original_index = next_index
        next_index += 1
        entries.append(entry)

    def flush_pending(next_key: str | None = None) -> list[str]:
        nonlocal pending
        if not pending:
            return []

        significant = [line for line in pending if line.strip()]
        disabled = [disabled_key(line) for line in significant]
        disabled = [key for key in disabled if key is not None]

        if not disabled or (
            next_key is not None and disabled[-1].lower() == next_key.lower()
        ):
            result = significant
            pending = []
            return result

        # A commented-out command that does not match the following active
        # command is a standalone block and should follow its own category.
        representative = disabled[-1]
        add_entry(
            Entry(
                key=representative,
                value=None,
                continuation=[],
                prefix=[],
                original_index=0,
                active=False,
                source_line="\n".join(significant),
            )
        )
        pending = []
        return []

    for line_number, original_line in enumerate(lines, start=1):
        line = original_line.rstrip(" \t")
        if GENERATED_SECTION_RE.match(line):
            current = None
            continue

        match = ACTIVE_ASSIGNMENT_RE.match(line)
        if match:
            key, value = match.groups()
            prefix = flush_pending(key)
            current = Entry(
                key=key,
                value=value,
                continuation=[],
                prefix=prefix,
                original_index=0,
                active=True,
            )
            add_entry(current)
            continue

        if not line.strip():
            current = None
            continue

        if is_comment(line):
            current = None
            pending.append(line)
            continue

        if "=" in line:
            unsafe.append(
                f"line {line_number}: assignment-like syntax was not recognized"
            )

        if current is not None:
            current.continuation.append(line)
        else:
            prefix = flush_pending()
            add_entry(
                Entry(
                    key=None,
                    value=None,
                    continuation=[],
                    prefix=prefix,
                    original_index=0,
                    active=False,
                    source_line=line,
                )
            )

    trailing = flush_pending()
    if trailing:
        add_entry(
            Entry(
                key=None,
                value=None,
                continuation=[],
                prefix=trailing,
                original_index=0,
                active=False,
            )
        )

    return ParsedOdf(entries=entries, unsafe_reasons=unsafe)


def semantic_signature(parsed: ParsedOdf) -> dict[str, list[tuple[str, tuple[str, ...]]]]:
    signature: dict[str, list[tuple[str, tuple[str, ...]]]] = collections.defaultdict(list)
    for entry in parsed.entries:
        if not entry.active or entry.key is None:
            continue
        signature[entry.key.lower()].append(
            (
                entry.value or "",
                tuple(line.strip() for line in entry.continuation),
            )
        )
    return dict(signature)


def generated_section(title: str) -> str:
    return f"// --- {title} ---"


def render_entry(entry: Entry) -> list[str]:
    output: list[str] = []
    output.extend(entry.prefix)
    if entry.active and entry.key is not None:
        output.append(f"{entry.key} = {entry.value or ''}".rstrip())
        for line in entry.continuation:
            output.append(f"    {line.lstrip()}".rstrip())
    elif entry.source_line:
        output.extend(entry.source_line.split("\n"))
    return output


def template_field_rank(profile: str, entry: Entry) -> tuple[float, int]:
    if entry.key is None:
        return (1_000_000, entry.original_index)

    key = entry.key.lower()
    ranks = TEMPLATE_FIELD_RANKS.get(profile, {})
    if key in ranks:
        return (float(ranks[key]), entry.original_index)

    # Apply the published position of representative numbered fields to their
    # complete arrays while retaining the numeric and repeated-key order.
    numbered_patterns = (
        (r"^(weapon)(\d+)$", "weapon1"),
        (r"^(weaponhardpoints)(\d+)$", "weaponhardpoints1"),
        (r"^(weapon)(\d+)(iconpos)$", "weapon1iconpos"),
        (r"^(minimalunits)(\d+)$", "minimalunits1"),
        (r"^(standardunits)(\d+)$", "standardunits1"),
        (r"^(superunits)(\d+)$", "superunits1"),
        (r"^(ctfunits)(\d+)$", "ctfunits1"),
        (r"^(preloadunits)(\d+)$", "preloadunits0"),
    )
    for pattern, representative in numbered_patterns:
        match = re.match(pattern, key)
        if match and representative in ranks:
            number = int(match.group(2))
            return (ranks[representative] + number / 1000.0, entry.original_index)

    return (1_000_000, entry.original_index)


def format_parsed(parsed: ParsedOdf) -> tuple[list[str], int]:
    meaningful = [
        entry
        for entry in parsed.entries
        if entry.active or entry.source_line or entry.prefix
    ]
    if not meaningful:
        return [], 0

    assignment_count = sum(1 for entry in meaningful if entry.active)
    use_sections = assignment_count >= 4

    if not use_sections:
        output: list[str] = []
        for entry in meaningful:
            output.extend(render_entry(entry))
        return output, 0

    profile = profile_for(parsed)
    grouped: dict[int, list[Entry]] = collections.defaultdict(list)
    for entry in meaningful:
        grouped[category_for(entry.key, profile)].append(entry)

    output = []
    section_count = 0
    for category_index in PROFILE_SECTION_ORDERS[profile]:
        category_entries = grouped.get(category_index)
        if not category_entries:
            continue
        category_entries = sorted(
            category_entries,
            key=lambda entry: template_field_rank(profile, entry),
        )
        if output:
            output.append("")
        output.append(generated_section(CATEGORY_TITLES[category_index]))
        section_count += 1
        for entry in category_entries:
            rendered = render_entry(entry)
            if entry.prefix and len(output) > 1 and output[-1] != "":
                output.append("")
            output.extend(rendered)

    return output, section_count


def decode_odf(data: bytes) -> tuple[str, str, bytes]:
    bom = b""
    payload = data
    if payload.startswith(b"\xef\xbb\xbf"):
        bom = b"\xef\xbb\xbf"
        payload = payload[len(bom) :]
    try:
        return payload.decode("utf-8"), "utf-8", bom
    except UnicodeDecodeError:
        return payload.decode("cp1252"), "cp1252", bom


def newline_for(data: bytes) -> str:
    crlf = data.count(b"\r\n")
    bare_lf = data.count(b"\n") - crlf
    return "\r\n" if crlf >= bare_lf else "\n"


def encode_odf(text: str, encoding: str, bom: bytes) -> bytes:
    return bom + text.encode(encoding)


def format_bytes(data: bytes) -> tuple[bytes, ParsedOdf, ParsedOdf, int]:
    text, encoding, bom = decode_odf(data)
    newline = newline_for(data)
    had_final_newline = text.endswith(("\n", "\r"))
    lines = text.splitlines()
    parsed_before = parse_lines(lines)
    if parsed_before.unsafe_reasons:
        return data, parsed_before, parsed_before, 0

    rendered, section_count = format_parsed(parsed_before)
    output_text = newline.join(rendered)
    if had_final_newline and rendered:
        output_text += newline
    output = encode_odf(output_text, encoding, bom)
    parsed_after = parse_lines(output_text.splitlines())
    return output, parsed_before, parsed_after, section_count


def atomic_write(path: Path, data: bytes) -> None:
    mode = path.stat().st_mode
    temporary_name = ""
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb", dir=path.parent, prefix=f".{path.name}.", delete=False
        ) as temporary:
            temporary.write(data)
            temporary.flush()
            os.fsync(temporary.fileno())
            temporary_name = temporary.name
        os.chmod(temporary_name, mode)
        os.replace(temporary_name, path)
    finally:
        if temporary_name and os.path.exists(temporary_name):
            os.unlink(temporary_name)


def odf_paths(root: Path) -> list[Path]:
    return sorted(
        (path for path in root.rglob("*") if path.is_file() and path.suffix.lower() == ".odf"),
        key=lambda path: str(path).lower(),
    )


def process_file(path: Path, root: Path, write: bool) -> FormatResult:
    data = path.read_bytes()
    relative = str(path.relative_to(root))
    try:
        output, before, after, section_count = format_bytes(data)
    except (UnicodeDecodeError, UnicodeEncodeError) as error:
        return FormatResult(
            path=relative,
            changed=False,
            safe=False,
            reason=f"encoding error: {error}",
            input_bytes=len(data),
            output_bytes=len(data),
        )

    assignments = sum(1 for entry in before.entries if entry.active)
    continuation_lines = sum(len(entry.continuation) for entry in before.entries)
    counts = collections.Counter(
        entry.key.lower()
        for entry in before.entries
        if entry.active and entry.key is not None
    )
    duplicate_keys = sum(1 for count in counts.values() if count > 1)

    if before.unsafe_reasons:
        return FormatResult(
            path=relative,
            changed=False,
            safe=False,
            reason="; ".join(before.unsafe_reasons),
            assignments=assignments,
            continuation_lines=continuation_lines,
            duplicate_keys=duplicate_keys,
            input_bytes=len(data),
            output_bytes=len(data),
        )

    if semantic_signature(before) != semantic_signature(after):
        return FormatResult(
            path=relative,
            changed=False,
            safe=False,
            reason="semantic signature changed after formatting",
            assignments=assignments,
            continuation_lines=continuation_lines,
            duplicate_keys=duplicate_keys,
            input_bytes=len(data),
            output_bytes=len(output),
        )

    second_output, second_before, second_after, _ = format_bytes(output)
    if second_before.unsafe_reasons or semantic_signature(second_before) != semantic_signature(second_after):
        return FormatResult(
            path=relative,
            changed=False,
            safe=False,
            reason="formatted output did not parse safely",
            assignments=assignments,
            continuation_lines=continuation_lines,
            duplicate_keys=duplicate_keys,
            input_bytes=len(data),
            output_bytes=len(output),
        )
    if second_output != output:
        return FormatResult(
            path=relative,
            changed=False,
            safe=False,
            reason="formatter is not idempotent for this file",
            assignments=assignments,
            continuation_lines=continuation_lines,
            duplicate_keys=duplicate_keys,
            input_bytes=len(data),
            output_bytes=len(output),
        )

    changed = output != data
    if write and changed:
        atomic_write(path, output)

    return FormatResult(
        path=relative,
        changed=changed,
        safe=True,
        assignments=assignments,
        continuation_lines=continuation_lines,
        section_count=section_count,
        duplicate_keys=duplicate_keys,
        input_bytes=len(data),
        output_bytes=len(output),
    )


def summary(results: list[FormatResult]) -> dict[str, int]:
    return {
        "files": len(results),
        "safe": sum(result.safe for result in results),
        "unsafe": sum(not result.safe for result in results),
        "changed": sum(result.changed for result in results),
        "unchanged": sum(not result.changed for result in results),
        "assignments": sum(result.assignments for result in results),
        "continuation_lines": sum(result.continuation_lines for result in results),
        "sections": sum(result.section_count for result in results),
        "files_with_duplicate_keys": sum(result.duplicate_keys > 0 for result in results),
        "input_bytes": sum(result.input_bytes for result in results),
        "output_bytes": sum(result.output_bytes for result in results),
    }


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="ODF file or directory to format")
    parser.add_argument(
        "--write",
        action="store_true",
        help="write validated changes atomically; default is a dry run",
    )
    parser.add_argument(
        "--report",
        type=Path,
        help="write a JSON report containing the summary and per-file results",
    )
    parser.add_argument(
        "--show-unsafe",
        action="store_true",
        help="print every file rejected by safety validation",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    root = arguments.root.resolve()
    if root.is_file():
        paths = [root]
        report_root = root.parent
    elif root.is_dir():
        paths = odf_paths(root)
        report_root = root
    else:
        print(f"error: path does not exist: {root}", file=sys.stderr)
        return 2

    results = [process_file(path, report_root, arguments.write) for path in paths]
    totals = summary(results)
    mode = "write" if arguments.write else "dry-run"
    print(f"ODF formatter ({mode})")
    for key, value in totals.items():
        print(f"{key}: {value}")

    unsafe = [result for result in results if not result.safe]
    if arguments.show_unsafe:
        for result in unsafe:
            print(f"unsafe: {result.path}: {result.reason}")

    if arguments.report:
        payload = {
            "mode": mode,
            "root": str(root),
            "summary": totals,
            "files": [dataclasses.asdict(result) for result in results],
        }
        arguments.report.parent.mkdir(parents=True, exist_ok=True)
        arguments.report.write_text(
            json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8"
        )

    return 1 if unsafe else 0


if __name__ == "__main__":
    raise SystemExit(main())
