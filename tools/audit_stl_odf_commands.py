#!/usr/bin/env python3
"""Static, provenance-preserving comparison of loose Armada ODF text.

Families are lexical heuristics, not engine command or inheritance semantics.
"""
from __future__ import annotations

import argparse
from collections import Counter, defaultdict
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re

CORPORA = ("STL", "STA1 Classic Mod", "STA2 Classic Mod", "Fleet Ops 4.0")
BASELINES = CORPORA[1:]
KEY = r"[A-Za-z_][\w.\[\]:-]*"
ASSIGNMENT = re.compile(rf"^\s*({KEY})\s*=\s*(.*)$", re.ASCII)
COMMENT_ASSIGNMENT = re.compile(rf"^\s*({KEY})\s*=\s*(.*)$", re.ASCII)
INCLUDE = re.compile(r"^\s*#?include\b\s*(.*)$", re.I)
SECTION = re.compile(r"^\s*\[([^]\r\n]+)\]\s*$")
ARCHIVE_SUFFIXES = {".fpq", ".zip", ".7z", ".rar", ".tar", ".gz"}
BACKUP_DIR = re.compile(r"^(?:backup|backups|bak|archive|archives)(?:[-_. ].*)?$", re.I)
BACKUP_FILE_TOKEN = re.compile(r"(?:^|[-_. ])(?:backup|backups|bak)(?=[-_. ]|$)", re.I)
# Only these known indexed forms collapse digits embedded before a later suffix.
INDEXED_PREFIXES = ("swarm", "replacement", "weapon")


def family(key: str) -> str:
    folded = key.casefold()
    folded = re.sub(r"\d+$", "<N>", folded)
    if folded.endswith("<N>"):
        return folded
    for prefix in INDEXED_PREFIXES:
        if folded.startswith(prefix):
            return re.sub(r"(?<=\D)\d+(?=\D)", "<N>", folded)
    return folded


def decode_odf(data: bytes) -> tuple[str, str]:
    for encoding in ("utf-8-sig", "cp1252"):
        try:
            return data.decode(encoding), encoding
        except UnicodeDecodeError:
            pass
    return data.decode("latin-1"), "latin-1-fallback"


def canonical_classlabel(value: str) -> str | None:
    """Extract only a leading quoted token or identifier for lexical comparison."""
    match = re.match(r'^\s*(?:"([^"\r\n]+)"|([A-Za-z_][\w.-]*))', value, re.ASCII)
    return (match[1] or match[2]).casefold() if match else None


def split_comments(line: str, in_block: bool = False, in_quote: bool = False) -> tuple[str, list[str], bool, bool]:
    """Remove C-style comments outside double quotes, preserving comment text."""
    code, comments = [], []
    i = 0
    while i < len(line):
        if in_block:
            end = line.find("*/", i)
            if end < 0:
                comments.append(line[i:])
                return "".join(code), comments, True, in_quote
            comments.append(line[i:end])
            i = end + 2
            in_block = False
            continue
        if not in_quote and line.startswith("//", i):
            comments.append(line[i + 2:])
            break
        if not in_quote and line.startswith("/*", i):
            in_block = True
            i += 2
            continue
        char = line[i]
        if char == '"' and (i == 0 or (len(line[:i]) - len(line[:i].rstrip("\\"))) % 2 == 0):
            in_quote = not in_quote
        code.append(char)
        i += 1
    return "".join(code), comments, in_block, in_quote


