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

The core rejects `ArmadaL.exe` or `FleetOpsHook.dll` unless their PE timestamp
and `SizeOfImage` match. SHA-256 and file size identify the exact local
reference binaries used for this audit; they are not currently runtime checks.

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

The maintained source currently contains 82 fixed-address mutation sites plus
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
| Armada | `0x0cd1f0` | inline | 5 | `55 8b ec 6a ff` | `game_object_class_find_project_id_hook`; associate captured fields with the completed class |
| Armada | `0x0c6ab0` | inline | 6 | `55 8b ec 83 ec 20` | `craft_explode_hook`; native/Lua destroyed-object dispatch and replacement publication |
| Armada | `0x13c8a0` | JMP | 6 | `b8 05 00 00 00 c3` | `default_user_profile_game_speed_hook`; supply the selected first-run game speed |
| Fleet Ops | `0x105fec` | CALL | 5 | `e8 23 3c 00 00` | `fofs_item_get_hash_lookup_hook`; FOFS item-get lookup dispatch |
| Fleet Ops | `0x1061e2` | CALL | 5 | `e8 2d 3a 00 00` | same dispatcher for item-locate |
| Fleet Ops | `0x106263` | CALL | 5 | `e8 ac 39 00 00` | same dispatcher for item-exists |
| Fleet Ops | `0x1063ee` | CALL | 5 | `e8 21 38 00 00` | same dispatcher for project-ID lookup |
| Fleet Ops | `0x10ab98` | inline | 5 | `53 56 57 8b f2` | `mod_user_directory_hook`; semantic `SettingsDirectory` override |
| Fleet Ops | `0x13e744` | inline | 5 | `55 8b ec 6a 00` | `fo_settings_get_instance_hook`; apply/save the first-run `Settings.xml` speed |
| Fleet Ops | `0x13e93c` | inline | 5 | `55 8b ec 51 53` | `game_configuration_new_hook`; initialize new configuration defaults |
| Fleet Ops | `0x13ea8c` | inline | 5 | `55 8b ec 51 53` | `game_configuration_load_profile_hook`; preserve the configured runtime default across profile loading |

The object-destroyed pair at `0x0cd1f0` and `0x0c6ab0` is installed only when
a native or Lua destroyed-object handler has registered. The other core sites
are installed before class, ODF, settings, or profile loading as appropriate.

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
| Armada | `0x000f3770` | inline | 6 | `55 8b ec 83 ec 08` | `selected_info_render_hook`; retain the complete stock state-1 single-object panel renderer, read its selected craft, and draw the aligned captain/registry rows in the same local rectangle space and live display/scissor state as the native captain component |

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
reads `InfoDisplay`'s selected craft and native captain text component at
`+0x1e8` and `+0xbc`. The captain component's live LTRB rectangle at `+0x58`
anchors both extension fields without reapplying the panel origin.

