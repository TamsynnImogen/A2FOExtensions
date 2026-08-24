# Armada 1 Compatibility TODO

This is the dedicated backlog for `A1Compat.dll` and the STA1 Classic
compatibility platform. Keep Armada 1-specific policy and project planning here
rather than mixing it into the general A2FOExtensions backlog.

## Goal

Allow an original Star Trek: Armada 1 mod to run as a Fleet Operations addon
with the smallest practical amount of manual conversion.

The intended addon relationship is:

```text
Fleet Ops 4.0
  └── STA1 Classic
        └── original STA1 mod, such as Millennium Project
```

The preferred installation workflow is:

1. Copy the original STA1 mod into an FO addon.
2. Set `ParentMod="STA1 Classic"` and the current FO asset version.
3. Load its original A1 BZN maps directly when supported; use non-destructive
   conversion only for fields which cannot be represented safely at runtime.
4. Run the compatibility audit and address only genuinely unsupported content.

Compatibility fixes should live in `A1Compat.dll`, STA1 Classic data, or
general conversion tools. Do not require repetitive edits to every inherited
mod when one safe compatibility rule can handle them all.

## Locked Decisions

* [x] Implement Armada 1 compatibility in a dedicated native module named
  `A1Compat.dll`.
* [x] Keep shared hook dispatch, module loading, filesystem services, and SDK
  facilities in `A2FOExtensions.dll`.
* [x] Keep Armada 1-specific behaviour and policy out of the core proxy DLL.
* [x] Package `A1Compat.dll` with STA1 Classic so child addons inherit it
  automatically without affecting ordinary FO4 or STA2 mods.
* [x] Keep backup module DLLs outside active `modules` directories; the loader
  initializes every `.dll` there, including renamed backup copies.
* [x] Treat STA1 Classic as a minimal compatibility parent rather than an
  enhanced-gameplay conversion.
* [x] Keep Millennium Project as a child addon rather than folding its balance
  or gameplay changes into the STA1 Classic baseline.
* [x] Prefer version/header-gated native A1 BZN bridges over modifying original
  maps; retain conversion tooling as a fallback for unsupported fields.
* [x] Move ownership of the `wingman -> craft` classlabel compatibility alias
  from `A2FOFeaturePack.dll` to `A1Compat.dll`.
* [x] Accept the original flat A1 `Addon` directory as compatibility input;
  reorganising all of its files into FO-native directories is not a mandatory
  porting step.

## Project Layout

Target parent-mod layout:

```text
Data/Mods/STA1 Classic/
  info.ini
  modules/
    A1Compat.dll
  odf/
  techtree/
  AI/
  bzn/
  misc/
  sprites/
  sod/
  textures/
  sounds/
```

Target source layout:

```text
modules/A1Compat/
tests/A1Compat/
tools/a1_bzn_convert/
docs/a1-compatibility.md
```

The exact source and test directory names may follow the repository's existing
build conventions, but the runtime DLL name remains `A1Compat.dll`.

## Local Validation Sources

These paths are local test inputs only. Do not copy proprietary Armada assets
or extracted ODF data into the open-source repository.

```text
Vanilla Armada 1 installation:
/home/tamsynn/Games/Heroic/Star Trek Armada/

Extracted Armada 1 ODF archive:
/home/tamsynn/Downloads/armada_odf_files.zip

Extracted Millennium Project 1.9se installation tree:
/home/tamsynn/Downloads/millenium_project19se/Star Trek - Armada/

Original Millennium Project 1.9se package:
/home/tamsynn/Downloads/millenium_project19se.zip
```

Current fixture fingerprints:

```text
Armada.exe SHA-256:
1c37ee8e4a24380c950a91ce5a7f9f96c4fcf3d8ffc0cc7e58faec3835350be6

armada_odf_files.zip SHA-256:
ecb29b9d0e134a0523f7f3ce5755c6de10f2a8c0667c2e45520aadc21bff6036

millenium_project19se.zip SHA-256:
3ffd647b69cecd94865b566ad9c815f5e4242965b34fac8066cd40288903aaa7
```

The ODF archive currently contains 489 ODF files:

| Directory | ODF files |
| --- | ---: |
| `other` | 98 |
| `ships` | 69 |
| `special_weapons` | 111 |
| `stations` | 97 |
| `weapons` | 114 |

The extracted Millennium Project tree currently contains 1,055 files. Its flat
`Addon` directory alone contains 615 mixed data files:

| Extension | Files |
| --- | ---: |
| `.odf` | 498 |
| `.tt` | 37 |
| `.wav` | 35 |
| `.bzn` | 15 |
| `.mdf` | 15 |
| `.bmp` | 15 |

## Compatibility Definition

An A1 mod is considered compatible when it can be installed above STA1
Classics and played without bulk manual rewriting of its ODFs or assets.

Required baseline:

* The mod appears correctly in Mod Settings and inherits STA1 Classic.
* Its declared races appear correctly in Instant Action.
* A converted map can be launched with the intended race, starting units, GUI,
  resources, and technology state.
* Construction, research, movement, combat, transporters, repair, resource
  collection, officer limits, special weapons, and destruction complete without
  crashes or silent substitutions.
* Save/load restores compatibility-owned state.
* Multiplayer peers make the same simulation decisions.
* Unsupported behaviour is logged clearly with the source filename and command
  where possible.
* Running normal FO4 and STA2 mods without STA1 Classic remains unchanged.

## Phase 0: Evidence and Compatibility Matrix

Do not implement broad compatibility guesses before completing a representative
audit of real Armada 1 data.