def parse_odf(text: str, source: str) -> dict:
    assignments, commented, includes, anomalies = [], [], [], []
    section, pending_comments, in_block = None, [], False
    current = None
    continuation_quote = False
    bracket_depth = 0
    for line_number, raw in enumerate(text.splitlines(), 1):
        recovered = bool(continuation_quote and not in_block and
                         (ASSIGNMENT.match(raw) or SECTION.match(raw) or INCLUDE.match(raw)) and
                         not raw.lstrip().startswith(("//", "/*")))
        if recovered:
            anomalies.append({"source": source, "line": current["line"], "text": current["key"],
                              "reason": "recovered_after_unclosed_quote", "recovery_line": line_number})
            current["parse_status"] = "unclosed_quote_recovered"
            continuation_quote = False
            bracket_depth = 0
        code, comments, in_block, quote_open = split_comments(raw, in_block, continuation_quote)
        stripped = code.strip()
        for comment in comments:
            candidate = COMMENT_ASSIGNMENT.match(comment.strip())
            if candidate:
                commented.append({"source": source, "line": line_number, "key": candidate[1],
                                  "value": candidate[2].strip(), "section": section, "comment": comment.strip()})
        section_match = SECTION.match(code) if not continuation_quote and bracket_depth <= 0 else None
        if current and not section_match and (continuation_quote or bracket_depth > 0 or
                        (stripped and re.match(r'^[\d+.,;(){}\[\]"-]', stripped) and not ASSIGNMENT.match(code))):
            if stripped:
                current["value"] += "\n" + stripped
                current["end_line"] = line_number
                current["inline_comments"].extend(c.strip() for c in comments if c.strip())
                continuation_quote = quote_open
                bracket_depth += stripped.count("[") + stripped.count("{") - stripped.count("]") - stripped.count("}")
            continue
        if not stripped:
            if comments:
                pending_comments.extend(c.strip() for c in comments if c.strip())
            elif not in_block:
                pending_comments = []
            continue
        if section_match:
            section = section_match[1].strip()
            pending_comments = []
            current = None
            continue
        include_match = INCLUDE.match(code)
        if include_match and (stripped.startswith("#") or "=" not in code):
            includes.append({"source": source, "line": line_number, "value": include_match[1].strip(),
                             "section": section})
            pending_comments = []
            current = None
            continue
        if stripped.startswith("#"):
            pending_comments.append(stripped)
            continue
        match = ASSIGNMENT.match(code)
        if match:
            key, value = match[1], match[2].strip()
            current = {"source": source, "line": line_number, "end_line": line_number,
                       "key": key, "family": family(key), "value": value, "section": section,
                       "preceding_comments": pending_comments[:], "inline_comments": [c.strip() for c in comments if c.strip()],
                       "parse_status": "after_unclosed_quote" if recovered else "parsed"}
            assignments.append(current)
            pending_comments = []
            continuation_quote = quote_open
            bracket_depth = value.count("[") + value.count("{") - value.count("]") - value.count("}")
            continue
        reason = "malformed_assignment" if "=" in code else "unknown_syntax"
        anomalies.append({"source": source, "line": line_number, "text": stripped, "reason": reason})
        pending_comments = []
        current = None
    if in_block:
        anomalies.append({"source": source, "line": len(text.splitlines()), "text": "", "reason": "unclosed_block_comment"})
    for item in assignments:
        if item["value"].count('"') % 2:
            anomalies.append({"source": source, "line": item["line"], "text": item["key"], "reason": "unclosed_quote"})
    classlabels = [{"source": a["source"], "line": a["line"], "value": a["value"],
                    "canonical_value": canonical_classlabel(a["value"]), "section": a["section"]}
                   for a in assignments if a["key"].casefold() == "classlabel"]
    return {"assignments": assignments, "commented": commented, "includes": includes,
            "anomalies": anomalies, "classlabels": classlabels}


def excluded_directory(name: str) -> str | None:
    if name.casefold() == "mods":
        return "nested_mods"
    if BACKUP_DIR.match(name):
        return "backup_directory"
    return None


def is_backup_file(path: Path) -> bool:
    return path.suffix.casefold() == ".bak" or (path.suffix.casefold() in ARCHIVE_SUFFIXES and
                                                  bool(BACKUP_FILE_TOKEN.search(path.stem)))


def scan_corpus(root: Path, supplemental: bool = False) -> dict:
    files, assignments, commented, classlabels, anomalies, includes, archives, excluded_paths = [], [], [], [], [], [], [], []
    trees = [p for p in root.iterdir() if p.is_dir() and p.name.casefold() in {"odf", "addon"}] if root.is_dir() else []
    if root.is_dir():
        for path in sorted(root.iterdir(), key=lambda p: p.name.casefold()):
            if path.is_file() and path.suffix.casefold() in ARCHIVE_SUFFIXES:
                if is_backup_file(path):
                    excluded_paths.append({"path": path.name, "bytes": path.stat().st_size,
                                           "reason": "backup_archive", "kind": "excluded_archive_descriptor"})
                else:
                    archives.append({"path": path.name, "bytes": path.stat().st_size, "kind": "archive_descriptor_only"})
    for tree in sorted(trees, key=lambda p: str(p).casefold()):
        for directory, dirs, names in os.walk(tree):
            base = Path(directory)
            kept = []
            for name in dirs:
                reason = excluded_directory(name)
                if reason:
                    excluded_paths.append({"path": (base / name).relative_to(root).as_posix(), "reason": reason})
                else:
                    kept.append(name)
            dirs[:] = sorted(kept, key=str.casefold)
            for name in sorted(names, key=str.casefold):
                path = base / name
                rel = path.relative_to(root).as_posix()
                if not path.is_file():
                    continue
                if is_backup_file(path):
                    excluded_paths.append({"path": rel, "bytes": path.stat().st_size,
                                           "reason": "backup_file", "kind": "excluded_file_descriptor"})
                    continue
                if path.suffix.casefold() in ARCHIVE_SUFFIXES:
                    archives.append({"path": rel, "bytes": path.stat().st_size, "kind": "archive_descriptor_only"})
                    continue
                if path.suffix.casefold() != ".odf":
                    continue
                data = path.read_bytes()
                decoded, encoding = decode_odf(data)
                parsed = parse_odf(decoded, rel)
                files.append({"path": rel, "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data),
                              "encoding": encoding, "assignments": len(parsed["assignments"]),
                              "includes": parsed["includes"], "classlabels": parsed["classlabels"],
                              "anomaly_count": len(parsed["anomalies"])})
                for key, target in (("assignments", assignments), ("commented", commented),
                                    ("classlabels", classlabels), ("anomalies", anomalies), ("includes", includes)):
                    target.extend(parsed[key])
    return {"root": str(root), "supplemental": supplemental, "trees": [str(p) for p in trees],
            "files": files, "assignments": assignments, "commented": commented,
            "classlabels": classlabels, "anomalies": anomalies, "includes": includes,
            "archives": archives, "excluded_paths": excluded_paths}


