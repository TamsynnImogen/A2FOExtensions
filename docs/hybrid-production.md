# Hybrid production queues

## Goal

A Producer-derived object should be able to expose all four native production
methods without disguising one method as another through `classLabel` or magic
ODF values:

```ini
constructItem0 = "fed_mining"
constructItem1 = "fed_yard"

yardItem0 = "fed_scout"
yardItem1 = "fed_destroyer"

researchItem0 = "fed_phaser_upgrade"

evolveItem0 = "bio_cruiser"
evolveItem1 = "bio_battleship"
```

`buildItem<N>` remains the compatibility route. The engine interprets those
entries from the object's normal `classLabel`. An explicit item command takes
precedence when resolving its target:

- `constructItem<N>` uses constructor placement and construction;
- `yardItem<N>` uses shipyard production;
- `researchItem<N>` uses research-station production;
- `evolveItem<N>` uses Evolver transformation.

The same target ODF cannot occur in two explicit method lists on one producer.
Armada's synchronized build order identifies the target class but does not
preserve which ODF command produced the button, so such an entry would be
ambiguous in multiplayer.

## Shared queue rules

The feature uses one ten-slot FIFO, matching the supported Producer and command
panel limits. A job records its queue ID, target project ID, production method,
and optional constructor placement. Only the front job can be active; the
other methods do not run in parallel.

Evolution is a terminal queue barrier:

1. Work already in the queue remains ahead of the evolution job.
2. The evolution job is appended at the back.
3. As soon as it is queued, every production list is locked against new jobs.
4. Cancelling that evolution job removes the barrier and reopens the lists.
5. When evolution reaches the front, it runs normally.
6. The replacement unit begins with its own empty queue and its own ODF lists.

The queue defensively clears impossible trailing state when an evolution job
finishes. This protects old or damaged save-sidecar data from transferring
orders to the replacement object.

Continuous production must obey the same FIFO. A repeat is appended at the
back after each completion, so an ordinary queued item cannot be starved by a
repeat target. Queuing evolution must disable repeat immediately and prevent
all refill attempts while the barrier exists.

## Implementation status

The platform-independent queue and build-list model is implemented in
`modules/A2FOFeaturePack/hybrid_production.*`. Host tests cover:

- all five command spellings, including legacy `buildItem<N>`;
- explicit-command precedence over the legacy fallback;
- unique slots and unambiguous target classes;
- deterministic mixed-method FIFO ordering;
- a single active job;
- evolution locking, cancellation, and completion;
- ten-slot capacity and queue-ID validation;
- repeat-at-the-back fairness.

For the first supported host, the commands are detected through Fleet
Operations' existing ResearchStation-class load callback and resolved through
the real ParameterDB/project-ID path into a class registry. At least one
explicit list must begin with item zero; once detected, sparse entries through
item 56 are retained. Parsing is intentionally not hooked at generic
ProducerClass level: doing so intercepts the complete startup class-loading
sweep and proved unstable in the live engine even though isolated hook smoke
tests passed.

The current playable candidate supports a ResearchStation host with distinct
runtime tables. `yardItem<N>` is exposed through the native Build button;
`researchItem<N>` is exposed through Research. If no explicit research list is
declared, the original ResearchStation primary list and its per-instance
upgrade-tier secondary list remain the Research table. Yard items are never
inserted into either upgrade-pod table.

Both categories feed the same inherited Producer FIFO at the native queue
offsets, with a capacity of ten and one active front item. ResearchStation's
vanilla busy result normally disables further choices while one item is active.
For a hybrid station, that UI-only result is relaxed during both Build and
Research refreshes while the shared queue has fewer than ten entries. The
execution, cancellation, save, and synchronized-order structures remain the
native Producer queue.

Fleet Ops classifies a ResearchStation as a plain single object despite its
inherited Producer queue. Its single-builder callback is not compatible with a
ResearchStation, so the adapter leaves that classification unchanged. It
intercepts Armada's two Fleet Ops-patched single-object dispatcher calls, runs
the original callbacks with their native ABI, and then applies a queue
post-pass while preserving the callback's result registers. Running last is
important because Fleet Ops' native single-object update can clear the
ResearchStation queue classes. The post-pass binds the ten native
`BuildWireframe` pointers at `ShipDisplay + 0x120` to a merged view of
`Producer::currentBuildClass` and the inherited Producer FIFO, deduplicated by
the active queue ID. Fleet Ops extends each 0x44-byte Armada object to 0x48
bytes and stores its `infoBuildQueueSlot_0` through `_9` frame control at
`BuildWireframe + 0x44`; the target `GameObjectClass` remains the native field
at `+0x3c`. This is necessary because ResearchStation can retain an active
upgrade pod outside the linked FIFO, whereas an ordinary Producer normally
leaves its active vessel at the FIFO head. One active/queued item draws one
wireframe, while mixed yard and research orders occupy successive native queue
positions. Fleet Ops' `ShipDisplay + 0x38c` array is deliberately untouched:
it contains weapon/buff system icons, not the production queue. Empty hybrid
and research-only stations do not opt into this adapter.

`researchItem<N>` follows the original ResearchStation code unchanged.
`yardItem<N>` uses Fleet Ops' generic Producer start/cancel/finish callbacks
and Armada's base Producer construction matrix, deliberately avoiding Shipyard
methods whose subclass-tail state overlaps ResearchStation pod arrays.

The menu adapter is driven by Fleet Ops' native palette modes: mode 2 refreshes
the yard table with Producer logic and mode 3 restores/refreshes the research
table with ResearchStation logic. This preserves separate Build and Research
buttons instead of combining both declarations into a single command list.
Fleet Ops normally binds Build, Research, Evolve, and Trade to the same
physical root-palette control because native classes expose only one of those
categories. When a single supported hybrid station is selected, the adapter
returns to root mode once and binds Build to the otherwise-unused adjacent
control. Research remains on the native shared control, so ordinary classes
and the native Evolve/Trade behavior are unchanged.

Manual in-game validation on 2026-08-04 confirmed the complete first
ResearchStation slice: Build and Research remain separate, each menu shows the
correct list, yard craft and research pods share all ten visible FIFO slots,
completed work advances without clearing the remaining queue, and a queued
unique pod disables its own research button without displacing a neighbouring
button. Ordering a pod also keeps the Research menu open. When the tenth slot
is occupied, the hybrid adapter suppresses the ordinary race insignia just as
native builder display does, and an active research pod draws its native
`<basename>_s` wireframe beside Fleet Ops' mouse-over progress bar.

That boundary is intentional. PDB class layouts show that `ConstructionRig`,
`Shipyard`, `ResearchStation`, and `Evolver` all inherit `Producer` at offset
zero, but store incompatible persistent state in the same subclass-tail bytes.
Calling a foreign subclass virtual method on an ordinary producer would
therefore risk corrupting pointers or crashing the game.

The complete runtime bridge still needs the remaining explicit,
signature-checked execution adapters:

1. Publish registered unique targets per host/method pair only when that exact
   executor adapter is available.
2. Carry the registered production method beside the synchronized native queue
   ID; never infer it later from the producer's `classLabel`.
3. Capture constructor placement when that job is ordered and replay it only
   when the job reaches the front.
4. Extend the ResearchStation yard/research slice to construction, evolution,
   and Shipyard-hosted research through adapters whose persistent state lives
   in separate sidecars rather than overlapping native subclass storage.
5. Serialize typed jobs and placements, rebuild sidecars on load, and validate
   identical ordering on two peers.

Until each remaining adapter is safe, its commands remain read-only in game
rather than displaying buttons that execute through the wrong subclass.
