# Nexus in-game shadow prototype

This update adds opt-in shadows to native Fleet Operations gameplay. It does
not change the menu scene renderer. Visual acceptance in the game is required.

Current installed stability update: **world-bias audit 4**, core SHA-256
`0b7f7af5bbd3946348595772ceeff399d95615ced75306718ebb5e349ebf3e21`.
This retains audit 3's native-depth preservation; see the final section below.

## Configuration

`A2FORenderer.ini` is read in increasing precedence: Data, oldest parent mod,
then the selected mod. These keys belong to `[Shadows]`:

| Key | Default | Accepted values |
| --- | --- | --- |
| Enabled | 0 | 0 or 1 |
| Size | 1024 | 512, 1024, 2048 or 4096 |
| MaxDraws | 2048 | 1 through 2048 |
| Strength | 0.55 | 0 through 0.8 |
| Bias | 0.0015 | 0 through 0.02 |
| BiasWorld | -1 | -1 selects legacy Bias; 0 through 100 uses fixed world-space units |

When `BiasWorld` is nonnegative it takes precedence over `Bias`. It is divided
by the fitted light depth range before the comparison, so unrelated distant
geometry cannot increase the effective world separation. The Nexus audit-4
candidate uses `BiasWorld=0.25`; other child overrides retain legacy behaviour
unless they explicitly choose a world bias.

Invalid inherited settings disable this feature. Module selection and managed
DXVK safety checks remain authoritative. The Nebula renderer module must be
enabled and the active DXVK payload must match the managed payload. This
feature does not activate a renderer backend or change graphics preferences.
Settings apply at process startup. The Nexus candidate override is
`mods/FO4Nexus/probes/ingame-shadows/A2FORenderer.ini`.

## Rendering and ownership

The supported ArmadaL main-view CALL at RVA `0x0019886e` must match all five
original bytes (`e8 9d f8 ff ff`) before patching. The wrapper forwards the
original cdecl three-argument world body at RVA `0x00198110` exactly once.
Inset views retain their original route. Native submission determines which
objects are visible; no extra object traversal reveals hidden geometry.

The existing MeshVB bridge supplies CPU vertex/index mirrors. The capture
copies validated triangle-list positions from uncloaked Craft draws that
write depth and have no cutout or transparent material state. Snapshots own
their triangles. Native GPU buffers are never locked, retained for replay or
modified. A view is capped at 786432 snapshot vertices and the configured
draw count; exceeding either aborts shadows for the view.

The brightest positive native directional light supplies the direction. A
fitted, quantized and texel-snapped projection renders an R32F depth map. A
3-by-3 PCF receiver pass uses original clip matrices and native depth with
depth writes disabled. A black alpha overlay modifies RGB and preserves
target alpha. Device reset/loss releases resources through existing hooks.
Unsupported capabilities, invalid snapshots or changed camera/targets skip
the pass. A D3D9 ALL state block and explicit target/depth/viewport saves
restore native state. D3DCompile is loaded dynamically. No simulation state
or multiplayer rules change.

## Limits and acceptance

This is post-light darkening, rather than Refit's per-key-light material
lighting. It can darken emissive contribution and translucent effects already
composited over a hull. Cutouts, cloaked hulls, M3D/general CPU meshes, planets
and nebula shadows are excluded. Offscreen casters are unavailable. Large
visible fleets share one fitted map and can lose shadow detail.

Deriving world coordinates from native clip constants assumes the fixed D3D9
view/projection describe the same camera. Refit's transform path and native
shader captures support the algebra; source/headless fixtures do not prove
this relationship in a live Fleet Operations frame. Alignment while moving
and zooming remains a manual acceptance gate.

The host math fixture covers conventions, inverses, finite inputs, bounds,
quantization and snapping. The native lifecycle fixture checks invalid inputs,
COM rejection and SM3 compilation. The GPU fixture renders a caster/receiver
on a private display, reads shadow/lit pixels, mutates source data to check
snapshot ownership, and checks state restoration. Record actual execution
and production build outcomes in the deployment receipt, not compilation alone.

After a complete restart, load Nexus and an ordinary skirmish. Inspect opaque
ships overlapping in the light direction, or near an opaque station; move and
zoom the camera. Check the HUD, selection, cloak, firing effects and return to
menus. Preserve fresh logs before restarting. `A2FOExtensions.log` reports
`In-game shadows:` with seen/captured counts, status and skip reason for the
first 12 views and every 300th view. `drawn` proves submission, not placement.

Set `[Shadows] Enabled=0` in the Nexus override and restart to disable the pass.
Keep the previous core DLL backup for full binary rollback.

## Verified deployment

