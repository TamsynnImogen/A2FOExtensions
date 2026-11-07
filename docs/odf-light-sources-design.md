# ODF light sources and per-light shadows

Status: staged owned native11 configuration/submission and gated core pipeline, stage 20,
2026-11-03, plus four private static Refit colour-light controls on 2026-11-06.
The parser, copied class-ODF producer, pinned world-list sampler, source assembly
and gated native pipeline pass host and private GPU checks. Directional and point
ODF intensity 3-versus-0 pairs show static receiver colour changes on Vulkan
and DX12 software-GPU paths. All four pairs have independent review. These results are **not a deployed game feature**. Native
ambient and complete offscreen caster/visibility capture remain open.
The installed shadow prototype is unchanged. See
[the renderer checklist](renderer/native11-progress.md) and
[stage 20 evidence](renderer/native11-migration.md#stage-20-core-offscreen-pipeline-completeness-and-publication-gates).

## Requested behaviour

An ODF can make an object a light source. Directional sources provide sunlight
across the scene; point sources illuminate nearby objects in every direction;
spot sources illuminate a cone (the requested "arc" behaviour). Nebulas can
provide local coloured illumination. Every source needs colour and a separate
intensity control. Objects between a source and a receiver block that source's
direct illumination; moving or rotating either object must update the result.

A sun's global directional approximation and a nearby finite point light are
different choices. A nebula's first implementation can use a local point light;
that is an approximation of an extended glowing volume, not volumetric light
transport or a physically soft area source.

## Compatibility baseline

The recovered Fleet Operations callback creates a registered point light when
native `lightColor` is present, even with `lightSource = 0`. A private probe of
the installed image observed a native point for a `lightSource = 1`/`lightColor`
source; `lightColor` alone has not yet been separately proved in that image.
Refit conservatively rejects both cases to avoid double emission.
Keep native object/ordnance lighting distinct from the opt-in Refit namespace.

Evidence: the recovered `GameObjectClassCtor2Callback` in
`reverse_engineering/FleetOpsHook/rebuild/src/hooks/game_object_class.cpp`
creates an ordinary point light when the class has no light and `lightColor`
is present. It reads falloff start/range with defaults 100 and 5. The bundled
FO readme lists these commands for GameObjects; staged photon-torpedo ODFs
also contain them. This is recovered-source evidence for the `lightColor`-only
path; the private installed-image probe established the paired
`lightSource = 1`/`lightColor` result. The native falloff formula and units
still need confirmation against the target.

The existing copied class-ODF callback can supply inherited configuration.
New native light registration, object pose/lifetime, visibility, and all
supported object classes need separate verification. A filename or unit name
such as `dlight.odf` is not proof of its actual light type.

## Legacy parser contract and future shadow design

The old field family remains parsed for native11 compatibility. An enabled old
`lightSource` is marked as a legacy native source; current Refit refuses it
until native ownership can be deduplicated. The shadow behaviour below is a
future design, not current Refit output.

| Command | Meaning |
| --- | --- |
| `lightSource = 1` | Observed with `lightColor`, FO creates a point light. Current Refit refuses enabled legacy sources to prevent double lighting. |
| `lightType = "directional"` | `directional`, `point`, or `spot`. |
| `lightColor = "1.0 0.95 0.85"` | Existing FO RGB name, with nonnegative finite components. Quote the vector for actual ODF string capture. |
| `lightIntensity = 1.0` | Nonnegative finite brightness multiplier. 0 emits no light; 1 leaves the specified colour at its base strength; 2 doubles it. |
| `lightDirection = "0.0 -1.0 0.0"` | Ray travel direction for directional/spot sources. Quote the vector for actual ODF string capture. Object-local direction rotates with its source. Exact native axis conversion must be verified. |
| `lightFalloffStart = 100.0` | Existing FO name for the full-strength distance of a local source. Ignore for directional light. |
| `lightFalloffRange = 500.0` | Existing FO name for the following fade distance. Ignore for directional light. |
| `lightConeInner = 30.0` | Spot's full cone angle in degrees, at full angular strength. |
| `lightConeOuter = 60.0` | Spot's full outer angle in degrees; fade between inner and outer, zero outside. Require 0 < inner <= outer < 180. |
| `lightCastShadows = 1` | Requested future per-source occlusion; current Refit only diagnoses shadow requests. |

Treat colour times intensity as source energy before distance/cone attenuation
and visibility. Intensity does not change hue or range. Keep source intensity
separate from the prototype's `[Shadows] Strength`, which is only an overlay
darkness setting. Missing/malformed fields, unknown types, and invalid values
need bounded diagnostics and must not partially register a source.

For an omnidirectional nebula glow use `point` with falloff; for a searchlight
use `spot` with direction, falloff, and both cone angles. A visible emissive
texture alone must not implicitly create a light.

## Current Refit integration

The private Refit trial requires both `[Refit] OdfLights = 1` and
`[Native11] CaptureWorldLights = 1`. It requests the ten `a2foLight*` aliases
alongside the legacy fields from the copied class ODF. If any known prefixed
field is present, the parser uses only that family. An enabled
`a2foLightSource = 1` also requires explicit native `lightSource = 0` and no
recognized legacy `lightColor` field, including an inherited one. Other
legacy type and falloff values do not fill missing prefixed values. This
prevents FO from creating a native point for the same opted-in source.

In the actual ODF, type, colour, and direction must be quoted so ParameterDB
captures them as strings. Scalar numbers remain plain. For `fed_venture.odf`
(class basename `fed_venture`), the private directional colour-source example is:

```ini
#include "fed_venture.odf"
basename = "fed_venture"
lightSource = 0
a2foLightSource = 1
a2foLightType = "directional"
a2foLightColor = "1 0 0"
a2foLightIntensity = 3
a2foLightDirection = "0 -1 0"
a2foLightCastShadows = 0
```

Only directional and point **colour** lights are bridged into ABI2, for scoped
eligible mesh draws. Spots/cones and geometry shadows are unsupported in this
path. A shadow request is diagnostic only; it does not attest occlusion or
shadow geometry. Four static private on/off controls now support narrow
directional/point colour response on both software-GPU backends: 4,828 and
4,905 directional receiver pixels, and 1,976 and 1,975 point pixels, have
red on-minus-off increase of at least 20 for Vulkan and DX12 respectively.
The directional summary is `build/refit-odf-directional-control-results.json`;
individual analyses are under `build/refit/light-controls-vk-dir-1793984111079143360/`,
`light-controls-dx12-dir-1793984389096129977/`,
`light-controls-vk-point-1793984626764814072/` and
`light-controls-dx12-point-long-1793985094097082647/`.
All four have independent review, including DX12 point at `build/refit/light-controls-dx12-point-long-1793985094097082647/independent-review.md`.
Movement, range/falloff, material response, physical shadows, hardware and
gameplay qualification remain open.

## Renderer requirements for the future shadow-enabled path

1. Use the same resolved source pose, direction, colour, attenuation, and cone
   for illumination and its shadow projection. Verify native direction sign
   and object-to-world transforms against the real lighting path first.
2. Apply occlusion to each source's direct contribution. Other lights,
   ambient light, and emissive materials remain visible. A black overlay on
   the finished image cannot supply this behaviour.
3. Use an appropriate projection: directional shadow map, perspective cone
   shadow map, and all-around coverage for point sources. Never approximate a
   point source with one arbitrary directional map.
4. Include relevant blockers even when outside the camera view. Preserve
   native depth, stencil, render state, object lifetime, and simulation.
   Do not replay gameplay or make hidden simulation-affecting light objects.
5. Prevent double lighting when an opted-in source already has a native FO
   light. Bound light count and shadow work with explicit diagnostics. Define
   unsupported renderer-route behaviour before deployment.

Acceptance needs independent light-on/off and intensity checks; opposite
directions on a fixed scene; moved/rotated spotlight and cutoff checks; a
receiver with blockers toward and away from the source; point-light blockers
on every side; two differently coloured sources where only one is occluded;
unaffected emissives/ambient; offscreen blockers; and native depth/MSAA/state
preservation. Source/configuration tests alone do not prove in-game lighting.