* [ ] Build a manifest of stock A1 ODFs, classlabels, inherited files, commands,
  races, tech trees, AIPs, maps, sprites, interface files, models, textures,
  sounds, and mission assets.
* [ ] Compare that manifest with the FO4/Roots parsers and runtime behaviour.
* [ ] Classify each difference as already compatible, data-shimmable,
  converter-handled, DLL-hooked, intentionally unsupported, or still unknown.
* [ ] Record the A1 source version and hashes of local validation fixtures
  without committing proprietary assets to the repository.
* [ ] Select a small synthetic fixture for every compatibility behaviour so
  automated tests do not depend on copyrighted stock data.
* [ ] Establish clean FO4 and STA2 regression baselines before enabling the
  module.

Compatibility matrix categories:

| Area | Stock A1 | FO4/Roots | Required action | Verified |
| --- | --- | --- | --- | --- |
| ODF parsing and inheritance | [ ] | [ ] | TBD | [ ] |
| Classlabels and object construction | [ ] | [ ] | TBD | [ ] |
| Race and resource definitions | [ ] | [ ] | TBD | [ ] |
| Officers and crew | [ ] | [ ] | TBD | [ ] |
| Tech trees and research | [ ] | [ ] | TBD | [ ] |
| AI and AIP files | [ ] | [ ] | TBD | [ ] |
| BZN maps and map objects | [ ] | [ ] | Native versioned shim; converter fallback | [ ] |
| GUI, CFG, SPR, buttons, and hotkeys | [ ] | [ ] | TBD | [ ] |
| SOD models and animations | [ ] | [ ] | TBD | [ ] |
| Textures and team colours | [ ] | [ ] | TBD | [ ] |
| Sounds, events, and music | [ ] | [ ] | TBD | [ ] |
| Missions, objectives, and cinematics | [ ] | [ ] | TBD | [ ] |
| Save/load | [ ] | [ ] | TBD | [ ] |
| Multiplayer synchronization | [ ] | [ ] | TBD | [ ] |

Shared compatibility already available outside `A1Compat.dll`:

* [~] `A2FORGBTextures.dll` redirects Armada's completed root-TGA
  `FileExists` and `OpenRead` requests while retaining native DDS/TGA priority.
  The RGB route was verified live with STA2 Classic on 2026-08-05. A raw
  combined fallback faulted in Armada's true-colour texture decoder during the
  2026-08-07 live test, and a 2026-08-09 folder test confirmed that Fleet Ops
  does not issue usable folder-qualified Index8/Compressed requests. The next
  candidate instead expands colour-mapped/RLE TGA data into bounded temporary
  24/32-bit TGAs before using the surviving flattened route. Initial live
  validation showed new non-RGB names working but exposed cross-folder
  precedence: parent RGB still masked a child Index8/Compressed override. The
  next revision resolves extension-root precedence before the per-root format
  tie-break. Automated fixtures pass; live override validation is pending.
  Team-colour compatibility remains separate.
* [BUG] Millennium Project through `STA2 Classic A1 Addon` currently reaches a
  Fleet Ops `EStringListError: List index out of bounds (1)` during menu
  startup. The identical exception was reproduced on the pre-RGB core with the
  RGB module absent, so track it as an A1-addon baseline issue rather than an
  RGB-bridge failure.

## Module Architecture

* [x] Scaffold `modules/A1Compat` as a 32-bit native SDK module with no direct
  dependency on private core state.
* [x] Detect that the active mod chain contains STA1 Classic through the
  inherited `a1compat.ini` marker before enabling any A1 policy. Loading a
  misplaced global DLL does not alter unrelated mods.
* [x] Declare and validate every required SDK capability during module
  initialization; fail closed with a useful log if the core is too old.
* [x] Register current A1 behaviour through shared dispatchers and callbacks
  wherever
  possible rather than competing inline hooks.
* [ ] Add a module version and compatibility-report header to the runtime log.
* [x] Make initialization transactional so rejected hooks or policies do not
  leave partially enabled A1 behaviour.
* [ ] Keep simulation state keyed by stable object or team identity and define
  lifecycle cleanup for destruction, ownership changes, map exit, and DLL
  shutdown.
* [x] Keep every unavoidable A1-scoped direct hook signature-checked and list
  it in `docs/addresses.md`; continue consuming the core-owned ParameterDB
  dispatcher at Armada RVA `0x135350` for shared ODF/classlabel policy.
* [ ] Add an explicit module-level diagnostic mode without enabling verbose
  logging for every player.

### Existing Feature Ownership

The `wingman -> craft` compatibility alias was moved from
`A2FOFeaturePack.dll` to the parent-scoped `A1Compat.dll`.

* [x] Assign future ownership of `wingman -> craft` to `A1Compat.dll`.
* [x] Register the alias conditionally when the active mod chain enables Armada
  1 compatibility.
* [x] Remove FeaturePack's registration only after A1Compat registers and tests
  the replacement, avoiding a window where neither module supplies the alias.
* [x] Ensure the modules never compete to register the alias during the
  transition.
* [x] Document the ownership change in the architecture, FeaturePack, A1Compat,
  and migration documentation.
* [ ] Inventory any other existing generic hooks which are actually A1 policy
  and assign one clear owner for each.

### Legacy `Addon` Directory Compatibility

Original A1 mods commonly place mixed asset types together in a flat `Addon`
directory. `A1Compat.dll` should expose supported files through the appropriate
FO lookup namespaces without physically reorganising the source mod.

* [x] Discover `Addon` case-insensitively in every active A1-compatible mod
  root for ODF lookup.
