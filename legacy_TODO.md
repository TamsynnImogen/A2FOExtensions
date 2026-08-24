# Star Trek: Legacy Feature TODO

This is the dedicated research and implementation backlog for useful Star Trek:
Legacy ODF features which could improve Fleet Operations. It is not currently a
commitment to provide general Legacy mod compatibility, M3D model loading, or
SOL map loading.

## Goal

Add the Legacy-derived commands which create worthwhile, ODF-driven RTS
gameplay or solve common modding limitations in Fleet Operations. Preserve the
original command names where their Legacy semantics are appropriate, but prefer
generic replacements over commands hard-coded to a particular race or era.

Every simulation-affecting addition must be deterministic in multiplayer,
survive save/load, validate inherited ODF values, and remain inactive for mods
which do not use it.

## Current Decisions

* [x] Defer native M3D and SOL support. Their likely benefit does not currently
  justify the loader, renderer, conversion, and compatibility work.
* [x] Prioritise tactical and simulation features over Legacy-specific visual
  formats.
* [x] Treat `logoFileNames` as an existing dormant Fleet Operations feature,
  not a new command to implement.
* [ ] Decide whether generic additions belong in `A2FOFeaturePack.dll` or a
  separate optional module after their hook ownership is known.
* [ ] Complete a command-by-command compatibility matrix separating:
  * stock A1/A2 commands;
  * commands used by FO4;
  * commands supported but unused by Fleet Operations;
  * genuinely Legacy-only commands;
  * M3D/SOL-specific data which should remain deferred.

## Existing Legacy-Derived FO Feature

### Per-vessel logos

`logoFileNames` is present in `FleetOpsHook.dll` and was derived from Legacy.
It is intended to act as a parallel list to `possibleCraftNames`: the selected
craft-name index selects the corresponding logo texture.

The local data currently contains 63 parent `Data/odf` definitions and no
definitions in the Fleet Ops 4.0 mod overlay. Legacy itself contains 50 ODFs
which define it. The entries therefore prove data and parser ancestry, but do
not by themselves prove that the complete rendering path remains active.

* [ ] Document the exact list matching, fallback, texture lookup, model node,
  and material behaviour.
* [ ] Confirm whether current Fleet Operations can display a matching logo with
  a minimal synthetic ship fixture.
* [ ] Verify mismatched list lengths, missing textures, inherited lists, random
  names, save/load, replacement vessels, rank changes, and captured ships.
* [ ] Do not add a duplicate parser or hook unless the existing implementation
  is proven incomplete.

## Priority 1: Tactical Combat

### Moddable combat manoeuvres

Candidate commands:

```ini
chanceFlyBy      = 0.60
chanceCircle     = 0.20
chanceCloverleaf = 0.10
chanceChase      = 0.10
```

These should let fast attack craft favour repeated passes while cruisers,
artillery ships, and other classes use appropriately different engagement
patterns.

* [ ] Recover the exact Legacy selection, normalization, retry, and fallback
  semantics.
* [ ] Determine where FO chooses chase and attack movement without replacing
  native formation or special-weapon movement policy.
* [ ] Define behaviour when probabilities total less than, equal to, or greater
  than one.
* [ ] Keep manoeuvre selection deterministic across multiplayer peers.
* [ ] Test stationary targets, moving targets, mixed selections, formations,
  large ships, artillery range, cloaking, and target loss.

### Directional subsystem hit chances

Candidate command families:

```ini
frontHullHitChance    = 100
frontEngineHitChance  = 0
frontShieldHitChance  = 0
frontSensorHitChance  = 0
frontWeaponHitChance  = 0

backHullHitChance     = 40
backEngineHitChance   = 40
backShieldHitChance   = 5
backSensorHitChance   = 5
backWeaponHitChance   = 10

leftHullHitChance     = 100
leftEngineHitChance   = 0
leftShieldHitChance   = 0
leftSensorHitChance   = 0
leftWeaponHitChance   = 0

rightHullHitChance    = 100
rightEngineHitChance  = 0
rightShieldHitChance  = 0
rightSensorHitChance  = 0
rightWeaponHitChance  = 0
```

* [ ] Confirm whether Legacy treats these values as percentages, relative
  weights, or overrides of another subsystem-damage table.
* [ ] Define the attacker-to-target angle boundaries for front, rear, left, and
  right.
* [ ] Preserve native FO subsystem selection when none of these commands are
  present.
* [ ] Decide how directional chances interact with subsystem-disabled states,
  missing systems, shield hits, area damage, special weapons, and self-damage.
* [ ] Add deterministic tests at angle boundaries and with inherited partial
  command sets.

### Subsystem destruction visuals

Candidate commands:

