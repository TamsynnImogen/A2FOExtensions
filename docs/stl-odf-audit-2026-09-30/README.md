# Star Trek: Legacy ODF comparison — 2026-09-30

## Result

The supplied STL corpus contains **1,604 distinct case-folded assignment keys** in **1,732 ODFs**. **678 keys across 535 lexical command families** have no active direct assignment in the supplied STA1, STA2 or FO4 mod corpora. The candidate catalogue retains **14,829 occurrences** with file and line, original spelling, complete values, section and nearby comments.

These are **research candidates**, not proven unsupported commands or an authenticated stock-Legacy command set. This STL folder mixes imported Legacy data, Armada definitions and local extension fixtures. One candidate, `logoFileNames`, also occurs in the shared Data corpus (63 files versus 51 STL files); it must not be treated as a wholly new Fleet Operations data command.

The input mod ODFs and extension runtime source were left unchanged. This work adds audit tooling, evidence and a link from the existing Legacy backlog; it implements no compatibility behavior.

## Reading the evidence

- [Candidate catalogue](candidates.md): every candidate command, representative value and clickable STL file/line, with shared Data and extension research notes.
- [Complete command matrix](command-matrix.md): all STL commands compared with STA1, STA2, FO4 and shared Data, including common commands.
- [Research index](research-index.json): every candidate occurrence and full value/comments, literal command-name references in current extension source, explicit indexed-module pointers and local fixture tags.
- [Raw candidates](candidates.json) and [raw comparison matrix](command-matrix.json): structured comparison data for future scripts.
- [Commented definitions](comments.json): 313 comment-only STL occurrences, plus all detected commented assignments. These do not count as active commands; multiple occurrences can name the same key.
- [Class-label matrix](classlabel-matrix.json): STL `classLabel` **values** compared separately, so a new class does not disappear simply because `classLabel` is a common key.
- [Input manifest](manifest.json): per-file SHA-256, size, encoding, direct includes and class labels, excluded paths, archive descriptors and parser anomalies.
- [Archive verification](archive-verification.json): package fingerprints, checker output and byte comparisons with loose files.
- [Embedded archive comparison](embedded-archive-comparison.json): additional historical package evidence, kept separate because loader activation is unverified.
- [Summary](summary.json): corpus and result totals.
- [Validation](validation.json): tool hashes, tests and independent source-reference checks.

## Corpus coverage

Counts include loose `.odf` files in the mod's `odf` and `addon` trees. Backup directories and nested mods are excluded; a filename beginning `old_` is retained. STA1 therefore includes five addon ODFs in addition to its 489-file ODF tree.

| Corpus | ODF files | Direct assignments | Distinct keys | Lexical families |
| --- | ---: | ---: | ---: | ---: |
| STL | 1,732 | 27,726 | 1,604 | 1,263 |
| STA1 Classic Mod | 494 | 7,983 | 1,799 | 1,453 |
| STA2 Classic Mod | 1,097 | 17,498 | 1,968 | 1,555 |
| Fleet Ops 4.0 | 9,364 | 384,513 | 3,051 | 2,220 |
| FO_ROOT | 1,696 | 82,699 | 2,428 | 1,772 |

The four requested roots are below `/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/`; the supplementary shared tree is `/home/tamsynn/NVMeData/Fleet Ops Roots/Data/odf/` (plus its addon tree if present). Exact absolute roots are in the manifest.

`STL/info.ini` identifies an “ST Legacy import” with `ParentMod="STA2 Classic Mod"` and `standalone=0`. STA1 also declares STA2 as parent; FO4 declares `standalone=1`. This audit compares the direct contents of each corpus independently. It does not flatten the mod inheritance chain or assert which shared definitions the live loader chooses.

The STA2 `ODF.fpq` contains 1,097 ODFs; all decoded bytes match the loose definitions. The shared Data `ODF.fpq` contains 1,696 ODFs, also all byte-identical to the loose tree, and two non-ODF members. One is an embedded `odf/system/ODF.fpq`: its 1,520 ODF members have no filenames absent from the shared loose tree, though 758 differ in content. Its only candidate-family match is `logoFileNames` (58 occurrences), which was already present in shared Data. It adds no otherwise unseen candidate family. Backup RAR archives are recorded but not used as active baselines.

