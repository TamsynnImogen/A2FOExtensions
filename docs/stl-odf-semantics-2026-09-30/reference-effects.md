# Particles, explosions and chunks

See [README](README.md) for confidence definitions and [the index](command-index.md) for all families. Documented means authored ODF comments describe the intent; it does not mean tested runtime support.

<a id="alwaysvisible"></a>

### `alwaysvisible`

**Spellings:** `alwaysVisible`, `alwaysvisible`.

**Inferred / low confidence**; role: presentation.

Requests that this particle system stay visible, plausibly bypassing an ordinary visibility condition.

Value form: numeric flag 1. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
1
```

Uncertainty: No local comment defines the condition, lifetime, or culling effect.

Next investigation: Find a native reader or compare visible output with this flag toggled.

Evidence: [odf/Explosions/psphaser_hit1.odf:75](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psphaser_hit1.odf:75>) — assignment and adjacent ODF settings.

<a id="burstamount"></a>

### `burstamount`

**Spellings:** `burstAmount`.

**Documented / high confidence**; role: presentation.

Number of particles created at each launch.

Value form: numeric count; examples include integer-looking decimals. Units: particles per launch. Observed in 91 files (91 assignments).

Examples:

```text
100
200
1
```

Uncertainty: No rounding rule is established for values such as 3.0.

Next investigation: Determine whether fractional inputs are truncated or parsed as integers.

Evidence: [odf/Explosions/psfire.odf:44](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:44>) — direct authored comment and assignment.

<a id="burstlaunchradius"></a>

### `burstlaunchradius`

**Spellings:** `burstLaunchRadius`, `burstlaunchRadius`.

**Inferred / low confidence**; role: presentation.

Additional radius setting adjacent to burstAmount and launchRadius, likely affecting the spread of one burst.

Value form: decimal scalar. Units: unknown. Observed in 12 files (12 assignments).

Examples:

```text
.05
0.3
0
```

Uncertainty: Its distinction from launchRadius is undocumented; the two fields coexist.

Next investigation: Determine how burstlaunchRadius differs from launchRadius in the emitter.

Evidence: [odf/Explosions/psfire2.odf:45](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire2.odf:45>) — assignment and adjacent ODF settings.

<a id="databasename"></a>

### `databasename`

**Spellings:** `databaseName`.

**Documented / high confidence**; role: asset.

Names the M3D asset used by the explosion object.

Value form: quoted asset basename. Units: not applicable. Observed in 2 files (2 assignments).

Examples:

```text
"xplode"
```

Uncertainty: The comment does not establish resolution path or fallback.

Next investigation: Resolve xplode against the installed M3D assets and identify the explosion loader.

Evidence: [odf/Explosions/xMine.odf:8](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/xMine.odf:8>) — direct authored comment and assignment.

<a id="dependentparticlesystems"></a>

### `dependentparticlesystems`

**Spellings:** `dependentParticleSystems`.

**Documented / high confidence**; role: presentation.

Lists additional particle-system ODF names to fire with this particle system.

Value form: space-separated quoted names; an empty value also occurs. Units: not applicable. Observed in 40 files (40 assignments).

Examples:

```text
"psexplosion_sparks2" "psexplosion_blast_wide1"
"psasteroid_chunks_sm" "psasteroid_chunks_md" "psasteroid_chunks_lrg" "psexplosion_blast_flare5" "psexplosion_sparks3"
"psexplosion_blast_flare6"
```

Uncertainty: Launch order, attachment and failure handling are unknown; empty assignment may be intentional.

Next investigation: Check dependent-system launch order and meaning of an empty assignment.

Evidence: [odf/Explosions/pssupernova.odf:65](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/pssupernova.odf:65>) — direct authored comment and assignment.

<a id="emitterfollow"></a>

### `emitterfollow`

**Spellings:** `emitterfollow`.

**Documented / medium confidence**; role: presentation.

Controls how closely particles follow emitter motion, according to an authored comment.

Value form: decimal scalar, observed 0 through 1; one source has trailing semicolon. Units: unknown. Observed in 39 files (40 assignments).

Examples:

```text
0.95;
0.85
0.75
```

Uncertainty: Comment says follow exactly but provides 0.95; endpoints and whether it applies to existing particles are unknown.

Next investigation: Inspect parsing of the semicolon and test emitter motion at 0 and 1.

Evidence: [odf/other/psbeamsparks.odf:53](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/psbeamsparks.odf:53>) — direct authored comment and assignment; [odf/Explosions/psfire.odf:81](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:81>) — assignment and adjacent ODF settings.

<a id="enabletextureanimation"></a>

### `enabletextureanimation`

**Spellings:** `enableTextureAnimation`.

**Documented / high confidence**; role: presentation.

Enables or disables texture-frame animation for particle sprites.

Value form: numeric flag 0 or 1. Units: not applicable. Observed in 88 files (88 assignments).

Examples:

```text
1
0
```

Uncertainty: Frame rate and interaction with randomizeStartFrame are unspecified.

Next investigation: Check animation behavior with both flags combined.

Evidence: [odf/Explosions/psfire.odf:9](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:9>) — direct authored comment and assignment; [odf/Explosions/pssparks.odf:8](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/pssparks.odf:8>) — direct authored comment and assignment.

<a id="endcolor"></a>

### `endcolor`

**Spellings:** `endcolor`.

**Inferred / medium confidence**; role: presentation.

Specifies the ending RGB color for a smoke particle transition, paired with startcolor.

Value form: three whitespace-separated decimal components. Units: unknown. Observed in 4 files (4 assignments).

Examples:

```text
0.0 0.3 0.0
0.0 0.0 0.0
0.3 0.3 0.3
```

Uncertainty: No direct comment describes this field or its interaction with particleColor<N>.

Next investigation: Identify which emitter class consumes startcolor/endcolor and the transition timing.

Evidence: [odf/Explosions/psdamagedsmoke.odf:59](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psdamagedsmoke.odf:59>) — assignment and adjacent ODF settings.

<a id="expansionrate"></a>

### `expansionrate`

**Spellings:** `expansionRate`.

**Documented / high confidence**; role: presentation.

Sets the shockwave expansion rate.

Value form: numeric scalar. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
60
```

