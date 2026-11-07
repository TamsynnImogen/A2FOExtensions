# Physics, captain tables, nebulae and configuration

See [README](README.md) for confidence definitions and [the index](command-index.md) for all families. Documented means authored ODF comments describe the intent; it does not mean tested runtime support.

<a id="albedo"></a>

### `albedo`

**Spellings:** `Albedo`.

**Documented / high confidence**; role: presentation.

Controls how much light scatters inside the nebula cloud. The source describes scattering, rather than object surface reflectance.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
0.9
```

Uncertainty: The scattering equation, clamp range and interaction with Extinction are unknown.

Next investigation: Find how nebula rendering combines Albedo and Extinction.

Evidence: [odf/other/mnebula6.odf:106](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:106>) — Authored comments and/or surrounding definitions with observed value 0.9.

<a id="attacksoundgroup"></a>

### `attacksoundgroup`

**Spellings:** `attackSoundGroup`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with nebula attacks/lightning; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 8 files (8 assignments).

Examples:

```text
"ST_GRP_Nebula_Lightning"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes attacksoundgroup.

Evidence: [odf/other/mnebula6.odf:21](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:21>) — Authored comments and/or surrounding definitions with observed value "ST_GRP_Nebula_Lightning".

<a id="avoidanceuse"></a>

### `avoidanceuse`

**Spellings:** `avoidanceUse`.

**Inferred / medium confidence**; role: gameplay.

Likely enables or disables collision-avoidance steering for an object; flying asteroid definitions set zero.

Value form: numeric scalar with observed trailing semicolon. Units: observed 0/1 flag. Observed in 6 files (6 assignments).

Examples:

```text
0;
```

Uncertainty: Zero may disable only a specific avoidance controller, not collision geometry or all steering.

Next investigation: Find which avoidance controller checks the flag and how it differs from collision prevention.

Evidence: [odf/other/flyingasteroid.odf:108](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/flyingasteroid.odf:108>) — Authored comments and/or surrounding definitions with observed value 0;.

<a id="bankalpha"></a>

### `bankalpha`

**Spellings:** `bankAlpha`.

**Documented / medium confidence**; role: gameplay.

Intended banking acceleration/deceleration. Class physics comments describe that role, but the base file says it appears ineffective in destination and direct-control modes.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 12 files (12 assignments).

Examples:

```text
1.0;
1;
.7;
```

Uncertainty: Conflicting comments suggest an obsolete or controller-specific field, not a verified feature.

Next investigation: Find any live consumer of bankAlpha and identify the control mode.

Evidence: [odf/other/craft_physics.odf:51](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:51>) — Authored comments and/or surrounding definitions with observed value 1;; [odf/other/k_brel_physics.odf:18](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/k_brel_physics.odf:18>) — Authored comments and/or surrounding definitions with observed value 3;.

<a id="bankalphafractionatrest"></a>

### `bankalphafractionatrest`

**Spellings:** `bankAlphaFractionAtRest`.

**Inferred / low confidence**; role: gameplay.

An old bank angular-acceleration tuning field, apparently intended as the at-rest fraction of angular acceleration. The base groups these settings under a comment saying they do not seem to do anything.

Value form: numeric scalar with observed trailing semicolon. Units: dimensionless fraction intended; runtime use unknown. Observed in 9 files (9 assignments).

Examples:

```text
.1;
1;
1.3333333333333333;
```

Uncertainty: Its intended analogy with Omega fields does not establish an active Alpha consumer.

Next investigation: Locate a live read-and-use path for bankalphafractionatrest, not merely a parser read.

Evidence: [odf/other/craft_physics.odf:155](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:155>) — Authored comments and/or surrounding definitions with observed value 1.3333333333333333;; [odf/other/class_scout_physics.odf:60](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:60>) — Authored comments and/or surrounding definitions with observed value 1;.

<a id="bankalphamaxspeedfraction"></a>

### `bankalphamaxspeedfraction`

**Spellings:** `bankAlphaMaxSpeedFraction`.

**Inferred / low confidence**; role: gameplay.

An old bank angular-acceleration tuning field, apparently intended as the speed fraction for reaching nominal angular acceleration. The base groups these settings under a comment saying they do not seem to do anything.

Value form: numeric scalar with observed trailing semicolon. Units: dimensionless fraction intended; runtime use unknown. Observed in 7 files (7 assignments).

Examples:

```text
.25;
```

Uncertainty: Its intended analogy with Omega fields does not establish an active Alpha consumer.

Next investigation: Locate a live read-and-use path for bankalphamaxspeedfraction, not merely a parser read.

Evidence: [odf/other/craft_physics.odf:158](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:158>) — Authored comments and/or surrounding definitions with observed value .25;.

<a id="bankomega"></a>

### `bankomega`

**Spellings:** `bankOmega`.

**Documented / high confidence**; role: gameplay.

Defines a maximum banking rate in older class physics definitions. The newer base uses minBankOmega/maxBankOmega endpoints; its comments say doubling the parameter doubles bank rate.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
1.5;
```

Uncertainty: Single-field versus endpoint-field precedence and direct-control behavior are unknown.

Next investigation: Determine which physics class consumes bankOmega and how it combines with min/maxBankOmega.

Evidence: [odf/other/class_science_physics.odf:31](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_science_physics.odf:31>) — Authored comments and/or surrounding definitions with observed value 1.5;.

<a id="bankomegafractionatrest"></a>

### `bankomegafractionatrest`

**Spellings:** `bankOmegaFractionAtRest`.

**Inferred / medium confidence**; role: gameplay.

Controls banking responsiveness at zero throttle relative to its bank-rate parameter, by analogy with the documented turnOmegaFractionAtRest family and explicit class comments.

Value form: numeric scalar with observed trailing semicolon. Units: relative factor. Observed in 10 files (10 assignments).

Examples:

```text
.6;
.2222;
```

Uncertainty: The base warns that fractions above one use a nontrivial mapping for turn rates; banking may differ.

Next investigation: Recover the zero-throttle banking calculation rather than assuming a simple multiplier.

Evidence: [odf/other/class_scout_physics.odf:59](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:59>) — Authored comments and/or surrounding definitions with observed value .2222;; [odf/other/class_science_physics.odf:43](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_science_physics.odf:43>) — Authored comments and/or surrounding definitions with observed value .6;.

<a id="bankomegamaxspeedfraction"></a>

### `bankomegamaxspeedfraction`

**Spellings:** `bankOmegaMaxSpeedFraction`.

**Inferred / medium confidence**; role: gameplay.

Sets the speed fraction at which bank responsiveness reaches its nominal rate; below this, the documented turn example blends from its at-rest fraction.

Value form: numeric scalar with observed trailing semicolon. Units: fraction of maximum ship speed. Observed in 7 files (7 assignments).

Examples:

```text
1;
```

Uncertainty: The turn comment is internally inconsistent about endpoints and accidentally mentions bankOmega; the exact curve needs consumer evidence.

Next investigation: Recover the bank rate-versus-throttle curve and behavior for fractions outside 0..1.

Evidence: [odf/other/craft_physics.odf:76](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:76>) — Authored comments and/or surrounding definitions with observed value 1;.

<a id="blobchaos"></a>

### `blobchaos`

**Spellings:** `BlobChaos`.

**Unresolved / low confidence**; role: presentation.

An unexplained scalar in the nebula blob-generation block. It may vary placement or shape disorder, but all observed values are zero and no comment defines the operation.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
0
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Find its geometry consumer and compare nonzero values.

Evidence: [odf/other/mnebula6.odf:89](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:89>) — Authored comments and/or surrounding definitions with observed value 0.

<a id="blobdensity"></a>

### `blobdensity`

**Spellings:** `BlobDensity`.

**Documented / high confidence**; role: presentation.

Controls the number/density of blobs used to construct procedural nebula geometry; the comment calls it number of blobs even though the value is fractional.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
2.5
```

Uncertainty: A value of 2.5 prevents treating this as a literal integer count.

Next investigation: Resolve count versus density and the conversion to generated blobs.

Evidence: [odf/other/mnebula6.odf:86](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:86>) — Authored comments and/or surrounding definitions with observed value 2.5.

<a id="blobsize"></a>

### `blobsize`

**Spellings:** `BlobSize`.

**Inferred / medium confidence**; role: presentation.

Likely sets the size of the blobs forming the nebula, alongside BlobDensity and SizeVariance.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
600
```

Uncertainty: The file provides no radius-versus-diameter definition or physical units.

Next investigation: Determine what geometric measurement BlobSize scales.

Evidence: [odf/other/mnebula6.odf:87](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:87>) — Authored comments and/or surrounding definitions with observed value 600.

<a id="breakmajorsoundgroup"></a>

### `breakmajorsoundgroup`

**Spellings:** `breakMajorSoundGroup`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with major component-break explosions; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 17 files (17 assignments).

Examples:

```text
"ST_GRP_Asteroid_Rock"
"ST_GRP_EXP_Ship_Medium"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes breakmajorsoundgroup.

Evidence: [odf/ships/craft.odf:217](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:217>) — Authored comments and/or surrounding definitions with observed value "ST_GRP_EXP_Ship_Medium"; [odf/other/flyingasteroid.odf:135](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/flyingasteroid.odf:135>) — Authored comments and/or surrounding definitions with observed value "ST_GRP_Asteroid_Rock".

<a id="breakminorsoundgroup"></a>

### `breakminorsoundgroup`

**Spellings:** `breakMinorSoundGroup`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with secondary/minor component-break explosions; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 17 files (17 assignments).

Examples:

```text
"ST_GRP_Asteroid_Rock"
"ST_GRP_EXP_Ship_Small"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes breakminorsoundgroup.

Evidence: [odf/ships/craft.odf:218](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:218>) — Authored comments and/or surrounding definitions with observed value "ST_GRP_EXP_Ship_Small"; [odf/other/flyingasteroid.odf:136](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/flyingasteroid.odf:136>) — Authored comments and/or surrounding definitions with observed value "ST_GRP_Asteroid_Rock".

<a id="breakparticlesystemsmajor"></a>

### `breakparticlesystemsmajor`

**Spellings:** `breakParticleSystemsMajor`.

**Documented / high confidence**; role: asset.

Lists particle systems for the initial explosion when a component such as a nacelle breaks off.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 37 files (37 assignments).

Examples:

```text
"pssparks_02" "psexplosion_5"
"psexplosion_minor3"
"psexplosion_blast_wide3_borg"
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Determine attachment, list playback and component-break event ownership.

Evidence: [odf/other/test.odf:35](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/test.odf:35>) — Authored comments and/or surrounding definitions with observed value "pssparks_02" "psexplosion_5"; [odf/ships/F_Norway.odf:136](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Norway.odf:136>) — Authored comments and/or surrounding definitions with observed value "pssparks_02" "psexplosion_3""psexplosion_7".

<a id="breakparticlesystemssecondary"></a>

### `breakparticlesystemssecondary`

**Spellings:** `breakParticleSystemsSecondary`.

**Documented / high confidence**; role: asset.

Lists effects for the precursor/secondary explosion before the main nacelle-break explosion.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 35 files (35 assignments).

Examples:

```text
"pssparks_02"
"explosionb"
"psexplosion_blast_wide4"
```

Uncertainty: The ship comment establishes ordering but not an exact delay for this field.

Next investigation: Recover relative event timing and attachment points.

Evidence: [odf/ships/craft.odf:154](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:154>) — Authored comments and/or surrounding definitions with observed value "psexplosion_blast_wide2"; [odf/ships/class_scout.odf:139](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_scout.odf:139>) — Authored comments and/or surrounding definitions with observed value "explosionb".

<a id="can-undock"></a>

### `can_undock`

**Spellings:** `can_undock`.

**Inferred / medium confidence**; role: ui.

Marks whether the contextual command menu offers undocking; shipyards set 1 while a transwarp portal sets 0.

Value form: numeric scalar. Units: observed 0/1 flag. Observed in 2 files (2 assignments).

Examples:

```text
0
1
```

Uncertainty: The UI flag does not itself prove a gameplay operation exists or can execute.

Next investigation: Determine the menu filter and whether the order dispatcher also checks can_undock.

Evidence: [odf/stations/shipyard.odf:59](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/shipyard.odf:59>) — Authored comments and/or surrounding definitions with observed value 1; [odf/other/transwarp_portal.odf:63](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/transwarp_portal.odf:63>) — Authored comments and/or surrounding definitions with observed value 0.

<a id="can-upgrade"></a>

### `can_upgrade`

**Spellings:** `can_upgrade`.

**Inferred / medium confidence**; role: ui.

Marks whether the contextual command menu offers upgrading; shipyards set 1 while a transwarp portal sets 0.

Value form: numeric scalar. Units: observed 0/1 flag. Observed in 3 files (3 assignments).

Examples:

```text
0
1
```

Uncertainty: The UI flag does not itself prove a gameplay operation exists or can execute.

Next investigation: Determine the menu filter and whether the order dispatcher also checks can_upgrade.

Evidence: [odf/stations/shipyard.odf:60](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/shipyard.odf:60>) — Authored comments and/or surrounding definitions with observed value 1; [odf/other/transwarp_portal.odf:64](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/transwarp_portal.odf:64>) — Authored comments and/or surrounding definitions with observed value 0.

