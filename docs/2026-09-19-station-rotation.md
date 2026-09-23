# Station rotation prototype — 2026-09-19

Scope: R/Shift+R construction placement, four cardinal facings, rotated
placement/pathfinding bounds and native station hardpoint/rally transforms.
Implemented in `A2FOStationRotation`; no core API change.

## Orders and transforms

Only class+position builds inside the user's PopupPalette broadcast receive
markers `0xb1`/`0xb2`/`0xb3` for one/two/three quarter-turns. Zero remains
`0x19`. Native packet type `0x0c` carries that byte, owner handle, class ID and
three unchanged position floats. The receiver restores `0x19` before native
SetCommand and writes `0x52393000 | turns` into AiCmdInfo.param (`+0x10`).
Native AiCmdInfo Save/Load serialize this integer. Other commands and AI builds
remain unchanged. All peers need the same enabled module.

The Handle_New_Command CALL applies the admitted command angle to the
BuildPositionInterface Matrix34 after native snapping and cookie generation.
Exact cardinal bases avoid accumulated rounding. Unmarked orders reset the
basis. HybridBuild's interface selection at `0x314d4` remains intact.
ConstructionRig::GetConstructionMatrix (`0xafba0`) copies this matrix;
StartBuild (`0xafbc0`) creates the ConstructionObject with it. FinishBuild
passes the construction matrix into the finished object.

Fleet Ops' FinishBuild matrix override (`0x1d1688`) can use a cached matrix.
Its capture at `0x1d1588` copies the construction object's complete world
transform into sidecar `+0x38` (validity `+0x68`), preserving this rotation.
No extra cache hook is required.

Producer::GetConstructionMatrix (`0xb9170`) obtains its construction hardpoint's
world transform through Entity::GetNodeTransform (`0xcff90`). Native
Shipyard::mRecomputeRallyPoint (`0xbc150`) uses that matrix's forward vector,
bounding-sphere radius and map-edge handling. The harness executes this actual
native rally routine for all four facings. Player-set rally destinations remain
world-space positions.

## Footprints

ST3D_Instance::GetSynchronousBoundingBox (`0x22ed30`) returns unrotated local
model bounds. CraftClass adds directional margins without using object facing.
Rotating the render matrix alone therefore cannot fix occupied cells.

Scoped UI hooks rotate the visible footprint and the arguments passed to
Station_Placement::Is_Clear_For_Placement. Fleet Ops still performs snapping
and restrictions; the rotation pivot comes from its already-snapped rectangle.
A CraftProcess::Build scope reads the saved BuildPositionInterface for its
two CanPlaceHere calls (`0x358d7`, `0x359b0`), independently of UI state.

Twelve checked path-planner CALLs supply their live owner to x86 bridges. The
helper reads the instance matrix, rotates the padded local rectangle, then
subtracts the original margins into a private BBOX. Native Add/Remove reapply
those margins, obtaining the rotated area. Shared class geometry is untouched.
Both registration and removal derive the angle from saved object state. The
checked FO AddToPathPlanners redirect remains in the chain for avoidMe policy.

## Hook ledger

Addresses are RVAs: preferred Armada base `0x400000`, FO base `0x5a800000`.
All signatures are checked before any module mutation. Inline overwrite
lengths end at instruction boundaries.

| Image / RVA | Boundary | Checked bytes |
| --- | --- | --- |
| Armada `0x0dd570` | KeyboardDriver::ReadChannels | `55 8b ec 8b 4d 10` |
| Armada `0x0fdca0` | PopupPalette broadcast | `55 8b ec 6a ff` |
| Armada `0x0d42d0` | class+position QueueCommand | `55 8b ec 56 8b f1` |
| Armada `0x0d1cb0` | class+position SetCommand | `55 8b ec 64 a1 00 00 00 00` |
| Armada `0x0738a0` | placeholder model render | `55 8b ec 83 ec 0c` |
| Armada `0x031500` | set placement CALL | `e8 8b c7 07 00` |
| Armada `0x0355d0` | CraftProcess::Build | `55 8b ec 83 ec 4c` |
| Armada `0x04ab70` | ActionMode::GetAction | `55 8b ec 6a ff` |
| FO `0x1fb3b0` | enhanced RenderFootprints | `55 8b ec 81 c4 48 ff ff ff` |
| Armada `0x0c0240` | GetFootprint | `55 8b ec 8b 81 d8 01 00 00` |
| FO `0x10db80` | enhanced CanPlaceHere | `55 8b ec 83 c4 84` |
| Armada `0x094de0` | placement clearance | `55 8b ec 83 ec 4c` |

Without FO, CanPlaceHere uses Armada `0xc01b0`, checked bytes
`55 8b ec 8b 81 d8 01 00 00`. Other checked callees: set placement `0xadc90`
(`55 8b ec 53 56`), AddToPathPlanners `0xc0e40` (`55 8b ec 56 8b f1`, or
E9 or PUSH/RET to checked FO `0x10df84`), RemoveFromPathPlanners `0xc0f40`
(`55 8b ec 53 56 8b f1`). FO Add starts `55 8b ec 51 89 4d fc`.
Text-entry check: Armada `0x25f120`, bytes `a1 bc 0f 7b 00`. Without FO,
RenderFootprints uses Armada `0x73930`, bytes `55 8b ec 83 ec 3c`.