Uncertainty: The comment does not specify distance unit, time base, or whether growth is linear.

Next investigation: Find the shockwave expansion calculation or measure radius over time.

Evidence: [odf/Explosions/swMine.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/swMine.odf:5>) — direct authored comment and assignment.

<a id="explosionsoundgroup"></a>

### `explosionsoundgroup`

**Spellings:** `explosionSoundGroup`.

**Documented / high confidence**; role: asset.

Names the sound group used for the explosion.

Value form: quoted group name. Units: not applicable. Observed in 3 files (3 assignments).

Examples:

```text
"ST_GRP_EXP_Ship_Large"
```

Uncertainty: Selection within the group and playback timing remain unknown.

Next investigation: Trace the sound-group lookup for xMine.

Evidence: [odf/Explosions/xMine.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/xMine.odf:5>) — direct authored comment and assignment.

<a id="friction"></a>

### `friction`

**Spellings:** `friction`.

**Documented / high confidence**; role: presentation.

Controls the rate at which particles slow.

Value form: decimal scalar. Units: unknown. Observed in 121 files (121 assignments).

Examples:

```text
1
0.3
.7
```

Uncertainty: The scale and time dependence of deceleration are not documented.

Next investigation: Measure decay or identify the particle update formula.

Evidence: [odf/Explosions/psfire.odf:56](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:56>) — direct authored comment and assignment.

<a id="full3d"></a>

### `full3d`

**Spellings:** `full3D`.

**Documented / medium confidence**; role: presentation.

Requests arbitrary 3D particle orientation.

Value form: numeric flag 0 observed. Units: not applicable. Observed in 1 files (1 assignments).

Examples:

```text
0
```

Uncertainty: Only value 0 occurs in the sample; how 3D orientation differs from rotate is unclear.

Next investigation: Inspect an emitter with full3d=1 or identify the orientation reader.

Evidence: [odf/Explosions/psdebris.odf:50](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psdebris.odf:50>) — direct authored comment and assignment.

<a id="hostvelocityscale"></a>

### `hostvelocityscale`

**Spellings:** `hostVelocityScale`.

**Inferred / low confidence**; role: presentation.

Likely scales a host-velocity contribution to particle motion.

Value form: decimal scalar 1.2 observed. Units: unknown. Observed in 1 files (1 assignments).

Examples:

```text
1.2
```

Uncertainty: No comment or comparison isolates which velocity term is scaled.

Next investigation: Compare moving-host output with this field changed.

Evidence: [odf/Explosions/pscontrail_smoke3.odf:38](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/pscontrail_smoke3.odf:38>) — assignment and adjacent ODF settings.

<a id="launchdelay"></a>

### `launchdelay`

**Spellings:** `launchDelay`.

**Documented / high confidence**; role: presentation.

Delay before the first particle launch.