<a id="colorindex"></a>

### `color<N>`

**Spellings:** `color0`, `color1`, `color10`, `color11`, `color12`, `color13`, `color14`, `color15`, `color16`, `color17`, `color18`, `color2`, `color3`, `color4`, `color5`, `color6`, `color7`, `color8`, `color9`.

**Inferred / medium confidence**; role: presentation.

Supplies the three color components for indexed flare layer N.

Value form: space-separated numeric tuple. Units: three color components. Observed in 1 files (19 assignments).

Examples:

```text
0.12 0.07 0.02
0.2 0.2 0.2
0.156 0.216 0.306
```

Uncertainty: Layer indexing is observed; coordinate frame, scale and exact resize rule are not documented.

Next investigation: Determine color normalization, alpha handling and compositing.

Evidence: [odf/other/lens_flare.odf:18](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:18>) — Authored comments and/or surrounding definitions with observed value 0.12 0.07 0.02; [odf/other/lens_flare.odf:24](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:24>) — Authored comments and/or surrounding definitions with observed value 0.2 0.2 0.2.

<a id="commandpointcost"></a>

### `commandpointcost`

**Spellings:** `commandPointCost`.

**Inferred / medium confidence**; role: gameplay.

Likely a command-point acquisition/selection cost for the object; the station base places it in its Cost block. This may be roster purchasing rather than a continuously reserved population cap.

Value form: numeric scalar. Units: command points suggested by name. Observed in 138 files (138 assignments).

Examples:

```text
0
5000
2000
```

Uncertainty: Spending, reservation, refunds and relation to production points are unverified.

Next investigation: Find the ship-selection/production point consumer and determine when points are deducted or released.

Evidence: [odf/stations/station.odf:143](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:143>) — Authored comments and/or surrounding definitions with observed value 0; [odf/ships/craft.odf:40](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:40>) — Authored comments and/or surrounding definitions with observed value 0.

<a id="decalnames"></a>

### `decalnames`

**Spellings:** `decalNames`.

**Inferred / medium confidence**; role: asset.

Provides named decal definitions such as damageBurn, damageMelt and damageScorch, apparently a resource catalogue for damage decals.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"damageBurn" "damageMelt" "damageScorch"
```

Uncertainty: Enumeration versus preload behavior and reference lookup are unknown.

Next investigation: Trace registration of names and their use by hull damage effects.

Evidence: [odf/other/decal.odf:1](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/decal.odf:1>) — Authored comments and/or surrounding definitions with observed value "damageBurn" "damageMelt" "damageScorch".

<a id="dedicatedrepairpoints"></a>

### `dedicatedrepairpoints`

**Spellings:** `dedicatedRepairPoints`.

**Documented / medium confidence**; role: gameplay.

Overrides cfg_DedicatedRepairPoints from the configuration. Repair craft and normal craft use different values, suggesting a dedicated repair allocation or rate.

Value form: numeric scalar. Units: unknown. Observed in 17 files (17 assignments).

Examples:

```text
.00001
0.0
16.0
```

Uncertainty: The override is explicit; whether the quantity is points per update, per second or a repair budget is not defined.

Next investigation: Recover the overridden configuration consumer and repair allocation units.

Evidence: [odf/ships/Repair_borg.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/Repair_borg.odf:5>) — Authored comments and/or surrounding definitions with observed value 16.0; [odf/ships/Repair_borgent.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/Repair_borgent.odf:5>) — Authored comments and/or surrounding definitions with observed value 10.0.

<a id="defaultsize"></a>

### `defaultsize`

**Spellings:** `DefaultSize`.

**Documented / high confidence**; role: presentation.

Specifies the nebula extents as a three-component vector.

Value form: comma-separated numeric tuple. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
900, 400, 900
```

Uncertainty: Axis order and full extents versus half extents are not specified.

Next investigation: Recover axis ordering and bounding-volume construction.

Evidence: [odf/other/mnebula6.odf:118](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:118>) — Authored comments and/or surrounding definitions with observed value 900, 400, 900.

<a id="distanceindex"></a>

### `distance<N>`

**Spellings:** `distance0`, `distance1`, `distance10`, `distance11`, `distance12`, `distance13`, `distance14`, `distance15`, `distance16`, `distance17`, `distance18`, `distance2`, `distance3`, `distance4`, `distance5`, `distance6`, `distance7`, `distance8`, `distance9`.

**Inferred / medium confidence**; role: presentation.

Positions indexed flare layer N along the flare arrangement, with observed negative and greater-than-one values.

Value form: numeric scalar. Units: unknown. Observed in 1 files (19 assignments).

Examples:

```text
-0.20
-0.30
0.5
```

Uncertainty: Layer indexing is observed; coordinate frame, scale and exact resize rule are not documented.

Next investigation: Recover the source-to-screen-center axis and origin convention.

Evidence: [odf/other/lens_flare.odf:17](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:17>) — Authored comments and/or surrounding definitions with observed value -0.20; [odf/other/lens_flare.odf:23](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:23>) — Authored comments and/or surrounding definitions with observed value -0.30.

<a id="easytractortarget"></a>

### `easytractortarget`

**Spellings:** `easyTractorTarget`.

**Inferred / low confidence**; role: gameplay.

Likely marks an object as eligible/easier for tractor-beam interaction; asteroid examples set one.

Value form: numeric scalar with observed trailing semicolon. Units: observed 0/1 flag. Observed in 7 files (7 assignments).

Examples:

```text
1;
```

Uncertainty: Target eligibility, strength scaling and any size/resistance bypass are not specified.

Next investigation: Identify the targeting or tractor-force branch controlled by easyTractorTarget.

Evidence: [odf/other/flyingasteroid.odf:110](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/flyingasteroid.odf:110>) — Authored comments and/or surrounding definitions with observed value 1;.

<a id="edenginesound"></a>

### `edenginesound`

**Spellings:** `edEngineSound`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with energy-distribution feedback for engines; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"ST_ENG_BOR_Energy_Engines"
"ST_ENG_FED_Energy_Engines"
"ST_ENG_KLI_Energy_Engines"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes edenginesound.

Evidence: [odf/other/fed.odf:22](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:22>) — Authored comments and/or surrounding definitions with observed value "ST_ENG_FED_Energy_Engines"; [odf/other/rom.odf:27](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:27>) — Authored comments and/or surrounding definitions with observed value "ST_ENG_ROM_Energy_Engines".

<a id="edshieldsound"></a>

### `edshieldsound`

**Spellings:** `edShieldSound`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with energy-distribution feedback for shields; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"ST_ENG_BOR_Energy_Shields"
"ST_ENG_FED_Energy_Shields"
"ST_ENG_KLI_Energy_Shields"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes edshieldsound.

Evidence: [odf/other/fed.odf:23](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:23>) — Authored comments and/or surrounding definitions with observed value "ST_ENG_FED_Energy_Shields"; [odf/other/rom.odf:28](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:28>) — Authored comments and/or surrounding definitions with observed value "ST_ENG_ROM_Energy_Shields".

<a id="edweaponsound"></a>

### `edweaponsound`

**Spellings:** `edWeaponSound`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with energy-distribution feedback for weapons; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"ST_ENG_BOR_Energy_Weapons"
"ST_ENG_FED_Energy_Weapons"
"ST_ENG_KLI_Energy_Weapons"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes edweaponsound.

Evidence: [odf/other/fed.odf:24](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:24>) — Authored comments and/or surrounding definitions with observed value "ST_ENG_FED_Energy_Weapons"; [odf/other/rom.odf:29](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:29>) — Authored comments and/or surrounding definitions with observed value "ST_ENG_ROM_Energy_Weapons".

<a id="endyear"></a>

### `endyear`

**Spellings:** `endYear`.

**Inferred / medium confidence**; role: gameplay.

Likely defines the latest year of availability for the craft/object, forming an era-selection interval.

Value form: numeric scalar. Units: year number. Observed in 131 files (131 assignments).

Examples:

```text
1999
2200
2400
```

Uncertainty: Inclusive endpoints, availability UI versus build filtering and out-of-era objects are unverified.

Next investigation: Find the era/ship-selection filter and determine interval boundary handling for endyear.

Evidence: [odf/ships/K_D_3.odf:41](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_3.odf:41>) — Authored comments and/or surrounding definitions with observed value 2300; [odf/ships/F_Nova.odf:65](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Nova.odf:65>) — Authored comments and/or surrounding definitions with observed value 2400.

<a id="enginemaxhitpoints"></a>

### `enginemaxhitpoints`

**Spellings:** `engineMaxHitPoints`.

**Inferred / medium confidence**; role: gameplay.

Sets the high hit-point endpoint for the engine system. Station hull comments label the pair as level-1 and level-5 values.

Value form: numeric scalar with observed trailing semicolon. Units: system hit points. Observed in 7 files (7 assignments).

Examples:

```text
210;
```

Uncertainty: Repeated hull assignments occur in one station; runtime duplicate precedence and interpolation are not established.

Next investigation: Recover how the engine level selects or interpolates hit-point capacity.

Evidence: [odf/other/flyingasteroid.odf:97](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/flyingasteroid.odf:97>) — Authored comments and/or surrounding definitions with observed value 210;.

<a id="enginemaxstatistic"></a>

### `enginemaxstatistic`

**Spellings:** `engineMaxStatistic`.

**Inferred / medium confidence**; role: gameplay.

Likely the high engine-performance endpoint paired with the speed scaling factor engineMinStatistic.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 8 files (8 assignments).

Examples:

```text
100;
2
```

Uncertainty: Endpoint scale differs dramatically between base craft and asteroid data.

Next investigation: Determine normalization and interpolation with enginesLevel/min/maxLevel.

Evidence: [odf/ships/craft.odf:69](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:69>) — Authored comments and/or surrounding definitions with observed value 2; [odf/other/flyingasteroid.odf:99](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/flyingasteroid.odf:99>) — Authored comments and/or surrounding definitions with observed value 100;.

<a id="engineminhitpoints"></a>

### `engineminhitpoints`

**Spellings:** `engineMinHitPoints`, `engineMinHitpoints`.

**Inferred / medium confidence**; role: gameplay.

Sets the low hit-point endpoint for the engine system. Station hull comments label the pair as level-1 and level-5 values.

Value form: numeric scalar with observed trailing semicolon. Units: system hit points. Observed in 8 files (8 assignments).

Examples:

```text
170;
100
```

Uncertainty: Repeated hull assignments occur in one station; runtime duplicate precedence and interpolation are not established.

Next investigation: Recover how the engine level selects or interpolates hit-point capacity.

Evidence: [odf/ships/craft.odf:67](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:67>) — Authored comments and/or surrounding definitions with observed value 100; [odf/other/flyingasteroid.odf:96](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/flyingasteroid.odf:96>) — Authored comments and/or surrounding definitions with observed value 170;.

<a id="engineminstatistic"></a>

### `engineminstatistic`

**Spellings:** `engineMinStatistic`.

**Documented / high confidence**; role: gameplay.

The low endpoint for the engine performance statistic used as a speed scaling factor; paired with engineMaxStatistic.

Value form: numeric scalar with observed trailing semicolon. Units: performance scale/rating, normalization unknown. Observed in 8 files (8 assignments).

Examples:

```text
100;
1
```

Uncertainty: craft.odf uses 1..2 while flying-asteroid definitions use 100..100; do not assume one universal scale.

Next investigation: Recover the level-to-statistic mapping and consumers of this endpoint.

Evidence: [odf/ships/craft.odf:68](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:68>) — Authored comments and/or surrounding definitions with observed value 1; [odf/other/flyingasteroid.odf:98](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/flyingasteroid.odf:98>) — Authored comments and/or surrounding definitions with observed value 100;.

<a id="engineslevel"></a>

### `engineslevel`

**Spellings:** `enginesLevel`.

**Documented / high confidence**; role: gameplay.

Sets the general upgradeable engines statistic displayed to the player. The ODF labels these values as system levels, separate from hit-point endpoints.

Value form: numeric scalar. Units: system-level rating, exact scale unknown. Observed in 166 files (166 assignments).

Examples:

```text
900
400
500
```

Uncertainty: The conversion from level to current hit points or performance is not recovered.

Next investigation: Recover how engineslevel feeds damage, performance and upgrade calculations.

Evidence: [odf/ships/K_D_3.odf:27](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_3.odf:27>) — Authored comments and/or surrounding definitions with observed value 875; [odf/ships/K_D_7.odf:29](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_7.odf:29>) — Authored comments and/or surrounding definitions with observed value 710.

<a id="enginestartonesound"></a>

### `enginestartonesound`

**Spellings:** `engineStartOneSound`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with a first engine-start audio stage; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"ST_ENG_BOR_Startup_1"
"ST_ENG_FED_Startup_1"
"ST_ENG_KLI_Startup_1"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes enginestartonesound.

Evidence: [odf/other/fed.odf:26](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:26>) — Authored comments and/or surrounding definitions with observed value "ST_ENG_FED_Startup_1"; [odf/other/rom.odf:31](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:31>) — Authored comments and/or surrounding definitions with observed value "ST_ENG_ROM_Startup_1".

<a id="enginestarttwosound"></a>

### `enginestarttwosound`

