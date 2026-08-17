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
- shared WeaponClass loading, two-phase weapon triggering, and Craft
  simulation/cleanup/post-load dispatch;
- global native-module policy and inherited extension-root ordering;
- Fleet Operations Mods-screen module selection and launch validation.

The built-in native modules separate optional policy:

- `A2FOAlwaysShowShields.dll`: opt-in persistent native shield visibility,
  a separately tracked continuous full-shield effect, checked lifecycle calls,
  central mission-publication, Starbase, and Fleet Ops common-render coverage, and
  the per-class ODF policy cache;
- `A2FOBuildTooltips.dll`: plain-text build durations in normal and verbose
  build-button tooltips, sourced from Armada's local-team adjusted construction
  time so game-setup and team modifiers remain authoritative, plus non-zero
  additional-resource costs when the resource module is active;
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
  Operations' native craft-name index, plus selected-object panel identity,
  ammunition, directional-shield graphics, the ranked-craft XP bar, and
  native hover regions for selected shield and XP status bars;
- `A2FOEditMenu.dll`: recursive `buildItemX` editor-menu navigation using the
  native visible menu buffer, renderer, object placement, and Back command;
- `A2FOMissionSelector.dll`: a scrollable combined campaign/mission shell
  dialog with moddable descriptions and previews, while Armada retains native
  availability, progression, filename selection, and mission launch;
- `A2FOFireArcs.dll`: optional owner-local box and cone weapon firing volumes,
  globally switchable through `RTS_CFG.h`, with checked Fleet Operations
  target-authorization and system-icon-render chains, UI-configurable
  per-hardpoint hover previews, and complete native fallback for weapon ODFs
  without the new commands;
- `A2FOEnergySystems.dll`: per-Craft Photon and Quantum Torpedo stores,
  per-launched-shot ammunition debit, automatic or provider-only recharge,
  same-team yard/RepairShip resupply, selected-panel current/maximum status,
  and save/load persistence;
- `A2FODirectionalShields.dll`: strictly opt-in four-facing Craft shield
  sidecars, A2 `maxShields` validation with an A1 missing-command fallback,
  owner-local hit classification, aggregate reconciliation, and depleted-arc
  impact-flare filtering with per-facing native effect colour, composed
  through WeaponDamageControls' single checked `Craft::Damage` hook;
- `A2FONormalWeaponTech.dll`: ordinary-weapon technology-tree enforcement,
  using each WeaponClass' own project ID and Fleet Operations' native recursive
  team-tree evaluator while treating unlisted weapons as requirement `0`;
- `A2FOResources.dll`: four independent sidecar resources beside the six
  native pools, with shared class/Race ODF dispatch, Producer payment/refund
  integration, an extra resource-panel row, and native extension accessors;
- `A2FOWreckage.dll`: deterministic ODF-driven destroyed-craft replacement,
  using the core's shared destruction snapshot and publication dispatcher;
- `A2FONebulaRenderer.dll`: opt-in controller for DX8 per-pixel ship lighting
  derived from armadaNebulaPatch and per-diffuse `textureX` /
  `emissiveX<Subsystem>` map sets (with the original six unnumbered commands
  retained as a single-material wildcard);
  the core owns its checked early pass-through hooks, shader resource cache,
  and Fleet Operations alpha-transition gateway because the shared DOT3 shader
  predates deferred module loading. Classic SODs use scoped MeshVB and checked
  GPU/CPU workspace fixed-function combiner sites instead of the DOT3 pixel
  shader. Loose
  emissive maps retain their authored RGB and sharp self-lit centres, while all
  mesh paths bind the matching composite directly during their native material
  draw. The experimental private-mask/`EndScene` framebuffer compositor is
  disabled: repeated render-target and shader-state replay is unstable through
  dxwrapper/d3d8to9/ReShade, particularly during edit-mode transitions.
  External halo bloom can instead be supplied by ReShade. Checked
  CraftSystem state reads keep operational maps lit, flicker control-disabled
  maps, and switch destroyed/repairing maps off. Full generated mip chains and
  trilinear sampling suppress UV shimmer during movement. It composes with
  A2FOCraftIdentity's completed CraftClass boundary and
  A2FOHybridBuild's common Craft render boundary rather than claiming duplicate
  hooks;