Value form: decimal scalar. Units: unknown. Observed in 121 files (121 assignments).

Examples:

```text
0.0
0.2
0
```

Uncertainty: No unit is stated; zero and fractional values occur.

Next investigation: Establish whether delay uses seconds or another clock.

Evidence: [odf/Explosions/psfire.odf:53](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:53>) — direct authored comment and assignment.

<a id="launchperiod"></a>

### `launchperiod`

**Spellings:** `launchPeriod`.

**Documented / high confidence**; role: presentation.

Time between particle launches.

Value form: decimal scalar, including zero. Units: unknown. Observed in 121 files (121 assignments).

Examples:

```text
0
0.05
5
```

Uncertainty: Zero-period behavior and precise time unit are not established.

Next investigation: Check emitter scheduling for launchPeriod=0.

Evidence: [odf/Explosions/psdebris_trail_fire.odf:46](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psdebris_trail_fire.odf:46>) — direct authored comment and assignment.

<a id="launchradius"></a>

### `launchradius`

**Spellings:** `launchRadius`.

**Documented / high confidence**; role: presentation.

Radius of the particle creation area.

Value form: decimal scalar. Units: unknown. Observed in 121 files (121 assignments).

Examples:

```text
0
0.3
0.05
```

Uncertainty: The origin, geometry, and distance unit are unspecified.

Next investigation: Identify spawn position calculation and relation to burstlaunchRadius.

Evidence: [odf/Explosions/psfire.odf:50](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:50>) — direct authored comment and assignment.

<a id="maxparticles"></a>

### `maxparticles`

**Spellings:** `maxParticles`.

**Documented / high confidence**; role: presentation.

Maximum number of particles existing at one time.

Value form: numeric count. Units: particles. Observed in 121 files (121 assignments).

Examples:

```text
1
100
200
```

Uncertainty: Behavior when a launch would exceed the maximum is not shown.

Next investigation: Determine whether excess launches are skipped or older particles removed.

Evidence: [odf/Explosions/psfire.odf:38](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:38>) — direct authored comment and assignment.

<a id="minsizeindex"></a>

### `minsize<N>`

**Spellings:** `minSize0`, `minSize1`, `minSize2`.

**Inferred / low confidence**; role: schema.

Indexed minimum-size parameter belonging to each chunkgen explosion entry.

Value form: indexed decimal scalar. Units: unknown. Observed in 302 files (587 assignments).

Examples:

```text
0.95
0.85
0.76
```

Uncertainty: The f_ds9_root sample has minSize0=0.77 but size0=0.48, so these must not be treated as a conventional numeric min/max pair without recovering the schema.

Next investigation: Find the chunk generator consumer and define minSize relative to size.

Evidence: [odf/chunkgen/f_ds9_root.odf:6](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/chunkgen/f_ds9_root.odf:6>) — assignment and adjacent ODF settings.

<a id="numchunksindex"></a>

### `numchunks<N>`

**Spellings:** `numChunks0`, `numChunks1`, `numChunks2`.

**Inferred / medium confidence**; role: schema.

Indexed count of chunks associated with each chunkgen explosion entry.

Value form: indexed integer count. Units: chunks, inferred. Observed in 302 files (587 assignments).

Examples:

```text
37
11
8
```

Uncertainty: No authored comment defines whether the value is an exact output count or generation target.

Next investigation: Inspect chunk generator output against numChunks values.

Evidence: [odf/chunkgen/f_ds9_root.odf:7](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/chunkgen/f_ds9_root.odf:7>) — assignment and adjacent ODF settings.

<a id="numexplosions"></a>

### `numexplosions`

**Spellings:** `numExplosions`.

**Inferred / medium confidence**; role: schema.

Declares how many indexed explosion entries follow in a chunkgen root.

Value form: integer count. Units: entries, inferred. Observed in 302 files (302 assignments).

Examples:

```text
2
3
1
```

Uncertainty: The observed groups use offset/size/minSize/numChunks, but missing-index behavior is unknown.

Next investigation: Check whether the reader iterates exactly numExplosions indices.

Evidence: [odf/chunkgen/f_ds9_root.odf:3](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/chunkgen/f_ds9_root.odf:3>) — assignment and adjacent ODF settings.

<a id="offsetindex"></a>

### `offset<N>`

**Spellings:** `offset0`, `offset1`, `offset2`.

**Inferred / medium confidence**; role: schema.

Three-component offset for an indexed chunkgen explosion entry.

Value form: indexed comma-separated three-decimal vector. Units: unknown. Observed in 302 files (579 assignments).

