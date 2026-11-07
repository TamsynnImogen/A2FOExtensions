# Built-in modules: tenth slice

Stage 10 moves the final twelve existing module implementations into
`A2FOExtensions.dll`, completing the core-provided inventory at forty-eight
of forty-eight module names:

- `A1Compat`
- `A2FOCraftIdentity`
- `A2FOFeaturePack`
- `A2FOLogMailer`
- `A2FOMissionSelector`
- `A2FOMissions`
- `A2FORefitMenus`
- `A2FOResources`
- `A2FOSOLMaps`
- `A2FOShellDisplay`
- `A2FOSquadrons`
- `A2FOVideo`

The twelve new built-ins retain startup-only selection, matching stage 9.
Only selected or required features initialize at startup. Existing per-module
`requiredX`, `rejectX`, and `activeX` policy controls remain effective, and
consolidation does not select or enable modules automatically. Changing a
selection still takes effect on restart. The earlier twenty-four modules keep
their live policy gates; the stage 9 twelve keep their existing startup-only
behavior. This consolidation does not introduce runtime native unpatching.

## Startup and compatibility boundaries

The built-in startup provider exposes its complete export set and a
`StartupReady` check so consumers do not mistake an inactive core feature for
a loaded provider. Standalone module build targets remain available, but a
stale standalone DLL is ignored when that module is provided by the core.
Selected implementations, native addresses and native assembly instruction bodies
are retained. Provider routing and off-state guards make startup ownership explicit.

Some related support files remain separate:

- `Data/modules/A2FOLogMailerHelper.exe` remains an optional utility for the
  mailer. Sending reports still requires explicit player consent.
- The A2FO `mmm_loader.dll` compatibility shim used by integrated mods remains
  separate. When the
  core accepts `A2FOMissions` at startup, the shim redirects to the core
  implementation; when the core blocks that feature, the shim prevents a
  stale standalone loader from acting as a fallback.
- The optional Rust Refit scene provider remains a separate utility and is not
  one of the forty-eight built-in module names.

Menu fonts and skin resources are embedded in the core. MMM's legacy C++20
objects and static FFmpeg libraries remain linked using isolated build flags.
The remaining support utilities mean that the package is not a single-file,
self-contained distribution.

The historical A2FOSquadrons repair crash remains unresolved by this source
consolidation. Preserve the existing module selection decision and consult the
Squadrons repair documentation before enabling that path.

## Build and verification boundary

The focused build target is `make builtin-stage10-build verify-builtins`.
`make builtin-stage10-tests A2FO_BUILTIN_WINEPREFIX=/tmp/a2fo-engine-compat-wine`
runs the old native suite and the final-batch fixtures.
The intended receipt is
[`build/builtin-stage10/validation.json`](../build/builtin-stage10/validation.json).
The build and export/import checks, all 112 existing native cases, and 51 final
fixture invocations passed. The latter include startup ownership, real-loader
ordering, mission routing, shell owner identity, all 12 FeaturePack callbacks,
inactive production exports, shared-library video decoding, and October 23
engine-profile and Refit checks. These headless results do not establish
in-game acceptance. Deployment status and rollback details are in the receipt.

The installed loader audit found one A2FO compatibility shim at
`Data/Mods/STA1 Classic Mod/missions/mmm_loader.dll` (the known prior shim
hash matches the earlier A1 deployment receipt). That is the deployment target
for the updated shim. The parent `Data/missions` loader and the SFC and Fleet
Ops 4.0 mod loaders are original MMM binaries and are preserved.

The two-file package was installed after independent source/package review.
Installed DLLs and every backup were hash-verified, and the rollback dry run
passed. Engine files, profile and INI/XML settings, mailer consent/helper, and
the three original legacy MMM loaders were preserved. No game was launched;
full-restart menu, Instant Action/match, save/load/exit, and campaign testing
where used remain manual acceptance steps.
