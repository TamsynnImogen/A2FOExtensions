# A2FOPointDefenseCycles

`A2FOPointDefenseCycles.dll` adds CannonImp-style multi-shot timing to the
existing `PointDefenseLaser` and Fleet Operations `OrdnanceDefenseWeapon`
classlabels. It is timing-only: both classes retain their native target search,
interception, hit chance, ordnance handling, effects, sounds, and attack rules.

## ODF commands

Put the commands in the point-defense **weapon ODF**:

```cpp
classLabel = "PointDefenseLaser"

shotDelay0 = 0.1
shotDelay1 = 0.1
shotDelay2 = 2.0
saveFireCyclePoint = 2
shotCycleResetTime = 15.0
```

The same commands work with:

```cpp
classLabel = "OrdnanceDefenseWeapon"
```

`shotDelay0` is used after the first successful shot/interception. Each later
success advances exactly once to the next numbered delay. After the last entry,
the sequence continues from `saveFireCyclePoint`; with the example above the
delays are `0.1, 0.1, 2.0, 2.0, ...` until reset.

`shotCycleResetTime` starts counting down only after the active shot delay has
elapsed. If the weapon remains ready without firing for that many seconds, its
next successful shot returns to `shotDelay0`. Zero or an omitted command
disables idle reset.

If no numbered delay exists, ordinary unnumbered `shotDelay` is the single
effective delay. The module explicitly enforces it for these point-defense
classes; it does not assume that merely parsing the inherited WeaponClass
field makes the original simulation honor it. If `shotDelay0` exists, the
contiguous numbered sequence takes precedence over unnumbered `shotDelay`.

## Validation

- Numbered entries must be contiguous from `shotDelay0`; a gap invalidates the
  extension policy and retains native timing.
- At most 64 entries (`shotDelay0` through `shotDelay63`) are accepted.
- Delays and reset time must be finite and non-negative.
- `saveFireCyclePoint` must be a whole zero-based index within the effective
  delay sequence. Its default is `0`.
- A malformed policy is logged once when its weapon class is built and falls
  back to the ordinary `shotDelay` behavior.

## Runtime behavior

The module keeps cycle and idle-reset state per Weapon instance. Armada's base
WeaponClass constructor parses `shotDelay`, but stock
`PointDefenseLaser::Simulate` does not use its timer. Roots' existing Jan_B
code cave advances the timer yet checks it only after attempting a shot. This
module replaces that entry with a pre-fire gate: the timer advances before
target search, and the simulation returns while it remains positive. The gate
performs the scalar countdown directly rather than re-entering an engine timer
routine from the patched branch. Once ready, a successful shot enters
`Weapon::mResetShotTimer`; the selected delay is supplied only for that call,
so Fleet Operations' existing reload modifiers are applied unchanged.

`OrdnanceDefenseWeapon` has a separate Fleet Operations timer assignment. The
module replaces that assignment at the confirmed successful firing-attempt
boundary while preserving the surrounding native candidate loop. One actual
point-defense shot therefore advances the cycle once even when several
ordnance candidates were examined during the same simulation tick.

The next delay index and remaining idle-reset time are appended after the
native Weapon fields and restored on load. State is removed by the common
Weapon destructor;
ownership changes keep the same Weapon instance and therefore retain its
cycle, matching CannonImp status-transfer behavior.

All binary hooks require exact signatures from the supported Fleet Operations
Roots build. If preflight fails, the DLL logs that its runtime is disabled and
leaves the game behavior unchanged.
