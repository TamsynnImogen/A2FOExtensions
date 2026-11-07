# Team-change weapon activation

`A2FOTeamChangeWeapons` adds this opt-in **weapon ODF** command:

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

## Built-in availability

`A2FOTeamChangeWeapons` is built into `A2FOExtensions.dll`. Keep the module's
original name in the Modules menu and in `requiredX`/`rejectX`/`activeX`
selections; do not install or release a separate DLL. The legacy standalone
build target remains available for SDK compatibility, but the core ignores a
stale standalone DLL during module discovery.

The core prepares this module once for each non-rejected mod, including when
initially disabled. `requiredX` locks it on, `rejectX` excludes it, and
existing inheritance and live Save selection rules remain unchanged. It is
unavailable for the process unless its own ready state, the weapon-class
dispatcher, and the craft dispatcher are all available.

Team metadata is captured during preparation even while the module is off.
The live flag gates only future ownership-change requests. Turning it off
cancels queued requests; activation tokens ensure requests queued before an
off/on transition are canceled even if no simulation pass ran between those
changes. Cleanup and successful PostLoad retain the existing pending-request
cleanup behavior. Already-activated native effects and countdowns are not
reversed. See [built-in module stage three](../../docs/builtin-modules-stage3.md)
for selection and acceptance details.

## Standalone SDK compatibility

For standalone SDK compatibility builds only, a module consumer can install
the DLL in central `Data/modules` and select `A2FOTeamChangeWeapons` for the
mod. For core releases, use the built-in selection and do not install the
standalone DLL. An `info.ini` selection can use an unused `activeX` index, or
require the feature:

```ini
[modules]
required0 = "A2FOTeamChangeWeapons"
```

Do not overwrite another existing `required0` entry; choose the next free
index. The standalone consumer needs the required API (v4 revision 15 or
newer). Every multiplayer peer must enable the same module and ODF settings.

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
