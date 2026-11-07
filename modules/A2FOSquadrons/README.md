# A2FOSquadrons implementation

**Repair hook compatibility and member-derived pricing, 2026-09-23.** The host-testable configuration and
registry are joined by a Fleet Operations adapter. `classLabel = "squadron"`
is hosted by the native Craft build descriptor. Checked Starbase and Shipyard
`FinishBuild` vtable hooks consume the abstract job before native
post-processing can queue a null placeholder, then starts a sequential launch.
Only the first real ship is constructed at completion; each following ship is
constructed after the previous ship leaves the native output-queue stage.
Members use the shipyard's native build transform and OutputQueueManager
launch path. A checked Fleet Ops selection
hook expands a click, box selection or deselection of a member to its surviving
squadmates, up to the native 30-ship selection limit. The ShipDisplay getter
adapter projects those physical handles into one visible tile per squad,
without changing the command selection. A `live/maximum` badge follows the
selected icon. Native health-bar current/maximum values are summed across
survivors; the native shield tint is weighted by their shield capacity.

Accepted repair visits now replenish missing members through normal paid
production. Native save persistence and aggregate initial physical unit-cap
reservation are still pending.

Initial build cost and time are now the count-weighted sum of the member
classes. The abstract squad ODF's own cost/time fields are overridden. The six
native resource costs feed the normal build-button, payment, progress and refund
paths; the four added resource costs are supplied to `A2FOResources` through a
read-only cached-cost export. Use the updated Squadrons, FeaturePack and Resources
DLLs together. Native team/game build modifiers continue to apply to the total.

The intended gameplay is recorded in the [squadron design](../../docs/squadrons-design.md).

## Implemented

- Dedicated squad definitions with numbered member ODF/count pairs, numeric
  row ordering, automatic-repair policy and strict validation. Member types
  are checked through a supplied class resolver. Nested squad definitions,
  unsupported classes and missing ODFs are rejected.
- Complete-complement membership commits, immutable configured slot types,
  deterministic squad IDs, stable `(row, ordinal)` slots, casualty counts,
  representative reassignment and last-member removal. Captured members
  detach into ordinary ships of their new owner.
- Incremental membership during native yard launch. The squad keeps its
  identity if an early ship dies before a later member exits. Yard loss,
  ownership changes or failed construction stop the remaining launch job.
- Logical selection planning: one entry per squad, distinct entries for two
  squads of the same composition, independent ordinary ships and deterministic
  expansion to all living members. Exceeding the caller's verified physical
  command limit fails with no partial output. It never changes targets itself.
- Automatic replacement planning during an explicitly accepted repair cycle.
  All survivors must be serviced at the same compatible, same-team yard.
  One outstanding ticket per squad reserves the lowest vacant slot. Completion
  checks the ticket, current repair conditions, owner and exact member type.
  Stale/duplicate completions fail. Cancellation returns the job identity for
  native refund/cap handling; total squad loss cannot resurrect a squad.
- Versioned structured snapshots retaining composition, vacancies, IDs and
  pending reservations. Restore validates into a temporary registry before
  replacing live state. Derived membership indexes are rebuilt.

Engineering bounds are 16 numbered rows (`0`–`15`), 32 members per squad,
256 squads per team and 4,096 globally. These bounds do **not** establish
Armada's actual command capacity or acceptable performance at those counts.
The native adapter currently rejects new squad builds above 30 members because
Fleet Ops' selection array has a checked 30-handle capacity.

## Configuration boundary

Research stations can also opt into selecting their attached shipyards as a
group with `selectPodsTogether = 1`. Clicking the main station or any of its
attached yards selects the main station and all surviving same-owner yards in
that station's native pod list. Shift-toggle removes/adds the whole group;
separate station complexes stay separate. This does not create a squadron,
compress the selection to one icon, reserve a numbered fleet, or change costs.
The flag defaults off and is accepted only on `classLabel = "research"` roots.
Shipyard children retain their own build queues and native behavior. Group
expansion uses the existing checked 30-handle selection limit, with no partial
addition when the full group would exceed it. Native attachment lists rebuild
the relation after load; no new save fields are written. Manual gameplay and
save/load acceptance remain required.

`parse_definition()` consumes **effective inherited ODF fields**, matching the
core SDK's field representation. It is not a second filesystem/ODF loader.
The native adapter receives those resolved inherited fields through the existing
core class-loader path, preserving includes and mod precedence. Duplicate effective squad
fields and unknown `squad*` commands are rejected to catch authoring mistakes.

