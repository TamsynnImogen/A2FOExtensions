# Hook and address register

This is the compatibility ledger for every binary address currently used by
A2FOExtensions. It separates locations that A2FO **modifies** from engine
functions, globals, return sites, vtables, and object-layout offsets that it
only calls or reads.

Unless a section explicitly says otherwise, every executable address is a
relative virtual address (RVA):

```text
runtime address = GetModuleHandle(image) + RVA
preferred address = PE ImageBase + RVA
```

Use RVAs when comparing builds. Preferred virtual addresses are useful in a
static disassembler but are not stable when Windows relocates an image.

The hook kinds used below are:

- **inline**: copy the checked whole-instruction prologue to a gateway, append
  a jump back, then replace the original prologue with `JMP rel32` and NOPs;
- **CALL**: replace a checked whole CALL instruction with `CALL rel32` and NOP
  any sixth or later byte;
- **JMP**: replace checked whole instructions with `JMP rel32` and NOP any
  sixth or later byte;
- **PTR**: replace a checked 32-bit function pointer in writable image data.

Expected bytes are the bytes required immediately before A2FO installs that
site. Most are original on-disk bytes. The two Armada ShipDisplay calls in the
HybridBuild table deliberately expect Fleet Ops' already-patched runtime
targets.

## Supported binary identities

The core rejects `FleetOpsHook.dll` unless its PE timestamp and `SizeOfImage`
match. It accepts the canonical `ArmadaL.exe` identity below, plus the
header-normalized form produced by ArmadaTextSave and related Fleet Ops tools.
That form has a save-time timestamp which changes on every rewrite and a
section-aligned `SizeOfImage` of `0x00404000`; the core therefore verifies its
preferred base, entry point, section layout, and the supported on-disk `.rdata`
fingerprint (`RVA 0x002ae000`, size `0x0003de90`, CRC-32 `0x8511f68c`). The
on-disk bytes are used because Fleet Ops can modify the loaded read-only image
before the extension core attaches. The same shared identity check is used by
core renderer hooks and optional native modules which depend on Armada code.
Every runtime patch still has its own exact instruction signature check.
SHA-256 and file size identify the exact local reference binaries used for this
audit; they are not otherwise runtime checks.

| Image | PE timestamp | `SizeOfImage` | Preferred base | File size | Reference SHA-256 |
| --- | ---: | ---: | ---: | ---: | --- |
| `ArmadaL.exe` (Armada II 1.1) | `0x3c4c76bd` | `0x00403999` | `0x00400000` | `4,810,516` | `9752cd058ea47090009f86836327a3d71f1c73dcdb923b62b0b6cba53bf85191` |
| `FleetOpsHook.dll` (supported Fleet Ops build) | `0x51f6475c` | `0x00322000` | `0x5a800000` | `3,043,840` | `30b4265c792cf5b53edafd9c25b694a051d6a7ba5a61c5c6d768a9addbdc67b2` |
| stock `Win2kDisableTaskSwitch.dll` | `0x2a425e19` | `0x0000a000` | `0x4a800000` | `16,384` | `db6071ae53ed0e5ca2855ba729b763ec7b68eba8c2cae95f61e3f7199548b035` |

The deployed stock startup DLL is renamed to
`Win2kDisableTaskSwitch.original.dll`; its internal PE/export name remains
`Win2kDisableTaskSwitch.dll`.

## Stock Fleet Ops startup load

Before A2FOExtensions existed, the stock `Win2kDisableTaskSwitch.dll` loaded
Fleet Ops from its DLL entry point. The ASCII string and its only code
reference are:

| Item | File offset | RVA | Preferred VA |
| --- | ---: | ---: | ---: |
| string `..\FleetOpsHook.dll` | `0x2d10` | `0x3910` | `0x4a803910` |
| `push` string reference | `0x2d00` | `0x3900` | `0x4a803900` |

The stock DLL has entry-point RVA `0x38f0`. The load happens sixteen bytes
later:

```asm
RVA 0x3900  push 0x4a803910       ; relocated pointer to "..\FleetOpsHook.dll"
RVA 0x3905  call RVA 0x3848       ; LoadLibraryA thunk
RVA 0x3848  jmp  dword ptr [RVA 0x6108] ; LoadLibraryA IAT slot
```

The ten bytes at RVA `0x3900` in the reference binary are
`68 10 39 80 4a e8 3e ff ff ff`. A2FO's startup proxy does not invent or
rewrite the Fleet Ops filename: it loads the renamed stock startup DLL first,
and this original code still performs the `..\FleetOpsHook.dll` load.

## Direct patch ledger

The maintained source currently contains 111 fixed-address mutation sites plus
one lazily selected per-class vtable hook. Conditional features do not
necessarily install every site in a given run, but no code or pointer write is
omitted below.

### Core: `A2FOExtensions.dll`

Source: [`../core/dllmain.cpp`](../core/dllmain.cpp)

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x0a85e0` | inline | 5 | `55 8b ec 6a ff` | `evolver_class_build_class_hook`; associate a completed Evolver class with its cocoon policy |
| Armada | `0x0a85d0` | inline | 6 | `c7 01 44 1b 6b 00` | `evolver_class_dtor_hook`; remove pointer-keyed cocoon state before class reuse |
| Armada | `0x0b0534` | JMP | 5 | `e9 a7 07 35 00` | `a2fo_cocoon_selector_hook`; choose cached/custom/default cocoon geometry |
| Armada | `0x400d10` | JMP | 6 | `8b 83 e8 01 00 00` | `a2fo_cocoon_update_selector_hook`; use the same geometry during cocoon updates |
| Armada | `0x134bf0` | inline | 9 | `55 8b ec 81 ec 00 01 00 00` | `parameter_db_get_int_hook`; preserve native integer lookup, then apply a registered classlabel default only when missing |
| Armada | `0x134cf0` | inline | 9 | `55 8b ec 81 ec 00 01 00 00` | `parameter_db_get_int_alternative_hook`; same policy for Armada's second integer getter |
| Armada | `0x134df0` | inline | 9 | `55 8b ec 81 ec 00 01 00 00` | `parameter_db_get_float_hook`; missing-only classlabel float defaults such as Wingman subsystem percentages |
| Armada | `0x134f50` | inline | 9 | `55 8b ec 81 ec 00 01 00 00` | `parameter_db_get_bool_hook`; missing-only classlabel boolean defaults |
| Armada | `0x135350` | inline | 9 | `55 8b ec 81 ec 00 01 00 00` | `parameter_db_get_string_hook`; classlabel aliases, declared ODF-field capture, and missing-only string defaults |
| Armada | `0x1354a0` | inline | 9 | `55 8b ec 81 ec 00 01 00 00` | `parameter_db_get_owned_string_hook`; missing-only allocated-string defaults such as `hotkeyLabel` |
| Armada | `0x135e80` | inline | 9 | `55 8b ec 81 ec 00 01 00 00` | `parameter_db_get_string_vector_hook`; missing-only repeated-string defaults such as `resourcesCanHandle` |
| Armada | `0x0cd1f0` | inline | 5 | `55 8b ec 6a ff` | `game_object_class_find_project_id_hook`; associate captured fields with the completed class and dispatch registered completed-class observers |
| Armada | `0x0c6ab0` | inline | 6 | `55 8b ec 83 ec 20` | `craft_explode_hook`; native destroyed-object dispatch and replacement publication |
| Armada | `0x13c8a0` | JMP | 6 | `b8 05 00 00 00 c3` | `default_user_profile_game_speed_hook`; supply the selected first-run game speed |
| Fleet Ops | `0x105fec` | CALL | 5 | `e8 23 3c 00 00` | `fofs_item_get_hash_lookup_hook`; FOFS item-get lookup dispatch |
| Fleet Ops | `0x1061e2` | CALL | 5 | `e8 2d 3a 00 00` | same dispatcher for item-locate |
| Fleet Ops | `0x106263` | CALL | 5 | `e8 ac 39 00 00` | same dispatcher for item-exists |
| Fleet Ops | `0x1063ee` | CALL | 5 | `e8 21 38 00 00` | same dispatcher for project-ID lookup |
| Fleet Ops | `0x10ab98` | inline | 5 | `53 56 57 8b f2` | `mod_user_directory_hook`; semantic `SettingsDirectory` override |
| Fleet Ops | `0x13e744` | inline | 5 | `55 8b ec 6a 00` | `fo_settings_get_instance_hook`; apply/save the first-run `Settings.xml` speed |
| Fleet Ops | `0x13e93c` | inline | 5 | `55 8b ec 51 53` | `game_configuration_new_hook`; initialize new configuration defaults |
| Fleet Ops | `0x13ea8c` | inline | 5 | `55 8b ec 51 53` | `game_configuration_load_profile_hook`; preserve the configured runtime default across profile loading |
| Fleet Ops | `0x10b98b` | CALL | 5 | `e8 98 72 0d 00` | `a2fo_race_parameter_db_dispatch_bridge`; dispatch registered completed-Race observers with Race in EBX and ParameterDB in EAX, then call the original Delphi destructor wrapper |
| Fleet Ops | `0x1bf89c` | inline | 5 | `55 8b ec 33 c9` | `a2fo_mod_settings_form_show_bridge`; call the native `TModSettingsForm.FormShow`, then add the main-DLL-owned Modules button |
| Fleet Ops | `0x1bef2c` | inline | 10 | `53 56 8b d8 8b b3 70 03 00 00` | `a2fo_mod_settings_launch_bridge`; validate required/rejected module policy before chaining `actLaunchNowExecute` |

The class-capture site at `0x0cd1f0` is installed when a completed-class or
destroyed-object handler registers. The explosion site at `0x0c6ab0` remains
destroyed-object-only. The Race call at `0x10b98b` is installed only when a
completed-Race handler registers and retains the original Delphi destructor at
Fleet Ops RVA `0x001e2c28` (`55 8b ec 51 89 45 fc`). The other core sites are
installed before class, ODF, settings, or profile loading as appropriate.

The Mods-screen bridge additionally calls the read-only NextGrid row helper at
FleetOpsHook RVA `0x0019f110`. Its synthetic first row stores a null data
pointer to represent Data; real rows store `TModificationInfo*`. The supported
`TModSettingsForm` layout places its HWND at `+0x1c4`, grid
at `+0x370`, Launch button at `+0x39c`, and Visit Website button at `+0x3a0`;
VCL `TControl` bounds are read from
`+0x40/+0x44/+0x48/+0x4c`. The selected grid row's opaque data pointer is at
`+0x08`; `TModificationInfo` stores the folder and title Delphi strings at
`+0x0c` and `+0x10`. These offsets came from the public FleetOpsHook map,
published-form RTTI, and disassembly of `ListMods`, `UpdateModSelection`, and
`FormShow` for the validated binary identity above. `TJetButton` is a
windowless VCL control and Fleet Operations repaints over ordinary native/GDI
overlays. The integration therefore constructs a genuine form-owned
`TJetButton` using its validated constructor at RVA `0x000dcb20`, attaches it
through `TControl.SetParent` (`0x000c55f0`), sets its VCL bounds and caption,
assigns the stock large-centred button images through
`FODialogAssignButtonImagesLargeCenter` (`0x001784c0`, style `8`),
and intercepts only `TJetButton.Click` at RVA `0x000dd260` to open the selector
for that one object. This uses Fleet Operations' normal control renderer and
event dispatch; it installs no input-device hook or capture.

When the active extension-root chain contains `a1compat.ini`, `A1Compat.dll`
transactionally registers `wingman -> craft` with the core-owned classlabel
dispatcher plus the 13 missing-only values from STA1 Classic's `a2craft.odf`.
It also registers the six `a2const.odf` defaults for the native
`constructionrig` classlabel and the seven `a2freight.odf` defaults for the
native `freighter` classlabel. The core remains the sole owner of the seven
checked ParameterDB sites, and no A1 default policy is active when STA1
Classic is absent.

`A2FOCheats.dll` owns the global Fleet Operations cheat extension:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Fleet Ops | `0x001fc320` | inline | 6 | `53 a1 <relocated current-player link>` | `show_me_the_money_hook`; retain Fleet Operations' registered command and multiplayer-cheat gate, then grant the current team the five Data/parent/active-mod `RTS_CFG.h` amounts (10,000 defaults) through the native Dilithium, Tritanium, Metal, Supplies, and Crew mutators |
| Fleet Ops | `0x001fccc8` | inline | 5 | `ba <relocated chat callback>` | `chat_hook_init_hook`; chain native chat initialization, then register `m`, `dis`, and `crash` and replace the incorrect `elim -> expl` registry entry with true selected-team elimination |

The handler validates the identical seven-byte prologues of the resource
mutators at Fleet Ops RVAs `0x001e35d0`, `0x001e35ec`, `0x001e3608`,
`0x001e3624`, and `0x001e3640`, plus Armada's
`Team::AddCrewCapacity(float)` prologue at RVA `0x000975b0`. Crew capacity is
increased before Crew because native
`Team::AddCrew(float)` clamps the available pool to that capacity.

Restored command registration validates `ChatRegisterCheat` at Fleet Ops RVA
`0x001fc2d0` and uses its Delphi dynamic array at RVA `0x0024a304`. Selected
crafts are resolved through the same selection (`0x002121bc`), entity lookup
(`0x0021275c`), and RTTI (`0x00211f04`) dispatch links used by native `expl`.
The command handlers signature-check and call Armada's
`GameTypeGeneral::Eliminate(int)` (`0x0007e8a0`),
`Craft::DisableShieldGenerator(float)` (`0x000ca010`), and
`Entity::GetTransform()` (`0x000cfd50`), then
`GameObject::QueueCommand(AiCommand, const Vector3&, long)` (`0x000d4490`)
through explicit MSVC-thiscall bridges. `m` reproduces the original local
`(0, 0, 200)` transform against the returned entity matrix and queues command
4. `crash` is deliberately process-terminating and performs no native object
mutation.

`A2FOCraftIdentity.dll` owns the ODF captain/registry and selected-object panel
extension:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x000bf090` | inline/JMP chain | 5/6 | stock `55 8b ec 6a ff`, or Fleet Ops' live `68 <handler> c3` resolving exactly to RVA `0x0010d6e4` whose handler begins `55 8b ec 83 c4 f8 53` | `craft_class_constructor_hook`; chain Fleet Operations' CraftClass enhancement, then copy the final `possibleCraftNames`, `possibleCaptainNames`, and `possibleCraftRegistry` string vectors from the completed ParameterDB |
| Armada | `0x000f3770` | inline | 6 | `55 8b ec 83 ec 08` | `selected_info_render_hook`; retain the complete stock state-1 single-object panel renderer, notify the bounded EnergySystems observer, and draw aligned captain/registry rows in the native component's local rectangle/display/scissor state |

The class hook accepts only Fleet Operations' exact checked CraftClass handler
or the untouched stock prologue. The companion fields read Craft's native
`possibleCraftNames` index at object offset `+0x218`, so they follow Armada's
existing random selection and save/load state without advancing the
synchronized RNG or changing the native save stream.

Supporting calls are signature-preflighted at Armada RVAs `0x001358f0`
(`ParameterDB::Get(DBRectangle)`), `0x00135ba0`
(`ParameterDB::Get(DBColor)`), `0x0011b160`
(`DisplayInterface::DrawText(rectangle)`), `0x000ce370`
(`GameObjectClass::GetOdfName`). The engine deallocator at `0x002527d0` is
required to remain readable; Fleet Operations may already route that public
entry through its active memory manager.
The module deliberately calls the core-owned/detoured string-vector getter at
`0x00135e80`, reads the GUI ParameterDB pointer at `0x0036502c`, reads Craft
object handle/class/name-index fields at offsets `+0x28`/`+0x40`/`+0x218`, and
reads `InfoDisplay`'s selected Craft and native captain text component at
`+0x1e8` and `+0xbc`. The captain component's live LTRB rectangle at `+0x58`
anchors both extension fields without reapplying the panel origin.
Every output passed to that string-vector getter must use Armada's full
16-byte layout: a four-byte allocator field followed by begin, end, and
capacity pointers. A three-pointer/12-byte temporary corrupts the caller stack
and reaches Armada's pointer-vector insertion at VA `0x00530106` with a null
destination.

