# Directional shields TODO

Status: design/research only. Do not treat directional shields as implemented
or multiplayer-safe.

## Goal

Add an optional `A2FODirectionalShields.dll` module which divides the native
shield capacity of an opted-in Craft-derived object into four independently
damaged facings:

- fore;
- aft;
- port;
- starboard.

A hit drains the facing struck relative to the object's own bow. Damage which
exceeds the selected facing passes through to ordinary hull and system damage.
Objects which do not opt in must retain Fleet Operations' native shield
behaviour exactly.

The first useful version should be passive: directional damage, automatic
recharge, a read-only selection-display widget, and save/load persistence.
Manual shield reinforcement, clickable quadrants, AI facing tactics, and
six-facing dorsal/ventral shields belong to later research.

## Feasibility evidence

The supported `ArmadaL.exe` retains the stock entry bytes for
`Craft::DamageAlloc` at Armada RVA `0x000c4bb0`. The original symbol signature
is:

```text
Craft::DamageAlloc(
    STDamage*,
    ST3D_Node const*,
    Vector3 const*,
    bool)
```

This supplies the damaged craft, damage record, source node and optional hit
vector before the native routine consumes shield and hull hitpoints. It is the
leading candidate for choosing a shield facing without hooking every weapon
class separately.

Other relevant original Armada RVAs are:

| RVA | Function | Possible use |
| --- | --- | --- |
| `0x000c4270` | `Craft::mRedistributeDamage` | Understand native damage splitting. |
| `0x000c4820` | `Craft::DetermineHitLocation` | Confirm source-node and impact semantics. |
| `0x000c64f0` | `Craft::ShieldsDown` | Preserve global shield-down transitions. |
| `0x000c65a0` | `Craft::RegenerateShields` | Distribute native recharge among facings. |
| `0x000c7a20` | `Craft::AddShields` | Route scripted and special-weapon repairs. |
| `0x000c7a50` | `Craft::GetShields` | Preserve aggregate compatibility. |
| `0x000c8670` | `Craft::ShieldCollapse` | Coordinate global and facing collapse effects. |
| `0x000ca010` | `Craft::DisableShieldGenerator` | Disable all facings consistently. |
| `0x000743b0` | `ShieldEffect::CreateShieldHit` | Suppress effects on an exposed facing. |
| `0x00108900` | `SelectionDisplay::mDrawShieldGauge` | Native selection-display integration. |

Fleet Operations also owns enhanced shield-hit and selection-display render,
post-load, cleanup, and tooltip paths. Any new hook must identify and chain the
live Fleet Operations detour rather than assuming the original prologue is
still present. Every address and byte signature must be recorded in
`docs/addresses.md` before an experimental DLL is deployed.

## Draft ODF contract

Keep `maxShields`, `curShields`, `shieldRate`, shield-generator damage and all
ordinary Fleet Operations commands authoritative. Directional commands only
divide that existing capacity:

```text
directionalShields = 1

foreShieldFraction = 0.25
aftShieldFraction = 0.25
portShieldFraction = 0.25
starboardShieldFraction = 0.25
```

Draft rules:

- The feature is disabled unless `directionalShields` is true.
- Missing fractions default to `0.25` each.
- Fractions must be finite and non-negative.
- A positive total is normalized to 1.0, avoiding accidental creation or loss
  of native maximum shield capacity.
- `maxShields` remains the total capacity presented to existing engine code.
- `curShields` is distributed using the configured fractions on first
  creation, unless restored directional state exists in a save.
- Four facings are fixed in the first version. Sparse or arbitrary named
  sectors are out of scope.
- Final command names remain provisional until the prototype proves the
  engine integration.

Possible later commands, not part of the initial contract:

```text
directionalShieldRechargePolicy = "weakest"
directionalShieldTransferRate = 0
directionalShieldHitFlashTime = 0.35
```

## Facing calculation

* [ ] Prove whether the `Vector3 const*` argument is a world-space impact
  point, incoming direction, or another location for beams and torpedoes.