* [x] Route ODFs from `Addon` through the recursive FO filesystem index.
* [ ] Route tech trees, maps and their metadata/previews, sounds, models,
  textures, sprites, and other confirmed data extensions through the correct
  engine lookup path.
* [x] Keep mod-root precedence authoritative for ODFs: active child, nearest
  parent, remaining parents, then shared Data.
* [x] Within one mod root, make the legacy `Addon` ODF authoritative over an
  ordinary structured ODF, matching Armada 1. A port which needs to replace an
  Addon file must update that Addon copy or supply it from a higher-priority
  child mod root.
* [ ] Treat `Addon` lookup as a virtual overlay; do not copy or rewrite source
  assets at runtime.
* [ ] Never execute DLLs, EXEs, installers, uninstallers, or unknown binary
  types merely because they are present in `Addon` or the source installation.
* [ ] Diagnose same-basename collisions with both physical paths and the rule
  which selected the winner.
* [ ] Use Millennium Project's 615-file `Addon` directory as the main mixed-file
  discovery and precedence fixture.
* [ ] Keep BZN conversion non-destructive: discover original maps in `Addon`
  but write converted output to the FO-native `bzn` directory.

## ODF Parsing and Gameplay Semantics

* [ ] Verify A1 text encoding, line endings, comments, numeric suffixes,
  filename case handling, inheritance syntax, and missing-value defaults.
* [ ] Add Windows-1252 fallback where FO4 does not already provide it.
* [ ] Inventory every A1 classlabel and determine its safe native FO4 host.
* [ ] Implement semantic aliases only when the native object layout is
  compatible; use sidecar behaviour where aliasing alone is insufficient.
* [ ] Compare A1 and FO4 defaults for craft, stations, constructors, producers,
  freighters, resource processors, repair facilities, pods, planets, map
  objects, and special weapons.
* [ ] Preserve A1 behaviour when an old command is omitted rather than silently
  inheriting an incompatible FO4 default.
* [ ] Define precedence for explicit A1 compatibility commands, inherited ODF
  commands, and legacy magic-value behaviour.
* [ ] Log unsupported commands once per source ODF in diagnostic builds.
* [ ] Reject structurally unsafe aliases rather than constructing a native
  subclass with an incompatible object tail.

## Race, Resource, Officer, and Crew Compatibility

* [ ] Reproduce the stock A1 race list and playable Instant Action ordering in
  STA1 Classic without changing normal FO4 race discovery.
  * [x] Confirm FO4 filters legacy race ODFs which omit `displayKey` and
    `instantActionSlot`; stock A1 uses `displayName` and has no slot field.
  * [x] Confirm the stock A1 playable-race discriminator: Borg, Federation,
    Klingon, and Romulan define `interfaceConfiguration`, while the non-playable
    race records in stock `races.odf` do not. Do not require
    `instantActionSlot`, `interfaceSprites`, or starting-unit commands for the
    legacy fallback.
  * [x] Restore the installed STA1 Classic race ODFs to their unmodified stock
    definitions; do not keep FO-only `displayKey` or `instantActionSlot`
    additions in compatibility fixtures.
  * [x] Add an A1Compat-only legacy race-menu fallback. If any faction
    referenced by the active `races.odf` lacks a valid `instantActionSlot`,
    traverse `race0` through `race<numberOfRaces - 1>` and add every faction
    which defines `interfaceConfiguration` to the dropdown in that exact
    declaration order. Entries without `interfaceConfiguration` remain loaded
    Race records but are omitted from the playable menu.
  * [x] When the fallback is active, derive a missing menu label from the
    faction's `displayName` without modifying its ODF. If every referenced
    faction supplies a valid `instantActionSlot`, retain normal FO4 slot and
    `displayKey` behaviour unchanged.
  * [ ] Cover stock A1 ordering, mixed present/missing slot fields, sparse
    `raceX` entries, inherited `interfaceConfiguration`, missing race ODFs,
    duplicate names, child-mod overrides, and repeated menu entry/exit in
    automated and manual tests.
* [ ] Verify race display names, GUI selection, interface CFG/SPR loading,
  cursors, transport sprites, music, starting units, and preload units.
  * [x] Diagnose the first race-menu crash at Armada `0x00513B05`: missing
    `gui_<race>.cfg` left `commBarNumberOfPlayers` at zero, so Armada allocated
    no communications-player array and dereferenced element zero.
  * [x] Use the stock A2 `gui_fed.cfg`, `gui_bor.cfg`, `gui_kli.cfg`, and
    `gui_rom.cfg` layouts as the initial startup-crash workaround.
  * [x] Retire that workaround as the compatibility target after proving A1's
    gameplay CFG/SPR syntax is parser-compatible. Preserve the raw A1 layout
    and translate the remaining A1/A2 runtime component differences instead.
  * [ ] Confirm each A1 race menu and gameplay HUD against its original
    presentation while keeping A2-only features opt-in through converted UI.
* [~] Map A1 starting-resource commands and resource visibility without
  inventing FO-only resources for an A1 race.
  * [x] Supply missing Crew, Dilithium, Metal, Tritanium, and Supply normal
    values from the active `SHOWMETHEMONEY_*` configuration and calculate each
    lots value as 1.5 times normal. Preserve explicit/inherited Race fields and
    leave every A1 ODF unchanged.
  * [x] Write Crew/Dilithium/Metal into Armada's native two-by-six Race matrix
    and expose Tritanium/Supply to `A2FOResources` through core-owned
    missing-only Race snapshot defaults. Do not invent Latinum, Officers,
    Biomatter, Credits, or Collective Connections values.
  * [ ] Live-test normal and lots Instant Action starts with an unchanged A1
    race set and with a child race that explicitly overrides one value.
