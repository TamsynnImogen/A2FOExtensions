# Built-in modules: seventh slice

The seventh slice moves six existing features into `A2FOExtensions.dll`,
bringing the core-provided inventory to eighteen names out of the existing
forty-eight. Each keeps its original module name and independent menu policy.
Standalone SDK targets remain available for compatibility builds, but normal
releases no longer require these six DLLs; stale standalone copies are ignored
by discovery.

## Preparation and lifetime

Every non-rejected built-in prepares once, even when initially inactive. Native
hooks and prepared state remain resident for the process lifetime. Selection
changes gate behavior; they do not unpatch hooks or unload code. Failed module
initialization rolls back its shared core registrations, including Breeders
aliases/defaults. Native hooks and private caches remain resident, with effects
disabled by unavailable gates. If initialization succeeds but later dispatcher
setup fails, the prepared Breeders schema remains registered; event effects
are disabled.

## Feature boundaries

- **A2FOEditMenu:** a generation change restores its own scratch state before
  the next native update. An off/on transition without a callback invalidates
  pending navigation. The original update is always called once.
- **A2FOM3D:** turning the feature off affects future resolver lookups. Existing
  resolver cache entries and loaded assets are retained for later activation;
  lookups while off use the native resolver. Built-in indexing retains old
  marked generated SOD caches on disk and ignores them for M3D precedence,
  rather than running the standalone module's cache deletion migration.
  Its existing automatic compatibility activation for managed mods remains;
  an explicit rejection disables it. While off, the native resolver can still
  see retained generated SOD files.
- **A2FOBreeders:** class aliases and ODF defaults are prepared for process
  lifetime, including while inactive; event behavior is gated. Activation
  changes discard transient waiting/retry work. Native seeds already spawned
  are not rolled back. Availability requires class, craft, and Producer
  support. Producer support requires the new FeaturePack export
  `A2FO_ProducerEventsReady`, which confirms completed queue hooks. An older or
  absent provider makes Breeders unavailable; use the updated core and
  FeaturePack together.
- **A2FOInfestation:** metadata remains prepared. Activation-generation
  changes discard pending infections and larva routes, but do not undo native
  hatches or larva effects already committed.
- **A2FOSwarmSystem:** render-only instances are dropped at the next
  simulation boundary after a generation change. Rendering stays suppressed
  until the new generation is synchronized. Cleanup never runs from menu or
  render callbacks. The built-in host store has process lifetime, avoiding
  native instance destruction from the core's CRT teardown; the OS reclaims
  any instances retained at process exit.
- **A2FOODFVariants:** race, base-ODF, and last-variant metadata remain
  available. Activation changes discard pending renderer refresh work; cleanup
  and post-load handling continue while inactive. Swaps occur on future active
  simulations. First observation uses the canonical base to avoid suffix
  stacking.

## Release and verification boundary

The stage 6 `A2FORefitMenus.dll` update remains required where that module is
used; this slice does not replace it. Focused checks are
`make builtin-tests builtin-native-tests verify-builtins`, the paired
`build/A2FOExtensions.dll` and `build/modules/A2FOFeaturePack.dll` targets, and
the standalone SDK targets `build/modules/A2FOEditMenu.dll`,
`build/modules/A2FOM3D.dll`, `build/modules/A2FOBreeders.dll`,
`build/modules/A2FOInfestation.dll`, `build/modules/A2FOSwarmSystem.dll`, and
`build/modules/A2FOODFVariants.dll`. Synthetic native fixtures do not start a
game entry point or exercise GUI/input, and passing them does not establish
native in-game acceptance. No deployment or manual acceptance is included.
See the [validation receipt](../build/builtin-stage7-validation.json) for actual
commands, outcomes, and artifact hashes.
