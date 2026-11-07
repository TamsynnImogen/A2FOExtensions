# Built-in modules: eighth slice

The eighth slice moves six existing features into `A2FOExtensions.dll`, bringing
the core-provided inventory to twenty-four names out of the existing
forty-eight. Each feature keeps its original module name and independent menu
policy. Standalone SDK build targets remain available; stale standalone copies
are deduplicated during built-in discovery.

## Preparation and lifetime

Built-in hooks and their callback storage remain resident for the process
lifetime. A policy toggle gates future behavior; it does not unpatch native
code. The two diagnostics modules additionally require their configuration INI
opt-in at startup: enabling their module policy after an INI-disabled startup
does not install hooks. Restart with the INI enabled to begin capture.

## Feature boundaries

- **A2FOFireArcs:** runtime metadata and behavior remain off when
  `firearc=0` in `RTS_CFG.h`; that setting retains the existing disabled
  behavior. The built-in name does not override this switch. With the runtime
  prepared, class metadata remains available across module-policy changes.
- **A2FOAnimations:** SOD source frames and ODF class/weapon metadata are collected
  while the feature is off. Activation-generation changes invalidate transient
  playback, launch, dock, and job state. Full behavior requires the class,
  craft, and weapon callbacks plus Producer dispatch confirmed by the updated
  `A2FOFeaturePack.dll` readiness export.
- **A2FOAlwaysShowShields:** configured class metadata remains available.
  Tracked native effects are stopped at the next game callback after the
  activation generation changes. Shared CraftClass, Craft simulation/cleanup,
  and render callbacks use the core-owned paths first, with the standalone
  observers retained as fallbacks.
- **A2FOStationRotation:** turning the feature off stops new rotation input,
  preview changes, and encoded order sends. Existing encoded orders,
  construction transforms, and footprint cleanup continue to be honored so
  native orders and placed objects remain consistent. Roots already implements
  station rotation natively; the extension is a no-op there.
- **A2FORadarDiagnostics:** counters and log capture are gated by both module
  policy and the startup INI opt-in. Selection alone does not start hooks when
  the INI was disabled at startup.
- **A2FORecoveryTests:** pending snapshots are invalidated across activation
  generations, and the observer releases its busy guard. The report directory
  is created only when active capture begins. The original pinned SHA, base,
  and complete-body admission checks are unchanged.

## Release and verification boundary

The stage 7 `A2FOFeaturePack.dll` and stage 6 `A2FORefitMenus.dll` companions
remain required where those features are used. Updated consumers that look up
core shield or animation exports—`A2FOCraftIdentity`, `A2FOTurrets`, and
`A2FOHybridBuild`—also need their companion builds. The validation receipt
records the exact required artifacts.

The explicit standalone SDK targets remain
`build/modules/A2FOFireArcs.dll`, `build/modules/A2FOAnimations.dll`,
`build/modules/A2FOAlwaysShowShields.dll`,
`build/modules/A2FOStationRotation.dll`,
`build/modules/A2FORadarDiagnostics.dll`, and
`build/modules/A2FORecoveryTests.dll`. The core build is
`build/A2FOExtensions.dll`; the FeaturePack target is
`build/modules/A2FOFeaturePack.dll`.

No deployment or manual game acceptance is included. The headless regression
suite passes 112 native fixture cases and the core export/dependency check.
It covers synthetic callbacks and gateways, including active arc decisions,
tracked shield cleanup, pending-capture invalidation, and bridge selection.
Recovery observer tests manually arm synthetic state after passive preparation;
they do not bypass or prove successful native SHA/base/body admission.
Native in-game acceptance remains pending. See the
[validation receipt](../build/builtin-stage8-validation.json) for exact commands,
outcomes, artifact identities, and the separate Refit compatibility checks.
