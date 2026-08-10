# Fleet Ops `info.ini` defaults

`A2FOInfoIni.dll` adds two optional fields to the active mod's `[mod]` section.
It owns parsing and path resolution; the core DLL retains only the early,
signature-checked user-directory and new-profile dispatch hooks. Ordinary Fleet
Ops code therefore continues to load and save the settings, while removing the
module restores native defaults without removing the core.

```ini
[mod]
DefaultGameSpeed = 5
SettingsDirectory = Armada Visual Upgrade
```

## `DefaultGameSpeed`

`DefaultGameSpeed` accepts an integer from `1` through `6`, matching the
Fleet Ops game-speed slider. A2FOExtensions replaces Armada's native
`GetDefaultUserProfileGameSpeed` result, uses the value when Fleet Ops
constructs a new profile, writes it into a first-run `Settings.xml`, and
applies it again at the operational profile-load boundary after Fleet Ops has
copied its settings. If the selected settings directory already contains a
completed profile, its saved `<gameSpeed>` value still takes precedence. This
makes the field a default rather than a forced setting.

## `SettingsDirectory`

`SettingsDirectory` redirects Fleet Ops' per-mod user directory. Do not add
`Settings.xml` to the value; the field names the containing directory.

- A bare name is another directory below the standard `mods` settings folder.
  `SettingsDirectory = STA2 Classic Mod` therefore shares that mod's settings.
- When the Data-level `info.ini` defines a settings directory, that directory
  becomes the shared root for active mods which use a bare name. For example,
  if Data selects `Z:\Portable\Fleet Ops Settings` and a mod selects
  `My Mod`, the mod resolves to
  `Z:\Portable\Fleet Ops Settings\mods\My Mod`.
- A relative path containing a separator is relative to
  `Star Trek Armada II Fleet Ops Config`. For example,
  `SettingsDirectory = profiles\Classic`.
- An absolute Windows or UNC path is used directly.
- Windows environment variables such as `%APPDATA%` are expanded.

The resolved path always has a trailing separator. Fleet Ops retains its usual
directory-creation behavior. Because this hooks Fleet Ops'
`getModUserDirectory` function, it redirects `Settings.xml` and any other
Fleet Ops feature which deliberately stores files in the current mod's user
directory.

Missing fields preserve Fleet Ops' original behavior. The fields are read from
the active mod's own `info.ini`; they are not inherited from `ParentMod`. The
Data-level settings directory is used only as the shared root for an active
mod's bare `SettingsDirectory` name.
