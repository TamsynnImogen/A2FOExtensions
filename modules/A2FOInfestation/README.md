# A2FOInfestation

Noxter-style delayed infestation for Fleet Operations.

## Weapon ODF

Add the following commands to the **special weapon ODF** that performs the
infestation:

```cpp
noxterInfestation = 1
infestationDelay = 12.0
infestationDamage = 500.0
infestationLarva = "nox_larva.odf"
infestationLarvaCount = 4
infestationExplosion = "xfirebsm"
infestationBypassShields = 1
infestationSpawnRadius = 20.0
```

`noxterInfestation` is the opt-in switch. The other commands are ignored when it
is absent or false.

- `infestationDelay` is the hatch delay in seconds. Default: `10.0`.
- `infestationDamage` is one direct damage pulse when hatching. Default: `0`.
- `infestationLarva` is the Craft ODF to release.
- `infestationLarvaCount` is the number released, clamped to `0..64`. Default: `1`.
- `infestationExplosion` is an optional native explosion ODF/project id.
- `infestationBypassShields = 1` makes the hatch damage internal: shields are
  ignored and hull takes the pulse. Default: `1`. Set it to `0` for ordinary
  shield-first damage.
- `infestationSpawnRadius` spreads multiple larvae around the victim instead of
  constructing every larva on the exact same point. Default: `20.0`.

The target is marked only after the native weapon trigger has committed, so
Fleet Operations keeps ownership of technology, energy, cooldown and normal
weapon validation. Friendly, non-Craft and already-infested targets are rejected
at the shared precheck boundary. Infestations do not stack.

## Mother ODF

The module deliberately reuses the Breeder system's existing mother marker:

```cpp
breederMother = 1
```

`A2FOBreeders.dll` does **not** have to be loaded for infestation rallying; both
modules independently consume the same ODF field. On hatching, all released
larvae belong to the team that performed the infestation and receive a native
move order toward that team's nearest live `breederMother`. The destination is
refreshed once per second while the mother moves. If it dies, the larva retargets
the next-nearest mother. If no mother exists yet, the larva remains available
and retries later.

## Repair cure

An infestation is removed when the infected Craft has been admitted to Armada's
native repair queue at a same-team yard. Merely issuing a repair command or
travelling toward the yard does not cure it. This is observed from the native
Craft queue state rather than installing another repair-queue detour, so it can
coexist with `A2FOSquadrons` reinforcement repair hooks.

## Damage behaviour

The hatch pulse goes through native `Craft::Damage`, so fatal damage still uses
Armada's normal death path instead of directly editing hull hitpoints. A zeroed
synthetic ordnance block is supplied around the `DamageInfo`; this keeps the
shared `A2FOWeaponDamageControls` and `A2FODirectionalShields` bridges from
mistaking the internal damage for an authored weapon hit. No subsystem is
selected by the pulse.

The attacker handle is retained for normal kill attribution while the original
infector still exists on the same team. If it has died or changed team before
hatching, the pulse uses attacker handle `0` rather than risking attribution to
a stale/reused handle. Larvae still belong to the original infector team.

## Current limitation

Active infestation countdowns are runtime state and are cleared by
`POST_LOAD`. Saving and loading while a vessel is infested therefore cures that
infestation for now. Persisting the countdown will require an explicit savegame
state bridge rather than guessing at unused Craft memory.

## Built-in availability

`A2FOExtensions.dll` provides this feature under the original
`A2FOInfestation` selection name, with independent menu policy. Prepared
metadata remains resident while inactive. Activation-generation changes drop
pending infections and larva routes; already committed native hatches and larva
effects are not rolled back. The standalone SDK target remains available, but
normal releases do not require its DLL and stale copies are ignored during
discovery.
