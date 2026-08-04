# A2FOInfoIni native module

`A2FOInfoIni.dll` owns the optional Fleet Ops `[mod]` defaults previously
embedded in the core hook DLL:

- `SettingsDirectory`
- `DefaultGameSpeed`

The core retains only the timing-critical, signature-checked dispatch hooks.
It waits for deferred module registration before the first settings lookup and
then copies values supplied by this module. Removing this DLL therefore keeps
Fleet Ops' native settings paths and game-speed defaults unchanged.

Data-level `SettingsDirectory` remains the shared root for a selected mod's
bare directory name. The active mod's `info.ini` supplies its own defaults;
parent-mod files continue to affect extension discovery but do not override
the active mod's settings policy.

