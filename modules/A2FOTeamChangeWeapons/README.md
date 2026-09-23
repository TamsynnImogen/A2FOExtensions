# Team-change weapon activation

`A2FOTeamChangeWeapons.dll` adds this opt-in **weapon ODF** command:

```text
activateOnTeamChange = 1
```

Default: `0`. Included ODF values are inherited; a child can disable the
command with `activateOnTeamChange = 0`. It is available to any weapon class,
including self-destruct, replaceweapons and ordinary weapons.

When its live ship or station changes team, each configured weapon receives
one native activation request on the next weapon simulation pass.
Ownership finishes changing first, so the weapon sees the **new owner**.
This works for AI and player owners and includes transitions to and from
team 0 (derelict/neutral), plus combined race-and-team swaps. Changing an
alliance without changing the object's team does not activate it. Assigning
the same team or changing only the race does not activate it either.

The command is an activation request, not a guaranteed shot: native technology,
resources, special energy, cooldowns and weapon-specific restrictions still
apply wherever the weapon's normal manual-activation path enforces them.
Shared A2FO weapon-trigger filters remain in the call chain. A rejected
request is not retried. Weapons with native `needTarget = 1` retain their
existing live weapon target if one exists. Otherwise the target is null and
the weapon relies on its own targetless behavior or automatic acquisition;
it cannot fire at an invented target. This does not select the capturing unit
or supply a new location for a ground-targeted weapon. The weapon's class determines what one
activation does; this command does not force continuous/toggle weapons to
become single-shot weapons.

For **self-destruct**, activation starts its normal authored countdown; it
does not shorten the countdown or bypass the native destruction behavior.
Already-on toggle weapons are left on. In particular, a second capture cannot
cancel or restart an already-running self-destruct countdown through this
command. Add the option to the self-destruct **weapon ODF**, not the ship ODF.

For a replaceweapon intended to activate **only on team changes**, also turn
off its independent native automatic activation options:

```text
activateOnTeamChange = 1
replacementInstantPlayer = 0
replacementInstantAI = 0
replacementInstantDerelict = 0
```

Native `replacementInstantDelay`/`replacementInstantCycle` govern the separate
automatic replacement path; this command submits a normal manual-style
activation, without adding a team-change delay.

## Installation

Install the DLL in the game's central `Data/modules` directory and select
`A2FOTeamChangeWeapons` for the mod in the Modules menu. An `info.ini` selection
can use an unused `activeX` index, or require the feature:

```ini
[modules]
required0 = "A2FOTeamChangeWeapons"
```

Do not overwrite another existing `required0` entry; choose the next free
index. The current core already supplies the required API (v4 revision 15 or
newer); this feature does not require rebuilding the core or other modules.
Every multiplayer peer must enable the same module and ODF settings.

## Lifecycle and validation

Initial native `SetTeam` during construction and restored ownership during
save loading are not activation events. The hooks observe completed native
`SwapTeam` and `SwapRaceAndTeam` calls. Unpublished, destroyed, expired and
non-Craft objects are ignored. Cleanup and successful PostLoad clear pending
requests; old requests are not saved or replayed after loading. Multiple
changes to the same craft before a weapon pass coalesce to one request for
its final owner. A capture occurring during a weapon pass is handled at the
next pass. No wall-clock timers or local-player decisions are used.

Headless integration tests execute relocated code from a private Armada
fixture for the ownership swaps, trigger and weapon loop, with synthetic
objects and effect callbacks. They also execute the actual native self-destruct
countdown through its detonation callback. They cover unconfigured weapons, same-team and
race changes, derelict transfers, rapid swaps, paused simulation, shared
trigger rejection, stale handles/owners, destruction, cleanup, load, and
existing targets, already-on toggles, and signature/partial-install failures.
They do **not** validate a live match,
replaceweapon effects, or multiplayer synchronization.

```sh
make build/modules/A2FOTeamChangeWeapons.dll build/team_change_weapons_smoke.exe
DISPLAY= WAYLAND_DISPLAY= wine build/team_change_weapons_smoke.exe /path/to/ArmadaL.exe
```