**Spellings:** `engineStartTwoSound`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with a second engine-start audio stage; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"ST_ENG_BOR_Startup_2"
"ST_ENG_FED_Startup_2"
"ST_ENG_KLI_Startup_2"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes enginestarttwosound.

Evidence: [odf/other/fed.odf:27](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:27>) — Authored comments and/or surrounding definitions with observed value "ST_ENG_FED_Startup_2"; [odf/other/rom.odf:32](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:32>) — Authored comments and/or surrounding definitions with observed value "ST_ENG_ROM_Startup_2".

<a id="evasionindex"></a>

### `evasion<N>`

**Spellings:** `Evasion0`, `Evasion1`, `Evasion2`, `Evasion3`, `Evasion4`.

**Documented / high confidence**; role: gameplay.

Defines a captain evasion-skill level row, changing chance to be hit. The triple follows the bonus/XP/extra-information schema, with negative bonus values.

Value form: comma-separated numeric tuple. Units: hit-chance modifier, XP threshold, auxiliary value. Observed in 4 files (20 assignments).

Examples:

```text
0.0,   500.0,  0.0
-0.15,  500.0,  0.0
-0.30,  500.0,  0.0
```

Uncertainty: The level4 timed comment is not sufficient to infer the underlying probability calculation.

Next investigation: Determine how the negative bonus affects opposing targeting and timed level4 behavior.

Evidence: [odf/other/fed.odf:187](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:187>) — Authored comments and/or surrounding definitions with observed value 0.0,   500.0,  0.0; [odf/other/fed.odf:191](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:191>) — Authored comments and/or surrounding definitions with observed value -0.50,  500.0, 30.0.

<a id="explosionmajorsoundgroup"></a>

### `explosionmajorsoundgroup`

**Spellings:** `explosionMajorSoundGroup`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with the major death explosion; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 17 files (17 assignments).

Examples:

```text
"ST_GRP_Asteroid_Rock"
"ST_GRP_EXP_Ship_Large"
"ST_GRP_EXP_Station"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes explosionmajorsoundgroup.

Evidence: [odf/ships/craft.odf:215](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:215>) — Authored comments and/or surrounding definitions with observed value "ST_GRP_EXP_Ship_Large"; [odf/stations/station.odf:149](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:149>) — Authored comments and/or surrounding definitions with observed value "ST_GRP_EXP_Station".

<a id="explosionminorsoundgroup"></a>

### `explosionminorsoundgroup`

**Spellings:** `explosionMinorSoundGroup`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with minor pre-destruction explosions; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 17 files (17 assignments).

Examples:

```text
"ST_GRP_Asteroid_Rock"
"ST_GRP_EXP_Ship_Small"
"ST_GRP_EXP_Station"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes explosionminorsoundgroup.

Evidence: [odf/ships/craft.odf:216](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:216>) — Authored comments and/or surrounding definitions with observed value "ST_GRP_EXP_Ship_Small"; [odf/stations/station.odf:150](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:150>) — Authored comments and/or surrounding definitions with observed value "ST_GRP_EXP_Station".

<a id="explosionparticlesystemsmajor"></a>

### `explosionparticlesystemsmajor`

**Spellings:** `explosionParticleSystemsMajor`.

**Documented / high confidence**; role: asset.

Lists particle systems when a dying craft finally explodes and breaks into chunks.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 42 files (42 assignments).

Examples:

```text
"psexplosion_asteroid"
"pssparks_01""pssparks_03" "psexplosion_6"
"psexplosion_major_borg"
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Recover major-explosion event timing and placement of multiple systems.

Evidence: [odf/ships/craft.odf:143](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:143>) — Authored comments and/or surrounding definitions with observed value "psexplosion_major"; [odf/ships/class_scout.odf:128](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_scout.odf:128>) — Authored comments and/or surrounding definitions with observed value "psexplosion_minor".

<a id="explosionparticlesystemsminor"></a>

### `explosionparticlesystemsminor`

**Spellings:** `explosionParticleSystemsMinor`.

**Documented / high confidence**; role: asset.

Lists small explosions played in succession while the dying ship drifts, before final breakup.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 41 files (41 assignments).

Examples:

```text
"psexplosion_asteroid"
"pssparks_01""pssparks_03" "psexplosion_12"
"psexplosion_blast_wide3_borg"
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Recover sequencing, repeated emission and cancellation/removal behavior.

Evidence: [odf/ships/craft.odf:145](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:145>) — Authored comments and/or surrounding definitions with observed value "psexplosion_blast_wide1"; [odf/ships/class_scout.odf:130](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_scout.odf:130>) — Authored comments and/or surrounding definitions with observed value "explosionb".

<a id="extinction"></a>

### `extinction`

**Spellings:** `Extinction`.

**Documented / high confidence**; role: presentation.

Controls absorption of light by the nebula cloud matter.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
8.0
```

Uncertainty: No scale, distance convention or absorption equation is documented.

Next investigation: Recover the absorption equation and its dependence on cloud depth.

Evidence: [odf/other/mnebula6.odf:108](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:108>) — Authored comments and/or surrounding definitions with observed value 8.0.

<a id="flarefadespeed"></a>

### `flarefadespeed`

**Spellings:** `flareFadeSpeed`.

**Documented / high confidence**; role: presentation.

Controls how quickly a flare fades when its source becomes obstructed.

Value form: numeric scalar. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
2.0
```

Uncertainty: This is a speed parameter, not proven to be a duration in seconds.

Next investigation: Recover fade integration and whether reveal uses the same speed.

Evidence: [odf/other/lens_flare.odf:9](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:9>) — Authored comments and/or surrounding definitions with observed value 2.0.

<a id="flareminvisibility"></a>

### `flareminvisibility`

**Spellings:** `flareMinVisibility`.

**Documented / high confidence**; role: presentation.

Requires a minimum visible fraction of the source before displaying its lens flare.

Value form: numeric scalar. Units: visible fraction; source calls it percentage and uses 0.4. Observed in 1 files (1 assignments).

Examples:

```text
0.4
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Determine how source visibility is measured and whether the threshold has hysteresis.

Evidence: [odf/other/lens_flare.odf:12](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:12>) — Authored comments and/or surrounding definitions with observed value 0.4.

<a id="flashbrightness"></a>

### `flashbrightness`

**Spellings:** `flashBrightness`.

**Inferred / medium confidence**; role: presentation.

Likely scales reveal-flash intensity.

Value form: numeric scalar. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
1.0
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Recover intensity scaling and saturation behavior.

Evidence: [odf/other/lens_flare.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:5>) — Authored comments and/or surrounding definitions with observed value 1.0.

<a id="flashsize"></a>

### `flashsize`

**Spellings:** `flashSize`.

**Inferred / medium confidence**; role: presentation.

Likely scales the reveal flash sprite.

Value form: numeric scalar. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
3.0
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Determine whether scale is relative to the source or screen dimensions.

Evidence: [odf/other/lens_flare.odf:4](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:4>) — Authored comments and/or surrounding definitions with observed value 3.0.

<a id="flashspeed"></a>

### `flashspeed`

**Spellings:** `flashSpeed`.

**Unresolved / low confidence**; role: presentation.

A timing or animation-speed parameter of the reveal flash; the file does not define the waveform.

Value form: numeric scalar. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
.75
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Determine whether it controls growth, fade or animation playback.

Evidence: [odf/other/lens_flare.odf:3](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:3>) — Authored comments and/or surrounding definitions with observed value .75.

<a id="flashsprite"></a>

### `flashsprite`

**Spellings:** `flashSprite`.

**Documented / high confidence**; role: asset.

Names the sprite for the bright flash shown when the flare is revealed.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"x_light_star"
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Identify reveal/occlusion events and sprite lookup rules.

Evidence: [odf/other/lens_flare.odf:2](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:2>) — Authored comments and/or surrounding definitions with observed value "x_light_star".

<a id="flashtriggerspeed"></a>

### `flashtriggerspeed`

**Spellings:** `flashTriggerSpeed`.

**Unresolved / low confidence**; role: presentation.

An unexplained trigger-speed scalar associated with the lens-flare flash.

Value form: numeric scalar. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
0.3
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Determine whether it sets a visibility-change threshold, repeat delay or pulse speed.

Evidence: [odf/other/lens_flare.odf:6](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:6>) — Authored comments and/or surrounding definitions with observed value 0.3.

<a id="geometrybias"></a>

### `geometrybias`

**Spellings:** `GeometryBias`.

**Documented / high confidence**; role: presentation.

Weights the central nebula blob relative to its tentacles, according to the adjacent comment.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
0
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Determine the bias formula and whether negative values are valid.

Evidence: [odf/other/mnebula6.odf:91](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:91>) — Authored comments and/or surrounding definitions with observed value 0.

<a id="has-orders"></a>

### `has_orders`

**Spellings:** `has_orders`.

**Inferred / medium confidence**; role: ui.

Likely marks objects as accepting or exposing orders in the contextual menu system.

Value form: numeric scalar. Units: observed 0/1 flag. Observed in 2 files (2 assignments).

Examples:

```text
0
1
```

Uncertainty: The UI flag does not itself prove a gameplay operation exists or can execute.

Next investigation: Determine the menu filter and whether the order dispatcher also checks has_orders.

Evidence: [odf/ships/craft.odf:132](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:132>) — Authored comments and/or surrounding definitions with observed value 1; [odf/other/transwarp_portal.odf:34](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/transwarp_portal.odf:34>) — Authored comments and/or surrounding definitions with observed value 0.

<a id="hasengines"></a>

### `hasengines`

**Spellings:** `hasEngines`.

**Inferred / medium confidence**; role: gameplay.

Declares whether the craft has a engine system. Base craft definitions enable it and organize related properties beneath it.

Value form: numeric scalar with observed trailing semicolon. Units: observed 0/1 flag. Observed in 146 files (146 assignments).

Examples:

```text
1;
0
1
```

Uncertainty: Whether zero removes a system, disables interactions, or only affects UI is not established.

Next investigation: Trace which system-creation and targeting paths test hasengines.

Evidence: [odf/ships/craft.odf:66](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:66>) — Authored comments and/or surrounding definitions with observed value 1; [odf/stations/station.odf:48](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:48>) — Authored comments and/or surrounding definitions with observed value 0.

<a id="hashull"></a>

### `hashull`

**Spellings:** `hasHull`.

**Documented / high confidence**; role: gameplay.

Declares whether the craft has a hull system. Base craft definitions enable it and organize related properties beneath it.

Value form: numeric scalar with observed trailing semicolon. Units: observed 0/1 flag. Observed in 166 files (166 assignments).

Examples:

```text
1;
0
1
```

Uncertainty: Whether zero removes a system, disables interactions, or only affects UI is not established.

Next investigation: Trace which system-creation and targeting paths test hashull.

Evidence: [odf/ships/craft.odf:44](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:44>) — Direct Hull System / we have a hull system comments..

<a id="hassensors"></a>

### `hassensors`

**Spellings:** `hasSensors`.

**Inferred / medium confidence**; role: gameplay.

Declares whether the craft has a sensor system. Base craft definitions enable it and organize related properties beneath it.

Value form: numeric scalar with observed trailing semicolon. Units: observed 0/1 flag. Observed in 166 files (166 assignments).

Examples:

```text
0
1;
1
```

Uncertainty: Whether zero removes a system, disables interactions, or only affects UI is not established.

Next investigation: Trace which system-creation and targeting paths test hassensors.

Evidence: [odf/ships/craft.odf:76](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:76>) — Authored comments and/or surrounding definitions with observed value 1; [odf/ships/K_D_3.odf:127](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_3.odf:127>) — Authored comments and/or surrounding definitions with observed value 1;.

<a id="hasshield"></a>

### `hasshield`

**Spellings:** `hasShield`.

**Inferred / medium confidence**; role: gameplay.

A singular-spelling shield-presence flag in many ship definitions. It probably indicates a shield system, but the base also uses hasShields.

Value form: numeric scalar with observed trailing semicolon. Units: observed 0/1 flag. Observed in 161 files (161 assignments).

Examples:

```text
0;
1;
```

Uncertainty: Singular/plural aliasing or typo behavior is unknown; both spellings are preserved.

Next investigation: Check which spellings the Legacy parser consumes and whether one overrides the other.

Evidence: [odf/ships/K_D_3.odf:124](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_3.odf:124>) — Authored comments and/or surrounding definitions with observed value 1;; [odf/ships/B_ent_cube.odf:135](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_ent_cube.odf:135>) — Authored comments and/or surrounding definitions with observed value 0;.

<a id="hasshields"></a>

### `hasshields`

**Spellings:** `hasShields`.

**Documented / high confidence**; role: gameplay.

Declares whether the craft has a shield system. Base craft definitions enable it and organize related properties beneath it.

Value form: numeric scalar. Units: observed 0/1 flag. Observed in 47 files (47 assignments).

Examples:

```text
0
1
```

Uncertainty: Whether zero removes a system, disables interactions, or only affects UI is not established.

Next investigation: Trace which system-creation and targeting paths test hasshields.

Evidence: [odf/ships/craft.odf:52](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:52>) — Authored comments and/or surrounding definitions with observed value 1; [odf/ships/F_NX_01.odf:27](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_NX_01.odf:27>) — Authored comments and/or surrounding definitions with observed value 0.

<a id="hasweapons"></a>

### `hasweapons`

**Spellings:** `hasWeapons`.

**Inferred / medium confidence**; role: gameplay.

Declares whether the craft has a weapon system. Base craft definitions enable it and organize related properties beneath it.

Value form: numeric scalar with observed trailing semicolon. Units: observed 0/1 flag. Observed in 212 files (214 assignments).

Examples:

```text
0
1;
1
```

Uncertainty: Whether zero removes a system, disables interactions, or only affects UI is not established.

Next investigation: Trace which system-creation and targeting paths test hasweapons.

Evidence: [odf/ships/craft.odf:79](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:79>) — Authored comments and/or surrounding definitions with observed value 1; [odf/stations/station.odf:59](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:59>) — Authored comments and/or surrounding definitions with observed value 0.

<a id="heightindex"></a>

### `height<N>`

**Spellings:** `height4`, `height5`.

**Inferred / medium confidence**; role: presentation.

Sets a separate vertical size for indexed flare layer N, paired with widthN.

Value form: numeric scalar. Units: unknown. Observed in 1 files (2 assignments).

Examples:

```text
0.2
0.8
```

Uncertainty: Layer indexing is observed; coordinate frame, scale and exact resize rule are not documented.

Next investigation: Determine height units and how it overrides sizeN.

Evidence: [odf/other/lens_flare.odf:43](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:43>) — Authored comments and/or surrounding definitions with observed value 0.2; [odf/other/lens_flare.odf:51](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:51>) — Authored comments and/or surrounding definitions with observed value 0.8.

<a id="hulllevel"></a>

### `hulllevel`

**Spellings:** `hullLevel`.

**Documented / high confidence**; role: gameplay.

Sets the general upgradeable hull statistic displayed to the player. The ODF labels these values as system levels, separate from hit-point endpoints.

Value form: numeric scalar. Units: system-level rating, exact scale unknown. Observed in 172 files (172 assignments).

Examples:

```text
20
10000
4000
```

Uncertainty: The conversion from level to current hit points or performance is not recovered.

Next investigation: Recover how hulllevel feeds damage, performance and upgrade calculations.

Evidence: [odf/ships/B_tng_rectanglebomb.odf:27](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_tng_rectanglebomb.odf:27>) — Authored comments and/or surrounding definitions with observed value 800; [odf/stations/command.odf:7](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/command.odf:7>) — Authored comments and/or surrounding definitions with observed value 1200.

<a id="hullmaxhitpoints"></a>

### `hullmaxhitpoints`

**Spellings:** `hullMaxHitPoints`.

**Documented / high confidence**; role: gameplay.

Sets the high hit-point endpoint for the hull system. Station hull comments label the pair as level-1 and level-5 values.

Value form: numeric scalar with observed trailing semicolon. Units: system hit points. Observed in 4 files (5 assignments).

Examples:

```text
250;
1200
2500
```

Uncertainty: Repeated hull assignments occur in one station; runtime duplicate precedence and interpolation are not established.

Next investigation: Recover how the hull level selects or interpolates hit-point capacity.

Evidence: [odf/other/flyingasteroid_LARGE.odf:70](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/flyingasteroid_LARGE.odf:70>) — Authored comments and/or surrounding definitions with observed value 250;; [odf/stations/ent03_fed_station.odf:77](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/ent03_fed_station.odf:77>) — Authored comments and/or surrounding definitions with observed value 1200.

<a id="hullminhitpoints"></a>

### `hullminhitpoints`

**Spellings:** `hullMinHitPoints`.

**Documented / high confidence**; role: gameplay.

Sets the low hit-point endpoint for the hull system. Station hull comments label the pair as level-1 and level-5 values.

Value form: numeric scalar with observed trailing semicolon. Units: system hit points. Observed in 4 files (5 assignments).

Examples:

```text
250;
1000
2500
```

Uncertainty: Repeated hull assignments occur in one station; runtime duplicate precedence and interpolation are not established.

Next investigation: Recover how the hull level selects or interpolates hit-point capacity.

Evidence: [odf/stations/ent03_fed_station.odf:76](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/ent03_fed_station.odf:76>) — Authored comments and/or surrounding definitions with observed value 1000; [odf/stations/ent03_fed_station.odf:88](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/ent03_fed_station.odf:88>) — Authored comments and/or surrounding definitions with observed value 2500.

<a id="isasteroid"></a>

### `isasteroid`

**Spellings:** `isAsteroid`.

**Inferred / low confidence**; role: schema.

Marks an asteroid definition as asteroid-like, probably for categorization or special handling.

Value form: numeric scalar. Units: observed 0/1 flag. Observed in 14 files (21 assignments).

Examples:

```text
1
```

Uncertainty: The sole surviving definition does not identify any behavior controlled by the flag.

Next investigation: Find the classification consumer and whether the field is legacy/dead data.

Evidence: [odf/other/aster.odf:12](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/aster.odf:12>) — Authored comments and/or surrounding definitions with observed value 1.

<a id="klaxonsound"></a>

### `klaxonsound`

**Spellings:** `klaxonSound`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with the faction warning/klaxon; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
"ST_AMB_FED_Klaxon"
"ST_AMB_KLI_Klaxon"
"ST_AMB_ROM_Klaxon"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes klaxonsound.

Evidence: [odf/other/fed.odf:25](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:25>) — Authored comments and/or surrounding definitions with observed value "ST_AMB_FED_Klaxon"; [odf/other/rom.odf:30](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:30>) — Authored comments and/or surrounding definitions with observed value "ST_AMB_ROM_Klaxon".

<a id="levelouttime"></a>

### `levelouttime`

**Spellings:** `levelOutTime`.

**Documented / medium confidence**; role: presentation.

Original intent: begin leveling the craft this many seconds before reaching the end of its path. The base annotates it as read but unused, while class files still override it.

Value form: numeric scalar with observed trailing semicolon. Units: seconds per authored comment. Observed in 6 files (6 assignments).

Examples:

```text
9;
.001;
3;
```

Uncertainty: A declared value is not evidence of a functioning leveling timer.

Next investigation: Determine whether any path controller actually uses levelOutTime.

Evidence: [odf/other/class_scout_physics.odf:97](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:97>) — Authored comments and/or surrounding definitions with observed value 9;; [odf/other/class_destroyer_physics.odf:98](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_destroyer_physics.odf:98>) — Authored comments and/or surrounding definitions with observed value .001;.

<a id="massscale"></a>

### `massscale`

**Spellings:** `MassScale`, `massScale`.

**Documented / high confidence**; role: gameplay.

Multiplies ship mass calculated from its size under an equal-density assumption; 1.0 is normal.

Value form: numeric scalar with observed trailing semicolon. Units: dimensionless multiplier. Observed in 14 files (15 assignments).

Examples:

```text
1000
1.0;
10
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Determine whether mass affects collision impulses, movement, break effects or multiple paths.

