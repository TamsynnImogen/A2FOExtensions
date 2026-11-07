# Owned animation poses — integration work in progress

This advances stage 53; it does **not** close its complete offscreen geometry
gate. Class and procedural transforms, visibility, LOD, material policy and
other draw domains remain separate. No legacy drawing is suppressed.

## Data path

1. The existing version-pinned SOD asset feeds a new private Rust metadata
   callback. It exports all authored nodes, numeric parents, matrices and
   matrix/scale channels, including nodes excluded by the static mesh preview.
2. `ModelPoseCache` copies callback data into immutable owners. Entries hold
   weak model references; published snapshots pin their model and metadata.
3. `capture_instance_animation_time` samples the observed instance's base time
   and the engine clock twice. It does not use the shared last-rendered instance
   or the shared elapsed-time variable.
4. `capture_runtime_pose_channels` copies the actual database channel list,
   targets, playback settings, optimized keys and timestamps twice, surrounded
   by matching instance-clock snapshots. Native selected-key state is not read.
5. `evaluate_owned_animation_pose` evaluates each instance independently and
   composes the authored hierarchy into owned model-space transforms.

The new private C metadata structs have checked 32-bit layouts: node 72 bytes,
channel 28 bytes and metadata 20 bytes. Rust, C and C++ are exercised together
in the whole-path fixture. Public renderer API1–8 remains unchanged.

Limits include 4,096 nodes, 256 channels, 65,535 keys per channel and 1,048,576
copied key/timestamp floats. The metadata cache is bounded to 64 entries and
128 MiB. Unsupported references, ambiguous targets, changing owners, malformed
vectors and unwritten channel targets fail admission. No previous instance's
matrix is substituted for an unwritten target.

## Verified native policy

The original scalar routine interpolates with x87 intermediates and one final
float store. Its modes distinguish step, wrapped interpolation and a guard key.
The original Matrix34 routine selects a complete key in all three active modes.
Matrix mode 3 still allocates a guard in the native vector; capture compares
those bytes for freshness and removes the unused guard before evaluation.

The isolated native comparison passed 392 instance-clock cases, 18,432 scalar
channel cases and 18,432 Matrix34 channel cases across four rounding modes.
This includes signed zero, finish/reverse policy, explicit/uniform timestamps,
direct/fallback targets and no-write behavior. These fixtures execute only the
pinned CPU routines in a private inspection mapping, never game initialization,
simulation or graphics functions. Their input executable is unchanged.

The whole-model GPU fixture passes real Rust metadata through a C layout check,
evaluates two independently timed instances, and checks both footprints through
API5 D3D11 readback. It also accepts the actual test assets:

| Model | Authored nodes | Channels |
| --- | ---: | --- |
| Venture | 15 | 0 |
| Mediterranean | 55 | 2 Matrix34 channels, 122 keys each |

Those counts establish parsed asset coverage, not native class or live-game
acceptance. The fixture's complete-caster assertion describes only its own
two-object scene.

## Core integration

The existing default-off `CaptureAnimationClock` option now enables the guarded
`native11_animation_clock::sample_time` and `sample_channels` producers.
Sampling is owner-thread-only and refuses callback reentry. Active model pins,
exact loaded code regions and node-observer ownership are checked before and
after capture. The complete SOD channel-loader body is pinned in two normalized
regions in addition to the existing clock and channel policy regions.

At the existing post-animation material observation point, sampled diagnostics
can compare the owned transforms with the already copied effective part states:

```text
Native11 owned transform: ... state=matched|differs|unavailable ... max_error=...
```

This uses a relative/absolute tolerance of `1e-4 * (1 + abs(native element))` for
composed matrices. The channel kernels themselves have the bit-exact native
comparisons described above. A mismatch remains diagnostic; it does not approve
offscreen geometry, bypass world-domain admission or change rendering.

The work-in-progress build ID is
`20261103-native11-owned-pose-integration-wip`. The candidate retains the canonical 198 core exports and original import list.
All 84 private runtime cases pass, including older API clients and seven
byte-identical reference previews. The pure Craft visibility policy also matches
the isolated original damage helper in 1,056 comparisons; it is not yet bound
to live class state. The candidate is staged only; seven protected installed
files remain unchanged.

## Remaining integration

- Port or account for native class/procedural transforms and per-instance node
  visibility, including hidden/damage/LOD groups.
- Bind owned part states and effective materials for complete world membership,
  and validate source lifetime across the full frame.
- Complete background, terrain, UI, effects, video/menu producers and native
  frame/presentation ownership.
- Run actual game parity, lighting/shadow and lifecycle acceptance on recorded
  binaries before calling the renderer ready for full gameplay testing.