def count_for(items: list[dict], key: str) -> dict:
    matches = [a for a in items if a["key"].casefold() == key]
    return {"assignment_count": len(matches), "file_count": len({a["source"] for a in matches}),
            "spellings": sorted({a["key"] for a in matches}), "examples": matches[:3]}


def build_reports(corpora: dict[str, dict]) -> dict[str, dict]:
    names = tuple(corpora)
    exact_index = {}
    family_index = {}
    for name in names:
        exact, families = defaultdict(list), defaultdict(list)
        for assignment in corpora[name]["assignments"]:
            exact[assignment["key"].casefold()].append(assignment)
            families[assignment["family"]].append(assignment)
        exact_index[name], family_index[name] = exact, families
    exact_keys = sorted(exact_index["STL"])
    baseline_families = set().union(*(family_index[name] for name in BASELINES))
    rows = []
    for key in exact_keys:
        key_family = family(key)
        row = {"key": key, "family": key_family,
               "corpora": {name: count_for(exact_index[name].get(key, []), key) for name in names},
               "family_comparison": {name: {"assignment_count": len(family_index[name].get(key_family, [])),
                                            "file_count": len({a["source"] for a in family_index[name].get(key_family, [])})}
                                     for name in names}}
        rows.append(row)
    candidate_keys = {r["key"] for r in rows if r["family"] not in baseline_families}
    candidates = [a for a in corpora["STL"]["assignments"] if a["key"].casefold() in candidate_keys]
    comment_only = [a for a in corpora["STL"]["commented"]
                    if a["key"].casefold() not in exact_index["STL"]]
    comment_only.sort(key=lambda a: (a["source"].casefold(), a["line"], a["key"].casefold()))
    class_index = {}
    for name in names:
        index = defaultdict(list)
        for label in corpora[name]["classlabels"]:
            if label["canonical_value"] is not None:
                index[label["canonical_value"]].append(label)
        class_index[name] = index
    class_rows = []
    for value in sorted(class_index["STL"]):
        row = {"value": value, "corpora": {}}
        for name in names:
            matches = class_index[name].get(value, [])
            row["corpora"][name] = {"assignment_count": len(matches), "file_count": len({a["source"] for a in matches}),
                                      "examples": matches[:3]}
        row["absent_from_baselines"] = all(row["corpora"][name]["assignment_count"] == 0 for name in BASELINES)
        class_rows.append(row)
    manifest = {"created_utc": datetime.now(timezone.utc).isoformat(),
                "methodology": "Static loose ODF assignment comparison. No include resolution, archive decoding, inheritance, overlay precedence, or runtime semantics. Family matching is a lexical heuristic.",
                "corpora": {name: {k: value[k] for k in ("root", "supplemental", "trees", "files", "archives", "excluded_paths", "anomalies")}
                            for name, value in corpora.items()}}
    summary = {"corpora": {name: {"files": len(v["files"]), "assignments": len(v["assignments"]),
                                    "exact_keys": len(exact_index[name]), "families": len(family_index[name]),
                                    "commented_assignments": len(v["commented"]), "classlabels": len(v["classlabels"]),
                                    "anomalies": len(v["anomalies"]), "archives": len(v["archives"])}
                           for name, v in corpora.items()},
               "stl_exact_keys": len(exact_keys), "candidate_occurrences": len(candidates),
               "candidate_keys": len(candidate_keys),
               "candidate_families": len({family(key) for key in candidate_keys}),
               "comment_only_occurrences": len(comment_only),
               "stl_classlabels_absent_baselines": sum(r["absent_from_baselines"] for r in class_rows)}
    return {"manifest.json": manifest, "command-matrix.json": {"rows": rows},
            "candidates.json": {"occurrences": candidates},
            "comments.json": {"comment_only_occurrences": comment_only,
                              "all_stl_commented_assignments": corpora["STL"]["commented"]},
            "classlabel-matrix.json": {"rows": class_rows}, "summary.json": summary}


def main() -> None:
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument("--mods-root", type=Path, default=Path("/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods"))
    cli.add_argument("--output", type=Path, required=True)
    args = cli.parse_args()
    if not args.mods_root.is_dir():
        cli.error(f"mods root is not a directory: {args.mods_root}")
    corpora = {name: scan_corpus(args.mods_root / name) for name in CORPORA}
    corpora["FO_ROOT"] = scan_corpus(args.mods_root.parent, supplemental=True)
    reports = build_reports(corpora)
    args.output.mkdir(parents=True, exist_ok=True)
    for name, content in reports.items():
        (args.output / name).write_text(json.dumps(content, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(json.dumps(reports["summary.json"], indent=2))


if __name__ == "__main__":
    main()
