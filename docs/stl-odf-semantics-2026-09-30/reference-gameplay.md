# Ships, weapons, stations and construction

See [README](README.md) for confidence definitions and [the index](command-index.md) for all families. Documented means authored ODF comments describe the intent; it does not mean tested runtime support.

<a id="accelerate"></a>

### `accelerate`

**Spellings:** `Accelerate`.

**Unresolved / low confidence**; role: ambiguous.

The planet ODF sets Accelerate=1 next to point generation fields, but the affected process is not explained.

Value form: numeric literal. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
1
```

Uncertainty: Point-system semantics and Fleet Ops runtime support are unverified.

Next investigation: Which STL parser or gameplay call site reads Accelerate, and what does its value control?

Evidence: [odf/stations/planet.odf:25](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:25>) — observed assignment and local context.

<a id="aftphotonspread"></a>

### `aftphotonspread`

**Spellings:** `aftPhotonSpread`.

**Inferred / medium confidence**; role: gameplay.

The Stinger places this flag next to its aft photon count, suggesting a spread setting for aft torpedoes; no spread geometry is specified.

Value form: numeric literal. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
1
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred aftPhotonSpread behavior?

Evidence: [odf/ships/K_Stinger.odf:40](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_Stinger.odf:40>) — observed assignment and local context.

<a id="ambient"></a>

### `ambient`

**Spellings:** `ambient`.

**Inferred / medium confidence**; role: presentation.

The station light material uses an RGB ambient colour, with a sun comment explaining that it glows on its own.

Value form: three space-separated numeric components. Units: unknown. Observed in 12 files (12 assignments).

Examples:

```text
0 0 0
255 255 255
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred ambient behavior?

Evidence: [odf/stations/sun.odf:27](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/sun.odf:27>) — assignment and nearby author comment; [odf/stations/bajor.odf:11](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/bajor.odf:11>) — observed assignment and local context.

<a id="avoidanceboundingellipsoidscaleforcraft"></a>

### `avoidanceboundingellipsoidscaleforcraft`

**Spellings:** `avoidanceBoundingEllipsoidScaleForCraft`.

**Documented / high confidence**; role: gameplay.

Multiplies a craft prevention ellipsoid to form its avoidance shell.

Value form: numeric literal. Units: scale. Observed in 1 files (1 assignments).

Examples:

```text
1.1
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement avoidanceBoundingEllipsoidScaleForCraft as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/craft.odf:199](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:199>) — assignment and nearby author comment.

<a id="avoidanceboundingellipsoidscalefornoncraft"></a>

### `avoidanceboundingellipsoidscalefornoncraft`

**Spellings:** `avoidanceBoundingEllipsoidScaleForNonCraft`.

**Documented / high confidence**; role: gameplay.

Multiplies a noncraft prevention ellipsoid, especially an asteroid, to form its avoidance shell.

Value form: numeric literal. Units: scale. Observed in 1 files (1 assignments).

Examples:

```text
1.3
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement avoidanceBoundingEllipsoidScaleForNonCraft as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/craft.odf:203](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:203>) — assignment and nearby author comment.

<a id="avoidancehysteresisscaleforawareness"></a>

### `avoidancehysteresisscaleforawareness`

**Spellings:** `avoidanceHysteresisScaleForAwareness`.

**Documented / high confidence**; role: gameplay.

Multiplies the avoidance shell to form a larger obstacle-awareness shell used in collision prediction.

Value form: numeric literal. Units: scale. Observed in 1 files (1 assignments).

Examples:

```text
1.015
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement avoidanceHysteresisScaleForAwareness as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/craft.odf:210](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:210>) — assignment and nearby author comment.

<a id="avoidancemaxlookaheadtime"></a>

### `avoidancemaxlookaheadtime`

**Spellings:** `avoidanceMaxLookaheadTime`.

**Documented / high confidence**; role: gameplay.

Caps future collision prediction time; collisions at this horizon have minimum urgency.

Value form: numeric literal. Units: game-core time. Observed in 1 files (1 assignments).

Examples:

```text
4.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement avoidanceMaxLookaheadTime as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/craft.odf:184](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:184>) — assignment and nearby author comment.

<a id="avoidancemaxsearchradius"></a>

### `avoidancemaxsearchradius`

**Spellings:** `avoidanceMaxSearchRadius`.

**Documented / high confidence**; role: gameplay.

Caps radius search used to find potential collision obstacles.

Value form: numeric literal. Units: distance units. Observed in 1 files (1 assignments).

Examples:

```text
2000.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement avoidanceMaxSearchRadius as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/craft.odf:191](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:191>) — assignment and nearby author comment.

<a id="avoidancemintimeformaxsteeringurgency"></a>

### `avoidancemintimeformaxsteeringurgency`

**Spellings:** `avoidanceMinTimeForMaxSteeringUrgency`.

**Documented / high confidence**; role: gameplay.

Sets predicted time-to-collision below which steering force reaches maximum urgency.

Value form: numeric literal. Units: game-core time. Observed in 1 files (1 assignments).

Examples:

```text
1.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement avoidanceMinTimeForMaxSteeringUrgency as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/craft.odf:187](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:187>) — assignment and nearby author comment.

<a id="avoidancetimetointerpolatebacktodesiredchase"></a>

### `avoidancetimetointerpolatebacktodesiredchase`

**Spellings:** `avoidanceTimeToInterpolateBackToDesiredChase`.

**Documented / high confidence**; role: gameplay.

Sets game time after a collision threat disappears to interpolate back to the planned chase/path course.

Value form: numeric literal. Units: game-core time. Observed in 2 files (2 assignments).

Examples:

```text
1.0
0.25
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement avoidanceTimeToInterpolateBackToDesiredChase as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/class_destroyer.odf:122](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_destroyer.odf:122>) — assignment and nearby author comment; [odf/ships/craft.odf:195](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:195>) — assignment and nearby author comment.

<a id="backenginehitchance"></a>

### `backenginehitchance`

**Spellings:** `backEngineHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the back facing hit-chance weight for the engine subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:90](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:90>) — observed assignment and local context; [odf/stations/station.odf:71](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:71>) — observed assignment and local context.

<a id="backhullhitchance"></a>

### `backhullhitchance`

**Spellings:** `backHullHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the back facing hit-chance weight for the hull subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
100
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:88](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:88>) — observed assignment and local context; [odf/stations/station.odf:69](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:69>) — observed assignment and local context.

<a id="backsensorhitchance"></a>

### `backsensorhitchance`

**Spellings:** `backSensorHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the back facing hit-chance weight for the sensor subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:91](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:91>) — observed assignment and local context; [odf/stations/station.odf:72](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:72>) — observed assignment and local context.

<a id="backshieldhitchance"></a>

### `backshieldhitchance`

**Spellings:** `backShieldHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the back facing hit-chance weight for the shield subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:89](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:89>) — observed assignment and local context; [odf/stations/station.odf:70](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:70>) — observed assignment and local context.

<a id="backweaponhitchance"></a>

### `backweaponhitchance`

**Spellings:** `backWeaponHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the back facing hit-chance weight for the weapon subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:92](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:92>) — observed assignment and local context; [odf/stations/station.odf:73](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:73>) — observed assignment and local context.

<a id="boostmodifier"></a>

### `boostmodifier`

**Spellings:** `boostModifier`.

**Inferred / medium confidence**; role: gameplay.

The value appears with a reticle-lock comment and likely adjusts how quickly targeting lock increases; its relationship to phaser/photon-specific modifiers is unclear.

Value form: numeric literal. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
0.02
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred boostModifier behavior?

Evidence: [odf/ships/craft.odf:9](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:9>) — assignment and nearby author comment.

<a id="borgentbuildlist"></a>

### `borgentbuildlist`

**Spellings:** `borgEntBuildList`.

**Documented / high confidence**; role: gameplay.

Defines the Borg ENT construction list for the owning planet. Each pair is an ODF name and the required planet upgrade level, numbered from zero; the trailing 0/1 is a level requirement, not an unexplained Boolean.

Value form: repeated quoted pairs: "ODF name" "required planet upgrade level". Units: zero-based planet upgrade level. Observed in 6 files (6 assignments).

Examples:

```text
"b_ent_shipyard" "1"
"b_ent_turret" "0"
"b_ent_shipyard" "0"
"b_ent_turret" "0"
"b_ent_sensor" "1"
"b_ent_shipyard" "0"
"b_ent_turret" "0"
"b_ent_sensor" "0"
"b_recycler" "0"
"b_tech_assim" "0"
```

Uncertainty: Selection by owner/era and inheritance are not traced; number parsing, duplicate items and invalid level handling remain unknown.

Next investigation: Recover owner/era selection, planet upgrade gating, duplicate handling and validation of required levels.

Evidence: [odf/stations/classj.odf:58](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:58>) — Shared build-list header explicitly defines ODF-name/required-planet-level pairs from zero..

<a id="borgentcommandpost"></a>

### `borgentcommandpost`

**Spellings:** `borgEntCommandPost`.

**Documented / high confidence**; role: gameplay.

Names the Borg ENT command-post ODF associated with a planet, selected by race and era according to the planet header.

Value form: quoted ODF basename. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"b_ent_command"
```

Uncertainty: Creation time, attachment/placement, capture replacement and absent-definition fallbacks are unknown.

Next investigation: Recover when the planet creates or replaces its race/era command post.

Evidence: [odf/stations/planet.odf:47](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:47>) — Header explicitly describes command-post variants by race and era..

<a id="borgtngbuildlist"></a>

### `borgtngbuildlist`

**Spellings:** `borgTngBuildList`.

**Documented / high confidence**; role: gameplay.

Defines the Borg TNG construction list for the owning planet. Each pair is an ODF name and the required planet upgrade level, numbered from zero; the trailing 0/1 is a level requirement, not an unexplained Boolean.

Value form: repeated quoted pairs: "ODF name" "required planet upgrade level". Units: zero-based planet upgrade level. Observed in 6 files (6 assignments).

Examples:

```text
"b_tng_shipyard" "1"
"b_tng_turret" "0"
"b_tng_shipyard" "0"
"b_tng_turret" "0"
"b_tng_sensor" "1"
"b_soliton" "1"
"b_tng_shipyard" "0"
"b_tng_turret" "0"
"b_tng_sensor" "0"
"b_recycler" "0"
"b_tech_assim" "0"
"b_soliton" "0"
```

Uncertainty: Selection by owner/era and inheritance are not traced; number parsing, duplicate items and invalid level handling remain unknown.

Next investigation: Recover owner/era selection, planet upgrade gating, duplicate handling and validation of required levels.

Evidence: [odf/stations/classj.odf:122](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:122>) — Shared build-list header explicitly defines ODF-name/required-planet-level pairs from zero..

<a id="borgtngcommandpost"></a>

### `borgtngcommandpost`

**Spellings:** `borgTngCommandPost`.

**Documented / high confidence**; role: gameplay.

Names the Borg TNG command-post ODF associated with a planet, selected by race and era according to the planet header.

Value form: quoted ODF basename. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"b_tng_command"
```

Uncertainty: Creation time, attachment/placement, capture replacement and absent-definition fallbacks are unknown.

Next investigation: Recover when the planet creates or replaces its race/era command post.

Evidence: [odf/stations/planet.odf:61](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:61>) — Header explicitly describes command-post variants by race and era..

<a id="borgtosbuildlist"></a>

### `borgtosbuildlist`

**Spellings:** `borgTosBuildList`.

**Documented / high confidence**; role: gameplay.

Defines the Borg TOS construction list for the owning planet. Each pair is an ODF name and the required planet upgrade level, numbered from zero; the trailing 0/1 is a level requirement, not an unexplained Boolean.

Value form: repeated quoted pairs: "ODF name" "required planet upgrade level". Units: zero-based planet upgrade level. Observed in 6 files (6 assignments).

Examples:

```text
"b_tos_shipyard" "1"
"b_tos_turret" "0"
"b_tos_shipyard" "0"
"b_tos_turret" "0"
"b_tos_sensor" "1"
"b_soliton" "1"
"b_tos_shipyard" "0"
"b_tos_turret" "0"
"b_tos_sensor" "0"
"b_recycler" "0"
"b_tech_assim" "0"
"b_soliton" "0"
```

Uncertainty: Selection by owner/era and inheritance are not traced; number parsing, duplicate items and invalid level handling remain unknown.

Next investigation: Recover owner/era selection, planet upgrade gating, duplicate handling and validation of required levels.

Evidence: [odf/stations/classj.odf:89](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:89>) — Shared build-list header explicitly defines ODF-name/required-planet-level pairs from zero..

<a id="borgtoscommandpost"></a>

### `borgtoscommandpost`

**Spellings:** `borgTosCommandPost`.

**Documented / high confidence**; role: gameplay.

Names the Borg TOS command-post ODF associated with a planet, selected by race and era according to the planet header.

Value form: quoted ODF basename. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"b_tos_command"
```

Uncertainty: Creation time, attachment/placement, capture replacement and absent-definition fallbacks are unknown.

Next investigation: Recover when the planet creates or replaces its race/era command post.

Evidence: [odf/stations/planet.odf:54](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:54>) — Header explicitly describes command-post variants by race and era..

<a id="buildlist"></a>

### `buildlist`

**Spellings:** `buildList`.

**Inferred / medium confidence**; role: gameplay.

The command post supplies an ordered list of station ODF names available for construction; no row flags appear in this generic list.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"f_ent_engineering" "f_ent_shipyard" "f_ent_turret" "f_ent_sensor" "f_soliton" "f_annex" "f_trade"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Recover the meaning of each trailing 0/1 row flag, list enumeration and race/era selection.

Evidence: [odf/stations/F_ENT_mining_base.odf:137](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/F_ENT_mining_base.odf:137>) — observed assignment and local context.

<a id="bumptexturename"></a>

### `bumptexturename`

**Spellings:** `bumpTextureName`.

**Inferred / medium confidence**; role: asset.

Planet material entries name a bump texture asset; the exact texture lookup and rendering behavior are unverified.

Value form: quoted string. Units: unknown. Observed in 26 files (26 assignments).

Examples:

```text
"ClassM_earth_Bump"
"classd_ice_bump"
"classd_lava_bump"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred bumpTextureName behavior?

Evidence: [odf/stations/bajor.odf:8](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/bajor.odf:8>) — observed assignment and local context; [odf/stations/classJb.odf:9](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classJb.odf:9>) — observed assignment and local context.

<a id="cameraradiusscale"></a>

### `cameraradiusscale`

**Spellings:** `cameraRadiusScale`.

**Unresolved / low confidence**; role: ambiguous.

A ship ODF labels this as a shot-director zoom scale, implying a camera distance based on object radius. The exact camera path using it is unverified.

Value form: numeric literal. Units: unknown. Observed in 115 files (115 assignments).

Examples:

```text
1.8
2.5
3.5
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL parser or gameplay call site reads cameraRadiusScale, and what does its value control?

Evidence: [odf/ships/B_tos_cube.odf:150](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_tos_cube.odf:150>) — assignment and nearby author comment; [odf/ships/F_Akula.odf:175](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akula.odf:175>) — assignment and nearby author comment.

<a id="cfg-polarizedhullplatingdamagereduction"></a>

### `cfg_polarizedhullplatingdamagereduction`

**Spellings:** `cfg_polarizedHullPlatingDamageReduction`.

**Documented / high confidence**; role: gameplay.

Ships with hull plating and all energy allocated to shields are said to reduce incoming damage by this amount.

Value form: numeric literal. Units: fraction. Observed in 2 files (2 assignments).

Examples:

```text
0.22
0.1
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement cfg_polarizedHullPlatingDamageReduction as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/F_Excelsior.odf:18](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Excelsior.odf:18>) — assignment and nearby author comment; [odf/ships/F_Excelsior_tng.odf:20](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Excelsior_tng.odf:20>) — assignment and nearby author comment.

<a id="chancechase"></a>

### `chancechase`

**Spellings:** `chanceChase`.

**Documented / high confidence**; role: gameplay.

Sets the authored AI chance weight for the chase attack maneuver; the ODF comment says the maneuver chances should sum to approximately one.

Value form: numeric literal. Units: fractional weight. Observed in 71 files (71 assignments).

Examples:

```text
0.100
0.80
0.25
```

Uncertainty: Normalization, random-choice process, fallback and actual maneuver paths are not specified.

Next investigation: Recover maneuver selection, normalization for invalid totals and transitions between attack paths.

Evidence: [odf/ships/craft.odf:33](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:33>) — Authored four-maneuver group says probabilities should sum to approximately one..

<a id="chancecircle"></a>

### `chancecircle`

**Spellings:** `chanceCircle`.

**Documented / high confidence**; role: gameplay.

Sets the authored AI chance weight for the circle attack maneuver; the ODF comment says the maneuver chances should sum to approximately one.

Value form: numeric literal. Units: fractional weight. Observed in 71 files (71 assignments).

Examples:

```text
0.00
0.25
0.10
```

Uncertainty: Normalization, random-choice process, fallback and actual maneuver paths are not specified.

Next investigation: Recover maneuver selection, normalization for invalid totals and transitions between attack paths.

Evidence: [odf/ships/craft.odf:31](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:31>) — Authored four-maneuver group says probabilities should sum to approximately one..

<a id="chancecloverleaf"></a>

### `chancecloverleaf`

**Spellings:** `chanceCloverleaf`.

**Documented / high confidence**; role: gameplay.

