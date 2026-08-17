# Directional shields status and remaining work

The first opt-in four-facing implementation is active. The authoritative
modding and runtime contract is documented in
[`modules/A2FODirectionalShields/README.md`](modules/A2FODirectionalShields/README.md),
with its selected-panel graphics and tooltips documented in
[`modules/A2FOCraftIdentity/README.md`](modules/A2FOCraftIdentity/README.md).

## Implemented contract

- `directionalShields = 1` explicitly opts a Craft in; ordinary Craft remain
  on Fleet Operations' native aggregate shield path.
- `forwardShieldStrength`, `aftShieldStrength`, `portShieldStrength`, and
  `starboardShieldStrength` define four positive capacities. They must total an
  explicitly declared `maxShields`; for A1-style ODFs without `maxShields`, the
  four capacities establish the native shield maximum.
- Hull health and repair remain independent. Native `shieldRate` restores the
  aggregate shield budget across depleted facings.
- Weapon-owner position in the target's local axes selects the struck facing.
  A depleted facing passes subsequent damage into Armada's ordinary hull and
  subsystem path even while other facings retain charge.
- Native shield-hit effects are associated with the struck facing. A depleted
  facing suppresses new effects and stops existing tracked effects; live
  effects use that facing's percentage for the stock red-to-green colour.
- `A2FOCraftIdentity` provides the optional `dsf`, `dsb`, `dsl`, and `dsr`
  selected-panel ring, per-Craft segment rectangles, two display modes,
  configurable compass mapping, healthy/low/critical colours, and localized
  normal/verbose tooltips with live `current / maximum` values.
- The numeric F/A and P/S rows remain the safe fallback when the GUI sprites
  are missing or not ready.

`A2FODirectionalShields` and `A2FOWeaponDamageControls` must both be selected;
the latter owns the checked `Craft::Damage` hook and supplies the bridge used
by the directional module.

## Persistence

Facing distribution is not yet appended to save data. Loading currently
distributes the saved native aggregate percentage proportionally across the
four configured facings. A future record must be bounded and versioned, load
older saves safely, and tolerate ODF capacity changes without duplicating or
losing native aggregate shield strength.

## Manual compatibility validation

- [ ] Confirm forward, aft, port, and starboard classification against
  stationary, moving, and rotating ships with beam, pulse, homing, and
  non-homing weapons.
- [ ] Verify exact hull overflow at diagonal boundaries and with weapons that
  bypass, drain, set, invert, or directly recharge shields.
- [ ] Verify collision, nebula, area-effect, chain, timed, scripted, and
  ownerless damage fallbacks.
- [ ] Validate native shield generator damage, regeneration delay, repair
  ships/yards, rank bonuses, upgrade pods, capture, replacement, and derelicts.
- [ ] Check ships, stations, constructors, freighters, and moving stations.
- [ ] Validate the ring, arc hover regions, and fallback rows across the
  supported 16:9, 16:10, 4:3, and 5:4 GUI layouts without covering queues,
  cargo, research, rank, or construction controls.
- [ ] Verify aborting and restarting missions repeatedly rebuilds GUI sprite
  state without stale-pointer crashes.
- [ ] Compare host/client facing values throughout mirrored multiplayer tests
  before advertising deterministic multiplayer support.

## Later possibilities

- Versioned per-facing save/load state.
- Commands to reinforce one facing or balance all facings.
- Transfer rates and efficiency losses.
- AI turning and reinforcement choices based on damaged facings.
- Clickable arc controls.
- Optional dorsal and ventral sectors.
- Weapon-specific directional shield penetration.

These additions must preserve the current passive display and must not change
objects which do not explicitly opt in.