* [ ] Resolve the source `ST3D_Node` to a world position when the explicit
  vector is absent.
* [ ] Transform the impact/source vector into the damaged object's local
  coordinates.
* [ ] Confirm Armada's local handedness and the sign of the craft's forward
  and right axes in the supported Fleet Operations executable.
* [ ] For the four-sector prototype, compare the absolute forward and lateral
  components:
  * forward-dominant positive: fore;
  * forward-dominant negative: aft;
  * lateral-dominant negative/positive: port/starboard after handedness is
    confirmed.
* [ ] Define exact boundary behaviour for hits at 45 degrees so every peer
  chooses the same facing.
* [ ] Define a deterministic fallback for damage without source or impact
  information. The leading policy is proportional distribution across all
  remaining facings rather than silently choosing fore.
* [ ] Test tall ships and impacts above/below the object even though the first
  version has no dorsal or ventral facings.

## Runtime state

Maintain module-owned sidecar state rather than enlarging Armada objects:

```text
DirectionalShieldState
    object identity / handle
    maximum[fore, aft, port, starboard]
    current[fore, aft, port, starboard]
    lastHitTime[fore, aft, port, starboard]
    configured recharge policy
```

* [ ] Key state by stable engine identity and guard against pointer/handle
  reuse after destruction.
* [ ] Create state only for an opted-in Craft-derived runtime class.
* [ ] Remove state at the shared destruction boundary and during module
  shutdown.
* [ ] Preserve state across capture/team changes.
* [ ] Decide whether rebuilt or replaced objects inherit any directional
  state from their predecessor.
* [ ] Expose read-only aggregate and per-facing queries through a semantic API
  only if another module genuinely needs them.
* [ ] Keep all simulation math deterministic and avoid wall-clock time.

## Damage integration

The native scalar shield value should mirror the sum of all current facings so
existing health displays and most global shield checks continue to work. It
must not be allowed to absorb a hit from the wrong facing.

* [ ] Document the complete `STDamage` layout, flags and damage-type values
  before modifying a live record.
* [ ] Confirm which portion of `Craft::DamageAlloc` applies shield absorption,
  system damage, crew damage, special damage and hull overflow.
* [ ] Prototype a checked detour which classifies the impact and records the
  native before/after values without changing gameplay.
* [ ] Choose between:
  * a narrow replacement of the shield-absorption portion followed by the
    native hull/system path for residual damage; or
  * a carefully bounded native-call shim which temporarily presents only the
    selected facing, captures the result, then restores the aggregate.
* [ ] Reject any design which recursively invokes `DamageAlloc`, loses damage
  flags, applies damage twice, or exposes temporary shield values to another
  simulation callback.
* [ ] Fully absorb damage while the selected facing has capacity.
* [ ] Pass exact overflow to native hull/system damage when the facing reaches
  zero.
* [ ] Ensure a depleted facing produces no shield bubble at that impact even
  while other facings remain charged.
* [ ] Preserve native critical-hit, subsystem, crew, difficulty modifier,
  shield-protection and shield-bypass behaviour.
* [ ] Decide how a weapon which drains or sets shields rather than dealing
  ordinary damage affects the four facings.
* [ ] Verify that linked hull turrets with `shieldProtection = 0` remain
  independently damageable and are not accidentally protected by the parent
  ship's directional state.

## Recharge and global shield operations

The initial policy should require no player micromanagement. Native
`shieldRate` remains the total recharge budget per second.

Leading initial policy:

1. do not recharge while the native shield delay is active;
2. give the recharge budget to the facing with the lowest percentage;
3. move to the next-lowest facing when it is full;
4. never create more aggregate capacity than `maxShields`.

* [ ] Chain or replace `Craft::RegenerateShields` without applying native
  recharge a second time.
* [ ] Route `Craft::AddShields` and other direct restoration to the weakest
  facing first, unless a special weapon has explicit directional semantics.