```odf
// Phase-one native build descriptor.
classLabel = "squadron"
unitName = "Patrol Squadron"
// Cost and build time are derived from 3 fighters + 2 bombers.
squadMember0 = "fighter"
squadMemberCount0 = 3
squadMember1 = "bomber"
squadMemberCount1 = 2
squadReinforceAtYard = 1
```

ODF references are case-insensitive basenames, with optional `.odf` suffix;
this increment accepts ASCII letters, digits, underscores and hyphens, up to
127 characters excluding the suffix. Directory paths are not accepted.
Rows may be sparse; two rows may use the same member class while keeping
distinct slot identities. `squadReinforceAtYard` defaults to `1` and accepts
only `0` or `1`. The total count is derived from the rows.

The parser leaves identity and technology metadata to Fleet Operations. Cost
and time come from the completed, inherited member classes; repeated member
rows contribute separately. Negative/non-finite values, missing member metadata
and overflowing totals reject admission instead of charging a partial total.
The native
adapter installs `squadron -> craft` as a build descriptor host. The outer
Starbase completion hook consumes the job and suppresses the placeholder;
claiming the later Producer `FINISHING` event returned null to Starbase and
caused an `OutputQueueManager` null dereference at Armada `0x0053757B`.
`STARTING_EFFECT` is rejected for squadron targets when the shared dispatcher
is present. Admission is restricted to producers with a hooked completion
vtable. The first outer-completion build patched only Starbase's table and
rejected `fyard` (`classLabel = "shipyard"`) at admission. Both native tables
now route the same Armada completion routine through the squadron hook.

## Native integration status and next work

1. **Phase one implemented:** squadron ODFs are captured from inherited core
   field snapshots; build admission validates the composition; normal native
   payment/build-time/technology handling uses the derived member totals; the outer Starbase
   completion starts sequential real outputs, retires the build-queue job and
   suppresses the abstract aliased Craft. A failed newly constructed member
   is expired and detached; already launched survivors keep their squad.
   Aggregate physical unit-cap reservation is **not** implemented yet, so this
   is not release-complete queue admission.
2. Published members use Armada object handles as the current stable runtime
   `ObjectId`; cleanup and observed team changes feed `remove()` and
   `change_owner()`. The phase-one member resolver rejects known stationary or
   non-Craft labels but deliberately accepts other existing mod-defined labels
   until the SDK exposes a definitive native CraftClass predicate.
3. The native selection hook expands member selections through Fleet Ops'
   ordinary selection path, so normal commands use the full physical group.
   Read-only ShipDisplay getters present one tile per squad while retaining
   the physical command list. The icon shows the current/configured member
   count and health bars combine the live members. Control-group acceptance
   and broader synchronized command validation remain. The pure planner has no
   native UI side effects. Fleet Ops' physical selection limit is 30; the
   multi-ship panel has 16 tile controls, so large logical selections still
   need presentation design.
4. Native repair admission is observed at `RepairQueueManager::Activate`.
   Healthy depleted members pass the Fleet Ops `NeedsRepair` check; the
   override is suppressed during actual berth service so normal health repair
   finishes. All survivors must finish their native repair/output pass at the
   same owned yard, whose inherited `buildItem0`–`buildItem99` list must offer
   the squad ODF. They wait near that yard while replacements build.
   `native_repair.inl` reserves one missing slot, calls FeaturePack's existing
   `A2FO_ProducerPushRefit` checked Producer admission export, and associates
   the exact native queue ID with the reservation. RefitYards registration and
   Producer hooks are unchanged. Normal member cost, build time, technology,
   resource/cap checks and cancellation refunds remain native-owned.
   The outer Starbase completion supplies the actual result pointer after
   native output placement. Each replacement must leave output before the next
   is queued. Native queue cancellation, incompatible orders, leaving the yard
   area, capture and total loss terminate the visit. Stop/idle at the yard keeps
   waiting; cancel the replacement queue item or issue a movement order to end
   replenishment. A paid result arriving after invalidation remains an ordinary
   ship rather than being destroyed or attached to a different squad.
5. Add native save serialization, object reconnection and pending-job
   association. `Snapshot` is an in-memory contract, **not** Armada save/load
   support. Restoring a pending reservation must reconnect its original job,
   not charge again or start a duplicate. Restore is for a full mission reset,
   after old callbacks/jobs are quiesced, not a live merge with outstanding
   callbacks from another timeline.

The registry runs on the synchronized simulation thread. Only read-only
selection queries belong in presentation code. It has no clock, randomness,
native memory writes, input automation, hook registration or deployment step.

## Phase-one smoke test

A minimal squad ODF can now be placed directly in an ordinary yard build list:

