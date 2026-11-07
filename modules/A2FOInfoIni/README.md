# A2FOInfoIni

This feature is built into `A2FOExtensions.dll`. Keep using `A2FOInfoIni` in
the Modules menu and `info.ini`; its standalone DLL remains available for
compatibility testing and is not a release dependency. See
[built-in module behaviour](../../docs/builtin-modules-stage2.md).

`A2FOInfoIni` supplies the optional Fleet Ops `[mod]` defaults:

- `SettingsDirectory`
- `DefaultGameSpeed`

When enabled, the provider supplies these values to the core's existing
settings and game-speed dispatch hooks. When disabled, it returns no defaults,
so native behavior applies to future lookups. Settings, profile speeds, or
paths already loaded are not undone; the timing-critical hooks remain installed.

Data-level `SettingsDirectory` remains the shared root for a selected mod's
bare directory name. The active mod's `info.ini` supplies its own defaults;
parent-mod files continue to affect extension discovery but do not override
the active mod's settings policy.