* [ ] Reproduce distinct current officers, maximum officers, and hard officer
  limit semantics where FO4 differs.
* [ ] Verify officer acquisition, officer costs, build blocking, tooltips,
  transfer, capture, recycling, and save/load.
* [ ] Restore A1 starbase officer-quarter upgrades (`oq1`, `oq2`, ...).
  * [x] Confirm stock A1 `Fbase`, `Bbase`, `Kbase`, and `Rbase` SODs contain
    `oq1` through `oq6`; Federation places them directly below `base_fed`.
  * [x] Recover the original A1 model contract: `maximumUpgrades` at class
    `+0x568`, `officerGain` at `+0x56c`, the node-pointer array at `+0x570`,
    and completed upgrades at Starbase `+0x7e4`.
  * [x] Hook Fleet Operations `Starbase::InitializeGeometry` at Armada RVA
    `0x000bda00` and restore the zero-upgrade pre-clone visibility state for
    exact numbered `oqN` nodes. Models without those nodes remain unchanged.
  * [x] Reintroduce A1 `UpgradeClass` completion semantics at the removed A1
    Starbase lifecycle boundary; A2/FO's retained `OfficerUpgradeClass::Build`
    remains a null-returning placeholder.
  * [x] Restore the four race-selected `b_fedoff`, `b_klingoff`, `b_romoff`,
    and `b_borgoff` sprite registrations omitted by the A2-compatible
    `gui_global.spr`; retain the original A1 `gb*offq` textures.
  * [x] Restore race selection for the current STA1 Classic data by recording
    Starbase and `OfficerUpgradeClass` `race` values, filtering mismatched
    palette slots, and rejecting mismatched synchronized queue orders.
  * [ ] Parse each legacy race ODF's `officerUpgradeODF` command so untouched
    third-party A1 officer-upgrade ODFs do not need a temporary `race` field.
  * [x] Consume officer upgrades in place through the checked A1Compat hook at
    A2 `Starbase::FinishBuild` RVA `0x000bbd90`. Claiming FeaturePack's later
    Producer `FINISHING` event skipped object creation but still returned null
    to the outer A2 Starbase routine; that null reached `OutputQueueManager`
    and faulted at `0x0053757b` while reading `object+0x44`. The direct
    Starbase boundary mirrors A1's preferred-VA `0x0045f570` special case and
    chains every ordinary completion unchanged.
  * [x] Suppress Armada's cosmetic Producer construction effect for matching
    officer upgrades through API revision 9's shared `STARTING_EFFECT` event.
    `OfficerUpgradeClass` is not a CraftClass, and passing it to the effect
    renderer caused the same `0x004cb151` fault before completion while it read
    the nonexistent class field at `+0x408`.
  * [x] Keep Starbase policy and `builder_ship` menu registration independent
    from officer target/completion signature checks, so an unavailable officer
    bridge cannot remove the ordinary construction menu.
  * [x] Restore the A2 `builder_ship` menu capability (`0x80` at
    `GameObjectClass+0x1d4`) for A1-policy Starbases with parsed build items.
    A1 predates A2's context-sensitive menu commands, so the item buttons can
    all be valid while Fleet Ops omits their outer Build command.
  * [x] Normalise A1's selector to the conventional FO system namespace
    (`odf/system/techlvl.odf`). A fresh-process test produced no palette
    change, proving the previous recursive `odf/other` copy was already being
    resolved and was not the missing-command cause.
  * [x] Parse `maximumUpgrades` and `officerGain`, track completed upgrades per
    starbase, reveal one `oqN` branch after each completion, and enforce the
    maximum without assuming six nodes.
  * [x] Apply translated FO maximum-officer changes on creation, completion,
    capture, ownership removal, and destruction using the stable Team pointer;
    do not write A1 available-officer gains into FO's enlisted counter.
  * [x] Serialize compatibility-owned completed count and cumulative gain in a
    versioned Starbase sidecar record.
  * [ ] Manually verify build-button selection, six sequential completions,
    over-limit rejection, capture/destruction reversal, save/load, and
    multiplayer synchronization in STA1 Classic.
* [ ] Audit crew accumulation, boarding, retreat, derelicts, capture, and repair
  defaults against stock A1 behaviour.
* [ ] Ensure random-race resolution selects a concrete A1 race and follows its
  UI, resources, texture suffixes, and starting-unit configuration.

## Construction, Research, and Technology

* [ ] Verify in-game that FO4 consumes the stock A1 `.tt` tech-tree files
  unchanged when `techlvl.odf` selects them from `odf/system`; moving the
  selector did not alter Producer palette visibility.
* [ ] Audit A1 full-tech, no-tech, research, special-weapon, and superweapon
  restrictions.
* [ ] Preserve A1 build-list ordering, availability, costs, build time, officer
  gating, and cancellation/refund behaviour.
* [ ] Bridge A2's context-sensitive menu commands for inherited A1 data,
  including shared `craft`/`station` defaults and constructor
  `builder_facility`, without requiring each child addon to edit its ODFs.
* [ ] Verify constructors, shipyards, research stations, upgrade pods, and
  replace/evolve-style objects used by real A1 mods.
* [ ] Define safe behaviour for missing or circular inherited technology
  entries and report them clearly.
* [ ] Test technology state through save/load and multiplayer.

## AI Compatibility

* [x] Bridge Fleet Operations' `<race>_instant_action_build_list` lookup to
  A1's `<race>_build_list` naming convention when the active A1 layer provides
  only the legacy plan; preserve explicit modern child-mod plans.
* [ ] Inventory stock and representative modded A1 AIP syntax and referenced
  build lists.
