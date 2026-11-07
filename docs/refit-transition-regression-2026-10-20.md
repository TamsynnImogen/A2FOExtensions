# Refit transition regression correction

The live report was reproducible in the preserved logs: entering Instant Action
could publish a newly allocated black backing canvas; loading switched the
renderer from 1920x1080 to 1280x800, rebuilt the provider device/assets, and spent
4,829 ms on its first frame while the departure clock was already advancing.
The native loading callback only presented at coarse progress intervals.
Evidence before changes is retained in
`build/refit-live-regression-20261020-094333Z/` (original logs, binaries, configs,
manifest and source-before copies).

## Changes

- ShellDisplay optionally seeds a fresh root backing canvas before its first
  partial paint. Refit supplies retained scene pixels for admitted setup dialogs;
  this callback does no worker startup or GPU work. Cached/child surfaces and
  failed seeds keep their existing native paths.
- The graphics worker retains its D3D9 device, provider, assets and shaders while
  replacing only output/depth/readback surfaces for a new resolution. A cold
  first draw no longer incurs an additional one-second adaptive idle.
- The provider excludes its first departure frame preparation from the camera
  clock. The native splash is prepared before starting a fresh departure epoch.
  Refit's camera swing, front-first ship departure and tunnel timing are retained.
- `LoadingAnimation=1` requests frames between native progress updates and after
  checked SOD/texture loading calls. Original calls retain their argument/return
  ABI and execute once. There is no artificial load hold or extra input pump.
  Each completed extra frame receives an adaptive idle of 3x its cost (67 ms to
  1 second), preserving loading work time. Existing native frames remain
  unthrottled.
- A loading texture is reused only when the checked splash and native device
  cleanup observers are available. Cleanup releases it before native device
  Release, including a display change within GameOpen, and at splash teardown.
  Changed cleanup signatures use transient textures. Frame-scoped state blocks
  are restored/deleted before returning. An opaque image covers the target;
  failed preparation does not clear native pixels to black.
- Existing A2FOInstantActionSettings race lookup detours are admitted only when
  the destination belongs to that loaded module and the untouched native tail
  matches. This restores player/faction information without replacing its hook.

## Evidence

The private headless scene fixture exercises the real provider at
1920x1080 -> 1280x800 -> 1920x1080 -> 1280x800, including 13 named views and
repeated departure. It retained one backend, published the first resized
departure frame in 427 ms, and measured a maximum UI exchange of 2 ms.
This is an isolated fixture measurement, not a native game launch benchmark.
The initial cold main-menu frame still cost 4,110 ms on the worker.
`scene-gpu.log` and `host/departure-contact.png` show the pan behind the fleet,
ship departure, tunnel, and a repeat launch from its initial pose.

The D3D8 fixture uses the actual production extra-frame function with synthetic
engine metadata and a real private device. It checks original overlay ordering,
full viewport/state restoration, rate limiting, refusal inside an existing
native scene, failed-config pixel preservation, cache reuse/revision/resize,
and release to the device reference baseline. The native console fixtures
check signature and installer failures, exact forwarding/order, inner call ABI,
and known versus foreign race detour ownership. These checks do not launch the
game or control user input.

Final checks passed: 12 portable suites, 38 console DLL cases, 18 loading
signature/forwarding cases against the installed binary data mappings, provider
and campaign bridge cases (20 flow checks total), five canvas/setup checks,
and both private GPU fixtures. Final evidence includes `portable-tests-final.log`,
`dll-console-final.log`, `console-results.json`, `canvas-results.json`,
`scene-gpu.log`, `loading-gpu-final.log`, and `final-source-binary-hashes.json`.
The final GPU test also invokes the production native cleanup observer and
checks the device reference baseline before the simulated engine release.

## Native acceptance

A complete game restart is required. Manually open Instant Action from the main
menu, return, and open it again; the retained scene should appear immediately.
Launch an IA match and observe the camera moving behind the selected fleet,
ships departing, and then the tunnel with player/team styling and minimap.
Return to menu and repeat; also check campaign objectives and gameplay arrival.
Fresh logs should report the root seed, one retained scene backend, output
surface resize, animation/cache admission, and native/extra frame totals at
splash teardown. Changing game display mode must release cached loading GPU
ownership before native cleanup. Actual whole-game load duration, visual
continuity, campaign/save/multiplayer timing and gameplay acceptance remain
manual; the headless fixtures cannot establish them.

## Deployment and rollback

Installed three DLLs (RefitMenus, RefitScene, ShellDisplay) and enabled
`LoadingAnimation=1` in the existing Refit INI. Other INI bytes were preserved.
The campaign catalogue, mission selector, native loadscreen ODF, renderer
settings and unrelated modules retain their recorded hashes. The receipt
[records before/after hashes](refit-transition-20261020-101837Z-deployment.json).
The verified backup is
`.deployment-backups/refit-transition-20261020-101837Z/`, including previous DLLs,
INI and live logs. With the game closed, restoring those four files to the
receipt target rolls back this candidate. Native acceptance is pending.
