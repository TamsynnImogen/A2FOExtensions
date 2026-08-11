# Current architecture

## Startup and ownership

`Win2kDisableTaskSwitch.dll` is a startup proxy. During process attachment it
loads the renamed shipped startup DLL and attaches `A2FOExtensions.dll`, keeping
Fleet Ops' required early load order. The core performs deferred initialization
on a worker outside the Windows loader lock.

The core permanently owns shared or lifetime-sensitive engine sites:

- checked patch installation and supported-binary validation;
- the FOFS item-lookup dispatcher;
- ParameterDB/classlabel and Evolver/cocoon dispatch;
- early Fleet Ops settings and profile-default dispatch sites;
- Craft destruction snapshot, replacement construction, and publication;
- extension-root overlay and native/Lua loading order.

The eleven built-in native modules separate optional policy:

- `A2FOAlwaysShowShields.dll`: opt-in persistent native shield visibility,
  a separately tracked continuous full-shield effect, checked lifecycle calls,
  central mission-publication, Starbase, and Fleet Ops common-render coverage, and
  the per-class ODF policy cache;
- `A2FOFeaturePack.dll`: recursive ODF indexing, queue conveniences, upgrade
  pods, and Bink scaling;
- `A2FOHybridBuild.dll`: `hybridbuild -> research`, the `cocoon` command, four
  production palettes, execution sidecars, queued placements, and previews;
- `A2FOInfoIni.dll`: `SettingsDirectory` and `DefaultGameSpeed` parsing and
  resolution;
- `A2FOCheats.dll`: signature-checked Fleet Operations cheat-handler
  extensions, including per-resource `RTS_CFG.h`-configured
  `showmethemoney` grants (10,000 defaults) and restored `m`, `dis`, `crash`,
  and team-elimination `elim` commands;
- `A2FOCraftIdentity.dll`: captain and registry ODF rows aligned to Fleet
  Operations' native craft-name index, plus selected-object panel text fields;
- `A2FOEditMenu.dll`: recursive `buildItemX` editor-menu navigation using the
  native visible menu buffer, renderer, object placement, and Back command;
- `A2FOFireArcs.dll`: optional owner-local box and cone weapon firing volumes,
  with checked Fleet Operations WeaponClass-constructor and system-icon-render
  chains, per-hardpoint hover previews, and complete native fallback for weapon
  ODFs without the new commands;
- `A2FONormalWeaponTech.dll`: ordinary-weapon technology-tree enforcement,
  using each WeaponClass' own project ID and Fleet Operations' native recursive
  team-tree evaluator while treating unlisted weapons as requirement `0`;
- `A2FORGBTextures.dll`: presence-based redirection of Armada's legacy
  `Textures\RGB`, `Textures\Index8`, and `Textures\Compressed` assets across
  Data, parent mods, and the active mod through Armada's TGA FileExists/OpenRead
  boundary, with null-source guards for failed minimap textures.
- `A2FOTurrets.dll`: the global semantic `turret -> sensor` classlabel,
  indexed parent-mount parsing, linked child-object lifecycle, target-driven
  yaw/pitch transforms, ownership propagation, and save/load reconnection. Its
  class-construction, simulation, and cleanup hooks explicitly chain Fleet
  Operations' pre-existing checked detours. Its simulation and cleanup hooks
  provide the single-owner object bridge used by
  `A2FOAlwaysShowShields.dll`; A2FOCraftIdentity supplies that module's common
  ship/station CraftClass registration bridge.

`A1Compat.dll` is an optional parent-mod module rather than a globally installed
built-in. It is packaged under `STA1 Classic/modules` and owns A1-only policy,
beginning with `wingman -> craft`, missing-only `a2craft.odf`, `a2const.odf`,
and `a2freight.odf` defaults, Armada 1 `Addon` ODF precedence, the
starbase officer-quarter system, and the signature-checked legacy nebula
sprite-node guard. The extension-root module
overlay therefore activates it only when `STA1 Classic` or one of its children
is selected.

FeaturePack owns the general Producer queue and ResearchStation class hooks.
HybridBuild registers a private callback table with FeaturePack so those shared
native sites are installed exactly once. The core uses the same pattern for
InfoIni: timing-sensitive hooks remain installed before settings load, while
revision-5 module registration supplies all optional `info.ini` semantics.
API revision 9 exposes shared Producer admission, claimable construction-
effect start, claimable pre-completion, post-completion, and destruction
events through the core registry. `A1Compat` consumes those events for
UpgradeClass admission, effect suppression, and destruction without patching
the existing FeaturePack or HybridBuild hook addresses. A1's removed officer
completion branch must run earlier than the general Producer callback, so
`A1Compat` separately owns the A1-scoped checked `Starbase::FinishBuild` hook
at Armada RVA `0x000bbd90`; every non-officer completion chains its gateway.