```odf
classLabel = "squadron"
unitName = "Test Fighter Squadron"
// Cost and build time are three times fighter's normal values.
squadMember0 = "fighter"
squadMemberCount0 = 3
squadReinforceAtYard = 1
```

Replace `fighter` with a known working mobile ship ODF in the test mod. Expected
log order is: squad ODF registration -> queue admission -> sequential launch
start -> one `entered yard queue after prior exit` line per following member
-> launch completed. No extra placeholder ship should appear. Member
death should produce a removal line; capturing one member should detach it from
the original squad. Clicking one member should select all surviving members,
show one selected squad tile, and send a move/attack order to each member.
The tile uses the first selected member's image with a `live/max` badge.
In-game acceptance of this revision remains pending.

## Build, install and manual acceptance

From the repository root:

```sh
make build/modules/A2FOSquadrons.dll build/modules/A2FOFeaturePack.dll build/modules/A2FOResources.dll squadrons-test build/squadrons_native_test.exe
python3 tests/verify_squadrons_native_rvas.py \
  '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/ArmadaL.exe' \
  '/home/tamsynn/NVMeData/Fleet Ops Roots/FleetOpsHook.dll'
```

The source is `modules/A2FOSquadrons/module.cpp`, `native_economics.inl`,
`native_repair.inl`, `squadron_state.cpp`, `squadron_config.cpp` and
`thiscall_bridge.S`. Close the game before copying the three rebuilt DLLs
(`A2FOSquadrons`, `A2FOFeaturePack`, `A2FOResources`) into the game's
`Data/modules/` directory; keep copies of the previous DLLs.
The active mod must enable `A2FOSquadrons`
through its existing module configuration. This revision needs no new ODF
commands: retain the squad definition and its yard `buildItem` entry.

The current test installation uses `Data/Mods/test/odf/squads/fsquad.odf`
(three `fscout`, two `fassault`) and `buildItem8 = "fsquad"` in the test yard.
Start a **new skirmish** after restarting the game; squad state is not restored
from native saves.

1. Build `fsquad`: all five ships must visibly depart one at a time, with no
   extra placeholder. Build an ordinary ship afterwards to check yard reuse.
2. Single-click any member: all living members should select and receive a
   move or attack order. Shift-toggle should add/remove the whole squad.
3. Check one selected squad icon and `5/5`; select two squads plus an ordinary
   ship to check three logical slots and separate squad badges.
4. Damage different members, including one that is not the clicked member:
   the native health bar must reflect their combined current/max health.
   Kill a member: the badge should read `4/5`; repeat with the initial leader.

The log should contain `Squad panel renderer installed` at startup and
`Squadron selection panel projection installed` when the selection UI becomes
available. Selection diagnostics include the physical native count (five for
the intact test squad), even though the panel shows one logical slot.

The x86 adapter harness can run headlessly in an isolated Wine prefix:

```sh
env -u DISPLAY -u WAYLAND_DISPLAY \
  WINEPREFIX="$HOME/.cache/a2fo-squadrons-wine" WINEDEBUG=-all \
  WINEDLLOVERRIDES=winemenubuilder.exe=d \
  timeout 45s wine build/squadrons_native_test.exe
```

It uses mocked native objects and gateways. It does not launch Armada or
automate desktop input, and does not replace the in-game checks above.

## Validation

```sh
make squadrons-test
make build/squadrons_test.exe
```

The first target runs 17 scenario groups, including member cost/time totals,
overflow rejection, three-fighter and mixed
three-fighter/two-bomber cases, leader death, wrong-type replacements, capture,
repair cancellation, total loss, selection limits and corrupt snapshots. The
second builds those same tests for 32-bit Windows with assertions enabled.
The host tests also participate in `make test`.

Validation on 2026-09-20: all 15 scenario groups passed on Linux and in the
32-bit Windows console executable under Wine using an isolated temporary
prefix with displays disabled. Clang AddressSanitizer, UndefinedBehaviorSanitizer
and leak checking passed. The full repository suite passed (37 C++ test
programs and 18 Python tests). Results for the full suite are in the local
`build/squadrons-regression.log`; no game process was launched or DLL deployed.

These tests exercise the policy code. They do not prove the new in-game native
construction path, visual correctness, save compatibility or multiplayer. The
outer-completion fix therefore still needs an Armada/Fleet Ops smoke test;
that historical test run predates native repair integration.