## Useful starting points for later investigation

The group names below describe names and source comments, **not recovered engine semantics**. Every command listed belongs to the candidate catalogue. Open the full occurrence list before selecting a fixture; base definitions and overrides can differ.

| Area | Example candidate names | Representative STL definition | Next evidence needed |
| --- | --- | --- | --- |
| Targeting and combat movement | `hasTargetingBoost`, `boostModifier`, `chanceFlyBy`, `chanceCircle`, `chanceCloverleaf`, `chanceChase` | [odf/ships/class_artillery.odf:9](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_artillery.odf:9>) | Recover targeting and manoeuvre selection consumers and defaults. |
| Directional subsystem damage | `frontHullHitChance`, `backEngineHitChance`, `leftWeaponHitChance` | [odf/ships/craft.odf:82](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:82>) | Resolve weight interpretation and facing boundaries; craft and station values differ. |
| Shield behavior | `shieldFrontProportion`, `shieldBackProportion`, `shieldMaxAbsorption`, `shieldMinAbsorption` | [odf/ships/craft.odf:60](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:60>) | Compare Legacy-facing fields with the existing directional-shield module. |
| Ordinary weapon pools | `weaponsEnergyReplenishRate`, `weaponPoolDiminishRate`, `energyPerPhoton`, `photonReplenishRate` | [odf/ships/B_class_battleship.odf:10](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_class_battleship.odf:10>) | Trace pool ownership, charging and firing authorization. |
| Special timing | `specialDurationTimer`, `specialCooldownTimer` | [odf/special_weapons/gcloak.odf:43](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/gcloak.odf:43>) | Recover timing units and interactions with cloak/energy. |
| Collision and movement physics | `avoidanceUse`, `avoidanceMaxLookaheadTime`, `bankAlpha`, `bankOmega` | [odf/other/flyingasteroid.odf:108](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/flyingasteroid.odf:108>) | Locate physics consumers; separate simulation changes from presentation. |
| Damage effects and break nodes | `damageParticleSystems`, `EngineDestructionEffect`, `engineBreakNodes`, `insideHullDamageMaterial` | [odf/ships/B_tng_multicube.odf:141](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_tng_multicube.odf:141>) | Map effects and break behavior to the current M3D/SOD paths. |
| Identity and captains | `hasCaptain`, `possibleCaptainNames`, `specificCaptain` | [odf/ships/craft.odf:37](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:37>) | Check existing A2FOCraftIdentity parsing before adding another path. |
| Era, upgrades and production | `startYear`, `endYear`, `upgrade0`, `commandPointCost`, `productionPointCost` | [odf/other/flyingasteroid.odf:23](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/flyingasteroid.odf:23>) | Recover availability and upgrade grammar; compare existing cap/resource systems. |
| Materials and particles | `bumpTextureName`, `specularTextureName`, `shaderName`, `enableTextureAnimation` | [odf/stations/bajor.odf:8](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/bajor.odf:8>) | Locate renderer consumers and format dependencies before adapting data. |

For example, `frontHullHitChance` is `100` in `odf/ships/craft.odf` but `.2` in `odf/stations/station.odf`. The comments alone do not establish a single percentage or weighting rule. The audit retains both values.

75 candidate keys have literal-name references in current extension source. Indexed animation, swarm, breeder and squad families have additional module pointers. A literal match can be registration, implementation, a label or unrelated text; it is a search lead, not proof of active support. Eleven candidate keys occur in the explicitly identified `nox_*` breeder or `fsquad.odf` fixtures. The entire candidate list remains available so locally adapted and imported data are not silently removed.

### Additional class-label values