Installed on 2026-11-01, with core SHA-256
`c7d143c2840fc03e99a7b837a5f3db7742f18a5a8ed43a45ae2fc8cc8843c409`.
Only the core DLL and the new Nexus renderer override were written. All 192
existing Nexus files and 221 protected installation files matched their
pre-deployment hashes. The existing 0.1.1 interface remains installed.

Host math, strict i686 compilation, shader compilation, GPU readback/state
checks, and actual DLL fake-host lifecycle checks passed. The private Xvfb
fixture used an isolated PortProton Wine prefix and the exact installed
managed DXVK DLL (`00bc142e98ddad1e334a4d88fa630038e092b0758b3d79e295172b181a4c772d`).
Readback was shadow=102, lit=255, caster=255, alpha=255 at all three probes.
Hostile inherited states, shader/declaration, stream/index, targets, depth and
viewport were restored. A receiver-plane PCF correction removes false shadow
darkening on the fixture's sloped caster plane. The fake host deliberately
fails the game-image check and installs no hooks; it proves DLL startup and
failure/retry lifecycle, not in-game execution.

All 197 previously installed export names and ordinals remain available.
The current source tree also supplies `A2FO_InitializeNoUnwind` at ordinal 198;
this existing additive API is included. All 11 imported DLLs and their imported
symbol sets match the prior installed core. The Rust-owned build receipt
verified 703 input files without drift. The build explicitly reused 295
validated mission-runtime objects and the installed media DLL, with hashes
recorded separately; those components were not freshly rebuilt. All 27 MMM
objects compiled during the initial partial rebuild matched the reused set.
Historical full MMM source/header drift was not established by the old
object receipts, so no fresh MMM source-build claim is made.

Evidence is in `build/ingame-shadows/verification.json`,
`paired-runtime-reuse.json`, and `private-fixtures-complete.log`.
The backup and deployment receipt are in
`build/ingame-shadows/deployment-20261101T214040Z/`.
To restore the previous core and remove this unchanged shadow override, exit
Fleet Operations and run `python3 build/ingame-shadows/rollback.py` from the
repository. The rollback refuses to overwrite later core or config changes.
Actual game integration, camera alignment, visual quality and performance
remain pending a full-restart manual skirmish test.

Runtime follow-up on 2026-11-01: Immy reports visible shadows in Nexus. The
separate `Mods/test` child (STA2 Classic: Test, parent STA2 Classic Mod) had
the renderer module and managed DXVK active, but no inherited `[Shadows]`
opt-in. The same shadow settings were added in its own `A2FORenderer.ini`;
the previous log and change receipt are under
`build/ingame-shadows/a2-test-20261101T220947Z/`. This needs a full process
restart before testing. Vertical movement availability is separate from
the shadow pass's use of model geometry and the renderer depth buffer.

## Stability correction: audit 3

Immy subsequently reported inconsistent/misplaced Nexus shadows and no visible
effect in Classic Test. The preserved Nexus log did report successful draws,
but only aggregated captures and one final skip reason. Its snapshot is under
`build/ingame-shadows/stability-20261101T222306Z/`.

