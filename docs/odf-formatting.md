# ODF Formatting

`tools/format_odfs.py` reformats Armada and Fleet Operations ODF files without
changing their parsed command values.

It was created for the loose Fleet Operations 4.0 ODF corpus, whose generated
files contain almost no whitespace or explanatory structure. Stock Armada 1
and Armada 2 ODFs informed the initial grouping. The second pass follows the
Fleet Operations Guide's published [ship, weapon, ordnance, and faction ODF
templates](http://guide.fleetops.net/guide/modding/tutorials/templates), while
keeping native runtime identity explicit at the end.

## Safety Rules

The formatter:

- keeps every assignment together with all following continuation/table rows;
- preserves the relative order of repeated assignments with the same
  case-insensitive key, retaining last-value-wins behaviour;
- preserves values, inline syntax, disabled assignments, and existing comments;
- preserves UTF-8 or Windows-1252 encoding, an existing UTF-8 BOM, CRLF/LF line
  endings, and the presence or absence of the final newline;
- writes changed files atomically;
- rejects assignment-like syntax it cannot recognize;
- compares per-key semantic signatures before accepting a result;
- formats each result a second time and rejects non-idempotent output.

The tool does not invent command-by-command descriptions. It adds only compact
semantic section headings, avoiding misleading explanations for undocumented
Fleet Operations commands.

## Fleet Operations Profiles

The formatter detects six layouts from their commands and `classlabel` values:

- GameObject (vessels, stations, pods, and related map objects)
- Weapon
- Ordnance
- Faction
- Interface or command configuration
- Generic configuration

Each layout renders the common categories in the order most appropriate to its
Fleet Operations template. Commands documented by a template also follow the
template's field order inside their section. Numbered arrays such as `weapon2`,
`standardUnits4`, and `preloadUnits10` follow their representative template
field naturally. Undocumented FO commands use conservative name/context rules
and retain their original order among other non-template commands.

## Sections

Commands are stably grouped into:

1. File identity and inheritance
2. Display and localization
3. Construction and costs
4. Hull, shields, crew, and energy
5. Subsystems, damage, and repair
6. Movement and physics
7. Weapons and ordnance
8. Accuracy, hardpoints, and targeting
9. Art, animation, and effects
10. Audio and events
11. Artificial intelligence and tactical behavior
12. Interface and controls
13. Progression and variants
14. Resource handling and recycling
15. Faction rules and starting units
16. Additional parameters
17. Capabilities and object behavior
18. Runtime class

Unrecognized but valid commands are preserved under `Additional parameters`
rather than guessed into a potentially misleading section.

Files with fewer than four active assignments are normalized without section
headings, preventing comments from overwhelming tiny inheritance or marker
ODFs.

## Usage

Dry run:

```bash
python3 tools/format_odfs.py /path/to/odf --show-unsafe
```

Dry run with a machine-readable report:

```bash
python3 tools/format_odfs.py /path/to/odf \
  --report /tmp/odf-format-report.json --show-unsafe
```

Write changes only after the dry run reports zero unsafe files:

```bash
python3 tools/format_odfs.py /path/to/odf --write \
  --report /path/to/odf-format-report.json --show-unsafe
```

Run the focused tests with:

```bash
python3 -m unittest tests/test_odf_formatter.py
```

After a corpus rewrite, run another dry pass. A successful idempotence check
reports zero changed files.

## Fleet Operations 4.0 Baseline

The pre-write audit on 2026-08-05 found:

| Metric | Count |
| --- | ---: |
| ODF files | 9,364 |
| Active assignments | 384,512 |
| Continuation/table rows | 3,648,475 |
| Files containing repeated keys | 6 |
| Files expected to change | 9,297 |
| Files already minimal/unchanged | 67 |
| Unsafe files | 0 |

The source corpus was 126,734,250 bytes. The formatted corpus was predicted to
be 140,014,040 bytes due solely to section comments, blank lines, and normalized
indentation.

## Fleet Operations Template Pass

The template-guided pass on 2026-08-05 detected:

| Profile | Files |
| --- | ---: |
| GameObject | 2,513 |
| Weapon | 3,609 |
| Ordnance | 2,739 |
| Faction | 9 |
| Interface/command configuration | 107 |
| Generic configuration | 387 |

All 2,048 command names present in the corpus are assigned to a reviewed
section. The fallback remains active for future commands.

| Metric | Count |
| --- | ---: |
| Files checked | 9,364 |
| Files changed by the template pass | 9,271 |
| Files unchanged | 93 |
| Unsafe files | 0 |
| Semantic mismatches against the pre-pass backup | 0 |
| Sections after the template pass | 75,289 |

The template-guided corpus is 140,472,762 bytes. A post-write dry run reports
zero further changes.
