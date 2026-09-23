# SOD animation controls: investigation and proposed interface

Status: original design and native-code investigation, 23 September 2026.
The first module is now implemented. The authoritative implemented commands,
event list and limits are in [A2FOAnimations](../modules/A2FOAnimations/README.md).
Paired production/repair/movement/attack states and weapon-name bindings are also
implemented; the module README supersedes the original event/retrigger proposals.
The broader construction/progress and repair-complete ideas below remain design
proposals; they are not all implemented event names.

First requested events: **construction, launch and repair**.

## What is already available

The checked ArmadaL engine has per-instance animation start time and flags,
channel duration and key data, and native looping and reverse evaluation.
`A2FOAnimatedHardpoints` already evaluates matrix channels for gameplay
hardpoint queries, including animated ancestors. Its current interface has no
ODF playback controls. See [the module README](../modules/A2FOAnimatedHardpoints/README.md).

This supports a practical extension: define clips over existing SOD animation
channels, then select and advance those clips separately for each object.
It does not create poses that are absent from the model.

## Multiple clips on one unit

Each numbered entry defines a named-event clip over the unit's existing SOD
animation timeline. A unit can have construction, launch and repair animations
in different frame ranges, or reuse one range in opposite directions.
`X` is replaced by 0, 1, 2, etc.; entries are contiguous from zero.

The initial design plays one clip at a time per unit and applies its cursor to
all participating matrix channels. No channel name or separate animation name
is required. The SOD must contain the requested sample range in each participating
channel with compatible timing. Diagnose incompatible tracks rather than silently
stretching them. Independent simultaneous channel groups are a later extension.

## Proposed ODF commands

These names and boolean defaults follow the requested interface. The basic commands are now implemented; use the module README for the available
events and tested configuration contract.

| Command | Meaning | Default |
| --- | --- | --- |
| `animationXevent` | Event that starts this clip | Required |
| `animationXstart` | Lower bound of the sample range, inclusive | Required |
| `animationXend` | Upper bound of the sample range, inclusive | Required |
| `animationXdirection` | 1 forward, 0 reverse | 1 |
| `animationXrepeat` | 1 loops until interrupted, 0 plays once | 0 |
| `animationXresetonend` | 1 resets to `animationXstart` on completion, 0 holds the terminal pose | 0 |
| `animationXspeed` | Positive playback multiplier; 1 normal, 2 twice as fast, 0.5 half speed | 1.0 |

```ini
// Open the doors as production starts, then hold them open.
animation0event = "production_begin"
animation0start = 0
animation0end = 30
animation0direction = 1
animation0repeat = 0
animation0resetonend = 0
animation0speed = 1.0

// Close them using the same range in reverse after the ship clears the bay.
animation1event = "launch_clear"
animation1start = 0
animation1end = 30
animation1direction = 0
animation1repeat = 0
animation1resetonend = 0

// Play a different part of the model timeline throughout repair.
animation2event = "repair_begin"
animation2start = 40
animation2end = 80
animation2repeat = 1

// Stop the repair loop and play its retraction animation.
animation3event = "repair_end"
animation3start = 81
animation3end = 100
animation3resetonend = 0
```

The example assumes the model contains the illustrated poses. It also needs a
`production_cancel` clip to close the doors if no launch occurs.

### Playback rules

- `start <= end` in both directions. Forward plays start to end; reverse plays
  end to start. Equal endpoints select a static pose without a zero-duration loop.
- With repeat off, hold the terminal pose unless resetonend is enabled.
- Reset means the literal configured `animationXstart` frame. For reverse
  playback the terminal pose is already that frame, so reset has no additional
  effect. It does not mean jumping back to the reverse entry frame (`end`).
- Repeat on loops the selected range in the selected direction until a new clip
  replaces it. Resetonend does not run at each loop boundary; it applies only to
  natural completion of a nonrepeating clip.
- A newly triggered clip replaces the active clip, including a repeating or held
  clip. Replacement does not first reset the outgoing clip. The incoming clip
  starts at its direction-dependent entry frame; seamless blending is deferred.
- Events fire on transitions, not every simulation tick while a condition holds.
  A subsequent occurrence of the same event starts its clip again. For the first
  version, duplicate event bindings on one unit are rejected as ambiguous.
- Missing settings use the defaults above. Reject boolean values other than 0/1,
  missing required fields, invalid ranges, nonfinite numbers, nonpositive speed
  and unknown events with an ODF-specific diagnostic.