`A2FOFireArcs.dll` owns the optional three-dimensional weapon-arc runtime:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x00264e30` | inline/JMP chain | 5/6 | stock `55 8b ec 6a ff`, or Fleet Ops' live `68 <handler> c3` resolving exactly to RVA `0x0010ef74` whose handler begins `55 8b ec 83 c4 dc 53` | `weapon_class_constructor_hook`; chain Fleet Operations' WeaponClass enhancement, parse the completed ODF's optional box/cone policy, and retain it by class pointer without mutating the stock WeaponClass arc fields |
| Armada | `0x0026f8c0` | inline/JMP chain | 6 | stock `55 8b ec 83 ec 1c`, or Fleet Ops' live `68 <handler> c3` resolving exactly to RVA `0x001358ac` whose handler begins `55 8b ec 83 c4 f4 53` | `weapon_can_fire_at_hook`; chain Fleet Operations' target filter, preserve native authorization, then apply only the configured horizontal envelope so Armada's two-dimensional attack AI can turn into yaw coverage safely |

Both hooks accept only Fleet Operations' exact checked handlers or the
untouched stock prologues. The firing hook is installed first, but remains a
pass-through until both sites are ready. A weapon ODF without any new arc
commands never enters module policy: its existing `restrictFireArc` byte and
native `fireArc` path remain unchanged. A valid custom policy's horizontal
envelope is applied only after the native range, obstruction, and optional
stock-arc checks succeed. `A2FOFireArcs_AllowWeaponTrigger` exports the full
box/cone decision for A2FOTurrets' existing late weapon-trigger hook; this
suppresses shots outside pitch coverage without entering Armada's unsafe
vertical attack-movement path.

The module reads numeric ODF angles through `ParameterDB::GetFloat` at Armada
RVA `0x00134df0`, with the active `ParameterDB::GetString` entry at
`0x00135350` used for mode names and quoted-number compatibility. It obtains
the live owner through `Weapon::GetOwner` at `0x00271050`, reads its matrix
through `Entity::GetTransform` at `0x000cfd50`, and reads the target position
from the same validated `GameObject+0xac` coordinates used by native target
authorization. A Weapon instance's WeaponClass pointer is at `+0x04`; Armada
Matrix34 uses right/up/forward/translation vectors at
`+0x00`/`+0x0c`/`+0x18`/`+0x24`.

`A2FONormalWeaponTech.dll` installs no independent engine hook. A normal
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

`A2FOTurrets.dll` owns the global indexed linked-turret runtime:

| Image | RVA | Kind | Bytes | Expected bytes | Handler and purpose |
| --- | ---: | --- | ---: | --- | --- |
| Armada | `0x000cc480` | inline/JMP chain | 5/6 | stock `55 8b ec 6a ff`, or Fleet Ops' live `68 <handler> c3` resolving exactly to RVA `0x0010bd80` whose handler begins `55 8b ec 83 c4 e8 53` | `game_object_class_constructor_hook`; chain Fleet Operations' class enhancement, then retain sparse `turret0..64`/`turretHardpoint0..64` parent pairs and semantic-turret rotation policy from the completed ParameterDB |
| Armada | `0x000c1fd0` | inline | 9 | `56 8b f1 8b 8e 34 02 00 00` | `craft_cleanup_hook`; unlink a destroyed turret and expire every child when its parent is cleaned up |
| Armada | `0x000c2870` | inline | 5 | `53 56 57 8b f1` | `craft_post_load_hook`; parse deterministic `A2FOT:<parent>:<index>` labels and stage child-parent reconnection without depending on object load order |
| Armada | `0x000c6530` | inline/JMP chain | 7/6 | stock `55 8b ec 53 8b 5d 08`, or Fleet Ops' live `68 <handler> c3` resolving exactly to RVA `0x001dcebc` whose handler begins `55 8b ec 51 53 89 4d` | `craft_simulate_hook`; chain Fleet Operations' existing simulation enhancement, then create pending child objects, reconnect loaded children, propagate ownership, slew them toward their current weapon target, and apply the mount-relative transform |
| Armada | `0x00271290` | inline | 6 | `55 8b ec 8b 45 08` | `weapon_trigger_object_hook`; retain the target passed through native automatic `Weapon::Trigger(GameObject const*)` firing for visual yaw/pitch tracking, then apply the optional A2FOFireArcs full-3D and A2FONormalWeaponTech team-tech filters before entering the native trigger |
| Armada | `0x00271340` | inline | 6 | `55 8b ec 8b 45 08` | `weapon_set_target_hook`; retain each semantic turret object's native weapon target handle for visual yaw/pitch tracking |

The module preflights all six sites before its first hook installation.
Fleet Operations already detours the GameObjectClass constructor and
`Craft::Simulate` during startup with absolute `push handler; ret` transfers.
A2FOTurrets accepts only the exact supported handlers at Fleet Ops RVAs
`0x0010bd80` and `0x001dcebc`, retains them as its original call targets, and
replaces the two existing entry transfers with checked A2FO jumps. The
untouched stock prologues remain supported for isolated fixtures.
It transactionally registers `turret -> sensor` and seven missing-only
defaults which hide the native child SensorArray from the interface, make it
an `avoidMe = 0` object, and remove its footprint/avoidance bookkeeping. The
child remains a complete native object and therefore owns ordinary weapons,
hitpoints, targeting, rendering, and save data.

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
| Armada | `0x3b7f8c` | PTR | 4 | slot equals the resolved `msvcrt!fopen` export | `legacy_texture_fopen`; resolve explicit RGB/Index8/Compressed requests plus root/RGB/prepared-Index8/prepared-Compressed TGA fallback while preserving real root/mod assets; DDS requests are never redirected to TGA bytes |
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
priority. The remaining Armada path is a true-colour loader, so RGB files may
answer directly while colour-mapped, 16-bit, grayscale, or RLE Index8/
Compressed candidates are expanded to bounded temporary 24/32-bit TGAs first.
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
| ST3D_Texture name / width / height | `+0x08` / `+0x1c` / `+0x20` |
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
4. When present in the active parent chain, `A1Compat.dll` loads first by
   deterministic filename order. It registers A1 policy only when an active
   extension root contains `a1compat.ini`, including `Addon` ODF overlay
   precedence, and owns the four A1-scoped diagnostic/defensive hooks listed
   above.
5. `A2FOCheats.dll` installs its resource handler and either updates the live
   cheat registry immediately or chains `ChatHookInit` and performs command
   registration after Fleet Operations initializes it.
6. `A2FOCraftIdentity.dll` chains Fleet Operations' CraftClass constructor and
   installs the two-field selected-object panel draw, aligned to the native
   ship-name row. It has no simulation, RNG, or save mutation.
7. `A2FOFireArcs.dll` chains Fleet Operations' WeaponClass constructor and
   installs the checked native firing-arc gate. It changes a WeaponClass only
   when its completed ODF contains a valid new arc policy.
8. `A2FONormalWeaponTech.dll` validates its read-only weapon/team technology
   bridge and exports the optional normal-weapon trigger decision before
   A2FOTurrets claims the shared trigger hook.
9. Deterministic filename ordering loads `A2FOFeaturePack.dll` before
   `A2FOHybridBuild.dll`. FeaturePack owns shared Producer and upgrade-station
   sites; HybridBuild composes with them through its private callback bridge.
10. `A2FOInfoIni.dll` owns policy only. It registers a provider and installs no
   binary patch itself; the timing-sensitive settings hooks remain core-owned.
11. `A2FORGBTextures.dll` loads after FeaturePack in filename order and owns an
   independent, conditional Armada `fopen` IAT bridge, the TGA
   FileExists/OpenRead routes, and the validated texture-lock/minimap guards.
   FeaturePack remains the sole semantic handler registered with the core FOFS
   dispatcher.
12. `A2FOTurrets.dll` then registers the global semantic `turret` policy and
   preflights all six of its Armada runtime sites before installing any of
   them. Child creation is deferred until the configured parent first
   simulates, after class and ODF loading has completed.
13. The destroyed-object hooks are installed after native/Lua registration and
   only when at least one handler needs them.

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