Examples:

```text
0.06,   0.63,  -0.48
0.25,  -0.03,  -0.76
-0.24,   0.34,  -0.69
```

Uncertainty: Coordinate frame and whether offsets are normalized are not established.

Next investigation: Identify the chunkgen coordinate origin and offset scale.

Evidence: [odf/chunkgen/f_ds9_root.odf:4](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/chunkgen/f_ds9_root.odf:4>) — assignment and adjacent ODF settings.

<a id="opacity"></a>

### `opacity`

**Spellings:** `opacity`.

**Documented / medium confidence**; role: presentation.

Sets overall particle opacity; an authored smoke comment describes partial visibility through it.

Value form: decimal scalar, including a value of 3. Units: unknown. Observed in 10 files (10 assignments).

Examples:

```text
0.5
0.75
3
```

Uncertainty: Values above 1 occur, so clamping or normalization cannot be assumed.

Next investigation: Determine rendering behavior for opacity greater than 1.

Evidence: [odf/Explosions/psfire_smoke.odf:80](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire_smoke.odf:80>) — direct authored comment and assignment.

<a id="particleangularvelocity"></a>

### `particleangularvelocity`

**Spellings:** `particleAngularVelocity`.

**Inferred / low confidence**; role: presentation.

Particle angular-velocity setting; in debris ODFs it appears alongside minimum and maximum random angular-velocity bounds.

Value form: numeric scalar. Units: unknown. Observed in 16 files (16 assignments).

Examples:

```text
2
3
20.0
```

Uncertainty: The bounds and base value can coexist, so their interaction is unclear.

Next investigation: Find the angular-velocity selection calculation when all three fields are present.

Evidence: [odf/Explosions/psasteroid_chunks_md.odf:36](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psasteroid_chunks_md.odf:36>) — direct authored comment and assignment; [odf/Explosions/psasteroid_chunks_sm.odf:36](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psasteroid_chunks_sm.odf:36>) — direct authored comment and assignment.

<a id="particleangularvelocitymax"></a>

### `particleangularvelocitymax`

**Spellings:** `particleAngularVelocityMax`.

**Inferred / medium confidence**; role: presentation.

Upper bound of a randomly chosen angular velocity, paired with particleAngularVelocityMin.

Value form: signed decimal scalar. Units: unknown. Observed in 99 files (99 assignments).

Examples:

```text
1
0
3
```

Uncertainty: The nearby comment describes the range collectively; distribution and relation to particleAngularVelocity are unclear.

Next investigation: Check random sampling and base angular-velocity use.

Evidence: [odf/Explosions/psfire.odf:35](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:35>) — assignment and adjacent ODF settings.

<a id="particleangularvelocitymin"></a>

### `particleangularvelocitymin`

**Spellings:** `particleAngularVelocityMin`.

**Documented / medium confidence**; role: presentation.

Lower bound of a randomly chosen angular velocity, paired with particleAngularVelocityMax.

Value form: signed decimal scalar. Units: unknown. Observed in 99 files (99 assignments).

Examples:

```text
-1
0
-3
```

Uncertainty: The comment covers the range, not the exact axis or angular unit.

Next investigation: Identify the angular unit and 3D rotation axis selection.

Evidence: [odf/Explosions/psfire.odf:34](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:34>) — direct authored comment and assignment.

<a id="particlecolorindex"></a>

### `particlecolor<N>`

**Spellings:** `particleColor0`, `particleColor1`, `particleColor2`.

**Documented / high confidence**; role: presentation.

Indexed RGB colors establish a particle color transition over its lifetime.

Value form: indexed whitespace-separated RGB triples. Units: unknown. Observed in 93 files (240 assignments).

Examples:

```text
1 1 1
0 0 0
.5 .5 1
```

Uncertainty: One observed triple is malformed (.60.36 .28); interpolation and accepted ranges are not proven.

Next investigation: Check malformed color parsing and transition interpolation.

Evidence: [odf/Explosions/psfire.odf:68](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:68>) — direct authored comment and assignment.

<a id="particlecolorinterpolant"></a>

### `particlecolorinterpolant`

**Spellings:** `particleColorInterpolant`.

**Documented / medium confidence**; role: presentation.

Selects the rate or curve of transition between particle colors; value 5 is annotated fast ramp up.

Value form: integer code 5 observed. Units: not applicable. Observed in 5 files (5 assignments).

Examples:

```text
5
```

Uncertainty: An authored comment says only two colors can be supplied with an interpolant; other codes are undocumented.