Evidence: [odf/ships/class_support.odf:124](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_support.odf:124>) — Authored comments and/or surrounding definitions with observed value 5.0;; [odf/ships/class_artillery.odf:136](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_artillery.odf:136>) — Authored comments and/or surrounding definitions with observed value 1.0;.

<a id="maxbackwardaccel"></a>

### `maxbackwardaccel`

**Spellings:** `maxBackwardAccel`.

**Inferred / medium confidence**; role: gameplay.

Likely the maximum-Engine-stat endpoint for backward acceleration, using the minFoo/maxFoo Engine-stat convention.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
70.0;
120.0;
160.0;
```

Uncertainty: The forward-acceleration comment also mentions backward movement, and old backwardAccel is annotated unused; these may be separate or obsolete paths.

Next investigation: Determine whether min/maxBackwardAccel are actually used independently of forward acceleration.

Evidence: [odf/other/craft_physics.odf:26](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:26>) — Authored comments and/or surrounding definitions with observed value 80.0;; [odf/other/class_scout_physics.odf:16](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:16>) — Authored comments and/or surrounding definitions with observed value 160.0;.

<a id="maxbankangle"></a>

### `maxbankangle`

**Spellings:** `maxBankAngle`.

**Documented / high confidence**; role: presentation.

Maximum bank angle for a destination-mode turn. The source recommends matching the direct-control full-speed maximum to avoid inconsistent presentation.

Value form: numeric scalar with observed trailing semicolon. Units: angle; degrees likely but not explicitly declared here. Observed in 9 files (9 assignments).

Examples:

```text
35;
32;
50;
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Recover the clamp and whether zero disables all banking controllers.

Evidence: [odf/other/class_destroyer_physics.odf:69](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_destroyer_physics.odf:69>) — Authored comments and/or surrounding definitions with observed value 50;; [odf/other/craft_physics.odf:88](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:88>) — Authored comments and/or surrounding definitions with observed value 45;.

<a id="maxbankomega"></a>

### `maxbankomega`

**Spellings:** `maxBankOmega`.

**Documented / high confidence**; role: gameplay.

Sets the maximum-Engine-stat endpoint for banking-rate parameter. craft_physics.odf explains that minFoo/maxFoo depend on the ship Engine stat, rather than being throttle limits.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 9 files (9 assignments).

Examples:

```text
1.5;
4;
5.5;
```

Uncertainty: Exact interpolation, conversion units and damaged-engine interaction are unverified.

Next investigation: Recover Engine-stat interpolation and the banking-rate parameter consumer.

Evidence: [odf/other/craft_physics.odf:47](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:47>) — Authored comments and/or surrounding definitions with observed value 1.5;; [odf/other/craft_borg_physics.odf:71](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_borg_physics.odf:71>) — Authored comments and/or surrounding definitions with observed value .02;.

<a id="maxdcbankangleatfullspeed"></a>

### `maxdcbankangleatfullspeed`

**Spellings:** `maxDCBankAngleAtFullSpeed`.

**Documented / high confidence**; role: presentation.

Sets the full-speed endpoint of maximum bank angle in direct-control mode. The source describes a linear interpolation between the two endpoints with current speed.

Value form: numeric scalar. Units: angle; likely degrees, not explicitly specified. Observed in 7 files (7 assignments).

Examples:

```text
35
32
50
```

Uncertainty: Destination-mode maxBankAngle is a separate setting.

Next investigation: Recover speed normalization, endpoint clamps and mode switching.

Evidence: [odf/other/craft_physics.odf:94](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:94>) — Authored comments and/or surrounding definitions with observed value 45; [odf/other/class_scout_physics.odf:74](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:74>) — Authored comments and/or surrounding definitions with observed value 25.

<a id="maxdcbankangleatzerospeed"></a>

### `maxdcbankangleatzerospeed`

**Spellings:** `maxDCBankAngleAtZeroSpeed`.

**Documented / high confidence**; role: presentation.

Sets the zero-speed endpoint of maximum bank angle in direct-control mode. The source describes a linear interpolation between the two endpoints with current speed.

Value form: numeric scalar. Units: angle; likely degrees, not explicitly specified. Observed in 7 files (7 assignments).

Examples:

```text
6
2
12
```

Uncertainty: Destination-mode maxBankAngle is a separate setting.

Next investigation: Recover speed normalization, endpoint clamps and mode switching.

Evidence: [odf/other/craft_physics.odf:93](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:93>) — Authored comments and/or surrounding definitions with observed value 10; [odf/other/class_scout_physics.odf:73](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:73>) — Authored comments and/or surrounding definitions with observed value 2.

<a id="maxforwardaccel"></a>

### `maxforwardaccel`

**Spellings:** `maxForwardAccel`.

**Documented / high confidence**; role: gameplay.

Sets the maximum-Engine-stat endpoint for forward/backward translational acceleration. craft_physics.odf explains that minFoo/maxFoo depend on the ship Engine stat, rather than being throttle limits.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 8 files (8 assignments).

Examples:

```text
50.0;
45.0;
60.0;
```

Uncertainty: Exact interpolation, conversion units and damaged-engine interaction are unverified. The forward field comment says it also applies backward; separate backward fields still require checking.

Next investigation: Recover Engine-stat interpolation and the forward/backward translational acceleration consumer.

Evidence: [odf/other/craft_physics.odf:23](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:23>) — Authored comments and/or surrounding definitions with observed value 40.0;; [odf/other/class_scout_physics.odf:13](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:13>) — Authored comments and/or surrounding definitions with observed value 80.0;.

<a id="maxpitchomega"></a>

### `maxpitchomega`

**Spellings:** `maxPitchOmega`.

**Documented / high confidence**; role: gameplay.

Sets the maximum-Engine-stat endpoint for maximum pitch rate. craft_physics.odf explains that minFoo/maxFoo depend on the ship Engine stat, rather than being throttle limits.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 11 files (11 assignments).

Examples:

```text
1.3;
0.5;
.02;
```

Uncertainty: Exact interpolation, conversion units and damaged-engine interaction are unverified.

Next investigation: Recover Engine-stat interpolation and the maximum pitch rate consumer.

Evidence: [odf/other/craft_physics.odf:38](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:38>) — Authored comments and/or surrounding definitions with observed value 1.3;; [odf/other/k_brel_physics.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/k_brel_physics.odf:5>) — Authored comments and/or surrounding definitions with observed value 1.1;.

<a id="maxshieldhitpoints"></a>

### `maxshieldhitpoints`

**Spellings:** `maxShieldHitPoints`.