The optional directional-shield ring resolves its four named GUI sprites from
Armada's `interfaceDB` pointer at RVA `0x00365030` through the checked database
lookup at `0x00220750` (`55 8b ec 8b 45 08 53 56 57`). This is intentionally
distinct from Fleet Operations' world-sprite database at `0x00212e10`. The
resolved ST3D sprites draw through Fleet Operations' checked colour and
scaled-2D helpers at `0x001e34b4` and `0x001e3498`; both begin
`55 8b ec 51 89 45 fc`. Partial facing fills temporarily crop the native
sprite texture window at `+0x38/+0x3c/+0x40/+0x44` and reduce the submitted
destination rectangle by the same centred proportion. This avoids changing
the shared viewport clip and lets forward/aft drain horizontally while
port/starboard drain vertically. The complete prior texture window and colour
at `+0x24` are restored after every submission. The module
reads optional per-Craft `forwardShieldPos`, `aftShieldPos`, `portShieldPos`,
and `starboardShieldPos` rectangles from the completed CraftClass ParameterDB.
They describe each visible segment's `x y width height` inside the graphic
area.

Normal and verbose per-facing tooltips inline-hook Fleet Operations'
`SelectionDisplayEnhancement.SelectionDisplay__GetTooltip_New_Wrapper` and
`GetVerboseTooltip_New_Wrapper` at RVAs `0x001e8b04` and `0x001e8f58`.
Both wrappers begin `55 8b ec 51 83 e9 24`; their original gateways remain
authoritative whenever the cursor is outside a rendered arc. Arc hit regions
use Armada's cursor globals at `0x00365018/0x0036501c`. The input update at
RVA `0x00119940` already converts physical mouse coordinates into the same
logical interface space submitted to ST3D, so the hit rectangles deliberately
remain untransformed. Text resolves through the localization manager
pointer/function at `0x003379fc/0x00081c90` and appends to Fleet Operations'
already-adjusted ostream through the import at `0x003b7dec` without
ResourceComponent's unrelated `+8` stream adjustment.
`ST3D_Sprite::DrawScaled2D` begins at Armada VA `0x0063ada0` and immediately
dereferences the sprite's frame-list sentinel at `ST3D_Sprite+0x1c`; a stale
post-mission sprite reaches VA `0x0063adbf` with that sentinel null. CraftIdentity
therefore keys its cache to the live interface database, validates every
sentinel before drawing, and uses the numeric fallback immediately after a
mission abort/restart. It does not enter the database lookup during that
transition; the same interface-database address must remain observed for 500 ms
before sprite resolution resumes.

The core owns the shared WeaponClass, weapon-trigger, and Craft lifecycle
boundaries used by FireArcs, NormalWeaponTech, WeaponDamageControls, Turrets,
and EnergySystems. EnergySystems uses the class and Craft lifecycle dispatchers
but no longer charges ammunition at the trigger-request boundary:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x00264e30` | inline/JMP chain | 5/6 | stock `55 8b ec 6a ff`, or Fleet Ops' checked handler at RVA `0x0010ef74` | `weapon_class_constructor_hook`; call the complete native/Fleet Ops constructor once, then dispatch copied requested ODF fields plus the live ParameterDB to every registered module |
| Armada | `0x00271290` | inline | 6 | `55 8b ec 8b 45 08` | `weapon_trigger_object_hook`; run every precheck, call native trigger only if all accept, then notify every handler that the trigger request completed |
| Armada | `0x000c6530` | inline/JMP chain | 7/6 | stock `55 8b ec 53 8b 5d 08`, or Fleet Ops' checked handler at RVA `0x001dcebc` | `craft_simulate_event_hook`; dispatch pre/post simulation notifications around the complete native/Fleet Ops call |
| Armada | `0x000c1fd0` | inline | 9 | `56 8b f1 8b 8e 34 02 00 00` | `craft_cleanup_event_hook`; notify sidecar owners before native cleanup invalidates the Craft |
| Armada | `0x000c2870` | inline | 5 | `53 56 57 8b f1` | `craft_post_load_event_hook`; notify handlers after successful native post-load |

`A2FOFireArcs.dll` owns the remaining optional three-dimensional weapon-arc
runtime hooks:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x0026f8c0` | inline/JMP chain | 6 | stock `55 8b ec 83 ec 1c`, or Fleet Ops' live `68 <handler> c3` resolving exactly to RVA `0x001358ac` whose handler begins `55 8b ec 83 c4 f4 53` | `weapon_can_fire_at_hook`; chain Fleet Operations' target filter, preserve native target/range/obstruction authorization, replace the stock directional gate, and apply the complete configured 3D volume |
| FleetOpsHook | `0x001ed458` | inline | 7 | `55 8b ec 83 c4 bc 53` | `ship_system_icon_render_hook`; preserve native `ShipSystemIcon` rendering, detect exact cursor ownership, resolve its live weapon-slot index, and draw the configured arc from every linked hardpoint |

The target simulation hook accepts only Fleet Operations' exact checked handler
or the untouched stock prologue, while the UI hook accepts only the exact
supported Fleet Operations render prologue. The UI hook is installed first,
but remains a pass-through until every site is ready. A weapon ODF without any
new arc commands never enters module policy: its existing `restrictFireArc`
byte and native `fireArc` path remain unchanged. For a valid custom policy, the
hook temporarily clears `WeaponClass+0x1b7` only while chaining native `CanFireAt`,
then restores it. This preserves target validity, range, and obstruction while
letting the custom box/cone own the directional decision, including pitch.
The same full decision is registered at the core's shared late trigger
boundary, closing any gap between target authorization and the actual shot.

The module reads numeric ODF angles through `ParameterDB::GetFloat` at Armada
RVA `0x00134df0`, with the active `ParameterDB::GetString` entry at
`0x00135350` used for mode names and quoted-number compatibility. It obtains
the live owner through `Weapon::GetOwner` at `0x00271050`, reads its matrix
through `Entity::GetTransform` at `0x000cfd50`, and reads the target position
from the same validated `GameObject+0xac` coordinates used by native target
authorization. A Weapon instance's WeaponClass pointer is at `+0x04`; Armada
stores `restrictFireArc` at WeaponClass `+0x1b7` and the stock `fireArc` float
at `+0x1b8`. The custom path does not modify either field persistently. Armada
Matrix34 uses right/up/forward/translation vectors at
`+0x00`/`+0x0c`/`+0x18`/`+0x24`.

The optional UI-colour path validates `ParameterDB::Get(DBColor)` at Armada
RVA `0x00135ba0` (expected `55 8b ec 81 ec 00 01 00 00`) and reads the active
GUI ParameterDB pointer from RVA `0x0036502c`. If either lookup is unavailable,
the hook retains its built-in boundary, centre, and valid-target colours while
the simulation feature remains active. The `RTS_CFG.h` `firearc` switch is
read through the extension-root API before any hook is installed, so
`firearc = 0` leaves both module-owned sites untouched and registers no shared
callbacks.

The hover preview additionally validates and calls Armada
`StandardComponent::IsMouseOverAndCursorOwner` at RVA `0x0010c140`,
`Entity::GetWorldTransform` at RVA `0x000cff90`, and
`DisplayInterface::DrawLine` at RVA `0x0011b130`. It also uses the read-only
`Weapon::GetTarget` entry at RVA `0x00271300` to resolve the live target before
testing it through the same pure arc geometry. `ShipSystemIcon+0x28` holds
the displayed Craft and `+0x30` its zero-based weapon index. Craft's live
WeaponSystem is at `+0x128`; that system's Weapon pointer vector begins/ends at
`+0x0c`/`+0x10`. Weapon's hardpoint-list sentinel is at `+0x10`, each list node
stores its SOD hardpoint at `+0x08`, and WeaponClass range at `+0x1c0` supplies
the bounded visualization scale. Line directions retain the same owner matrix
used by the firing gate while only their origins use each hardpoint's world
translation.

`A2FOWeaponDamageControls.dll` owns the two optional weapon damage-channel
commands:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x000c5bb0` | inline | 6 | `55 8b ec 83 ec 10` | `craft_damage_hook`; identify the firing WeaponClass from the embedded ordnance DamageInfo, copy the 32-byte record, add only the requested native damage flags, and pass the private copy through the complete native Craft damage path |
| Armada | `0x000c5f08` | JMP | 10 | `8b 43 04 c7 45 fc 00 00 00 00` | `a2fo_weapon_damage_hull_amount_bridge`; after native shield absorption/spillover resolution, multiply Craft::Damage's frame-local hull/system amount by the active per-hit hull correction, replay the displaced damage-type load and local initialization, then resume at RVA `0x000c5f12` |

Armada stores the firing Weapon pointer at Ordnance `+0x38`, embeds its
DamageInfo at `+0x3c`, and stores the WeaponClass pointer at Weapon `+0x04`.
DamageInfo flags are a 32-bit field at `+0x14`: native bit `0x01` bypasses
shield subtraction, while bit `0x80` zeros the later hull/system amount. When
`canDamageShields` is false and Craft `+0x1c8` still holds positive shield
strength, the module combines both bits so the shield blocks the complete hit
without losing strength. Once the shield reaches zero, a hull-enabled weapon
returns to the native hull path. The module only ORs flags, so an ordnance's
existing `ignoreShield`/shield-only state cannot be cleared by a default-true
command. The live `ParameterDB::Get(bool)` entry is Armada RVA `0x00134f50`.
The modifiers use `ParameterDB::Get(cLookup)` at RVA `0x00135630`, with native
`cLookup` construction/destruction/resolution at RVAs `0x0025cfb0`,
`0x0025cfd0`, and `0x0025d170`. That is the same project-ID table mechanism
used by stock `damageBase` and `hitChance`: the first number is the fallback,
followed by quoted target ODF/value pairs. The target Craft's class is at
`+0x40`, whose project-ID object pointer is at class `+0x1cc`. The outer hook
scales the copied DamageInfo by the resolved shield modifier while shields are
active, and the internal bridge corrects only any resolved spillover by
`hull / shield`; exposed-hull hits are scaled directly by the resolved hull
modifier. A zero shield modifier cannot produce spillover.
Weapon damage policy construction is registered with the core's shared
WeaponClass dispatcher, avoiding a module-order constructor chain.

`A2FODirectionalShields.dll` owns no second `Craft::Damage` patch. It owns
three separate shield-effect entry hooks:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x000743b0` | inline | 10 | `55 8b ec 6a ff 68 87 b5 69 00` | `create_shield_hit_hook`; for opted-in Craft only, bind a type-0 `xshldx01` effect to the immediately completed directional-damage facing (with active-scope and target-local matrix fallbacks), suppress creation on a depleted facing, write the face ratio to the returned effect, and register its ID with that facing; type-1 collapse and type-7 persistent-outline effects pass through |
| Armada | `0x00074770` | inline | 9 | `55 8b ec 83 ec 08 8d 45 08` | `stop_shield_effect_hook`; retire a tracked impact-effect ID when its native owner stops it, then chain the native stop routine |
| Armada | `0x000747d0` | inline | 9 | `55 8b ec 83 ec 08 8d 45 08` | `update_shield_effect_hook`; refresh tracked beam effects using the owning facing's percentage and stop them once that facing is depleted |

The module registers
completed-class and Craft simulation/cleanup handlers with the core, then
connects its bounded `BeginDamage`/`EndDamage` callbacks through
`A2FOWeaponDamageControls`' checked damage hook. Each module can initiate the
bridge handshake after the other is resident, so `info.ini` ordering is not a
runtime dependency. The selected facing is chosen
from `Entity::GetTransform` at Armada RVA `0x000cfd50` (expected
`8b 41 04 83 c0 44 c3`) and the firing owner from `Weapon::GetOwner` at RVA
`0x00271050` (expected prefix `8b 49 18 51 e8`). Ordnance stores its Weapon at
`+0x38` and embeds the 32-byte DamageInfo at `+0x3c`.

Craft `+0x1c8` is the native current shield aggregate and Craft's class pointer
is at `+0x40`. `CraftClass+0x208` is the native maximum shield pool used by the
stock UI ratio and shield-recharge ceiling. This must not be confused with
`WeaponClass+0x208`, which is a different layout and stores a project-ID
object. Hull current/maximum remain the independent `GameObject+0x15c/+0x160`
fields; directional shields do not write either hull field or reinterpret the
native `healthRate`. When `maxShields` is absent from an A1-era ODF, the module
writes the four-facing sum only to `CraftClass+0x208`. An explicit A2
`maxShields` remains native and must match that sum.

The same numeric offset has a different meaning on a live `Craft` object:
`Craft+0x208` is the signed native shield-effect ID, with `-1` meaning no
effect. `Craft::Damage` creates a type-1 collapse effect through
`ShieldEffect::CreateShieldHit` at Armada RVA `0x000743b0` after its temporary
shield value reaches zero, then stores the returned ID at `Craft+0x208`.
Directional `EndDamage` calls `ShieldEffect::ShieldStop` at RVA `0x00074770`
(expected prefix `55 8b ec 83 ec 08 8d 45`) and restores `-1` whenever the
struck facing is depleted. This happens before the four-facing aggregate is
restored. Normal weapon impacts use shield type `0` (`xshldx01`), but Armada's
ordinary weapon path supplies an identity matrix and creates the effect just
after `Craft::Damage` returns. `EndDamage` therefore retains the completed hit
facing until that following type-0 creation consumes it. Callers that supply a
target-local translation can still use the matrix fallback. Returned effect
IDs are tracked independently of the owning ordnance class. `EndDamage` stops
all tracked effects for a facing as it reaches zero, while the update hook
catches retained beam effects and prevents them being refreshed over exposed
hull.

`Craft::GetShields` at RVA `0x000c8a50` returns
`Craft+0x1c8 / Craft+0x1cc`. Native ShieldHit creation copies that ratio into
effect `+0xc0`; rendering converts it to the stock red-to-green colour
gradient. Matrix update at RVA `0x000747d0` does not refresh the colour. The
directional creation hook temporarily places
`aggregateMaximum * facingCurrent / facingMaximum` in `Craft+0x1c8`, restores
the exact prior value, and explicitly writes `facingCurrent / facingMaximum`
to the created effect's `+0xc0`. Tracked updates write the ratio directly by
finding the effect ID in the native tree whose head pointer is at Armada RVA
`0x00336dec`. These visual paths never reconcile or otherwise mutate the four
sidecar stores. Type `1` remains the native collapse effect and type `7`
remains available to the separately tracked `A2FOAlwaysShowShields` outline.

`A2FONormalWeaponTech.dll` installs no independent engine hook and registers
its precheck at the core's shared weapon-trigger dispatcher. A normal
WeaponClass stores its own four-byte project-ID object pointer at `+0x208`,
copied from `ParameterDB+0x34` by Armada's constructor in the same pattern as
`GameObjectClass+0x1cc`. The exported trigger filter obtains the owner through
Armada `Weapon::GetOwner` RVA `0x00271050` (expected prefix
`8b 49 18 51 e8`), reads `GameObject+0xec` for its team, and resolves that
team through the FleetOpsHook pointer at RVA `0x00212f08`. A non-null
technology item is evaluated by Fleet Operations' native recursive routine at
RVA `0x00120680` (expected prefix
`53 51 89 14 24 33 d2 8b 40 0c 8b 0c 24 8b 5c 88`). A null item is the
fail-open/default-`0` path. `WeaponClass+0x1b4` identifies special weapons,
which bypass this added filter because Fleet Operations already owns their
technology behaviour.