Next investigation: Identify supported interpolant codes and verify the two-color limit.

Evidence: [odf/Explosions/psselfdestruct.odf:64](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psselfdestruct.odf:64>) — direct authored comment and assignment.

<a id="particlecolorvariance"></a>

### `particlecolorvariance`

**Spellings:** `particleColorVariance`.

**Documented / medium confidence**; role: presentation.

Adds variation to a spawned particle’s RGB color.

Value form: three decimal components; malformed .2.1 0 also occurs. Units: unknown. Observed in 23 files (23 assignments).

Examples:

```text
.2.1 0
.05 .05 .04
.2 0 0
```

Uncertainty: Distribution, sign convention and handling of malformed syntax are unknown.

Next investigation: Check the parser and sampled color distribution.

Evidence: [odf/Explosions/psfire.odf:73](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:73>) — direct authored comment and assignment.

<a id="particledirectionspread"></a>

### `particledirectionspread`

**Spellings:** `particleDirectionSpread`.

**Documented / medium confidence**; role: presentation.

Controls spread away from the base emission direction; a comment maps 0 to directly behind and 1 to all directions.

Value form: decimal scalar; values above 1 occur. Units: unknown. Observed in 41 files (41 assignments).

Examples:

```text
1
1.3
0.01
```

Uncertainty: The 0-to-1 description may be local to a trail; values 1.3 occur elsewhere.

Next investigation: Determine behavior above 1 and the reference direction for each emitter.

Evidence: [odf/other/psartiltrail.odf:25](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/psartiltrail.odf:25>) — direct authored comment and assignment; [odf/other/psbeamsparks.odf:35](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/psbeamsparks.odf:35>) — direct authored comment and assignment.

<a id="particlegrowthinterpolant"></a>

### `particlegrowthinterpolant`

**Spellings:** `particleGrowthInterpolant`.

**Documented / low confidence**; role: presentation.

Selects a particle growth curve, but the authored code labels conflict: 1 is called sinusoidal, 5 natural logarithmic, and 2 is called both sinusoidal and exponential in different files.

Value form: integer code. Units: not applicable. Observed in 98 files (98 assignments).

Examples:

```text
1
5
6
```

Uncertainty: Value 2 has contradictory authored comments; the code-to-curve mapping and exact formulas cannot be resolved from ODF text.

Next investigation: Resolve the conflicting value-2 labels from the native curve reader or controlled visuals.

Evidence: [odf/Explosions/pssupernova.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/pssupernova.odf:5>) — direct authored comment and assignment; [odf/Explosions/pssupernova_dust.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/pssupernova_dust.odf:5>) — direct authored comment and assignment; [odf/Explosions/psexplosion_sparks.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psexplosion_sparks.odf:5>) — direct authored comment and assignment; [odf/Explosions/psfire_smoke.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire_smoke.odf:5>) — value 2 is annotated exponential, contradicting the sparks file.

<a id="particlegrowthrate"></a>

### `particlegrowthrate`

**Spellings:** `particleGrowthRate`.

**Documented / high confidence**; role: presentation.

Rate at which a particle grows or shrinks.

Value form: signed or unsigned decimal scalar. Units: unknown. Observed in 121 files (121 assignments).

Examples:

```text
0.20
0
.5
```

Uncertainty: No unit, sign rule or relationship to start size is documented.

Next investigation: Identify the size update formula and negative-rate behavior.

Evidence: [odf/Explosions/psfire.odf:3](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:3>) — direct authored comment and assignment.

<a id="particleinfinitelifetime"></a>

### `particleinfinitelifetime`

**Spellings:** `particleInfiniteLifetime`.

**Documented / high confidence**; role: presentation.

Requests particles that never expire until explicitly removed.

Value form: numeric flag 1. Units: not applicable. Observed in 3 files (3 assignments).

Examples:

```text
1
```

Uncertainty: Removal trigger and interaction with maxParticles are unknown.

Next investigation: Find removal conditions for infinite-lived particles.

Evidence: [odf/Explosions/pssupernova.odf:11](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/pssupernova.odf:11>) — direct authored comment and assignment.

<a id="particlelifetime"></a>

### `particlelifetime`

**Spellings:** `particleLifetime`.

**Documented / high confidence**; role: presentation.

Sets particle lifetime.

Value form: decimal scalar. Units: unknown. Observed in 121 files (121 assignments).

Examples:

```text
1
2
1.5
```

Uncertainty: The file does not state a time unit.

Next investigation: Verify lifetime clock and interaction with variance.

