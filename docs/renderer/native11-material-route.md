# Base Craft material selector route

The world material producer now qualifies the base Craft render dispatch and
captures whether each actor reaches the native base/Borg texture selector.
This advances gate 53; it does not complete effective materials, native geometry
production or any of the nine implementation gates.

## Pinned branch and per-actor identity

The reviewed executable is SHA-256
`4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de`.
Craft render RVA `cb0e0..cb325` reads the class from instance `+0x98` and compares
instance database `+0x80` with class primary render database `+0x1dc`.
Only the equal branch reaches selector call RVA `cb2ab`, with that class as ECX
and the instance Borg byte at `+0xa1` as its argument. Both branches ultimately
call the common renderer at `cb318`. The saved material table's database field
at class `+0x1d8` is a distinct source and cannot establish this branch by itself.

The read-only route capture checks the world entity/instance/class identities,
current class version, exact instance vtable and its render slot `+0x14`, class
vtable and primary database twice. Core qualifies the complete base Craft render
body through the source profile. It never calls the native renderer or copies
cached instance Borg flags. Fresh logical preparation still supplies Borg state.

The ordered world batch brackets each table with route and preparation reads,
compares two full sweeps and rechecks routes after the final model/preparation
rewalk. A matching primary route may yield correlated candidate operations.
A nonprimary route explicitly reports `selector_bypassed` with no selector writes
or table read. An unknown dispatch or unreadable route reports
`route_unavailable`. Neither unavailable nor bypassed means an absent material.

## Publication and current access

The Core getter rechecks the route, current baseline/model/class/texture owners,
and the correlated table's count, pointer, ordered rows and mesh alternates.
It rechecks the route after table capture. In-place changes now revoke current
access even when owner pointers and versions have not changed. The retained
snapshot remains immutable. Baseline, code, thread and lifecycle guards remain.
Unknown detours are rejected; no additional game hook is installed.

## Evidence and limits

The new private CPU case executes the pinned original Craft render body on
owned synthetic buffers, replacing only its selector and final common-render
calls with register/stack recorders. All role pointers except the tested
optional database damage role are null, so no damage traversal, allocation,
simulation, startup or graphics code runs. Its **1,536** comparisons cover three
primary database relations, optional damage role and all 256 Borg-byte values;
class/argument identity and final common-call identity are checked. **40** source
byte, relocation and execution-permission mutations are rejected. The existing
9,600 original selector/slot-write comparisons remain separate coverage.

The world fixture checks bypass, bad dispatch, two-pass/cross-sweep/final-rewalk
mutations and failure atomicity. Strict host and Clang ASan/UBSan runs pass.
The Core fixture additionally checks post-publication primary database, table
row/count/pointer and alternate mutations while preserving valid logical parents.
All **161/161 final-artifact private cases** pass, including the original
selector and route CPU comparisons, Core mutation tests, GPU pixels and retained
service clients. Four changed production translation units pass strict i686
compilation; all eight affected Windows fixtures were rebuilt. The helper and
seven reference previews remain byte-identical; seven installed hashes and the
prior manual inspection package are unchanged. No new game startup is claimed.
Final artifact/build/runtime hashes are recorded by the
[material route checkpoint](../../build/native11/material-route-milestone.json).

Class material initialization readiness, complete selected-model dispatch,
independently owned prior slots, group/channel mapping and ordered extension
faction/subsystem/shader overlays remain open. No complete effective-material
or presentation authority is granted by this route check. The new Core is
staged only; no game is launched for this checkpoint. The earlier manual
inspection copy retains its prior startup-verified binaries and recovery state.