| Value absent from three baseline corpora | STL files | Example source |
| --- | ---: | --- |
| `breeder` | 1 | [odf/stations/nox_breeder_base.odf:4](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/nox_breeder_base.odf:4>) |
| `explosion` | 3 | [odf/Explosions/xArtillery.odf:2](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/xArtillery.odf:2>) |
| `long_range_sensor` | 2 | [odf/special_weapons/long_range_sensor.odf:46](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/long_range_sensor.odf:46>) |
| `nebula_set` | 2 | [odf/other/nebulaset.odf:6](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/nebulaset.odf:6>) |
| `resource` | 3 | [odf/other/ResourceLatinum.odf:10](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/ResourceLatinum.odf:10>) |
| `shockwave` | 3 | [odf/Explosions/swArtillery.odf:2](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/swArtillery.odf:2>) |
| `squadron` | 1 | [odf/squads/fsquad.odf:1](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/squads/fsquad.odf:1>) |
| `transwarpportal` | 1 | [odf/other/transwarp_portal.odf:27](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/transwarp_portal.odf:27>) |
| `turret` | 1 | [odf/ships/fcruise1turr.odf:3](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/fcruise1turr.odf:3>) |
| `warp` | 1 | [odf/special_weapons/gwarp.odf:37](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/gwarp.odf:37>) |

`breeder` and `squadron` have local extension fixture provenance. The remaining values also require stock binary/parser checks; data absence is not evidence that a native class is unavailable.

## Interpretation and limits

- The comparison uses active-looking `identifier = value` text. `//`, `/* ... */` and hash-prefixed comments are excluded; INI sections and include directives are recorded separately. Multiline values and duplicate definitions are retained. Runtime precedence, include expansion, value types and units are not reconstructed.
- Case folding and lexical family grouping reduce casing/index noise; they do not prove parser case insensitivity or alias equivalence. Trailing number runs are grouped, along with explicit embedded indices for `swarm`, `replacement` and `weapon`. Other numbered forms remain separate. Same-spelled keys in different sections or object classes may mean different things.
- Source-file and line attribution is verified against this snapshot. Original distribution provenance cannot be recovered from the mixed mod folder alone. None of these inputs is asserted to be a pristine retail corpus.
- STL has **35 recorded parser anomalies**, including slash-prefixed assignments, a spaced `engine BreakNodes` key, missing equals signs, unterminated comments and quotes. These remain visible in the manifest. At an unmistakable new assignment/section/include after an unclosed quote, the scanner recovers and marks the malformed preceding value with `parse_status`; this is an audit heuristic, not a claim about the engine parser. Unterminated block comments remain comments.
- Absence in ODF data cannot establish lack of native support. Later work must find a parser and runtime consumer in the relevant pinned binaries, then verify actual behavior. Existing hooks and data commands should be checked before adding duplicates.

## Reproduce and validate

From `/home/tamsynn/A2FOHookExtensions`:

```sh
python3 -m unittest discover -s tests -p stl_odf_audit_test.py -v
python3 tools/audit_stl_odf_commands.py --output docs/stl-odf-audit-2026-09-30
python3 tools/render_stl_odf_audit.py --audit docs/stl-odf-audit-2026-09-30
```

These regenerate the JSON snapshot and the two Markdown catalogues/research index. The README is curated; the archive verification files are separate evidence and are not recreated by those commands. Do not treat a later rescan as the original pinned snapshot.

The package checks used existing `src/fpq.rs`, `src/path_case.rs`, `src/bin/fpq_check.rs` and `src/bin/fpq_extract.rs` from `/home/tamsynn/OpenSourced-Space-RTS`, with source hashes recorded in the archive report. They were compiled offline in an isolated `/tmp/stl-odf-fpq-audit-20260930` crate using cached `anyhow=1.0.104`; the other project's source/build tree was not changed. The decoder checks passed 22 tests, with three private-corpus tests skipped; all three actual packages were decoded separately. For future archive refreshes, use those headless tools and compare extracted relative-path hashes with the loose tree again.

Seven scanner regression tests passed. Independent review verified all 14,829 candidate source lines and 116 literal extension references; all 14,383 live input-file hashes match the manifest, and the Markdown links resolve. These checks verify extraction and report integrity. They provide no in-game compatibility acceptance. No game launch, DLL build/deployment or GUI automation was performed.