Evidence: [odf/Explosions/psfire.odf:16](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:16>) — direct authored comment and assignment.

<a id="particlelifetimevariance"></a>

### `particlelifetimevariance`

**Spellings:** `particleLifetimeVariance`.

**Documented / medium confidence**; role: presentation.

Adds random additional particle lifetime.

Value form: nonnegative decimal scalar. Units: unknown. Observed in 121 files (121 assignments).

Examples:

```text
0
0.01
0.5
```

Uncertainty: Distribution and whether additional means strictly positive are unverified.

Next investigation: Measure or inspect the variance sampling rule.

Evidence: [odf/Explosions/psfire.odf:19](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:19>) — direct authored comment and assignment.

<a id="particlesize"></a>

### `particlesize`

**Spellings:** `particleSize`.

**Documented / high confidence**; role: presentation.

Sets particle size.

Value form: decimal scalar. Units: unknown. Observed in 119 files (119 assignments).

Examples:

```text
.6
1.0
0.005
```

Uncertainty: Geometry and scale relative to the host are not documented.

Next investigation: Measure rendered dimensions for a known particleSize.

Evidence: [odf/Explosions/psfire.odf:22](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:22>) — direct authored comment and assignment.

<a id="particlesizevariance"></a>

### `particlesizevariance`

**Spellings:** `particleSizeVariance`.

**Documented / medium confidence**; role: presentation.

Adds random additional particle size.

Value form: nonnegative decimal scalar. Units: unknown. Observed in 121 files (121 assignments).

Examples:

```text
0
0.4
0.00133
```

Uncertainty: Distribution and interaction with particleStartSize are unknown.

Next investigation: Inspect spawn-size sampling with and without particleStartSize.

Evidence: [odf/Explosions/psfire.odf:25](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:25>) — direct authored comment and assignment.

<a id="particlestartsize"></a>

### `particlestartsize`

**Spellings:** `particleStartSize`.

**Documented / medium confidence**; role: presentation.

Sets the size at which particles start.

Value form: decimal scalar. Units: unknown. Observed in 81 files (81 assignments).

Examples:

```text
0
5
3
```

Uncertainty: Relationship to particleSize and growthRate is undocumented.

Next investigation: Determine precedence when particleStartSize and particleSize coexist.

Evidence: [odf/Explosions/pssparks.odf:24](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/pssparks.odf:24>) — direct authored comment and assignment.

<a id="particleuniformcolorvariance"></a>

### `particleuniformcolorvariance`

**Spellings:** `particleUniformColorVariance`.

**Documented / medium confidence**; role: presentation.

Restricts color variation to a single component, described in one ODF as the maximum.

Value form: numeric flag 0 or 1. Units: not applicable. Observed in 22 files (23 assignments).

Examples:

```text
0
1
```

Uncertainty: The phrase single component (the maximum) does not say whether one RGB channel changes or one variance scalar is shared across channels; the formula is not recovered.

Next investigation: Recover how the maximum variance component is selected and applied to the RGB channels.

Evidence: [odf/Explosions/psfire.odf:76](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:76>) — direct authored comment and assignment.

<a id="particlevelocity"></a>

### `particlevelocity`

**Spellings:** `particleVelocity`.

**Documented / high confidence**; role: presentation.

Sets particle velocity.

Value form: decimal scalar. Units: unknown. Observed in 121 files (121 assignments).

Examples:

```text
0
0.0
0.2
```

Uncertainty: Direction and motion unit are unspecified.

Next investigation: Determine velocity unit and base-direction combination.

Evidence: [odf/Explosions/psfire.odf:28](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:28>) — direct authored comment and assignment.

<a id="particlevelocityvariance"></a>

### `particlevelocityvariance`

**Spellings:** `particleVelocityVariance`.

**Documented / medium confidence**; role: presentation.

Adds random particle velocity.

Value form: nonnegative decimal scalar. Units: unknown. Observed in 121 files (121 assignments).

Examples:

```text
0
0.2
0.1
```

Uncertainty: Distribution and whether variation can lower speed are unclear.

Next investigation: Inspect speed sampling around particleVelocity.

Evidence: [odf/Explosions/psfire.odf:31](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:31>) — direct authored comment and assignment.

<a id="randomizestartframe"></a>

### `randomizestartframe`

**Spellings:** `randomizeStartFrame`.

**Documented / high confidence**; role: presentation.

Randomizes the initial texture frame; with texture animation disabled, a comment says this randomizes the chosen texture.

