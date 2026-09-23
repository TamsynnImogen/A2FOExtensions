# Team-change weapon hook evidence

Implemented 2026-09-20 in `A2FOTeamChangeWeapons`, using the existing shared
WeaponClass loader and Craft cleanup/PostLoad API. No core/API mutation.

Armada II 1.1 symbols in the local reference `armada2.map` identify the
following RVAs, verified against the current Roots `Data/ArmadaL.exe`.

| RVA | Function | Checked prefix |
| --- | --- | --- |
| `0x000d0ea0` | `GameObject::SwapTeam(int)` | `55 8b ec 56 8b f1` |
| `0x000d0ed0` | `GameObject::SwapRaceAndTeam(Race const*,int)` | `55 8b ec 56 8b f1` |
| `0x0026eb00` | `Weapon::SimulateAll(float)`, cdecl | `55 8b ec a1` plus relocated address of the global at RVA `0x003b5564` |
| `0x00013800` | `Craft::GetCraftUnlessDestroyed(handle)`, called unchanged | `55 8b ec 8b 45 08` |
| `0x00271290` | `Weapon::Trigger(GameObject const*)`, called through the core's shared entry | `55 8b ec 8b 45 08` before shared hook installation |
| `0x00271300` | `Weapon::GetTarget`, called unchanged | `8b 49 38 51 e8` |

The first three entries are detoured at whole-instruction boundaries (6, 6,
8 bytes); copied gateway instructions have no relative branches. The absolute
list operand is checked using the loaded image base, including relocation.
All signatures are validated before registering callbacks or installing any
hook. On partial hook failure, the DLL stays resident with feature readiness
false and installed hooks chain native behavior.

The two ownership methods call virtual ClearTeam (`+0xe0`), virtual SetTeam
(`+0xdc`), then the native command reset at RVA `0xd1a40`. Activation must not
run while team bookkeeping is incomplete. ResearchStation ownership methods
at `0xb9cb0` and `0xb9d10` delegate to these base methods as well as handling
their pods. Initial creation uses SetTeam rather than these swap methods.

Native `Craft::GetCraftUnlessDestroyed` resolves the handle, requires type
bit `0x08` at Entity `+0x14`, and rejects Craft `+0x113` (destroyed). The
module additionally checks expired byte `+0x27`. Live weapons come from
Craft `+0x128` Carrier, vector begin/end `+0x0c/+0x10`, bounded to 256 slots
(matching the existing CraftIdentity guard). Weapon `+0x18` must reference
the same owner handle. Policies key on WeaponClass at Weapon `+0x04`.

`Weapon::Trigger(nullptr)` stores the native invalid target handle at `+0x38`
and sets active byte `+0x2c`. It does not execute effects itself. The later
native `Weapon::SimulateAll` loop validates each owner and dispatches virtual
Simulate at weapon vtable `+0x10`. This preserves native simulation order and
all weapon-class implementation checks. The new module calls the public
Trigger entry, retaining the shared extension prechecks.

The native WeaponClass constructor at RVA `0x264e30` reads `needTarget` into
class byte `+0x1de` (lookup at `0x265279`, name VA `0x7312fc`). When that byte
is set, the module passes the existing `Weapon::GetTarget` result to Trigger
instead; targetless weapons receive null. No new target or location is selected.
Weapon byte `+0x2d` is the native toggled-on flag, as demonstrated by
`Carrier::ToggleOffAllWeapons` at `0x267d80`. Configured weapons already on
are not triggered again, preserving their active state rather than toggling
them off. Native `SelfDestruct::Simulate` at `0x26cf80` uses this same flag,
copies its class `+0x250` countdown into Weapon `+0x3c`, advances it by elapsed
simulation time, then calls the craft's destruction vtable slot `+0xf8` with
the authored `+0x254` explosion multiplier. The integration test executes that
routine through countdown, recapture and expiry, stubbing only its message
and final destruction callbacks.

Fleet Ops' current ReplaceWeapon simulation at RVA `0x141584` reads this
active byte separately from `replacementInstantPlayer`, `replacementInstantAI`
and `replacementInstantDerelict`. Its manual activation path still evaluates
native readiness/resource policy. The new module does not modify private
ReplaceWeapon offsets, suppress costs, or force its effects directly.

Pending work retains a handle, pointer identity and resulting team; it
re-resolves the live owner and enumerates current weapons at dispatch. It
does not retain weapon pointers between simulation passes. Events are
processed in native callback order, never unordered-map iteration order.
Same-craft changes before the next pass coalesce to the latest owner.
Cleanup/PostLoad remove pending work. No extra save-stream fields are added.

The headless Wine harness maps and relocates the actual EXE bytes without
running its entry point. It uses the real hook installer and ABI bridges and
executes the three native functions above against synthetic state; only
external engine dependencies and per-weapon effects are replaced. In-game
capture/replaceweapon behavior and multiplayer still require manual testing.

Validation completed: `make test`, `make verify`, headless Wine DLL loading,
and the current Roots EXE native integration harness all passed. The harness
also covers the native self-destruct countdown through detonation and prevents
a recapture from cancelling it through this extension.

The new DLL is installed under Roots `Data/modules` and selected by the added
`active29` entry in `Data/info.ini`. The previous policy and deployment manifest
are in `Data/rollback/2026-09-20-team-change-weapons-014029/`. No weapon ODF was
changed; the feature remains opt-in per weapon. DLL SHA-256:
`a77cd6c7ba933591c9d087a8b21c9b0d7d9aa6ddbcc0167d1a343f0fc3f73868`.