- `A2FOPointDefenseCycles.dll`: per-instance, saveable numbered shot-delay
  cycles for `PointDefenseLaser` and `OrdnanceDefenseWeapon`, plus accurate
  pre-fire enforcement of ordinary `shotDelay` for PointDefenseLaser while
  preserving the native target/interception paths and reload modifiers;
- `A2FOTextureVariants.dll`: render-time faction texture suffix selection and
  case-insensitive Race-name SOD node visibility using each craft's live owner,
  plus DDS-aware native Borg alternate preflight. Shared class geometry is
  restored/reselected immediately before each craft draw, while Armada retains
  ownership of its native `borg` node and Jan_B diffuse/bump route;
- `A2FORGBTextures.dll`: presence-based redirection of Armada's legacy
  `Textures\RGB`, `Textures\Index8`, and `Textures\Compressed` assets across
  Data, parent mods, and the active mod through Armada's TGA FileExists/OpenRead
  boundary. Its flattened true-colour route expands indexed, grayscale,
  16-bit, and RLE TGA variants before loading, with null-source guards for
  failed minimap textures.
- `A2FOSwarmSystem.dll`: sparse numbered ambient-traffic definitions on any
  rendered host ODF, implemented as shared-model `ST3D_Instance` visuals with
  host-local randomized movement, launch/interaction hardpoint visits, dwell
  and return states, conservative swept host-bounds avoidance, bounded local
  member separation and hardpoint occupancy, native worker-bee visibility
  policy, and automatic lifecycle reconstruction without creating gameplay
  objects;
- `A2FOTurrets.dll`: the global semantic `turret -> sensor` classlabel,
  indexed parent-mount parsing, linked child-object lifecycle, target-driven
  yaw/pitch transforms, ownership propagation, and save/load reconnection. Its
  class-construction, simulation, and cleanup hooks explicitly chain Fleet
  Operations' pre-existing checked detours. Its simulation and cleanup hooks
  provide the single-owner object bridge used by
  `A2FOAlwaysShowShields.dll`; A2FOCraftIdentity supplies that module's common
  ship/station CraftClass registration bridge.

`A1Compat.dll` is an optional globally installed module selected by the
`STA1 Classic` parent's `[modules]` requirements. It owns A1-only policy,
beginning with `wingman -> craft`, missing-only `a2craft.odf`, `a2const.odf`,
and `a2freight.odf` defaults, Armada 1 `Addon` ODF precedence, the
starbase officer-quarter system, and the signature-checked legacy nebula
sprite-node guard. Its activation marker and required policy therefore enable
it only when `STA1 Classic` or one of its children is selected.

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
- `core/module_policy.*` owns host-testable `[modules]` parsing, inherited
  constraints, legacy compatibility, and `activeX` persistence.
- `core/module_loader.*` owns global DLL discovery, deterministic ordering,
  registration transactions, and shutdown ordering.
- `core/module_menu.*` owns the supported Fleet Operations Mods-screen button,
  selector dialog, and launch-requirement validation.
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
`ParentMod`, then the active mod. Native modules use that order when reading
inherited configuration and assets, with later roots overriding earlier
values. Native DLLs do not participate in this overlay: they are
discovered only under `Data/modules` and filtered by the root chain's
`[modules]` rules before deterministic loading. This prevents a mod from
silently supplying or replacing executable code.

## Registration transactions

Each `A2FO_ModuleInit` is a transaction. The core records its dispatcher
registrations and ownership claims. If initialization returns false or throws
where catchable, the core rolls that module back before continuing. A rejected
DLL is unloaded only
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
  -> first valid claim wins
  -> core finds, constructs, positions and publishes replacement
  -> original explosion continues
```

Every handler declares its required ODF field names at startup. The snapshot is
the case-insensitive union of active declarations, plus `basename`. Native
event pointers are callback-scoped, and the core validates replacement names,
flags, and ownership before acting.

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
Revision 11 appends a checked fixed-size byte writer for data constants and
pointer slots which cannot use the existing CALL/JMP helpers.
Revision 12 appends a copied runtime query for Fleet Operations' resolved
per-mod settings directory so modules do not duplicate `SettingsDirectory`
resolution policy.
Revision 13 adds shared completed-object-class and completed-Race ODF
dispatchers. Revision 14 adds shared WeaponClass, weapon-trigger, and Craft
lifecycle dispatchers. Existing v4 modules continue to receive their original
struct prefix.

See [`../sdk/README.md`](../sdk/README.md) and
[`addresses.md`](addresses.md).