The queue feature chains feature-specific Armada and Fleet Ops Producer sites
after exact signature checks. If the supported build or any required signature
does not match, the affected feature is disabled rather than patching an
unknown binary.

## Reading and maintaining the source

The source is divided by ownership rather than by the game feature which first
needed a helper:

- `core/dllmain.cpp` owns process-lifetime shared hooks, dispatch registries,
  and the `A2FO_ModuleApi` implementation.
- `core/hook.*` is the only general-purpose machine-code patch writer.
- `core/extension_roots.*`, `fpq_paths.*`, and `odf_paths.*` contain the
  host-testable path and precedence rules used by the core and FeaturePack.
- `core/lua_host.*` owns the bounded Lua state and converts engine events into
  pointer-free script values.
- `core/module_loader.*` owns DLL overlay, deterministic ordering, registration
  transactions, and shutdown ordering.
- `modules/<name>/module.cpp` owns that module's engine-facing state and hooks.
  Pure parsing or mathematics is split into a neighbouring source/header pair
  whenever it can be tested without loading the game.
- Assembly files are ABI adapters only. They move cdecl arguments into MSVC
  thiscall or Delphi registers and must not acquire gameplay policy.

Comments next to RVAs, offsets, signatures, hook lengths, and assembly
continuations are part of the compatibility contract. They explain why a
value is safe for the supported binaries and should be updated with any code
which changes that assumption. Obvious local expressions are intentionally
left uncommented so the engine invariants remain visible rather than buried in
line-by-line narration.

All engine callbacks obey the same maintenance rules:

1. Treat engine pointers as callback-scoped unless a stable handle or class
   identity is explicitly documented.
2. Validate the complete supported signature set before installing the first
   hook whenever partial activation would be unsafe.
3. Chain a known Fleet Operations detour instead of silently replacing it.
4. Keep installed inline hooks process-lifetime; never unload code still
   reachable from a patched executable address.
5. Fail open for optional policy when reverse-engineered runtime state is
   unavailable, but fail closed before writing to an unknown binary layout.
6. Keep synchronized-game decisions deterministic and require identical DLL
   and ODF policy on every multiplayer participant.

## Deterministic extension overlay

Roots are ordered from lowest to highest precedence: shared `Data`, each
`ParentMod`, then the active mod. DLLs and scripts with the same case-insensitive
basename are replaced by the higher-precedence copy. The resulting native DLLs
and Lua files execute in case-insensitive filename order.

## Registration transactions

Each `A2FO_ModuleInit` and each Lua startup chunk is a transaction. The core
records its dispatcher registrations and ownership claims. If initialization
returns false, throws where catchable, or reports a Lua error, the core rolls
that script/module back before continuing. A rejected DLL is unloaded only
after its registrations have been removed.

Registrations are startup-only. Low-level hooks installed directly by a module
cannot generally be undone safely; modules must therefore validate everything
that can fail before publishing a callback table or installing their first
low-level hook.

## Destroyed-object flow

```text
Craft::Explode (checked core hook)
  -> copy handle, team, transform, source ODF and declared ODF fields
  -> native handlers in module/registration order
  -> Lua handlers in script/registration order
  -> first valid claim wins
  -> core finds, constructs, positions and publishes replacement
  -> original explosion continues
```

Every handler declares its required ODF field names at startup. The snapshot is
the case-insensitive union of active declarations, plus `basename`. Native
event pointers and Lua ODF views are callback-scoped. No script receives an
engine pointer, and the core validates replacement names, flags, and ownership
before acting.

This dispatcher is also useful groundwork for later Noxter mechanics: it gives
future infestation or spawn-on-death modules a deterministic, synchronized
lifecycle event without competing for the Craft destruction hook.

## ABI compatibility

The native ABI remains major version 4. Revisions 1 through 7 only append
fields; revision 1 introduced the revision/capability metadata and member-size
macro, revision 5 adds the optional `info.ini` defaults provider, and revision
6 adds the transactional ODF-overlay directory registry. Revision 7 adds the
transactional Producer-event registry and runtime dispatcher.
Revision 8 adds a claimable pre-native Producer completion event without
changing the API structure.
Revision 9 adds a claimable pre-native construction-effect event, likewise
without changing the API structure.
Revision 10 appends the transactional classlabel ODF-default registry. The core
owns the shared typed ParameterDB hooks; a module supplies copied command/value
pairs which are consulted only after the normal ODF/include lookup fails.
Existing v4 modules continue to receive their original struct prefix. Lua has
an independent major/revision pair and `a2fo.require_api`.

See [`../sdk/README.md`](../sdk/README.md), [`lua-api.md`](lua-api.md), and
[`addresses.md`](addresses.md).