* [ ] Distribute `SetShieldPercent` and maximum-shield changes predictably.
* [ ] Recalculate facing maxima after rank, officer, upgrade or team bonuses
  while preserving each facing's percentage where possible.
* [ ] Treat a disabled or destroyed shield generator as disabling all four
  facings without unintentionally deleting their stored charge if the native
  effect is temporary.
* [ ] Trigger the ordinary global collapse/down behavior when aggregate
  shields reach zero.
* [ ] Define whether one collapsed facing counts as globally shields-down for
  transport, boarding and other binary native checks. The first-version
  recommendation is no: those checks remain global until every facing is
  down.

## Selection-display UI

The agreed visual direction is a compact four-quadrant widget in the central
command information panel:

```text
                    FORE 100%
                         ^
              PORT 98% [ship] STARBOARD 99%
                         v
                     AFT 96%
```

The ship marker always points upward and represents the selected object's bow,
not the camera orientation. Four curved arcs surround it and provide the fast
visual read shown in the UI mock-up.

Recommended rendering approach:

- one static background/ring sprite;
- one central generic ship-orientation sprite;
- separate fore, aft, port and starboard overlay sprites;
- approximately 21 fill frames per overlay in 5% steps;
- exact percentages drawn as ordinary text;
- green above 50%, yellow at 21-50%, red at 1-20%, and dark at zero;
- an optional brief bright hit frame on the facing most recently struck;
- aggregate shield percentage or points beside the existing shield icon;
- ordinary hull, crew and readiness information retained beside the widget.

* [ ] Locate the live Fleet Operations selection-display render chain and
  prove a passive overlay can be drawn without disturbing native controls.
* [ ] Show the widget only for one selected directional-shield object.
* [ ] Preserve the native display for ordinary objects and multiple selection.
* [ ] Check ships, stations, constructors, freighters and Producer-derived
  objects whose central panel already shows contextual information.
* [ ] Avoid overlap with ten-slot build queues, research progress, cargo,
  rally-point and rank displays.
* [ ] Validate `gui_16x9.cfg`, `gui_16x10.cfg`, `gui_4x3.cfg` and
  `gui_5x4.cfg` layouts at their supported resolutions.
* [ ] Add per-facing tooltips such as
  `Fore shields: 812 / 1000 - recharging` through the existing Fleet
  Operations tooltip chain.
* [ ] Keep the first widget display-only. Clickable arcs and manual transfer
  require separate input, command and synchronization design.
* [ ] Confirm observer/replay and non-owned-object displays reveal no more
  information than ordinary shield status already does.

## Save/load

* [ ] Design a bounded versioned save record containing object identity,
  maxima, current values, recharge delay and any later transfer state.
* [ ] Restore saved state regardless of whether the object or module record is
  loaded first.
* [ ] Avoid duplicating initialization when a directional object reconnects to
  saved sidecar state.
* [ ] Define loading behavior when the object's ODF fractions changed after
  the save was written.
* [ ] Load old saves without directional records by distributing the native
  aggregate shield value according to current ODF fractions.
* [ ] Ensure removing the module or disabling the ODF feature leaves a save
  with an ordinary valid aggregate shield value.
* [ ] Test save/load while fully charged, partially damaged, one-facing-down,
  globally down, recharging, disabled, under repair and immediately after a
  hit.

## Compatibility matrix

* [ ] Beam weapons and pulse weapons.
* [ ] Homing and non-homing torpedoes.
* [ ] Area-effect, chain and timed damage.
* [ ] Collision, nebula, map hazard, self and scripted damage.
* [ ] Shield-bypass and shield-protection modifiers.
* [ ] Shield disruptor, shield-removing torpedo, shield inversion,
  remodulation, emitters and direct shield-recharge effects.
