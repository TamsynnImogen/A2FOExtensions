# A2FOMissionSelector

`A2FOMissionSelector.dll` replaces Armada II's separate Single Player and
fixed ten-row Mission Select screens with one scrollable campaign/mission
browser. The four stock campaign sections deliberately retain native
`CampaignAvailable`, progression, mission filename-table, `SetupMission`, and
loading paths. Additional INI-defined campaigns reuse one native launch-table
cell temporarily, without enlarging or writing beyond Armada's fixed table.

The first version uses a dark modal Win32 dialog over the shell. If its native
tables or window cannot be created, it falls back to Armada's original Single
Player screen rather than blocking access to missions. Back, Escape, and the
window close button all return to Armada's main menu.

## Optional metadata

Mods may place `mission_selector.ini` in their root or `misc` directory.
Higher-precedence mods override parent values:

```ini
[campaign1]
title = Federation Campaign
overview = Command Starfleet through the opening campaign.

[campaign1.mission0]
title = Invasion
description = The Enterprise answers a distress call.
objectives = Investigate the system.\nProtect the Enterprise.
thumbnail = bzn/a2_fed01.png
```

Sections are `campaign0` through `campaign3`; mission sections append
`.mission0` through `.mission9`. Missing values fall back to native filenames
and built-in campaign labels. Mission thumbnails also auto-resolve beside the
corresponding BZN as PNG, BMP, JPEG, or JPG files.

## Custom campaigns and missions

`campaign4` and higher are custom campaigns. A custom mission must provide
`file`, which is the BZN filename passed to Armada's native mission loader:

```ini
[campaign4]
title = Bonus Missions
overview = Standalone stories outside the stock campaign progression.
unlocked = 1

[campaign4.mission0]
file = bonus01.bzn
title = The Long Night
description = A remote Federation colony has stopped responding.
objectives = Investigate the colony.\nRescue any survivors.
thumbnail = bitmaps/MissionSelector/bonus01.png
unlocked = 1
```

Campaign indices may be sparse from 0 through 127 and mission indices may be
sparse from 0 through 511; both are ordered numerically. Custom
campaigns and missions default to unlocked; `unlocked = 0` can hide their Start
button, but there is not yet independent saveable custom-campaign progression.
The supported display fields are:

- campaign: `title`, `overview`, `unlocked`;
- mission: `file`, `title`, `description`, `objectives`, `thumbnail`,
  `unlocked`.

By default a custom BZN borrows native launch cell Federation mission 1. The
advanced `nativeCampaign = 0..3` and `nativeMission = 0..9` fields may choose a
different temporary cell if a mission depends on native campaign identity.
The selected pointer is restored as soon as `SetupMission` returns.

Setting `file` on `campaign0` through `campaign3` also replaces that displayed
mission's native BZN for the launch only. This is a simpler alternative to
overriding `mshell.set`; native unlock state still applies to those slots.