`A2FOEnergySystems.dll` registers its class and Craft simulation / cleanup work
with the shared core dispatchers above. Its optional selected UI registers with
CraftIdentity's bounded observer. Weapon `+0x04` supplies the WeaponClass
policy and `Weapon::GetOwner` at RVA `0x00271050` supplies the Craft whose
Photon or Quantum store is checked and debited. The normal Weapon ordnance
selectors at RVAs `0x0026fd40` and `0x0026fde0` run for each firing attempt,
even when Armada reuses an object from its ordnance pool. They reject a shot
which cannot pay and retain a successful selection until the common post-fire
commit at RVA `0x00270dd0` debits the declared cost. Fleet Operations'
`CannonImp` bypasses that complete path: its selector at FleetOpsHook RVA
`0x0013a550` gates ammunition, its guided launch at RVA `0x001392cc` debits a
successful projectile, and Armada's position launch at RVA `0x002679f0`
handles the alternate CannonImp branch only while that selector is pending.
Mode-2 resupply
uses GameObject class/team/position fields at `+0x40`, `+0xec`, and `+0xac`;
cleanup notifications remove provider and store sidecars before pointer reuse.
Its selected-info handler uses the native captain text component at
`InfoDisplay+0xbc`, its live rectangle at `+0x58`, the GUI ParameterDB pointer
at RVA `0x0036502c`, typed rectangle/colour getters at RVAs `0x001358f0` and
`0x00135ba0`, and the rectangle-aware text draw at RVA `0x0011b160`.
The module directly owns its per-launched-shot enforcement and persistence hooks:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x0026fd40` | inline | 10 | `55 8b ec 83 ec 0c 8b 45 08 57` | `weapon_select_target_ordnance_hook`; reject a target-directed shot when its Craft cannot pay, otherwise retain a successful pooled-ordnance selection for the commit hook |
| Armada | `0x0026fde0` | inline | 11 | `55 8b ec 83 ec 14 53 56 8b f1 57` | `weapon_select_position_ordnance_hook`; apply the same availability gate to position-directed normal-weapon shots |
| Armada | `0x00270dd0` | inline | 7 | `55 8b ec 51 56 8b f1` | `weapon_commit_shot_hook`; debit exactly once after a normal Weapon launch, then preserve the common native post-fire work |
| FleetOpsHook | `0x0013a550` | inline | 6 | `55 8b ec 83 c4 ec` | `cannon_imp_select_target_ordnance_hook`; gate CannonImp pooled-ordnance selections and mark its generic-launch fallback |
| FleetOpsHook | `0x001392cc` | inline | 6 | `55 8b ec 83 c4 a0` | `cannon_imp_launch_target_ordnance_hook`; debit one configured cost after each actual guided CannonImp projectile launch |
| Armada | `0x002679f0` | inline | 6 | `55 8b ec 83 ec 48` | `weapon_launch_position_ordnance_hook`; debit CannonImp's alternate position-launch branch without charging ordinary Weapon launches twice |
| Armada | `0x000c2340` | inline | 9 | `55 8b ec 81 ec bc 00 00 00` | `craft_load_hook`; chain native Craft loading, then restore and clamp the versioned Photon/Quantum store block for configured classes |
| Armada | `0x000c2980` | inline | 9 | `55 8b ec 81 ec 84 00 00 00` | `craft_save_hook`; chain native Craft saving, then append the two current store values for configured classes |

The persistence block uses Armada's byte writer/reader at RVAs `0x0012c680`
and `0x0012d7a0`. Existing saves made before enabling the module do not contain
that block and are intentionally rejected for configured Craft classes.

`A2FOPointDefenseCycles.dll` owns the optional numbered-delay runtime for the
two point-defense classlabels:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x0026c090` | inline | 5 | `55 8b ec 6a ff` | `point_defense_build_class_hook`; retain a validated `PointDefenseLaser` delay policy by the completed WeaponClass pointer |
| FleetOpsHook | `0x001f8794` | inline | 6 | `55 8b ec 83 c4 f8` | `ordnance_defense_build_class_hook`; retain the matching `OrdnanceDefenseWeapon` policy without extending Fleet Operations' class layout |
| Armada | `0x0026c180` | inline | 6 | `55 8b ec 83 ec 74` | `point_defense_simulate_hook`; update idle-reset state only when the pre-fire timer gate ran and no shot advanced the cycle |
| Armada | `0x0026c1d7` | JMP | 5 | `e9 44 58 19 00` | `a2fo_point_defense_pdl_timer_gate_bridge`; replace Roots' late Jan_B timing cave entry with a pre-fire timer gate, continuing target search at RVA `0x0026c1dd` only when ready and otherwise leaving through RVA `0x0026c3c7` |
| FleetOpsHook | `0x001f80cc` | inline | 9 | `55 8b ec 81 c4 54 ff ff ff` | `ordnance_defense_simulate_hook`; apply the same per-instance idle-reset rule around Fleet Operations' native interceptor loop |
| Armada | `0x00270dd0` | inline | 5 | `55 8b ec 51 56` | `weapon_reset_shot_timer_hook`; supply one selected `PointDefenseLaser` delay only while native `Weapon::mResetShotTimer` applies the existing owner/reload modifiers |
| FleetOpsHook | `0x00134ea4` | JMP | 25 | `8b 45 fc 89 c2 8b 52 04 d9 82 4c 02 00 00 d9 58 28 8b 55 e8 e9 31 35 0c 00` | `a2fo_point_defense_odw_delay_bridge`; replace only the successful `OrdnanceDefenseWeapon` class-delay assignment, then continue at RVA `0x001f83ee` with its weapon and candidate registers restored |
| Armada | `0x0026ebc0` / `0x0026ec50` | inline | 5 each | `55 8b ec 53 56` | `weapon_load_hook` / `weapon_save_hook`; chain the complete native fields first, then restore/append the next cycle index and idle-reset remainder only for configured point-defense classes |
| Armada | `0x0026eed0` | inline | 5 | `55 8b ec 6a ff` | `weapon_destructor_hook`; erase the instance sidecar before the common native Weapon destructor runs |

All sites are preflighted before the first patch. Numeric commands are read
through Armada `ParameterDB::GetFloat` at RVA `0x00134df0`, with
`ParameterDB::GetString` at `0x00135350` distinguishing malformed/quoted
values. Weapon stores its class pointer at `+0x04` and native shot timer at
`+0x28`; WeaponClass stores unnumbered `shotDelay` at `+0x24c`. Save support
uses Armada's ordinary `FileWriter` float/integer entries at RVAs
`0x0012ee70`/`0x0012ee30` and matching `FileReader` entries at
`0x0012efb0`/`0x0012ef70`. The FleetOpsHook RVAs include the PE `.text`
section adjustment; their linker-map segment offsets are `0x1000` lower.
Armada's base WeaponClass constructor does parse `shotDelay` into `+0x24c`,
but stock PointDefenseLaser never consults its inherited timer. Roots' replaced
branch at RVA `0x0026c1d7` entered a Jan_B cave which advanced the timer before
search but deferred its readiness check until after a firing attempt. The new
bridge retains direct scalar countdown and the successful-shot reset path, but
moves readiness enforcement before target search. It deliberately avoids
re-entering an engine timer routine from the already-active simulation frame.

Cycle policy and operational details are in
[`modules/A2FOPointDefenseCycles/README.md`](../modules/A2FOPointDefenseCycles/README.md).

`A2FOSwarmSystem.dll` owns dynamic render-only ambient traffic. It uses the
stock global `WorkerBee` colony only as a checked simulation/render frame
boundary; its members are independent `ST3D_Instance` values and never enter
Armada's `GameObject` list.

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x00067db0` | inline | 9 | `55 8b ec 81 ec 08 01 00 00` | `bee_colony_simulate_hook`; chain stock simulation, discover configured hosts, update host-local sidecars, and release absent hosts when `this` is the global worker-bee colony |
| Armada | `0x000688a0` | inline | 8 | `55 8b ec 51 8b 01 53 56` | `bee_colony_render_hook`; chain stock rendering, apply each live host transform, reuse native bee visibility, and draw the retained visual instances |

The module preflights every unshared native function it invokes before
installing its first hook. The core has already signature-checked and detoured
the two `ParameterDB::Get` entries by deferred-module load time, so the module
accepts either their stock prefixes or the core's five-byte near jumps. Fleet
Operations may likewise detour Armada's shared `operator delete`; the module
requires its live entry to remain readable and uses it rather than bypassing
the active memory manager.
Principal read-only bindings are:

| Binding | Armada RVA / layout | Expected prefix or role |
| --- | ---: | --- |
| global `WorkerBee` colony | `0x00336e48` | singleton identity used to reject the unrelated drone-colony passes |
| `GameObject::objectList` pointer | `0x00361084` | MSVC list head at `+0x04`, count at `+0x08`, object in node `+0x08` |
| `BeeColony::ShouldDrawBee` | `0x00068d50` | `55 8b ec 8b 45 08`; reads only the owner handle at synthetic Bee `+0xbc` before applying native visibility/spectator policy |
| `GameObjectClass::GetHierarchyRoot` / `GetOdfName` | `0x000cd940` / `0x000ce370` | checked SOD-root and native ODF-name access |
| `Entity::GetTransform` / `GetWorldTransform` | `0x000cfd50` / `0x000cff90` | checked host and hardpoint transforms |
| `Entity::GetBoundingSphere` | `0x000cfd70` | `8b 41 04 83 c0 34 c3`; host-local centre/radius used for lightweight host-mesh exclusion |
| `ST3D_Node::FindRecursive` | `0x00238780` | `55 8b ec 56 57`; named launch/interaction hardpoint resolution |
| `ParameterDB` constructor / destructor | `0x00134160` / `0x001341d0` | reopen each distinct host ODF through the native VFS/include/parent-mod path |
| `ParameterDB::Get(int/float/bool)` | `0x00134bf0` / `0x00134df0` / `0x00134f50` | native typed parsing for counts, movement values, and flags |
| `ParameterDB::Get(char*)` / string vector | `0x00135350` / `0x00135e80` | model names and the engine's existing quoted ODF list semantics |
| `ST3D_Instance` constructor / destructor | `0x0022e120` / `0x0022e1f0` | checked lifetime for the `0x84`-byte render-only instance |
| `ST3D_Instance::SetAnimationFlags` / `TriggerAnimation` | `0x0022e290` / `0x0022ea90` | native worker-bee-compatible animation initialization; static SODs are unaffected |
| `ST3D_Instance::ScaleGeometry` / `SetTransform` | `0x0022e2c0` / `0x0022e330` | per-definition scale and per-frame world transform |
| `ST3D_Instance::Render` / `SetDatabase` | `0x0022e750` / `0x0022eed0` | native shared-model render and SOD/database selection |
| engine `operator delete` | `0x002527d0` | release strings allocated by native vector parsing |

The `ST3D_Instance` linker-map segment offsets are `0x1000` below these PE
RVAs; the table deliberately records executable RVAs. Host identity uses
`GameObject+0x28`, expiration uses `+0x27`, and the shared class pointer uses
`+0x40`. The instance's logical/visible database pointers are `+0x7c/+0x80`.
Positions and targets remain host-local; interaction hardpoints are converted
back from their live world transforms on each approach/dwell update. Armada's
existing host sphere plus each visual instance's scaled sphere provides a
conservative exclusion boundary; swept movement is projected and redirected
tangentially on contact without enabling physics or collision. Bounded
sidecar-only pair relaxation prevents same-definition members from clumping,
including while dwelling at a shared hardpoint. Per-hardpoint sidecar
reservations cap concurrent approach/dwell traffic, and each visit must be
followed by a roaming leg. Save files contain no swarm records, so normal host
restoration reconstructs the ODF-defined visuals deterministically.

Configuration, caps, and lifecycle details are in
[`modules/A2FOSwarmSystem/README.md`](../modules/A2FOSwarmSystem/README.md).

`A2FOEditMenu.dll` owns recursive `buildItemX` navigation in Armada's map
editor:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x0011c610` | inline | 9 | `55 8b ec 81 ec c4 00 00 00` | `edit_menu_update_hook`; chain the native edit-menu update, inspect completed level transitions, replace the active native category slot when a selected `buildItemX` target itself contains `buildItemX`, and pop the plugin breadcrumb on native Back |

Armada's editor-menu table begins at RVA `0x00365038`. It contains 12 root
slots of `0x518` bytes; each slot contains its source/title pointers followed
by 12 native `0x6c`-byte build-menu records. The apparent `itemX` string array
at record offset `+0x0c` actually contains numeric `cPrjID` values produced by
`ParameterDB::GetProjectId`; companion class pointers begin at `+0x3c`. The
module resolves each configured leaf name through Armada RVA `0x000cd370`,
reads its canonical project ID through `GameObjectClass+0x1cc`, and publishes
both values. Armada's allocation routines at RVAs `0x00252710` and
`0x002527d0` own the temporary source/title strings. The module restores the
original root slot after the final Back operation. Only the overwritten update
entry requires its exact stock signature; the public resolver and allocator
entries need to remain readable because Fleet Operations may already detour
them to its active object database or memory manager. Parser and runtime details are in
[`modules/A2FOEditMenu/README.md`](../modules/A2FOEditMenu/README.md).

`A2FOInstantActionSettings.dll` owns the guarded Instant Action load repair:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x001c9430` | inline | 5 | `55 8b ec 6a ff` | `load_settings_hook`; count calls while transparently chaining Armada's complete native `LoadSettings(GameSetup&)` reader |
| Armada | `0x0012d7a0` | inline | 5 | `55 8b ec 53 56` | `read_blob_hook`; only during the tracked load, decode the exact 824-byte `setupDetails = <hex>` line after its separator whitespace, then return to Armada's normal validation path |
| Armada | `0x0008b180` | inline | 5 | `55 8b ec 53 56` | `race_by_id_hook`; preserve every valid race lookup and, only for the starting-unit caller returning to `0x00088b77`, substitute an empty Race table when a loaded setup names an unavailable race, preventing Armada's null `Race+0x32c` dereference at `0x00088ba7` |
| Armada | `0x001c2270` | inline | 5 | `55 8b ec 6a ff` | `multiplayer_setup_dialog_hook`; let the original setup dialog process every message first, recognize the replacement child button's `BN_CLICKED` command and the native mouse route, then invoke the same loader only when a Load-button action did not reach it |
| Fleet Ops | `0x001c7220` | inline | 8 | `55 8b ec b9 08 00 00 00` | `read_advanced_settings_hook`; transparently chain `TAdvancedOptionsForm.ReadAdvancedSettings` and report the live Ferengi/model-to-checkbox refresh path |

Armada's native `ShellMultipleToggleButton` label scans at RVAs
`0x001a6b47`, `0x001a6cd3`, `0x001a6dde`, `0x001a6e62`, `0x001a6ec1`, and
`0x001a6f21` can receive Fleet Operations option records whose inline label
bytes occupy the field expected to contain a C-string pointer. A narrowly
scoped vectored-exception guard handles access violations only at those exact
instructions, clears unreadable labels in the affected control, and resumes
with Armada's native empty string from RVA `0x003b7dc4`. The destructor read
at RVA `0x001a6d2e` has a matching null-label recovery path continuing at RVA
`0x001a6d4b`.

The module reads Fleet Operations' replacement Save and Load button pointers
at Armada RVAs `0x003a32dc` and `0x003a32d8`. Their wrapper bounds are the
signed 32-bit left/top/right/bottom values at `+0x14/+0x18/+0x1c/+0x20`.
When the Load bounds are incomplete, its working origin and the adjacent Save
control's dimensions reconstruct the exact rectangle; a fully empty Load
rectangle uses the form's six-pixel vertical row gap. An invalid Save control
disables the fallback rather than widening the click target.

The current multiplayer setup shell is read from Armada RVA `0x0036b8d4`.
The repair calls its checked no-argument `GetGameSetup` method at RVA
`0x00157940`, preserves the native host-only rule through
`GameSetup::isHost` at RVA `0x00146de0`, and then enters the tracked loader
gateway. A per-process invocation counter prevents a click already handled by
the original dialog from loading twice. The loader, enclosing dialog dispatch,
and Fleet Operations Advanced Settings refresh each report the live Ferengi
and technology-level state so a profile read failure can be distinguished from
a post-load overwrite or display problem. Details are in
[`modules/A2FOInstantActionSettings/README.md`](../modules/A2FOInstantActionSettings/README.md).

`A2FOBuildTooltips.dll` owns the checked build-button text extension:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x000e6ca0` | inline | 10 | `55 8b ec 6a ff 68 5b ed 69 00` | `button_text_bridge`; chain the complete normal `ModeInfo::ButtonText` formatter and prepare compact additional-resource/build-time tokens for type-1 build items |
| Armada | `0x000e72b0` | inline | 10 | `55 8b ec 6a ff 68 80 ed 69 00` | `button_verbose_bridge`; chain the complete verbose `ModeInfo::ButtonVerbose` formatter, including Fleet Operations' internal enhancements, with full additional-resource names and an expanded `N seconds` build-time token |
| Armada | `0x000e711a` | call | 5 | `e8 71 ab f9 ff` | `end_extra_lookup`; replace the normal tooltip's final `GUI_CP_END_EXTRA` result with compact additional costs, `N s`, and the localized closing bracket |
| Armada | `0x000e772a` | call | 5 | `e8 61 a5 f9 ff` | `end_extra_lookup`; replace the verbose tooltip's final `GUI_CP_END_EXTRA` result with full-name additional costs, `N seconds`, and the localized closing bracket |

