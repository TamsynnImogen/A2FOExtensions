# Weapon ODF muzzle flashes

`A2FOMuzzleFlashes.dll` adds optional attached sprite flashes to projectile
weapons, including pulse cannons and torpedo launchers. Enable this module
alongside the other extensions. It does not require an ammunition cost.

## Weapon configuration

Put these settings in the **weapon ODF**, not the ship or projectile ODF:

```ini
// Example sprite identifier: register it in the mod's loaded .spr files first.
muzzleFlashSprite = "my_pulse_muzzle"
muzzleFlashSize = 8.0
muzzleFlashDuration = 0.12
muzzleFlashColor = "1.0 0.65 0.25"
```

For a torpedo weapon, choose a different registered sprite, size, or colour
in that weapon's ODF. These settings do not change the projectile's appearance.

| Setting | Default | Meaning |
| --- | --- | --- |
| `muzzleFlashSprite` | Empty, disabled | Sprite identifier from a loaded `.spr` registry, not a texture filename, SOD, or ordnance ODF. |
| `muzzleFlashSize` | `8.0` | Native particle size in scene units; greater than zero and at most `10000`. |
| `muzzleFlashDuration` | `0.12` | Lifetime in simulation seconds; greater than zero and at most `10`. |
| `muzzleFlashColor` | `"1.0 1.0 1.0"` | Three RGB multipliers, each from `0` to `1`. Sprite material/texture controls blending and transparency. |

The example sprite is a placeholder, not an asset supplied by this module.
Use an existing suitable sprite or register your own muzzle artwork using
the mod's normal sprite setup.

ODF includes are supported. An explicit `muzzleFlashSprite = ""` disables
an inherited flash. Without these settings, existing weapons are unchanged.
Invalid numeric/colour settings disable the flash for that weapon and log a
warning. An unknown sprite is skipped safely, with one warning per loaded
weapon class in `A2FOExtensions.log`.

## Runtime behaviour and limits

- The flash uses the actual firing hardpoint, not the target or the first
  hardpoint listed in the ship ODF. Native particles query its world position
  during rendering, so the effect follows the ship and its hardpoint.
- The creation hook runs after native ordnance construction returns a pointer.
  Targeted, position-targeted, and deliberate-miss creation paths converge
  here. A rejected trigger or dry weapon that creates no projectile produces
  no flash. Repeat shots use their individual projectile creation calls.
- The native particle renderer handles owner visibility and invalid owner
  handles. Simulation handles expiry; this module retains no craft/node
  pointers beyond the shot callback and adds no savegame data or RNG calls.
- This uses the existing native particle budget: up to four concurrent node
  particles per owner and the engine's shared global particle limit. Damage
  and repair effects share that budget, so heavy bursts may omit some flashes.
- This is a sprite flash, not an attached SOD or a dynamic light. It does not
  change damage, arcs, ammunition, shot timing, or cloaking rendering.
- A custom weapon path which bypasses native `OrdnanceClass::Build` is outside
  this hook's coverage.

## Implementation boundary

The module owns one checked detour at Armada RVA `0x18aba0`:
`OrdnanceClass::Build(ST3D_Node*, Matrix34&, Weapon*)`. It forwards all three
arguments and returns the original ordnance pointer unchanged. Native
`Weapon::mCreateTargetedOrdnance` and `mCreateFreeOrdnance` use this builder;
Fleet Ops' cannon firing wrappers dispatch into the weapon creation methods.

The visual uses `NodeParticleEffect::AddParticle` at RVA `0x733c0`. Before
calling it, the module checks the sprite lookup at RVA `0x220750`, because
the native particle renderer assumes the sprite exists. No render detour is
installed. EnergySystems' selection, launch, and shot-timer hooks are untouched.

Build target:

```sh
make build/modules/A2FOMuzzleFlashes.dll
```

In-game acceptance checks still required: configured pulse and torpedo weapons,
rapid bursts, multiple/animated hardpoints, moving ships, visibility/cloaking,
owner destruction, missing sprites, empty ammunition, rejected arcs, and
save/load. Check that unconfigured weapons retain their existing visuals.

## Event-animation notification

The updated DLL also notifies [A2FOAnimations](../modules/A2FOAnimations/README.md)
after a successful projectile build. This runs independently of configured flash
sprites, so automatic weapons can trigger their ODF-name animation with
`animation = 1` on the weapon. The optional bridge preserves the shot, ammunition
behaviour and returned ordnance. Muzzle flashes also work when Animations is absent.