* [ ] Verify parsing, race selection, starting conditions, goals, fleet
  composition, defensive/offensive lists, and difficulty modifiers.
* [ ] Add translation only for confirmed semantic differences.
* [ ] Report missing ODF and technology references without crashing the AI.
* [ ] Run repeatable AI-versus-AI smoke matches for every stock A1 race.

## BZN Map Compatibility and Conversion

Prefer transparent runtime loading owned by A1Compat, gated by a validated A1
header and the live FileReader version. Retain a companion non-destructive
converter for schema fields which cannot be represented safely by the A2
runtime.

* [x] Confirm Fleet Operations retains A1 `saveGameDesc`, `binarySave`, and
  old-version header branches for local versions 2050–2053.
* [x] Compare A1 and A2 load order and bypass A2's additional craft-class table
  loader at RVA `0x002025c0` only for validated A1 readers. This removes one
  confirmed A2-only stage but does not repair the earlier object-tail mismatch.
* [x] Trace the native object and Teams loader cursor deltas and add a guarded
  diagnostic resynchronization which accepts exactly one serialized
  `EmptyMission` marker before preserving native `AiMission::LoadMission`.
* [x] Identify the 20-record A1 neutral-object tail left outside A2's primary
  object count on `2blue.bzn`, validate it against a unique later mission, and
  replay it through the native object loader before Teams instead of skipping
  its nebulae, moons, and wormholes.
* [ ] Add context-aware translation for A1 scripted/campaign missions; the
  header BZN filename must not be passed directly to A2's mission factory
  because it can immediately trigger incompatible end conditions.
* [x] Add a checked `RtimeClass::Load` input-call bridge which honors the live
  labelled field's declared 40-byte size and supports a 32-byte legacy record
  only when explicitly declared, zero-padding the A2 buffer in that case.
* [x] Translate the validated A1 header min/max bounds into native A2
  `MapDetails` and live world bounds so Instant Action no longer reports `0x0`.
* [x] Publish those live bounds before the native object loader transforms any
  serialized position; mission-time publication is too late and misplaces the
  already-constructed map contents.
* [x] Parse companion A1 MDF `StartLocations`/`StartN` records, convert the
  `0..117` inverted-Y minimap grid to world-space A2 start records, and mark
  surplus native slots empty without editing either source file.
* [ ] Live-test the MDF bridge on `2blue.bzn`; confirm both player slots are
  assigned and the match no longer goes directly to the end-game screen.

* [ ] Document the exact A1 and FO4 BZN layouts field by field.
* [ ] Preserve map dimensions, terrain/background, object ODF names, positions,
  rotations, teams, player starts, ownership, resources, nebulae, wormholes,
  blocking regions, paths, triggers, and map metadata where representable.
* [ ] Define conversions for fields whose coordinate system or meaning changed.
* [ ] Preserve unknown fields in diagnostics rather than silently discarding
  them.
* [ ] Implement a non-destructive single-file conversion command.
* [ ] Add recursive batch conversion for an addon folder.
* [ ] Default output to a separate directory or new filename; never overwrite
  original maps without an explicit option.
* [ ] Add dry-run, verbose-report, and machine-readable report modes.
* [ ] Copy or convert map previews and associated text only when required.
* [ ] Validate every referenced ODF against the complete inherited mod chain.
* [ ] Make unresolved objects warnings by default and configurable errors for
  strict validation.
* [ ] Add round-trip structural tests and known-map launch tests.
* [ ] Document which mission scripting or trigger features cannot be converted
  automatically.

## Assets and Presentation