Value form: numeric flag 0 or 1. Units: not applicable. Observed in 88 files (88 assignments).

Examples:

```text
0
1
```

Uncertainty: The frame selection distribution and atlas/list interaction are unknown.

Next investigation: Check random frame choice for animated and static sprites.

Evidence: [odf/Explosions/psfire.odf:13](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:13>) — direct authored comment and assignment; [odf/Explosions/pssparks.odf:12](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/pssparks.odf:12>) — direct authored comment and assignment.

<a id="randomseed"></a>

### `randomseed`

**Spellings:** `RandomSeed`, `randomSeed`.

**Documented / medium confidence**; role: presentation.

Supplies an integer seed for repeatable random particle or nebula settings.

Value form: integer. Units: not applicable. Observed in 56 files (56 assignments).

Examples:

```text
10
4
0
```

Uncertainty: A consistent-seed comment exists, but scope of the random stream is unspecified.

Next investigation: Identify which random generator reads randomSeed in each class.

Evidence: [odf/Explosions/psexplosion_major.odf:72](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psexplosion_major.odf:72>) — direct authored comment and assignment.

<a id="rotate"></a>

### `rotate`

**Spellings:** `rotate`.

**Documented / medium confidence**; role: presentation.

Randomizes particle initial orientation; an authored comment calls the result random but fixed.

Value form: numeric scalar including 0, 0.25 and 1. Units: unknown. Observed in 104 files (104 assignments).

Examples:

```text
1
0.25
0
```

Uncertainty: Fractional values occur, so boolean-only semantics are unsafe; rotation axis is unclear.

Next investigation: Determine how rotate=0.25 differs from 1 and whether later rotation is separate.

Evidence: [odf/Explosions/psfire.odf:65](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:65>) — direct authored comment and assignment; [odf/Explosions/pssmoke.odf:41](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/pssmoke.odf:41>) — direct authored comment and assignment.

<a id="seed"></a>

### `seed`

**Spellings:** `seed`.

**Inferred / low confidence**; role: schema.

Seed value in generated chunkgen roots, plausibly making generated chunk patterns repeatable.

Value form: integer. Units: not applicable. Observed in 287 files (287 assignments).

Examples:

```text
65
73
12
```

Uncertainty: The comment says generated using random defaults but does not define what seed controls.

Next investigation: Find the chunk generator consumer and test identical seeds.

Evidence: [odf/chunkgen/f_ds9_root.odf:2](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/chunkgen/f_ds9_root.odf:2>) — direct authored comment and assignment.

<a id="sizeindex"></a>

### `size<N>`

**Spellings:** `size0`, `size1`, `size10`, `size11`, `size12`, `size13`, `size14`, `size15`, `size16`, `size17`, `size18`, `size2`, `size3`, `size6`, `size7`, `size8`, `size9`.

**Inferred / medium confidence**; role: ambiguous.

Indexed size field used both for a chunkgen explosion entry and for a lens-flare sprite entry.

Value form: indexed decimal scalar. Units: unknown. Observed in 288 files (581 assignments).

Examples:

```text
0.52
0.67
0.29
```

Uncertainty: The shared lexical family spans different owning classes; size semantics and scale may differ.

Next investigation: Identify separate chunkgen and lens-flare readers and their size scales.

Evidence: [odf/other/lens_flare.odf:16](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/other/lens_flare.odf:16>) — assignment and adjacent ODF settings; [odf/chunkgen/f_ds9_root.odf:5](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/chunkgen/f_ds9_root.odf:5>) — indexed chunkgen size assignment beside offset0, minSize0, and numChunks0.

<a id="spritelist"></a>

### `spritelist`

**Spellings:** `spriteList`.

**Documented / high confidence**; role: asset.

Names candidate sprites; one is selected randomly when each particle is created.

Value form: one or more space-separated quoted sprite names. Units: not applicable. Observed in 121 files (121 assignments).

Examples:

```text
"damagemelt"
"explosionb"
"asteroid1" "asteroid3" "asteroid4" "asteroid5"
```

Uncertainty: Fallback for missing sprites and selection weighting are unknown.

Next investigation: Check asset lookup and random-choice distribution.

Evidence: [odf/Explosions/psfire.odf:62](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psfire.odf:62>) — direct authored comment and assignment.

<a id="startcolor"></a>

### `startcolor`

**Spellings:** `startcolor`.

**Inferred / medium confidence**; role: presentation.

Specifies the starting RGB color for a smoke particle transition, paired with endcolor.