**Inferred / medium confidence**; role: gameplay.

Likely directly specifies a shield hit-point capacity/maximum; it appears in station tweaks and ship SHIELD VALUES blocks.

Value form: numeric scalar. Units: shield hit points. Observed in 15 files (15 assignments).

Examples:

```text
0
400
50
```

Uncertainty: Relationship to shieldMin/MaxHitPoints, shieldLevel and facing pools is unknown.

Next investigation: Identify which shield-capacity field wins when endpoint and direct maximum fields coexist.

Evidence: [odf/stations/K_Tos_Shipyard.odf:16](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/K_Tos_Shipyard.odf:16>) — Authored comments and/or surrounding definitions with observed value 100; [odf/ships/v_surak.odf:105](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/v_surak.odf:105>) — Authored comments and/or surrounding definitions with observed value 250.

<a id="maxshipspeed"></a>

### `maxshipspeed`

**Spellings:** `maxShipSpeed`.

**Documented / high confidence**; role: gameplay.

Sets the maximum-Engine-stat endpoint for full-throttle impulse speed. craft_physics.odf explains that minFoo/maxFoo depend on the ship Engine stat, rather than being throttle limits.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 15 files (16 assignments).

Examples:

```text
450.0;
400.0;
300.0
```

Uncertainty: A low-stat top-speed setting is not a minimum allowed travel speed.

Next investigation: Recover Engine-stat interpolation and the full-throttle impulse speed consumer.

Evidence: [odf/other/craft_physics.odf:55](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:55>) — Authored comments and/or surrounding definitions with observed value 250.0;; [odf/other/class_scout_physics.odf:42](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:42>) — Authored comments and/or surrounding definitions with observed value 450.0;.

<a id="maxspeedfractionforbank"></a>

### `maxspeedfractionforbank`

**Spellings:** `maxSpeedFractionForBank`.

**Documented / medium confidence**; role: presentation.

An old speed-fraction threshold for beginning banking. The comments describe the original intent but group the base settings as obsolete or obscure.

Value form: numeric scalar with observed trailing semicolon. Units: fraction of maximum impulse speed. Observed in 9 files (9 assignments).

Examples:

```text
.2
.2;
1.0;
```

Uncertainty: No separate meaning for the min/max pair is specified, and current use is doubtful.

Next investigation: Determine whether these thresholds gate banking or have been replaced by newer angle/rate fields.

Evidence: [odf/other/craft_physics.odf:110](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:110>) — Authored comments and/or surrounding definitions with observed value 0.1;; [odf/other/class_scout_physics.odf:80](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:80>) — Authored comments and/or surrounding definitions with observed value .2;.

<a id="maxturnomega"></a>

### `maxturnomega`

**Spellings:** `maxTurnOmega`.

**Documented / high confidence**; role: gameplay.

Sets the maximum-Engine-stat endpoint for maximum yaw turning rate. craft_physics.odf explains that minFoo/maxFoo depend on the ship Engine stat, rather than being throttle limits.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 9 files (9 assignments).

Examples:

```text
1.3;
0.5;
.02;
```

Uncertainty: Exact interpolation, conversion units and damaged-engine interaction are unverified.

Next investigation: Recover Engine-stat interpolation and the maximum yaw turning rate consumer.

Evidence: [odf/other/craft_physics.odf:31](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:31>) — Authored comments and/or surrounding definitions with observed value 1.3;; [odf/other/craft_borg_physics.odf:57](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_borg_physics.odf:57>) — Authored comments and/or surrounding definitions with observed value .02;.

<a id="menuobjectindex"></a>

### `menuobject<N>`

**Spellings:** `menuObject1`, `menuObject10`, `menuObject11`, `menuObject2`, `menuObject3`, `menuObject4`, `menuObject5`, `menuObject6`, `menuObject7`, `menuObject8`, `menuObject9`, `menuobject3`, `menuobject7`, `menuobject8`.

**Inferred / medium confidence**; role: ui.

Lists indexed ODF targets in the object-editor/menu hierarchy: e_fed points to era, Enterprise and station submenus. The entries may refer to submenus or placeable objects depending on the target ODF.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 67 files (366 assignments).

Examples:

```text
"bump"
"B_recycler"
"eb_ent_comba.odf"
```

Uncertainty: Index start, gaps, target classification and missing-file behavior are unverified.

Next investigation: Trace the menu loader and distinguish submenu navigation from object placement.

Evidence: [odf/other/e_fed.odf:2](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/e_fed.odf:2>) — Authored comments and/or surrounding definitions with observed value "ef_ent_comba.odf"; [odf/other/e_fed.odf:3](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/e_fed.odf:3>) — Authored comments and/or surrounding definitions with observed value "ef_tos_comba.odf".

<a id="minbackwardaccel"></a>

### `minbackwardaccel`

**Spellings:** `minBackwardAccel`.

**Inferred / medium confidence**; role: gameplay.

Likely the minimum-Engine-stat endpoint for backward acceleration, using the minFoo/maxFoo Engine-stat convention.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
20.0;
30.0;
40.0;
```

Uncertainty: The forward-acceleration comment also mentions backward movement, and old backwardAccel is annotated unused; these may be separate or obsolete paths.

Next investigation: Determine whether min/maxBackwardAccel are actually used independently of forward acceleration.

Evidence: [odf/other/craft_physics.odf:25](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:25>) — Authored comments and/or surrounding definitions with observed value 40.0;; [odf/other/class_cruiser_physics.odf:15](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_cruiser_physics.odf:15>) — Authored comments and/or surrounding definitions with observed value 30.0;.

<a id="minbankomega"></a>

### `minbankomega`

**Spellings:** `minBankOmega`.

**Documented / high confidence**; role: gameplay.

Sets the minimum-Engine-stat endpoint for banking-rate parameter. craft_physics.odf explains that minFoo/maxFoo depend on the ship Engine stat, rather than being throttle limits.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 9 files (9 assignments).

Examples:

```text
.6;
1.7;
0.5;
```

Uncertainty: Exact interpolation, conversion units and damaged-engine interaction are unverified.

Next investigation: Recover Engine-stat interpolation and the banking-rate parameter consumer.

Evidence: [odf/other/craft_physics.odf:46](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:46>) — Authored comments and/or surrounding definitions with observed value .6;; [odf/other/craft_borg_physics.odf:70](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_borg_physics.odf:70>) — Authored comments and/or surrounding definitions with observed value .01;.

<a id="minforwardaccel"></a>

### `minforwardaccel`

**Spellings:** `minForwardAccel`.

**Documented / high confidence**; role: gameplay.

Sets the minimum-Engine-stat endpoint for forward/backward translational acceleration. craft_physics.odf explains that minFoo/maxFoo depend on the ship Engine stat, rather than being throttle limits.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 8 files (8 assignments).

Examples:

```text
10.0;
12.0;
15.0;
```

Uncertainty: Exact interpolation, conversion units and damaged-engine interaction are unverified. The forward field comment says it also applies backward; separate backward fields still require checking.

Next investigation: Recover Engine-stat interpolation and the forward/backward translational acceleration consumer.

Evidence: [odf/other/craft_physics.odf:22](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:22>) — Authored comments and/or surrounding definitions with observed value 20.0;; [odf/other/class_scout_physics.odf:12](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:12>) — Authored comments and/or surrounding definitions with observed value 20.0;.

<a id="minpitchomega"></a>

### `minpitchomega`

**Spellings:** `minPitchOmega`.

**Documented / high confidence**; role: gameplay.

Sets the minimum-Engine-stat endpoint for maximum pitch rate. craft_physics.odf explains that minFoo/maxFoo depend on the ship Engine stat, rather than being throttle limits.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 11 files (11 assignments).

Examples:

```text
.3;
0.5;
.01;
```

Uncertainty: Exact interpolation, conversion units and damaged-engine interaction are unverified.

Next investigation: Recover Engine-stat interpolation and the maximum pitch rate consumer.

Evidence: [odf/other/craft_physics.odf:37](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:37>) — Authored comments and/or surrounding definitions with observed value .3;; [odf/other/k_brel_physics.odf:4](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/k_brel_physics.odf:4>) — Authored comments and/or surrounding definitions with observed value .1;.

<a id="minshipspeed"></a>

### `minshipspeed`

**Spellings:** `minShipSpeed`.

**Documented / high confidence**; role: gameplay.

Sets the minimum-Engine-stat endpoint for full-throttle impulse speed. craft_physics.odf explains that minFoo/maxFoo depend on the ship Engine stat, rather than being throttle limits.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 15 files (15 assignments).

Examples:

```text
100.0;
120.0;
130.0;
```

Uncertainty: A low-stat top-speed setting is not a minimum allowed travel speed.

Next investigation: Recover Engine-stat interpolation and the full-throttle impulse speed consumer.

Evidence: [odf/other/class_support_physics.odf:16](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_support_physics.odf:16>) — Authored comments and/or surrounding definitions with observed value 100.0;; [odf/other/r_transport_physics.odf:6](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/r_transport_physics.odf:6>) — Authored comments and/or surrounding definitions with observed value 75.0;.

<a id="minspeedfractionforbank"></a>

### `minspeedfractionforbank`

**Spellings:** `minSpeedFractionForBank`.

**Documented / medium confidence**; role: presentation.

An old speed-fraction threshold for beginning banking. The comments describe the original intent but group the base settings as obsolete or obscure.

Value form: numeric scalar with observed trailing semicolon. Units: fraction of maximum impulse speed. Observed in 9 files (9 assignments).

Examples:

```text
.05;
0.9;
0.01;
```

Uncertainty: No separate meaning for the min/max pair is specified, and current use is doubtful.

Next investigation: Determine whether these thresholds gate banking or have been replaced by newer angle/rate fields.

Evidence: [odf/other/class_artillery_physics.odf:53](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_artillery_physics.odf:53>) — Authored comments and/or surrounding definitions with observed value .05;; [odf/other/craft_physics.odf:109](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:109>) — Authored comments and/or surrounding definitions with observed value 0.01;.

<a id="minturnomega"></a>

### `minturnomega`

**Spellings:** `minTurnOmega`.

**Documented / high confidence**; role: gameplay.

Sets the minimum-Engine-stat endpoint for maximum yaw turning rate. craft_physics.odf explains that minFoo/maxFoo depend on the ship Engine stat, rather than being throttle limits.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 9 files (9 assignments).

Examples:

```text
.3;
0.5;
.01;
```

Uncertainty: Exact interpolation, conversion units and damaged-engine interaction are unverified.

Next investigation: Recover Engine-stat interpolation and the maximum yaw turning rate consumer.

Evidence: [odf/other/craft_borg_physics.odf:56](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_borg_physics.odf:56>) — Authored comments and/or surrounding definitions with observed value .01;; [odf/other/class_scout_physics.odf:19](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:19>) — Authored comments and/or surrounding definitions with observed value .3;.

<a id="music"></a>

### `music`

**Spellings:** `music`.

**Documented / high confidence**; role: asset.

Names a race music-playlist ODF; the faction file explicitly labels it Music play list.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
"borg_music.odf"
"fed_music.odf"
"kli_music.odf"
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Recover playlist inheritance and how race selection chooses it.

Evidence: [odf/other/fed.odf:19](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:19>) — Authored comments and/or surrounding definitions with observed value "fed_music.odf"; [odf/other/rom.odf:24](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:24>) — Authored comments and/or surrounding definitions with observed value "rom_music.odf".

<a id="musicdefeat"></a>

### `musicdefeat`

**Spellings:** `musicDefeat`.

**Inferred / medium confidence**; role: asset.

Lists music resources for the defeat; names of the referenced ST_MUSIC assets corroborate the intended event.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"ST_MUSIC_BOR_DEFEAT_1"
"ST_MUSIC_FED_DEFEAT_1"
"ST_MUSIC_KLI_DEFEAT_1"
```

Uncertainty: Selection order, randomization, transition conditions and looping are not specified.

Next investigation: Determine which event selects musicdefeat and how multiple tracks are scheduled.

Evidence: [odf/other/fed_music.odf:4](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed_music.odf:4>) — Authored comments and/or surrounding definitions with observed value "ST_MUSIC_FED_DEFEAT_1"; [odf/other/kli_music.odf:4](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/kli_music.odf:4>) — Authored comments and/or surrounding definitions with observed value "ST_MUSIC_KLI_DEFEAT_1".

<a id="musichigh"></a>

### `musichigh`

**Spellings:** `musicHigh`.

**Inferred / medium confidence**; role: asset.

