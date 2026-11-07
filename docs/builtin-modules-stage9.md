# Built-in modules: ninth slice

Stage 9 adds twelve existing modules to `A2FOExtensions.dll`, taking the
core-provided inventory to thirty-six of the existing forty-eight module
names. The other twelve remain standalone modules. Existing standalone build
targets are preserved, and stale standalone DLL copies are ignored when the
same module is provided by the core.

## Modules and startup policy

The stage 9 modules are `A2FOCheats`, `A2FOCollectiveXP`,
`A2FODirectionalShields`, `A2FOEnergySystems`, `A2FOHybridBuild`,
`A2FOMultiYardBuild`, `A2FONebulaRenderer`, `A2FOPointDefenseCycles`,
`A2FORefitYards`, `A2FOTurrets`, `A2FORGBTextures`, and
`A2FOTextureVariants`.

These twelve retain startup-only selection. Only selected or required modules
initialize. Inactive and rejected modules do not initialize, allocate their
feature resources, or install their hooks. Saving a changed selection in the
module menu reports that a restart is pending; it does not change the current
callbacks or native hooks. The previous twenty-four built-ins keep their
existing live policy gates. Built-in API exports remain present while their
feature is off.

When a provider consumer can use either implementation, it selects the core
provider only if the complete required symbol set is present; otherwise it
falls back to the loaded standalone provider. Each feature keeps its own
readiness checks. Engine compatibility profile behavior and the Oct 23
reference workflow remain as documented in
[`engine-update-profiles.md`](engine-update-profiles.md); the previous
deployed release remains the deployment baseline.

## Build and verification boundary

The focused build target is `make builtin-stage9-build verify-builtins`. Run
`make builtin-stage9-tests A2FO_BUILTIN_WINEPREFIX=/tmp/a2fo-engine-compat-wine`
for the native suite with an existing, user-owned headless Wine prefix. The
startup-policy fixtures use stubs for all twelve modules, and provider tests
cover both a complete core symbol set and a missing-symbol fallback. A production
adapter fixture checks invalid API rejection, inert off-state exports, stale DLL
deduplication, and early renderer policy. The original HybridBuild assembly and
namespaced adapter have identical `.text` bytes. The validation receipt is
[`build/builtin-stage9/validation.json`](../build/builtin-stage9/validation.json).

The build, export/import checks, 112 existing native cases, 14 Windows startup
policy cases, two provider cases, production-adapter checks, seven Oct 23 Refit
cases, and 50 engine-profile checks passed. Portable policy and profile-tool
checks also passed. Source, build, and headless results do not establish native
in-game acceptance. The three-file package passed independent review and was installed with a
hash-verified backup of every replaced file. Engine files, compatibility profiles
and module selections were preserved. The receipt lists the backup and installed
hashes; `rollback_update.py` beside it provides a verified dry run and an explicit
`--apply` restore. No game was launched; full-restart in-game acceptance is pending.