Sets the authored AI chance weight for the cloverleaf attack maneuver; the ODF comment says the maneuver chances should sum to approximately one.

Value form: numeric literal. Units: fractional weight. Observed in 71 files (71 assignments).

Examples:

```text
0.00
0.25
0.30
```

Uncertainty: Normalization, random-choice process, fallback and actual maneuver paths are not specified.

Next investigation: Recover maneuver selection, normalization for invalid totals and transitions between attack paths.

Evidence: [odf/ships/craft.odf:32](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:32>) — Authored four-maneuver group says probabilities should sum to approximately one..

<a id="chanceflyby"></a>

### `chanceflyby`

**Spellings:** `chanceFlyBy`.

**Documented / high confidence**; role: gameplay.

Sets the authored AI chance weight for the flyby attack maneuver; the ODF comment says the maneuver chances should sum to approximately one.

Value form: numeric literal. Units: fractional weight. Observed in 71 files (71 assignments).

Examples:

```text
0.00
0.20
0.25
```

Uncertainty: Normalization, random-choice process, fallback and actual maneuver paths are not specified.

Next investigation: Recover maneuver selection, normalization for invalid totals and transitions between attack paths.

Evidence: [odf/ships/craft.odf:30](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:30>) — Authored four-maneuver group says probabilities should sum to approximately one..

<a id="chunksmokeparticlesystems"></a>

### `chunksmokeparticlesystems`

**Spellings:** `ChunkSmokeParticleSystems`.

**Documented / high confidence**; role: presentation.

The listed particle systems attach smoke/fire to debris chunks after a craft fragments.

Value form: quoted string. Units: unknown. Observed in 1 files (2 assignments).

Examples:

```text
"psdebris_trail_smoke"
"psdebris_trail_fire"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement ChunkSmokeParticleSystems as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/craft.odf:160](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:160>) — assignment and nearby author comment.

<a id="coronacolor"></a>

### `coronacolor`

**Spellings:** `coronaColor`.

**Inferred / medium confidence**; role: presentation.

Planet ODFs give a three-component colour for the corona effect; component scale and renderer mapping are not documented.

Value form: three space-separated numeric components. Units: unknown. Observed in 23 files (23 assignments).

Examples:

```text
242 144 98
47 37 15
94 145 176
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred coronaColor behavior?

Evidence: [odf/stations/classj.odf:26](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:26>) — observed assignment and local context; [odf/stations/earth.odf:39](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/earth.odf:39>) — observed assignment and local context.

<a id="coronaselfillumination"></a>

### `coronaselfillumination`

**Spellings:** `coronaSelfIllumination`.

**Inferred / medium confidence**; role: presentation.

Planet ODFs use a numeric self-illumination setting for corona rendering; the exact light calculation is unknown.

Value form: numeric literal. Units: unknown. Observed in 14 files (14 assignments).

Examples:

```text
.5
1
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred coronaSelfIllumination behavior?

Evidence: [odf/stations/earth.odf:40](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/earth.odf:40>) — observed assignment and local context; [odf/stations/sun.odf:23](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/sun.odf:23>) — observed assignment and local context.

<a id="coronawidth"></a>

### `coronawidth`

**Spellings:** `coronaWidth`.

**Inferred / medium confidence**; role: presentation.

Planet ODFs use a numeric width for the corona visual; physical or screen units are not specified.

Value form: numeric literal. Units: unknown. Observed in 23 files (23 assignments).

Examples:

```text
60
150
600
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred coronaWidth behavior?

Evidence: [odf/stations/earth.odf:38](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/earth.odf:38>) — observed assignment and local context; [odf/stations/mars.odf:21](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/mars.odf:21>) — observed assignment and local context.

<a id="criticalbreaknodes"></a>

### `criticalbreaknodes`

**Spellings:** `criticalBreakNodes`.

**Inferred / medium confidence**; role: gameplay.

Names model nodes associated with critical breakup; the trigger and ordering are not documented.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 15 files (15 assignments).

Examples:

```text
"Neck"
"Boom"
"Left_Impulse" "Right_Impulse"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Determine which damage/break event uses the list and whether it hides, detaches or selects model-node subtrees.

Evidence: [odf/ships/K_Brel.odf:114](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_Brel.odf:114>) — observed assignment and local context; [odf/ships/K_D_7.odf:118](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_7.odf:118>) — observed assignment and local context.

<a id="damagedecal"></a>

### `damagedecal`

**Spellings:** `damageDecal`.

**Documented / high confidence**; role: presentation.

Names the decal applied to a damaged target surface.

Value form: quoted string. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
"damageMelt"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement damageDecal as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/special_weapons/spacemine.odf:17](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/spacemine.odf:17>) — assignment and nearby author comment.

<a id="damagemodelsizemodifier"></a>

### `damagemodelsizemodifier`

**Spellings:** `damageModelSizeModifier`.

**Inferred / medium confidence**; role: gameplay.

A ship damage-model size scalar; the source gives no reference size or trigger.

Value form: numeric literal. Units: unknown. Observed in 7 files (7 assignments).

Examples:

```text
3
5
6
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred damageModelSizeModifier behavior?

Evidence: [odf/ships/B_ent_cube.odf:143](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_ent_cube.odf:143>) — observed assignment and local context; [odf/ships/B_tng_cube.odf:120](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_tng_cube.odf:120>) — observed assignment and local context.

<a id="damageparticlesystems"></a>

### `damageparticlesystems`

**Spellings:** `damageParticleSystems`.

**Documented / high confidence**; role: presentation.

Lists particle systems played together when a craft suffers hull damage.

Value form: quoted string. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
"psgreenfire" "psdamaged_green_smoke" "psexplosion_major_borg"
"psfire_venting"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement damageParticleSystems as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/B_tng_multicube.odf:141](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_tng_multicube.odf:141>) — assignment and nearby author comment; [odf/ships/craft.odf:158](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:158>) — assignment and nearby author comment.

<a id="delaybetweenshots"></a>

### `delaybetweenshots`

**Spellings:** `delayBetweenShots`.

**Documented / high confidence**; role: gameplay.

Sets the delay between successive shots from the weapon.

Value form: numeric literal. Units: time units (seconds not expressly confirmed). Observed in 1 files (1 assignments).

Examples:

```text
0.3
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement delayBetweenShots as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Pulse/pulse.odf:54](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Pulse/pulse.odf:54>) — assignment and nearby author comment.

<a id="diffuse"></a>

### `diffuse`

**Spellings:** `diffuse`.

**Inferred / medium confidence**; role: presentation.

Station material diffuse colour as three numeric components; colour-space conversion is not documented.

Value form: three space-separated numeric components. Units: unknown. Observed in 17 files (17 assignments).

Examples:

```text
255 255 255
0 0 0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred diffuse behavior?

Evidence: [odf/stations/bajor.odf:12](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/bajor.odf:12>) — observed assignment and local context; [odf/stations/sun.odf:28](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/sun.odf:28>) — observed assignment and local context.

<a id="dispersalmindamagefraction"></a>

### `dispersalmindamagefraction`

**Spellings:** `dispersalMinDamageFraction`.

**Documented / high confidence**; role: gameplay.

Sets the fraction of normal damage retained at the weapon’s extreme range.

Value form: numeric literal. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
1.0
.3
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement dispersalMinDamageFraction as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/special_weapons/long_range_sensoro.odf:10](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/long_range_sensoro.odf:10>) — assignment and nearby author comment; [odf/weapons/Phasers/phasero.odf:12](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/phasero.odf:12>) — assignment and nearby author comment.

<a id="dispersalrangefraction"></a>

### `dispersalrangefraction`

**Spellings:** `dispersalRangeFraction`.

**Documented / high confidence**; role: gameplay.

Sets the fraction of maximum range where damage begins to fall off.

Value form: numeric literal. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
1.0
.3
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement dispersalRangeFraction as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/special_weapons/long_range_sensoro.odf:8](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/long_range_sensoro.odf:8>) — assignment and nearby author comment; [odf/weapons/Phasers/phasero.odf:10](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/phasero.odf:10>) — assignment and nearby author comment.

<a id="dockingareadimensions"></a>

### `dockingareadimensions`

**Spellings:** `dockingAreaDimensions`.

**Documented / high confidence**; role: gameplay.

Defines the dimensions of the inner shipyard docking region.

Value form: three comma-separated numeric components. Units: unknown. Observed in 10 files (10 assignments).

Examples:

```text
300.0, 160.0, 0.0
100.0, 70.0, 0.0
300.0, 140.0, 0.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement dockingAreaDimensions as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/stations/K_Tos_Shipyard.odf:20](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/K_Tos_Shipyard.odf:20>) — assignment and nearby author comment; [odf/stations/F_ENT_shipyard.odf:16](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/F_ENT_shipyard.odf:16>) — observed assignment and local context.

<a id="draghalflife"></a>

### `draghalflife`

**Spellings:** `dragHalfLife`.

**Inferred / medium confidence**; role: gameplay.

A torpedo acceleration tuning value; the comment says values scale from 1 slow to 0 fast but does not define an actual half-life.

Value form: numeric literal. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
0.5
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred dragHalfLife behavior?

Evidence: [odf/weapons/Photons/photono.odf:86](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/photono.odf:86>) — assignment and nearby author comment.

<a id="durationtofullintensity"></a>

### `durationtofullintensity`

**Spellings:** `durationToFullIntensity`.

**Documented / high confidence**; role: gameplay.

Sets how long the phaser visual takes to reach full intensity.

Value form: numeric literal. Units: time units (likely seconds; not explicit). Observed in 40 files (40 assignments).

Examples:

```text
1.25
9.0
1.9
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement durationToFullIntensity as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Phasers/B_phaser_ent_lrg_ord.odf:10](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/B_phaser_ent_lrg_ord.odf:10>) — assignment and nearby author comment; [odf/weapons/Phasers/B_phaser_ent_med_ord.odf:10](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/B_phaser_ent_med_ord.odf:10>) — assignment and nearby author comment.

<a id="durationtoreachfullradiussize"></a>

### `durationtoreachfullradiussize`

**Spellings:** `durationToReachFullRadiusSize`.

**Inferred / medium confidence**; role: gameplay.

Likely controls the time for beam radius to reach its final size, because it follows minRadiusOfBeam/maxRadiusOfBeam. However, its comment says full intensity, so radius-versus-intensity is unresolved.

Value form: numeric literal. Units: time units (likely seconds; not explicit). Observed in 43 files (43 assignments).

Examples:

```text
1.00
4.00
3.00
```

Uncertainty: Possible copied comment or a shared intensity/radius timer; the field name alone cannot resolve the distinction.

Next investigation: Trace whether this timer interpolates beam radius, intensity or both.

Evidence: [odf/weapons/Phasers/B_phaser_ent_lrg_ord.odf:19](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/B_phaser_ent_lrg_ord.odf:19>) — assignment and nearby author comment; [odf/weapons/Phasers/fed_phaser_tos_med_ord.odf:19](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/fed_phaser_tos_med_ord.odf:19>) — assignment and nearby author comment.

<a id="effectonhithull"></a>

### `effectonhithull`

**Spellings:** `effectOnHitHull`.

**Documented / high confidence**; role: presentation.

Names the impact effect played when this weapon hits a target’s hull.

Value form: quoted string. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
"psphaser_hit1"
"psimpact_torp_hull"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement effectOnHitHull as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Phasers/phasero.odf:69](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/phasero.odf:69>) — assignment and nearby author comment; [odf/weapons/Photons/photono.odf:79](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/photono.odf:79>) — assignment and nearby author comment.

<a id="effectonhithull-size"></a>

### `effectonhithull_size`

**Spellings:** `effectOnHitHull_size`.

**Inferred / medium confidence**; role: presentation.

Scales the impact effect played on a hull hit; the reference asset size is unknown.

Value form: numeric literal. Units: scale. Observed in 2 files (2 assignments).

Examples:

```text
6.0
10.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred effectOnHitHull_size behavior?

Evidence: [odf/weapons/Phasers/phasero.odf:71](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/phasero.odf:71>) — observed assignment and local context; [odf/weapons/Photons/photono.odf:81](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/photono.odf:81>) — observed assignment and local context.

<a id="effectonhitshields"></a>

### `effectonhitshields`

**Spellings:** `effectOnHitShields`.

**Inferred / medium confidence**; role: presentation.

Names the impact effect played when this weapon hits a target’s shields.

Value form: quoted string. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
"psphaser_hit2"
"psimpact_torp_shield"
```

Uncertainty: No command-specific ODF comment found; meaning follows placement and naming.

Next investigation: Which STL runtime parser or call site confirms the inferred effectOnHitShields behavior?

Evidence: [odf/weapons/Phasers/phasero.odf:70](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/phasero.odf:70>) — observed assignment and local context; [odf/weapons/Photons/photono.odf:80](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/photono.odf:80>) — observed assignment and local context.

<a id="effectonhitshields-size"></a>

### `effectonhitshields_size`

**Spellings:** `effectOnHitShields_size`.

**Inferred / medium confidence**; role: presentation.

Scales the impact effect played on a shields hit; the reference asset size is unknown.

Value form: numeric literal. Units: scale. Observed in 2 files (2 assignments).

Examples:

```text
6.0
8.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred effectOnHitShields_size behavior?

Evidence: [odf/weapons/Phasers/phasero.odf:72](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/phasero.odf:72>) — observed assignment and local context; [odf/weapons/Photons/photono.odf:82](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/photono.odf:82>) — observed assignment and local context.

<a id="energyeffectonhitcraft"></a>

### `energyeffectonhitcraft`

**Spellings:** `energyEffectOnHitCraft`.

**Documented / high confidence**; role: presentation.

Names the energy effect played when this weapon hits a target’s craft.

Value form: quoted string. Units: unknown. Observed in 5 files (5 assignments).

Examples:

```text
"x_explosion_03"
"xcritical_1"
"pssparks"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement energyEffectOnHitCraft as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/special_weapons/chainp.odf:72](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/chainp.odf:72>) — assignment and nearby author comment; [odf/special_weapons/spacemine.odf:10](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/spacemine.odf:10>) — assignment and nearby author comment.

<a id="energyeffectonhitcraft-size"></a>

### `energyeffectonhitcraft_size`

**Spellings:** `energyEffectOnHitCraft_Size`.

**Inferred / medium confidence**; role: presentation.

Scales the energy effect played on a craft hit; the reference asset size is unknown.

Value form: numeric literal. Units: scale. Observed in 5 files (5 assignments).

Examples:

```text
.75
1.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred energyEffectOnHitCraft_Size behavior?

Evidence: [odf/special_weapons/chainp.odf:73](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/chainp.odf:73>) — observed assignment and local context; [odf/weapons/Phasers/phasero.odf:74](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/phasero.odf:74>) — observed assignment and local context.

<a id="energyeffectonhitshields"></a>

### `energyeffectonhitshields`

**Spellings:** `energyEffectOnHitShields`.

**Documented / high confidence**; role: presentation.

Names the energy effect played when this weapon hits a target’s shields.

Value form: quoted string. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
"photon_shieldhit_1"
"xcritical_8"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement energyEffectOnHitShields as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/special_weapons/chainp.odf:78](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/chainp.odf:78>) — assignment and nearby author comment; [odf/special_weapons/long_range_sensoro.odf:75](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/long_range_sensoro.odf:75>) — assignment and nearby author comment.

<a id="energyeffectonhitshields-size"></a>

### `energyeffectonhitshields_size`

**Spellings:** `energyEffectOnHitShields_Size`.

**Inferred / medium confidence**; role: presentation.

Scales the energy effect played on a shields hit; the reference asset size is unknown.

Value form: numeric literal. Units: scale. Observed in 3 files (3 assignments).

Examples:

```text
1.0
1.25
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred energyEffectOnHitShields_Size behavior?

Evidence: [odf/special_weapons/chainp.odf:79](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/chainp.odf:79>) — observed assignment and local context; [odf/special_weapons/long_range_sensoro.odf:76](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/long_range_sensoro.odf:76>) — observed assignment and local context.

<a id="energyperphoton"></a>

### `energyperphoton`

**Spellings:** `energyPerPhoton`.

**Documented / high confidence**; role: gameplay.

Sets energy expended when firing a photon torpedo.

Value form: numeric literal. Units: unknown. Observed in 105 files (105 assignments).

Examples:

```text
0.1
0.2
0.3
```

Uncertainty: The comments establish expenditure, but do not say whether the values are absolute energy or a fraction of an energy pool.

Next investigation: Recover pool units/capacity, update time base, energy-distribution influence and the exact debit/replenishment formula.

Evidence: [odf/ships/F_Akira.odf:18](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akira.odf:18>) — assignment and nearby author comment; [odf/ships/R_Dderidex.odf:28](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/R_Dderidex.odf:28>) — assignment and nearby author comment.

<a id="energyperpulsephaser"></a>

### `energyperpulsephaser`

**Spellings:** `energyPerPulsePhaser`.

**Documented / high confidence**; role: gameplay.

Sets energy expended per pulse-phaser shot.

Value form: numeric literal. Units: unknown. Observed in 89 files (91 assignments).

Examples:

```text
0.01
0.08
0.07
```

Uncertainty: The comments establish expenditure, but do not say whether the values are absolute energy or a fraction of an energy pool.

Next investigation: Recover pool units/capacity, update time base, energy-distribution influence and the exact debit/replenishment formula.