Lists music resources for the high-intensity/action state; names of the referenced ST_MUSIC assets corroborate the intended event.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"ST_MUSIC_BOR_HIGH_1" "ST_MUSIC_BOR_HIGH_2" "ST_MUSIC_BOR_HIGH_3"
"ST_MUSIC_FED_HIGH_1" "ST_MUSIC_FED_HIGH_2"
"ST_MUSIC_KLI_HIGH_1" "ST_MUSIC_KLI_HIGH_2" "ST_MUSIC_KLI_HIGH_3"
```

Uncertainty: Selection order, randomization, transition conditions and looping are not specified.

Next investigation: Determine which event selects musichigh and how multiple tracks are scheduled.

Evidence: [odf/other/fed_music.odf:2](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed_music.odf:2>) — Authored comments and/or surrounding definitions with observed value "ST_MUSIC_FED_HIGH_1" "ST_MUSIC_FED_HIGH_2"; [odf/other/kli_music.odf:2](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/kli_music.odf:2>) — Authored comments and/or surrounding definitions with observed value "ST_MUSIC_KLI_HIGH_1" "ST_MUSIC_KLI_HIGH_2" "ST_MUSIC_KLI_HIGH_3".

<a id="musiclow"></a>

### `musiclow`

**Spellings:** `musicLow`.

**Inferred / medium confidence**; role: asset.

Lists music resources for the low-intensity/quiet state; names of the referenced ST_MUSIC assets corroborate the intended event.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"ST_MUSIC_BOR_LOW_1" "ST_MUSIC_BOR_LOW_2" "ST_MUSIC_BOR_LOW_3"
"ST_MUSIC_FED_LOW_1" "ST_MUSIC_FED_LOW_2" "ST_MUSIC_FED_LOW_3" "ST_MUSIC_FED_LOW_4"
"ST_MUSIC_KLI_LOW_1" "ST_MUSIC_KLI_LOW_2"
```

Uncertainty: Selection order, randomization, transition conditions and looping are not specified.

Next investigation: Determine which event selects musiclow and how multiple tracks are scheduled.

Evidence: [odf/other/fed_music.odf:1](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed_music.odf:1>) — Authored comments and/or surrounding definitions with observed value "ST_MUSIC_FED_LOW_1" "ST_MUSIC_FED_LOW_2" "ST_MUSIC_FED_LOW_3" "ST_MUSIC_FED_LOW; [odf/other/kli_music.odf:1](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/kli_music.odf:1>) — Authored comments and/or surrounding definitions with observed value "ST_MUSIC_KLI_LOW_1" "ST_MUSIC_KLI_LOW_2".

<a id="musicshiplost"></a>

### `musicshiplost`

**Spellings:** `musicShipLost`.

**Inferred / medium confidence**; role: asset.

Lists music resources for the ship-loss event; names of the referenced ST_MUSIC assets corroborate the intended event.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"ST_MUSIC_BOR_SHIP_LOST_1"
"ST_MUSIC_FED_SHIP_LOST_1"
"ST_MUSIC_KLI_SHIP_LOST_1"
```

Uncertainty: Selection order, randomization, transition conditions and looping are not specified.

Next investigation: Determine which event selects musicshiplost and how multiple tracks are scheduled.

Evidence: [odf/other/fed_music.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed_music.odf:5>) — Authored comments and/or surrounding definitions with observed value "ST_MUSIC_FED_SHIP_LOST_1"; [odf/other/kli_music.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/kli_music.odf:5>) — Authored comments and/or surrounding definitions with observed value "ST_MUSIC_KLI_SHIP_LOST_1".

<a id="musicvictory"></a>

### `musicvictory`

**Spellings:** `musicVictory`.

**Inferred / medium confidence**; role: asset.

Lists music resources for the victory; names of the referenced ST_MUSIC assets corroborate the intended event.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"ST_MUSIC_BOR_VICTORY_1"
"ST_MUSIC_FED_VICTORY_1" "ST_MUSIC_FED_VICTORY_2" "ST_MUSIC_FED_VICTORY_3"
"ST_MUSIC_KLI_VICTORY_1"
```

Uncertainty: Selection order, randomization, transition conditions and looping are not specified.

Next investigation: Determine which event selects musicvictory and how multiple tracks are scheduled.

Evidence: [odf/other/fed_music.odf:3](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed_music.odf:3>) — Authored comments and/or surrounding definitions with observed value "ST_MUSIC_FED_VICTORY_1" "ST_MUSIC_FED_VICTORY_2" "ST_MUSIC_FED_VICTORY_3"; [odf/other/kli_music.odf:3](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/kli_music.odf:3>) — Authored comments and/or surrounding definitions with observed value "ST_MUSIC_KLI_VICTORY_1".

<a id="noisetype"></a>

### `noisetype`

**Spellings:** `NoiseType`.

**Inferred / medium confidence**; role: presentation.

Selects a nebula noise mode. The source explicitly lists 0, 1 and 2 as supported values but does not name their algorithms.

Value form: numeric scalar. Units: enum values 0, 1, 2 documented. Observed in 6 files (6 assignments).

Examples:

```text
1
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Identify the three algorithm branches and fallback for other values.

Evidence: [odf/other/mnebula6.odf:102](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:102>) — Authored comments and/or surrounding definitions with observed value 1.

<a id="numberofplayableraces"></a>

### `numberofplayableraces`

**Spellings:** `numberOfPlayableRaces`.

**Inferred / medium confidence**; role: schema.

Sets the declared number of playable races, observed as four in races.odf.

Value form: numeric scalar. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
4
```

Uncertainty: The number may bound list enumeration rather than independently enable/disable factions.

Next investigation: Recover faction-list indexing and how nonplayable races are handled.

Evidence: [odf/other/races.odf:2](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/races.odf:2>) — Authored comments and/or surrounding definitions with observed value 4.

<a id="perlinoctaves"></a>

### `perlinoctaves`

**Spellings:** `PerlinOctaves`.

**Documented / high confidence**; role: presentation.

Controls the number of noise contributions: more octaves give finer variation and fewer produce a smoother surface.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
2
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Recover permitted octave counts and persistence weighting.

Evidence: [odf/other/mnebula6.odf:100](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:100>) — Authored comments and/or surrounding definitions with observed value 2.

<a id="perlinscale"></a>

### `perlinscale`

**Spellings:** `PerlinScale`.

**Documented / high confidence**; role: presentation.

Adjusts the main geometric detail of the nebula; the source says higher values produce more tentacles.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
2
```

Uncertainty: The exact frequency/coordinate scaling is not specified.

Next investigation: Recover how noise coordinates and density thresholds use the scale.

Evidence: [odf/other/mnebula6.odf:96](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:96>) — Authored comments and/or surrounding definitions with observed value 2.

<a id="perlintiling"></a>

### `perlintiling`

**Spellings:** `PerlinTiling`.

**Inferred / medium confidence**; role: presentation.

A nebula noise/geometry setting likely controlling spatial repetition or tiling scale.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
1000
```

Uncertainty: No comment establishes whether this is a period, distance or count.

Next investigation: Trace the parameter into the noise coordinate calculation.

Evidence: [odf/other/mnebula6.odf:93](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:93>) — Authored comments and/or surrounding definitions with observed value 1000.

<a id="pitchalphamaxspeedfraction"></a>

### `pitchalphamaxspeedfraction`

**Spellings:** `pitchAlphaMaxSpeedFraction`.

**Inferred / low confidence**; role: gameplay.

An old pitch angular-acceleration tuning field, apparently intended as the speed fraction for reaching nominal angular acceleration. The base groups these settings under a comment saying they do not seem to do anything.

Value form: numeric scalar with observed trailing semicolon. Units: dimensionless fraction intended; runtime use unknown. Observed in 6 files (6 assignments).

Examples:

```text
.25;
.2;
```

Uncertainty: Its intended analogy with Omega fields does not establish an active Alpha consumer.

Next investigation: Locate a live read-and-use path for pitchalphamaxspeedfraction, not merely a parser read.

Evidence: [odf/other/craft_physics.odf:157](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:157>) — Authored comments and/or surrounding definitions with observed value .25;; [odf/other/class_scout_physics.odf:54](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:54>) — Authored comments and/or surrounding definitions with observed value .2;.

<a id="pitchdefaultscale"></a>

### `pitchdefaultscale`

**Spellings:** `pitchDefaultScale`.

**Documented / medium confidence**; role: presentation.

Intended scale for returning toward pitchDefault while resting; old comments say nonpositive values preserve the previous pitch. An inline note associates it with sMaxAccelController.

Value form: numeric scalar with observed trailing semicolon. Units: unknown. Observed in 8 files (8 assignments).

Examples:

```text
1.0;
```

Uncertainty: The base labels this area obsolete/obscure; this is intended controller behavior, not confirmed execution.

Next investigation: Verify whether the Legacy build still uses sMaxAccelController and this parameter.

Evidence: [odf/other/craft_physics.odf:123](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:123>) — Authored comments and/or surrounding definitions with observed value 1.0;; [odf/other/k_brel_physics.odf:15](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/k_brel_physics.odf:15>) — Authored comments and/or surrounding definitions with observed value 1.0;.

<a id="pitchomegamaxspeedfraction"></a>

### `pitchomegamaxspeedfraction`

**Spellings:** `pitchOmegaMaxSpeedFraction`.

**Inferred / medium confidence**; role: gameplay.

Sets the speed fraction at which pitch responsiveness reaches its nominal rate; below this, the documented turn example blends from its at-rest fraction.

Value form: numeric scalar with observed trailing semicolon. Units: fraction of maximum ship speed. Observed in 6 files (6 assignments).

Examples:

```text
0.25;
```

Uncertainty: The turn comment is internally inconsistent about endpoints and accidentally mentions bankOmega; the exact curve needs consumer evidence.

Next investigation: Recover the pitch rate-versus-throttle curve and behavior for fractions outside 0..1.

Evidence: [odf/other/craft_physics.odf:75](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:75>) — Authored comments and/or surrounding definitions with observed value 0.25;.

<a id="planetracetexture"></a>

### `planetracetexture`

**Spellings:** `planetRaceTexture`.

**Documented / high confidence**; role: asset.

Names the race-specific planet texture, likely for captured/owned-planet appearance. The source explicitly calls it a planet race-specific texture.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"borg_capture"
"fed_capture"
"klingon_capture"
```

Uncertainty: Capture/ownership switching is inferred from the asset name, not documented event logic.

Next investigation: Determine which planet material is replaced and when ownership refreshes it.

Evidence: [odf/other/fed.odf:56](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:56>) — Authored comments and/or surrounding definitions with observed value "fed_capture"; [odf/other/rom.odf:61](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:61>) — Authored comments and/or surrounding definitions with observed value "romulan_capture".

<a id="primarycolor"></a>

### `primarycolor`

**Spellings:** `PrimaryColor`.

**Documented / high confidence**; role: presentation.

Sets the dominant nebula color.

Value form: space-separated numeric tuple. Units: three observed color components; normalization not explicitly documented. Observed in 6 files (6 assignments).

Examples:

```text
0 0 0.5
0.5 0 0
0.5 0.5 0
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Determine color range and blending with SecondaryColor.

Evidence: [odf/other/mnebula6.odf:114](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:114>) — Authored comments and/or surrounding definitions with observed value 0.5 0.5 0; [odf/other/mnebula7.odf:115](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula7.odf:115>) — Authored comments and/or surrounding definitions with observed value 0.5 0 0.

<a id="raceicon"></a>

### `raceicon`

**Spellings:** `raceIcon`.

**Documented / high confidence**; role: asset.

