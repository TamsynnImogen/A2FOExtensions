# Queue enhancements

The supported Armada Producer has a ten-item engine queue, and the build UI has
ten corresponding slots. The feature pack uses those native slots and Fleet
Ops' normal Producer path; it does not create a parallel construction system.

## Controls

- Ctrl-click a build item: issue ordinary synchronized build commands until
  all ten queue positions have been requested. Existing items naturally leave
  fewer free slots. The click sends one reserved typed-class marker while
  Armada's input/network command buffer is active. Every peer consumes that
  marker and performs the same checked native-queue insertions.
- Ctrl+Alt-click: send the continuous variant of the same synchronized fill
  marker. Every peer fills the queue, consumes the marker before Armada can
  install it as a live object command, then records the same repeat target.
- Select the active item normally: disable repeat without adding another copy.
- Select a different item, cancel/clear the queue, delete an item, or destroy
  the yard: disable the old repeat state.

On completion, the feature calls Fleet Ops' checked queue-push path. If costs
cannot currently be paid, the queue remains empty and retries every 30 Producer
simulation ticks. Fleet Ops' real synchronized command path remains the
authority for the initial fill; the extension does not duplicate its
availability rules with a separate probe.

## Save/load state

Repeat state is keyed at runtime by the synchronized Producer object handle. To
avoid changing Fleet Ops' save-stream layout, `Producer::Save` temporarily
places a magic value and the target project ID into two existing serialized
queue-ID fields, calls the original serializer, then restores the live values.
`Producer::Load` detects the marker, reconstructs the queue IDs from the loaded
linked list, resolves the class by project ID, and restores the sidecar state.

Old saves have no marker and load through the original path. This design still
needs real-save validation before release use; a collision with the magic value
is extremely unlikely but the serializer behavior must be confirmed in game.

## Safety and current status

All Armada and Fleet Ops functions used by synchronized queue fill and
continuous production are preflighted. The outgoing hook is on Armada's typed
`GameObject::QueueCommand` path for build orders, after Fleet Ops' UI has chosen
the Producer and target class but while the network command buffer is still
active. The matching receive hook consumes both marker variants. Hook wrappers
always chain the prior behavior when the queue enhancements are not fully
enabled.

Implementation and static/Wine DLL validation are complete. The following are
manual gates because the test harness cannot run an actual match simulation:

Observed in single-player testing:

- Ctrl-click fills the native queue.
- Ctrl+Alt-click fills it and automatically refills completed items.
- Manually deleting a queued item disables repeat; the remaining queue then
  completes normally.

Remaining manual gates:

1. Single player: partially filled queues; selecting the active item again;
   selecting a different item; resource pause/recovery; tech loss; queue clear;
   and yard destruction.
2. Active save/load: start continuous production, allow at least one automatic
   refill, save with items queued, reload, and confirm refill continues.
3. Paused save/load: activate continuous production, remove enough resources
   to force a retry pause, save/reload, restore resources, and confirm it
   resumes.
4. Cancellation and compatibility saves: cancel repeat before saving and
   confirm it stays cancelled after load; also load an older save made without
   the marker.
5. Multiplayer: two peers activate/cancel repeat and compare queue/state over
   multiple completions, including a temporary resource shortage.
6. Synchronization: confirm each fill marker produces the same queue count on
   every peer and continuous state activates after the checked fill.

The proposed green active/yellow paused build-button overlay and configurable
hotkeys are not implemented. AI yards are unaffected unless they receive the
same player broadcast command sequence.