* [ ] Verify A1 SOD loading, node names, hardpoints, animations, damage meshes,
  Borg textures, team colours, lightmaps, and texture suffixes in FO4.
  * [x] Map `0x0049DD62` to
    `NebulaClass::s_SetTexturesRecursive`, where an unresolved Storm3D node
    leaves its nested render pointer null.
  * [x] Disprove the initial missing-model theory: installing byte-identical
    stock `Mnebula1.sod` through `Mnebula5.sod` files did not change the fault.
  * [x] Identify the inherited-map dependency: the A1 `Sprites/sprites.spr`
    master registry replaced FO's registry and omitted `fleetops.spr` and
    `fleetops_comp.spr`, which define the nodes used by FO map-nebula SODs.
  * [x] Stage and deploy a combined A1/FO sprite registry retaining the A1
    includes and restoring both FO includes.
  * [x] Confirm the combined registry alone does not change the
    `0x0049DD62` fault; retain it as a required inherited-map compatibility
    fix rather than recording it as this crash's root cause.
  * [x] Test without the eight nested `sod/Software` variants. Five are
    90-byte `Mnebula` sprite stubs sharing basenames with the valid root
    hardware models, but moving all eight outside the active SOD tree did not
    alter the fault.
  * [x] Identify `map_nebula_crystalid.odf` in the captured exception stack.
    Its SOD uses `ncyrstA` through `ncyrstD`, defined by FO's `fleetops.spr`.
  * [x] Confirm that installing the active parent's exact `fleetops.spr` and
    `fleetops_comp.spr` beside the child master registry does not change the
    fault; missing parent-table traversal is not the direct cause.
  * [x] Map the fault to `NebulaClass::s_SetTexturesRecursive` at Armada RVA
    `0x0009dd62`: an `ST3D_SpriteNode` has a null type-specific-data pointer at
    node offset `0xc0`, which native code dereferences at offset `0x2c`.
  * [x] Add a signature-checked A1Compat inline guard at Armada RVA
    `0x0009dd40`. Log node/parent names and skip only invalid sprite nodes while
    passing valid nodes through the native gateway.
  * [ ] Use the guard log to identify every affected node, then fix the
    upstream sprite/SOD construction contract and decide whether the guard
    remains as defensive A1 compatibility.
    * [x] Identify 81 rejected nodes across the `fluid*`, `tachyon*`, and A2
      `big*` nebula families. A1's child `nebula.spr` overrode the STA2
      superset while omitting all of those definitions.
    * [x] Use STA2 Classic's strict-superset `nebula.spr` in the local
      compatibility parent; preserve the A1 original outside the active tree.
    * [ ] Confirm the superset table initializes every shared nebula SOD node,
      then reassess whether the null guard is still required.
  * [x] Map the subsequent game-open failure at Armada RVA `0x0013c334` to
    `RtimeClass::Load(FileReader&)` dereferencing a null serialized-object
    factory.
  * [x] Add a non-bypassing diagnostic at RVA `0x0013c2da` which logs the
    missing 40-byte runtime-class name through the safe registry finder at RVA
    `0x0013c1a0`. The checked site deliberately follows the absolute-address
    instruction at RVA `0x0013c2d4`; reports include all 40 raw bytes and the
    native caller RVA when the purported name is non-textual.
  * [x] Extend the diagnostic with `FileReader` flags, buffer size, cursor,
    remaining byte count, the second read result, and bytes around the cursor.
    The `mp08walr.bzn` failure is not EOF: the active 66,515-byte A2 v2172 map
    still has 16,879 bytes remaining when `AiMission::LoadMission` is called.
  * [x] Locate that cursor at offset 49,636, inside the first loaded `mdmoon`
    record (`mdmoon20`). The A1 child ODF overrides A2's `mdmoon.odf` while
    omitting `resource = "ResourceMoon"`, so its runtime resource state does
    not match the object serialized by the inherited A2 map. `mmooninf.odf`
    has the equivalent missing `ResourceMoonInf` declaration.
  * [x] Remove the temporary moon ODF edits. Raw A1 data must remain unchanged;
    missing A2 constructor commands belong in A1Compat's runtime policy.
  * [x] Add a missing-only A2 Classic moon-resource bridge at
    `GameObjectClass`'s checked `resource` lookup. Match only
    `mdmoon[digits]` or `mmooninf[digits]` by resolved project ID so asteroids,
    comets, and explicit/inherited resource values remain untouched.
  * [ ] Confirm the runtime bridge lets `mp08walr.bzn` pass the old
    `0x0013c334` load stage, then audit other inherited-map ODF overrides for
    configuration-dependent serialized state.
    * [x] The temporary data experiment proved that supplying
      `ResourceMoon`/`ResourceMoonInf` removes the old failure; the permanent
      implementation now supplies those values in A1Compat instead.
    * [x] A 2026-08-22 retest showed the permanent bridge was still unavailable:
      Fleet Operations had already detoured `cPrjID::GetOdfName`, while
      A1Compat accepted only its untouched Armada prologue. The bridge now
      validates and chains either the stock entry or Fleet Operations' checked
      live detour before installing the missing-only resource lookup.
    * [x] The following retest installed the bridge but still reached the same
      cursor without applying a default. The resource lookup occurs before the
      core's completed-class cache necessarily contains its ParameterDB.
    * [x] Reject a live `ParameterDB::GetString` fallback for that cache miss.
      A 2026-08-22 retest exposed that the shared identity helper is also used
      defensively with `GameObjectClass` pointers; treating one as a
      `ParameterDB` crashed at Armada RVA `0x001353c1`. The early moon bridge
      now relies only on its exact resolved project-ID family.
  * [x] Map the RVA `0x0010ad39` failure to
    `StandardText::InitializeConfiguration`: an ST3D sprite lookup returned
    null, then native code read field `+0x50` without checking it.
  * [x] Add a non-bypassing diagnostic at RVA `0x0010ad23` to log the live
    configuration TString, generated sprite name, item index, and caller RVA
    while preserving the null result and native failure.
  * [x] Make `a2_gui_global.spr` an A1Compat-owned essential GUI table at
    Armada RVA `0x0011a776`: load it into the fresh interface database before
    the active child `gui_global.spr`, then verify
    `buttonBackgroundPanel.0` before GUI component initialization.
    * [ ] Confirm Millennium Project logs both essential/final sentinels as
      present and advances beyond Armada RVA `0x0010ad39` without adding a
      child-local include.
  * [x] Use the StandardText diagnostic to identify and correct the child
    CFG/SPR override responsible for the missing sprite, retaining A2/FO GUI
    definitions where Armada 1 UI data is structurally incompatible.
    * [x] Identify `buttonBackgroundPanel.0` as the missing item. The child
      `gui_global.spr` overrides STA2 Classic's file but omits both panel
      sprites while the child `gui_interface.cfg` is byte-identical to the A2
      version that requires them.
    * [x] Preserve the displaced child table outside the active tree as
      `gui_global.a1-original.spr`, then use STA2 Classic's complete
      `gui_global.spr` in the local compatibility parent.
    * [ ] Confirm both `buttonBackgroundPanel` items load and the map advances
      beyond RVA `0x0010ad39`; keep the diagnostic for the next missing sprite
      until the full A2 GUI contract has been validated.
      * [x] Confirm the old StandardText crash disappears; the map now reaches
        Fleet Ops `CraftEnhancement.Craft_mLevelUp` at RVA `0x001dbdcb`.
  * [x] Map the new `0x001dbdcb` fault to the native
    `Craft -> Side(+0xf0) -> Race(+0x244) -> canGainXP(+0x634)` chain. The read
    of address `0x00000634` means the craft's Side has no Race pointer.
  * [x] Add a signature-checked, non-bypassing A1Compat diagnostic at the
    seven-byte `canGainXP` read. Log the exact craft ODF, object handle/team,
    Side/Race/class/enhancement pointers, force flag, and caller Fleet Ops RVA,
    then execute the displaced read unchanged.
  * [x] Use the Craft_mLevelUp diagnostic report to identify why that craft's
    Side is raceless, then restore the missing A2 runtime contract without
    changing the A1 ODF.
    * [x] Identify `zferscav.odf` on neutral team 0 during
      `Craft_Init_Callback` (`force=0`). The A1 `races.odf` override omitted
      A2/FO's `norace.odf`, leaving neutral Side objects without a Race.
    * [x] Prove that appending `norace.odf` after the ten original A1 races
      makes the map advance beyond Fleet Ops RVA `0x001dbdcb`; use this only
      as diagnosis, not as the shipped compatibility fix.
    * [x] Replace the non-bypassing diagnostic with an A1Compat runtime
      missing-only default. Valid Race objects still use their native
      `Race+0x634` value; a null neutral Race receives A2 Classic's
      `canGainXP = false` result and continues without editing `races.odf`.
    * [x] Confirm that narrow fallback advances through eight neutral Ferengi
      craft, then identify the next null-Race consumer at Armada RVA
      `0x000b5259` as `Race+0x290` `crewAccumulationRate`.
    * [x] Replace per-field compatibility as the primary solution with a
      runtime-only `Race::InitAll` registry default: append inherited
      `norace.odf` after the declared A1 records only when missing, preserve
      every original index, and leave the neutral entry nonplayable.
      * [x] Accept and chain the core A2FOExtensions detours already installed
        on `ParameterDB::GetInt` and `GetString`; the first deployment rejected
        the valid core target and therefore left both Race lookups unpatched.
    * [x] Confirm the unchanged ten-entry A1 `races.odf` enters live gameplay
      and logs `A2 Classic neutral Race registry default` before finalizing
      eleven Race records.
  * [ ] Resolve the reported serialized class through data conversion or a
    compatible registered implementation; remove the temporary diagnostic
    once the input contract is known.
  * [x] Confirm the five classic nebula SODs are byte-identical in stock A1,
    stock A2, and STA2 Classic; keep the local copies for A1 map compatibility.
  * [ ] Inventory the remaining SOD dependencies before the first map launch;
    keep proprietary models in the local installed mod, not this repository.