Validation for the 2026-09-22 revision: 16 host policy scenarios and four x86
adapter regression groups passed. The x86 groups cover single-click and
toggle expansion, full-selection replacement, exact UI return-address filters,
physical command-list preservation, unequal member health capacities,
casualties, badge text/position and calling convention, render-state restoration,
sequential native output gating, yard loss and construction failure. The six
ShipDisplay call sites, Render vtable entry and text renderer were also checked
against the installed ArmadaL.exe without executing it.

Single-click follow-up: the live log exposed `native count=1` after expansion.
The fourth native argument (`[ebp+0x14]`) clears selection; the sixth controls
compatibility checks. The earlier mock incorrectly assigned clearing to the
sixth argument. The corrected regression reproduces the old failure. Peer
calls now pass zero for clearing, repeated selection audio and compatibility
filtering; the original click keeps its flags. Capacity checks and toggle
normalization also use the actual clear argument. The offline verifier checks
both flag branches in the installed FleetOpsHook.dll. Native tests cover a
single click on every member, clear-then-toggle, and independent clear versus
compatibility flags with a full prior selection.


## Paid repair reinforcement (2026-09-23)

Requires the FeaturePack `A2FO_ProducerPushRefit` and updated
`A2FO_ProducerCancelQueuedJob` exports. No new ODF
command is required: `squadReinforceAtYard = 1` is the default; set it to `0`
to disable replacements. The owned repair yard must list the **squad ODF** in
its build palette. Missing members use their own ODF's normal cost and build
time, one job at a time; they do not charge the whole squad cost again.
Survivors clear the repair berth before production starts. They must remain
within the yard's native bounding radius plus 512 world units. Production
rally points that send a replacement away can end the visit; keep the rally
point local when replenishing several missing members. This staging behavior
and the real resource debit/build progress still require in-game acceptance.
Native save/load and multiplayer acceptance remain unverified.

Build and check from `/home/tamsynn/A2FOHookExtensions`:

```sh
make build/modules/A2FOSquadrons.dll build/modules/A2FOFeaturePack.dll build/modules/A2FOResources.dll squadrons-test build/squadrons_native_test.exe
env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX=/home/tamsynn/.cache/a2fo-squadrons-wine WINEDEBUG=-all WINEDLLOVERRIDES=winemenubuilder.exe=d timeout 45s wine build/squadrons_native_test.exe
python3 tests/verify_squadrons_native_rvas.py '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/ArmadaL.exe' '/home/tamsynn/NVMeData/Fleet Ops Roots/FleetOpsHook.dll'
```

The native regression harness covers healthy depletion and the three-argument
NeedsRepair ABI, repair/recycle discrimination, actual berth admission,
all-survivor/output gates, rejected admission retry, exact job identity,
sequential replacements, queue cancellation, movement, distance, yard capture,
yard loss and total squad loss. Existing selection, health and initial-launch
regressions remain included. Static checks verify the new entry signatures,
repair vtable, command IDs, geometry layout and native completion result ABI.

Manual acceptance in a **new skirmish after restarting the game**:

1. Build `fsquad` at `fyard`, lose a scout and an assault ship, then repair the
   survivors at that owned yard. Repeat with full-health survivors.
2. After all survivors clear repair, verify one replacement build job appears.
   Confirm each missing type's normal resource cost and build time. Let its
   output leave before the following replacement queues.
3. Verify the badge returns to `5/5`, single-click selects every member, and
   commands and aggregate health include replacements.
4. Repeat with insufficient resources/full queue; verify it waits without
   duplicate jobs. Cancel the replacement in the yard queue, move the squad
   away, or capture/destroy the yard; verify replenishment stops.
5. Repair an ordinary ship and recycle a ship: neither should create a squad
   replacement. Verify a completely destroyed squad never returns.

Runtime log markers: `Native squad reinforcement enabled`, `Squad repair
accepted`, `Squad replacement queued at normal cost/build time`, and
`Paid replacement joined its squad and entered native yard output`.

### Repair hook conflict fixed

The failing game log contained `Native squad reinforcement hooks unavailable`.
FeaturePack had correctly detoured Fleet Ops' queue-deletion function before
Squadrons initialized; Squadrons incorrectly demanded that function's untouched
prologue. The repair installer now checks only the repair entries it owns.
Cancellation uses FeaturePack's exact native queue-ID export and preserves its
refund callbacks, bypassing grouped GUI slot remapping.

The x86 regression first reproduced the old initialization failure with a
FeaturePack-style JMP at the deletion entry, then passed after the fix. It also
rejects unknown NeedsRepair signatures, checks sequential paid replacements,
and verifies that member loading and aggregate pricing do not modify ordinary
ship classes. The pricing log starts `Squad economics 'fsquad':` and reports
the member count, base time and all ten costs.

