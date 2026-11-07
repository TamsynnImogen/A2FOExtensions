# Refit scene pacing and Instant Action handover

The next native run confirmed that the PNG preview, loading layout and cached
texture cleanup were active. Camera motion and warp still stuttered, and entering
Instant Action still showed black. Logs and installed binaries/configs were
preserved in `build/refit-seamless-20261020-114247Z/before/`.

## Findings

The native run recorded 37 loading callbacks/presents in the first three seconds,
a longest gap of 1,513 ms and a total loading interval of 30,952 ms. Its worker
continued rendering a wall-clock animation between callbacks, so the game could
miss camera and ship motion while busy loading. Each discarded image also cost
a full GPU readback and CPU copy. The visible menu still used a 67 ms timer and
the old render-cost duty limiter. This differs from Refit's direct drawing on the
engine's graphics device; our retained offscreen renderer remains necessary for
the native GDI menus and its resource ownership stays on its worker thread.

The initial IA seed was copied before native registration, three milliseconds
after registration started. A normal root paint arrived only after 1,383 ms.
Copying to a hidden window does not protect a later show/resize clear. There was
also no distinct IA camera request: the provider remained on `main`.

## Changes

The renderer now keeps at most one completed frame ahead of its consumer. It
waits for acknowledgement before rendering another frame. The provider accepts
an optional, monotonic animation timestamp; each consumed frame advances it by
at most 50 ms. Long native loading steps therefore preserve the choreography
instead of jumping over it. Map hot reload still uses real time. Old providers
without the optional export retain their existing clock behavior.

Visible menu rendering and refresh now target 30 Hz, with a minimum four
millisecond yield on slow frames. Hidden/disabled menus still pause. No catch-up
loop, input pump, cross-thread use of the engine device or loading hold was added.

The native IA presenter and its admitted background asset explicitly request an
optional `<selected view>.instantaction` camera. Generic dialogs do not. An
authored `main.instantaction` view blends over the existing scene; returning to
the main menu requests `main` again. A missing alias retains the selected fleet.
Campaign IDs, expanded campaign counts and future scene storage remain separate
from this camera naming convention.

ShellDisplay replays the cached seed after native show/size messages, once the
root is visible, until its first normal paint. It never changes window visibility,
activation or input. The new optional canvas-completion callback reports the
actual physical presenter/copy result. Logical-DC draws release their worker
ticket only after success; refused copies retain the choreography. Direct GDI
draws acknowledge after drawing. Extra loading frames acknowledge after successful
EndScene/Present; normal loading frames acknowledge after the checked native
status renderer returns. Native Present failures inside that renderer remain
owned by the native engine; this is not a new interception of its Present call.

## Verification

The isolated GPU scene test exercised main/IA camera selection, generic-dialog
exclusion, named campaigns, resize, departure, a stalled consumer, restart and
return with one retained backend. It also replayed successful and failed physical
canvas completions. The initial run produced 84 frames in three seconds, including
225 ms for resize/first departure preparation. The final run, with the explicit
IA context and failed-redraw retry checks, produced 77 frames in three seconds,
including 501 ms of initial preparation. It sampled 179 departure frames with
a longest gap of 147 ms; maximum UI exchange was 15 ms. These are private
headless measurements, not native FPS results or a controlled performance comparison.

The D3D8 fixture checked production loading-layer drawing, native overlay order,
state restoration, successful acknowledgement, refused/open-scene/cadence/fallback
paths and resource cleanup. Portable mailbox checks cover duplicate/stale tickets,
old dimensions and a 1.5-second consumer gap advancing animation by only 50 ms.
The provider clock fixture checks invalid/backward timestamps, thread ownership
and lifecycle continuity. Shell fixtures cover hidden-window seeding, native
show/resize clears, completion failure and callback exceptions.

Native acceptance requires a full restart and main menu -> IA -> launch, then
return and repeat. Check the visible pan into IA, fleet departure, tunnel cadence,
and gameplay arrival. A blocking native asset load can still delay presentation;
this change preserves animation across that delay and avoids discarded worker
frames. Seamless appearance and performance parity with Refit remain unconfirmed
until the next native run. No desktop input or focus automation was used.

## Installed candidate

Final verification passed 12 portable checks, eight Wine canvas/content/render/
clock fixtures, 25 loading/provider/campaign flow cases and 38 DLL console
processes, plus the separate scene and D3D8 GPU fixtures. The final source and
binary hashes are in
`build/refit-seamless-20261020-114247Z/final-source-binary-hashes.json`.

Installed with the game closed into `/home/tamsynn/NVMeData/Fleet Ops Roots`:

- `A2FORefitMenus.dll`: SHA-256 `dba2034b6592fa9288e3a29f85b2dd99144c18f7de90612c2145a4ba2e282fc2`.
- `A2FOShellDisplay.dll`: SHA-256 `07d7c353fb58a977b4b5a79e989a33726871040da75999e3bf9d5d31475b22fc`.
- `refit/A2FORefitScene.dll`: SHA-256 `37402de2026ace29fc358272d1715f7f303538705fbafab4ec65290731969ec8`.
- `RefitScene/Menu/map.ini`: retained existing recipe content and appended the
  optional `main.instantaction` view.

All 59 present protected files retained their hashes, including settings,
themes, the map thumbnail, engine, renderer wrappers and unrelated modules.
Original files and logs are backed up under
`.deployment-backups/refit-seamless-20261020-120620Z/`.
The [deployment receipt](refit-seamless-20261020-120620Z-deployment.json) records
every before/after hash. Full-restart native acceptance remains pending.
