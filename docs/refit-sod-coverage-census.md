# Refit replacement coverage: static SOD census

Created 2026-11-06 by Claude, alongside Codex's ODF light-control work. Read-only analysis: it involved
no Core/Renderer source change, no build of an attested candidate, no game run and no live-install write.
It covers checklist §6 item 1 ("Inventory representative FO 4.0 … SODs … Record supported/unsupported
cases"), for **static** gates only.

## What was measured

`tools/refit_sod_census/` is a standalone Rust crate, located outside `core/ renderer/ rust/ sdk/` so that
it stays out of the Core source attestation. It links the **same** `rust/a2fo_native11_model` policy parser
and `a2fmt` SOD reader that Core uses, and replays the static gates of
`renderer/refit/mesh_scope.cpp`:

1. SOD version 1.7–1.93 (`mesh_model_format_supported2`)
2. whole-model acceptance by `a2fo_native11_model` (`valid_model` + emission limits)
3. exactly one material group per mesh node (`native_group_count!=1`; the native group count is assumed to equal the SOD face-group count)
4. 1–21845 triangles
5. DrawFlags ⊆ two_sided, i.e. blend mode `opaque`/`default`

**Runtime gates are not evaluated:** legacy route 1 vs MeshVB, pass 0, live pose/visibility, LOD and
damage selection, canonical material/texture rechecks and geometry parity. These figures are an
**upper bound** on what the current route can replace.

Input: the effective FO4 Nexus set. The roots are `FO4 Nexus/sod` (absent), then `Fleet Ops 4.0/sod`, then
base `Data/sod`. File names are case-insensitive and the first root to hold a name wins. The result is
927 SODs, with 492 shadowed and 2,332 drawable mesh nodes holding 978,367 triangles.

Evidence: `build/refit-sod-census/20261106T170036Z/`

| File | SHA-256 |
|---|---|
| census.jsonl | `2889dc9378ed08fbdea56f52e02dee22823d8421273c62b783a632482d8f08d0` |
| coverage.json | `b9df7fa9ba8e1d80795c53d1085a344889e8692ad3f03c9acc603c219956ea86` |
| REPORT.md | `91b05c68fd34b4a8ab07cda9ee12b1ee80be1a34ba53c07720ecc24555535801` |

The census was produced before the `--why` diagnostic mode was added to `main.rs`; the census code path is
unchanged.

Reproduce:

```sh
(cd tools/refit_sod_census && CARGO_TARGET_DIR=$PWD/target cargo build --release --offline)
R="/home/tamsynn/NVMeData/Fleet Ops Roots/Data"; OUT=build/refit-sod-census/$(date -u +%Y%m%dT%H%M%SZ); mkdir -p $OUT
tools/refit_sod_census/target/release/refit_sod_census "$R/Mods/FO4 Nexus/sod" "$R/Mods/Fleet Ops 4.0/sod" "$R/sod" > $OUT/census.jsonl
python3 tools/refit_sod_census/summarise.py $OUT/census.jsonl
tools/refit_sod_census/target/release/refit_sod_census --why PATH.sod        # per-check policy diagnosis
tools/refit_sod_census/target/release/refit_sod_census --materials PATH.sod  # dump lighting material table
```

## Results

| First blocking gate | Meshes % | Triangles % |
|---|---:|---:|
| **statically eligible** | 52.1 | **72.2** |
| multi_group (always exactly 2 groups, all opaque) | 6.0 | 13.6 |
| parser_policy (whole model rejected) | 2.8 | 8.2 |
| blend_mode (additive 574, alpha 318, wormhole, atmosphere…) | 38.8 | 6.0 |
| sod_version (1.2–1.6 weapon/light SODs) | 0.4 | 0.0 |
| triangle_limit | 0 | 0 |

The effect of lifting each gate on statically eligible triangles:

| Gate lifted | Eligible triangles |
|---|---|
| none (today) | 72.2% |
| multi-group | 85.8% |
| multi-group + blend modes | 91.8% |
| multi-group + blend modes + parser policy | 100% |

## Findings

1. **Bump/self-illum is not a coverage blocker.** Of the eligible triangles, 93% are on the dot3 bump
   path and 95% are self-illuminated. The live-qualified Mediterranean submission is itself
   `Box01`/`Box02`: 80 triangles, opaque, bump, self-illuminated. That mesh has already passed the runtime
   route-1/pass-0 checks. The static figure is therefore not undermined by the bump path. MeshVB-route
   frequency is still unmeasured, so record per-route counts in a live run.
2. **Two-group meshes are the best next coverage step (+13.6% of triangles).** All 162 multi-group
   meshes have exactly 2 groups and are all opaque. Rerun `20261106T231042Z` added per-group material
   names; its coverage figures are identical to the first run. Every one of these meshes is a **base
   material plus its `team_*` variant**:

   | Material pair | Meshes | Triangles |
   |---|---:|---:|
   | `basic_shaderbump` + `team_basic_shaderbump` | 135 | 141,868 |
   | `basic_federation` + `team_basic_federation` | 12 | 13,117 |
   | `basic_klingon` + `team_basic_federation` | 11 | 10,457 |
   | `basic_romulan` + `team_basic_federation` | 3 | 1,236 |

   The team group is small: the median is 2.1% of the mesh's faces, and the 90th percentile is 47.5%.
   Both groups are self-illuminated wherever both are defined. So multi-group support is effectively
   **team-colour support**: per-group draw ranges, with the team group consuming the native team colour
   (`material+0x45` / Storm team presentation, already sampled for ODF freshness), plus the existing
   base/emissive rule per group. Lifting this gate also helps finding 3, because most undefined
   materials are `team_*` names.
   **Update 2026-11-06:** this is implemented behind opt-in `[Refit]TwoGroupMeshes=1` and qualified on
   private Vulkan with white/black controls. See [refit-two-group-meshes.md](refit-two-group-meshes.md).
3. **Named ships are rejected as whole models by undefined lighting materials (8.2%).** These are
   `fed_defiant`, `kli_vorcha`, `kli_kbeajq(r6)`, `rom_saeihr`, `rom_tavara`, `rom_cehlaer`,
   `dom_s_workbee`, `nbre_cruiserd`, and the Antares/Eraudi yards. Their groups name
   `team_basic_shaderbump`, `basic_shaderbump` or `team_basic_federation`, but those materials are not
   defined in the file. They are also absent from loose text files and `ODF.fpq`, which holds ODFs only,
   and from the global lighting palette `Material.SOD`. Both the base and FO 4.0 palettes define the same
   24 `base_*`/`glow_*`/`warp_*`/`impulse_fed`/`bussard_fed`/`team_Bglow` materials and nothing else.
   No ordinary SOD relies on the palette. Every whole-model material rejection is one of these three
   undefined names.

   **What the engine image shows.** This is static evidence from pinned `ArmadaL.exe`, SHA-256 `4300ab71…`:
   - The Storm3D database constructor at `0x62b160` builds a texture material `"Global"` (`0x72c13c`)
     at `db+0x44`.
   - The same constructor builds a **built-in `ST3D_LightingMaterial` named `"default"`**
     (`0x72c144`) at `db+0xc`. It is created by ctor `0x6304f0` (vtable `0x6bc9a4`) from call site
     `0x62b200`, and its colour components are initialised to 1.0f.
   - The palette is loaded separately at `0x53e595`: `0x62cf10(db, "material", 1.0f)`, with the error
     string at `0x7118b8`.
   - Other `ST3D_LightingMaterial` constructors are `0x630600` (10 call sites in game code) and
     `0x6306e0`. The only `ST3D_SelfIlluminatingMaterial` vtable writer is at `0x62b6a1`.

   **Resolved statically (2026-11-07).** Lighting materials form one **process-wide, first-come
   namespace**:
   - `Find(name, classIndex)` (`0x620750`) walks the per-class list at `container + (idx*3+0x12)*4`
     using `NamedObject::NameIs` (`0x620fd0`, `_stricmp`). It returns **NULL on a miss**; there is no
     fallback inside it.
   - The SOD material-table loader (`0x63f618`) creates a material only if
     `Find(name, 1)` in the `Global` container (`db+0x44`) misses. If the name already exists, the
     file's own definition is **silently ignored**. It calls `ST3D_LightingMaterial` ctor `0x630600`,
     which registers into `Global` via `0x620270`.
   - The mesh-group loader resolves each group with `Find(name, 1)` at `0x6468b5`, in `Global` or a
     per-database container depending on the check at `0x6468a0`. It stores the result in the
     **native group record `+0x14`**, the field Core's `material_current` reads. **On NULL it stores
     `db+0xc`, the built-in `"default"`** (`0x6468c1–0x6468c9`).
   - Built-in `default` (ctor `0x6304f0`, `0x62b200`): diffuse (1,1,1), specular (0,1,0), power 2.0,
     model 1, not self-illuminating, ambient copied from engine globals `0x7a8f98…`. Its team flag is
     `strnicmp(name,"team",4)==0`, the same prefix rule Core uses.

   So `fed_defiant`'s `team_basic_shaderbump` resolves to **whichever SOD loaded earlier defined
   it**, for example any of the 99 files that define it. It falls back to the built-in `default` only
   if no loaded SOD defines it. The effective material is load-order dependent.

   **Implication for a fix.** Mapping an undefined name to a fixed "engine default" would be wrong
   whenever another SOD supplied the name. A correct design reads the **native** group material
   (`group+0x14`: name, parameters, class) at preparation and draw. It shades with those parameters
   rather than the file's, which needs a per-part material override in the renderer ABI. It accepts
   the built-in `default` only when the native pointer *is* `db+0xc`. This is ABI and Core work and
   was not started here.
4. **Planets are rejected whole by the `atmosphere` blend mode.** That is 13 `map_planet_class*` files,
   whose opaque planet bodies are blocked along with the atmosphere shell. `rom_generix_spcp` uses
   `opaque002`. Per-mesh rather than whole-model rejection for unknown modes would unlock the bodies
   without inventing a blend mapping. This is a policy decision for the shared parser, because the old
   Native11 path uses the same parser.
5. **Blend modes are most meshes but few triangles.** 39% of meshes are additive/alpha (engine glows,
   windows, markers) but they hold only 6% of triangles. They also matter visually, and transparent
   sorting is a separate gate.
6. **Non-mesh content is never replaced by the per-mesh API:** 5,148 sprite nodes and 36 emitters. SOD
   lights: none. Shields (`shield_*`, 25k triangles) and `effect_*` are 0–2% eligible.
7. Of the 927 models, 186 are fully eligible, 98 partly and 522 not at all. Most of the 522 are small
   effect, weapon or blend-only SODs. Borg (98%), Species 8472 (99%) and Cardassian, DS9 and support
   ships (100%) are nearly fully covered. Federation (61%), Klingon (58%) and Dominion (59%) are pulled
   down by findings 2 and 3.

8. **Load-order-dependent materials put about 13% of "eligible" triangles at risk.** This follows
   from the namespace above. Core's `material_current` compares the native material against the
   SOD file's *own* definition with exact `memcmp`. Across the 927 effective SODs, 149 lighting
   material names are defined, 66 of them in more than one file and **21 with conflicting
   parameters** (counting every field `material_current` compares). Of the statically eligible
   triangles, **73.7%** use a conflicted name, and **12.7% (89,718)** come from a file whose
   definition is not the majority variant. These are refused whenever the majority definition loads
   first. Borg is the most exposed family (37k triangles), then `map_*` (17k), Federation (9k) and
   Romulan (7k).
   - `basic_borg`: 89 files have diffuse 1.0 and 57 have 0.8.
   - `basic_black`: two incompatible lighting models.
   - Some conflicts are **exporter float noise**: `basic_shaderbump` specular `0.099999994` (190
     files) vs `0.10000002` (8 files).
   - Some files define a name twice: `bor_assembler.sod` defines `basic_borg` with two different
     parameter sets.

   Refusal is the correct fail-safe today, because the renderer shades from the file's parameters.
   The durable fix is the native-material design in finding 3. Command:
   `refit_sod_census --materials <all effective SODs>`, which now prints ambient, diffuse,
   specular, power, model and self-illum.
9. **Usage-weighted coverage: what players actually see.** I resolved all 10,715 effective ODFs: 186
   from Nexus, 9,364 from FO 4.0 and 1,701 base loose files. The base loose set mirrors `ODF.fpq`,
   whose payloads use a custom LZ compression and were not decoded. Resolution follows `#include`
   inheritance and treats `basename` (default: the ODF's own name) as the model.
   - **650** of the 927 SODs are referenced by some ODF; 277 are unreferenced.
   - **230** are **unit models**, referenced by `craft`, `shipyard`, `starbase`, `constructionrig`
     or `freighter`.

   | SOD set | Triangles | Eligible | Two-group | Parser policy | Blend mode |
   |---|---:|---:|---:|---:|---:|
   | all | 978,367 | 72.2% | 13.6% | 8.2% | 6.0% |
   | referenced | 876,588 | 74.3% | 14.3% | 9.1% | 2.2% |
   | **unit models** | 593,571 | **74.5%** | **16.7%** | 7.7% | 1.1% |
   | unreferenced | 101,779 | 53.7% | 7.1% | 0.1% | 38.9% |

   - **Two-group support matters most for units.** Of the 230 unit models, 102 are fully eligible
     today. 63 have no eligible mesh: 40 are blocked *only* by two-group meshes, 11 by two-group plus
     blend mode, and 11 by parser policy (undefined materials, finding 3). With
     `TwoGroupMeshes`, **150 of 230** unit models are fully statically eligible.
   - The blend-mode share is mostly unreferenced effect content (38.9% there vs 1.1% for units).
   - Weapon and ordnance SODs referenced through keys other than `basename` are not attributed, so
     the "referenced" set slightly understates use.

   Evidence: `build/refit-sod-census/20261106T231042Z/odf-usage.json` (SHA-256 `afa0ecbb…`), from
   `tools/refit_sod_census/odf_usage.py`.

## Not established

- Runtime coverage: per-route counts, LOD/damage selection in play, the effects of the
  geometry-parity and material rechecks.
- ~~Whether ODF.fpq contains SODs~~ **Settled:** the only FPQ in the install is base `Data/ODF.fpq`,
  version 2. It holds 1,698 files under `odf\`: 1,696 `.odf`, one nested `.fpq` and one `.txt`, with
  no SOD or material content. Loose `sod/` directories are the complete SOD input.
- ~~Native resolution of undefined group material names~~ **Settled statically** (finding 3). The actual runtime load order, and which definition wins per name in a real match, is still unmeasured.
- Visual correctness of any eligible mesh beyond the existing Mediterranean controls.