Evidence: [odf/ships/F_Defiant.odf:21](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Defiant.odf:21>) — assignment and nearby author comment; [odf/ships/K_Brel.odf:17](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_Brel.odf:17>) — assignment and nearby author comment.

<a id="enginebasefreq"></a>

### `enginebasefreq`

**Spellings:** `EngineBaseFreq`.

**Inferred / medium confidence**; role: gameplay.

Sets a baseline frequency used by the engine sound configuration; audible mapping is not documented.

Value form: numeric literal. Units: unknown. Observed in 7 files (7 assignments).

Examples:

```text
22050.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred EngineBaseFreq behavior?

Evidence: [odf/ships/class_scout.odf:88](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_scout.odf:88>) — assignment and nearby author comment.

<a id="enginebreaknodes"></a>

### `enginebreaknodes`

**Spellings:** `engineBreakNodes`.

**Inferred / medium confidence**; role: asset.

Names model nodes broken away with engine subsystem destruction.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 101 files (102 assignments).

Examples:

```text
"Hull_B_0" "Hull_C_0"
"break01" "break02"
"Hull_B_0"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Determine which damage/break event uses the list and whether it hides, detaches or selects model-node subtrees.

Evidence: [odf/ships/F_Akira.odf:133](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akira.odf:133>) — assignment and nearby author comment; [odf/ships/F_Akula.odf:154](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akula.odf:154>) — assignment and nearby author comment.

<a id="enginedestructioneffect"></a>

### `enginedestructioneffect`

**Spellings:** `EngineDestructionEffect`.

**Inferred / medium confidence**; role: presentation.

Names the visual effect played when the engine subsystem is destroyed.

Value form: quoted string. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"psexplosion_engines"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred EngineDestructionEffect behavior?

Evidence: [odf/ships/craft.odf:165](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:165>) — observed assignment and local context.

<a id="enginedestructionhardpoints"></a>

### `enginedestructionhardpoints`

**Spellings:** `EngineDestructionHardpoints`.

**Inferred / medium confidence**; role: asset.

Lists model hardpoints where engine subsystem destruction effects appear.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 119 files (120 assignments).

Examples:

```text
"hp01"
"hp31"
"hp17"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred EngineDestructionHardpoints behavior?

Evidence: [odf/ships/B_tng_obelisk.odf:116](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_tng_obelisk.odf:116>) — assignment and nearby author comment; [odf/ships/F_Akira.odf:112](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akira.odf:112>) — assignment and nearby author comment.

<a id="enginemaxfreq"></a>

### `enginemaxfreq`

**Spellings:** `EngineMaxFreq`.

**Inferred / medium confidence**; role: gameplay.

Sets a maximum frequency used by the engine sound configuration; audible mapping is not documented.

Value form: numeric literal. Units: unknown. Observed in 7 files (7 assignments).

Examples:

```text
66150.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred EngineMaxFreq behavior?

Evidence: [odf/ships/class_scout.odf:89](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_scout.odf:89>) — observed assignment and local context.

<a id="engineofffreq"></a>

### `engineofffreq`

**Spellings:** `EngineOffFreq`.

**Inferred / medium confidence**; role: gameplay.

Sets a frequency for the engine-off sound state; audible mapping is not documented.

Value form: numeric literal. Units: unknown. Observed in 7 files (7 assignments).

Examples:

```text
22050.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred EngineOffFreq behavior?

Evidence: [odf/ships/class_scout.odf:90](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_scout.odf:90>) — observed assignment and local context.

<a id="engineramptime"></a>

### `engineramptime`

**Spellings:** `EngineRampTime`.

**Inferred / medium confidence**; role: gameplay.

Sets a ramp duration for engine sound frequency or level; the source does not say which.

Value form: numeric literal. Units: unknown. Observed in 7 files (7 assignments).

Examples:

```text
2.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred EngineRampTime behavior?

Evidence: [odf/ships/class_scout.odf:91](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_scout.odf:91>) — observed assignment and local context.

<a id="enginesmaxlevel"></a>

### `enginesmaxlevel`

**Spellings:** `enginesMaxLevel`.

**Inferred / medium confidence**; role: gameplay.

Sets the maximum engine upgrade level used for scaling engine upgrades.

Value form: numeric literal. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
2000
```

Uncertainty: No command-specific ODF comment found; meaning follows placement and naming.

Next investigation: Which STL runtime parser or call site confirms the inferred enginesMaxLevel behavior?

Evidence: [odf/ships/craft.odf:73](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:73>) — observed assignment and local context.

<a id="enginesminlevel"></a>

### `enginesminlevel`

**Spellings:** `enginesMinLevel`.

**Documented / high confidence**; role: gameplay.

Sets the minimum engine upgrade level used for scaling engine upgrades.

Value form: numeric literal. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
100
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement enginesMinLevel as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/craft.odf:72](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:72>) — assignment and nearby author comment.

<a id="enginesound"></a>

### `enginesound`

**Spellings:** `engineSound`.

**Inferred / medium confidence**; role: presentation.

Names the ship engine sound asset; playback conditions are not specified.

Value form: quoted string. Units: unknown. Observed in 92 files (92 assignments).

Examples:

```text
"ST_ENG_BOR_Artillery"
"ST_ENG_BOR_Battleship"
"ST_ENG_BOR_Cruiser"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred engineSound behavior?

Evidence: [odf/ships/B_class_cruiser.odf:6](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_class_cruiser.odf:6>) — assignment and nearby author comment; [odf/ships/B_class_science.odf:6](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_class_science.odf:6>) — assignment and nearby author comment.

<a id="enginetargethardpoints"></a>

### `enginetargethardpoints`

**Spellings:** `EngineTargetHardpoints`.

**Inferred / medium confidence**; role: asset.

Lists model hardpoints used when attacking or targeting the engine subsystem.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 121 files (121 assignments).

Examples:

```text
"hp01"
"hp31"
"hp17"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Determine how hardpoint lists select aim points and handle missing names, visibility changes and destroyed subsystems.

Evidence: [odf/ships/B_tng_cube.odf:108](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_tng_cube.odf:108>) — assignment and nearby author comment; [odf/ships/B_tng_cube_tact.odf:130](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_tng_cube_tact.odf:130>) — assignment and nearby author comment.

<a id="ent"></a>

### `ent`

**Spellings:** `ent`.

**Documented / high confidence**; role: ui.

Lists Enterprise-era ship types available to choose in Skirmish/MP for the faction. The source calls these selectable ship types, rather than a preselected spawn fleet.

Value form: space-separated quoted ODF basenames. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"B_ent_rectangle" "B_ent_sphere" "B_ent_obelisk"
"B_ent_cube" "B_ent_pyramid" "B_ent_diamond"
"F_Discovery" "F_Saladin" "F_NX_01"
"F_Yorktown" "F_Poseidon" "F_Daedalus" "F_Fortunate"
"K_K_5" "K_Raptor" "K_Stinger"
"K_Victorious" "K_Predator" "K_X_1"
```

Uncertainty: Selection UI filtering, duplicates, starting availability and era lookup remain unverified.

Next investigation: Trace era/faction ship selection and distinguish selection lists from shipN spawn fleets.

Evidence: [odf/default fleets/borg_fleet.odf:7](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/default fleets/borg_fleet.odf:7>) — assignment and nearby author comment; [odf/default fleets/federation_fleet.odf:7](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/default fleets/federation_fleet.odf:7>) — assignment and nearby author comment.

<a id="entrywounddecal"></a>

### `entrywounddecal`

**Spellings:** `entryWoundDecal`.

**Inferred / medium confidence**; role: presentation.

Names a decal for the projectile entry side of a hit; placement is not documented.

Value form: quoted string. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
"photon_hit"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred entryWoundDecal behavior?

Evidence: [odf/special_weapons/chainp.odf:83](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/chainp.odf:83>) — observed assignment and local context.

<a id="escortme"></a>

### `escortme`

**Spellings:** `escortMe`.

**Documented / high confidence**; role: gameplay.

The flag tells AI to escort this ship.

Value form: numeric literal. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
1
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement escortMe as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/f_civilian.odf:64](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/f_civilian.odf:64>) — assignment and nearby author comment.

<a id="exitwounddecal"></a>

### `exitwounddecal`

**Spellings:** `exitWoundDecal`.

**Inferred / medium confidence**; role: presentation.

Names a decal for the projectile exit side of a hit; placement is not documented.

Value form: quoted string. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
"photon_hit"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred exitWoundDecal behavior?

Evidence: [odf/special_weapons/chainp.odf:84](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/chainp.odf:84>) — observed assignment and local context.

<a id="explosionname"></a>

### `explosionname`

**Spellings:** `explosionName`.

**Documented / high confidence**; role: gameplay.

Names the explosion ODF class spawned when the ordnance detonates.

Value form: quoted string. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"xartillery"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement explosionName as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Photons/artillery_ord.odf:52](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/artillery_ord.odf:52>) — assignment and nearby author comment.

<a id="explosionparticlesystemssecondary"></a>

### `explosionparticlesystemssecondary`

**Spellings:** `explosionParticleSystemsSecondary`.

**Documented / high confidence**; role: presentation.

Lists secondary particle explosions at chunk-split points, said to occur 1.2 seconds before the major explosion.

Value form: quoted string. Units: unknown. Observed in 29 files (29 assignments).

Examples:

```text
"psexplosion_minor_borg"
"psexplosion_blast_borg1"
"psexplosion_weapons"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement explosionParticleSystemsSecondary as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/B_class_cruiser.odf:29](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_class_cruiser.odf:29>) — assignment and nearby author comment; [odf/ships/B_class_scout.odf:28](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_class_scout.odf:28>) — assignment and nearby author comment.

<a id="explosiveimpulse"></a>

### `explosiveimpulse`

**Spellings:** `explosiveImpulse`.

**Documented / high confidence**; role: gameplay.

Sets an absolute impulse applied to a hit ship; the author warns this may later change to damage-relative scaling.

Value form: numeric literal with optional trailing semicolon. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
1000;
300
2000;
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement explosiveImpulse as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Phasers/phasero.odf:61](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/phasero.odf:61>) — assignment and nearby author comment; [odf/weapons/Photons/photono.odf:63](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/photono.odf:63>) — assignment and nearby author comment.

<a id="explosivelinearscale"></a>

### `explosivelinearscale`

**Spellings:** `explosiveLinearScale`.

**Documented / high confidence**; role: gameplay.

Sets the fraction of explosive push, separate from rotational twist.

Value form: numeric literal with optional trailing semicolon. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
.25
1.0
1.0;
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement explosiveLinearScale as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/special_weapons/long_range_sensoro.odf:66](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/long_range_sensoro.odf:66>) — assignment and nearby author comment; [odf/weapons/Phasers/phasero.odf:66](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/phasero.odf:66>) — assignment and nearby author comment.

<a id="explosivetime"></a>

### `explosivetime`

**Spellings:** `explosiveTime`.

**Documented / high confidence**; role: gameplay.

Sets the seconds over which a triangular explosive impulse is applied.

Value form: numeric literal with optional trailing semicolon. Units: seconds. Observed in 4 files (4 assignments).

Examples:

```text
0.25;
0.5
0.4;
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement explosiveTime as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/special_weapons/long_range_sensoro.odf:63](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/long_range_sensoro.odf:63>) — assignment and nearby author comment; [odf/weapons/Phasers/phasero.odf:63](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/phasero.odf:63>) — assignment and nearby author comment.

<a id="federationentbuildlist"></a>

### `federationentbuildlist`

**Spellings:** `federationEntBuildList`.

**Documented / high confidence**; role: gameplay.

Defines the Federation ENT construction list for the owning planet. Each pair is an ODF name and the required planet upgrade level, numbered from zero; the trailing 0/1 is a level requirement, not an unexplained Boolean.

Value form: repeated quoted pairs: "ODF name" "required planet upgrade level". Units: zero-based planet upgrade level. Observed in 6 files (6 assignments).

Examples:

```text
"f_ent_shipyard" "1"
"f_ent_turret"   "0"
"f_ent_engineering" "1"
"f_ent_shipyard" "0"
"f_ent_turret" "0"
"f_ent_sensor" "1"
"f_ent_engineering" "0"
"f_ent_shipyard" "0"
"f_ent_turret" "0"
"f_ent_sensor" "0"
"f_annex" "0"
"f_trade" "0"
```

Uncertainty: Selection by owner/era and inheritance are not traced; number parsing, duplicate items and invalid level handling remain unknown.

Next investigation: Recover owner/era selection, planet upgrade gating, duplicate handling and validation of required levels.

Evidence: [odf/stations/classj.odf:44](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:44>) — Shared build-list header explicitly defines ODF-name/required-planet-level pairs from zero..

<a id="federationentcommandpost"></a>

### `federationentcommandpost`

**Spellings:** `federationEntCommandPost`.

**Documented / high confidence**; role: gameplay.

Names the Federation ENT command-post ODF associated with a planet, selected by race and era according to the planet header.

Value form: quoted ODF basename. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"f_ent_command"
```

Uncertainty: Creation time, attachment/placement, capture replacement and absent-definition fallbacks are unknown.

Next investigation: Recover when the planet creates or replaces its race/era command post.

Evidence: [odf/stations/planet.odf:45](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:45>) — Header explicitly describes command-post variants by race and era..

<a id="federationtngbuildlist"></a>

### `federationtngbuildlist`

**Spellings:** `federationTngBuildList`.

**Documented / high confidence**; role: gameplay.

Defines the Federation TNG construction list for the owning planet. Each pair is an ODF name and the required planet upgrade level, numbered from zero; the trailing 0/1 is a level requirement, not an unexplained Boolean.

Value form: repeated quoted pairs: "ODF name" "required planet upgrade level". Units: zero-based planet upgrade level. Observed in 6 files (6 assignments).

Examples:

```text
"f_tng_shipyard" "1"
"f_tng_turret" "0"
"f_tng_engineering" "1"
"f_tng_shipyard" "0"
"f_tng_turret" "0"
"f_tng_sensor" "1"
"f_soliton" "1"
"f_tng_engineering" "0"
"f_tng_shipyard" "0"
"f_tng_turret" "0"
"f_tng_sensor" "0"
"f_annex" "0"
"f_trade" "0"
"f_soliton" "0"
```

Uncertainty: Selection by owner/era and inheritance are not traced; number parsing, duplicate items and invalid level handling remain unknown.

Next investigation: Recover owner/era selection, planet upgrade gating, duplicate handling and validation of required levels.

Evidence: [odf/stations/classj.odf:106](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:106>) — Shared build-list header explicitly defines ODF-name/required-planet-level pairs from zero..

<a id="federationtngcommandpost"></a>

### `federationtngcommandpost`

**Spellings:** `federationTngCommandPost`.

**Documented / high confidence**; role: gameplay.

Names the Federation TNG command-post ODF associated with a planet, selected by race and era according to the planet header.

Value form: quoted ODF basename. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"f_tng_command"
```

Uncertainty: Creation time, attachment/placement, capture replacement and absent-definition fallbacks are unknown.

Next investigation: Recover when the planet creates or replaces its race/era command post.

Evidence: [odf/stations/planet.odf:59](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:59>) — Header explicitly describes command-post variants by race and era..

<a id="federationtosbuildlist"></a>

### `federationtosbuildlist`

**Spellings:** `federationTosBuildList`.

**Documented / high confidence**; role: gameplay.

Defines the Federation TOS construction list for the owning planet. Each pair is an ODF name and the required planet upgrade level, numbered from zero; the trailing 0/1 is a level requirement, not an unexplained Boolean.

Value form: repeated quoted pairs: "ODF name" "required planet upgrade level". Units: zero-based planet upgrade level. Observed in 6 files (6 assignments).

Examples:

```text
"f_tos_shipyard" "1"
"f_tos_turret" "0"
"f_tos_engineering" "1"
"f_tos_shipyard" "0"
"f_tos_turret" "0"
"f_tos_sensor" "1"
"f_soliton" "1"
"f_tos_engineering" "0"
"f_tos_shipyard" "0"
"f_tos_turret" "0"
"f_tos_sensor" "0"
"f_annex" "0"
"f_trade" "0"
"f_soliton" "0"
```

Uncertainty: Selection by owner/era and inheritance are not traced; number parsing, duplicate items and invalid level handling remain unknown.

Next investigation: Recover owner/era selection, planet upgrade gating, duplicate handling and validation of required levels.

Evidence: [odf/stations/classj.odf:73](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:73>) — Shared build-list header explicitly defines ODF-name/required-planet-level pairs from zero..

<a id="federationtoscommandpost"></a>

### `federationtoscommandpost`

**Spellings:** `federationTosCommandPost`.

**Documented / high confidence**; role: gameplay.

Names the Federation TOS command-post ODF associated with a planet, selected by race and era according to the planet header.

Value form: quoted ODF basename. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"f_tos_command"
```

Uncertainty: Creation time, attachment/placement, capture replacement and absent-definition fallbacks are unknown.

Next investigation: Recover when the planet creates or replaces its race/era command post.

Evidence: [odf/stations/planet.odf:52](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:52>) — Header explicitly describes command-post variants by race and era..

<a id="fireallbanks"></a>

### `fireallbanks`

**Spellings:** `fireAllBanks`.

**Documented / high confidence**; role: gameplay.

Makes the weapon fire from all available banks when enabled.

Value form: numeric literal. Units: unknown. Observed in 39 files (40 assignments).

Examples:

```text
0
1
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement fireAllBanks as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Pulse/rom_base_pulse.odf:30](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Pulse/rom_base_pulse.odf:30>) — assignment and nearby author comment; [odf/weapons/Pulse/rom_ent_pulse.odf:30](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Pulse/rom_ent_pulse.odf:30>) — assignment and nearby author comment.