For a build item, `ModeInfo+0x04` is the type and `ModeInfo+0x0c` is the target
`GameObjectClass`. The module obtains the local team through Armada RVA
`0x000d0060`, then calls `GameObjectClass::Get_Build_Time(team)` at RVA
`0x000ce290` (`55 8b ec 51 53 56 57 8b f9`). Producer uses that same getter
when construction starts. The getter conditionally applies the Instant Action
Ship Build Time multiplier according to native class flags. To cover Fleet
Operations build classes for which that condition leaves the result at raw ODF
time, the module also obtains the live GameSetup through RVA `0x00157940` and
reads `GameSetup::GetBuildTimeModifier` at RVA `0x00146360`. It applies that
multiplier only when the native result still matches `GameObjectClass+0x68`,
and therefore does not multiply an already-adjusted result twice. Unfamiliar
third values are retained because they may contain a native team/AI adjustment.
Game speed is not folded into the number: it controls the real-time pace at
which game seconds pass.

The two final `GUI_CP_END_EXTRA` localization calls are patched so non-zero
additional costs and the adjusted `N s` token are inserted before Armada
appends the native closing bracket. Source:
[`modules/A2FOBuildTooltips/module.cpp`](../modules/A2FOBuildTooltips/module.cpp).

### Ten independent resources

Source: [`../modules/A2FOResources/module.cpp`](../modules/A2FOResources/module.cpp)

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x00095670` | inline | 10 | `55 8b ec 6a ff 68 1d ca 69 00` | `team_constructor_hook`; discard stale pointer-keyed state so the four sidecar balances initialize from the live Race/start-resource setting |
| Armada | `0x00095850` | inline | 7 | `55 8b ec 51 53 56 57` | `team_destructor_hook`; remove the four sidecar balances before Team storage can be reused |
| Armada | `0x0010a350` | inline | 10 | `55 8b ec 81 ec 04 04 00 00 56` | `resource_component_tooltip_hook`; emit the local Race's configured short tooltip for native resource indices `0..5`, otherwise chain the complete native formatter |
| Armada | `0x0010a500` | inline | 10 | `55 8b ec 81 ec 04 04 00 00 56` | `resource_component_verbose_tooltip_hook`; emit the configured verbose native-resource tooltip, otherwise chain the complete native formatter |
| Armada | `0x000e6e1d` | call | 5 | `e8 6e ae f9 ff` | `native_resource_res_lookup`; cached `ButtonText` dilithium `Res` lookup |
| Armada | `0x000e6ea1` | call | 5 | `e8 ea ad f9 ff` | `native_resource_res_lookup`; cached `ButtonText` metal `Res` lookup |
| Armada | `0x000e6f25` | call | 5 | `e8 66 ad f9 ff` | `native_resource_res_lookup`; cached `ButtonText` latinum `Res` lookup |
| Armada | `0x000e6fa9` | call | 5 | `e8 e2 ac f9 ff` | `native_resource_res_lookup`; cached `ButtonText` biomatter `Res` lookup |
| Armada | `0x000e7029` | call | 5 | `e8 62 ac f9 ff` | `native_resource_res_lookup`; cached `ButtonText` crew `Res` lookup |
| Armada | `0x000e7462` | call | 5 | `e8 29 a8 f9 ff` | `native_resource_res_lookup`; cached `ButtonVerbose` dilithium `Res` lookup |
| Armada | `0x000e74de` | call | 5 | `e8 ad a7 f9 ff` | `native_resource_res_lookup`; cached `ButtonVerbose` metal `Res` lookup |
| Armada | `0x000e755a` | call | 5 | `e8 31 a7 f9 ff` | `native_resource_res_lookup`; cached `ButtonVerbose` latinum `Res` lookup |
| Armada | `0x000e75d6` | call | 5 | `e8 b5 a6 f9 ff` | `native_resource_res_lookup`; cached `ButtonVerbose` biomatter `Res` lookup |
| Armada | `0x000e764e` | call | 5 | `e8 3d a6 f9 ff` | `native_resource_res_lookup`; cached `ButtonVerbose` crew `Res` lookup |
| Armada | `0x000ffa40` | inline | 9 | `a1 cc 43 76 00 56 8b f1 57` | `resource_panel_render_hook`; chain all six native fields, then draw tritanium, supply, credits, and collective connections in configurable `resource_6..9` rectangles |
| Fleet Ops | `0x001226ec` | inline | 7 | `55 8b ec 83 c4 c8 53` | `a2fo_resources_deduct_hook`; enforce the four extra costs, chain the authoritative six-resource Producer transaction, and debit sidecar balances only after native success |

The module reads `Producer+0xf0` for its Team and the completed Race pointer at
`Team+0x244`. It obtains the selected GameSetup through Armada RVAs
`0x00157940` and `0x0036b8d4`, calls the normal/lots resource selector at
`0x00146310`, and reads the infinite-resources flag at `0x00146400`. Completed
object/Race ODF fields arrive through the core-owned revision-13 dispatch
sites. FeaturePack's shared Producer owner supplies cancellation,
single-delete, and clear notifications for exact custom-cost refunds.

The panel obtains the local team and pointer through Armada RVAs `0x000d0060`
and `0x00096340`. It reads optional raw GUI rectangles using
`ParameterDB::Get(DBRectangle)` at `0x001358f0`, the GUI ParameterDB pointer at
`0x0036502c`, and draws through the native rectangle-aware text function at
`0x0011b160`. One live panel text component provides the shared font/display
context and coordinate scale for all four independent `resource_6..9`
rectangles; no added resource is paired with a native balance. The native
six-value `ResourceInterface` layout is never enlarged or overwritten. Cursor
coordinates at `0x00365018/0x0036501c` select the added rectangle whose short
and verbose localized text is assigned to the existing panel-background
component through `StandardComponent::SetTooltipText` at `0x0010c040` and
`SetVerboseTooltipText` at `0x0010c080`.

`A2FOMissionSelector.dll` owns the single-player shell replacement:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x001dbd90` | inline | 6 | `8b 0d 08 d5 7a 00` | `do_single_hook`; open the combined campaign/mission browser, falling through its gateway to the complete native Single Player dialog if catalog or dialog creation fails |
| Armada | `0x001d6d50` | inline | 6 | `55 8b ec 8b 45 08` | `do_mission_select_hook`; normally retain the native Mission Select dialog, but accept exactly one preselected mission while the replacement invokes native `SetupMission` |

The module validates native `SetupMission(HWND, int&)` at Armada RVA
`0x001dcc00` (`55 8b ec 6a ff 68`) before installing either hook. It validates
and calls `CampaignAvailable(int)` at RVA `0x001dbd60`
(`55 8b ec 8b 45 08`), reads the current campaign at
`0x003a89a4`, selected mission byte at `0x003a89ac`, campaign progress bytes at
`0x003a89ad`, tutorial count at `0x003a89b5`, and the four-by-ten mission
filename pointer table at `0x003a8afc`. Back restores the stock main-menu shell
state (`1`) at `0x003a8980` before ending the modal dialog, matching the native
Single Player dialog's exit path. Start writes only the two native
selection globals, arms the one-shot acceptance bridge, and enters
`SetupMission`; Armada still chooses the mission filename, switches game
state, and closes the modal dialog. For an INI-defined custom BZN, the module
temporarily replaces exactly one checked cell in that table with the selected
filename, calls `SetupMission`, and restores the original pointer before
returning to its dialog procedure. It never expands or indexes past the native
four-by-ten allocation. Details and metadata precedence are in
[`modules/A2FOMissionSelector/README.md`](../modules/A2FOMissionSelector/README.md).

`A2FOTurrets.dll` owns the global indexed linked-turret runtime:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x000cc480` | inline/JMP chain | 5/6 | stock `55 8b ec 6a ff`, or Fleet Ops' live `68 <handler> c3` resolving exactly to RVA `0x0010bd80` whose handler begins `55 8b ec 83 c4 e8 53` | `game_object_class_constructor_hook`; chain Fleet Operations' class enhancement, then retain sparse `turret0..64`/`turretHardpoint0..64` parent pairs and semantic-turret rotation policy from the completed ParameterDB |
| Armada | `0x00271340` | inline | 6 | `55 8b ec 8b 45 08` | `weapon_set_target_hook`; retain each semantic turret object's native weapon target handle for visual yaw/pitch tracking |

The module directly owns only these two sites. Simulation, post-load, cleanup,
and trigger policy are registered with the core's shared dispatchers. Fleet
Operations' GameObjectClass constructor detour is accepted only when it
resolves to the checked handler at RVA `0x0010bd80`; the untouched stock
prologue remains supported for isolated fixtures.
It transactionally registers `turret -> sensor` and seven missing-only
defaults which hide the native child SensorArray from the interface, make it
an `avoidMe = 0` object, and remove its footprint/avoidance bookkeeping. The
child remains a complete native object and therefore owns ordinary weapons,
hitpoints, targeting, rendering, and save data.

A2FOAlwaysShowShields owns three inline hooks because Starbase overrides do not
enter `Craft::Simulate`. A2FOCraftIdentity's existing CraftClass-construction
hook forwards completed ship and station ODFs; A2FOTurrets' existing
GameObjectClass-construction hook supplies the additional class path.
A2FOHybridBuild's existing Fleet Ops
`Craft::RenderInternal` callback also invokes the object observer before native
rendering, covering Fleet Ops-derived station classes which bypass both stock
simulation entries. The shield module validates and calls these otherwise
untouched Armada routines:

| Image | RVA | Kind | Expected bytes | Purpose |
| --- | ---: | --- | --- | --- |
| Armada | `0x0002b910` | inline | `55 8b ec 56 8b 75 0c 8b 46 40` | central GameObject mission-publication dispatcher called by both normal object creation and `AiMission::AddObject`; after publication, apply the shield observer without depending on Fleet Operations subclass construction or virtual routing |
| Armada | `0x00072b60` | inline | `55 8b ec 83 ec 0c a1 10 b6 76 00` | `RenderGameObjects` global-list pass; immediately before Armada builds the frame, enumerate every GameObject so hidden Fleet Operations subclasses cannot bypass shield observation |
| Armada | `0x000bdb10` | inline | `55 8b ec 53 8b 5d 08` | `Starbase::Simulate`; chain native starbase simulation, then run the same shield observer used after ordinary Craft simulation |
| Armada | `0x000743b0` | native call / optional chained entry hook | `55 8b ec 6a ff 68 87 b5 69 00`, or the DirectionalShields `JMP rel32` | `ShieldEffect::CreateShieldHit`; create a separately tracked, infinite-duration type-7 `WEclairlink1` effect using its native colour; the directional hook explicitly passes type 7 through |
| Armada | `0x00074770` | native call / optional chained entry hook | `55 8b ec 83 ec 08 8d 45`, or the DirectionalShields `JMP rel32` | `ShieldEffect::ShieldStop`; remove the module-owned effect when shields fall or the Craft is cleaned up; untracked type-7 IDs pass through the directional hook unchanged |

The completed class ParameterDB is read through the existing checked
GetString entry at Armada RVA `0x00135350`. Runtime state comes from
GameObject+`0x40` (class) and Craft+`0x1c8` (current shields). The global
GameObject-list owner pointer is at Armada RVA `0x00361084`; its list nodes
store the GameObject pointer at `+0x8`. The identity
shield matrix is at Armada RVA `0x00369380`; shield type `7` loads
`WEclairlink1`, the Clairvoyance-link effect intentionally reused as this
feature's persistent visual. The final create flag is `0`, retaining the
effect's native colour. Module-owned effect IDs deliberately never enter
Craft+`0x208`, which remains reserved for native impact/collapse effects.
Missing or zero `alwaysShowShields` values never enter module policy.

`A1Compat.dll` also owns these A1-scoped compatibility hooks:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x0009dd40` | inline | 7 | `55 8b ec 56 8b 75 08` | `nebula_set_textures_recursive_hook`; diagnose and skip an `ST3D_SpriteNode` whose type-specific data at node offset `0xc0` is null before native `NebulaClass::s_SetTexturesRecursive` dereferences offset `0x2c` from it at RVA `0x0009dd62` |
| Armada | `0x000ab710` | inline | 5 | `55 8b ec 6a ff` | `starbase_class_build_class_hook`; call native `StarbaseClass::BuildClass`, then parse A1 `maximumUpgrades` and `officerGain` as strings through the validated `ParameterDB::GetString` entry at RVA `0x00135350`; RVA `0x00135200` is `GetProjectId` and must not be detoured as an integer getter |
| Armada | `0x000bbd90` | vtable target | 5 | `53 56 57 8b f1` | `starbase_finish_build_hook`; reproduce A1's `Starbase::FinishBuild` OfficerUpgradeClass branch before A2's derived object/output-queue post-processing, while chaining the native gateway for every ordinary build |
| Armada | `0x000bda00` | inline | 6 | `55 8b ec 8b 45 08` | `a2fo_a1_starbase_initialize_geometry_hook`; before Fleet Operations initializes Starbase geometry, set node flag bit 0 on exact numbered `oqN` branches according to compatibility-owned completion state |
| Armada | `0x000bda30` | inline | 5 | `55 8b ec 51 56` | `starbase_clear_team_hook`; remove A1 base/quarter officer capacity and reset completed quarters before native `Starbase::ClearTeam` |
| Armada | `0x000bda70` | inline | 6 | `55 8b ec 8b 45 08` | `starbase_set_team_hook`; after native `Starbase::SetTeam`, credit the new owner with the A1 base and retained-quarter officer capacity |
| Armada | `0x000bdaa0` | inline | 5 | `55 8b ec 56 57` | `starbase_load_hook`; restore versioned compatibility-owned completed-quarter count and cumulative gain before/around native `Starbase::Load` |
| Armada | `0x000bdae0` | inline | 5 | `55 8b ec 56 57` | `starbase_save_hook`; serialize versioned completed-quarter count and cumulative gain before native `Starbase::Save` |
| Armada | `0x0010ad23` | inline diagnostic | 5 | `6a 01 8d 4d d8` | `a2fo_a1_standard_text_sprite_hook`; after `StandardText::InitializeConfiguration` looks up a generated GUI sprite, report null results with the live configuration/name strings and item index before the native null dereference at RVA `0x0010ad39` |
| Armada | `0x0013c2da` | inline diagnostic | 5 | `83 c4 18 33 c0` | `a2fo_a1_rtime_load_name_hook`; after `RtimeClass::Load(FileReader&)` reads its 40-byte serialized type name and performs its absolute global load, report names absent from the runtime-class registry before native code dereferences a null factory at RVA `0x0013c334` |
| Fleet Ops | `0x001dbdcb` | inline diagnostic | 7 | `0f b6 80 34 06 00 00` | `a2fo_a1_craft_level_up_race_hook`; in `CraftEnhancement.Craft_mLevelUp`, report the craft ODF, handle/team, Side, Race, class, enhancement record, force flag, and native caller when the `Craft -> Side(+0xf0) -> Race(+0x244)` chain yields null before the native `Race+0x634` `canGainXP` read |

The hook is installed only by the parent-scoped module after an active
`a1compat.ini` marker is found. Its gateway preserves native recursion, so
valid nodes follow the original function unchanged. Recursive calls re-enter
the guard; returning for one invalid node lets the native parent loop advance
to that node's next sibling.

The officer-quarter geometry hook mirrors A1's placement of the visibility update:
immediately before instance geometry is initialized. In the supported Fleet
Operations executable, a Starbase's `GameObjectClass*` is at object offset
`0x40`; the class's geometry-database pointer is at `+0x1d8`; its hierarchy
root is at database offset `+0x3c`; and `ST3D_Node` uses first-child `+0x1c`,
next-sibling `+0x20`, and flags `+0xbc`. Only flag bit `0x1` is changed.

The source A1 implementation provides the semantic reference: its
`StarbaseClass` stores `maximumUpgrades` at `+0x568`, `officerGain` at `+0x56c`,
and its `oqN` pointer array at `+0x570`; `Starbase::InitializeGeometry` at
preferred VA `0x0045f3d0` exposes nodes below the completed count and hides the
rest. A1 stores that count at Starbase offset `+0x7e4`. `A1Compat` keeps the FO
equivalent in a pointer-keyed sidecar because Armada 2's smaller Starbase has no
compatible field. Officer upgrade classes are identified by the retained
`OfficerUpgradeClass` vtable at Armada RVA `0x002b4144`; their `officerGain` is
at class offset `+0x1e0`. A1Compat records target race identity after
`OfficerUpgradeClass::BuildClass` at Armada RVA `0x000ce910`.

The source A1 `Starbase::FinishBuild` at preferred VA `0x0045f570` detects an
`OfficerUpgradeClass`, applies its in-place Team/count changes, and invokes the
Producer cleanup branch without entering ordinary Starbase built-object
post-processing. A2 removed that override. Its replacement at Armada RVA
`0x000bbd90` calls `Producer::FinishBuild`, then assumes the return is a real
`GameObject`. Claiming FeaturePack's later Producer `FINISHING` event still
returns null to that outer A2 routine; the null is eventually admitted to
`OutputQueueManager`, whose method at RVA `0x00136570` then faults at
`0x0013657b` while reading `object+0x44`.