Keyframe numbers mean **exported SOD samples**, zero based, not necessarily the
original 3D editor timeline numbers. Preserve that numbering before native
optimisation; do not expose indices into an optimised channel array. A model
inspector should report channel names, original counts, duration and target nodes.

Use stored timing rather than assume 30 FPS. For a uniformly sampled source
channel with N samples and duration D, native sample spacing is D/N. A bounded
clip from A to B reaches its endpoint after `(B-A) * D/N / speed` seconds.
For repeating playback, the implementation also holds the terminal sample for
one sample interval, so a loop lasts `(B-A+1) * D/N / speed` seconds. Channels with an
explicit time table use its sample timestamps. Source-editor frame labels need
an explicit mapping if they differ from the exported samples.

Pause/resume, finite repeat counts, ping-pong, delay, progress-driven playback,
independent channel groups and pose blending are possible later additions. They
are not requirements of the initial command set above. Native matrix playback
must not be described as smooth interpolation without further work: the inspected
matrix EvaluateAndPlay implementation copies the selected sample. Slowing an
animation with few samples may visibly step.

## Construction, launch and repair events

These event names are proposed. The distinction between the object being built
and its producer is necessary: they have separate models and animation state.

| Event | Object whose animation runs | Intended trigger |
| --- | --- | --- |
| `construction_begin` | Object under construction | Actual construction begins |
| `construction_complete` | Object under construction | Construction reaches completion |
| `production_begin` | Yard or other producer | Job becomes active, not merely queued |
| `production_cancel` | Producer | Active job is cancelled |
| `launch_begin` | Producer | Finished object begins leaving its bay |
| `launch_clear` | Producer | Outgoing object clears the bay |
| `repair_begin` | Repair facility | Actual repair service starts |
| `repair_end` | Repair facility | Service stops, including interruption |
| `repair_complete` | Repair facility | Successful service completion |

A later progress-driven mode could make a station visibly assemble itself in
step with authoritative construction progress. It would remain at the same pose
when construction pauses and reach the end pose on completion. This is distinct
from the initial timed clip triggered by `construction_begin`; the current command
set does not promise automatic progress synchronisation. An object whose native
build visual is a separate effect also requires an explicit mapping to that
instance. No additional progress-mode ODF syntax is fixed yet.

For repair arms, play an opening clip at `repair_begin`, hold or loop a servicing
clip while service is active, then retract at `repair_end`. `repair_complete` alone
is insufficient: departure, cancellation, capture or destruction can interrupt
service. Yard-side clips run on the yard; a repairing ship needs a separate
ship-side binding if its own model should animate.

Multiple repair berths or construction slots need a bay/job identity or an
active-service count. One ship leaving must not retract equipment still serving
another. Squad replacement production must remain distinguishable from normal
production so repair and production bindings do not fight over the same channel.

### Visual events versus gameplay gates

Starting a door animation does not itself prevent a ship from launching.
Reliable "open doors, then release ship" behaviour needs a separate optional
launch gate tied to clip completion and the exact job/bay. That gate must resume
once, handle cancellation/capture/save-load, and cooperate with squad production.
Do not claim or suppress native finish events merely to play a visual clip.

The first controller should be visual only. Verified launch gating can then be
added explicitly. The door clips above is illustrative; bay-clear detection
still needs a native implementation and in-game validation.

## Implementation approach

1. Add a small host-testable clip state machine: cursor, event transitions,
   range, speed, loop flag, reset/hold pose and replacement rules. Advance from
   simulation time, independent of camera visibility or render rate.
2. Resolve ODF clips to the loaded SOD channels and retain source sample timing.
   Keep playback state on each object instance. Two ships using the same SOD
   must be able to hold different poses and run in opposite directions.
3. Introduce a scoped evaluation context used by both native rendering and
   AnimatedHardpoints. Evaluate selected channels at the same controlled cursor;
   let unbound channels use native behaviour. Restore global evaluator state and
   active-instance context on every exit, including nested evaluations.
4. Update AnimatedHardpoints cache keys with controlled cursor, direction and
   playback generation. Its current time/start/triggered key cannot distinguish
   every pause, seek or clip change. Audit invalidation when render evaluation
   writes shared database transforms.
5. Bind actual production/repair transitions first. Add launch-begin/clear and
   construction progress only after their native sources are verified. Queue
   admission is not production start, nor is construction finish bay clearance.
6. Persist active clips, cursor, loop state and held poses using stable object
   identities. Verify cleanup, save/load and deterministic multiplayer behaviour
   before advertising those capabilities.