* [ ] Cloaking variants which retain or drop shields.
* [ ] Transport, boarding and shield-gated special actions.
* [ ] Repair yards, repair ships and automatic regeneration.
* [ ] Rank, officer, upgrade-pod and difficulty modifiers.
* [ ] Capture, team transfer, replacement, recrewing and derelicts.
* [ ] Ships, stations, moving stations and linked hull-turret parents.
* [ ] Multiple selection, control groups, observers and replays.
* [ ] Existing saves, new saves and saves loaded without the module.

## Multiplayer and determinism

* [ ] Prove impact classification uses synchronized simulation values on every
  peer.
* [ ] Use fixed branch boundaries and identical floating-point operations for
  sector selection and recharge distribution.
* [ ] Confirm UI-only hit flashes never feed simulation state.
* [ ] Run repeated mirrored tests and compare per-facing state after every hit.
* [ ] Test host/client save loading, reconnect behavior and long battles.
* [ ] Treat the module as single-player experimental until the complete matrix
  passes. Never advertise multiplayer safety based only on a no-crash test.

## Phased plan

### Phase 0 - research only

* [ ] Verify every candidate byte signature against the supported
  `ArmadaL.exe` and live Fleet Operations detours.
* [ ] Recover the damage structure and relevant Craft field layouts.
* [ ] Document hook ownership and safe chaining.

### Phase 1 - observational proof

* [ ] Add an opt-in test module which logs fore/aft/port/starboard
  classification without changing damage.
* [ ] Manually confirm classification with stationary and rotating test ships
  attacked from all four directions.
* [ ] Confirm fallbacks for damage without an impact vector.

### Phase 2 - passive combat prototype

* [ ] Add four sidecar pools and directional absorption on one test ship.
* [ ] Confirm exact hull overflow and exposed-facing shield-effect suppression.
* [ ] Keep native aggregate shield reporting valid.

### Phase 3 - recharge and lifecycle

* [ ] Integrate recharge, direct shield changes, generator state, destruction,
  capture and bonuses.
* [ ] Complete single-player compatibility tests before UI work expands.

### Phase 4 - selection-display widget

* [ ] Add sprite assets, percentage text, state colours, tooltips and hit
  flashes following the approved mock-up.
* [ ] Validate every supported aspect ratio and contextual selection display.

### Phase 5 - persistence

* [ ] Add versioned save/load state and old-save fallback.
* [ ] Complete the save/load matrix before broader deployment.

### Phase 6 - special cases and multiplayer

* [ ] Complete the compatibility matrix, profiling, determinism checks and
  multiplayer testing.

## Later possibilities

These must not block the passive four-facing version:

- command buttons to reinforce fore, aft, port or starboard;
- balance-all-facings command;
- transfer rates and efficiency losses;
- AI selection of a facing to reinforce;
- AI turning behavior which protects a damaged facing;
- clickable quadrants in the selection display;
- dorsal and ventral sectors;
- weapon-specific directional shield penetration;
- race-specific widget art and shield behavior.

## Explicit non-goals for the first version

- replacing ordinary shields globally;
- modifying objects which do not opt in;
- treating four sectors as four full copies of `maxShields`;
- using linked child objects as shield collision proxies;
- manual power transfer or clickable UI;
- tactical AI changes;
- dorsal/ventral facings;
- claiming multiplayer support before deterministic testing;
- synthetic input automation for in-game tests.

## First release gate

The feature is not ready for an experimental release until all of the
following are true:

* [ ] Ordinary non-directional objects are byte-for-byte behaviorally
  unaffected at the candidate hooks.
* [ ] Four-direction weapon tests select the correct facing while the ship is
  stationary, moving and rotating.
* [ ] Facing depletion and hull overflow conserve damage exactly.
* [ ] Recharge and direct shield changes never exceed native maximum shields.
* [ ] The widget is correct at every supported aspect ratio and never covers
  contextual controls.
* [ ] Save/load restores all four facings without duplication or reset.
* [ ] Unsupported damage types have an explicit tested fallback.
* [ ] A rollback DLL and hashes are recorded before deployment.
* [ ] All automated checks pass, followed by manual in-game validation without
  synthetic input automation.