<a id="firesoundgroup"></a>

### `firesoundgroup`

**Spellings:** `fireSoundGroup`.

**Documented / high confidence**; role: presentation.

Names the sound group played when this weapon fires.

Value form: quoted string. Units: unknown. Observed in 34 files (34 assignments).

Examples:

```text
"ST_GRP_BOR_Phaser_Blast_1"
"ST_GRP_BOR_Phaser_Blast_2"
"ST_GRP_BOR_Phaser_Blast_3"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement fireSoundGroup as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Pulse/Fed_tng_defiant_pulse.odf:54](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Pulse/Fed_tng_defiant_pulse.odf:54>) — assignment and nearby author comment; [odf/weapons/Pulse/fed_pulse_main_lvl1.odf:4](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Pulse/fed_pulse_main_lvl1.odf:4>) — assignment and nearby author comment.

<a id="flarecolorfullintensity"></a>

### `flarecolorfullintensity`

**Spellings:** `flareColorFullIntensity`.

**Documented / high confidence**; role: presentation.

Sets the flare colour reached when the phaser reaches full intensity.

Value form: three space-separated numeric components. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
1.0 1.0 1.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement flareColorFullIntensity as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Phasers/f_constit_re_phaser_ord.odf:8](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/f_constit_re_phaser_ord.odf:8>) — assignment and nearby author comment.

<a id="flarethreshold"></a>

### `flarethreshold`

**Spellings:** `flareThreshold`.

**Documented / high confidence**; role: presentation.

Sets the phaser intensity threshold at which the flare switches to burst colour.

Value form: numeric literal. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
0.80
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement flareThreshold as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Phasers/f_constit_re_phaser_ord.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/f_constit_re_phaser_ord.odf:5>) — assignment and nearby author comment.

<a id="frontenginehitchance"></a>

### `frontenginehitchance`

**Spellings:** `frontEngineHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the front facing hit-chance weight for the engine subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:84](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:84>) — observed assignment and local context; [odf/stations/station.odf:65](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:65>) — observed assignment and local context.

<a id="fronthullhitchance"></a>

### `fronthullhitchance`

**Spellings:** `frontHullHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the front facing hit-chance weight for the hull subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
100
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:82](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:82>) — assignment and nearby author comment; [odf/stations/station.odf:63](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:63>) — assignment and nearby author comment.

<a id="frontsensorhitchance"></a>

### `frontsensorhitchance`

**Spellings:** `frontSensorHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the front facing hit-chance weight for the sensor subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:85](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:85>) — observed assignment and local context; [odf/stations/station.odf:66](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:66>) — observed assignment and local context.

<a id="frontshieldhitchance"></a>

### `frontshieldhitchance`

**Spellings:** `frontShieldHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the front facing hit-chance weight for the shield subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:83](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:83>) — observed assignment and local context; [odf/stations/station.odf:64](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:64>) — observed assignment and local context.

<a id="frontweaponhitchance"></a>

### `frontweaponhitchance`

**Spellings:** `frontWeaponHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the front facing hit-chance weight for the weapon subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:86](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:86>) — observed assignment and local context; [odf/stations/station.odf:67](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:67>) — observed assignment and local context.

<a id="givescommandpoints"></a>

### `givescommandpoints`

**Spellings:** `GivesCommandPoints`.

**Inferred / medium confidence**; role: gameplay.

Flags a planet/station as a command-point source; the source does not explain the production formula.

Value form: numeric literal. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
1
```

Uncertainty: Point-system semantics and Fleet Ops runtime support are unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred GivesCommandPoints behavior?

Evidence: [odf/stations/planet.odf:23](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:23>) — observed assignment and local context.

<a id="hascaptain"></a>

### `hascaptain`

**Spellings:** `hasCaptain`.

**Documented / high confidence**; role: gameplay.

Marks a normal craft as having a captain; captain selection rules are not given.

Value form: numeric literal. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
1
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement hasCaptain as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/craft.odf:37](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:37>) — assignment and nearby author comment.

<a id="hashullplating"></a>

### `hashullplating`

**Spellings:** `hasHUllplating`, `hasHullPlating`, `hasHullplating`.

**Inferred / medium confidence**; role: gameplay.

Enables hull plating for the ship; see the separately documented damage-reduction setting.

Value form: numeric literal. Units: unknown. Observed in 39 files (39 assignments).

Examples:

```text
1
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred hasHUllplating behavior?

Evidence: [odf/ships/F_NX_01.odf:28](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_NX_01.odf:28>) — observed assignment and local context.

<a id="haspoints"></a>

### `haspoints`

**Spellings:** `HasPoints`.

**Inferred / medium confidence**; role: gameplay.

Enables the planet/station point resource or point generation; the type of points is unclear from this assignment alone.

Value form: numeric literal. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
1
```

Uncertainty: Point-system semantics and Fleet Ops runtime support are unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred HasPoints behavior?

Evidence: [odf/stations/planet.odf:22](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:22>) — observed assignment and local context.

<a id="hastargetingboost"></a>

### `hastargetingboost`

**Spellings:** `hasTargetingBoost`.

**Documented / high confidence**; role: gameplay.

Enables a craft targeting-boost system.

Value form: numeric literal. Units: unknown. Observed in 23 files (24 assignments).

Examples:

```text
1
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement hasTargetingBoost as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/F_Sovereign.odf:36](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Sovereign.odf:36>) — assignment and nearby author comment; [odf/ships/craft.odf:8](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:8>) — assignment and nearby author comment.

<a id="hitsoundgroup"></a>

### `hitsoundgroup`

**Spellings:** `hitSoundGroup`.

**Documented / high confidence**; role: presentation.

Names the sound group played when ordnance hits its target.

Value form: quoted string. Units: unknown. Observed in 24 files (24 assignments).

Examples:

```text
"ST_GRP_EXP_Ship_Small"
"ST_GRP_BORKLI_Photon_Ship"
"ST_GRP_FEDROM_Photon_Ship"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement hitSoundGroup as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/special_weapons/chainp.odf:87](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/chainp.odf:87>) — assignment and nearby author comment; [odf/weapons/Photons/tractor_bm.odf:91](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/tractor_bm.odf:91>) — assignment and nearby author comment.

<a id="hotkeyname"></a>

### `hotkeyname`

**Spellings:** `hotkeyName`.

**Documented / high confidence**; role: ui.

Names the command hotkey binding identifier, such as attack or move.

Value form: quoted command identifier. Units: unknown. Observed in 13 files (13 assignments).

Examples:

```text
"attack"
"cancel"
"decommission"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement hotkeyName as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/commands/decommission.odf:13](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/commands/decommission.odf:13>) — assignment and nearby author comment; [odf/commands/move.odf:12](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/commands/move.odf:12>) — assignment and nearby author comment.

<a id="impactsprite"></a>

### `impactsprite`

**Spellings:** `impactSprite`.

**Inferred / medium confidence**; role: presentation.

Names the sprite shown at the weapon impact; exact hit surface mapping is unverified.

Value form: quoted string. Units: unknown. Observed in 50 files (50 assignments).

Examples:

```text
"r_tng_beam_impact"
"f_ent_beam_impact"
"f_tng_beam_impact"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred impactSprite behavior?

Evidence: [odf/weapons/Phasers/B_phaser_ent_lrg_ord.odf:21](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/B_phaser_ent_lrg_ord.odf:21>) — observed assignment and local context; [odf/weapons/Phasers/K_phaser_ent_lrg_ord.odf:25](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/K_phaser_ent_lrg_ord.odf:25>) — observed assignment and local context.

<a id="insidehulldamagematerial"></a>

### `insidehulldamagematerial`

**Spellings:** `insideHullDamageMaterial`.

**Inferred / medium confidence**; role: presentation.

Names the material used for visible damage inside the hull; trigger and model mapping are unclear.

Value form: quoted string. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"damageInterior"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred insideHullDamageMaterial behavior?

Evidence: [odf/ships/craft.odf:175](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:175>) — observed assignment and local context.

<a id="islensflaresource"></a>

### `islensflaresource`

**Spellings:** `isLensFlareSource`.

**Documented / high confidence**; role: gameplay.

Marks the object as a lens-flare source.

Value form: numeric literal. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
1
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement isLensFlareSource as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/stations/sun.odf:39](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/sun.odf:39>) — assignment and nearby author comment.

<a id="klingonentbuildlist"></a>

### `klingonentbuildlist`

**Spellings:** `klingonEntBuildList`.

**Documented / high confidence**; role: gameplay.

Defines the Klingon ENT construction list for the owning planet. Each pair is an ODF name and the required planet upgrade level, numbered from zero; the trailing 0/1 is a level requirement, not an unexplained Boolean.

Value form: repeated quoted pairs: "ODF name" "required planet upgrade level". Units: zero-based planet upgrade level. Observed in 6 files (6 assignments).

Examples:

```text
"k_ent_shipyard" "1"
"k_ent_turret" "0"
"k_ent_engineering" "1"
"k_ent_shipyard" "0"
"k_ent_turret" "0"
"k_ent_sensor" "1"
"k_ent_engineering" "0"
"k_ent_shipyard" "0"
"k_ent_turret" "0"
"k_ent_sensor" "0"
"k_shrine" "0"
"k_big_gun" "0"
```

Uncertainty: Selection by owner/era and inheritance are not traced; number parsing, duplicate items and invalid level handling remain unknown.

Next investigation: Recover owner/era selection, planet upgrade gating, duplicate handling and validation of required levels.

Evidence: [odf/stations/classj.odf:51](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:51>) — Shared build-list header explicitly defines ODF-name/required-planet-level pairs from zero..

<a id="klingonentcommandpost"></a>

### `klingonentcommandpost`

**Spellings:** `klingonEntCommandPost`.

**Documented / high confidence**; role: gameplay.

Names the Klingon ENT command-post ODF associated with a planet, selected by race and era according to the planet header.

Value form: quoted ODF basename. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"k_ent_command"
```

Uncertainty: Creation time, attachment/placement, capture replacement and absent-definition fallbacks are unknown.

Next investigation: Recover when the planet creates or replaces its race/era command post.

Evidence: [odf/stations/planet.odf:46](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:46>) — Header explicitly describes command-post variants by race and era..

<a id="klingontngbuildlist"></a>

### `klingontngbuildlist`

**Spellings:** `klingonTngBuildList`.

**Documented / high confidence**; role: gameplay.

Defines the Klingon TNG construction list for the owning planet. Each pair is an ODF name and the required planet upgrade level, numbered from zero; the trailing 0/1 is a level requirement, not an unexplained Boolean.

Value form: repeated quoted pairs: "ODF name" "required planet upgrade level". Units: zero-based planet upgrade level. Observed in 6 files (6 assignments).

Examples:

```text
"k_tng_shipyard" "1"
"k_tng_turret" "0"
"k_tng_engineering" "1"
"k_tng_shipyard" "0"
"k_tng_turret" "0"
"k_tng_sensor" "1"
"k_soliton" "1"
"k_tng_engineering" "0"
"k_tng_shipyard" "0"
"k_tng_turret" "0"
"k_tng_sensor" "0"
"k_shrine" "0"
"k_big_gun" "0"
"k_soliton" "0"
```

Uncertainty: Selection by owner/era and inheritance are not traced; number parsing, duplicate items and invalid level handling remain unknown.

Next investigation: Recover owner/era selection, planet upgrade gating, duplicate handling and validation of required levels.

Evidence: [odf/stations/classj.odf:114](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:114>) — Shared build-list header explicitly defines ODF-name/required-planet-level pairs from zero..

<a id="klingontngcommandpost"></a>

### `klingontngcommandpost`

**Spellings:** `klingonTngCommandPost`.

**Documented / high confidence**; role: gameplay.

Names the Klingon TNG command-post ODF associated with a planet, selected by race and era according to the planet header.

Value form: quoted ODF basename. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"k_tng_command"
```

Uncertainty: Creation time, attachment/placement, capture replacement and absent-definition fallbacks are unknown.

Next investigation: Recover when the planet creates or replaces its race/era command post.

Evidence: [odf/stations/planet.odf:60](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:60>) — Header explicitly describes command-post variants by race and era..

<a id="klingontosbuildlist"></a>

### `klingontosbuildlist`

**Spellings:** `klingonTosBuildList`.

**Documented / high confidence**; role: gameplay.

Defines the Klingon TOS construction list for the owning planet. Each pair is an ODF name and the required planet upgrade level, numbered from zero; the trailing 0/1 is a level requirement, not an unexplained Boolean.

Value form: repeated quoted pairs: "ODF name" "required planet upgrade level". Units: zero-based planet upgrade level. Observed in 6 files (6 assignments).

Examples:

```text
"k_tos_shipyard" "1"
"k_tos_turret" "0"
"k_tos_engineering" "1"
"k_tos_shipyard" "0"
"k_tos_turret" "0"
"k_tos_sensor" "1"
"k_soliton" "1"
"k_tos_engineering" "0"
"k_tos_shipyard" "0"
"k_tos_turret" "0"
"k_tos_sensor" "0"
"k_shrine" "0"
"k_big_gun" "0"
"k_soliton" "0"
```

Uncertainty: Selection by owner/era and inheritance are not traced; number parsing, duplicate items and invalid level handling remain unknown.

Next investigation: Recover owner/era selection, planet upgrade gating, duplicate handling and validation of required levels.

Evidence: [odf/stations/classj.odf:81](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:81>) — Shared build-list header explicitly defines ODF-name/required-planet-level pairs from zero..

<a id="klingontoscommandpost"></a>

### `klingontoscommandpost`

**Spellings:** `klingonTosCommandPost`.

**Documented / high confidence**; role: gameplay.

Names the Klingon TOS command-post ODF associated with a planet, selected by race and era according to the planet header.

Value form: quoted ODF basename. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"k_tos_command"
```

Uncertainty: Creation time, attachment/placement, capture replacement and absent-definition fallbacks are unknown.

Next investigation: Recover when the planet creates or replaces its race/era command post.

Evidence: [odf/stations/planet.odf:53](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:53>) — Header explicitly describes command-post variants by race and era..

<a id="leftenginehitchance"></a>

### `leftenginehitchance`

**Spellings:** `leftEngineHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the left facing hit-chance weight for the engine subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:96](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:96>) — observed assignment and local context; [odf/stations/station.odf:77](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:77>) — observed assignment and local context.

<a id="lefthullhitchance"></a>

### `lefthullhitchance`

**Spellings:** `leftHullHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the left facing hit-chance weight for the hull subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
100
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:94](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:94>) — observed assignment and local context; [odf/stations/station.odf:75](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:75>) — observed assignment and local context.

<a id="leftsensorhitchance"></a>

### `leftsensorhitchance`

**Spellings:** `leftSensorHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the left facing hit-chance weight for the sensor subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:97](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:97>) — observed assignment and local context; [odf/stations/station.odf:78](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:78>) — observed assignment and local context.

<a id="leftshieldhitchance"></a>

### `leftshieldhitchance`

**Spellings:** `leftShieldHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the left facing hit-chance weight for the shield subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:95](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:95>) — observed assignment and local context; [odf/stations/station.odf:76](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:76>) — observed assignment and local context.

<a id="leftweaponhitchance"></a>

### `leftweaponhitchance`

**Spellings:** `leftWeaponHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the left facing hit-chance weight for the weapon subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:98](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:98>) — observed assignment and local context; [odf/stations/station.odf:79](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:79>) — observed assignment and local context.

<a id="logofilenames"></a>

### `logofilenames`

**Spellings:** `logoFileNames`.

**Inferred / medium confidence**; role: asset.

Lists logo texture names associated with the ship; exact placement and selection are unclear.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 50 files (50 assignments).

Examples:

```text
"F_Akira_Logo_1" "F_Akira_Logo_2" "F_Akira_Logo_3" "F_Akira_Logo_4"
"F_Akula_Logo_1" "F_Akula_Logo_2" "F_Akula_Logo_3" "F_Akula_Logo_4"
"F_Ambassador_Logo_1" "F_Ambassador_Logo_2" "F_Ambassador_Logo_3" "F_Ambassador_Logo_4" "F_Ambassador_Logo_5"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred logoFileNames behavior?

Evidence: [odf/ships/F_Akira.odf:55](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akira.odf:55>) — observed assignment and local context; [odf/ships/F_Akula.odf:80](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akula.odf:80>) — observed assignment and local context.

<a id="longrangesensorrangemultiplier"></a>

### `longrangesensorrangemultiplier`

**Spellings:** `longRangeSensorRangeMultiplier`.

**Inferred / medium confidence**; role: gameplay.

Multiplies the ship’s sensor range during long-range sensor boost; triggering and base range are not stated.

Value form: numeric literal. Units: unknown. Observed in 7 files (7 assignments).

Examples:

```text
1.7
1.3
1.5
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred longRangeSensorRangeMultiplier behavior?

Evidence: [odf/ships/class_cruiser.odf:6](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_cruiser.odf:6>) — assignment and nearby author comment; [odf/ships/class_science.odf:6](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_science.odf:6>) — assignment and nearby author comment.

<a id="loopfiresound"></a>

### `loopfiresound`

**Spellings:** `loopFireSound`.

**Inferred / medium confidence**; role: presentation.

Names the sound asset looped while a special weapon is firing; lifecycle is unverified.

Value form: quoted string. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
"ST_SPC_Sensors_Loop"
"ST_SPC_Tractor_Beam_Loop"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred loopFireSound behavior?

Evidence: [odf/special_weapons/long_range_sensor.odf:40](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/long_range_sensor.odf:40>) — observed assignment and local context; [odf/weapons/Photons/f_tractor_bm.odf:20](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/f_tractor_bm.odf:20>) — observed assignment and local context.

<a id="loopfiresoundgroup"></a>

### `loopfiresoundgroup`

**Spellings:** `loopFireSoundGroup`.

**Inferred / medium confidence**; role: presentation.

Names the sound group looped during sustained weapon fire; lifecycle is unverified.

Value form: quoted string. Units: unknown. Observed in 21 files (21 assignments).

Examples:

```text
"ST_GRP_BOR_Phaser_Loop_1"
"ST_GRP_BOR_Phaser_Loop_2"
"ST_GRP_BOR_Phaser_Loop_3"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred loopFireSoundGroup behavior?

Evidence: [odf/weapons/Pulse/fed_pulse_main_lvl1.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Pulse/fed_pulse_main_lvl1.odf:5>) — observed assignment and local context; [odf/weapons/Pulse/fed_pulse_main_lvl2.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Pulse/fed_pulse_main_lvl2.odf:5>) — observed assignment and local context.

<a id="maxcommandpoints"></a>

### `maxcommandpoints`

**Spellings:** `maxCommandPoints`.

**Inferred / medium confidence**; role: gameplay.

Sets a maximum command-point amount for the planet/station; scope of cap is unclear.

Value form: numeric literal. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
800
```

Uncertainty: Point-system semantics and Fleet Ops runtime support are unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred maxCommandPoints behavior?

Evidence: [odf/stations/classd.odf:8](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classd.odf:8>) — observed assignment and local context.

<a id="maxphaserboost"></a>

### `maxphaserboost`

**Spellings:** `maxPhaserBoost`.

**Documented / high confidence**; role: gameplay.

Sets the maximum accumulated phaser targeting boost value.

Value form: numeric literal. Units: unknown. Observed in 15 files (15 assignments).

Examples:

```text
0.50
0.40
0.80
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement maxPhaserBoost as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/F_Enterprise_E.odf:67](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise_E.odf:67>) — assignment and nearby author comment; [odf/ships/F_Stargazer.odf:78](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Stargazer.odf:78>) — assignment and nearby author comment.

<a id="maxphotonboost"></a>

### `maxphotonboost`

**Spellings:** `maxPhotonBoost`.

**Documented / high confidence**; role: gameplay.

Sets the maximum accumulated photon targeting boost value.

Value form: numeric literal. Units: unknown. Observed in 15 files (15 assignments).

Examples:

```text
0.50
0.30
0.25
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement maxPhotonBoost as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/F_Enterprise_E.odf:73](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise_E.odf:73>) — assignment and nearby author comment; [odf/ships/F_Stargazer.odf:84](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Stargazer.odf:84>) — assignment and nearby author comment.

<a id="maxradiusofbeam"></a>

### `maxradiusofbeam`

**Spellings:** `maxRadiusOfBeam`.

**Documented / high confidence**; role: presentation.

Sets the beam’s end radius.

Value form: numeric literal. Units: unknown. Observed in 38 files (38 assignments).

Examples:

```text
15.00
12.00
10.00
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement maxRadiusOfBeam as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/special_weapons/long_range_sensoro.odf:93](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/long_range_sensoro.odf:93>) — assignment and nearby author comment; [odf/weapons/Phasers/B_phaser_ent_med_ord.odf:16](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/B_phaser_ent_med_ord.odf:16>) — assignment and nearby author comment.

<a id="maxupgradelevel"></a>

### `maxupgradelevel`

**Spellings:** `maxUpgradeLevel`.

**Documented / high confidence**; role: gameplay.

Sets the owning object’s highest upgrade level, starting from zero. It is used by shipyards and planets; planet zero means no upgrade and one is annotated as one upgrade.

Value form: numeric literal with optional trailing semicolon. Units: level index from zero. Observed in 11 files (11 assignments).

Examples:

```text
3
1;
0;
```

Uncertainty: This is an object upgrade limit, not a universal shipyard-only field; level effects and failure handling remain unverified.

Next investigation: Recover the owning-object upgrade dispatch and how a new level changes available construction items.

Evidence: [odf/stations/B_Ent_Shipyard.odf:46](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/B_Ent_Shipyard.odf:46>) — assignment and nearby author comment; [odf/stations/classd.odf:40](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classd.odf:40>) — assignment and nearby author comment.

<a id="minradiusofbeam"></a>

### `minradiusofbeam`

**Spellings:** `minRadiusOfBeam`.

**Documented / high confidence**; role: presentation.

Sets the beam’s start radius.

Value form: numeric literal. Units: unknown. Observed in 38 files (38 assignments).

Examples:

```text
4.00
2.00
1.00
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement minRadiusOfBeam as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/special_weapons/long_range_sensoro.odf:90](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/long_range_sensoro.odf:90>) — assignment and nearby author comment; [odf/weapons/Phasers/B_phaser_ent_sml_ord.odf:13](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/B_phaser_ent_sml_ord.odf:13>) — assignment and nearby author comment.

<a id="numberofaftphotons"></a>

### `numberofaftphotons`

**Spellings:** `numberOfAftPhotons`.

**Inferred / medium confidence**; role: gameplay.

Sets how many aft-facing photon launchers or shots the ship has; the source does not distinguish launchers from volley count.

Value form: numeric literal. Units: unknown. Observed in 155 files (155 assignments).

Examples:

```text
0
3
2
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred numberOfAftPhotons behavior?

Evidence: [odf/ships/F_Akira.odf:36](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akira.odf:36>) — assignment and nearby author comment; [odf/ships/F_Nova.odf:59](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Nova.odf:59>) — assignment and nearby author comment.

<a id="numberofforephotons"></a>

### `numberofforephotons`

**Spellings:** `numberOfForePhotons`.

**Inferred / medium confidence**; role: gameplay.

Sets how many fore-facing photon launchers or shots the ship has; the source does not distinguish launchers from volley count.

Value form: numeric literal. Units: unknown. Observed in 158 files (158 assignments).

Examples:

```text
3
2
1
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred numberOfForePhotons behavior?

Evidence: [odf/ships/F_Galaxy_1.odf:46](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Galaxy_1.odf:46>) — observed assignment and local context; [odf/ships/K_Brel.odf:34](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_Brel.odf:34>) — observed assignment and local context.

<a id="numberofphaserbeams"></a>

### `numberofphaserbeams`

**Spellings:** `numberOfPhaserBeams`.

**Inferred / medium confidence**; role: gameplay.

Sets how many phaser beams the ship can field; simultaneous fire and hardpoint pairing are unknown.

Value form: numeric literal. Units: unknown. Observed in 171 files (171 assignments).

Examples:

```text
2
1
6
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred numberOfPhaserBeams behavior?

Evidence: [odf/ships/K_D_3.odf:31](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_3.odf:31>) — assignment and nearby author comment; [odf/ships/K_D_7.odf:33](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_7.odf:33>) — assignment and nearby author comment.

<a id="numships"></a>

### `numships`

**Spellings:** `numShips`.

**Documented / high confidence**; role: schema.

Sets the number of ship rows in a default-fleet definition.

Value form: numeric literal. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
4
5
2
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement numShips as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/default fleets/dev1_fleet.odf:4](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/default fleets/dev1_fleet.odf:4>) — assignment and nearby author comment; [odf/default fleets/dev2_fleet.odf:4](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/default fleets/dev2_fleet.odf:4>) — assignment and nearby author comment.

<a id="ordnancecapacity"></a>

### `ordnancecapacity`

**Spellings:** `ordnanceCapacity`.

**Documented / high confidence**; role: gameplay.

Sets total photon ammunition capacity; -1 denotes unlimited capacity.

Value form: numeric literal. Units: unknown. Observed in 148 files (148 assignments).

Examples:

```text
40
60
30
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement ordnanceCapacity as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Photons/Tuerell_tos_photon.odf:13](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/Tuerell_tos_photon.odf:13>) — assignment and nearby author comment; [odf/weapons/Photons/f_constit_re_photon.odf:7](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/f_constit_re_photon.odf:7>) — assignment and nearby author comment.

<a id="particletrail"></a>

### `particletrail`

**Spellings:** `particleTrail`.

**Documented / high confidence**; role: presentation.

Names the optional particle trail following the projectile.

Value form: quoted string. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
"psartiltrail"
"psphotontrail"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement particleTrail as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Photons/artillery_ord.odf:40](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/artillery_ord.odf:40>) — assignment and nearby author comment; [odf/weapons/Photons/rom_photon_ord_main.odf:22](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/rom_photon_ord_main.odf:22>) — assignment and nearby author comment.

<a id="phaserboostmodifier"></a>

### `phaserboostmodifier`

**Spellings:** `PhaserboostModifier`.

**Documented / high confidence**; role: gameplay.

Sets the rate at which phaser targeting reticles move toward lock.

Value form: numeric literal. Units: unknown. Observed in 12 files (12 assignments).

Examples:

```text
0.20
0.05
0.12
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement PhaserboostModifier as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/F_Enterprise.odf:61](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise.odf:61>) — assignment and nearby author comment; [odf/ships/F_Enterprise_E.odf:65](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise_E.odf:65>) — assignment and nearby author comment.

<a id="phaserdamage"></a>

### `phaserdamage`

**Spellings:** `phaserDamage`.

**Documented / high confidence**; role: gameplay.

Modifies hit-point damage inflicted by this phaser per damage tick.

Value form: numeric literal. Units: unknown. Observed in 10 files (10 assignments).

Examples:

```text
1.0
0.2
0.14
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Recover base-versus-modifier composition, the damage tick interval, and shield/hull application order.

Evidence: [odf/weapons/Phasers/kling_phaser_main.odf:16](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/kling_phaser_main.odf:16>) — assignment and nearby author comment; [odf/weapons/Phasers/phasero.odf:93](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/phasero.odf:93>) — assignment and nearby author comment.

<a id="phaserrechargerate"></a>

### `phaserrechargerate`

**Spellings:** `phaserRechargeRate`.

**Inferred / medium confidence**; role: gameplay.

Sets phaser recharge speed; examples 0.5 and 0.1666 are annotated as 2 and 6 seconds, but the conversion rule is not explicitly stated.

Value form: numeric literal with optional trailing semicolon. Units: unknown. Observed in 9 files (9 assignments).

Examples:

```text
0.2;
0.08;
0.1666;
```

Uncertainty: The 0.5 to 2-second and 0.1666 to 6-second examples suggest a reciprocal rate, but it is not independently verified.

Next investigation: Confirm whether interval equals 1/rate and determine the energy-ready versus firing-ready gates.

Evidence: [odf/ships/K_BirdofPrey.odf:66](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_BirdofPrey.odf:66>) — assignment and nearby author comment; [odf/ships/K_D_7.odf:136](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_7.odf:136>) — assignment and nearby author comment.

<a id="phasertargetingboostthreshold"></a>

### `phasertargetingboostthreshold`

**Spellings:** `phaserTargetingBoostThreshold`.

**Documented / high confidence**; role: gameplay.

Sets base chance to hit before phaser targeting boost accumulates.

Value form: numeric literal. Units: unknown. Observed in 15 files (15 assignments).

Examples:

```text
0.55
0.60
0.40
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement phaserTargetingBoostThreshold as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/F_Enterprise_E.odf:66](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise_E.odf:66>) — assignment and nearby author comment; [odf/ships/F_Stargazer.odf:77](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Stargazer.odf:77>) — assignment and nearby author comment.

<a id="phasertargetlocklossvalue"></a>

### `phasertargetlocklossvalue`

**Spellings:** `phaserTargetLockLossValue`.

**Documented / high confidence**; role: gameplay.

Sets the boost/countdown value applied when phaser target lock is lost.

Value form: numeric literal. Units: unknown. Observed in 15 files (15 assignments).

Examples:

```text
0.25
0.05
0.30
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement phaserTargetLockLossValue as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/F_Enterprise_E.odf:68](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise_E.odf:68>) — assignment and nearby author comment; [odf/ships/F_Stargazer.odf:79](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Stargazer.odf:79>) — assignment and nearby author comment.

<a id="photonboostmodifier"></a>

### `photonboostmodifier`

**Spellings:** `PhotonBoostModifier`.

**Documented / high confidence**; role: gameplay.

Sets the rate at which photon targeting reticles move toward lock.

Value form: numeric literal. Units: unknown. Observed in 12 files (12 assignments).

Examples:

```text
0.25
0.02
0.10
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement PhotonBoostModifier as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/F_Enterprise.odf:67](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise.odf:67>) — assignment and nearby author comment; [odf/ships/F_Enterprise_E.odf:71](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise_E.odf:71>) — assignment and nearby author comment.

<a id="photondamage"></a>

### `photondamage`

**Spellings:** `photonDamage`.

**Documented / high confidence**; role: gameplay.

Modifies the global photon damage configuration value.

Value form: numeric literal. Units: unknown. Observed in 8 files (8 assignments).

Examples:

```text
.2
.15
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Recover base-versus-modifier composition, the damage tick interval, and shield/hull application order.

Evidence: [odf/weapons/Photons/kling_photon_main.odf:17](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/kling_photon_main.odf:17>) — assignment and nearby author comment; [odf/weapons/Photons/rom_photon_main.odf:13](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/rom_photon_main.odf:13>) — assignment and nearby author comment.

<a id="photonhulldamagereduction"></a>

### `photonhulldamagereduction`

**Spellings:** `photonHullDamageReduction`.

**Inferred / medium confidence**; role: gameplay.

Reduces photon damage to the hull, under the nearby Borg armour heading; activation and application formula are not documented.

Value form: numeric literal. Units: fraction. Observed in 17 files (17 assignments).

Examples:

```text
0.60
0.30
0.15
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred photonHullDamageReduction behavior?

Evidence: [odf/ships/k_tos_boss.odf:43](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/k_tos_boss.odf:43>) — assignment and nearby author comment; [odf/ships/B_tng_cube.odf:20](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_tng_cube.odf:20>) — observed assignment and local context.

<a id="photonrechargerate"></a>

### `photonrechargerate`

**Spellings:** `photonRechargeRate`.

**Unresolved / low confidence**; role: ambiguous.

A station sets a photon recharge value, but there is no local comment explaining whether this is a period or rate.

Value form: numeric literal with optional trailing semicolon. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
0.20;
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL parser or gameplay call site reads photonRechargeRate, and what does its value control?

Evidence: [odf/stations/F_TOS_starbase.odf:162](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/F_TOS_starbase.odf:162>) — observed assignment and local context.

<a id="photonreplenishrate"></a>

### `photonreplenishrate`

**Spellings:** `photonReplenishRate`.

**Inferred / medium confidence**; role: gameplay.

Sets the rate at which a ship restores photon ordnance; replenishment units are not documented.

Value form: numeric literal. Units: unknown. Observed in 110 files (110 assignments).

Examples:

```text
0.9
1.5
1.9
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Recover pool units/capacity, update time base, energy-distribution influence and the exact debit/replenishment formula.

Evidence: [odf/ships/F_Defiant.odf:28](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Defiant.odf:28>) — assignment and nearby author comment; [odf/ships/F_Enterprise_A.odf:37](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise_A.odf:37>) — assignment and nearby author comment.

<a id="photonspread"></a>

### `photonspread`

**Spellings:** `photonSpread`.

**Inferred / medium confidence**; role: gameplay.

Configures photon spread for a ship; the observed 0, 1, 2 and 4 values show this is not demonstrably Boolean.

Value form: numeric literal. Units: unknown. Observed in 157 files (157 assignments).

Examples:

```text
1
0
4
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred photonSpread behavior?

Evidence: [odf/ships/F_Miranda.odf:34](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Miranda.odf:34>) — observed assignment and local context; [odf/ships/F_Oberth.odf:36](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Oberth.odf:36>) — observed assignment and local context.

<a id="photontargetingboostthreshold"></a>

### `photontargetingboostthreshold`

**Spellings:** `photonTargetingBoostThreshold`.

**Documented / high confidence**; role: gameplay.

Sets base chance to hit before photon targeting boost accumulates.

Value form: numeric literal. Units: unknown. Observed in 15 files (15 assignments).

Examples:

```text
0.45
0.50
0.40
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement photonTargetingBoostThreshold as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/F_Enterprise_E.odf:72](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise_E.odf:72>) — assignment and nearby author comment; [odf/ships/F_Stargazer.odf:83](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Stargazer.odf:83>) — assignment and nearby author comment.

<a id="photontargetlocklossvalue"></a>

### `photontargetlocklossvalue`

**Spellings:** `photonTargetLockLossValue`.

**Documented / high confidence**; role: gameplay.

Sets the boost/countdown value applied when photon target lock is lost.

Value form: numeric literal. Units: unknown. Observed in 15 files (15 assignments).

Examples:

```text
0.20
0.10
0.30
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement photonTargetLockLossValue as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/F_Enterprise_A.odf:57](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise_A.odf:57>) — assignment and nearby author comment; [odf/ships/F_Enterprise_E.odf:74](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Enterprise_E.odf:74>) — assignment and nearby author comment.

