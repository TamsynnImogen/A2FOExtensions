# A2FOAnimations

Event-driven animation clips for a unit's existing SOD matrix-animation timeline.
Source and native DLL implemented; host and headless x86 integration tests are
provided. Visual in-game acceptance remains required.

Enable `A2FOAnimations` in the mod's `[modules]` list. Install the updated
`A2FOAnimatedHardpoints.dll` as well and enable it to make weapons and gameplay
hardpoint queries follow the same frame as the visible model.

## ODF commands

Use contiguous indices starting at 0, up to 32 entries. Each event can be bound
once per unit ODF. Effective inherited fields are read through the core SDK.
All commands are optional as a group: units without event clips retain native playback.

| Command | Meaning | Default |
| --- | --- | --- |
| `animationXevent` | Built-in event name below, or a weapon ODF basename | Required |
| `animationXstart` | First exported sample, zero based, inclusive | Required |
| `animationXend` | Last exported sample, inclusive; must be >= start | Required |
| `animationXdirection` | 1 forward, 0 reverse | 1 |
| `animationXrepeat` | 1 repeat until replaced, 0 play once | 0 |
| `animationXresetonend` | 1 return to the configured start frame, 0 hold terminal pose | 0 |
| `animationXspeed` | Positive speed multiplier, up to 10000 | 1.0 |

Each entry is a clip in the same model timeline. One clip plays at a time for each
unit; a discrete event replaces it at the new clip's entry pose. Separate units
sharing a SOD have separate playback state. An event with no binding leaves the
current clip alone. A subsequent occurrence of a discrete event restarts its clip;
continuing repair or a queued production order does not restart it each tick.
Paired state events follow the additional rules below.

Forward plays start to end; reverse plays end to start. A repeating clip gives
each sample, including the terminal sample, its native-duration interval before
wrapping. Resetonend applies only to natural one-shot completion, not interruption
or loop boundaries. For reverse, the terminal pose is already `start`, so reset
has no additional effect. Equal endpoints hold a static pose, even with repeat on.

Speed 1 uses the SOD's stored timing, not an assumed FPS. Frames are original
exported SOD samples captured before engine keyframe optimisation; original 3D
editor frame labels may differ. Playback uses native-style discrete matrix poses,
not pose blending. All animated matrix tracks must cover the requested range and
have compatible sample intervals; constant one-sample tracks stay constant.

Invalid ODFs log their filename and disable their clips. Invalid model/range
bindings log the ODF and event, retain native/previous playback, and retry when
geometry becomes available. When any valid event clips are defined, their matrix
playback overrides the unit's vanilla `animation = ...` setting. Before the first
bound event, the unit holds `animation0start`; a `spawn` clip can select a different
initial pose. Native loop, reverse and triggered flags do not override the event
controller. No change to the unit's vanilla `animation` value is necessary.
Invalid/unresolved model bindings still fail back to previous/native playback.

## Available event names

There are sixteen built-in names, plus weapon ODF basenames. Matching is case-insensitive.

| Event | Runs on | Trigger |
| --- | --- | --- |
| `spawn` | Configured craft/station | First simulation observation; skipped when an earlier native event already selected a clip |
| `load` | Configured craft/station | Native craft PostLoad completes; explicitly selects the desired loaded-game pose/clip |
| `production_begin` | Producer, such as a yard | A new active job reaches the native start-construction-effect callback; deduplicated by queue ID |
| `production_complete` | Producer | Job completion; native construction rigs wait until all worker bees have landed |
| `production_cancel` | Producer | Job cancellation; native construction rigs wait until all worker bees have landed |
| `launch_begin` | Yard and launched craft | Craft enters the yard's build-output queue; yard fires for its first tracked outgoing craft |
| `launch_clear` | Yard and launched craft | Native output queue releases the craft to its rally/normal command; yard fires when its last tracked outgoing craft clears |
| `launch_abort` | Yard and launched craft, where still alive | Tracked output is removed without normal release, or either object is cleaned up; surviving counterpart receives the event |
| `repair_begin` | Yard and serviced craft | Native Shipyard::SetRepairDock assigns the craft to the dock |
| `repair_end` | Yard and serviced craft, where still alive | Dock assignment clears/changes, or either participant is cleaned up |
| `production` | Producer | Open on production-begin, hold, reverse on production-complete/cancel |
| `repair` | Yard and serviced craft | Open on repair-begin, hold, reverse on repair-end |
| `move` | Moving unit | Open during actual translation at any speed, reverse when stopped |
| `move_impulse` | Moving unit | Open during translation outside native warp mode, reverse on stop or mode exit |
| `move_warp` | Moving unit | Open during translation in native warp mode, reverse on stop or mode exit |
| `attack` | Attacking unit | Open during an attack order/AI attack action or recent weapon use, reverse when inactive |

`production_*` are construction events on the **builder/yard**. They do not
animate a separate construction hologram or provide a progress-driven animation
on a station being assembled. `construction_begin`, `construction_complete` and
`repair_complete` are not built-in events. RepairEnd deliberately covers both
completion and interruption; it is not proof that all damage was repaired.