A1Compat therefore owns a checked A1-scoped hook at `Starbase::FinishBuild`
RVA `0x000bbd90`. Matching officer upgrades mirror Fleet Ops' active-technology
count decrement, use Armada's native queue-pop helper at RVA `0x000b79b0`,
clear construction state/effects, apply officer and `oqN` state, and return
directly to `Producer::UpdateBuild`, which ignores the virtual return. Every
ordinary Starbase completion follows the native gateway unchanged. FeaturePack
continues to own and dispatch the general Fleet Ops Producer events, but its
later `FINISHING`/`FINISHED` pair is no longer used to implement A1 officers.

Armada's `Producer::mStartConstructionEffect` at RVA `0x000b8140` passes the
class at `Producer+0x254` to the Craft construction-effect creator at RVA
`0x00069960`. `OfficerUpgradeClass` is not layout-compatible with CraftClass;
letting that cosmetic instance render produced the pre-completion
`CraftInstance::RenderInternal` fault at RVA `0x000cb151` while reading the
foreign class's nonexistent `+0x408` field. Native API revision 9 adds the
claimable `STARTING_EFFECT` event at the existing HybridBuild-owned checked
hook. A1Compat claims only matching A1 officer upgrades, leaving the timed
Producer job intact with no cosmetic Craft instance.

A1's Team fields at `+0x18/+0x1c` represented available and maximum officers.
FO retains A2's enlisted/max model at Team `+0x160/+0x164`; this stage adjusts
only `+0x164`. Writing the A1 gain into `+0x160` would incorrectly enlist units
which do not exist. Restoring A1's available/max UI semantics remains a
separate resource-compatibility task.

The RtimeClass site is diagnostic only: it preserves registers and flags,
reports the name plus `FileReader` cursor/flags and surrounding stream bytes
through A1Compat, executes the displaced instruction through its gateway, and
leaves the native failure unchanged. The stable site follows an instruction
with an absolute operand, which is deliberately excluded from the checked
signature. Registry lookup uses the safe
`RtimeClass::Find(char const*)` helper at Armada RVA `0x0013c1a0`.

The StandardText site is likewise diagnostic only. It preserves the null
lookup result and all native register/flag state, logs the generated sprite
name while its temporary TString is live, executes the displaced cleanup
setup through its gateway, and leaves native failure behaviour unchanged.

The Craft level-up site is also non-bypassing. At entry EAX contains the Race
pointer, the Craft is retained in the native frame at `[EBP-4]`, and EBX holds
the force-level-up flag. The bridge preserves all registers and flags, logs
only null Race results, then executes the displaced `movzx eax,[eax+0x634]`
through the gateway so FleetOps' original success or failure remains visible.
ODF identification uses the preflighted Armada class-name getter at RVA
`0x000ce370`.

### FeaturePack queue hooks

Source:
[`../modules/A2FOFeaturePack/queue_enhancement.cpp`](../modules/A2FOFeaturePack/queue_enhancement.cpp)

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x0d4280` | inline | 6 | `55 8b ec 56 8b f1` | `game_object_queue_class_command_hook`; emit synchronized fill/repeat markers |
| Armada | `0x0d45f0` | inline | 6 | `55 8b ec 8b 45 0c` | `game_object_dequeue_class_command_hook`; consume typed-class markers on every peer |
| Armada | `0x0b77d0` | inline | 5 | `55 8b ec 6a ff` | `producer_dtor_hook`; dispatch API revision-7 destruction, then discard continuous state |
| Armada | `0x0b7840` | inline | 6 | `55 8b ec 56 8b f1` | `producer_simulate_hook`; refill/retry continuous production |
| Armada | `0x0b88d0` | inline | 6 | `55 8b ec 83 ec 10` | `producer_load_hook`; restore the A2FO repeat marker |
| Armada | `0x0b8aa0` | inline | 6 | `55 8b ec 83 ec 08` | `producer_save_hook`; persist the A2FO repeat marker |
| Fleet Ops | `0x12255c` | inline | 5 | `55 8b ec 51 53` | `producer_finish_hook`; capture the queue-head class, chain native completion, dispatch API revision-7 completion, then run continuous refill |
| Fleet Ops | `0x122514` | inline | 7 | `55 8b ec 51 89 4d fc` | `producer_cancel_hook`; stop repeat on cancellation |
| Fleet Ops | `0x122a10` | inline | 6 | `55 8b ec 83 c4 cc` | `producer_command_push_hook`; dispatch API revision-7 admission before observing/composing checked build-command insertion |
| Fleet Ops | `0x122c8c` | inline | 6 | `55 8b ec 83 c4 c8` | `producer_act_delete_hook`; cancel repeat after manual item deletion |
| Fleet Ops | `0x122ef4` | inline | 6 | `55 8b ec 83 c4 cc` | `producer_clear_hook`; cancel repeat after queue clear |

### FeaturePack configurable upgrade-pod hooks

Source:
[`../modules/A2FOFeaturePack/upgrade_pods.cpp`](../modules/A2FOFeaturePack/upgrade_pods.cpp)

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x0b95f0` | inline | 8 | `55 8b ec 8b 45 08 53 56` | `pod_attach_hook`; track extended tier/system/multiplier and advance station state |
| Armada | `0x0b95a0` | inline | 7 | `56 8b f1 57 8b 7e 40` | `pod_detach_hook`; remove sidecar state and recompute the effective multiplier |
| Armada | `0x0b99b0` | inline | 8 | `55 8b ec 56 57 8b 7d 08` | `station_constructor_hook`; create per-station tier-list state |
| Armada | `0x0b9b50` | inline | 5 | `55 8b ec 6a ff` | `station_destructor_hook`; release per-station state |
| Fleet Ops | `0x10c5e4` | inline | 7 | `55 8b ec 83 c4 f8 53` | `pod_class_hook`; retain declared upgrade tiers above Fleet Ops' native limit |
| Fleet Ops | `0x10c618` | inline | 5 | `55 8b ec 51 53` | `pod_class_dtor_hook`; clear class sidecars |
| Fleet Ops | `0x1e3e00` | inline | 7 | `55 8b ec 83 c4 f8 53` | `station_class_hook`; parse tier-indexed station build lists |
| Fleet Ops | `0x1e3ea0` | inline | 9 | `55 8b ec 51 53 56 8b 75 08` | `station_can_build_hook`; admit sidecar-only higher-tier pods |
| Fleet Ops | `0x1fcffc` | inline | 5 | `55 8b ec 51 53` | `same_type_hook`; compare extended pod identity safely |

### FeaturePack Bink patches

Source:
[`../modules/A2FOFeaturePack/bink_video.cpp`](../modules/A2FOFeaturePack/bink_video.cpp)

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Fleet Ops | `0x1aa6ab` | CALL | 5 | `e8 c0 fd ff ff` | `bink_render_hook`; prepare a full D3D9 target viewport before the native renderer |
| Armada | `0x0639ba` | CALL | 6 | `ff 15 98 7a 7b 00` | `armada_bink_set_dibits_scaled`; replace the unscaled GDI `SetDIBitsToDevice` call |
| Armada | `0x0e5da5` | CALL | 5 | `e8 a6 e5 f7 ff` | `armada_bink_texture_render_scaled`; first menu/campaign texture-render call |
| Armada | `0x0e6153` | CALL | 5 | `e8 f8 e1 f7 ff` | same wrapper for the second texture-render call |

### Legacy texture-folder bridge

Source: [`../modules/A2FORGBTextures/module.cpp`](../modules/A2FORGBTextures/module.cpp)

These sites are installed only when an extension root contains at least one of `Textures\RGB`, `Textures\Index8`, or `Textures\Compressed`.

| Image | RVA | Kind | Bytes | Expected value | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x3b7f8c` | PTR | 4 | slot equals the resolved `msvcrt!fopen` export | `legacy_texture_fopen`; resolve explicit RGB/Index8/Compressed requests plus the validated/prepared root-level TGA fallback while preserving real root/mod assets; DDS requests are never redirected to TGA bytes |
| Armada | `0x2400d0` | inline | 6 | `55 8b ec 8b 45 08` | `file_exists_hook`; make Armada's completed root-TGA request resolve to the selected physical legacy texture without claiming a missing DDS |
| Armada | `0x240150` | inline | 6 | `55 8b ec 8b 45 08` | `open_read_hook`; open the same selected TGA through Armada's original binary stream and loader |
| Armada | `0x242780` | inline | 5 | `55 8b ec 56 57` | `lock_surface_hook`; identify a failed mapped legacy texture and report the unavailable surface before the null-source guards run |
| Armada | `0x0e7c80` | inline | 6 | `55 8b ec 83 ec 40` | `blend_pixels_xrgb888_hook`; pass valid blends unchanged and skip a blend only when its source-pixel pointer is null |
| Armada | `0x0e96c0` | inline | 6 | `55 8b ec 83 ec 28` | `update_scan_grid_sprite_hook`; preflight both locked scan-grid textures and skip the update if either remains unavailable |

The stream route preserves real DDS and root-TGA priority; it redirects only a
completed TGA request. The fopen wrapper passes unrelated paths and writes to
the retained function unchanged. Stream and guard sites are separately
signature-gated and installed in partial-safe order. Shutdown restores the
pointer only while the slot still contains this module's handler; inline hooks
are process-lifetime changes.

### Nebula DX8 renderer

Source: [`../core/nebula_renderer.cpp`](../core/nebula_renderer.cpp)

This is the DX8 portion of armadaNebulaPatch, ported from its custom loader,
hook toolkit, and MinHook detours into checked core primitives. Armada creates
its shared DOT3 shader before deferred modules load, so the core owns these
sites during process attach. Every replacement remains disabled/pass-through
until the first DOT3 compilation finds the controller DLL and all assets.

| Image | RVA | Kind | Bytes | Expected bytes/value | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x32b580` | DATA | 32 | padded `shaders\\dot3_directional.nvv` | replace the native DOT3 vertex-shader source with `shaders\\dx8\\vertex\\vs.nvv` |
| Armada | `0x226e50` | inline | 10 | `55 8b ec 6a ff 68 cb ba 6a 00` | `compile_dot3_mesh_hook`; preserve native DOT3 mesh/vertex compilation and assemble the paired pixel shader |
| Armada | `0x2279af` | inline | 6 | `ff 92 1c 01 00 00` | Reserved experimental `a2fo_nebula_dot3_draw_hook` site. It is not installed while `kNativeFramebufferBloomEnabled` is false because replaying the live DOT3 draw into a private mask is unstable through the supported wrapper chain. |
| Armada | `0x223ce4` | inline | 7 | `8b 10 51 50 ff 52 38` | Reserved experimental `a2fo_nebula_device_reset_hook` site. It is not installed while native framebuffer bloom is disabled, so no private default-pool bloom targets exist to release. |
| Armada | `0x223ee9` | inline | 9 | `56 8b 06 ff 90 8c 00 00 00` | Reserved experimental `a2fo_nebula_frame_bloom_hook` site. The pre-`EndScene` compositor is not installed while `kNativeFramebufferBloomEnabled` is false. |
| Armada | `0x23e4ea` | inline | 8 | `8b 16 8b 0d 08 d5 7a 00` | `a2fo_nebula_standard_pre_hook`; after `ST3D_Standard_MeshVB::Render` configures its native material, save texture stage 1 and add the active craft's emissive composite without replacing the fixed-function lighting path |
| Armada | `0x23e5aa` | inline | 5 | `5f 5e 85 c0 5b` | `a2fo_nebula_standard_post_hook`; immediately after the standard indexed draw, restore the previous stage-1 texture and all modified combiner/sampler states, then replay the native epilogue. Optional mask resubmission remains behind the disabled native-framebuffer flag. |
| Armada | `0x232585` | inline | 7 | `8b 03 8b cb ff 50 18` | `a2fo_nebula_nonvb_pre_hook`; after the current texture-material pass is configured, activate the scoped emissive stage immediately before the selected DX8 workspace's virtual `Submit` issues its Direct3D draw |
| Armada | `0x23258c` | inline | 6 | `8b 45 fc 46 3b f0` | `a2fo_nebula_nonvb_post_hook`; restore the complete preceding texture-stage state and continue the material-pass loop. Optional CPU-array mask replay remains behind the disabled native-framebuffer flag. |
| Armada | `0x248bfb` | inline | 6 | `ff 92 1c 01 00 00` | Reserved experimental `a2fo_nebula_workspace_dx8_draw_hook` site. The rolling GPU-buffer mask-capture hook is not installed while `kNativeFramebufferBloomEnabled` is false. |
| Fleet Ops | `0x210bb4` | PTR | 4 | slot equals Armada base + `0x22c270` | route Fleet Ops' DOT3 `ArmadaFunctions.ST3D_GraphicsEngine_GetShaderHandle` call to `a2fo_nebula_set_pixel_shader_hook` at first DOT3 compilation, after early Fleet Ops initialisation; retain native custom-vertex-shader lookup/creation, resolve its selected live DX8 device, create the paired pixel shader, upload transform/camera constants, and select it |
| Fleet Ops | `0x1e67d1` | inline | 13 | `8b 40 0c f7 80 2c 01 00 00 04 00 00 00` | `a2fo_nebula_alpha_hook`; disable the pixel shader at the fixed-pipeline transition, replay both displaced instructions, and resume at `0x1e67de` |

`A2FOCraftIdentity` sends each completed CraftClass/ParameterDB pair to the
optional renderer controller, which copies either the legacy six-path wildcard
or up to 64 indexed `textureX`/six-path material sets into the core.
`A2FOHybridBuild` wraps the native Craft render call it already owns
with the core's begin/end exports. The renderer can therefore select a class
policy at DOT3, MeshVB, and legacy non-VB draw time without another class
constructor or Craft render detour. The fixed-function sites above are
required because classic SODs may bypass both Armada's DOT3 shader path and
`ST3D_Standard_MeshVB::Render`. Despite that renderer route's `NonVB` name,
`fbattle.sod` was observed using `ST3D_WorkspaceDirectX8`: its `Submit` derives
the vertex count from workspace offsets `0x04/0x30`, triangle count from
`0x40/0x2c`, reads stride/FVF at `0x9c/0x98`, and retains start index at
`0xbc`. The mask replay uses the native call's live `EBX` vertex count, `ECX`
start index, and `EAX` triangle count; an earlier diagnostic cursor calculation
divided the six-byte triangle indices by twelve and therefore rendered exactly
half the batch. Its mask replay now occurs at the native indexed-call instruction
so a later rolling workspace submission cannot replace that batch. When the
narrow material hook is absent, the exact hook selects the enclosing registered
Craft directly; this includes the main hull workspace instead of capturing only
the 20-triangle Team-colour group. The alternate
`ST3D_WorkspaceDirectX8NonVB` layout retains the existing checked
`DrawIndexedPrimitiveUP` post path. Both isolated mask paths retain a neutral
stage 0 and sample the emissive texture on stage 1, matching the visible
material pass's UV route rather than classic stage-0 material transforms.

The final site deliberately differs from upstream's five-byte middle-function
JMP. Upstream executed the function epilogue immediately, suppressing the
remaining alpha draws and producing its documented black editor-crystal text.
The A2FO gateway preserves the remainder of Fleet Operations' renderer.
`/d3d9` and `-d3d9` disable this initial module before any mutation because the
upstream DX9 branch is unfinished and ABI-dependent.

### HybridBuild runtime hooks

Source:
[`../modules/A2FOHybridBuild/hybrid_production_runtime.cpp`](../modules/A2FOHybridBuild/hybrid_production_runtime.cpp)