<a id="pivotradiusscale"></a>

### `pivotradiusscale`

**Spellings:** `pivotRadiusScale`.

**Inferred / medium confidence**; role: gameplay.

Scales the ship’s turning/pivot radius; reference geometry and effects are not documented.

Value form: numeric literal. Units: unknown. Observed in 31 files (31 assignments).

Examples:

```text
2.0
2.5
3.5
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred pivotRadiusScale behavior?

Evidence: [odf/ships/B_ent_sphere.odf:117](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_ent_sphere.odf:117>) — observed assignment and local context; [odf/ships/B_tng_cube.odf:140](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_tng_cube.odf:140>) — observed assignment and local context.

<a id="pointlightcolor"></a>

### `pointlightcolor`

**Spellings:** `pointLightColor`.

**Documented / high confidence**; role: presentation.

Sets the colour of light cast by the sun/planet on other objects.

Value form: three space-separated numeric components. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
210 210 189
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement pointLightColor as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/stations/sun.odf:32](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/sun.odf:32>) — assignment and nearby author comment.

<a id="pointmax"></a>

### `pointmax`

**Spellings:** `PointMax`.

**Inferred / medium confidence**; role: gameplay.

Caps the station/planet point amount; the resource called points is not further defined.

Value form: numeric literal. Units: unknown. Observed in 10 files (10 assignments).

Examples:

```text
1500
3000
6000
```

Uncertainty: Point-system semantics and Fleet Ops runtime support are unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred PointMax behavior?

Evidence: [odf/stations/K_TOS_mining.odf:28](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/K_TOS_mining.odf:28>) — observed assignment and local context; [odf/stations/classd.odf:103](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classd.odf:103>) — observed assignment and local context.

<a id="pointrate"></a>

### `pointrate`

**Spellings:** `PointRate`.

**Inferred / medium confidence**; role: gameplay.

Sets a rate of point production at this station/planet; point identity and time base are not documented.

Value form: numeric literal. Units: unknown. Observed in 10 files (10 assignments).

Examples:

```text
0.83
1.67
.83
```

Uncertainty: Point-system semantics and Fleet Ops runtime support are unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred PointRate behavior?

Evidence: [odf/stations/K_TOS_mining.odf:27](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/K_TOS_mining.odf:27>) — assignment and nearby author comment; [odf/stations/classd.odf:102](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classd.odf:102>) — observed assignment and local context.

<a id="pointtime"></a>

### `pointtime`

**Spellings:** `PointTime`.

**Unresolved / low confidence**; role: ambiguous.

Sets a numeric point-generation timing parameter beside PointRate and PointMax; its exact role and units are undocumented.

Value form: numeric literal. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
300
```

Uncertainty: Point-system semantics and Fleet Ops runtime support are unverified.

Next investigation: Which STL parser or gameplay call site reads PointTime, and what does its value control?

Evidence: [odf/stations/planet.odf:24](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:24>) — observed assignment and local context.

<a id="productionpointcost"></a>

### `productionpointcost`

**Spellings:** `productionPointCost`.

**Inferred / medium confidence**; role: gameplay.

Sets the point cost of constructing the station; whether it consumes the same points as PointRate is not established.

Value form: numeric literal. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
1000
100
```

Uncertainty: Point-system semantics and Fleet Ops runtime support are unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred productionPointCost behavior?

Evidence: [odf/stations/planet.odf:7](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:7>) — observed assignment and local context; [odf/stations/station.odf:144](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:144>) — observed assignment and local context.

<a id="rightenginehitchance"></a>

### `rightenginehitchance`

**Spellings:** `rightEngineHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the right facing hit-chance weight for the engine subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:102](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:102>) — observed assignment and local context; [odf/stations/station.odf:83](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:83>) — observed assignment and local context.

<a id="righthullhitchance"></a>

### `righthullhitchance`

**Spellings:** `rightHullHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the right facing hit-chance weight for the hull subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
100
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:100](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:100>) — observed assignment and local context; [odf/stations/station.odf:81](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:81>) — observed assignment and local context.

<a id="rightsensorhitchance"></a>

### `rightsensorhitchance`

**Spellings:** `rightSensorHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the right facing hit-chance weight for the sensor subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:103](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:103>) — observed assignment and local context; [odf/stations/station.odf:84](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:84>) — observed assignment and local context.

<a id="rightshieldhitchance"></a>

### `rightshieldhitchance`

**Spellings:** `rightShieldHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the right facing hit-chance weight for the shield subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:101](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:101>) — observed assignment and local context; [odf/stations/station.odf:82](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:82>) — observed assignment and local context.

<a id="rightweaponhitchance"></a>

### `rightweaponhitchance`

**Spellings:** `rightWeaponHitChance`.

**Inferred / medium confidence**; role: gameplay.

Assigns the right facing hit-chance weight for the weapon subsystem when attacks select a target system.

Value form: numeric literal. Units: unknown scale. Observed in 2 files (2 assignments).

Examples:

```text
0
.2
```

Uncertainty: Craft defaults use hull=100 and other systems=0, while station defaults use 0.2 for every system; these incompatible apparent scales prevent interpreting values as percentages or probabilities.

Next investigation: Recover facing-angle boundaries, weight normalization (100 versus .2), and behavior for missing/disabled subsystems.

Evidence: [odf/ships/craft.odf:104](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:104>) — observed assignment and local context; [odf/stations/station.odf:85](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:85>) — observed assignment and local context.

<a id="romulanentbuildlist"></a>

### `romulanentbuildlist`

**Spellings:** `romulanEntBuildList`.

**Documented / high confidence**; role: gameplay.

Defines the Romulan ENT construction list for the owning planet. Each pair is an ODF name and the required planet upgrade level, numbered from zero; the trailing 0/1 is a level requirement, not an unexplained Boolean.

Value form: repeated quoted pairs: "ODF name" "required planet upgrade level". Units: zero-based planet upgrade level. Observed in 6 files (6 assignments).

Examples:

```text
"r_ent_shipyard" "1"
"r_ent_turret" "0"
"r_ent_engineering" "1"
"r_ent_shipyard" "0"
"r_ent_turret" "0"
"r_ent_sensor" "1"
"r_ent_engineering" "0"
"r_ent_shipyard" "0"
"r_ent_turret" "0"
"r_ent_sensor" "0"
"r_shroud" "0"
"r_tal_shiar" "0"
```

Uncertainty: Selection by owner/era and inheritance are not traced; number parsing, duplicate items and invalid level handling remain unknown.

Next investigation: Recover owner/era selection, planet upgrade gating, duplicate handling and validation of required levels.

Evidence: [odf/stations/classj.odf:64](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:64>) — Shared build-list header explicitly defines ODF-name/required-planet-level pairs from zero..

<a id="romulanentcommandpost"></a>

### `romulanentcommandpost`

**Spellings:** `romulanEntCommandPost`.

**Documented / high confidence**; role: gameplay.

Names the Romulan ENT command-post ODF associated with a planet, selected by race and era according to the planet header.

Value form: quoted ODF basename. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"r_ent_command"
```

Uncertainty: Creation time, attachment/placement, capture replacement and absent-definition fallbacks are unknown.

Next investigation: Recover when the planet creates or replaces its race/era command post.

Evidence: [odf/stations/planet.odf:48](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:48>) — Header explicitly describes command-post variants by race and era..

<a id="romulantngbuildlist"></a>

### `romulantngbuildlist`

**Spellings:** `romulanTngBuildList`.

**Documented / high confidence**; role: gameplay.

Defines the Romulan TNG construction list for the owning planet. Each pair is an ODF name and the required planet upgrade level, numbered from zero; the trailing 0/1 is a level requirement, not an unexplained Boolean.

Value form: repeated quoted pairs: "ODF name" "required planet upgrade level". Units: zero-based planet upgrade level. Observed in 6 files (6 assignments).

Examples:

```text
"r_tng_shipyard" "1"
"r_tng_turret" "0"
"r_tng_engineering" "1"
"r_tng_shipyard" "0"
"r_tng_turret" "0"
"r_tng_sensor" "1"
"r_soliton" "1"
"r_tng_engineering" "0"
"r_tng_shipyard" "0"
"r_tng_turret" "0"
"r_tng_sensor" "0"
"r_shroud" "0"
"r_tal_shiar" "0"
"r_soliton" "0"
```

Uncertainty: Selection by owner/era and inheritance are not traced; number parsing, duplicate items and invalid level handling remain unknown.

Next investigation: Recover owner/era selection, planet upgrade gating, duplicate handling and validation of required levels.

Evidence: [odf/stations/classj.odf:129](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:129>) — Shared build-list header explicitly defines ODF-name/required-planet-level pairs from zero..

<a id="romulantngcommandpost"></a>

### `romulantngcommandpost`

**Spellings:** `romulanTngCommandPost`.

**Documented / high confidence**; role: gameplay.

Names the Romulan TNG command-post ODF associated with a planet, selected by race and era according to the planet header.

Value form: quoted ODF basename. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"r_tng_command"
```

Uncertainty: Creation time, attachment/placement, capture replacement and absent-definition fallbacks are unknown.

Next investigation: Recover when the planet creates or replaces its race/era command post.

Evidence: [odf/stations/planet.odf:62](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:62>) — Header explicitly describes command-post variants by race and era..

<a id="romulantosbuildlist"></a>

### `romulantosbuildlist`

**Spellings:** `romulanTosBuildList`.

**Documented / high confidence**; role: gameplay.

Defines the Romulan TOS construction list for the owning planet. Each pair is an ODF name and the required planet upgrade level, numbered from zero; the trailing 0/1 is a level requirement, not an unexplained Boolean.

Value form: repeated quoted pairs: "ODF name" "required planet upgrade level". Units: zero-based planet upgrade level. Observed in 6 files (6 assignments).

Examples:

```text
"r_tos_shipyard" "1"
"r_tos_turret" "0"
"r_tos_engineering" "1"
"r_tos_shipyard" "0"
"r_tos_turret" "0"
"r_tos_sensor" "1"
"r_soliton" "1"
"r_tos_engineering" "0"
"r_tos_shipyard" "0"
"r_tos_turret" "0"
"r_tos_sensor" "0"
"r_shroud" "0"
"r_tal_shiar" "0"
"r_soliton" "0"
```

Uncertainty: Selection by owner/era and inheritance are not traced; number parsing, duplicate items and invalid level handling remain unknown.

Next investigation: Recover owner/era selection, planet upgrade gating, duplicate handling and validation of required levels.

Evidence: [odf/stations/classj.odf:96](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classj.odf:96>) — Shared build-list header explicitly defines ODF-name/required-planet-level pairs from zero..

<a id="romulantoscommandpost"></a>

### `romulantoscommandpost`

**Spellings:** `romulanTosCommandPost`.

**Documented / high confidence**; role: gameplay.

Names the Romulan TOS command-post ODF associated with a planet, selected by race and era according to the planet header.

Value form: quoted ODF basename. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"r_tos_command"
```

Uncertainty: Creation time, attachment/placement, capture replacement and absent-definition fallbacks are unknown.

Next investigation: Recover when the planet creates or replaces its race/era command post.

Evidence: [odf/stations/planet.odf:55](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/planet.odf:55>) — Header explicitly describes command-post variants by race and era..

<a id="selfdestructdamage"></a>

### `selfdestructdamage`

**Spellings:** `selfDestructDamage`.

**Documented / high confidence**; role: gameplay.

Sets damage applied when the craft blows up. Nearby comments describe the ship being destroyed, so this is not established as exclusive to a manually issued self-destruct order.

Value form: numeric literal with optional trailing semicolon. Units: unknown. Observed in 124 files (126 assignments).

Examples:

```text
1000;
550;
400;
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement selfDestructDamage as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/F_Akira.odf:137](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akira.odf:137>) — assignment and nearby author comment; [odf/ships/F_Nova.odf:161](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Nova.odf:161>) — assignment and nearby author comment.

<a id="selfdestructradius"></a>

### `selfdestructradius`

**Spellings:** `selfDestructRadius`.

**Inferred / medium confidence**; role: gameplay.

Sets the area affected by self-destruct damage; distance units and falloff are not stated.

Value form: numeric literal with optional trailing semicolon. Units: unknown. Observed in 124 files (126 assignments).

Examples:

```text
1000;
450;
350;
```

Uncertainty: The comment refers to ship destruction; manual self-destruct exclusivity, distance units and damage falloff are not established.

Next investigation: Which STL runtime parser or call site confirms the inferred selfDestructRadius behavior?

Evidence: [odf/ships/K_ENT_Raptor.odf:132](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_ENT_Raptor.odf:132>) — assignment and nearby author comment; [odf/ships/K_Raptor.odf:133](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_Raptor.odf:133>) — assignment and nearby author comment.

<a id="selfilluminating"></a>

### `selfilluminating`

**Spellings:** `selfIlluminating`.

**Inferred / medium confidence**; role: presentation.

Enables self-illumination of the station material/planet; the material math is undocumented.

Value form: numeric literal. Units: unknown. Observed in 8 files (8 assignments).

Examples:

```text
1
0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred selfIlluminating behavior?

Evidence: [odf/stations/bajor.odf:7](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/bajor.odf:7>) — observed assignment and local context; [odf/stations/classd_lava.odf:12](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classd_lava.odf:12>) — observed assignment and local context.

<a id="sensorbreaknodes"></a>

### `sensorbreaknodes`

**Spellings:** `sensorBreakNodes`.

**Inferred / medium confidence**; role: asset.

Names model nodes broken away with sensor subsystem destruction.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
"Sensor"
"pod"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Determine which damage/break event uses the list and whether it hides, detaches or selects model-node subtrees.

Evidence: [odf/ships/F_Steamrunner.odf:160](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Steamrunner.odf:160>) — assignment and nearby author comment; [odf/ships/F_Fortunate.odf:97](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Fortunate.odf:97>) — observed assignment and local context.

<a id="sensorbreakpoints"></a>

### `sensorbreakpoints`

**Spellings:** `sensorBreakPoints`.

**Inferred / medium confidence**; role: gameplay.

Names a sensor breakup point; distinction from sensorBreakNodes is undocumented.

Value form: quoted string. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
"bridge"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Determine which damage/break event uses the list and whether it hides, detaches or selects model-node subtrees.

Evidence: [odf/ships/K_D_7.odf:115](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_7.odf:115>) — observed assignment and local context.

<a id="sensordestructioneffect"></a>

### `sensordestructioneffect`

**Spellings:** `SensorDestructionEffect`.

**Inferred / medium confidence**; role: presentation.

Names the visual effect played when the sensor subsystem is destroyed.

Value form: quoted string. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
"psexplosion_sensors"
"psexplosion_9"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred SensorDestructionEffect behavior?

Evidence: [odf/ships/craft.odf:166](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:166>) — observed assignment and local context; [odf/ships/q_ent03_kcargo.odf:54](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/q_ent03_kcargo.odf:54>) — observed assignment and local context.

<a id="sensordestructionhardpoints"></a>

### `sensordestructionhardpoints`

**Spellings:** `SensorDestructionHardpoints`.

**Inferred / medium confidence**; role: asset.

Lists model hardpoints where sensor subsystem destruction effects appear.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 120 files (122 assignments).

Examples:

```text
"hp06"
"hp16"
"hp32"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred SensorDestructionHardpoints behavior?

Evidence: [odf/ships/B_ent_cube.odf:119](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_ent_cube.odf:119>) — assignment and nearby author comment; [odf/ships/B_ent_diamond.odf:112](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_ent_diamond.odf:112>) — assignment and nearby author comment.

<a id="sensordestructionsize"></a>

### `sensordestructionsize`

**Spellings:** `SensorDestructionSize`.

**Inferred / medium confidence**; role: presentation.

Scales the visual effect for sensor subsystem destruction.

Value form: numeric literal. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
2.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred SensorDestructionSize behavior?

Evidence: [odf/ships/q_ent03_kcargo.odf:55](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/q_ent03_kcargo.odf:55>) — observed assignment and local context.

<a id="sensormaxstatistic"></a>

### `sensormaxstatistic`

**Spellings:** `sensorMaxStatistic`.

**Inferred / medium confidence**; role: gameplay.

Sets the maximum sensor statistic or range at an upgraded level; the associated minimum is described as short-range sensor range.

Value form: numeric literal. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
1600
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred sensorMaxStatistic behavior?

Evidence: [odf/stations/station.odf:55](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:55>) — observed assignment and local context.

<a id="sensorminhitpoints"></a>

### `sensorminhitpoints`

**Spellings:** `sensorMinHitPoints`.

**Inferred / medium confidence**; role: gameplay.

Sets minimum sensor subsystem hit points at a low upgrade level; the exact level rule is not in this assignment.

Value form: numeric literal. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
100
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred sensorMinHitPoints behavior?

Evidence: [odf/stations/station.odf:53](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:53>) — observed assignment and local context.

<a id="sensorminstatistic"></a>

### `sensorminstatistic`

**Spellings:** `sensorMinStatistic`.

**Documented / high confidence**; role: gameplay.

Sets the short-range sensor range at the minimum sensor statistic.

Value form: numeric literal. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
800
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement sensorMinStatistic as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/stations/station.odf:54](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:54>) — assignment and nearby author comment.

<a id="sensortargethardpoints"></a>

### `sensortargethardpoints`

**Spellings:** `SensorTargetHardpoints`, `sensorTargetHardpoints`.

**Inferred / medium confidence**; role: asset.

Lists model hardpoints used when attacking or targeting the sensor subsystem.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 118 files (118 assignments).

Examples:

```text
"hp06"
"hp16"
"hp32"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Determine how hardpoint lists select aim points and handle missing names, visibility changes and destroyed subsystems.