For native construction ships such as `fconst`, job completion/cancellation
starts the bee recall but does not immediately end the animation's production
state. The controller keeps it active until the native returned-bee count reaches
the class's worker-bee total, then emits `production_complete` or
`production_cancel`. This delays both the paired `production` reversal and any
separate completion/cancel clip. It uses landing state, not a fixed timer, and
does not delay the actual job completion, cancellation or refund. A new active
job supersedes a pending end event. Zero-bee ships and jobs with no created bee
group do not wait. Shipyards and hybrid research producers retain their existing
job-notification timing; the latter use a different construction sidecar.

Launch events track the build-output queue, including sequential squad members.
The separate repair-output queue does not trigger launch events. Native output
release defines `launch_clear`; it is not a custom geometric clearance test.
If several events occur in the same simulation step, native callback order wins;
for example production-complete may arrive after the first launch-begin. Bind
only the event intended to control the shared door clip.

## Paired states and priority

`production`, `repair`, `move`, `move_impulse`, `move_warp` and `attack` play in
the configured direction while active, hold the terminal pose, then play in the
opposite direction when inactive. With the default direction 1 this is forward,
hold, reverse. Repeat and resetonend are ignored for these six paired states;
start/end, speed and direction still apply.

For example, doors using frames 0..30 can open for construction, stay open until
the job finishes or is cancelled, and then close automatically:

```ini
animation0event = "production"
animation0start = 0
animation0end = 30
animation0speed = 1.0
```

Use `repair`, `move`, `move_impulse`, `move_warp` or `attack` for the same behaviour
on those states, with a frame range appropriate to the model.

Cancellation midway reverses from the current cursor. If the same state becomes
active again while returning, it turns around at its current cursor. It does not
jump to an endpoint. A `production` binding takes precedence over separate
production-begin/complete/cancel bindings; `repair` similarly owns repair-begin/end.

Only one clip can drive the model at once. When several paired states are active,
the priority is **repair, production, attack, move_warp, move_impulse, move**.
Only configured bindings participate, so a generic `move` clip covers both speed
modes when no more specific movement clip is configured. Movement is measured from
translation between simulation updates, not from a move order or turning in place.
Warp classification uses the supported native hover/Borg/smooth control's warp
state. Attack orders include approach/chasing; automatic weapon use keeps attack
active for one simulation second after the latest use.

Discrete events and weapon clips can interrupt a paired clip. Once a one-shot
finishes, a still-active paired state is selected again from its entry pose.
Another state transition can replace a discrete clip earlier. There is no pose
blending between different frame ranges. A repeating discrete clip continues
until replaced by another event/state transition.

## Weapon-name events

Put the weapon's ODF basename in the unit's event field, and enable animation on
the weapon itself. This is the weapon ODF, not its ordnance ODF, display name or
unit weapon-slot name. Optional `.odf` extensions and letter case are normalised.
Built-in event names are reserved and cannot also be weapon-event names.

```ini
// On the unit:
animation0event = "fphoton"
animation0start = 90
animation0end = 110
animation0speed = 1.0
animation0repeat = 0
animation0resetonend = 1
```

```ini
// In fphoton.odf (the weapon):
animation = 1
```

A missing or zero weapon `animation` value disables its named animation event.
Any finite nonzero numeric value enables it. These weapon-name clips use normal
repeat/reset settings, rather than automatic reverse-on-end behaviour.

Install the updated **A2FOMuzzleFlashes.dll** and enable that module for ordinary
automatic projectile weapons: its shared completed-projectile observer notifies
Animations even when no muzzle-flash sprite is configured. This avoids competing
hooks with the ammunition system. No projectile notification is sent for a failed
ordnance build. The SDK's accepted TriggerObject notification also supports
explicit/special weapon uses without projectiles. That notification means the
request was accepted; it does not guarantee a projectile was produced. Rejected
prechecks do not animate. Rapid shots can restart the clip repeatedly.

Because custom names are now allowed, an unrecognised identifier is treated as a
weapon basename. It will not fire unless that actual weapon enables `animation`.

## Example

Assume the SOD has closed doors at frame 0, open doors at frame 30, and repair
movement on frames 40..80. Replace the ranges to match your actual model.

```ini
animation0event = "spawn"
animation0start = 0
animation0end = 0

animation1event = "production_begin"
animation1start = 0
animation1end = 30
animation1direction = 1
animation1repeat = 0
animation1resetonend = 0
animation1speed = 1.0

animation2event = "launch_clear"
animation2start = 0
animation2end = 30
animation2direction = 0

animation3event = "production_cancel"
animation3start = 0
animation3end = 30
animation3direction = 0

animation4event = "repair_begin"
animation4start = 40
animation4end = 80
animation4repeat = 1

animation5event = "repair_end"
animation5start = 0
animation5end = 0

animation6event = "launch_abort"
animation6start = 0
animation6end = 30
animation6direction = 0

animation7event = "load"
animation7start = 0
animation7end = 0
```