```ini
EngineDestructionEffect     = "psexplosion_engines"
EngineDestructionHardpoints = "hp01"

WeaponDestructionEffect     = "psexplosion_weapons"
WeaponDestructionHardpoints = "hp02" "hp03"
WeaponDestructionSize       = 2.0

SensorDestructionEffect     = "psexplosion_sensors"
SensorDestructionHardpoints = "hp04"
SensorDestructionSize       = 2.0

ShieldDestructionEffect     = "psexplosion_shields"
ShieldDestructionHardpoints = "hp05"

engineBreakNodes          = "Engines"
weaponBreakNodes          = "Weapons"
sensorBreakNodes          = "Sensors"
shieldGeneratorBreakNodes = "Shields"
criticalBreakNodes        = "Neck"
```

Related visual commands to investigate include `damageParticleSystems`,
`ChunkSmokeParticleSystems`, `breakParticleSystemsMajor`,
`breakParticleSystemsSecondary`, `explosionParticleSystemsMajor`,
`explosionParticleSystemsMinor`, and `explosionParticleSystemsSecondary`.

* [ ] Map each effect to existing FO particle, SOD-node, hardpoint, and break
  behaviour before adding a parallel visual system.
* [ ] Define missing effect, hardpoint, and node fallbacks which cannot crash
  the renderer.
* [ ] Confirm effect ownership and cleanup when a ship is repaired, replaced,
  captured, derelict, destroyed, or removed during map shutdown.

## Priority 2: Combat Resources and Facing

### Ordinary weapon-energy and ammunition pools

Candidate commands:

```ini
weaponsEnergyReplenishRate = 0.65
weaponPoolDiminishRate     = 1.10
energyPerPhoton            = 0.10
energyPerPulsePhaser       = 0.01
photonReplenishRate        = 0.90
phaserRechargeRate         = 0.20
photonRechargeRate         = 0.20
fireAllBanks               = 1
photonSpread               = 1
aftPhotonSpread            = 1
numberOfAftPhotons         = 2
```

This could support burst-fire ships, sustained-energy weapons, finite torpedo
magazines, ammunition regeneration, and faction-specific weapon economies
without abusing special energy.

* [ ] Recover the relationship between ship pools, weapon ordnance, banks,
  recharge, replenish, and fire authorization in Legacy.
* [ ] Decide whether FO requires one shared weapon pool, one pool per weapon
  family, or explicit named pools.
* [ ] Define UI feedback for an empty or recharging ordinary weapon pool.
* [ ] Preserve normal FO firing behaviour when the commands are absent.
* [ ] Save and synchronize all new pool state and deterministic recharge
  timing.

### Directional shield proportions

Candidate commands:

```ini
shieldFrontProportion = 1.5
shieldBackProportion  = 0.5
shieldLeftProportion  = 1.0
shieldRightProportion = 1.0
```

* [ ] Determine whether Legacy proportions modify one shared shield pool or
  create independently depleted arcs.
* [ ] Prefer shared-pool weighting for an initial implementation unless Legacy
  evidence or a strong gameplay requirement justifies four persistent pools.
* [ ] Add clear UI feedback before introducing independently depleted arcs.
* [ ] Test shield recharge, shield-disabling weapons, remodulation, transport,
  repair, save/load, capture, and multiplayer.

### Explicit special-weapon timing

Candidate commands:

```ini
specialDurationTimer = 20.0
specialCooldownTimer = 60.0
```

* [ ] Confirm whether these are defined on the launcher, ordnance, command, or
  host craft and whether they are seconds or simulation ticks.
* [ ] Define their interaction with special energy, toggle weapons, research,
  replacement objects, queueing, cancellation, and AI activation.
* [ ] Expose duration and cooldown state to command-button overlays where
  practical.

## Priority 3: Movement and Object Control

### Collision avoidance controls

Candidate commands:

```ini
avoidanceUse                                     = 1
avoidanceMaxLookAheadTime                        = 4.0
avoidanceMinTimeForMaxSteeringUrgency            = 1.0
avoidanceMaxSearchRadius                         = 2000.0
avoidanceTimeToInterpolateBackToDesiredChase     = 1.0
avoidanceBoundingEllipsoidScaleForCraft          = 1.1
avoidanceBoundingEllipsoidScaleForNonCraft       = 1.3
avoidanceHysteresisScaleForAwareness             = 1.015
```

* [ ] Verify exact spelling and defaults from the Legacy parser.
* [ ] Profile FO formation, pathfinding, and collision hot paths before adding
  more per-object searches.
* [ ] Clamp unsafe radii and timing values and preserve stable behaviour when
  objects overlap or cannot find a clear route.
* [ ] Validate large mixed fleets, narrow yards, asteroid fields, construction
  sites, map edges, and multiplayer determinism.

### Ship movement presentation

Lower-priority candidates include `bankAlpha`, `bankOmega`, `maxBankAngle`,
`minBankOmega`, `maxBankOmega`, `minPitchOmega`, `maxPitchOmega`,
`minTurnOmega`, `maxTurnOmega`, `verticalSpeed`, `verticalAccel`,
`sideDragHalfLife`, and `verticalDragHalfLife`.

* [ ] Separate purely visual banking from simulation-affecting turn and
  acceleration changes.