Evidence: [odf/ships/B_ent_cube.odf:113](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_ent_cube.odf:113>) — assignment and nearby author comment; [odf/ships/B_ent_diamond.odf:106](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_ent_diamond.odf:106>) — assignment and nearby author comment.

<a id="shadername"></a>

### `shadername`

**Spellings:** `shadername`.

**Inferred / medium confidence**; role: asset.

Names the shader assigned to a planet; lookup and renderer compatibility are unknown.

Value form: quoted string. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"planet_lava"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred shadername behavior?

Evidence: [odf/stations/classd_lava.odf:14](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classd_lava.odf:14>) — observed assignment and local context.

<a id="shieldbackproportion"></a>

### `shieldbackproportion`

**Spellings:** `shieldBackProportion`.

**Inferred / medium confidence**; role: ambiguous.

Sets the relative size/weight of the back shield facing compared with the other facings, according to the four-field group.

Value form: numeric literal. Units: relative proportion. Observed in 2 files (2 assignments).

Examples:

```text
1
```

Uncertainty: The phrase how big does not distinguish capacity allocation from angular coverage; independent facing pools and normalization are not established.

Next investigation: Determine whether proportions allocate shield capacity, define angular spans, or weight another facing property.

Evidence: [odf/ships/craft.odf:61](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:61>) — observed assignment and local context.

<a id="shieldbreaknodes"></a>

### `shieldbreaknodes`

**Spellings:** `shieldBreakNodes`.

**Inferred / medium confidence**; role: asset.

Names model nodes broken away with shield subsystem destruction.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"Shield"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Determine which damage/break event uses the list and whether it hides, detaches or selects model-node subtrees.

Evidence: [odf/ships/F_Lakota.odf:121](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Lakota.odf:121>) — observed assignment and local context.

<a id="shieldcolor"></a>

### `shieldcolor`

**Spellings:** `shieldColor`.

**Documented / high confidence**; role: presentation.

Sets the shield effect colour using normalized components from 0 to 1, according to the comment.

Value form: three space-separated numeric components. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
0.99 0.99 0.15
0.2 1.0 1.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement shieldColor as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/K_Ravenous_borgboss.odf:165](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_Ravenous_borgboss.odf:165>) — assignment and nearby author comment; [odf/ships/v_tuerell_d.odf:119](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/v_tuerell_d.odf:119>) — assignment and nearby author comment.

<a id="shielddestructioneffect"></a>

### `shielddestructioneffect`

**Spellings:** `ShieldDestructionEffect`.

**Inferred / medium confidence**; role: presentation.

Names the visual effect played when the shield subsystem is destroyed.

Value form: quoted string. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
"psexplosion_shields"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred ShieldDestructionEffect behavior?

Evidence: [odf/ships/craft.odf:164](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:164>) — observed assignment and local context.

<a id="shielddestructionhardpoints"></a>

### `shielddestructionhardpoints`

**Spellings:** `ShieldDestructionHardpoints`.

**Inferred / medium confidence**; role: asset.

Lists model hardpoints where shield subsystem destruction effects appear.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 86 files (86 assignments).

Examples:

```text
"hp04"
"hp15"
"hp30"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred ShieldDestructionHardpoints behavior?

Evidence: [odf/ships/F_Akira.odf:114](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akira.odf:114>) — observed assignment and local context; [odf/ships/F_Galaxy.odf:132](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Galaxy.odf:132>) — observed assignment and local context.

<a id="shielddisabletime"></a>

### `shielddisabletime`

**Spellings:** `shieldDisableTime`.

**Documented / high confidence**; role: gameplay.

Sets how long the shields remain disabled.

Value form: numeric literal. Units: seconds. Observed in 3 files (3 assignments).

Examples:

```text
10
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement shieldDisableTime as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/craft.odf:55](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:55>) — assignment and nearby author comment.

<a id="shieldfrontproportion"></a>

### `shieldfrontproportion`

**Spellings:** `shieldFrontProportion`.

**Documented / high confidence**; role: ambiguous.

Sets the relative size/weight of the front shield facing compared with the other facings, according to the four-field group.

Value form: numeric literal. Units: relative proportion. Observed in 2 files (2 assignments).

Examples:

```text
1
```

Uncertainty: The phrase how big does not distinguish capacity allocation from angular coverage; independent facing pools and normalization are not established.

Next investigation: Determine whether proportions allocate shield capacity, define angular spans, or weight another facing property.

Evidence: [odf/ships/craft.odf:60](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:60>) — assignment and nearby author comment.

<a id="shieldgeneratorbreaknodes"></a>

### `shieldgeneratorbreaknodes`

**Spellings:** `shieldGeneratorBreakNodes`, `shieldgeneratorBreakNodes`.

**Inferred / low confidence**; role: asset.

Names model nodes associated with shield-generator breakage; observed assignments include shield and engine/bridge node names, so the exact trigger and detached parts are unclear.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 14 files (14 assignments).

Examples:

```text
"Shields"
"R Engine" "L Engine"
"Eng_Bridge"
```

Uncertainty: The listed nodes differ across craft and no comment defines whether this is subsystem damage, breakup, or a cosmetic link. Fleet Ops support is unverified.

Next investigation: Determine which damage/break event uses the list and whether it hides, detaches or selects model-node subtrees.

Evidence: [odf/ships/K_Brel.odf:112](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_Brel.odf:112>) — observed assignment and local context; [odf/ships/K_D_7.odf:116](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_D_7.odf:116>) — observed assignment and local context.

<a id="shieldleftproportion"></a>

### `shieldleftproportion`

**Spellings:** `shieldLeftProportion`.

**Inferred / medium confidence**; role: ambiguous.

Sets the relative size/weight of the left shield facing compared with the other facings, according to the four-field group.

Value form: numeric literal. Units: relative proportion. Observed in 2 files (2 assignments).

Examples:

```text
1
```

Uncertainty: The phrase how big does not distinguish capacity allocation from angular coverage; independent facing pools and normalization are not established.

Next investigation: Determine whether proportions allocate shield capacity, define angular spans, or weight another facing property.

Evidence: [odf/ships/craft.odf:63](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:63>) — observed assignment and local context.

<a id="shieldmaxabsorption"></a>

### `shieldmaxabsorption`

**Spellings:** `shieldMaxAbsorption`.

**Documented / high confidence**; role: gameplay.

Sets the fraction of damage absorbed when shields are full.

Value form: numeric literal. Units: fraction. Observed in 3 files (3 assignments).

Examples:

```text
.7
1.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement shieldMaxAbsorption as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/craft.odf:56](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:56>) — assignment and nearby author comment; [odf/stations/station.odf:38](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:38>) — assignment and nearby author comment.

<a id="shieldmaxstatistic"></a>

### `shieldmaxstatistic`

**Spellings:** `shieldMaxStatistic`.

**Documented / high confidence**; role: gameplay.

Sets total shield points at level 5.

Value form: numeric literal. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
1500
100
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement shieldMaxStatistic as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/stations/ent03_fed_station.odf:97](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/ent03_fed_station.odf:97>) — assignment and nearby author comment; [odf/stations/station.odf:35](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:35>) — assignment and nearby author comment.

<a id="shieldminabsorption"></a>

### `shieldminabsorption`

**Spellings:** `shieldMinAbsorption`.

**Documented / high confidence**; role: gameplay.

Sets the fraction of damage absorbed when shields are low.

Value form: numeric literal. Units: fraction. Observed in 3 files (3 assignments).

Examples:

```text
.3
0.4
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement shieldMinAbsorption as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/craft.odf:57](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:57>) — assignment and nearby author comment; [odf/stations/station.odf:39](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:39>) — assignment and nearby author comment.

<a id="shieldminhitpoints"></a>

### `shieldminhitpoints`

**Spellings:** `shieldMinHitPoints`.

**Inferred / medium confidence**; role: gameplay.

Sets minimum shield subsystem hit points; the source groups it under level 1 and 5 values but does not explain interpolation.

Value form: numeric literal. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
500
100
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred shieldMinHitPoints behavior?

Evidence: [odf/stations/ent03_fed_station.odf:95](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/ent03_fed_station.odf:95>) — assignment and nearby author comment; [odf/stations/station.odf:33](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:33>) — assignment and nearby author comment.

<a id="shieldminstatistic"></a>

### `shieldminstatistic`

**Spellings:** `shieldMinStatistic`.

**Documented / high confidence**; role: gameplay.

Sets total shield points at level 1.

Value form: numeric literal. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
1500
1000
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement shieldMinStatistic as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/stations/ent03_fed_station.odf:96](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/ent03_fed_station.odf:96>) — assignment and nearby author comment; [odf/stations/station.odf:34](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/station.odf:34>) — assignment and nearby author comment.

<a id="shieldrechargerate"></a>

### `shieldrechargerate`

**Spellings:** `shieldRechargeRate`.

**Documented / high confidence**; role: gameplay.

Sets how quickly shields repair or recharge; the time base is not stated.

Value form: numeric literal. Units: unknown. Observed in 12 files (12 assignments).

Examples:

```text
1.0
0.8
50.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement shieldRechargeRate as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/ships/class_science.odf:73](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_science.odf:73>) — assignment and nearby author comment; [odf/ships/v_surak.odf:123](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/v_surak.odf:123>) — assignment and nearby author comment.

<a id="shieldrechargeratemodifier"></a>

### `shieldrechargeratemodifier`

**Spellings:** `shieldRechargeRateModifier`.

**Documented / high confidence**; role: gameplay.

Sets a per-ship modifier for the shield recharge rate.

Value form: numeric literal. Units: unknown. Observed in 75 files (75 assignments).

Examples:

```text
1.0
0.8
0.95
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement shieldRechargeRateModifier as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/stations/F_TOS_command.odf:17](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/F_TOS_command.odf:17>) — assignment and nearby author comment; [odf/ships/F_Defiant.odf:32](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Defiant.odf:32>) — assignment and nearby author comment.

<a id="shieldrightproportion"></a>

### `shieldrightproportion`

**Spellings:** `shieldRightProportion`.

**Inferred / medium confidence**; role: ambiguous.

Sets the relative size/weight of the right shield facing compared with the other facings, according to the four-field group.

Value form: numeric literal. Units: relative proportion. Observed in 2 files (2 assignments).

Examples:

```text
1
```

Uncertainty: The phrase how big does not distinguish capacity allocation from angular coverage; independent facing pools and normalization are not established.

Next investigation: Determine whether proportions allocate shield capacity, define angular spans, or weight another facing property.

Evidence: [odf/ships/craft.odf:62](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:62>) — observed assignment and local context.

<a id="shieldtargethardpoints"></a>

### `shieldtargethardpoints`

**Spellings:** `ShieldTargetHardpoints`.

**Inferred / medium confidence**; role: asset.

Lists model hardpoints used when attacking or targeting the shield subsystem.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 85 files (85 assignments).

Examples:

```text
"hp04"
"hp30"
"hp16"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Determine how hardpoint lists select aim points and handle missing names, visibility changes and destroyed subsystems.

Evidence: [odf/ships/F_Akira.odf:108](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akira.odf:108>) — observed assignment and local context; [odf/ships/F_Galaxy.odf:126](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Galaxy.odf:126>) — observed assignment and local context.

<a id="shipindex"></a>

### `ship<N>`

**Spellings:** `ship0`, `ship1`, `ship2`, `ship3`, `ship4`.

**Documented / high confidence**; role: schema.

Defines one numbered vessel in a default fleet. The authored row grammar is three quoted fields: class ODF, display name and label; an empty display-name string is used by dev1_fleet.

Value form: three quoted strings: class ODF, display name, label. Units: unknown. Observed in 3 files (11 assignments).

Examples:

```text
"f_miranda_refit" "" "f_miranda_refit"
"f_NX_01" "Test Fleet ship 1" "f_nx01_1"
"f_constitution_refit" "" "f_constitution_refit"
```

Uncertainty: Label lookup, blank-name fallback, list gaps and spawning order are not recovered.

Next investigation: Recover fleet construction, name fallback and the purpose of the label field.

Evidence: [odf/default fleets/dev1_fleet.odf:8](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/default fleets/dev1_fleet.odf:8>) — observed assignment and local context; [odf/default fleets/dev2_fleet.odf:8](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/default fleets/dev2_fleet.odf:8>) — observed assignment and local context.

<a id="shipsize"></a>

### `shipsize`

**Spellings:** `shipSize`.

**Inferred / medium confidence**; role: gameplay.

Sets a ship size category as an integer; gameplay effect and category boundaries are undocumented.

Value form: numeric literal. Units: unknown. Observed in 5 files (5 assignments).

Examples:

```text
4
3
2
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred shipSize behavior?

Evidence: [odf/ships/class_battleship.odf:118](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_battleship.odf:118>) — assignment and nearby author comment; [odf/ships/class_cruiser.odf:126](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_cruiser.odf:126>) — assignment and nearby author comment.

<a id="showclass"></a>

### `showclass`

**Spellings:** `showclass`.

**Inferred / medium confidence**; role: ui.

Names a GUI class string displayed for the ship, such as battleship or cruiser; the precise UI surface is unknown.

Value form: quoted string. Units: unknown. Observed in 5 files (5 assignments).

Examples:

```text
"GUI_CLASS_BATTLESHIP"
"GUI_CLASS_CRUISER"
"GUI_CLASS_DESTROYER"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred showclass behavior?

Evidence: [odf/ships/class_cruiser.odf:28](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_cruiser.odf:28>) — observed assignment and local context; [odf/ships/class_destroyer.odf:25](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/class_destroyer.odf:25>) — observed assignment and local context.

<a id="sparkseffectonhitcraft"></a>

### `sparkseffectonhitcraft`

**Spellings:** `sparksEffectOnHitCraft`.

**Inferred / medium confidence**; role: presentation.

Names the sparks effect played when this weapon hits a target’s craft.

Value form: quoted string. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"pssparks"
"pssparks_shield"
```

Uncertainty: No command-specific ODF comment found; meaning follows placement and naming.

Next investigation: Which STL runtime parser or call site confirms the inferred sparksEffectOnHitCraft behavior?

Evidence: [odf/special_weapons/chainp.odf:74](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/chainp.odf:74>) — observed assignment and local context; [odf/special_weapons/spacemine.odf:12](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/spacemine.odf:12>) — observed assignment and local context.

<a id="sparkseffectonhitcraft-size"></a>

### `sparkseffectonhitcraft_size`

**Spellings:** `sparksEffectOnHitCraft_Size`.

**Inferred / medium confidence**; role: presentation.

Scales the sparks effect played on a craft hit; the reference asset size is unknown.

Value form: numeric literal. Units: scale. Observed in 4 files (4 assignments).

Examples:

```text
2.0
10.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred sparksEffectOnHitCraft_Size behavior?

Evidence: [odf/special_weapons/chainp.odf:75](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/chainp.odf:75>) — observed assignment and local context; [odf/special_weapons/spacemine.odf:13](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/spacemine.odf:13>) — observed assignment and local context.

<a id="sparkseffectonhitshields"></a>

### `sparkseffectonhitshields`

**Spellings:** `sparksEffectOnHitShields`.

**Inferred / medium confidence**; role: presentation.

Names the sparks effect played when this weapon hits a target’s shields.

Value form: quoted string. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
"pssparks"
```

Uncertainty: No command-specific ODF comment found; meaning follows placement and naming.

Next investigation: Which STL runtime parser or call site confirms the inferred sparksEffectOnHitShields behavior?

Evidence: [odf/special_weapons/chainp.odf:80](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/chainp.odf:80>) — observed assignment and local context.

<a id="sparkseffectonhitshields-size"></a>

### `sparkseffectonhitshields_size`

**Spellings:** `sparksEffectOnHitShields_Size`.

**Inferred / medium confidence**; role: presentation.

Scales the sparks effect played on a shields hit; the reference asset size is unknown.

Value form: numeric literal. Units: scale. Observed in 3 files (3 assignments).

Examples:

```text
2.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred sparksEffectOnHitShields_Size behavior?

Evidence: [odf/special_weapons/chainp.odf:81](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/chainp.odf:81>) — observed assignment and local context.

<a id="specialcooldowntimer"></a>

### `specialcooldowntimer`

**Spellings:** `specialCooldownTimer`.

**Documented / high confidence**; role: gameplay.

Sets cooldown before a cloak or warp special ability may activate again.

Value form: numeric literal. Units: time units (seconds not expressly confirmed). Observed in 4 files (4 assignments).

Examples:

```text
140
40
120
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Recover timer units, start/reset events, negative/zero sentinels and interaction with energy depletion.

Evidence: [odf/special_weapons/gcloak.odf:46](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/gcloak.odf:46>) — assignment and nearby author comment; [odf/special_weapons/gcloak_permanent.odf:46](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/gcloak_permanent.odf:46>) — assignment and nearby author comment.

<a id="specialdurationtimer"></a>

### `specialdurationtimer`

**Spellings:** `specialDurationTimer`.

**Documented / high confidence**; role: gameplay.

Sets how long a cloak or warp special ability remains active; -1 appears for permanent cloak, but sentinel handling is unverified.

