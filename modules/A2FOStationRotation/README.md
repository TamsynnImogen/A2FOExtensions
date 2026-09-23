# A2FOStationRotation

Experimental station placement in 90-degree steps. During a construction
placement preview, **R** adds 90 degrees and **Shift+R** subtracts 90 degrees.
Each fresh press turns once; holding R does not repeat. Four presses return to
normal facing. The mouse wheel keeps its normal zoom behavior. R is reserved
for rotation during placement, so it does not also activate Repair. Outside
placement, R retains its ordinary binding. Chat/text entry, background focus,
and Ctrl/Alt combinations do not rotate. Cancelling placement or choosing
another station class resets the preview.

Install `A2FOStationRotation.dll` in `Data/modules` and select it with the Mods
screen's **Modules** button. A managed `info.ini` can instead add an unused row
to its existing `[modules]` section, for example:

```ini
active28 = "A2FOStationRotation"
```

No ODF or input-map changes are required. This uses the existing v4 core API;
other module DLLs do not need rebuilding for this feature.

The committed angle belongs to the build order. Later preview changes cannot
rotate an earlier order. Construction receives a full rotated station matrix,
so hardpoints and the native default shipyard rally/launch direction follow
the facing. This does not rotate an already-built station or change a rally
destination explicitly chosen by the player.

Placement clearance, the displayed footprint and occupied pathfinding cells
rotate around the station origin, including off-centre model bounds and unequal
directional margins. Cancellation, destruction and post-load registration use
the same calculation. Fleet Ops snapping, build-near restrictions, clearance
and path-planner options remain in the native chain. Construction-time
clearance reads the queued transform, not the current local preview.

All multiplayer peers must use the same enabled module version. Rotated orders
use reserved command markers which unmodified peers cannot interpret. The angle
is stored in a native saved command integer, then in native saved construction
and object matrices. Keep the module enabled when loading a save containing
rotated stations so their pathfinding footprints are reconstructed correctly.

This prototype has native policy tests and a headless x86 harness using the
current ArmadaL/FleetOpsHook images. The harness executes the assembly bridges,
Armada's footprint formula and its actual shipyard rally calculation. It does
not launch a game or establish multiplayer. In-game queues, save/load,
model-specific launch/repair behavior and multiplayer still need manual testing.
Arbitrary non-cardinal editor rotations are outside its footprint support.

Manual acceptance checks:

1. Place an asymmetric station at each angle beside an obstacle. Check that
   preview clearance agrees with construction and the completed footprint.
2. Queue different angles with one constructor and a HybridBuild producer.
   Change the preview afterwards; existing orders must keep their own facing.
3. Cancel construction and destroy a completed station. Check that ships can
   cross the released area and another station can be placed there.
4. Build a shipyard/repair yard at each angle. Check its exit hardpoint, default
   rally marker, departing ships and incoming repairs. Also try a custom rally.
5. Save/reload with a queued order, construction in progress and a finished yard.
   Repeat the footprint and rally checks before multiplayer testing.

Build and tests:

```sh
make build/modules/A2FOStationRotation.dll build/station_rotation_test
./build/station_rotation_test
make build/station_rotation_smoke.exe
```

From `build`, the headless smoke accepts Windows paths to the supported
`ArmadaL.exe` and `FleetOpsHook.dll` as its two arguments. It maps their bytes
without invoking either entry point. See the
[address and implementation audit](../../docs/2026-09-19-station-rotation.md).
