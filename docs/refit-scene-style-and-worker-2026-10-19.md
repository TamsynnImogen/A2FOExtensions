# Refit scene-style menu and asynchronous renderer — 19 October 2026

Historical record. The later [native-resolution/loading correction](refit-native-resolution-and-loading-2026-10-19.md)
supersedes this candidate's resolution limit and validation/deployment status.

**Withdrawn deployment:** the user reported a startup exception after this
candidate was installed. The previous menu DLL has been restored, and rendering
is disabled in the live config pending investigation. The captured exception
is in FleetOpsHook's early initialization; it does not establish that this menu
DLL caused the failure. See [the crash/rollback record](refit-startup-crash-and-background-config-2026-10-19.md).

The user's comparison showed two different upstream presentations: our boxed
shell-menu treatment versus Refit's open scene-menu overlay. Our request for a
system Segoe UI face also allowed Wine to substitute a different font. This
update follows the scene overlay while retaining Fleet Ops' native actions.

The user also reported cold-load/first-faction stalls and renewed lag after
entering a menu and returning. The preserved live log records the first frame
at **44164 ms**, later frames at 85 ms and 15 ms, and the reduced nebula bakes at
about 15–21 ms. The previous budget/profile change reduced recurring bake cost
but still ran all driver, shader and asset work on the native menu thread. Its
device was destroyed with the main form, recreating GPU resources on return.

## Changes

`presentation.cpp` now follows upstream `ui/src/menus/screens/main_menu.rs`'s
scene `overlay`: open left-aligned rows, 26-unit condensed text, a 68-unit
ARMADA II title, a 28-unit STAR TREK line, orange rule, and hover glow/accent bar.
Native localized captions are converted to Unicode and uppercased for display;
their native action bindings and original caption strings stay authoritative.
Map Editor remains the existing Fleet Ops action. Short-window scaling reserves
the title's rise above the menu column to avoid clipping.

The DLL and GDI fixture embed Noto Sans Condensed 2.015 Medium and SemiBold,
copied unchanged from the pinned upstream's `platform/vendor/` directory.
`AddFontMemResourceEx` registers these fonts privately; GDI drawing verifies the
selected face rather than accepting an arbitrary system substitute. No system
font is installed. The fonts have their own **SIL OFL 1.1** licence and source
notice under `third_party/armada2-refit/fonts/` and in the staged licence bundle.
Background scaling is bilinear and the scene outside the text column is no
longer globally dimmed to 73%. The 640-pixel GPU width cap remains in force;
interpolation smooths scaling but cannot reproduce native-resolution detail.

`menu_scene.cpp` now creates one worker on first demand. It copies the SDK's root
strings on the UI thread, then performs all provider loading, D3D creation,
shader/model work, drawing, readback and release on that worker. The provider's
existing owner-thread guard is respected. The worker owns a hidden, never shown
or activated `WS_POPUP` window for its independent windowed D3D9 device. It does
not use a game HWND as the device owner, call Present, change game display mode,
or generate input. Its own message queue is pumped while it waits for work.

`scene_mailbox.hpp` exchanges small requests and shared immutable CPU frames
under a mutex. No driver, parsing or logging callback runs under that mutex.
UI paint callbacks borrow a completed snapshot or paint their static fallback;
they do not wait for a render. Scene selection works during background loading,
with a preparing-background message. Stale draws from a superseded selection or
size are not published. Worker diagnostic messages are drained through SDK
logging on the UI thread.

Native submenus pause rendering and reuse a CPU snapshot. Main-form destruction
removes its timer/subclass but retains the private device, provider and caches.
On returning, the main form resumes that backend. Device recreation is limited
to a changed render size or error recovery. After a failed draw the worker
releases its device and retries after at least one second, at most three
consecutive failures; permanent failure keeps the last frame/static fallback.
Shutdown signals the resident worker without joining from a loader callback.

## Verification and limits

- MinGW `-Werror` module build and six portable suites pass. New mailbox tests
  retain an old frame through pause/resume, read snapshots while a producer is
  blocked, change requests during an in-flight draw, reject stale results, and
  preserve borrowed frame lifetime after replacement. Retry tests cover cooldown
  and tick wraparound. Layout tests cover the raised title on short windows.
- The offscreen Wine GDI fixture checks the actual bundled font face, open idle
  rows, orange title rule, hover/pressed/disabled states, display uppercasing,
  full right-side brightness and interpolated source edges. It creates no game
  window or input events. The preview is a synthetic rendering, not a game capture.
- The native console matrix and four-profile/30-signature audit are recorded in
  `build/refit-visual-native.log` and `build/refit-visual-bindings.json`. They check
  admitted native callback contracts, not a real game/rendering session.
- Independent read-only review and standalone Windows compilation found no
  concrete thread/lifetime blocker in the worker and retry paths.
- The Rust provider and its resources are unchanged from the previous
  Data-only asset-resolution correction. No engine hooks or addresses change.
- An optional GCC ASan check could not link because this host lacks its expected
  `libasan.so.8.0.0`; the normal portable threaded test passed. No sanitizer or
  actual GPU-worker acceptance is claimed.

The first-use shader/model cost still exists. Moving it to the worker avoids an
explicit UI-thread wait; driver-level contention, graphics startup, scene
appearance, actual responsiveness and device-loss recovery require native game
validation. The selected Fleet Ops models/textures also differ from the stock
assets in the Refit screenshot, so the scene will not be pixel-identical.
The full Instant Action Refit control-layout port remains unfinished.

## Deployment and manual check

The timestamped receipt records the prior DLL backup, preserved live logs, test
evidence, installed hashes and protected files. This update changes only
`Data/modules/A2FORefitMenus.dll` and adds the two Noto licence/source notices to
`Data/modules/refit/licenses/`. It keeps the renderer provider, menu profile,
faction model mappings, game binaries and accepted ShellDisplay DLL unchanged.

Deployed at local `2026-10-19 00:31:58`:
[receipt](refit-scene-style-worker-20261019-003158-deployment.json).
Rollback DLL: `.deployment-backups/refit-scene-style-worker-20261019-003158/Data/modules/A2FORefitMenus.dll`.
With the game closed, copying that DLL back to the matching game path restores
the previous candidate. Leave the added font attribution notices in place.
The backup also retains the two pre-update game logs and the test evidence.

Fully quit/relaunch Fleet Ops. Check that the main page has open condensed rows;
try navigating while the 3D background prepares, select a first-use faction,
visit Instant Action/Options and return several times. Confirm the selected
background returns without another backend creation, and verify both windowed
and borderless behavior. Fresh logs should contain `Refit scene worker:` and
`Refit worker timing: ... UI did not wait`. That log describes where work ran;
user-observed input responsiveness is still the acceptance criterion.