Value form: numeric literal. Units: time units (seconds not expressly confirmed). Observed in 4 files (4 assignments).

Examples:

```text
100
-1
20
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Recover timer units, start/reset events, negative/zero sentinels and interaction with energy depletion.

Evidence: [odf/special_weapons/gcloak.odf:43](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/gcloak.odf:43>) — assignment and nearby author comment; [odf/special_weapons/gcloak_permanent.odf:43](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/special_weapons/gcloak_permanent.odf:43>) — assignment and nearby author comment.

<a id="specialeffectonhit"></a>

### `specialeffectonhit`

**Spellings:** `specialEffectOnHit`.

**Documented / high confidence**; role: presentation.

Names a cosmetic particle effect played when the weapon hits craft.

Value form: quoted string. Units: unknown. Observed in 15 files (15 assignments).

Examples:

```text
"pssparks"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement specialEffectOnHit as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Pulse/fed_tng_pulse.odf:50](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Pulse/fed_tng_pulse.odf:50>) — assignment and nearby author comment; [odf/weapons/Pulse/pulseo.odf:49](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Pulse/pulseo.odf:49>) — assignment and nearby author comment.

<a id="specificcaptain"></a>

### `specificcaptain`

**Spellings:** `specificCaptain`.

**Inferred / medium confidence**; role: gameplay.

Names the captain voice/persona for the ship, such as Kirk or Picard; how this interacts with HasCaptain is not documented.

Value form: quoted string. Units: unknown. Observed in 8 files (8 assignments).

Examples:

```text
"Kirk"
"Sisko"
"Archer"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred specificCaptain behavior?

Evidence: [odf/ships/F_Constitution.odf:205](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Constitution.odf:205>) — assignment and nearby author comment; [odf/ships/F_Defiant.odf:151](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Defiant.odf:151>) — assignment and nearby author comment.

<a id="specular"></a>

### `specular`

**Spellings:** `specular`.

**Inferred / medium confidence**; role: presentation.

Sets a station material specular RGB colour; component scaling and lighting calculation are unknown.

Value form: three space-separated numeric components. Units: unknown. Observed in 9 files (9 assignments).

Examples:

```text
255 255 255
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred specular behavior?

Evidence: [odf/stations/bajor.odf:13](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/bajor.odf:13>) — observed assignment and local context.

<a id="specularpower"></a>

### `specularpower`

**Spellings:** `specularPower`, `specularpower`.

**Inferred / medium confidence**; role: presentation.

Sets the material specular highlight exponent or strength; exact shader semantics are unknown.

Value form: numeric literal. Units: unknown. Observed in 10 files (10 assignments).

Examples:

```text
20
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred specularPower behavior?

Evidence: [odf/stations/bajor.odf:14](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/bajor.odf:14>) — observed assignment and local context.

<a id="speculartexturename"></a>

### `speculartexturename`

**Spellings:** `specularTextureName`.

**Inferred / medium confidence**; role: asset.

Names the station/planet specular texture asset; texture lookup rules are unverified.

Value form: quoted string. Units: unknown. Observed in 16 files (16 assignments).

Examples:

```text
"ClassM_earth_Spec"
"classd_lava_spec"
"classJb_spec"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred specularTextureName behavior?

Evidence: [odf/stations/bajor.odf:10](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/bajor.odf:10>) — observed assignment and local context; [odf/stations/classJb.odf:8](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/classJb.odf:8>) — observed assignment and local context.

<a id="startintensity"></a>

### `startintensity`

**Spellings:** `startIntensity`.

**Documented / high confidence**; role: presentation.

Sets the initial phaser beam intensity before it reaches full intensity.

Value form: numeric literal. Units: unknown. Observed in 40 files (40 assignments).

Examples:

```text
0.33
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Does the STL runtime implement startIntensity as the authored comment describes, and does Fleet Ops preserve it?

Evidence: [odf/weapons/Phasers/phasero.odf:78](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/phasero.odf:78>) — assignment and nearby author comment.

<a id="tng"></a>

### `tng`

**Spellings:** `tng`.

**Documented / high confidence**; role: ui.

Lists Next Generation-era ship types available to choose in Skirmish/MP for the faction. The source calls these selectable ship types, rather than a preselected spawn fleet.

Value form: space-separated quoted ODF basenames. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"B_tng_rectangle" "B_tng_sphere" "B_tng_obelisk"
"B_tng_cube" "B_tng_cube_tact" "B_tng_pyramid" "B_tng_diamond"
"F_Nova" "F_Defiant" "F_Steamrunner"
"F_Ambassador" "F_Akira" "F_Lakota" "F_Galaxy"
"F_Sovereign" "F_Nebula" "F_Intrepid" "F_Norway"
"K_Brel" "K_K_32" "K_Ktinga"
"K_D_12" "K_Kvort" "K_L_40" "K_Neghvar"
"K_Vorcha" "K_Feklar" "K_Stalker" "K_Death_Rite"
```

Uncertainty: Selection UI filtering, duplicates, starting availability and era lookup remain unverified.

Next investigation: Trace era/faction ship selection and distinguish selection lists from shipN spawn fleets.

Evidence: [odf/default fleets/borg_fleet.odf:17](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/default fleets/borg_fleet.odf:17>) — assignment and nearby author comment; [odf/default fleets/federation_fleet.odf:17](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/default fleets/federation_fleet.odf:17>) — assignment and nearby author comment.

<a id="tos"></a>

### `tos`

**Spellings:** `tos`.

**Documented / high confidence**; role: ui.

Lists Original Series-era ship types available to choose in Skirmish/MP for the faction. The source calls these selectable ship types, rather than a preselected spawn fleet.

Value form: space-separated quoted ODF basenames. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
"B_tos_rectangle" "B_tos_sphere" "B_tos_obelisk" "B_tos_cube"
"B_tos_pyramid" "B_tos_diamond"
"F_Strider" "F_Apollo" "F_Akula" "F_Constitution"
"F_Constitution_refit" "F_Excelsior"
"F_Proxima" "F_Miranda_refit" "F_Oberth"
"F_Constellation" "F_Centaur"
"K_BirdofPrey" "K_D_3" "K_D_7" "K_K_18"
"K_Ravenous" "K_K_30" "K_D_8"
```

Uncertainty: Selection UI filtering, duplicates, starting availability and era lookup remain unverified.

Next investigation: Trace era/faction ship selection and distinguish selection lists from shipN spawn fleets.

Evidence: [odf/default fleets/borg_fleet.odf:12](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/default fleets/borg_fleet.odf:12>) — assignment and nearby author comment; [odf/default fleets/federation_fleet.odf:11](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/default fleets/federation_fleet.odf:11>) — assignment and nearby author comment.

<a id="trailhardpoints"></a>

### `trailhardpoints`

**Spellings:** `trailHardpoints`.

**Inferred / medium confidence**; role: asset.

Lists model hardpoints used to attach ship trails; trail effect selection is not shown.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
"hp43" "hp44" "hp45" "hp46"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred trailHardpoints behavior?

Evidence: [odf/ships/B_tng_cube.odf:104](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/B_tng_cube.odf:104>) — observed assignment and local context.

<a id="upgradeindex"></a>

### `upgrade<N>`

**Spellings:** `upgrade0`, `upgrade1`, `upgrade10`, `upgrade11`, `upgrade16`, `upgrade2`, `upgrade3`, `upgrade4`, `upgrade5`, `upgrade6`, `upgrade7`, `upgrade8`, `upgrade9`.

**Documented / high confidence**; role: gameplay.

Defines a numbered craft upgrade instruction. Authored grammar: upgrade database key, UI string key, operation (add/mult/toggle/special), cost and allowMultiple; add/mult append systemName and upgrade_value.

Value form: quoted instruction: key UI-key operation cost allowMultiple [systemName upgrade_value]. Units: mixed identifiers, cost amount, repeatability flag and operation-dependent value. Observed in 14 files (116 assignments).

Examples:

```text
"hullSystemUpgrade GUI_SHIPINFO_HULL add 150 0 hullSystem 50"
"hullSystemUpgrade0 GUI_SHIPINFO_HULL add 100 0 hullSystem 200"
"weaponSystemUpgrade GUI_SHIPINFO_WEAPON add 250 0 weaponSystem 100"
```

Uncertainty: The documented key should be unique, but repeated hullSystemUpgrade rows exist. Cost currency, multiplicative composition (e.g. 0.3), flag enforcement and toggle/special operands remain unknown.

Next investigation: Recover duplicate-key handling, cost currency, allowMultiple enforcement and the four operation formulas.

Evidence: [odf/ships/K_Brel_borg.odf:41](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_Brel_borg.odf:41>) — Full upgrade row grammar and unique-key requirement are documented in the immediately preceding block..

<a id="warpsuppressionradiusmultiplier"></a>

### `warpsuppressionradiusmultiplier`

**Spellings:** `warpSuppressionRadiusMultiplier`.

**Unresolved / low confidence**; role: ambiguous.

Sets a numeric modifier for warp-suppression radius on sensor/turret ODFs; the base radius and activation mechanism are unverified.

Value form: numeric literal. Units: unknown. Observed in 8 files (8 assignments).

Examples:

```text
1.0
0.3
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL parser or gameplay call site reads warpSuppressionRadiusMultiplier, and what does its value control?

Evidence: [odf/stations/sensor.odf:9](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/sensor.odf:9>) — observed assignment and local context; [odf/stations/turret.odf:3](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/turret.odf:3>) — observed assignment and local context.

<a id="weaponbreaknodes"></a>

### `weaponbreaknodes`

**Spellings:** `weaponBreakNodes`, `weaponbreaknodes`.

**Inferred / medium confidence**; role: asset.

Names model nodes broken away with weapon subsystem destruction.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 23 files (23 assignments).

Examples:

```text
"Weapons"
"Weapon"
"f_poseidon_Weapons"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Determine which damage/break event uses the list and whether it hides, detaches or selects model-node subtrees.

Evidence: [odf/ships/F_Akira.odf:134](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akira.odf:134>) — observed assignment and local context; [odf/ships/F_Akula.odf:155](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akula.odf:155>) — observed assignment and local context.

<a id="weapondamage"></a>

### `weapondamage`

**Spellings:** `weaponDamage`.

**Documented / high confidence**; role: gameplay.

Sets damage per tick for a successful phaser hit; other weapon classes may interpret this field differently.

Value form: numeric literal. Units: unknown. Observed in 269 files (269 assignments).

Examples:

```text
2.0
1.5
1.3
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Recover base-versus-modifier composition, the damage tick interval, and shield/hull application order.

Evidence: [odf/weapons/Phasers/fed_ent_phaser_1beam.odf:11](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/fed_ent_phaser_1beam.odf:11>) — assignment and nearby author comment; [odf/weapons/Phasers/fed_ent_phaser_3beam.odf:11](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Phasers/fed_ent_phaser_3beam.odf:11>) — assignment and nearby author comment.

<a id="weapondestructioneffect"></a>

### `weapondestructioneffect`

**Spellings:** `WeaponDestructionEffect`.

**Inferred / medium confidence**; role: presentation.

Names the visual effect played when the weapon subsystem is destroyed.

Value form: quoted string. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
"psexplosion_weapons"
"psexplosion_8"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred WeaponDestructionEffect behavior?

Evidence: [odf/ships/q_ent03_kcargo.odf:50](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/q_ent03_kcargo.odf:50>) — assignment and nearby author comment; [odf/ships/craft.odf:167](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/craft.odf:167>) — observed assignment and local context.

<a id="weapondestructionhardpoints"></a>

### `weapondestructionhardpoints`

**Spellings:** `WeaponDestructionHardpoints`.

**Inferred / medium confidence**; role: asset.

Lists model hardpoints where weapon subsystem destruction effects appear.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 120 files (125 assignments).

Examples:

```text
"hp16"
"hp36"
"hp37"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred WeaponDestructionHardpoints behavior?

Evidence: [odf/ships/F_Galaxy.odf:136](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Galaxy.odf:136>) — assignment and nearby author comment; [odf/ships/F_Nova.odf:138](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Nova.odf:138>) — observed assignment and local context.

<a id="weapondestructionsize"></a>

### `weapondestructionsize`

**Spellings:** `WeaponDestructionSize`.

**Inferred / medium confidence**; role: presentation.

Scales the visual effect for weapon subsystem destruction.

Value form: numeric literal. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
2.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred WeaponDestructionSize behavior?

Evidence: [odf/ships/q_ent03_kcargo.odf:51](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/q_ent03_kcargo.odf:51>) — observed assignment and local context.

<a id="weaponeffect"></a>

### `weaponeffect`

**Spellings:** `weaponEffect`.

**Unresolved / low confidence**; role: presentation.

An empty weapon-effect value is present; no visible effect or fallback semantics can be established.

Value form: quoted string. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
""
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL parser or gameplay call site reads weaponEffect, and what does its value control?

Evidence: [odf/weapons/Pulse/pulseo.odf:51](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Pulse/pulseo.odf:51>) — observed assignment and local context.

<a id="weaponeffect-size"></a>

### `weaponeffect_size`

**Spellings:** `weaponEffect_size`.

**Inferred / medium confidence**; role: presentation.

Sets a numeric scale for weaponEffect, but the paired effect name is empty in the sampled ODF.

Value form: numeric literal. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
10.0
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred weaponEffect_size behavior?

Evidence: [odf/weapons/Pulse/pulseo.odf:52](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Pulse/pulseo.odf:52>) — observed assignment and local context.

<a id="weaponmaxhitpoints"></a>

### `weaponmaxhitpoints`

**Spellings:** `weaponMaxHitPoints`.

**Inferred / medium confidence**; role: gameplay.

Sets maximum weapon subsystem hit points for a station; level mapping is not documented.

Value form: numeric literal with optional trailing semicolon. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
185;
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred weaponMaxHitPoints behavior?

Evidence: [odf/stations/F_TOS_starbase.odf:119](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/F_TOS_starbase.odf:119>) — observed assignment and local context.

<a id="weaponminhitpoints"></a>

### `weaponminhitpoints`

**Spellings:** `weaponMinHitPoints`.

**Inferred / medium confidence**; role: gameplay.

Sets minimum weapon subsystem hit points for a station; level mapping is not documented.

Value form: numeric literal with optional trailing semicolon. Units: unknown. Observed in 2 files (2 assignments).

Examples:

```text
145;
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred weaponMinHitPoints behavior?

Evidence: [odf/stations/F_TOS_starbase.odf:118](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/stations/F_TOS_starbase.odf:118>) — observed assignment and local context.

<a id="weaponpooldiminishrate"></a>

### `weaponpooldiminishrate`

**Spellings:** `weaponPoolDiminishRate`.

**Documented / high confidence**; role: gameplay.

Sets how slowly the ship’s weapons energy pool diminishes.

Value form: numeric literal. Units: unknown. Observed in 135 files (136 assignments).

Examples:

```text
1.1
1.5
1.85
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Recover pool units/capacity, update time base, energy-distribution influence and the exact debit/replenishment formula.

Evidence: [odf/ships/F_Stargazer.odf:61](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Stargazer.odf:61>) — assignment and nearby author comment; [odf/ships/K_Brel.odf:19](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/K_Brel.odf:19>) — assignment and nearby author comment.

<a id="weaponsenergyreplenishrate"></a>

### `weaponsenergyreplenishrate`

**Spellings:** `weaponsEnergyReplenishRate`.

**Documented / high confidence**; role: gameplay.

Sets how quickly the weapons energy pool is restored.

Value form: numeric literal. Units: unknown. Observed in 139 files (140 assignments).

Examples:

```text
0.65
1.3
0.75
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Recover pool units/capacity, update time base, energy-distribution influence and the exact debit/replenishment formula.

Evidence: [odf/ships/F_Apollo.odf:18](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Apollo.odf:18>) — assignment and nearby author comment; [odf/ships/F_Oberth.odf:19](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Oberth.odf:19>) — assignment and nearby author comment.

<a id="weapontargethardpoints"></a>

### `weapontargethardpoints`

**Spellings:** `WeaponTargetHardpoints`.

**Inferred / medium confidence**; role: asset.

Lists model hardpoints used when attacking or targeting the weapon subsystem.

Value form: quoted ODF/asset name or space-separated quoted names. Units: unknown. Observed in 118 files (118 assignments).

Examples:

```text
"hp16"
"hp36"
"hp37"
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Determine how hardpoint lists select aim points and handle missing names, visibility changes and destroyed subsystems.

Evidence: [odf/ships/F_Akira.odf:107](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Akira.odf:107>) — observed assignment and local context; [odf/ships/F_Nova.odf:132](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/ships/F_Nova.odf:132>) — observed assignment and local context.

<a id="weapontype"></a>

### `weapontype`

**Spellings:** `weaponType`.

**Inferred / medium confidence**; role: gameplay.

Classifies the special weapon as phaser, photon, or special; dispatch behavior is unverified.

Value form: observed ODF value: 'special'. Units: unknown. Observed in 5 files (5 assignments).

Examples:

```text
special
phaser
photon
```

Uncertainty: ODF-authored meaning only; Fleet Ops compatibility and live behavior remain unverified.

Next investigation: Which STL runtime parser or call site confirms the inferred weaponType behavior?

Evidence: [odf/weapons/Photons/f_tractor_bm.odf:9](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/f_tractor_bm.odf:9>) — observed assignment and local context; [odf/weapons/Photons/photon.odf:9](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/weapons/Photons/photon.odf:9>) — observed assignment and local context.