* [ ] Verify TGA variants, alpha handling, animated textures, sprites, wireframes,
  buttons, cursors, and minimap assets.
* [ ] Audit A1 GUI CFG and SPR commands against FO4 layouts and scaling.
  * [x] Confirm A1 and A2 gameplay CFG/SPR syntax is parser-compatible and
    identify the primary gap as runtime component semantics rather than file
    format conversion.
  * [x] Detect raw A1 gameplay layouts from `speedPanelArea` plus
    `controlPanelArea`, while preserving any CFG that declares modern screen
    dimensions.
  * [x] Apply A1's 640x480 reference size to the live gameplay ParameterDB
    before component PostLoad, allowing native rectangle scaling without
    rewriting a mod CFG.
  * [x] Adapt Fleet Ops' PopupPalette controls to each raw A1 race's exact
    `controlButton1` through `controlButton12` rectangles at render time,
    composing after popup compaction without replacing HybridBuild's popup
    ownership or rewriting the CFG.
  * [x] Adapt A2's three ShipDisplay layout/background names to A1's single
    `infoPanelArea`, `infoBlackArea`, and `infoBackgroundPanel` entries at the
    shared runtime rectangle/string loaders. Keep all other ShipDisplay keys
    native so the A1 single-selection data remains authoritative.
  * [ ] Restore the A1 SpeedRail, ControlPanel, individual resource panels,
    minimap, ship display, and cinematic behavior through A1Compat adapters.
  * [ ] Audit A1 font-table selection separately after the panel adapters are
    functional.
  * Project rule: A1Compat targets faithful A1 gameplay UI styling and
    behavior. It does not force A2-only interface features into an A1 layout;
    modders can ship an A2-compatible UI when they want those features.
* [ ] Preserve data-driven A1 presentation where possible; use compatibility
  defaults only when an old asset omits something FO4 requires.
* [ ] Verify A1 WAV/music/event references and define safe fallbacks for missing
  files without hiding genuine porting errors.
  * [x] Diagnose FleetOpsHook's one-track shuffle failure. At Fleet Ops RVA
    `0x001d448f`, `TMusicPlayer.randomizeTrackOrder` calls
    `TStringList.Exchange(0, 1)` when only one declared file exists. With STA2
    Classic as the temporary parent, only each race's `300` WAV existed while
    its A1 `115` and `100` WAVs were absent.
  * [x] Install the vanilla A1 `sounds/music` directory into the local STA1
    Classic validation mod; keep those copyrighted assets out of this source
    repository.
  * [ ] Decide whether `A1Compat.dll` should guard the generic one-track case,
    while still logging missing files rather than hiding incomplete addons.
* [ ] Keep proprietary A1 assets in local/mod validation data, not in the
  open-source DLL repository.

## Missions and Scripted Content

* [ ] Inventory stock A1 mission, objective, trigger, cinematic, and campaign
  formats before promising campaign compatibility.
* [ ] Separate skirmish-map compatibility from campaign compatibility so
  mission work cannot block the first playable release.
* [ ] Determine which mission behaviours can be translated, shimmed at runtime,
  or must remain unsupported.