| Image | RVA | Kind | Bytes | Expected bytes/target | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x0b80f0` | inline | 5 | `55 8b ec 56 57` | `producer_get_action_hook`; route queued hybrid jobs through their execution sidecars |
| Armada | `0x0314d4` | inline | 6 | `8b 8e a4 02 00 00` | `a2fo_hybrid_build_position_load_dispatch`; select the placement interface belonging to the queued command |
| Armada | `0x0b8140` | inline | 7 | `55 8b ec 83 ec 30 56` | `producer_start_effect_hook`; start the proper yard/research/construction/evolve effect |
| Armada | `0x0b8470` | inline | 9 | `56 8b f1 8b 86 68 02 00 00` | `producer_cancel_effect_hook`; cancel the active hybrid effect |
| Armada | `0x0b8dd0` | inline | 9 | `55 8b ec 8b 81 68 02 00 00` | `producer_update_effect_hook`; update the active hybrid effect |
| Armada | `0x0b8f30` | inline | 9 | `56 8b f1 8b 86 68 02 00 00` | `producer_stop_effect_hook`; stop the active hybrid effect |
| Fleet Ops | `0x1dc1bc` | inline | 7 | `55 8b ec 83 c4 f4 53` | `craft_render_internal_hook`; render protected hybrid construction/evolution sidecars |
| Armada | `0x0ba0e0` | inline | 7 | `55 8b ec 83 ec 0c 56` | `research_start_hook`; start the selected method while retaining the shared FIFO |
| Armada | `0x0ba1b0` | inline | 6 | `55 8b ec 51 56 57` | `research_cancel_hook`; method-aware cancellation |
| Armada | `0x0ba280` | inline | 9 | `56 8b f1 8b 86 54 02 00 00` | `research_can_build_hook`; isolated-list admission and native uniqueness composition |
| Armada | `0x0ba4a0` | inline | 6 | `55 8b ec 53 56 57` | `research_item_conflict_hook`; method-aware queued-item conflict checks |
| Armada | `0x0babd0` | inline | 6 | `55 8b ec 51 56 57` | `research_matrix_hook`; return the correct construction/evolution matrix |
| Fleet Ops | `0x1e9b3c` | inline | 7 | `55 8b ec 51 89 4d fc` | `research_finish_hook`; complete the hybrid method and advance the shared queue |
| Armada | `0x0e69e0` | inline | 9 | `8b c1 56 8b 90 88 00 00 00` | `control_button_press_hook`; preserve method identity through root/submenu button dispatch |
| Armada | `0x0e7950` | inline | 7 | `55 8b ec 83 ec 48 53` | `mode_info_build_button_name_hook`; select the dedicated Construction sprite name |
| Armada | `0x0ee530` | inline | 5 | `55 8b ec 6a ff` | `race_icon_render_hook`; suppress the insignia behind a full ten-slot queue |
| Armada | `0x0b0210` | inline | 8 | `8b 81 54 02 00 00 85 c0` | `producer_is_busy_hook`; classify admission/cleanup callers without breaking queued placement jobs |
| Fleet Ops | `0x122b04` | inline | 6 | `55 8b ec 83 c4 e0` | `fo_producer_pop_checked_hook`; preserve the right sidecar while Fleet Ops pops/replaces queue entries |
| Fleet Ops | `0x1e6c70` | inline | 7 | `55 8b ec 83 c4 e4 53` | `popup_update_buttons_hook`; maintain four isolated production palettes and menu retention |
| Armada | `0x0afbe8` | CALL | 5 | `e8 73 5c fc ff` | `construction_rig_build_hardpoints_hook`; scoped class-safe hardpoint bypass for hybrid placement |
| Fleet Ops | `0x1e6d97` | CALL | 5 | `e8 50 b6 ff ff` | `build_button_bind_hook`; bind the hybrid Build button to its own root slot |
| Fleet Ops | `0x1e6e3f` | CALL | 5 | `e8 a8 b5 ff ff` | `evolve_button_bind_hook`; bind Evolve without overwriting another production method |
| Fleet Ops | `0x1e6f41` | CALL | 5 | `e8 a6 b4 ff ff` | `ai_button_bind_hook`; move AI when the hybrid Construction slot occupies its native entry |
| Fleet Ops | `0x1e8572` | CALL | 5 | `e8 fd f9 ff ff` | `selection_display_draw_producer_wireframe_hook`; draw the active research-pod hover wireframe |
| Armada | `0x0f2c49` | CALL | 5 | `e8 1a bc 4f 5a` | `a2fo_ship_display_single_object_display_dispatch`; queue-wireframe display post-pass after Fleet Ops' active target |
| Armada | `0x0f29e4` | CALL | 5 | `e8 2b c1 4f 5a` | `a2fo_ship_display_single_object_simulate_dispatch`; matching queue-wireframe simulation post-pass |
| Fleet Ops | `0x240c00` | PTR | 4 | slot equals Fleet Ops base + `0x10ad40` | `object_control_button_press_hook`; intercept Fleet Ops' virtual ObjectControlButton press handler, retaining the original target |

HybridBuild preflights every required signature and the vtable-slot target
before its first write. The direct pointer replacement is possible because the
slot is in writable Fleet Ops image data; all code changes use the core's
checked patch API.

#### Dynamic HybridBuild `GetAction` vtable hook

Hybrid Construction also needs one hook whose address cannot be expressed as a
module RVA. When a registered hybrid station is selected,
`ensure_hybrid_get_action_route` reads the station's current vtable and replaces
the function pointer at vtable offset `+0x90` with
`hybrid_get_action_vtable_hook`. The original target is retained in
`g_hybrid_get_action_originals`, keyed by the exact vtable pointer, and is used
as the fallback route.

This is a four-byte pointer replacement performed once per distinct hybrid
station vtable. The code checks that the object, vtable, and slot are readable,
changes the slot protection with `VirtualProtect`, writes the pointer, flushes
the instruction cache, and restores the old protection. There is deliberately
no fixed RVA or static expected-byte sequence for this dynamic site.

## Engine routines, globals, and continuation sites

The addresses below are dependencies, not additional patches. A routine may
still have a checked prologue because the feature refuses to run when a helper
does not match the supported build.

### Core dependencies

| Image | RVA | Role |
| --- | ---: | --- |
| Armada | `0x00001370` | get current `AiMission`; preflighted for replacement publication |
| Armada | `0x0b0c1b` | continuation after the high-RVA cocoon update splice |
| Armada | `0x0cd370` | find `GameObjectClass` by ODF name; preflighted |
| Armada | `0x0cd390` | construct replacement object; preflighted |
| Armada | `0x0ce370` | obtain a class's source ODF name; preflighted |
| Armada | `0x0cfd50` | obtain source-object transform; preflighted |
| Armada | `0x22cf10` | load SOD geometry |
| Armada | `0x3ad508` | SOD database pointer |
| Armada | `0x33fccc`, `0x33fd3c` | native default/alternative cocoon geometry slots |
| Fleet Ops | `0x109c14` | original FOFS hash-table lookup called by the core dispatcher |
| Fleet Ops | `0x13e824` | save the Fleet Ops settings singleton |
| Fleet Ops | `0x00570c` | Delphi `System.@LStrAsg` for lifetime-safe directory assignment |

### Recursive ODF dependencies

`A2FOFeaturePack.dll` installs no extra FOFS call-site patch. It registers a
semantic lookup handler with the four core FOFS patches and calls these Fleet
Ops helpers. `A1Compat.dll` likewise installs no patch: it registers `Addon`
through the native API revision 6 ODF-overlay registry, which FeaturePack
consumes through the same FOFS handler and scanner helpers.

| Fleet Ops RVA | Role and validation |
| ---: | --- |
| `0x0056b8`, `0x0058b0` | Delphi long-string clear/from-PChar helpers |
| `0x080824` | `TList.Add` for registered virtual directories |
| `0x0fa83c` | Fleet Ops filename hash; checked bytes `55 8b ec 83 c4 f4 53 56 57` |
| `0x10870c` | virtual-directory class reference |
| `0x108b6c` | virtual-directory constructor |
| `0x108c14` | override recalculation |
| `0x1092d0` | add file to hash table; checked bytes `53 56 51 8b f1 89 14 24` |
| `0x109488` | scan disk items; checked bytes `55 8b ec 81 c4 a4 fe ff ff` |
| `0x109650` | scan FPQ items; checked bytes `55 8b ec 83 c4 cc` |

### STA1 race-interface diagnostic

| Binary | Address | Role |
| --- | ---: | --- |
| Armada | `0x00513B05` | Reads `communications_player_array[selected_index]` while constructing the communications bar. If a legacy race's interface CFG does not inherit the FO `commBarNumberOfPlayers` and `commBarPlayer_*` contract, the array pointer at object offset `+0x98` remains null and this instruction faults. This is a data-contract address, not a patched hook. |
| Armada | `0x0049DD62` | `NebulaClass::s_SetTexturesRecursive`; reads a Storm3D node's nested render pointer at node offset `+0xC0`, then its member at `+0x2C`. The pointer is null while loading `map_nebula_crystalid.odf`, identified directly in the exception stack. Its SOD contains `ncyrstA`–`ncyrstD`, whose definitions live in FO's `fleetops.spr`. Installing valid classic nebula SODs, restoring the two FO include directives, and disabling nested `sod/Software` variants did not individually alter the fault. The current test places the exact parent `fleetops.spr` and `fleetops_comp.spr` beside the child registry to test whether sprite includes fail to traverse parent roots. This is a data-contract diagnostic address, not a patched hook. |

### Legacy texture-folder dependencies

Source: [`../modules/A2FORGBTextures/module.cpp`](../modules/A2FORGBTextures/module.cpp)

Armada's original texture fallback constructs a legacy TGA pathname and opens
it through ArmadaL.exe's own MSVCRT import. It never asks FOFS. Fleet
Operations' `newSearchDirectories_init` deliberately replaces the original
`Textures\RGB\` pathname buffer with `Textures\` during its own startup. The
module accepts those two exact literal states and preserves root-level TGA
priority. The remaining Armada path is a true-colour loader, so every candidate
is validated regardless of folder. Already-compatible uncompressed 24/32-bit
files answer directly, while colour-mapped, 16-bit, grayscale, and RLE TGA
types 9, 10, and 11 are expanded to bounded temporary 24/32-bit TGAs first.
This includes RLE files in root `Textures`, RGB, Index8, and Compressed.
Explicit `Textures\RGB\...` true-colour requests receive the same preparation;
explicit Index8 and Compressed requests retain their native namespaces.
Malformed or unsupported candidates fail closed. Extension-root precedence is
resolved before format: an active child Index8/Compressed candidate overrides
a parent RGB candidate. Within one root the tie-break order is root, RGB,
Index8, then Compressed.

The exact PE identity, accepted pathname state, and unclaimed `msvcrt!fopen`
IAT pointer are hard bridge gates. The stream route separately requires exact
`FileExists` and `OpenRead` entries. It acts only after Fleet Ops' DDS lookup
has failed and Armada has constructed its normal root-level TGA path.

| Image | RVA | Role and validation |
| --- | ---: | --- |
| Fleet Ops | `0x1d0ec4` | `newSearchDirectories_init`; passes Armada VA `0x0072d178` and the padded `Textures\` replacement to `SelfMemWrite` |
| Fleet Ops | `0x0f78d0` | `memoryTools.SelfMemWrite`, called by the pathname rewrite above |
| Armada | `0x2400d0` | `ST3D_FileStream_FileExists`; stream-route hook entry, checked bytes `55 8b ec 8b 45 08` |
| Armada | `0x240150` | `ST3D_BinaryFileStream::OpenRead`; matching physical-file open route, checked bytes `55 8b ec 8b 45 08` |
| Armada | `0x242ee0` | original read-parameters routine; checked bytes `55 8b ec 6a ff 68` |
| Armada | `0x242fa5` | read-parameters call to the file-existence helper; checked bytes `e8 26 d1 ff ff` |
| Armada | `0x2434b0` | original pixel-data loader retained by Fleet Ops; checked bytes `55 8b ec 6a ff 68` |
| Armada | `0x243553` | pixel loader call to the file-existence helper; checked bytes `e8 78 cb ff ff` |
| Armada | `0x242780` | `ST3D_Texture::LockSurface`; checked bytes `55 8b ec 56 57`; texture name pointer is at `ST3D_Texture+0x08`, dimensions at `+0x1c/+0x20` |
| Armada | `0x0e7c80` | `BlendPixels_XRGB888`; checked bytes `55 8b ec 83 ec 40`; the eighth stack argument is the source-pixel pointer |
| Armada | `0x0e96c0` | `RadarComponent::UpdateScanGridSprite`; checked bytes `55 8b ec 83 ec 28`; guarded after a null lock exposed its unchecked pixel copy |
| Armada | `0x23b150` | `ST3D_Sprite::GetTexture`; dependency bytes `8b 41 58 c3` |
| Armada | `0x2437b0` | `ST3D_Texture::UnlockSurface`; dependency bytes `55 8b ec 56 57` |
| Armada | `0x3ad508` | renderer singleton pointer; current texture-device index is at renderer `+0xc0` |
| Armada | `0x32d178` | original NUL-terminated `Textures\RGB\` buffer; accepted after Fleet Ops rewrites it to the exact `Textures\` form |
| Armada | `0x3b7f8c` | imported `fopen` slot; must equal the resolved `msvcrt!fopen` export before the pointer write |

### Ownership-aware texture and node variants

Source: [`../modules/A2FOTextureVariants/module.cpp`](../modules/A2FOTextureVariants/module.cpp)

The Borg repair replaces only the TGA-only preflight CALL. It invokes the
original generator first and accepts a matching DDS only when that native test
fails. The faction route captures `factionTextureSuffix` and the existing Race
ODF `name` while Fleet Operations' Race `ParameterDB` is still alive, follows
each `CraftInstance`'s live owning Race, and replaces the existing render-time
Borg swap CALL. Its wrapper always invokes native
`CraftClass::SetBorgMeshTextures` first, retaining Fleet Operations' Jan_B
diffuse/bump patch inside that routine.

When `A2FOCraftIdentity.dll` is active, the subsystem-damage route receives
completed CraftClass/ParameterDB pairs through its cooperative observer and
adds no constructor patch. If CraftIdentity is not selected for a mod,
TextureVariants instead chains only the validated stock/Fleet Ops CraftClass
constructor boundary at Armada RVA `0x0bf090` (the Fleet Ops handler is RVA
`0x10d6e4`). The Fleet Ops update callback observes live CraftSystem
transitions and the per-craft render wrapper applies the selected hidden node
and every descendant immediately before that individual shared SOD renders,
then restores the shared hierarchy after the draw returns.

| Image | RVA | Role and validation |
| --- | ---: | --- |
| Armada | `0x23307e` | Borg alternate call to `GenerateTextureFilename`; patched CALL, checked bytes `e8 3d f3 00 00` |
| Armada | `0x2423c0` | original TGA filename generator; dependency bytes `55 8b ec 51 53 56 57` |
| Armada | `0x2400d0` | `ST3D_FileStream::FileExists`; used for the same `Textures\\*.dds`/`.tga` probes as the Fleet Ops texture enhancement and allowed to be owned by the RGB bridge |
| Armada | `0x0cb2ab` | `CraftInstance::RenderInternal` call to `SetBorgMeshTextures`; patched CALL, checked bytes `e8 c0 48 ff ff` |
| Armada | `0x0cb2b0` | native post-CALL `borg` node selector; retained unchanged and executes after the wrapper returns |
| Armada | `0x0cb318` | final `CraftInstance` render CALL after native damage/`borg` node selection; patched CALL, checked bytes `e8 93 a6 00 00`; scopes the missing-subtree flags and root repair scale to one draw |
| Armada | `0x0d59b0` | original `CraftInstance` render routine reached by the scoped wrapper; dependency bytes `55 8b ec 51 8b 81` |
| Armada | `0x0bfb70` | native/Jan_B `CraftClass::SetBorgMeshTextures`; dependency bytes `55 8b ec 56 57` |
| Armada | `0x231380` | `ST3D_Mesh::GetTexture`; dependency bytes `55 8b ec 8b 45 08 8b` |
| Armada | `0x2313b0` | `ST3D_Mesh::SetTexture`; diffuse-slot writer, dependency bytes `55 8b ec 83 ec 10 53` |
| Armada | `0x242870` | `ST3D_Texture::Find`; variant allocation/cache route, dependency bytes `55 8b ec 64 a1 00` |
| Armada | `0x135200` | `ParameterDB::GetProjectId`; resolves each optional `<subsystem>MeshXexplosion`, dependency bytes `55 8b ec 81 ec 40 01 00 00` |
| Armada | `0x064ce0` | `ExplosionClass::Find`; resolves the configured cPrjID, dependency bytes `55 8b ec 6a ff 68 f8 af 69`; returned class virtual slot `+0x08` builds the correct derived explosion |
| Armada | `0x0cfd50` | `Entity::GetTransform`; converts sampled repair positions back into the owning craft's local coordinates, dependency bytes `8b 41 04 83 c0 44 c3` |
| Armada | `0x0cff90` | `Entity::GetWorldTransform`; supplies the selected SOD node's 48-byte world Matrix34, dependency bytes `55 8b ec 8b 45 0c 8b 49 04` |
| Armada | `0x0cfd70` | `Entity::GetPhysicalDimensions`; supplies the craft radius used to scale repair sparks, dependency bytes `8b 41 04 83 c0 34` |
| Armada | `0x0733c0` | `NodeParticleEffect::AddParticle`; attaches stock `xspark` to the selected missing node, dependency bytes `55 8b ec 83 ec 34` |
| Armada | `0x0734c0` | position overload of `NodeParticleEffect::AddParticle`; emits moving stock `xspark` welding effects at deterministic mesh-surface samples, dependency bytes `55 8b ec 83 ec 34` |
| Fleet Ops | `0x1fb250` | `CraftInstance_Update_Callback`; seven-byte checked inline hook `55 8b ec 51 53 56 57` whose gateway preserves the complete Fleet Ops callback |

The supported object layout reads `GameObject+0xfc` for the live owning Race,
matching Armada's native Borg update. `CraftClass+0x1d8` owns the geometry;
its first Storm3D node is at geometry `+0x3c`. Nodes link siblings/children at
`+0x1c/+0x20`, store their name pointer at `+0x08`, and store render flags at
`+0xbc`. Like the native `borg` route, custom faction nodes clear flag bits 0
and 1 for the matching owner and set both for other loaded races; all other
bits are preserved. Descendants inherit the parent node state. The native
`borg` node is excluded from the custom selector. Mesh Borg alternates are at
`+0x13c`. Cached original `ST3D_Texture` database/name/flags fields are
`+0x04/+0x08/+0x18`.

Subsystem entries additionally enumerate the configured node's complete
child hierarchy through `+0x20`, following `+0x1c` only within each direct
child list so unrelated siblings cannot be captured. While the part is
missing, bits 0 and 1 are set on the root and every descendant. During repair,
the root may render at its eased scale but every descendant remains hidden
until completion. Applying this after native damage/`borg` selection prevents
either native branch from revealing an attachment in empty space. Exact
pre-draw flags and scales are restored after each draw because the model is
shared between every instance of its CraftClass.

Subsystem runtime state uses `Craft+0x1e0`, a pointer to five 0x30-byte
CraftSystem records ordered sensors, engines, weapons, life support, and
shield generator. Each record exposes operational/forced-disabled bytes at
`+0x00/+0x01`, maximum hitpoints at `+0x04`, current hitpoints as a double at
`+0x18`, and temporary disable time at `+0x28`. `GameObject+0x28/+0x40`
provide the stable handle and CraftClass. Timed/forced disables do not select
a part; a genuinely destroyed system does. Increasing hitpoints drive repair
sparks and eased mesh reconstruction; the operational byte completes and
releases the visual state. Initially destroyed systems hide a deterministic
part without a false load-time explosion.
For resolved mesh nodes, the ordinary uniform scale at `ST3D_Node+0x54` is
retained and multiplied by eased subsystem repair progress immediately before
that craft renders. The mesh/MRM virtual type is `1`/`9`, with its calculated
local bounding minimum and maximum at `+0xe4/+0xf0`; deterministic points on
that surface are transformed node-local to world and then back to craft-local
for the position-particle overload. Group nodes keep their original scale and
use the node-bound fallback.

The crash that motivated the retry follows this read-only Armada call chain:

| RVA | Role |
| ---: | --- |
| `0x0e9e40` | `RadarComponent::CacheMap`, caller of the per-object map update |
| `0x0e9b40` | `RadarComponent::UpdateMapObject`; obtains the class `mapSprite`, its texture, and a locked pixel buffer |
| `0x23b150` | `ST3D_Sprite::GetTexture`, called with the class `mapSprite` at `UpdateMapObject+0xb6` |
| `0x0e9c15` | call to `ST3D_Texture::LockSurface`; the failed case returns null with texture dimensions still `-1 x -1` |
| `0x0e9c4a` | virtual XRGB888 blend call; its eighth argument is the lock result |
| `0x0e7d57` | original faulting `mov (%ecx,%edi),%eax` when the source-pixel base is null |
| `0x0e97fd` | second observed fault, `rep movsd` in `UpdateScanGridSprite`, after both legacy scan-grid texture locks remained unavailable |

### Nebula DX8 renderer dependencies

Source: [`../core/nebula_renderer.cpp`](../core/nebula_renderer.cpp)

| Image/library | RVA/export/offset | Role |
| --- | ---: | --- |
| Armada | `0x22c270` | original MSVC-thiscall `ST3D_GraphicsEngine::GetShaderHandle`; always called before the paired pixel stage |
| Armada | `0x3ad5e0` | camera-to-node matrix; its forward vector supplies vertex constant 19 as four bounded floats |
| Storm3D shader renderer | `+0xc0` | bounded current-device index used by native `GetShaderHandle` |
| Storm3D shader renderer | `+0xcc + index * 4` | current DX8 device-wrapper pointer table used by native `GetShaderHandle` |
| Storm3D DX8 device wrapper | `+0x90` | live `IDirect3DDevice8*` used to create and select the paired pixel shader |
| Storm3D shader renderer | `+0x44` | owning `ST3D_Engine*`; its texture-object registry sentinel is at engine `+0x84` |
| `ST3D_Texture` | `+0x08`, `+0x40 + index * 4` | original texture-name pointer and per-device `ST3D_DeviceTexture*` |
| `ST3D_DeviceTextureDirectX8` | `+0x04` | native `IDirect3DTexture8*`, compared with live texture stage 0 to bind an indexed emissive set |
| `D3DX81ab.dll` | `D3DXAssembleShaderFromFileA` | assemble `Data\\Shaders\\dx8\\pixel\\ps.nvv` at runtime |
| `D3DX81ab.dll` | `D3DXCreateTextureFromFileExA` | lazily load and scale loose subsystem emissive images into managed A8R8G8B8 textures |
| `D3DX81ab.dll` | `D3DXMatrixInverse`, `D3DXMatrixMultiply`, `D3DXMatrixTranspose` | construct vertex constants 7 through 18 without adding a D3DX import-library dependency |

Motion-dependent emissive reads use the supported Fleet Operations layout:

| Native object | Offset/RVA | Role |
| --- | ---: | --- |
| `GameObject` | `+0xdc` | live three-float linear velocity used to distinguish stationary and impulse travel |
| `Craft` | `+0x1b0` | native physics-controller pointer |
| Armada Trek-physics vtable | `0x2b28a4` | validates the controller type before any derived state is read |
| Trek-physics controller | `+0x20` | `warpEffectState`: 0 normal/gravity-well, 1 warp-in, 2 steady warp, 3 warp-out |
| `CraftClass` | `+0x3ec` | maximum impulse speed used only as a conservative fallback for non-Trek physics controllers |

The full warp intensity is selected only for native state 2. State 0 with a
non-zero live velocity selects the impulse-travel profile; states 1 and 3 keep
the lower gravity-well warp profile. If the validated Trek controller is not
available, a velocity above 105% of the class impulse limit is treated as warp.
All pointers and ranges are bounded before use.

Craft emissive state reads the class pointer at `Craft + 0x40` and the native
five-record subsystem block pointer at `Craft + 0x1e0`. Each subsystem record
is `0x30` bytes: `+0x00` is operational, `+0x01` is forced/control-disabled,
`+0x04` is maximum hitpoints, `+0x18` is current hitpoints as a double, and
`+0x28` is the timed-disable counter. Sensors are record 0, Engines 1, Weapons
2, Life Support 3, and Shield Generator 4. Both warp and impulse emissive
channels follow Engines because the native Craft layout has no separate
warp/impulse damage records. Operational maps stay on; healthy forced/timed
disabled maps flicker independently in 90 ms phases; destroyed and not-yet-
fully-repaired maps stay off. Every read is bounded and fails open to the
configured light when an unfamiliar layout is encountered.

Subsystem/hull damage decals share that five-record subsystem layout. Hull
damage instead reads `GameObject+0x15c` for current health and `+0x160` for
the live per-instance maximum health. Armada's native health update at RVA
`0x000d2080` writes `+0x15c` and recomputes the cached `+0x158` health ratio;
its ratio getter at RVA `0x00400ef0` independently divides `+0x15c` by
`+0x160`. `Craft+0x1c8` is current shields and must not be used for hull
activation.

Selected ship-name logo decals read Fleet Operations' native name selection
at `Craft+0x218`. FleetOpsHook VA `0x5a90bc84` reconstructs an enhancement-
wrapper pointer from four CraftClass bytes at `+0x185/+0x186/+0x187/+0x19f`;
the wrapper's `+0x04` points to the CraftClass enhancement sidecar and that
sidecar's first pointer identifies its owning class. The `logoFileNames` vector
begin/end are at sidecar `+0x148/+0x14c`, while its parallel loaded
`ST3D_Texture*` table is at sidecar `+0x154`. FleetOpsHook VA `0x5a90ce59`
initialises and reads that vector; VA `0x5a90ce85` derives its bounded row
count, and VA `0x5a90cef2` loads each non-empty texture into the parallel table.
The decal renderer uses the selected row directly for the unsuffixed case.
Suffixed multi-plane variants remain controller-resolved loose files and retain
the same row indexing.

Indexed material selection reads the live stage-0 diffuse texture, finds its
owning `ST3D_Texture` in the bounded type-5 Storm3D object registry, and
normalises the original name to a lowercase basename without a directory or
extension. The same normalisation is applied to each ODF `textureX` value.
This binds emissive sets by actual diffuse identity rather than SOD material
order and leaves an unmatched material untouched. The older unnumbered map set
is retained as an explicit class-wide wildcard.

Loaded emissive sources retain their authored RGB values before subsystem
composites are cached, supplying the sharp self-lit material centre. Complete
generated mip chains and trilinear sampling stabilise thin UV regions. DOT3,
standard MeshVB, GPU-buffer workspace, and CPU-buffer workspace material paths
apply those selective emissive composites directly and restore their scoped
shader or texture-stage state after each draw.

The experimental private render-target mask and pre-`EndScene` blur compositor
remain in the source for further research but are disabled by
`kNativeFramebufferBloomEnabled = false`. Replaying Armada's opaque draw state
proved unstable through dxwrapper/d3d8to9/ReShade, including UI and edit-mode
transitions. ReShade may provide the external bloom halo from the stable native
emissive pixels without the extension rebinding render targets or replaying
geometry.

The D3DX functions are resolved dynamically from Fleet Operations' existing
DLL. The core retains the compiled buffer for its full hook lifetime. Native
`GetShaderHandle` always runs first; if live-device resolution or pixel-shader
creation fails, the hook returns Armada's vertex-shader handle unchanged and
logs the first failure.

### Indexed hull-turret dependencies

Source: [`../modules/A2FOTurrets/module.cpp`](../modules/A2FOTurrets/module.cpp)

These are called or read after the six direct hook signatures above have all
passed. The core has already accepted the supported Armada executable identity
before loading native modules.

| Armada RVA | Role |
| ---: | --- |
| `0x00001370` | `AiMission::GetCurrent`; obtain the mission which publishes a newly constructed child through its vtable slot `+0x18` |
| `0x000caae0` | `Craft::DoExpire`; remove an invalid/orphaned child or every linked child of a destroyed parent |
| `0x000cd370` | `GameObjectClass::Find(char const*)`; resolve the `turretX` ODF basename |
| `0x000cd390` | `GameObjectClass::Construct`; build the turret with its mount matrix, parent team, new handle, and deterministic relationship label |
| `0x000cd940` | `GameObjectClass::GetHierarchyRoot`; obtain the parent class hierarchy used to resolve a named mount node |
| `0x000ce370` | `GameObjectClass::GetOdfName`; diagnostics and pointer-keyed class-policy identity |
| `0x000cfd50` | `Entity::GetTransform`; read the native weapon target's position |
| `0x000cff90` | `Entity::GetWorldTransform`; combine the parent instance and resolved hardpoint into the live mount transform |
| `0x000cfff0` | `Entity::Get(handle)`; reconnect and validate parent, child, and target handles without retaining raw object pointers |
| `0x000d0ea0` | `GameObject::SwapTeam`; follow a parent ownership change when race is unchanged |
| `0x000d0ed0` | `GameObject::SwapRaceAndTeam`; follow a parent race/team change |
| `0x000d4ce0` | `GameObject::SetTransform`; place the complete child SOD at the composed mount/yaw/pitch matrix |
| `0x00135350` | core-detoured `ParameterDB::GetString`; read indexed mount strings and optional semantic-turret values from the completed include chain |
| `0x00238780` | `ST3D_Node::FindNodeRecursive`; resolve `turretHardpointX` below the parent hierarchy root |
| `0x00271050` | `Weapon::GetOwner`; restrict target tracking to weapons owned by semantic turret objects |

### Queue and upgrade-pod dependencies

| Owner | Image | RVA | Role |
| --- | --- | ---: | --- |
| queue | Armada | `0x0cd150` | find class by project ID; preflighted |
| queue | Armada | `0x36133c`, `0x361344` | Control and Alt command-state pointers |
| queue | Fleet Ops | `0x1229b8` | checked native queue push; preflighted |
| upgrade pods | Armada | `0x096340` | team upgrade-manager lookup; preflighted |
| upgrade pods | Armada | `0x0987d0` | bounded native tier-3 multiplier projection; preflighted |
| upgrade pods | Armada | `0x0cd370`, `0x0cd1f0` | class lookup by name/project ID; preflighted before the later destroyed-object hook |
| upgrade pods | Armada | `0x135200` | `ParameterDB::GetProjectId`; preflighted |
| upgrade pods | Armada | `0x135350` | core-detoured `ParameterDB::GetString` public entry |

### Bink dependencies

| Image | RVA | Role |
| --- | ---: | --- |
| Fleet Ops | `0x1aa470` | original TBinkIntro frame renderer; preflighted and called by the wrapper |
| Armada | `0x064350` | original D3D8 texture renderer; preflighted and called by both wrappers |
| Armada | `0x365010` | active viewport-owner global |

### HybridBuild Armada dependencies

| RVA(s) | Role |
| ---: | --- |
| `0x135200`, `0x135350`, `0x0cd1f0` | project-ID/string reads and class lookup; `GetString` intentionally uses the core-detoured public entry |
| `0x031194` → `0x0311a1`, `0x031495` → `0x03149f` | checked cleanup/admission callers and their return addresses used to classify `Producer::IsBusy` |
| `0x0311a5` → `0x0311ac`, `0x031525` → `0x03152c` | checked cleanup/replacement callers and return addresses used to classify checked queue pops |
| `0x0314da` | continuation after the placement-interface load splice |
| `0x0afa30`, `0x0afa3f` | `ConstructionRig::GetAction` and its busy-return path |
| `0x0afbc0`, `0x075860` | `ConstructionRig::StartBuild` and native hardpoint builder |
| `0x0aff00`, `0x0aff90`, `0x0afea0`, `0x0afba0` | ConstructionRig cancel, finish, remove-object, and matrix helpers |
| `0x2b22ec` | ConstructionRig vtable identity |
| `0x0adc40`, `0x0adc70` | BuildPositionInterface constructor/destructor |
| `0x073aa0` | placeholder renderer capability probe; preflighted |
| `0x252710`, `0x2527d0` | active game allocator/deallocator public entries |
| `0x2b41d8` | null object-ID global |
| `0x0b9170`, `0x0b8c10` | Producer construction matrix and update-buttons helpers |
| `0x0b0e10`, `0x0b1150` | Evolver swap-object and construction-matrix helpers |
| `0x0b04f0`, `0x0b0770`, `0x0b08d0`, `0x0b0970`, `0x0b0a10`, `0x0b1170` | Evolver start/remove/cancel/stop/update/render helpers; preflighted |
| `0x1f17b0`, `0x3a86b4` | Fleet Ops-active debriefing destroy-ship entry and debriefing-data global |
| `0x309f60` | root-button action metadata table used to select `b_construct` |

### HybridBuild Fleet Ops dependencies

| RVA(s) | Role |
| ---: | --- |
| `0x1222c0`, `0x122514`, `0x12255c` | Producer start and active cancel/finish entries; cancel/finish may already be FeaturePack gateways |
| `0x1e23ec` | ControlButton-state ModeInfo helper |
| `0x1e7f74` | original producer-wireframe drawing helper |
| `0x212e10`, `0x212350` | sprite-database and screen-dimension globals |
| `0x1e32d4`, `0x1e34b4`, `0x1e3498` | sprite lookup, colour, and scaled-2D drawing helpers |
| `0x1ee868`, `0x1eeb14` | active Fleet Ops ShipDisplay display/simulate targets called before A2FO's post-pass |
| `0x10ad40` | expected original ObjectControlButton press target retained after the vtable replacement |
| `0x247ef4` | base of the 64-entry PopupPalette button-pointer array |
| `0x247f0c`, `0x247f14`, `0x247f1c`, `0x247f18`, `0x247f20` | spare, preferred/fallback Evolve, Construction, and moved-AI root-button pointer slots |

### Validation-only signature ledger

These routines and caller fragments are not patched at the listed site, but a
feature compares the exact bytes before enabling code that depends on them.
Signatures already present in the direct patch tables are not duplicated here.

| Owner | Image | RVA | Expected bytes | Dependency |
| --- | --- | ---: | --- | --- |
| core | Armada | `0x00001370` | `a1 dc 47 73 00 c3` | current `AiMission` getter |
| core | Armada | `0x0cd370` | `55 8b ec 8b 45 08` | class lookup by ODF name |
| core | Armada | `0x0cd390` | `55 8b ec 81 ec 84 00 00 00` | replacement-object construction |
| core | Armada | `0x0ce370` | `8b 89 cc 01 00 00 e9 25 b0 18 00` | class ODF-name getter |
| core | Armada | `0x0cfd50` | `8b 41 04 83 c0 44 c3` | object transform getter |
| queue | Armada | `0x0cd150` | `55 8b ec a1 f8 0b 74 00` | class lookup by project ID |
| queue | Fleet Ops | `0x1229b8` | `55 8b ec 51 53` | checked native queue push |
| upgrade pods | Armada | `0x096340` | `55 8b ec 8b 45 08` | team manager lookup |
| upgrade pods | Armada | `0x0987d0` | `55 8b ec 8b 55 08 8b 45 0c` | native multiplier projection |
| upgrade pods | Armada | `0x0cd370` | `55 8b ec 8b 45 08` | class lookup by name |
| upgrade pods / HybridBuild | Armada | `0x0cd1f0` | `55 8b ec 6a ff 68` | class lookup by project ID before the later core field-capture hook |
| upgrade pods / HybridBuild | Armada | `0x135200` | `55 8b ec 81 ec 40 01 00 00` | `ParameterDB::GetProjectId` |
| Bink | Fleet Ops | `0x1aa470` | `53 83 c4 80 8b d8` | original TBinkIntro frame renderer |
| Bink | Armada | `0x064350` | `55 8b ec 6a ff` | original texture movie renderer |
| HybridBuild | Armada | `0x0afa30` | `55 8b ec 56 57 8b f9` | `ConstructionRig::GetAction` |
| HybridBuild | Armada | `0x0afbc0` | `55 8b ec 83 ec 30 56` | `ConstructionRig::StartBuild` |
| HybridBuild | Armada | `0x0aff00` | `56 8b f1 57 b9 48 6e 73 00` | `ConstructionRig::CancelBuild` |
| HybridBuild | Armada | `0x0aff90` | `55 8b ec 51 53 56 57` | `ConstructionRig::FinishBuild` |
| HybridBuild | Armada | `0x0afea0` | `56 57 8b f9 8b 87 b4 02 00 00` | `ConstructionRig::RemoveConstructionObject` |
| HybridBuild | Armada | `0x0afba0` | `55 8b ec 8b 45 08 56` | `ConstructionRig::GetConstructionMatrix` |
| HybridBuild | Armada | `0x0adc40` | `55 8b ec 8b c1 56` | BuildPositionInterface constructor |
| HybridBuild | Armada | `0x0adc70` | `56 8b 71 34 85 f6` | BuildPositionInterface destructor |
| HybridBuild | Armada | `0x073aa0` | `55 8b ec 6a ff 68` | placeholder-render support probe |
| HybridBuild | Armada | `0x031194` | `8b 73 30 8b ce 8b 06 ff 90 38 01 00 00 84 c0` | build-command cleanup `IsBusy` caller |
| HybridBuild | Armada | `0x031495` | `8b 06 8b ce ff 90 38 01 00 00 8b 7d 08 84 c0` | build-command admission `IsBusy` caller |
| HybridBuild | Armada | `0x0311a5` | `8b ce e8` | build-command cleanup queue-pop caller prefix |
| HybridBuild | Armada | `0x031525` | `8b ce e8` | build-command replacement queue-pop caller prefix |
| HybridBuild | Armada | `0x0b9170` | `55 8b ec 8b 81 58 02 00 00` | `Producer::GetConstructionMatrix` |
| HybridBuild | Armada | `0x0b0e10` | `55 8b ec 53 56 57` | `Evolver::mSwapObjects` |
| HybridBuild | Armada | `0x0b1150` | `55 8b ec 56 57` | `Evolver::GetConstructionMatrix` |
| HybridBuild | Armada | `0x0b04f0` | `55 8b ec 6a ff` | `Evolver::mStartConstructionEffect` |
| HybridBuild | Armada | `0x0b0770` | `56 8b f1 8b 86 ac 02 00 00` | `Evolver::mDoRemoveConstructionEffect` |
| HybridBuild | Armada | `0x0b08d0` | `56 8b f1 8b 86 ac 02 00 00` | `Evolver::mCancelConstructionEffect` |
| HybridBuild | Armada | `0x0b0970` | `56 8b f1 8b 86 ac 02 00 00` | `Evolver::mStopConstructionEffect` |
| HybridBuild | Armada | `0x0b0a10` | `55 8b ec 81 ec ac 00 00 00` | `Evolver::mUpdateConstructionEffect` |
| HybridBuild | Armada | `0x0b1170` | `55 8b ec 53 56` | `Evolver::RenderInternal` |
| HybridBuild | Fleet Ops | `0x1222c0` | `55 8b ec 83 c4 ec 53` | Producer start callback |

Several public entries are deliberately **not** compared with their on-disk
prologues at module initialization: the core has already detoured
`ParameterDB::GetString`; Fleet Ops may own the game allocator/deallocator and
debriefing entry; and FeaturePack may already wrap Fleet Ops Producer
cancel/finish. HybridBuild intentionally calls those active public entries so
the installed policies compose. Checked call sites, expected vtable targets,
module identity, and the tables above protect the surrounding integration.

## Reverse-engineered structure and object offsets

These are byte offsets from runtime objects or records, not module RVAs. They
are just as build-specific as code addresses.

### Shared Armada object/queue layout

| Object/record field | Offset |
| --- | ---: |
| object flags / expired / handle / class / label / team / race | `+0x14` / `+0x27` / `+0x28` / `+0x40` / `+0x48` / `+0xec` / `+0xfc` |
| class tag / basename / project ID / menu capabilities | `+0x70` / `+0x7c` / `+0x1cc` / `+0x1d4` |
| GameObjectClass minimap sprite / icon (`mapSprite` / `mapIcon`) | `+0x18c` / `+0x190` |
| ST3D_Texture name / width / height / per-device wrappers | `+0x08` / `+0x1c` / `+0x20` / `+0x40` |
| Producer current build class / construction effect | `+0x254` / `+0x268` |
| Producer queue head / count / current ID / next ID | `+0x270` / `+0x274` / `+0x2a0` / `+0x2a8` |
| queue-item next / queue ID | `+0x08` / `+0x0c` |
| Producer class build-item array | `+0x450` (`57` entries) |

### Recursive FOFS layout

| Record field | Offset |
| --- | ---: |
| file-entry next / basename / project ID / packed flag | `+0x08` / `+0x0c` / `+0x14` / `+0x18` |
| file-entry mod info / primary-root / overridden | `+0x1c` / `+0x20` / `+0x21` |
| mod-info priority | `+0x3c` |

Fleet Ops has `28` built-in virtual directories; A2FO caps the combined list
at `255`.

### Upgrade-pod layout

| Object/record field | Offset |
| --- | ---: |
| ResearchPod attached station | `+0x250` |
| pod-class is-upgrade / tier / system / multiplier | `+0x450` / `+0x454` / `+0x458` / `+0x45c` |
| station-class secondary items / flags | `+0x494` / `+0x4cc` |
| station-instance pod count / pod array / secondary items | `+0x2ac` / `+0x2b0` / `+0x2c0` |

The Producer list has `57` entries, the secondary station table has `58`, and
Armada exposes `5` ship-upgrade systems. A2FO's policy hard-cap is tier `16`.

### Bink layout

| Object/record field | Offset |
| --- | ---: |
| TBinkIntro D3D9 device | `+0x44` |
| Armada viewport-owner full-render rectangle | `+0x1b0` |

### HybridBuild-specific layout

| Object/record field | Offset/size |
| --- | ---: |
| Producer button list / enabled mask | `+0x130` / list `+0x08` |
| ControlButton state / ModeInfo | `+0x34` / `+0x84` |
| ModeInfo size; type / target class / action index | `0x18`; `+0x04` / `+0x0c` / `+0x14` |
| update-build-buttons vtable slot / popup current menu | `+0xe8` / `+0x124` |
| ShipDisplay queue / selected object | `+0x120` / `+0x1e8` |
| BuildWireframe owner / target class; RaceIcon owner | `+0x28` / `+0x3c`; `+0x28` |
| ResearchPod class flag / level / family | `+0x450` / `+0x454` / `+0x458` |
| Evolver protected tail / size | `+0x2ac` / `0x1c` |
| Evolver-tail opacity / build-start position | tail `+0x04` / tail `+0x10` |
| ConstructionRig protected tail / size | `+0x2a4` / `0x14` |
| ConstructionRig tail interface / sound handle / timer / object ID | tail `+0x00` / `+0x08` / `+0x0c` / `+0x10` |
| construction-matrix position | `+0x24` |
| BuildPositionInterface size / matrix | `0x3c` / `+0x04` (`0x30` bytes) |
| BuildCommand target class | `+0x20` |
| GetAction / ConstructionRig IsBusy / matrix vtable slots | `+0x90` / `+0x138` / `+0x188` |
| class has-geometry vtable slot | `+0x28` |
| root-button action record size / sprite stem | `0x10` / record `+0x08` |

Other fixed layout limits are `57` runtime build-list entries, `14` native
research buttons, `64` Fleet Ops popup buttons, and `10` native queue slots.

### Indexed hull-turret layout

| Object/record field | Offset/size |
| --- | ---: |
| Armada Matrix34 right / up / forward / translation vectors | `+0x00` / `+0x0c` / `+0x18` / `+0x24`; total `0x30` bytes |
| `AiMission` add-object virtual slot | vtable `+0x18` |
| supported indexed parent pairs | `0..64`, sparse |

The turret runtime stores only engine handles across simulation frames. Raw
class and hierarchy-node pointers are retained only for process-lifetime class
policy and immutable model-node lookup respectively.

## Ownership and installation order

1. The proxy `Win2kDisableTaskSwitch.dll` loads
   `Win2kDisableTaskSwitch.original.dll`; the original entry point loads
   `..\FleetOpsHook.dll`.
2. The proxy attaches `A2FOExtensions.dll`. The timing-critical core sites are
   installed during process attach before Armada class/settings/profile work.
3. The deferred worker revalidates the binaries, installs any core site not
   already ready, and discovers modules outside the loader lock.
4. When required by the active parent chain, the globally installed
   `A1Compat.dll` loads first by deterministic filename order. It registers A1
   policy only when an active extension root contains `a1compat.ini`, including
   `Addon` ODF overlay precedence, and owns the four A1-scoped
   diagnostic/defensive hooks listed above.
5. `A2FOAlwaysShowShields.dll` validates the native shield-effect routines
   and exports its optional compatibility observers.
6. `A2FOCheats.dll` installs its resource handler and either updates the live
   cheat registry immediately or chains `ChatHookInit` and performs command
   registration after Fleet Operations initializes it.
7. `A2FOCraftIdentity.dll` chains Fleet Operations' CraftClass constructor and
   installs the two-field selected-object panel draw, aligned to the native
   ship-name row. It has no simulation, RNG, or save mutation.
8. `A2FOEditMenu.dll` installs its checked editor update hook and only replaces
   the active native category buffer while the user is inside a recursively
   linked submenu. The stock renderer and placement path remain authoritative.
9. `A2FOFireArcs.dll` registers its WeaponClass and trigger policies with the
   core, then installs the checked firing-arc gate and ShipSystemIcon hook.
   It changes a WeaponClass only when its completed ODF contains a valid new arc
   policy; the UI hook is otherwise a pass-through.
10. `A2FONormalWeaponTech.dll` validates its read-only weapon/team technology
   bridge and registers its normal-weapon trigger decision with the core.
11. The core owns the isolated DX8 DOT3 shader sites early because Armada may
   build that shared shader before deferred modules load. The sites remain
   pass-through unless `A2FONebulaRenderer.dll` and its assets exist at first
   DOT3 use. The deferred DLL is an opt-in/status/ODF controller; optional
   class registration and Craft render context travel through already-owned
   cooperative boundaries and do not add hook sites. DX9 never enables the
   custom renderer.
12. Deterministic filename ordering loads `A2FOFeaturePack.dll` before
   `A2FOHybridBuild.dll`. FeaturePack owns shared Producer and upgrade-station
   sites; HybridBuild composes with them through its private callback bridge.
13. `A2FOInfoIni.dll` owns policy only. It registers a provider and installs no
   binary patch itself; the timing-sensitive settings hooks remain core-owned.
14. `A2FOPointDefenseCycles.dll` preflights its Armada and Fleet Operations
   class, simulation, pre-fire gate, timer, save/load, cleanup, and successful
   firing-attempt sites before installing its timing-only runtime. Weapon ODFs
   without a numbered cycle use ordinary `shotDelay`; PointDefenseLaser now
   enforces it before attempting to fire.
15. `A2FOResources.dll` registers completed GameObjectClass/Race field
   observers, owns Team lifetime/payment/panel sites, and receives exact refund
   notifications from FeaturePack's shared Producer hooks. Its four balances
   remain separate from the six native `ResourceInterface` slots.
16. `A2FORGBTextures.dll` loads after FeaturePack in filename order and owns an
   independent, conditional Armada `fopen` IAT bridge, the TGA
   FileExists/OpenRead routes, and the validated texture-lock/minimap guards.
   FeaturePack remains the sole semantic handler registered with the core FOFS
   dispatcher.
17. `A2FOSwarmSystem.dll` preflights its worker-bee frame boundaries and all
   native parsing, hierarchy, transform, visibility, instance, and allocation
   helpers before installing either hook. It has no shared semantic callback
   dependency and creates no `GameObject` entries.
18. `A2FOTextureVariants.dll` registers its Race name and optional texture
   suffix fields with the core-owned completed-Race dispatcher, then follows live craft ownership
   through the checked Fleet Operations update callback. Its render wrapper
   retains the native Borg texture/node route and selects only custom nodes
   matching the current owner's Race name. `A2FOCraftIdentity.dll` lazily
   resolves its exported completed-CraftClass observer for numbered subsystem
   mesh/explosion policies; no second CraftClass entry hook is installed while
   that owner is active. If it is not selected, TextureVariants uses the exact
   validated Fleet Ops constructor chain as its independent fallback.
19. `A2FOTurrets.dll` then registers the global semantic `turret` policy and
   preflights all six of its Armada runtime sites before installing any of
   them. Child creation is deferred until the configured parent first
   simulates, after class and ODF loading has completed. Its shared callbacks
   also apply configured persistent shield visibility after native simulation
   and release it before native Craft cleanup.
20. `A2FOWeaponDamageControls.dll` loads after FireArcs by filename order,
   chains its live WeaponClass constructor detour when present, and installs
   the common Craft damage hooks. Weapon ODFs without any of the four damage
   commands retain no sidecar and stay on the exact native damage path.
21. The completed-class, completed-Race, and destroyed-object hooks are
   installed after native-module registration and only when at least one handler
   needs each semantic boundary.

Low-level hooks are process-lifetime changes. Each feature validates all of
its required byte signatures before its first patch wherever its architecture
allows; an unsupported binary disables the affected feature instead of using
wildcard signatures.

## Address provenance and maintenance rule

Friendly function names come from the Armada II 1.1 symbols, the Fleet Ops
public map, and instruction-level disassembly of the exact identities above.
COFF map entries of the form `0001:offset` are section-relative offsets, not
image RVAs. For both reference binaries' `.text` sections, add the section RVA
`0x1000`; equivalently, use the preferred VA printed in the map minus the PE
image base. The tables record the resulting image RVAs used by the
implementation.

The operational source of truth is the `k*Rva` constants and `kExpected*`
arrays in the linked source files. Any change that adds, removes, or moves a
binary address must update this register in the same change, including:

- image and RVA;
- whether the location is patched, called, read, or used as a continuation;
- patch kind and exact overwritten bytes/width;
- owner and semantic purpose;
- validation/provenance notes;
- any related object-layout offsets or module-order assumptions.