## Build and checks

From the repository root, with MinGW i686 C++ and a host C++17 compiler:

```sh
make build/modules/A2FOAnimations.dll build/modules/A2FOAnimatedHardpoints.dll build/modules/A2FOMuzzleFlashes.dll
make animations-test
make build/animations_native_test.exe build/muzzle_animation_bridge_test.exe
make build/animations_runtime_init_smoke.exe
# Headless test; use an existing isolated Wine prefix.
env -u DISPLAY -u WAYLAND_DISPLAY \
  WINEPREFIX="$HOME/.cache/a2fo-squadrons-wine" WINEDEBUG=-all \
  WINEDLLOVERRIDES=winemenubuilder.exe=d \
  timeout 45s wine build/animations_native_test.exe
env -u DISPLAY -u WAYLAND_DISPLAY \
  WINEPREFIX="$HOME/.cache/a2fo-squadrons-wine" WINEDEBUG=-all \
  WINEDLLOVERRIDES=winemenubuilder.exe=d \
  timeout 45s wine build/muzzle_animation_bridge_test.exe
python3 tools/verify_animation_native_rvas.py '/path/to/Data/ArmadaL.exe'
# Map installed binaries without executing their entry points; reproduce FOFS's
# startup detour and verify module registration and rejection of unknown targets.
env -u DISPLAY -u WAYLAND_DISPLAY \
  WINEPREFIX="$HOME/.cache/a2fo-squadrons-wine" WINEDEBUG=-all \
  WINEDLLOVERRIDES=winemenubuilder.exe=d \
  timeout 45s wine build/animations_runtime_init_smoke.exe \
  'Z:\path\to\Data\ArmadaL.exe' 'Z:\path\to\FleetOpsHook.dll'
```

The DLLs are written to `build/modules/`. With the game closed, back up and copy
all three to `Data/modules/`, then append `activeN = "A2FOAnimations"` using the next
free index in the test mod's `[modules]` section. Keep its AnimatedHardpoints and MuzzleFlashes entries.
Check `A2FOExtensions.log` for `SOD animation clips initialized` and for ODF or
signature errors before testing clips.

Fleet Ops replaces `cPrjID::GetOdfName` during filesystem startup. The module
accepts its checked replacement inside `FleetOpsHook.dll` and preserves that
entry. Builds before the 23 September startup fix rejected this replacement and
logged `Unsupported animation helper signature; runtime disabled`; no event ODF
fields were registered in that case. The fixed build logs
`Using checked FleetOpsHook cPrjID::GetOdfName entry` before successful startup.
Other unknown helper changes still disable the runtime and now log their RVA.

Manual checks in a new skirmish:

1. Two identical units play different events without sharing a pose.
2. Forward/reverse, speed, repeat, reset and hold match the model's frame ranges.
3. Animated weapon hardpoints remain attached at held and reversed poses.
4. Queueing a job does not start production animation; active construction does.
5. Cancel production; launch an ordinary ship and a squad; check clear/abort clips.
6. Start and interrupt repair; docking does not continuously restart its clip.
7. Move the camera away and back; clip timing remains based on simulation.
8. Load a save and check the explicitly configured `load` pose.
9. Interrupt paired production/repair halfway, restart while closing, and verify
   continuous reversal without jumping.
10. Move at impulse and warp, stop, attack and stop attacking; check paired pose
    changes and specific-over-generic movement priority.
11. Fire an animation-enabled weapon, then a disabled weapon. The named clip must
    follow only the enabled weapon; check ordinary auto-fire and special uses.
12. Try vanilla `animation = 0`, `1` and `2` on a configured unit; the event
    controller must retain its own initial/held pose and selected clip.
13. On `fconst`, complete construction and cancel it separately: keep the doors
    open while the bees return, and start reversing only after the last landing.
    Repeat with a second queued job and with zero worker bees.

## Current limits

- Supported checked ArmadaL/Fleet Operations Roots images only. Hook signatures
  are verified as a group before patching; partial hook failures remain resident
  pass-throughs. Native production, repair and queue behaviour is preserved.
- Matrix tracks and animated nulls only. Sprite/visibility channels, sounds and
  emitters retain native behaviour. There is no independent channel selector yet.
- Visual playback does not hold a launch or delay repair until a pose is reached.
- Playback state is not serialized yet. Loading clears the old state and fires
  `load`; without a load binding the initial configured pose is held. Movement
  and attack observations resume on simulation. Active production/dock/launch
  event history is not reconstructed from a save. Multiplayer is unvalidated.
- No automatic progress-driven construction clips, smooth pose blending or
  simultaneous clips on separate parts of one unit.
- A replacement model restarts the active binding once its new channels resolve.

Implementation: [module.cpp](module.cpp), [playback.hpp](playback.hpp),
[optional hardpoint bridge](api.hpp). Earlier research and future ideas are in
[the design document](../../docs/sod-animation-controls-design.md).
