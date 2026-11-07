# Animation timing recovery and diagnostic trial

Stages 51–52 establish a pinned engine inventory, owned CPU timing policy and
default-off render-time clock diagnostics. Stage 53's complete offscreen producer
remains open. A matching clock/key index does not certify node poses, textures,
visibility, LOD, other world domains or a complete native gameplay frame.

## Verified timing policy

`animation_phase.hpp/.cpp` recovers the channel phase policy from the exact
ArmadaL SHA-256 `4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de`,
routine RVA `0x213910`. It consumes owned timing and key timestamps, preserves
loop/reverse flags and the native nonloop endpoint sentinel, and restores the
caller's x87 control word. The uniform-key selector includes the native 0.05f
bias; its interpolation fraction is deliberately not clamped to one.

The supported input subset is finite nonnegative elapsed/offset time, positive
duration, 1–65,535 keys and optional strictly increasing timestamps beginning at
zero and below duration. Loop cycle counts must fit signed32. Unsupported and
nonfinite intervals fail without changing caller output.

The private fixture runs 6,144 bit-exact comparisons across four x87 rounding
modes, loop/reverse combinations, explicit/uniform times and several key counts.
It maps the pinned executable without startup, rebases six reviewed HIGHLOW
operands and binds the sole required MSVCRT `_ftol` import in private memory.
The algorithm instructions are unchanged. This executes one isolated native CPU
routine; it executes no game startup, simulation or native graphics. Earlier
failed inspection-mapping attempts are preserved separately and are not passing
evidence. No input executable file is patched.

## Read-only diagnostic

`[Native11] CaptureAnimationClock=1` additionally requires the existing scene,
asset, texture, material and node producers. The core validates the exact image,
nine code regions with relocation normalization, two channel dispatch slots and
the checked node observer. The default is zero. Invalid or unavailable admission
leaves the diagnostic disabled and never authorizes graphics takeover.

At an already validated post-animation observation, the diagnostic copies clock
inputs for the current instance and checks native selected keys against the
owned timing policy. It validates reciprocal list links, ownership, channel
class/dispatch, vector extents and a 256-channel/1 MiB timestamp cap. Two reads
must agree before publishing a snapshot. Paused animation uses the observed
native force-first policy while still checking raw clock consistency. Reentry,
foreign threads and changed profile/observer ownership are refused. Sampling
failures are diagnostics and do not turn an incomplete capture into geometry.

## Manual trial gate

The staged core and optional trial tool are for a bounded diagnostic test.
`tools/native11_clock_trial.py` defaults to a read-only plan. Its explicit apply
mode backs up and verifies the installed core and Nexus renderer INI before
replacing those two files. Rollback verifies the trial's current bytes before
restoring the originals. The tool does not modify executables, FleetOpsHook or
the graphics wrapper, and does not launch or control the game. Inspect its plan
and recorded hashes before applying a trial.

After a verified trial is installed, manually launch FO4 Nexus, start a short
skirmish, move a ship and pan it out of and back into view, then exit. Preserve
`Data/A2FOExtensions.log`. Check the stage 52 build ID and lines beginning
`Native11 animation clock:` for `matched` or a concrete unavailable reason.
The agent can read that log directly once the test has finished. Keyboard/mouse
automation and window activation are prohibited by the workspace instructions.

Passing this trial supplies one prerequisite for the offscreen evaluator.
It does not close stage 53 or the stages 54–65 integration/release gates.