Additional integration checks execute the production Resources payment/refund
paths and FeaturePack's exact-job export with an active grouped GUI context:

```sh
make build/squadrons_resources_test.exe build/squadrons_queue_bridge_test.exe
env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX=/home/tamsynn/.cache/a2fo-squadrons-wine WINEDEBUG=-all WINEDLLOVERRIDES=winemenubuilder.exe=d timeout 45s wine build/squadrons_resources_test.exe
env -u DISPLAY -u WAYLAND_DISPLAY WINEPREFIX=/home/tamsynn/.cache/a2fo-squadrons-wine WINEDEBUG=-all WINEDLLOVERRIDES=winemenubuilder.exe=d timeout 45s wine build/squadrons_queue_bridge_test.exe
```

Validation: 17 host scenarios, seven native adapter groups, the Resources
payment/refund integration check, the FeaturePack grouped-queue cancellation
check, installed-game static RVA checks and four documentation checks passed.
The DLLs are 32-bit PE files with system-only runtime dependencies. FeaturePack
retains its pre-existing unused-variable compiler warning.

For the current test fixture (3 `fscout`, 2 `fassault`), the explicit member
fields give base time `3 * 6 + 2 * 12 = 42` seconds, dilithium
`3 * 55 + 2 * 150 = 465`, metal `100`, crew `1030` and officers `14`.
Inherited values and native game/team modifiers remain authoritative. No
separate cost/time commands are needed on `fsquad.odf`.

This revision still requires a new-skirmish in-game check: verify the build
tooltip and debit, lose members, repair all survivors at `fyard`, wait for the
paid replacements to launch and confirm the badge returns to `5/5`.


## Checkpoint — 2026-10-28: A2FOSquadrons portable conversion complete

Rust now owns the remaining bar member traversal, health-value accumulation,
weighted colour admission/accumulation/division, repair-yard distance/radius
policy, retry arithmetic/admission, strict additional-cost parsing, peer snapshot
and duplicate-selection scans, and active-technology bounds/counter policy.
The existing Rust class/economics, production, squad registry, selection,
reinforcement and lifecycle controllers remain in use. Normal paid replacement
cost/build-time and native yard completion behavior are preserved.

C++ retains the exact hooks and addresses, checked engine-memory access,
thiscall/SDK/Win32 calls, native containers/string lifetimes, lock and exception
scopes, provider invocation, and native registration marshalling. Rust numeric
kernels reproduce the original GNU++17/O2 x87 instruction order and spills.
Whole-hook parity exposed an overflow C1 difference missed by isolated arithmetic
references; the final Rust values-output copy also preserves the original x87
reload/spill. Earlier failed evidence and candidate artifacts are retained. The new numeric
fixture also avoids a legacy test helper that forced PC24 for PC53/64
economics names; historical fixtures are preserved and actual control words
are asserted throughout the new matrix.

Validation passed: 229 Rust tests, formatting, host/i686 Windows Clippy, both
release archives, integrated/standalone DLLs and built-in batch9 admission.
All 134 original/current trace pairs (268 native executions, 8,200 equal
stdout lines) match across standalone and built-in forms. These include 12 x87
precision/rounding modes, 624 bar sample rows, 1,056 distance/radius rows,
336 full retry-update rows, strict parser cases, nested same-bar/provider reentry,
selection mutation, sequential launch/paid repair, and expected native hook
termination. Numeric fixtures compare raw float words, sticky flags/C1, control
word and stack balance. All 79 referenced Rust implementations link; 197
integrated and three standalone exports/ordinals are unchanged, with no new
system imports. Sixty-eight native functions/layout guards and SDK/ASM pins
remain intact.

The build pins 781 product inputs, including all 14 owned .inl files. Six cached
objects depending on owned .inl files were rebuilt; remaining incremental native
cache and 7,851 third-party source pins were authenticated. This prospectively
extends source coverage beyond earlier component maps; earlier receipts remain
unchanged. Canonical root artifacts and existing checkpoints are preserved.

Evidence: build/rust-migration-stage10/squadrons/checkpoint/validation.json.
Squadrons is completed_pending_census. Strict totals remain 116/151 with 35
uncredited records: six completed components await a shared census refresh,
and 29 other units remain. No strict archive/common-DLL binding was relaxed.
Candidate artifacts are isolated under squadrons/candidate. Nothing was deployed
and no game was launched; GPU and in-game acceptance, including a fresh-skirmish
paid reinforcement check, remain pending. This conversion does not establish a
fix for any previously observed in-game repair failure.