Names the faction/race icon sprite.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"race_icon_borg"
"race_icon_fed"
"race_icon_kling"
```

Uncertainty: Specific UI panels and fallback handling are unknown.

Next investigation: Identify race-selection, roster and HUD consumers of the icon.

Evidence: [odf/other/fed.odf:65](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:65>) — Authored comments and/or surrounding definitions with observed value "race_icon_fed"; [odf/other/rom.odf:71](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:71>) — Authored comments and/or surrounding definitions with observed value "race_icon_rom".

<a id="reallength"></a>

### `reallength`

**Spellings:** `realLength`.

**Inferred / medium confidence**; role: presentation.

Likely sets the intended physical/model length used for scaling a vessel or station. It consistently sits in CHANGE THE SCALE blocks.

Value form: numeric scalar. Units: world-length scale, physical meter equivalence unverified. Observed in 246 files (246 assignments).

Examples:

```text
650
950
850
```

Uncertainty: Axis, bounding-box measurement, collision scale and interaction with scaleM3D are unknown.

Next investigation: Recover the geometric measurement used to scale a model to realLength.

Evidence: [odf/ships/K_Neghvar.odf:130](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_Neghvar.odf:130>) — Authored comments and/or surrounding definitions with observed value 725; [odf/ships/k_tos_boss.odf:110](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/k_tos_boss.odf:110>) — Authored comments and/or surrounding definitions with observed value 800.

<a id="receivesrandomsystemdamage"></a>

### `receivesrandomsystemdamage`

**Spellings:** `receivesRandomSystemDamage`.

**Inferred / medium confidence**; role: gameplay.

A likely opt-out flag for random subsystem damage; several Borg hulls explicitly set it to zero.

Value form: numeric scalar. Units: observed 0/1 flag. Observed in 15 files (15 assignments).

Examples:

```text
0
```

Uncertainty: This does not prove immunity to targeted system hits or destruction effects.

Next investigation: Determine which damage path checks the flag and how targeted hits interact.

Evidence: [odf/ships/B_tng_cube.odf:122](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_tng_cube.odf:122>) — Authored comments and/or surrounding definitions with observed value 0.

<a id="repairindex"></a>

### `repair<N>`

**Spellings:** `Repair0`, `Repair1`, `Repair2`, `Repair3`, `Repair4`.

**Documented / high confidence**; role: gameplay.

Defines a captain repair-skill level row. The first component is a repair-rate multiplier, the second an XP threshold and the third extra information; the level4 row explicitly switches to self-repair and does not use the multiplier.

Value form: comma-separated numeric tuple. Units: repair multiplier, XP threshold, auxiliary value. Observed in 4 files (20 assignments).

Examples:

```text
1.0,  500.0, 0.0
2.0,  500.0, 0.0
3.0,  500.0, 0.0
```

Uncertainty: The self-repair activation and targets/rates are not defined by the data.

Next investigation: Recover the level4 branch and which repair process earlier multipliers affect.

Evidence: [odf/other/fed.odf:194](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:194>) — Authored comments and/or surrounding definitions with observed value 1.0,  500.0, 0.0; [odf/other/fed.odf:198](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:198>) — Authored comments and/or surrounding definitions with observed value 4.0,  500.0, 0.0.

<a id="resizeindex"></a>

### `resize<N>`

**Spellings:** `resize2`, `resize3`, `resize4`, `resize5`.

**Inferred / low confidence**; role: presentation.

A flag on selected flare layers, likely controlling resizing of those layers.

Value form: numeric scalar. Units: unknown. Observed in 1 files (4 assignments).

Examples:

```text
1
```

Uncertainty: Layer indexing is observed; coordinate frame, scale and exact resize rule are not documented.

Next investigation: Find what resizeN changes and which event triggers resizing.

Evidence: [odf/other/lens_flare.odf:31](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:31>) — Authored comments and/or surrounding definitions with observed value 1.

<a id="scalem3d"></a>

### `scalem3d`

**Spellings:** `scaleM3D`.

**Documented / high confidence**; role: presentation.

Scales an M3D model; nebula comments explicitly describe changing M3D scale and ship blocks use the same name.

Value form: numeric scalar. Units: dimensionless scale intended. Observed in 17 files (17 assignments).

Examples:

```text
.1
.2
5
```

Uncertainty: Global versus per-axis scaling and precedence with realLength are unverified.

Next investigation: Determine how scaleM3D combines with realLength and object/editor scale.

Evidence: [odf/ships/Castellani.odf:36](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/Castellani.odf:36>) — Authored comments and/or surrounding definitions with observed value 4.5; [odf/ships/F_Enterprise_E.odf:83](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise_E.odf:83>) — Authored comments and/or surrounding definitions with observed value 5.

<a id="secondarycolor"></a>

### `secondarycolor`

**Spellings:** `SecondaryColor`.

**Documented / high confidence**; role: presentation.

Sets the nebula secondary color.

Value form: comma-separated numeric tuple. Units: three observed color components; normalization not explicitly documented. Observed in 6 files (6 assignments).

Examples:

```text
0,0,0
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Determine which density or light regions use the secondary color.

Evidence: [odf/other/mnebula6.odf:116](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:116>) — Authored comments and/or surrounding definitions with observed value 0,0,0.

<a id="sensorslevel"></a>

### `sensorslevel`

**Spellings:** `sensorsLevel`.

**Documented / high confidence**; role: gameplay.

Sets the general upgradeable sensors statistic displayed to the player. The ODF labels these values as system levels, separate from hit-point endpoints.

Value form: numeric scalar. Units: system-level rating, exact scale unknown. Observed in 168 files (168 assignments).

Examples:

```text
500
650
1000
```

Uncertainty: The conversion from level to current hit points or performance is not recovered.

Next investigation: Recover how sensorslevel feeds damage, performance and upgrade calculations.

Evidence: [odf/ships/K_Death_Rite.odf:30](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_Death_Rite.odf:30>) — Authored comments and/or surrounding definitions with observed value 1050; [odf/ships/K_Death_Rite_Detector.odf:31](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_Death_Rite_Detector.odf:31>) — Authored comments and/or surrounding definitions with observed value 700.

<a id="shielddamagedcolor"></a>

### `shielddamagedcolor`

**Spellings:** `shieldDamagedColor`.

**Documented / high confidence**; role: presentation.

Sets the shield effect color in the damaged condition. The race comment explicitly uses components out of 1 rather than 255.

Value form: space-separated numeric tuple. Units: normalized three-component color. Observed in 4 files (4 assignments).

Examples:

```text
0.0 0.5 0.0
0.32 0.5 0.69
.75 0 0
```

Uncertainty: The health threshold and blend versus discrete switch are not specified.

Next investigation: Recover the shield-health color transition rule and RGB channel ordering.

Evidence: [odf/other/fed.odf:210](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:210>) — Authored comments and/or surrounding definitions with observed value 0.32 0.5 0.69; [odf/other/rom.odf:209](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:209>) — Authored comments and/or surrounding definitions with observed value 0.0 0.5 0.0.

<a id="shieldhealthycolor"></a>

### `shieldhealthycolor`

**Spellings:** `shieldHealthyColor`.

**Documented / high confidence**; role: presentation.

Sets the shield effect color in the healthy condition. The race comment explicitly uses components out of 1 rather than 255.

Value form: space-separated numeric tuple. Units: normalized three-component color. Observed in 4 files (4 assignments).

Examples:

```text
0.0 0.5 0.0
0.32 0.5 0.69
.75 0 0
```

Uncertainty: The health threshold and blend versus discrete switch are not specified.

Next investigation: Recover the shield-health color transition rule and RGB channel ordering.

Evidence: [odf/other/fed.odf:209](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:209>) — Authored comments and/or surrounding definitions with observed value 0.32 0.5 0.69; [odf/other/rom.odf:208](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:208>) — Authored comments and/or surrounding definitions with observed value 0.0 0.5 0.0.

<a id="shieldslevel"></a>

### `shieldslevel`

**Spellings:** `shieldsLevel`.

**Documented / high confidence**; role: gameplay.

Sets the general upgradeable shields statistic displayed to the player. The ODF labels these values as system levels, separate from hit-point endpoints.

Value form: numeric scalar. Units: system-level rating, exact scale unknown. Observed in 174 files (174 assignments).

Examples:

```text
200
0
400
```

Uncertainty: The conversion from level to current hit points or performance is not recovered.

Next investigation: Recover how shieldslevel feeds damage, performance and upgrade calculations.

Evidence: [odf/ships/K_D_3.odf:28](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_3.odf:28>) — Authored comments and/or surrounding definitions with observed value 600; [odf/ships/K_D_7.odf:30](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_7.odf:30>) — Authored comments and/or surrounding definitions with observed value 900.

<a id="sidedraghalflife"></a>

### `sidedraghalflife`

**Spellings:** `sideDragHalfLife`.

**Documented / medium confidence**; role: gameplay.

Intended time for sideways velocity to halve, controlling side-slip/skidding. Old comments say zero removes side-slip and one means a one-second halving time.

Value form: numeric scalar with observed trailing semicolon. Units: seconds per authored example. Observed in 1 files (1 assignments).

Examples:

```text
.3;
```

Uncertainty: Annotated as read and possibly used, inside the obsolete/obscure block.

Next investigation: Recover the lateral damping equation, zero handling and active controller.

Evidence: [odf/other/craft_physics.odf:140](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:140>) — Authored comments and/or surrounding definitions with observed value .3;.

<a id="sizevariance"></a>

### `sizevariance`

**Spellings:** `SizeVariance`.

**Inferred / medium confidence**; role: presentation.

Likely controls variation of nebula blob sizes around BlobSize.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
0
```

Uncertainty: The only sampled value is zero; absolute versus relative spread and distribution are unknown.

Next investigation: Recover the size-variation distribution and scaling.

Evidence: [odf/other/mnebula6.odf:88](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:88>) — Authored comments and/or surrounding definitions with observed value 0.

<a id="specialindex"></a>

### `special<N>`

**Spellings:** `Special0`, `Special1`, `Special2`, `Special3`, `Special4`.

**Documented / high confidence**; role: gameplay.

Defines the race-specific captain special-skill table. Federation Command reduces the apparent damage to functioning systems, whereas Klingon Honor gives temporary hit/damage bonuses after a finishing blow; identical keys do not represent one universal ability.

Value form: comma-separated numeric tuple. Units: race-specific bonus, XP threshold, auxiliary value/timing. Observed in 4 files (20 assignments).

Examples:

```text
0.0,   500.0, 0.0
0,  500.0, 0.0
0.20,  500.0, 0.0
```

Uncertainty: Triple components and final-level exceptions must be interpreted by race; Borg/Romulan rows may be placeholders.

Next investigation: Recover each race-specific special branch, duration handling and unsupported-race fallback.

Evidence: [odf/other/kling.odf:204](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/kling.odf:204>) — Authored comments and/or surrounding definitions with observed value 0.0,   500.0, 0.0; [odf/other/fed.odf:202](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:202>) — Authored comments and/or surrounding definitions with observed value 0.0,   500.0, 0.0.

<a id="splattexturename"></a>

### `splattexturename`

**Spellings:** `SplatTextureName`.

**Inferred / medium confidence**; role: asset.

Names a texture used by the nebula generator/renderer; the observed nebula_density asset suggests a density or splat map.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
"nebula_density"
```

Uncertainty: Density sampling versus sprite rendering is not established.

Next investigation: Find the texture binding and channel usage in the nebula renderer.

Evidence: [odf/other/mnebula6.odf:120](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:120>) — Authored comments and/or surrounding definitions with observed value "nebula_density".

<a id="spriteindex"></a>

### `sprite<N>`

**Spellings:** `sprite0`, `sprite1`, `sprite10`, `sprite11`, `sprite12`, `sprite13`, `sprite14`, `sprite15`, `sprite16`, `sprite17`, `sprite18`, `sprite2`, `sprite3`, `sprite4`, `sprite5`, `sprite6`, `sprite7`, `sprite8`, `sprite9`.

**Inferred / medium confidence**; role: asset.

Names the sprite for indexed flare layer N; layer comments identify rays, streaks, rings and spots.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 1 files (19 assignments).

Examples:

```text
"x_sunflare_03"
"xglow_soft"
"x_sunflare_04"
```

Uncertainty: Layer indexing is observed; coordinate frame, scale and exact resize rule are not documented.

Next investigation: Recover layer enumeration and sprite-resource lookup.

Evidence: [odf/other/lens_flare.odf:21](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:21>) — Authored comments and/or surrounding definitions with observed value "x_sunflare_03"; [odf/other/lens_flare.odf:118](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:118>) — Authored comments and/or surrounding definitions with observed value "x_sunflare_03".

<a id="spritedensity"></a>

### `spritedensity`

**Spellings:** `SpriteDensity`.

**Documented / high confidence**; role: presentation.

Controls the population/density of nebula sprites relative to a sprite-sized volume.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
1.1
```

Uncertainty: The density sampling and volume formula are unknown.

Next investigation: Determine how SpriteDensity and SpriteSize determine the generated count.

Evidence: [odf/other/mnebula6.odf:112](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:112>) — Authored comments and/or surrounding definitions with observed value 1.1.

<a id="spritesize"></a>

### `spritesize`

**Spellings:** `SpriteSize`.

**Documented / high confidence**; role: presentation.

Sets the size of sprites used to render the procedural nebula.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
200.0
```

Uncertainty: World or screen space and radius-versus-width remain unknown.

Next investigation: Recover sprite geometry scaling and interaction with model scale.

Evidence: [odf/other/mnebula6.odf:110](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:110>) — Authored comments and/or surrounding definitions with observed value 200.0.

<a id="startyear"></a>

### `startyear`

**Spellings:** `startYear`.

**Inferred / medium confidence**; role: gameplay.

Likely defines the earliest year of availability for the craft/object, forming an era-selection interval.

Value form: numeric scalar. Units: year number. Observed in 131 files (131 assignments).

Examples:

```text
1960
2160
2360
```

Uncertainty: Inclusive endpoints, availability UI versus build filtering and out-of-era objects are unverified.

Next investigation: Find the era/ship-selection filter and determine interval boundary handling for startyear.

Evidence: [odf/ships/F_UberBirdofPrey.odf:42](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_UberBirdofPrey.odf:42>) — Authored comments and/or surrounding definitions with observed value 2000; [odf/ships/K_D_3.odf:40](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_3.odf:40>) — Authored comments and/or surrounding definitions with observed value 2253.

<a id="suppresseswarp"></a>

### `suppresseswarp`

**Spellings:** `suppressesWarp`.

**Documented / high confidence**; role: gameplay.

A flag indicating whether a nebula prevents warping. A zero value is explicitly annotated as allowing warp in that nebula.

Value form: numeric scalar. Units: observed 0/1 flag. Observed in 2 files (2 assignments).

Examples:

```text
0
1
```

Uncertainty: Affected volume, entry/exit handling and warp already in progress are unknown.

Next investigation: Determine whether it blocks warp activation, cancels travel, or both.

Evidence: [odf/other/mnebula12.odf:83](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula12.odf:83>) — Authored comments and/or surrounding definitions with observed value 0; [odf/other/mnebula13.odf:86](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula13.odf:86>) — Authored comments and/or surrounding definitions with observed value 1.

<a id="targetingindex"></a>

### `targeting<N>`

**Spellings:** `Targeting0`, `Targeting1`, `Targeting2`, `Targeting3`, `Targeting4`.

**Documented / high confidence**; role: gameplay.

Defines a captain targeting-skill level row. The race file describes each triple as bonus, XP until the next level, and extra information; targeting affects chance to hit.