The stability update corrects shadow-map sampling to D3D9's integer raster
centres and half-texel texture centres. PCF comparisons now evaluate the
receiver plane at the actual sampled texel's raster centre, including the
sub-texel offset. The native VS 1.1 perspective fixture previously darkened
the lit caster to 102; it now stays 255 while the intended cast shadow stays
102. Both translated and large-world perspective cases pass. This matches
[Microsoft's D3D9 pixel/texel mapping contract](https://learn.microsoft.com/en-us/windows/win32/direct3d9/directly-mapping-texels-to-pixels).

World-transform reconstruction also solves the combined camera matrices in
double precision and only then converts the affine result to float. The host
regression reports zero position error at tested camera offsets 1200, 10000
and 100000, compared with 0.0131836 x error in the old path at offset 1200.
This test establishes the controlled algebra, not every native game matrix.

The shadow map now uses minimum blending into R32F with depth writes/testing
disabled, leaving the game's depth surface bound throughout. The former
private-depth swap was unsafe for discard-enabled native depth surfaces:
[D3D9 permits their contents to become invalid after a depth-surface switch](https://learn.microsoft.com/en-us/windows/win32/api/d3d9/nf-d3d9-idirect3ddevice9-createdepthstencilsurface).
The new pass never binds another depth surface or clears native depth/stencil.
Shadow-map size is capped to fit the native depth dimensions; native surfaces
smaller than 512 cannot use this path. MSAA shadow colour targets match the
native sample type/quality and resolve to the sampling texture. Unsupported
float blending or surface pairing disables the feature with a diagnostic.

Strict builds, host math, SM3 compilation, exact-managed-DXVK pixel/state tests,
native VS 1.1 orthographic/perspective tests, foreground occlusion preservation,
2x MSAA preservation and actual DLL fake-host lifecycle checks passed. The
uncaptured green foreground occluder remained exactly (0,255,0); lit/caster
pixels stayed 255 and cast-shadow pixels 102. The source/header compiled
without the optional test-probe macro; no probe/false-colour mode is deployed.
All 198 current exports and all imports match the preceding installed core.

The update adds bounded capture samples (native craft position versus inferred
origin/basis), fit size/depth range, effective world bias and category counts
for rejected context/route/material/mirror/base/light/vertex/GPU captures.
GPU rejection reasons are preserved before end-of-view status replaces them.
Pass/capture-count changes receive bounded extra logging. Light direction and
geometry admission rules have not been broadened or guessed.

Evidence: `build/ingame-shadows/audit3-verification.json`,
`audit3-runtime-tests.log`, `audit3-dll-load.log`, and the production build
receipt. Installed backup/receipt:
`build/ingame-shadows/audit3-deployment-20261101T233723Z/`.
Only the shared core DLL changed; all 228 protected files, including both
mods' renderer/module settings and existing Nexus content, matched their
pre-deployment hashes. The paired media and mission runtime remain the
previously recorded verified reused components.

For this latest update's binary rollback, exit the game and run
`python3 build/ingame-shadows/rollback-audit3.py`. This restores the preceding
prototype core and preserves both mods' settings. Fresh Nexus and Classic Test
visual acceptance remains pending; the older first-prototype rollback script
deliberately refuses to overwrite this newer core.

The requested next lighting model is recorded in
[ODF light sources and per-light shadows](odf-light-sources-design.md): global
directional sources, local point/nebula sources, cone spotlights, colour,
intensity, and source-specific occlusion. That document is a design contract;
its proposed ODF commands are not active in this prototype.

## World-bias audit 4

The 2026-11-01 recording and fresh log were preserved in
`build/ingame-shadows/recording-20261101T235625Z/`. Its effective normalized
bias moved from about 0.58 to 1.42 world units as the captured scene changed.
That establishes scene-dependent separation, not proof that every apparent
placement problem in the recording has the same cause.

The fixed-world-bias regression uses a camera shear so a caster and its shadow
are separately visible. A distant, camera-clipped record extends only the
light-space depth range. With the same blocker and receiver, legacy normalized
bias loses the shadow (pixel 255); fixed world bias keeps the same shadow pixel
102 before and after the distant record is added. Lit/caster pixels remain
255 and target alpha remains 255. This is a demonstrated depth-range
regression, not a tuning-only claim or in-game visual acceptance.

Capture samples now log up to 24 distinct craft/mesh/group combinations,
rather than consuming the whole budget on the first stationary starbase.
For matching native compositor captures, all collected light directions and
RGB values are logged, including maps with several lights. The projection's
light sign and geometry admission remain unchanged. These samples permit
comparison against the actual hull-lighting direction on the next live run.

All six private shadow GPU/lifecycle fixtures and host math passed. The full
assembled core's fake-host lifecycle passed; the new D3D11 depth snapshot
fixture and existing translated-game fixture passed with the paired Rust
library. All 198 exports and the complete import set match audit 3. Fresh core
link inputs stayed stable across the build (703 pinned inputs); previously
recorded mission objects/media reuse remains explicit.

The D3D11 addition is a callable depth-copy API, not an enabled visual effect
or renderer switch. See [depth snapshot evidence](renderer/depth-snapshot-2026-11-02.md).
The four upstream projects supply no additional shadow or D3D11 implementation;
see [the source review](fnsoidathq-renderer-review-2026-11-02.md).

Installed on 2026-11-02 at 00:43 UTC. Only the shared core and Nexus's renderer
INI changed; 299 protected files matched before/after, including both active
ArmadaL images, FleetOpsHook, module selections, and Nexus assets. The Classic
Test renderer override is unchanged. This deployment does not install either
legacy-DLL retirement trial or the withdrawn Nexus UI candidate.

Verification: `build/ingame-shadows/audit4-verification.json`,
`audit4-runtime-tests.log`, `audit4-assembled-tests.log`, and `audit4-abi.json`.
Backup/receipt: `build/ingame-shadows/audit4-deployment-20261102T004316Z/`.
Rollback: exit the game, then run
`python3 build/ingame-shadows/rollback-audit4.py`; it restores the preceding
core and Nexus INI together and rejects later unrecognized changes.

Manual acceptance is pending: fully restart Nexus and repeat the same moving
Mediterranean/station view. Check whether the cast stays attached as other
ships move, plus selection, cloaks, firing, HUD and menu return. Preserve a
fresh log/recording for the remaining alignment and native-light comparison.