* [ ] Add diagnostics for unsupported mission commands and missing objective
  text rather than failing silently.
* [ ] Validate campaign progression and saved mission state only after stock
  skirmish compatibility is stable.

## Save/Load and Multiplayer

* [ ] Inventory every piece of A1 compatibility state not already serialized by
  Armada/Fleet Ops.
* [ ] Version the module's save markers and reject incompatible state safely.
* [ ] Rebuild derivable state on load rather than serializing redundant native
  pointers or process addresses.
* [ ] Synchronize all compatibility-owned orders and state transitions through
  deterministic simulation paths.
* [ ] Test mixed object ownership, capture, derelicts, team changes, and player
  departure.
* [ ] Confirm identical content and module versions during multiplayer setup and
  provide a useful mismatch diagnostic.

## Diagnostics and Modder Workflow

* [ ] Add an A1 compatibility audit mode which scans an addon and produces:
  * unrecognized classlabels and ODF commands;
  * unresolved inheritance and asset references;
  * unsupported map or mission fields;
  * race, technology, AI, sprite, sound, and model dependencies;
  * case-only filename collisions;
  * files shadowed by Data, parent, or active-mod precedence;
  * required BZN conversions.
* [ ] Report both the logical basename and winning physical mod root for every
  precedence problem.
* [ ] Keep warnings actionable and deduplicate repeated inherited errors.
* [ ] Support a strict mode suitable for automated validation and a permissive
  mode suitable for launching incomplete ports.
* [ ] Produce a concise end-of-scan compatibility summary.

## Testing Matrix

Minimum automated/runtime coverage:

| Test | Clean FO4 | STA2 mod | STA1 Classic | A1 child mod |
| --- | --- | --- | --- | --- |
| Module activation isolation | [ ] | [ ] | [ ] | [ ] |
| Race/menu discovery | [ ] | [ ] | [ ] | [ ] |
| ODF aliases and defaults | [ ] | [ ] | [ ] | [ ] |
| Map conversion and launch | N/A | N/A | [ ] | [ ] |
| Construction and research | [ ] | [ ] | [ ] | [ ] |
| Resources, officers, and crew | [ ] | [ ] | [ ] | [ ] |
| AI skirmish | [ ] | [ ] | [ ] | [ ] |
| Save/load | [ ] | [ ] | [ ] | [ ] |
| Multiplayer synchronization | [ ] | [ ] | [ ] | [ ] |

Test content levels:

* [ ] Synthetic ODF/map fixtures for every parser and semantic rule.
* [ ] Stock STA1 Instant Action smoke test for each playable race.
* [ ] Representative stock A1 maps covering common and unusual map objects.
* [ ] One small unmodified third-party A1 addon as the first child-mod test.
* [ ] Millennium Project as the broad acceptance and regression addon.
* [ ] A normal FO4 match and a normal STA2-addon match after every hook-level
  compatibility change.

## Delivery Milestones

### Milestone 1: Audit and Skeleton

* [ ] Complete the initial compatibility matrix.
* [x] Scaffold and conditionally activate `A1Compat.dll`.
* [x] Add module logging and synthetic smoke fixtures.
* [x] Document the STA1 Classic parent/addon installation contract.

### Milestone 2: Stock Instant Action

* [ ] Load the stock A1 races and interface through STA1 Classic.
* [ ] Launch one converted skirmish map.
* [ ] Build, move, fight, harvest, repair, research, and save/load with one race.
* [ ] Confirm clean FO4 and STA2 behaviour remains unchanged.

### Milestone 3: Complete Stock Skirmish

* [ ] Make every stock A1 race playable in Instant Action.
* [ ] Batch-convert and validate representative stock A1 maps.
* [ ] Complete technology, AI, resource, officer, and crew compatibility.
* [ ] Run save/load and multiplayer validation.

### Milestone 4: Third-Party Addons

* [ ] Port one small A1 addon using only parent metadata, map conversion, and
  fixes for confirmed unsupported behaviour.
* [ ] Convert Millennium Project into a child addon of STA1 Classic.
* [ ] Record every manual Millennium change and move repeated compatibility
  work back into `A1Compat.dll` or the converter where safe.
* [ ] Publish a modder-facing compatibility and migration guide.

### Milestone 5: Campaign Compatibility

* [ ] Reassess campaign scope after stock and modded Instant Action are stable.
* [ ] Implement only the mission compatibility supported by the completed
  evidence matrix.
* [ ] Clearly document any campaign features which still require manual porting
  or cannot be supported safely.

## Documentation

* [x] Create `docs/a1-compatibility.md` covering module activation, supported A1
  behaviour, addon layout, BZN conversion, diagnostics, and known limitations.
* [ ] Document all hooks and addresses in `docs/addresses.md` with source,
  calculation, expected bytes, overwritten length, owner, and validation status.
* [ ] Maintain a command/classlabel compatibility reference generated from or
  checked against the audit matrix.
* [ ] Document the minimal steps for turning an A1 mod into an STA1 Classic
  child addon.
* [ ] Keep unsupported behaviour explicit; never claim general A1 compatibility
  from a single successful map or mod.

## Open Design Questions

* [ ] Should converted maps keep their original basename or receive an explicit
  suffix so originals and converted copies can coexist?
* [ ] Should BZN conversion remain an offline tool, become transparent at load
  time, or support both after the offline path is proven?
* [ ] Which A1 behaviours should be exact emulation, and which harmless FO4
  improvements may remain enabled?
* [ ] Which campaign and mission systems are practical without invasive engine
  replacement?
* [ ] Should the audit/converter be standalone executables, subcommands of one
  A1 compatibility tool, or both?