* [ ] Avoid importing Legacy physics wholesale when a small FO-facing control
  provides the useful behaviour.

### Practical object flags and geometry controls

Candidates:

```ini
can_upgrade = 0
can_undock  = 0
has_orders  = 0

cameraRadiusScale = 1.5
pivotRadiusScale  = 1.2
dockingAreaDimensions = 300.0, 160.0, 0.0

suppressesWarp  = 1
easyTractorTarget = 1
avoidanceUse    = 0
```

* [ ] Verify whether an existing FO command already provides each behaviour.
* [ ] Add only controls with a proven runtime consumer and useful failure-safe
  default.
* [ ] Test selection, camera framing, docking, construction, repair, AI orders,
  tractor beams, warp, save/load, and map-created objects.

## Priority 4: Mod and Campaign Systems

### Era availability

Candidate commands:

```ini
startYear = 2370
endYear   = 2380
```

* [ ] Define where a scenario, campaign, or Instant Action setup obtains its
  active year.
* [ ] Filter construction and availability without deleting objects already on
  the map.
* [ ] Prefer generic era or availability lists over Legacy's hard-coded
  `federationTosBuildList`, `federationEntBuildList`,
  `federationTngBuildList`, and equivalent race-specific commands.

### Captains, identity, and experience

Candidate commands:

```ini
hasCaptain           = 1
possibleCaptainNames = "Captain A" "Captain B"
specificCaptain      = "Captain Kirk"
xpDamageGiven        = 100.0
xpDamageTaken        = 100.0
xpDestroyed          = 0.0
```

* [ ] Decide whether captain identity is display-only or can modify gameplay.
* [ ] Integrate experience multipliers with FO veterancy without maintaining a
  second rank system.
* [ ] Preserve selected captain and logo identity through save/load, capture,
  replacement, rank changes, and multiplayer.

### Command and production points

Legacy candidates include `commandPointCost`, `GivesCommandPoints`,
`maxCommandPoints`, and `productionPointCost`.

* [ ] Compare these systems with FO fleet caps, slot costs, officers, resources,
  and technology limits before implementing anything.
* [ ] Prefer extending FO's existing generic cap system over adding a duplicate
  Legacy-only population resource.
* [ ] Consider arbitrary named caps only if they can be exposed consistently to
  the build UI, AI, tooltips, save/load, and multiplayer.

## Deferred or Poor-Fit Commands

Do not prioritise these without a separate proven use case:

* M3D material and shader commands such as `bumpTextureName`,
  `specularTextureName`, `shaderName`, and M3D scale or chunk-generation data;
* M3D chunk-model generation and Legacy damage meshes which do not map cleanly
  to SOD nodes;
* SOL-specific map data;
* hard-coded race/era build-list commands when a generic availability system
  would be more moddable;
* duplicate command-point rules which compete with FO fleet caps;
* internal-looking state commands such as `hasHull`, `enginesLevel`,
  `weaponsLevel`, and similar fields until their actual parser and runtime
  semantics are demonstrated;
* Legacy nebula, Perlin-noise, lens-flare, and other renderer-specific commands
  until the existing FO renderer and texture pipeline are stable.

## Research and Implementation Requirements

For every selected command:

* [ ] Find the exact Legacy parser and runtime consumer; an ODF occurrence or
  executable string alone is not sufficient evidence of behaviour.
* [ ] Check `ArmadaL.exe`, `FleetOpsHook.dll`, and current FO data before
  classifying the command as unsupported.
* [ ] Record the owning module, hook address/RVA, signature, calling convention,
  original bytes, continuation path, and supported executable fingerprints in
  `docs/addresses.md`.
* [ ] Define type, units, valid range, default, inheritance, missing-value,
  malformed-value, and explicit-overrides-fallback behaviour.
* [ ] Keep ordinary FO behaviour byte-for-byte or behaviourally unchanged when
  the new command is absent.
* [ ] Use synthetic fixtures rather than committing proprietary Legacy assets.
* [ ] Add parser, runtime, error-path, lifecycle, save/load, and multiplayer
  tests proportional to the feature's risk.
* [ ] Log unsupported or malformed fields once per source ODF in diagnostic
  builds without spamming release logs.
* [ ] Document confirmed behaviour separately from hypotheses and retain source
  and confidence notes for reverse-engineered findings.

## Suggested Implementation Order

1. Finish the Legacy/FO command and support matrix.
2. Validate the existing `logoFileNames` path and document it.
3. Implement contained object flags, camera/pivot scaling, and explicit special
   timers where clean native hook points exist.
4. Add subsystem destruction effects and break-node customization.
5. Add directional subsystem hit chances.
6. Add moddable combat manoeuvre probabilities.
7. Prototype weapon-energy/ammunition pools with UI and synchronized state.
8. Consider directional shield arcs and collision-avoidance controls only after
   the lower-risk combat features are stable.
9. Add generic era/campaign and cap systems only when an active mod requires
   them.
