# Refit renderer manual gameplay check

This guide covers a short, user-run visual check of the isolated renderer package at `/home/tamsynn/NVMeData/Projects/A2FO-Manual-Renderer-Test`. It is a separate full game copy with its own Wine 11.13 prefix. The original game install and shortcut remain untouched. Keep the test folder in place for this first pass.

This is a focused gameplay check, not full renderer coverage or physical-GPU, spot-light, shadow, or release acceptance. Unsupported meshes may fall back to the original renderer, so a dramatic whole-scene visual change is not expected. Fleet Operations HUD stays enabled; Refit UI is excluded.

## Spotlights (2026-11-07, current build)

The replacement children now run Core `10ecb1a5…` and Renderer `bdb3b522…`, which add ODF
spotlights. The Compare children and the Display are unchanged. Backup:
`candidate-backups/spotlights-1794052954195424535/`.

Stock Fleet Ops and Nexus ODFs define no spot lights, so **a normal match looks the same as before**.
To try one:
1. In the replacement child's `A2FORenderer.ini`, set `OdfLights=1` (under `[Refit]`) and
   `CaptureWorldLights=1` (under `[Native11]`). These were off for the first gameplay check; turning
   them on for real play is not yet tested beyond the private fixtures. `Check-Package.sh` will then
   report the changed config, which is expected.
2. Give a ship ODF a light, for example:
   ```
   lightSource = 0
   a2foLightSource = 1
   a2foLightType = "spot"
   a2foLightColor = "1 0 0"
   a2foLightIntensity = 3
   a2foLightDirection = "1 0 0"
   a2foLightFalloffStart = 1000
   a2foLightFalloffRange = 500
   a2foLightConeInner = 20
   a2foLightConeOuter = 30
   ```
   Cone angles are full angles in degrees, and the direction is in the ship's own space.

## Two-part ship check (2026-11-07, current build)

The package ran Core `a7384890…` before the spotlight build: the same behaviour as `6903cfdd…`, plus a `[RefitScene] coverage #N` summary line in the log every 30 s. That line shows how many draws the new renderer replaced and why others were refused, so please keep the session logs. It contains the moving-ship flicker fix you reported as much
better, a small edge-on triangle fix, and **two-part (hull + team-colour) ship support**.
`TwoGroupMeshes=1` is set in the two replacement children only; the Compare launchers are unchanged.
More ships are now drawn by the new renderer, especially ones with team-colour stripes. Examples
include Saber, Premonition, Talon, Eresis and many yards.

What to look for in a normal Duel or skirmish:
- **Team colours:** is your team colour on your ships, and the enemy's on theirs? Do the stripes
  sit in the right places?
- Any hull that looks inside-out, flickers, or shows the wrong texture, especially while the camera
  zooms (LOD switches) or a ship is damaged.
- Whether moving-ship brightness stays as good as on the previous build.

Rollback is automatic if staging fails. For a manual rollback, the previous files are in
`candidate-backups/coverage-telemetry-1794049898255614377/` (and the earlier `two-group-sliver-1794017479704549619/` for the build before that). Material colours are **not** in this
build.

## Moving-ship brightness retest (2026-11-07)

The first user DX12 replacement run reached native mesh submission and exited normally, but brightness/colour flickered while a ship moved. A focused candidate corrects a reproducible false geometry rejection under tiny motion; the reported visual cause is still unconfirmed. Its actual DLL passed static private100-second scene checks on both backends. The new candidate and staging status are recorded in `build/refit/geometry-motion-candidate/` in the project.

The user reports brightness is “much better” on Core854, which remains the current manual build. A separate guard-hardening candidate is being qualified privately. Future sessions pin the exact package input hashes before and after; input continuity is recorded by `input_identity_after.all_inputs_match`. Use **`Start-DX12.desktop`** and repeat the same ship movement, zoom and camera angle. Note whether brightness changes abruptly during motion, during turning, or while stationary. If it persists, exit normally and run `Compare-DX12.sh` separately on the same map/ship; report whether comparison mode also flickers. This comparison uses original FO mesh draws through the same native Display. Vulkan should be checked separately after DX12.

## Before starting

For double-click launch, use **`Start-Vulkan.desktop`** (or `Start-DX12.desktop`) in the test folder. These open a console and keep startup errors visible. A shell script double-click can hide an early failure.

Run these from a terminal in your normal desktop session:

```bash
cd /home/tamsynn/NVMeData/Projects/A2FO-Manual-Renderer-Test
./Check-Package.sh
./Launch-Vulkan.sh
```

1. Confirm the package is ready and run `Check-Package.sh` from the package directory. Do not start if it reports a package or configuration failure.
2. Start the Vulkan replacement using `Launch-Vulkan.sh`. It enables `LiveParts`, `SubmitMeshes`, and the scene/asset/texture/material/node capture flags.
3. Keep `HeadlessSceneTest` and `HeadlessLightProbe` off. For this first geometry/gameplay check, keep `OdfLights=0` and `CaptureWorldLights=0`.
4. Do not enable artificial spawning, fog changes, camera movement, or material controls. Use normal game controls only.

The launcher opens the menu; start Instant Action yourself. Leave the renderer INI files unchanged for this first pass. Normal game settings and saves stay inside the test copy. This first package is for offline single-player testing.

The desktop launcher supports filesystem and abstract XWayland sockets. Abstract-only X11 requires sharing the desktop network namespace; filesystem X11 retains network isolation. The real home and input devices remain hidden. Early errors are saved as `logs/prelaunch-error.json` and timestamped copies, even when no game session starts.

The comparison launcher keeps the same capture and observation settings but sets `SubmitMeshes=0`. It uses the same native Display route with original FO mesh draws. It helps isolate an explicit mesh problem; it does not switch to the original legacy graphics backend or make GPU work identical.

## Vulkan run

1. Start the game with `Launch-Vulkan.sh` and begin a normal single-player **Federation Duel** skirmish.
2. Select and move the Mediterranean constructor and Venture scout. Use ordinary zoom, pan, selection, building, and combat for 5–10 minutes.
3. Note visible issues, especially inside-out or missing faces, culling, lighting, textures, HUD/UI composition, and depth ordering. Unsupported meshes falling back is expected; record specific affected units or objects when possible.
4. Exit through the normal game menu. Do not force-kill the game unless it is unresponsive; note that if it happens.

## DX12 run

After the Vulkan session has ended, start a separate run using `Launch-DX12.sh`. Repeat the same map, faction, actions, and observation period as closely as practical. Exit through the normal menu before any comparison run.

If a specific mesh appears wrong, use the corresponding `Compare-Vulkan.sh` or `Compare-DX12.sh` only as a separate diagnostic session, and note the exact object and backend. Stop the comparison session normally. Use `Stop-Test.sh` only for a session owned by this package.

## Report and logs

Each run writes a session directory under `logs/<session-id>/`, including `after`, stdout/stderr, and evidence metadata. Share the last relevant session folder and report:

- Backend and mode (replacement or comparison), plus map/faction.
- Approximate duration and actions performed.
- Exact unit/object and what looked wrong; include when it first appeared and whether it reproduced.
- Any crash, hang, launcher/check failure, or unexpected fallback.

Do not delete session logs before they are collected. Rollback is simply to close the isolated test session and use the original game shortcut; no original files were changed, and no uninstall or deletion is needed.

## Verification status

- Package check passes for 55 pinned launcher/configuration/binary inputs; original executable/provider hashes are preserved.
- The hardware preflight enumerated the NVIDIA GTX 1660 Ti headlessly. It did not start a game or desktop window. Receipt: package `logs/gpu-preflight.json`.
- Nine process, file-path and environment safety regressions passed, with independent review.
- Vulkan and DX12 each passed a 150-second owned-Xvfb/software-GPU launcher check: native factory/source admission, fixtures absent, concurrent launch rejected, and owned Stop/prefix cleanup completed. Receipts are `build/refit-manual-package-checks/vulkan-1794008047973544792/receipt.json` and `dx12-1794008248208690975/receipt.json` in the source project. The first failed Stop attempt is retained as diagnostic history.
- These direct-map headless runs had an empty/fogged world without Instant Action setup, so they do not establish gameplay or live replacement coverage. Their intentional Stop is not a natural-exit test.
- Baseline user DX12 session `20261107T000335Z-7a4bfbab-dx12-replacement` proves desktop startup, native mesh submission and natural exit0; it also carries the brightness-flicker report.
- **Still yours to verify:** corrected moving-ship brightness on the new candidate, Vulkan desktop behavior, broader populated gameplay, visual correctness and stability. These results are not full renderer acceptance.