Value form: comma-separated numeric tuple. Units: hit-chance bonus, XP threshold, auxiliary value. Observed in 4 files (20 assignments).

Examples:

```text
0.0,   500.0,  0.0
0.15,  500.0,  0.0
0.30,  500.0,  0.0
```

Uncertainty: Level4 comments mention 95% for 30 seconds although the first value remains 0.50; simple addition is not proven.

Next investigation: Recover indexed level progression, bonus formula and interpretation of the third component.

Evidence: [odf/other/fed.odf:180](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:180>) — Authored comments and/or surrounding definitions with observed value 0.0,   500.0,  0.0; [odf/other/fed.odf:184](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:184>) — Authored comments and/or surrounding definitions with observed value 0.50,  500.0, 30.0.

<a id="texturename"></a>

### `texturename`

**Spellings:** `textureName`.

**Inferred / medium confidence**; role: asset.

Names the texture resource for a nebula-set definition, observed as nebula_test.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
"nebula_test"
"nebula_test1"
```

Uncertainty: The map layout and relationship to UnitsPerTexel are unknown.

Next investigation: Trace which nebula-set texture channels are consumed.

Evidence: [odf/other/nebulaset.odf:15](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/nebulaset.odf:15>) — Authored comments and/or surrounding definitions with observed value "nebula_test"; [odf/other/nebulaset1.odf:15](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/nebulaset1.odf:15>) — Authored comments and/or surrounding definitions with observed value "nebula_test1".

<a id="tractorbeamcolor"></a>

### `tractorbeamcolor`

**Spellings:** `tractorBeamColor`.

**Documented / high confidence**; role: presentation.

Sets the faction tractor-beam color as three components.

Value form: space-separated numeric tuple. Units: three color components; normalization inferred from samples. Observed in 1 files (1 assignments).

Examples:

```text
0 .1 .5
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Determine color normalization and interaction with beam sprite/material.

Evidence: [odf/other/fed.odf:213](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:213>) — Authored comments and/or surrounding definitions with observed value 0 .1 .5.

<a id="transience"></a>

### `transience`

**Spellings:** `Transience`.

**Documented / high confidence**; role: presentation.

Controls the decrease of noise over successive octaves.

Value form: numeric scalar. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
2
```

Uncertainty: Division, attenuation curve and allowed range are not documented.

Next investigation: Determine how each octave amplitude is derived from Transience.

Evidence: [odf/other/mnebula6.odf:104](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/mnebula6.odf:104>) — Authored comments and/or surrounding definitions with observed value 2.

<a id="turnalphamaxspeedfraction"></a>

### `turnalphamaxspeedfraction`

**Spellings:** `turnAlphaMaxSpeedFraction`.

**Inferred / low confidence**; role: gameplay.

An old turn angular-acceleration tuning field, apparently intended as the speed fraction for reaching nominal angular acceleration. The base groups these settings under a comment saying they do not seem to do anything.

Value form: numeric scalar with observed trailing semicolon. Units: dimensionless fraction intended; runtime use unknown. Observed in 6 files (6 assignments).

Examples:

```text
.25;
.2;
0.25;
```

Uncertainty: Its intended analogy with Omega fields does not establish an active Alpha consumer.

Next investigation: Locate a live read-and-use path for turnalphamaxspeedfraction, not merely a parser read.

Evidence: [odf/other/craft_physics.odf:156](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:156>) — Authored comments and/or surrounding definitions with observed value 0.25;; [odf/other/class_scout_physics.odf:52](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:52>) — Authored comments and/or surrounding definitions with observed value .2;.

<a id="turnomegamaxspeedfraction"></a>

### `turnomegamaxspeedfraction`

**Spellings:** `turnOmegaMaxSpeedFraction`.

**Documented / medium confidence**; role: gameplay.

Sets the speed fraction at which turn responsiveness reaches its nominal rate; below this, the documented turn example blends from its at-rest fraction.

Value form: numeric scalar with observed trailing semicolon. Units: fraction of maximum ship speed. Observed in 6 files (6 assignments).

Examples:

```text
0.25;
```

Uncertainty: The turn comment is internally inconsistent about endpoints and accidentally mentions bankOmega; the exact curve needs consumer evidence.

Next investigation: Recover the turn rate-versus-throttle curve and behavior for fractions outside 0..1.

Evidence: [odf/other/craft_physics.odf:74](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:74>) — Authored comments and/or surrounding definitions with observed value 0.25;; [odf/other/class_scout_physics.odf:51](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_scout_physics.odf:51>) — Authored comments and/or surrounding definitions with observed value 0.25;.

<a id="unitspertexel"></a>

### `unitspertexel`

**Spellings:** `unitsPerTexel`.

**Inferred / medium confidence**; role: presentation.

Likely maps nebula-set texture pixels to world-space distance. The texture resource and numeric scale are defined together.

Value form: numeric scalar. Units: likely world units per texture pixel; not confirmed. Observed in 2 files (2 assignments).

Examples:

```text
200
```

Uncertainty: Exact consumer, default and range behavior remain unverified.

Next investigation: Recover how texel coordinates map to generated nebula positions.

Evidence: [odf/other/nebulaset.odf:16](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/nebulaset.odf:16>) — Authored comments and/or surrounding definitions with observed value 200.

<a id="useradialmenu"></a>

### `useradialmenu`

**Spellings:** `useRadialMenu`.

**Inferred / low confidence**; role: ui.

Likely opts an object into the radial/context command menu; the surviving example enables it on a flying asteroid.

Value form: numeric scalar. Units: observed 0/1 flag. Observed in 7 files (7 assignments).

Examples:

```text
1
```

Uncertainty: The UI flag does not itself prove a gameplay operation exists or can execute.

Next investigation: Determine the menu filter and whether the order dispatcher also checks useradialmenu.

Evidence: [odf/other/flyingasteroid.odf:61](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/flyingasteroid.odf:61>) — Authored comments and/or surrounding definitions with observed value 1.

<a id="verticalaccel"></a>

### `verticalaccel`

**Spellings:** `verticalAccel`.

**Documented / medium confidence**; role: gameplay.

Intended limit for vertical acceleration; class-file comments describe that purpose. The base says these fields are read but seem unused.

Value form: numeric scalar with observed trailing semicolon. Units: world-distance/time convention unknown. Observed in 12 files (12 assignments).

Examples:

```text
60
5
1
```

Uncertainty: Class variants say it does not seem to have an effect; not a confirmed new movement capability.

Next investigation: Trace whether any active movement controller uses verticalaccel.

Evidence: [odf/other/craft_borg_physics.odf:20](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_borg_physics.odf:20>) — Authored comments and/or surrounding definitions with observed value 200; [odf/other/class_science_physics.odf:6](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/class_science_physics.odf:6>) — Authored comments and/or surrounding definitions with observed value 60.

<a id="verticaldraghalflife"></a>

### `verticaldraghalflife`

**Spellings:** `verticalDragHalfLife`.

**Documented / medium confidence**; role: gameplay.

Intended vertical counterpart of the velocity-half-life damping parameter. The base explicitly annotates it as read but unused.

Value form: numeric scalar with observed trailing semicolon. Units: seconds by adjoining half-life comments. Observed in 1 files (1 assignments).

Examples:

```text
.1;
```

Uncertainty: Vertical damping is an intended interpretation, not a demonstrated live effect.

Next investigation: Check whether there is a vertical damping consumer after the parser read.

Evidence: [odf/other/craft_physics.odf:141](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:141>) — Authored comments and/or surrounding definitions with observed value .1;.

<a id="verticalspeed"></a>

### `verticalspeed`

**Spellings:** `verticalSpeed`.

**Documented / medium confidence**; role: gameplay.

Intended limit for vertical travel speed; class-file comments describe that purpose. The base says these fields are read but seem unused.

Value form: numeric scalar with observed trailing semicolon. Units: world-distance/time convention unknown. Observed in 12 files (12 assignments).

Examples:

```text
60
5
1
```

Uncertainty: Class variants say it does not seem to have an effect; not a confirmed new movement capability.

Next investigation: Trace whether any active movement controller uses verticalspeed.

Evidence: [odf/other/craft_borg_physics.odf:17](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_borg_physics.odf:17>) — Authored comments and/or surrounding definitions with observed value 200; [odf/other/craft_physics.odf:126](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/craft_physics.odf:126>) — Authored comments and/or surrounding definitions with observed value 500;.

<a id="warpinsound"></a>

### `warpinsound`

**Spellings:** `warpInSound`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with entering warp; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"ST_AMB_BOR_Warp_In"
"ST_AMB_FED_Warp_In"
"ST_AMB_KLI_Warp_In"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes warpinsound.

Evidence: [odf/other/fed.odf:28](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:28>) — Authored comments and/or surrounding definitions with observed value "ST_AMB_FED_Warp_In"; [odf/other/rom.odf:33](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:33>) — Authored comments and/or surrounding definitions with observed value "ST_AMB_ROM_Warp_In".

<a id="warpoutsound"></a>

### `warpoutsound`

**Spellings:** `warpOutSound`.

**Inferred / medium confidence**; role: asset.

Names an audio resource/group associated with leaving warp; the supplied asset names and adjacent event fields support this interpretation.

Value form: quoted resource/name or quoted string list. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"ST_AMB_BOR_Warp_Out"
"ST_AMB_FED_Warp_Out"
"ST_AMB_KLI_Warp_Out"
```

Uncertainty: Precise trigger, spatialization and distinction between group and individual sound lookup are unverified.

Next investigation: Identify the audio event that consumes warpoutsound.

Evidence: [odf/other/fed.odf:29](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:29>) — Authored comments and/or surrounding definitions with observed value "ST_AMB_FED_Warp_Out"; [odf/other/rom.odf:34](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/rom.odf:34>) — Authored comments and/or surrounding definitions with observed value "ST_AMB_ROM_Warp_Out".

<a id="weaponslevel"></a>

### `weaponslevel`

**Spellings:** `weaponsLevel`.

**Documented / high confidence**; role: gameplay.

Sets the general upgradeable weapons statistic displayed to the player. The ODF labels these values as system levels, separate from hit-point endpoints.

Value form: numeric scalar. Units: system-level rating, exact scale unknown. Observed in 166 files (166 assignments).

Examples:

```text
200
800
500
```

Uncertainty: The conversion from level to current hit points or performance is not recovered.

Next investigation: Recover how weaponslevel feeds damage, performance and upgrade calculations.

Evidence: [odf/ships/K_D_3.odf:25](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_3.odf:25>) — Authored comments and/or surrounding definitions with observed value 900; [odf/ships/K_D_7.odf:27](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_7.odf:27>) — Authored comments and/or surrounding definitions with observed value 1100.

<a id="widthindex"></a>

### `width<N>`

**Spellings:** `width4`, `width5`.

**Inferred / medium confidence**; role: presentation.

Sets a separate horizontal size for indexed flare layer N, used by the horizontal/vertical streak entries.

Value form: numeric scalar. Units: unknown. Observed in 1 files (2 assignments).

Examples:

```text
1.35
0.2
```

Uncertainty: Layer indexing is observed; coordinate frame, scale and exact resize rule are not documented.

Next investigation: Determine width units and how it overrides sizeN.

Evidence: [odf/other/lens_flare.odf:42](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:42>) — Authored comments and/or surrounding definitions with observed value 1.35; [odf/other/lens_flare.odf:50](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:50>) — Authored comments and/or surrounding definitions with observed value 0.2.

<a id="xpdamagegiven"></a>

### `xpdamagegiven`

**Spellings:** `xpDamageGiven`.

**Documented / high confidence**; role: gameplay.

Adds captain experience for each point of damage inflicted on an enemy.

Value form: numeric scalar. Units: XP per damage point per authored comment. Observed in 4 files (4 assignments).

Examples:

```text
100.0
```

Uncertainty: Damage credited before/after mitigation and eligible attacks are unknown.

Next investigation: Recover the experience award event and which captain/object receives credit.

Evidence: [odf/other/fed.odf:174](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:174>) — Authored comments and/or surrounding definitions with observed value 100.0.

<a id="xpdamagetaken"></a>

### `xpdamagetaken`

**Spellings:** `xpDamageTaken`.

**Documented / high confidence**; role: gameplay.

Adds captain experience for each point of damage received from an enemy.

Value form: numeric scalar. Units: XP per damage point per authored comment. Observed in 4 files (4 assignments).

Examples:

```text
100.0
```

Uncertainty: Shield/hull distinctions, friendly damage and credit recipient are unknown.

Next investigation: Recover the experience award event and which captain/object receives credit.

Evidence: [odf/other/fed.odf:175](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:175>) — Authored comments and/or surrounding definitions with observed value 100.0.

<a id="xpdestroyed"></a>

### `xpdestroyed`

**Spellings:** `xpDestroyed`.

**Documented / high confidence**; role: gameplay.

Adds experience when an enemy craft is destroyed.

Value form: numeric scalar. Units: XP per destroyed enemy craft per authored comment. Observed in 4 files (4 assignments).

Examples:

```text
0.0
```

Uncertainty: Kill credit, shared credit and object categories are unknown.

Next investigation: Recover the experience award event and which captain/object receives credit.

Evidence: [odf/other/fed.odf:176](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/fed.odf:176>) — Authored comments and/or surrounding definitions with observed value 0.0.