Value form: three whitespace-separated decimal components. Units: unknown. Observed in 3 files (3 assignments).

Examples:

```text
0.0 0.6 0.0
0.5 0.5 0.5
0.6 0.6 0.6
```

Uncertainty: No direct comment explains interpolation or relation to particleColor<N>.

Next investigation: Identify the reader and transition timing for startcolor/endcolor.

Evidence: [odf/Explosions/psdamagedsmoke.odf:58](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psdamagedsmoke.odf:58>) — assignment and adjacent ODF settings.

<a id="stretch"></a>

### `stretch`

**Spellings:** `stretch`.

**Documented / medium confidence**; role: presentation.

Stretches sprites along their velocity vector to simulate motion blur.

Value form: decimal scalar, including zero and values above 1. Units: unknown. Observed in 14 files (14 assignments).

Examples:

```text
.1
2
1
```

Uncertainty: The value scale and distinction from stretchFactor are not stated.

Next investigation: Inspect sprite aspect ratio for stretch values 0, 1 and 2.

Evidence: [odf/Explosions/psflash.odf:48](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psflash.odf:48>) — direct authored comment and assignment.

<a id="stretchfactor"></a>

### `stretchfactor`

**Spellings:** `stretchFactor`.

**Documented / medium confidence**; role: presentation.

A time-related factor for velocity-aligned sprite stretching.

Value form: decimal scalar. Units: unknown. Observed in 14 files (14 assignments).

Examples:

```text
0.09
2
0.1
```

Uncertainty: The authored comment says time but does not define an actual unit or formula.

Next investigation: Find stretch calculation involving stretchFactor.

Evidence: [odf/Explosions/psflash.odf:51](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psflash.odf:51>) — direct authored comment and assignment.

<a id="timetofadein"></a>

### `timetofadein`

**Spellings:** `timetofadein`.

**Inferred / medium confidence**; role: presentation.

Likely duration of particle-system fade-in, paired with timetofadeout.

Value form: decimal scalar 0.5 observed. Units: unknown. Observed in 6 files (6 assignments).

Examples:

```text
0.5
```

Uncertainty: The shared preceding comment says fade in and out times; the time unit is unspecified.

Next investigation: Determine whether fade-in applies per particle or to the whole system.

Evidence: [odf/Explosions/psdamagedsmoke.odf:56](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psdamagedsmoke.odf:56>) — assignment and adjacent ODF settings.

<a id="timetofadeout"></a>

### `timetofadeout`

**Spellings:** `timetofadeout`.

**Inferred / medium confidence**; role: presentation.

Likely duration of particle-system fade-out; paired smoke settings are described as fade in and out times.

Value form: decimal scalar. Units: unknown. Observed in 23 files (23 assignments).

Examples:

```text
0.3
.3
.2
```

Uncertainty: Other occurrences lack a direct comment, and fade trigger/time unit are unknown.

Next investigation: Determine fade-out trigger and whether it applies per particle or system.

Evidence: [odf/Explosions/pshull_chunks.odf:75](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/pshull_chunks.odf:75>) — direct authored comment and assignment; [odf/Explosions/psasteroid_chunks_lrg.odf:74](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psasteroid_chunks_lrg.odf:74>) — direct authored comment and assignment.

<a id="trackhost"></a>

### `trackhost`

**Spellings:** `trackHost`.

**Documented / medium confidence**; role: presentation.

Requests tracking of the host object for this particle system.

Value form: numeric flag 1. Units: not applicable. Observed in 9 files (9 assignments).

Examples:

```text
1
```

Uncertainty: Distinction from emitterfollow and tracking after host destruction is unknown.

Next investigation: Compare trackHost and emitterfollow behavior during host motion/destruction.

Evidence: [odf/Explosions/psdebris_trail_smoke.odf:78](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psdebris_trail_smoke.odf:78>) — direct authored comment and assignment.

<a id="velocityregulatesemission"></a>

### `velocityregulatesemission`

**Spellings:** `velocityRegulatesEmission`.

**Documented / medium confidence**; role: presentation.

Makes host speed regulate particle emission rate, as stated in a trail ODF.

Value form: numeric flag 1. Units: not applicable. Observed in 2 files (2 assignments).

Examples:

```text
1
```

Uncertainty: No speed-to-rate mapping, zero-speed behavior, or normalization is given.

Next investigation: Measure particle launch rate across host speeds.

Evidence: [odf/Explosions/psdebris_trail_fire.odf:82](</home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/STL/odf/Explosions/psdebris_trail_fire.odf:82>) — direct authored comment and assignment.