Each path CALL checks the complete five-byte E8 instruction and displacement
to the original Add/Remove address:

| Armada CALL | Operation | Owner register / caller |
| --- | --- | --- |
| `0x07e534` | Add | ESI / CaptureTheFlag post-load |
| `0x0a34be` | Add | EBX / ConstructionObject SetBuildClass |
| `0x0a36fb` | Add | EBX / ConstructionObject post-load |
| `0x0b0440` | Add | ESI / Evolver post-load |
| `0x0b0f1a` | Add | ESI / Evolver StartBuild |
| `0x0b125c` | Add | ESI / Evolver add footprint |
| `0x0d05db` | Add | ESI / GameObject InitializeGeometry |
| `0x0a3560` | Remove | EBX / ConstructionObject destructor |
| `0x0b0f99` | Remove | ESI / Evolver CancelBuild |
| `0x0b1003` | Remove | ESI / Evolver FinishBuild |
| `0x0b129d` | Remove | ESI / Evolver remove footprint |
| `0x0c217e` | Remove | EDI / Craft destructor |

Position bridge: ECX = interface, EDI = AiCmdInfo, stack = position/class,
ret 8. Planner bridges: ECX = class, stack = position/BBOX/bool, ret 12.
Nonvolatile registers are preserved. ActionMode::GetAction is cdecl with a
hidden result pointer; other wrappers use thiscall (fastcall plus unused EDX
in MinGW).

Layouts: active mode pointer Armada `0x364e48`, mode type `+4`, class `+0xc`;
keyboard scan-code bits (R = bit 18); AiCmdInfo object `+0x7c`; BuildPositionInterface
matrix `+4`, cookie `+0x34`, valid `+0x38`; Producer interface `+0x2a4`;
CraftProcess owner `+0x30`; object instance `+4`, instance matrix `+0x44`;
class geometry `+0x1d8`, BBOX geometry `+4`; class clearance `+0x214`, margins
+X/-X/+Z/-Z at `+0x218/+0x21c/+0x220/+0x224`. Vertical margins stay native.

## Validation and limits

Policy tests cover forward/reverse presses, held-key suppression, focus/chat
and class transitions, wraparound, all command bytes,
exact matrices and asymmetric bounds/margins. The headless x86 harness maps
the current EXE/DLL without running their entry points, validates signatures
and FO Add redirection, tests signature rejection and partial installation,
executes every bridge variant with stack/register checks, tests different
sender/receiver preview angles and construction clearance, and executes native
footprint and shipyard rally code. Other native services use narrow stubs.

All 24 mutations must succeed before input consumption or rotated command
encoding is enabled. Partial hooks remain resident as inactive pass-throughs.
This is a build-specific prototype; future binary changes may disable it.
In-game, multiplayer, full save/load and model-specific launch/repair checks
remain manual acceptance work listed in the module README.

## Initial wheel prototype deployment

Installed `Data/modules/A2FOStationRotation.dll` and enabled it as `active28`
in the base Fleet Operations: Roots `Data/info.ini`. Mods with their own
managed selections must select it separately. DLL SHA-256:
`3cbd164a3acd421375884af56d04b0c201ca730e84731ef25893f523f217f1c0`.

`make verify`, `make test`, the station regression and the general DLL-loading
smoke passed. The pre-change policy and deployment manifest are under
`Data/rollback/2026-09-19-station-rotation-163958`. The core, ArmadaL.exe,
FleetOpsHook.dll and other deployed modules were not replaced.

## R/Shift+R revision and live-load fix

The initial game log reported a signature mismatch and left the module
inactive. The original preflight accepted only E9 for the native AddToPathPlanners
redirect. FO's HookCodeNt (`0xf75f8`) installs `68 <absolute callback> c3` instead.
Both encodings are now accepted only when they target the exact checked FO
callback; arbitrary redirects still fail. Signature failures now log the image
and RVA. The headless fixture reproduces the PUSH/RET form and tests rejection
when its destination changes.

FO replaces the RenderFootprints CALL at Armada `0x193343` with its own function
at FO `0x1fb3b0`, bypassing Armada `0x73930`. The scope hook now targets that FO
replacement, retaining the stock entry as the no-FO fallback.

KeyboardDriver::ReadChannels is thiscall with three stack arguments (device,
axes, button-bit array) and an integer status result. Its original routine
copies four 32-bit scan-code words. The module then reads R (scan code `0x13`,
bit `0x12`) from that returned game buffer. While placement is active it
removes only that R bit before Input.map evaluates bindings, preventing a
simultaneous Repair command. OS input, the driver's physical key state and
text-character queue are not altered. Shift selects reverse rotation; Ctrl/Alt,
chat and background focus pass through. Held-key tracking requires release
before another turn and survives mode/focus changes. There is no camera hook
or wheel-accumulator access in the module.

The regression executes the actual native keyboard copy, verifies unrelated
bits and normal R pass-through, and confirms that the wheel accumulator stays
unchanged while the full placement/footprint/rally regression runs.

The R/Shift+R revision passed `make verify`, `make test`, the station
regression and DLL-loading smoke, and replaced only the station module DLL.
SHA-256: `c28cfa624cfdaa139fa500a35c2f9d01c0af32499d730c68191f7adac23ecb98`.
The previous DLL and pre-update game log are under
`Data/rollback/2026-09-19-station-rotation-r-165334`. Runtime confirmation awaits a game restart.