Do not permanently rewrite shared channel duration, keyframes or node transforms
to encode one object's state. A global animation-clock override alone also cannot
give separate channels different simultaneous playback cursors.

Initial scope should cover matrix animations and animated nulls. SOD visibility
channels, sprite animation, emitter timing and sound events require their own
coverage; they are not automatically solved by controlling matrix channels.

## Verified native evidence

Inspected executable:
`/home/tamsynn/NVMeData/Fleet Ops Roots/Data/ArmadaL.exe`.

SHA-256: `63bd645d82aec75cdd7ee3e3271fb67bb54f4ae1d1636a736b0db620bd997e36`.

Addresses below are image-relative RVAs. The Fleet Operations symbol map's
`0001:` text-section offsets require adding 0x1000 to obtain these RVAs.
Symbols were checked against executable disassembly, not assumed from the map.

| Function | RVA | Observed behaviour |
| --- | --- | --- |
| Animation SetStartTime | 0x213670 | Computes elapsed time from engine clock and start |
| ForceFirstAnimationFrame | 0x2136c0 | Sets elapsed animation time to zero |
| AnimationChannel Evaluate | 0x213910 | Chooses key and fraction using time, duration and flags |
| Matrix EvaluateAndPlay | 0x2161c0 | Evaluates channel and copies selected matrix sample |
| Instance SetAnimationFlags | 0x22e290 | Stores flags at instance +0x0c |
| Instance RenderInternal | 0x22e780 | Sets active instance, evaluates visible channels, renders |
| Instance TriggerAnimation | 0x22ea90 | Captures start time at +0x18 and sets +0x76 |
| Instance Animate | 0x22eab0 | Separate channel evaluation path |
| ReadAnimationChannel | 0x23f850 | Reads sample count and duration; binds channel to target |

The evaluator tests instance flag 0x1 for looping and 0x4 for reverse. This is
an observed internal implementation, not a new supported modder bitmask API.
Its active-instance lookup matters: changing flags on an instance is insufficient
if a gameplay evaluation runs without the corresponding active-instance context.

Channel fields observed: duration float +0x2c, selected key +0x30, key count
+0x34, phase/time offset +0x38, optional time-array range +0x20/+0x24.
Global current/start animation times are at RVA 0x3a8e74/0x3a8e78.

The SOD loader reads four bytes after the sample count into channel duration.
Existing local tools call those bits `block_length`; that label is misleading.
A read-only parse of the installed `sod/map_wormhole.sod` found three matrix
channels (`wormglow`, `Mwo23`, `Mwo22`), each with 360 samples and those bits
interpreting as approximately 12.0333 seconds. The tools should preserve and
expose duration correctly before adding clip authoring. No asset was rewritten.
The next two bytes are read individually by the engine, while the current local
parser combines them into a uint16 type; future tooling should audit that too.

Relevant repository integration points:

- [AnimatedHardpoints implementation](../modules/A2FOAnimatedHardpoints/module.cpp)
- [SDK producer and craft events](../sdk/include/a2fo_module_api.h)
- [Squad repair lifecycle](../modules/A2FOSquadrons/native_repair.inl)
- [Current ArcLab SOD reader](../tools/A2FOArcLab/src/sod.rs)

The producer SDK provides admission, finishing, finished, starting-effect and
cancellation/deletion/clear events. These are useful building blocks, but do not
establish every semantic trigger listed above. Some events are claimable and
already used by other modules; animation observation must preserve their work.

## Acceptance checks before release

- Forward and reverse clips hit exact endpoints without wrapping outside range.
- Speed multipliers, one-shot completion and looping have documented timing.
- Forward/reverse reset and hold obey the configured defaults.
- A repair-end clip replaces the repair loop without restarting it every tick.
- Two instances sharing one SOD animate independently, including offscreen.
- Animated mesh and hardpoint queries agree during playback, pause and hold.
- Construction begin/complete events select the intended clips.
- If progress mode is added, construction pause/resume agrees with its poses.
- Queued jobs do not animate as active jobs; cancellation closes any held pose.
- Launch events follow the real outgoing ship, including squad member launches.
- Repair interruption and overlapping repair clients do not leave stuck poses.
- Missing channels and unsupported executable signatures fail safely.
- Save/load restores clip state; multiplayer simulation does not depend on FPS.

The first module implements part of this plan; see its README for validation
evidence and remaining limits. In-game acceptance is still required.
