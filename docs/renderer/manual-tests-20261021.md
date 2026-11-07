# Manual acceptance: renderer diagnostic foundation

Installed in `/home/tamsynn/NVMeData/Fleet Ops Roots/Data` on 2026-10-21. Receipt: [deployment-20261021-005222Z.json](deployment-20261021-005222Z.json). The game was closed during deployment; no game was launched automatically.

Only `A2FOExtensions.dll` and `A2FORenderer.ini` changed. The INI's existing bytes and sections were retained, with a new section appended:

```ini
[D3D8Compat]
Backend=d3d8
Trace=1
MaxEvents=8192
DebugLayer=0
```

Existing `[Renderer] Backend=dxvk`, effects, compatibility flags, menu modules, proxies and executable remain as installed. This first test is legacy rendering plus observation; full D3D11 game rendering is not implemented.

## First session: legacy path with tracing

Use your usual Fleet Ops Roots launcher and mod. Keep your existing resolution/window settings for this run; leave the renderer dropdown and effects unchanged.

1. Start normally. Let an intro play if your setup has one, then skip the remaining intro as usual. Note crashes, black screens, missing video or a new delay before the menu. State if intros were already disabled.
2. Wait for the 3D main-menu background. Visit Instant Action, return to the menu, and repeat once. Check scene/camera continuity, UI text, transparency, buttons and mouse alignment. Note any stutter or delay compared with the previous build.
3. Open options and the mod-manager screen, then return. Confirm their controls and artwork render and respond normally; changing mods or renderer settings is not required yet.
4. Open the map editor with a known working map. Pan/zoom, select a ship/station/map object and check textures/UI. Exit without saving changes.
5. Start a small Instant Action game with a familiar faction/map. Spend about one or two minutes moving/selecting ships, selecting a station, attacking something and looking at weapons/particles/shields and a planet/nebula where available. Include an existing M3D ship if this mod has one. Check alpha/glows/cloak only where you already have a familiar example. Try one normal alt-tab if it is ordinarily reliable on your setup.
6. Quit normally. Report which steps passed and the first failure, if any. For a failure, note the screen/action, mod/map/faction, resolution/window mode and whether it also occurred before this deployment. A manually captured screenshot is helpful for visual defects.

Please report after this session before repeatedly restarting: the unique `A2FOD3D8-*.jsonl` traces persist, but `A2FOExtensions.log` can be overwritten by the next start. Both are in `Data`; Codex can read and preserve them locally. No upload or full faction sweep is required for this first smoke test. These bounded diagnostics can affect performance, so report changes without treating them as final backend performance results.

## Second session: D3D11 diagnostic, after the first session is reviewed

Keep the game fully closed before changing settings. Preserve the first session's logs. In `Data/A2FORenderer.ini`, change only `Backend` **inside `[D3D8Compat]`** to `d3d11-probe`; leave `[Renderer] Backend=dxvk` alone. Use normal windowed or borderless mode, not exclusive fullscreen.

Start normally, reach the menus, then enter a small Instant Action game if no native D3D8 Present has happened yet. The first eligible successful windowed Present attempts one blue diagnostic frame with an SM5 gradient triangle and releases the temporary D3D11 device. The flash may be too brief to notice; the log must show `[D3D11] Probe complete HRESULT=0x00000000`. Subsequent frames should continue using D3D8. Verify input/UI alignment, textures, viewport and normal gameplay for about a minute, then quit.

The test establishes device/presentation coexistence in Armada's real window. It does not establish D3D11 translation of ships/UI or rendering parity. Exclusive fullscreen/resolution sweeps, all factions and heavy scenes follow after basic legacy observation and this probe pass.

## Recovery

To disable the observer/probe, close the game and set `[D3D8Compat] Backend=d3d8` and `Trace=0`, then fully restart. This keeps the deployed core but installs no optional D3D8 observation hooks.

The exact predeployment originals are at:

`/home/tamsynn/A2FOHookExtensions/.deployment-backups/d3d11-foundation-20261021-005222Z/originals`

For a complete rollback, close the game, preserve the new logs/config first, then restore **only** `originals/Data/A2FOExtensions.dll` and `originals/Data/A2FORenderer.ini` to the matching `Fleet Ops Roots/Data` paths. Other backed-up files are evidence/safekeeping; they were not replaced and need no restoration. `backup-manifest.json` and `deployment.json` record verified hashes.
