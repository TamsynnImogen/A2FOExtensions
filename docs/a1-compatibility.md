# Armada 1 compatibility

## Scope

`STA1 Classic` is a minimal Fleet Operations parent mod intended to let stock
Armada 1 content and third-party A1 addons run with as little manual conversion
as practical. It uses the Armada 2/Fleet Operations interface rather than
attempting to recreate A1's original LDL menus.

The initial parent chain is:

```text
Fleet Ops 4.0
  `-- STA1 Classic
        `-- original A1 child addon
```

This milestone establishes the parent metadata and the optional
`A1Compat.dll` boundary, including legacy race discovery for the Instant Action
dropdown. It does not yet claim complete Instant Action gameplay or campaign
compatibility.

## Installation contract

The parent is installed at:

```text
Data/Mods/STA1 Classic/
```

Its `info.ini` contains:

```ini
[mod]
Name=Star Trek: Armada Classic
AssetVersion=50000
ParentMod="Fleet Ops 4.0"
standalone=0

[modules]
required0="A1Compat"
required1="A2FORGBTextures"
```

The root also contains `a1compat.ini`, an extension-owned activation marker.
Its optional compatibility-isolation setting is:

```ini
[A1Compat]
SafeMode = 0
```

It accepts `1/true/yes/on/enabled` and `0/false/no/off/disabled`.
Safe mode retains the `wingman` alias, missing-only class defaults, `Addon`
overlay, and legacy race-menu fallback while withholding the riskier
executable compatibility and officer-quarter runtime hooks.

`A1Compat.dll` and `A2FORGBTextures.dll` belong in the central `Data/modules`
directory. The parent's required module policy selects them for the parent and
its children; mods cannot ship or override DLLs in their own folder.
`A2FORGBTextures.dll` remains inert when the corresponding legacy texture
directories are absent.

The DLL additionally checks the active extension-root chain for
`a1compat.ini`. If the DLL is mistakenly selected outside the A1 parent,
initialization is rejected unless the marker is genuinely present in the
active chain.

An A1 child addon uses:

```ini
[mod]
ParentMod="STA1 Classic"
AssetVersion=50000
standalone=0
```

The child should retain its original data where possible. BZN conversion and
small compatibility overlays are preferred over bulk manual ODF rewriting.

## Initial module behaviour

The first `A1Compat.dll` policy maps:

```ini
classLabel = "wingman"
```

to Armada's compatible native `craft` implementation through the core-owned
classlabel dispatcher. The alias moved out of `A2FOFeaturePack.dll`, preventing
an A1-specific rule from affecting normal FO4 and STA2 mods.

The same transaction registers these `a2craft.odf` values as Wingman-only
defaults: the seven subsystem/crew/hull `HitPercent` commands and the six
`ship`, `has_hitpoints`, `has_crew`, `transporter`,
`SHOW_MOVEMENT_AUTONOMY`, and `can_explore` flags. Armada's normal ParameterDB
lookup runs first, including the complete ODF include chain. A default is
therefore used only when the command is genuinely absent; local and inherited
values are never replaced.

`constructionrig` remains a native Armada 2 classlabel and needs no alias.
A1Compat nevertheless registers the six `a2const.odf` values against that
source identity: `shipclass`, `builder_facility`,
`SHOW_MOVEMENT_AUTONOMY`, `SHOW_SW_AUTONOMY`, `shipType`, and `hotkeyLabel`.
They use the same missing-only precedence as the Wingman defaults.

The native `freighter` classlabel likewise receives `a2freight.odf`'s seven
mining/resource defaults: `shipclass`, `maxDilithium`, `alert`, `miner`,
`SHOW_MOVEMENT_AUTONOMY`, `resourcesCanHandle`, and `hotkeyLabel`. It is not
aliased, and normal ODF/include values still take precedence.

Raw A1 `research` stations receive A2's missing `research = 1` context-menu
capability and `transporter = 1` flag through the same missing-only policy.
This exposes the native research-pod list for stations such as `fresear` and
`fresear2` without modifying their ODFs or overriding converted-mod values.

A1's `scout = 1` is an AI/default-order marker, not the A2 context-menu
capability which exposes Scout/Search. For a completed class whose inherited
ODF sets that legacy marker, A1Compat supplies A2 ScoutBase's missing
`combat`, `alert`, `can_sandd`, and `can_explore` capability bits. Any
explicit or inherited declaration, including zero, remains authoritative.
This restores the stock A1 scouts' Orders palette without editing their ODFs.

There is a separate A1 basename collision: its shared scout ship base is
`scout.odf`, the same basename used by A2's Explore CommandInfo. If flat ODF
selection feeds the ship base to the command loader, A1Compat repairs only the
result whose `buttonName` remained empty, supplying the stock Explore command
identity, Orders position, and `ship + can_explore` source mask. A valid
command ODF is left unchanged and no A1 file is edited.

Raw A1 stations inherit `is_starbase = 1` but predate A2's menu-capability
declarations. For those completed classes, A1Compat supplies missing
`facility`, `has_crew`, and `has_hitpoints` bits so Fleet Operations can expose
the Recrew command. Explicit and inherited declarations, including zero,
remain authoritative and no station ODF is rewritten.

A1 source ODFs are not rewritten to add A2 commands. For A2 maps containing
`scrap` moon records, A1Compat intercepts only `GameObjectClass`'s `resource`
lookup. Normal ODF/include resolution runs first. If the command is absent,
the resolved project ID selects the A2 Classic default:
`mdmoon` plus numbered variants uses `ResourceMoon`, and `mmooninf` plus
numbered variants uses `ResourceMoonInf`. Those exact moon families also
receive missing-only `spatial_object` and `has_resource` capability bits, so
they remain targetable by freighters as well as physically collidable. Other
`scrap` objects, including asteroids and comets, remain untouched.

The module requires the versioned native SDK and registers its alias
transactionally. Rejected registration causes initialization to fail without
leaving partial A1 policy behind. It also installs the A1-scoped,
signature-checked nebula sprite-node guard and compatibility bridges documented
below.

It also registers the original A1 `Addon` directory as an ODF overlay. The
shared FeaturePack filesystem hook indexes `.odf` files found there without
copying them into the structured `odf` tree. Within each mod root, `Addon` wins
over an ordinary structured ODF with the same basename, matching Armada 1.
Normal mod-root priority remains stronger, producing this order:

1. active child `Addon`;
2. active child structured ODF;
3. nearest parent `Addon`;
4. nearest parent structured ODF;
5. remaining parents and shared Data by the same rule.

This milestone covers ODFs only. Other legacy `Addon` asset types still need
their own confirmed lookup bridges.

Fleet Operations' Instant Action menu additionally expects `displayKey` and a
unique `instantActionSlot` in each playable race ODF. Stock Armada 1 supplies
`displayName` but neither FO-specific field, which produces an empty race list.
A1Compat now consumes resolved Race ODF fields through the core's shared
Race-loaded callback without changing those files. If every record referenced
by the active `races.odf` has a distinct in-range `instantActionSlot`, native FO
slot and `displayKey` behaviour is retained unchanged. If any record lacks a
valid slot, A1Compat assigns contiguous runtime slots to records which define
`interfaceConfiguration`, in exact `race0` through
`race<numberOfRaces - 1>` declaration order. All other Race records remain
loaded but are omitted from the playable dropdown. Missing playable
`displayKey` values are supplied at runtime from the resolved `displayName`, so
inherited fields and child-mod overrides keep normal ParameterDB precedence.

The same completed-Race boundary supplies the starting-resource commands which
stock A1 faction ODFs omit. A1Compat resolves the effective
`SHOWMETHEMONEY_CREW`, `SHOWMETHEMONEY_DILITHIUM`,
`SHOWMETHEMONEY_METAL`, `SHOWMETHEMONEY_TRITANIUM`, and
`SHOWMETHEMONEY_SUPPLIES` values from the active `RTS_CFG.h` chain. Missing
`normal*` fields receive those values and missing `lots*` fields receive 1.5
times normal. Native Crew/Dilithium/Metal values are written to the Race
starting-resource matrix; Tritanium/Supply remain independent A2FOResources
pools and are visible to that module through the shared snapshot. No default
is invented for resources absent from `showmethemoney`, and explicit or
inherited Race values always win. This is runtime policy: the A1 ODFs remain
unchanged.

Raw A1 `teamcolor.odf` files use thirteen named values (`white` through
`black`) rather than Fleet Operations' `mpcolor01` through `mpcolor16` keys.
Fleet Operations' rewritten `TeamColor_Init` table therefore does not populate
the player-colour entries used by the Instant Action setup screen and minimap.
Those consumers are patched to the relocated `FOTeamColor` array in
`FleetOpsHook.dll`, not Armada's original static palette. While the A1 marker
is active, A1Compat merges the winning table from each
extension root and translates the legacy names into player slots 1 through 13
at runtime. Normal root precedence is preserved; explicit `mpcolorXX` entries
win over legacy aliases in the same layer, and parent values can supply the
three player slots that A1 did not define. The palette is reapplied after every
native team-colour initialization and writes the live relocated array, without
changing an A1 file on disk. This
does not force model team-colour tinting or alter its graphics checkbox.

A1 physics files use `impulseSpeed` as their turning tier and `warpSpeed` as
ordinary aligned cruise; Borg movement additionally uses the legacy
`borgPhysics` boolean. A2 instead requires `combatSpeed`, uses `impulseSpeed`
for ordinary cruise, and reserves `warpSpeed` for strategic warp. At the exact
PhysicsClass lookup calls, A1Compat first preserves normal ParameterDB
resolution. When `combatSpeed` is absent, the translated combat tier is the
larger of legacy impulse and half legacy cruise, A2 impulse receives the legacy
cruise value, and A2 strategic warp is disabled. A missing `physics` receives
`borg` for nonzero `borgPhysics` and `smooth` otherwise. The ordinary path
supplies the matching A2 Classic smooth-controller profile per shared physics
file (`cnstphys`, `destphys`, or `battphys`), with the Federation destroyer
profile as the default for an unknown ordinary family. Explicit smooth fields
still win. A declared or inherited `combatSpeed` identifies an A2/FO contract
and leaves every native speed value authoritative. The legacy ODFs remain
unchanged.

The race ODFs retain the shared `gui_fed.cfg`, `gui_bor.cfg`, `gui_kli.cfg`,
and `gui_rom.cfg` names. STA1 Classic uses the stock Armada 2 versions because
the Armada 1 interface layouts are incompatible with the A2/FO executable.
These include the parent `gui_interface.cfg`. Without them,
`commBarNumberOfPlayers` remains zero and Armada crashes at `0x00513B05` while
dereferencing the absent communications-player array. Original A1 interface
layouts must not be restored over these A2 compatibility files.

## Local validation inputs

These are local evidence sources, not repository assets:

```text
Vanilla Armada 1:
/home/tamsynn/Games/Heroic/Star Trek Armada/

Extracted stock A1 ODF archive:
/home/tamsynn/Downloads/armada_odf_files.zip

Millennium Project 1.9se:
/home/tamsynn/Downloads/millenium_project19se/Star Trek - Armada/
```

Copyrighted ODFs, models, textures, sounds, maps, interface files, and other
game data must not be copied into this source repository. Synthetic fixtures
should cover automated compatibility tests.

The five classic nebula models, `Mnebula1.sod` through `Mnebula5.sod`, are
byte-identical in stock A1 and A2 and need no conversion. They remain local
installed assets rather than repository files.

The sprite master registry is also an engine-data contract. A1's
`Sprites/sprites.spr` must retain its classic includes while also including
FO's `fleetops.spr` and `fleetops_comp.spr`. Without those two parent tables,
the SOD nodes used by inherited FO map-nebula objects are unavailable. This
was a real compatibility gap, but restoring the includes did not alter the
separate `NebulaClass::s_SetTexturesRecursive` fault at `0x0049DD62`.

Several A1/A2 `SOD/Software` files are tiny sprite stubs sharing basenames with
their hardware SODs. They are currently kept in a reversible disabled-assets
directory outside `sod/`, but disabling them did not alter the observed
`0x0049DD62` fault and they are not its root cause.

The exception stack identifies the affected inherited class as
`map_nebula_crystalid.odf`. Its SOD uses `ncyrstA` through `ncyrstD`, all
defined in FO's `fleetops.spr`. A runtime test therefore places the exact
parent `fleetops.spr` and `fleetops_comp.spr` beside the child master registry
to determine whether sprite-table includes can traverse parent roots.

The native fault is inside
`NebulaClass::s_SetTexturesRecursive(ST3D_Node*)`: Fleet Operations places the
function at Armada RVA `0x0009dd40`, and RVA `0x0009dd62` reads offset `0x2c`
through the sprite node's null type-specific-data pointer. `A1Compat.dll`
hooks the signature-checked seven-byte prologue and checks only nodes whose
virtual type is `3` (`ST3D_SpriteNode`). Invalid nodes are logged with their
node and parent names and skipped; every valid node passes through the native
gateway unchanged. This guard is deliberately parent-scoped.

The guard identified 81 missing A2-era nodes, including the `fluid*`,
`tachyon*`, and `big*` nebula families. A1's `nebula.spr` is a strict subset of
the STA2 table but overrides the recursive parent file by basename. The local
compatibility parent therefore uses STA2 Classic's superset `nebula.spr`, which
retains every A1 definition and adds the nodes required by the shared
hardware SODs and A2/FO map objects. The original A1 table remains a local
backup outside the active mod tree.

After bypassing that fault, Armada reached `RtimeClass::Load(FileReader&)` and
failed at RVA `0x0013c334` because the A2 loader attempted to read a runtime
class where the A1 stream had no matching section. Temporary instrumentation
identified stream misalignment rather than a missing registered class and was
removed after the checked runtime-class-width, object-tail, load-order, and
mission bridges replaced it.

Direct Armada 1 BZN loading now has its first schema bridges. Fleet Operations'
map-open path still contains the original `saveGameDesc`, `binarySave`, and
pre-2067 branches and correctly preserves local A1 versions 2050–2053.
Observed A1 maps declare 40-byte runtime-class records like A2. The
fixed-character input bridge at Armada RVA `0x0013c2c3` follows the live
labelled field's declared width for a completely validated A1 reader and
supports a 32-byte legacy record only when that field explicitly declares it.

The same validated A1 header contains six labelled floats in min/max X, Y, Z
order. A2 expects their equivalent in `MapDetails::MPDMinExtent` and
`MapDetails::MPDSize`; without that translation, Instant Action displays `0x0`
even though the A1 map has real bounds. The checked `KnownMaps` call at Armada
RVAs `0x00147a59` and `0x001b7da9` now preserve native
`MapDetailsFactory::Load` for both selected-map launch and map discovery. Bare,
virtual `bzn\\...`, and already-qualified filenames resolve through the active
extension-root chain. The bridge then fills those two fields for A1 maps only.
The object-loader bridge republishes the corresponding min/max values to
Armada's live world-bound globals before its native position conversion begins.
Applying them at the later runtime-class/mission boundary leaves the displayed
map size correct but transforms all previously constructed objects against
A2's old bounds. X/Z remain the source map's values, while Y is widened only as
needed to include A2's native `-1250..1250` scanner envelope. This keeps A1
moons at Y `-50` and wormholes at Y `-30` inside the gameplay visibility
volume without changing their serialized positions. Original BZN bytes remain
untouched.

Armada 1 stores multiplayer player positions beside the map in `<map>.mdf` as
`StartLocations` plus `StartN = X Y`. A2 expects world-space positions in its
native `StartLocationDetails` array instead. The same checked MapDetails bridge
now validates A1's `0..117` minimap grid, converts X directly and inverted Y to
world X/Z, uses the map's vertical midpoint for Y, and marks unused A2 slots
empty. This restores the opposing player slots needed by normal Instant Action
setup and victory checks without editing either source file.

Fleet Operations' Instant Action AI requests
`<race>_instant_action_build_list` and numbered variants, whereas A1 races use
`<race>_build_list`. A1Compat intercepts the checked
`AIP_Manager::Look_Up_New_AIP` entry at Armada RVA `0x00025a50`. When the
highest relevant A1 mod layer contains only the legacy filename, that loaded
plan is returned instead of a same-named Fleet Ops plan inherited from Data.
Explicit modern plans at equal or higher priority retain normal lookup.

The runtime-class width bridge patches only the fixed-character CALL at Armada
RVA `0x0013c2c3`.
The replacement validates the complete A1 front matter in the live reader and
requires its parsed version to match `FileReader+0x08`; numeric version alone
cannot activate it. A validated A1 stream uses the field's declared 40-byte
width, or receives a zero-padded 40-byte native buffer only when the field
explicitly declares the shorter 32-byte legacy width. A2/FO maps retain the
requested contract.

The `2blue.bzn` trace showed `AiMission::LoadMission` receiving an `mnebula5`
record rather than a mission. Comparing `LoadGame_MainLoad` with the original
Armada 1 executable identified one difference: A2 inserts a craft-class table
loader after the shared Teams loader, while A1 has no equivalent stage.
A1Compat replaces the checked A2 table call at RVA `0x002025c0`; validated A1
readers return success without consuming bytes, while A2/FO readers execute the
native loader at RVA `0x000767a0`.

Live testing proved that bypass is necessary but insufficient. A2's primary
object count stops at `mnebula5219`, before a trailing A1 neutral block of 20
complete objects: ten nebulae, two infinite moons, four dilithium moons, and
four wormholes. For a validated A1 reader, the object wrapper requires one
unique later `EmptyMission`, counts only structurally complete object prefixes,
and passes the tail through A2's native object loader with a temporary
in-memory count record. It restores the displaced bytes immediately, so the
source BZN remains unchanged. The native Teams loader then begins at the real
Teams data rather than consuming part of the first omitted nebula.

The checked mission call at RVA `0x00202608` retains its forward-only,
unique-marker resynchronization as a guarded fallback. Native A2/FO streams are
neither scanned, replayed, nor moved. The A1 header's source-map filename
remains diagnostic only until campaign-aware mission translation exists, and
no original BZN is rewritten.

The moon-resource bridge runs during `GameObjectClass::BuildClass`, before the
core's completed-class field cache is guaranteed to contain that ParameterDB.
It therefore prefers a cached original classlabel when available but falls
back to the live validated `ParameterDB::GetString` entry on an early cache
miss. A cache miss alone must not suppress the missing-only A2 resource
default.

After A1Compat supplies the missing A2 moon-resource command at runtime, map
loading advances beyond that serialization fault and reaches
`StandardText::InitializeConfiguration`.
Fleet Operations crashes at Armada RVA `0x0010ad39` when an ST3D sprite lookup
returns null and native code immediately reads the sprite's field at offset
`0x50`. Temporary instrumentation identified the missing generated sprite and
was removed after the upstream CFG/SPR bridge was implemented.

The first report names `buttonBackgroundPanel.0`. The active child
`gui_interface.cfg` is byte-identical to STA2 Classic and requests both
`buttonBackgroundPanel.0` and `.1`, but the child's overriding
`gui_global.spr` omits them. A data-only include in the compatibility parent's
table cannot solve this generally because basename override selects the child
table first.

`A1Compat.dll` therefore replaces the checked `ReadTable` CALL at Armada RVA
`0x0011a776`, inside `DisplayInterface::PostLoadAll`. With the newly constructed
interface sprite database and native `ST3D_SpriteTableParser` still live, the
bridge reads `a2_gui_global.spr` first and the active mod's winning
`gui_global.spr` second. The active table remains authoritative for duplicate
names, while required A2/FO records survive a child override. The loader checks
`buttonBackgroundPanel.0` immediately after the essential table and again
after the active table, logging both native results before any interface
components initialize. This affects only the startup GUI database; it does not
load the file through the separate world `sprites.spr` registry.

The matching first gameplay-UI bridge hooks the stable post-construction
publication instruction at Armada RVA `0x0011a80f`. The preceding constructor
CALL can already be redirected by Fleet Operations before deferred modules
load, so A1Compat deliberately composes after it instead of replacing it. With
the completed database still in EAX, the bridge identifies a raw A1 layout
only when both `speedPanelArea` and `controlPanelArea` exist while
`screenWidth` and `screenHeight` are both absent. Before the component PostLoad
loop begins, A1Compat changes the active database's integer reference
dimensions at `+0x2c/+0x30` from A2's default 1600x1200 to A1's 640x480.
`ParameterDB::Get(DBRectangle)` then performs the native scaling for every
legacy rectangle A2 consumes. Explicit modern screen dimensions and non-A1
layouts are never changed. Each construction logs the CFG, four-key evidence,
original dimensions, selected policy, and number of legacy command-button
rectangles captured.

A1's `ControlPanel` is the ancestor of A2/Fleet Operations' `PopupPalette`,
not the A2 top-bar `ButtonPanel`. Fleet Operations compacts active command
modes into a 64-control backing array and lays those controls out immediately
before `ControlButton::Render`; HybridBuild also deliberately owns that popup
update path. A1Compat therefore leaves both systems in place and hooks only
the final Armada `ControlButton::Render` boundary at RVA `0x000e64e0`. For a
detected raw-A1 layout, the first twelve compacted popup controls receive the
native-scaled, panel-relative `controlButton1` through `controlButton12`
areas captured from the selected race CFG. `ParameterDB::GetRectangle` retains
the CFG's `x, y, width, height` semantics, so A1Compat converts each area to the
inclusive `left, top, right, bottom` fields stored by `ControlButton`. This
preserves non-grid A1
layouts such as the Romulan and Borg panels, while command identity, page
binding, input handling, and build/research behavior remain native. Controls
outside the twelve legacy slots are not mutated by this first adapter slice.

A1 has one Status Report/ship-display panel, whereas A2's `ShipDisplay`
requests separate low, middle, and tall rectangles and background panels.
For the exact live legacy gameplay database, A1Compat aliases
`infoPanelArea_0..2` to `infoPanelArea`, `infoBlackArea_0..2` to
`infoBlackArea`, and the A2 low/middle background names to A1's
`infoBackgroundPanelArea`/`infoBackgroundPanel`. Rectangle aliases run at the
supported ArmadaL `DisplayInterface::LoadRectangle` entry (RVA `0x0011b430`);
the two background-name aliases run at `ParameterDB::GetString` (RVA
`0x00135350`). Installation of the string boundary occurs after every other
A1Compat feature has preflighted that shared native entry. Nonlegacy databases
and unrelated keys remain native.

The target is faithful A1 gameplay UI presentation and behavior. A1Compat
does not automatically graft A2-only panels or controls into legacy layouts;
mods that want those features can ship an A2-compatible interface.

With the missing panel sprites restored, game opening advances into
`CraftEnhancement.Craft_mLevelUp` and faults at Fleet Ops RVA `0x001dbdcb`.
The function follows `Craft+0xf0` to its `Side`, then `Side+0x244` to its
`Race`, and reads the RaceEnhancement `canGainXP` flag at `Race+0x634`. The
observed read of address `0x00000634` proves that this craft's Side has a null
Race pointer; it does not indicate a missing RaceEnhancement sidecar. The
checked hook records the craft ODF, handle/team, Side and Race pointers, class
and craft-enhancement pointers, force flag, and caller Fleet Ops RVA.

The first report identifies `zferscav.odf` on neutral team 0 during
`Craft_Init_Callback` (`force=0`). Stock A2 registers `norace.odf` specifically
for uninhabited objects, but A1's overriding ten-entry `races.odf` does not
contain it. A1Compat therefore patches only `Race::InitAll`'s checked
`numberOfRaces` and `raceN` lookups. When the declared list has no neutral Race
and the next index is unoccupied, the module appends inherited `norace.odf` to
the runtime registry as entry 10. Appending rather than inserting preserves
all ten original A1 race indices. `norace` lacks an interface configuration
and remains outside the playable Instant Action slots. Existing declarations
remain authoritative and the A1 `races.odf` is never edited.

The earlier narrow `canGainXP = false` fallback passed the former Fleet Ops
`Craft_mLevelUp` fault, then exposed a second null-Race read of
`crewAccumulationRate` at Armada RVA `0x000b5259`. That second report confirms
that a complete neutral Race object, rather than per-field null guards, is the
correct compatibility contract. The eleven-entry runtime registry supplies
all inherited `norace.odf` fields to every native consumer.

## Officer-quarter starbase geometry

The stock A1 Federation, Borg, Klingon, and Romulan starbase SODs each contain
`oq1` through `oq6`. These are the visual officer-quarter upgrades. Fleet
Operations retains the model nodes but Armada 2 removed the A1 starbase officer
upgrade system, so all six branches render unless compatibility code prepares
their visibility before geometry cloning.

`A1Compat.dll` now hooks Fleet Operations' `Starbase::InitializeGeometry` at
Armada RVA `0x000bda00`. It walks the class model hierarchy, recognises only
names which are exactly case-insensitive `oq` followed by a positive decimal
number, and sets node flag bit 0 for every index above the completed-upgrade
count. It does not require A1's Federation-specific `base_fed` parent and does
nothing to models without numbered `oqN` nodes.

The gameplay bridge now parses `maximumUpgrades` and base `officerGain` from
each A1 Starbase ODF. FeaturePack dispatches shared admission and lifecycle
events through the native API when loaded. For A1-only chains where FeaturePack
is absent, A1Compat also guards Fleet Operations' writable native Producer
queue target cell before insertion. A1Compat recognises the retained
`OfficerUpgradeClass`, applies its `officerGain`, increments per-starbase state,
reveals the matching next `oqN`, and rejects admissions once completed plus
queued upgrades reaches the declared maximum.

A2 retained `OfficerUpgradeClass::Build()` as a null-returning stub, while its
ordinary completion path assumes a renderable `GameObject`. The first bridge
claimed FeaturePack's revision-8 `FINISHING` event around the Fleet Ops
Producer callback. That safely skipped `Producer::FinishBuild`, but it was one
level too late: A2's outer `Starbase::FinishBuild` at Armada RVA `0x000bbd90`
still received null, passed it into its built-object post-processing, and the
null later reached `OutputQueueManager`. The observed completion fault was
Armada `0x0053757b`, reading address `0x00000044` from that null object.

The original A1 implementation gives the required boundary. Its
`Starbase::FinishBuild` at preferred VA `0x0045f570` detects
`OfficerUpgradeClass`, applies the Team/count changes, and calls only the
Producer cleanup branch. A1Compat now hooks the corresponding A2 Starbase
method at RVA `0x000bbd90`. Matching upgrades decrement Fleet Ops' active
technology count, call Armada's native queue-pop helper at RVA `0x000b79b0`,
clear construction state/effects, apply officer and `oqN` state, and return to
`Producer::UpdateBuild`, which ignores the result. Ordinary Starbase builds
chain through the native gateway. No Fleet Ops Producer hook is duplicated.

A second live test exposed the earlier failure boundary. Armada's
`Producer::mStartConstructionEffect` at RVA `0x000b8140` passes
`currentBuildClass` directly to its Craft construction-effect creator. An
`OfficerUpgradeClass` is a small policy class rather than a `CraftClass`, so
the cosmetic instance eventually reached `CraftInstance::RenderInternal` and
read beyond it at class offset `+0x408`; the observed fault was again
`0x004cb151`, before any `FINISHING` event. Native API revision 9 therefore
adds a claimable `STARTING_EFFECT` event at the already shared construction-
effect hook. A1Compat suppresses that cosmetic effect only for the matching
A1 Starbase officer upgrade. If that optional emitter is not loaded, A1Compat
also guards Starbase's live derived effect slot at vtable offset `+0x16c`
(Armada RVA `0x000bbe90`); its ordinary path chains the override and any later
generic Producer detour. The build timer, resource charging, queue state, and
A1-scoped Starbase completion remain active.

Starbase policy/menu registration is deliberately independent of the officer
target and completion bridge. If either optional officer boundary fails its
signature check, ordinary construction and the restored A1 `builder_ship`
menu remain active; officer buttons are withheld and the exact unavailable
boundary is logged.

A1 selected the visible command through each race ODF's `officerUpgradeODF`.
A2/FO no longer consumes that command. Investigation showed all four retained
slots as visible even when `race` is declared in `fedoff`,
`klingoff`, `romoff`, and `borgoff`. A1Compat now records those target race
identities during `OfficerUpgradeClass::BuildClass` at Armada RVA
`0x000ce910`, records the Starbase race,
and filters mismatches both when buttons are bound and when synchronized queue
orders are admitted. Reading the original race ODF's `officerUpgradeODF`
directly remains necessary before untouched third-party A1 upgrade ODFs can
omit the temporary `race` declaration. The A2-compatible `gui_global.spr`
does restore
`b_fedoff`, `b_klingoff`, `b_romoff`, and `b_borgoff`, mapped to the original
`gbfoffq`, `gbkoffq`, `gbroffq`, and `gbboffq` textures.

Starbase ownership hooks credit and remove base plus completed-quarter capacity
on creation, capture, removal, and destruction. Capture follows A1 by resetting
the captured base to zero completed quarters. A versioned 16-byte sidecar save
record stores the count and cumulative upgrade gain before native Starbase
state; loading restores the visual/lifecycle state while the native Team save
retains the aggregate officer maximum.

Inspection confirmed that Fleet Operations retains all stock A1
starbase `buildItem` entries after parsing. Moving A1's `techlvl.odf` from its
recursive `odf/other` location to the conventional `odf/system` namespace
produced no palette change, proving that the recursive copy was already being
resolved and was not the missing-command cause. A checked filter at Fleet Ops'
`ProducerClass::mBuildButtonIsVisible` boundary (FleetOpsHook RVA
`0x0011d8f8`) preserves the native result and hides only an officer upgrade
whose recorded race does not match the owning A1 starbase.

Every stock A1 Starbase item was otherwise natively visible. The
actual loss occurred one level above it: Armada II introduced
`builder_ship = 1` for context-sensitive menus, while A1 data predates the
command. Fleet Ops represents it as capability bit `0x80` at
`GameObjectClass+0x1d4` and will not create the outer Build command without it.
`A1Compat.dll` now adds that bit after parsing only for A1-policy Starbases
which have at least one real Producer item. This avoids requiring inherited A1
mods to modify each Starbase ODF.

This compatibility bit must stay scoped to the Starbase class-policy path. A
temporary global `GameObjectClass::Construct` bridge applied `builder_ship` to
unrelated Producer subclasses and then reached an unverified
`Producer::ConstructButtonList` action-table layout, destabilising menu
construction. A1Compat therefore does not hook either generic boundary for this
fix; constructor `builder_facility` support remains a separate compatibility
task until its native menu contract is verified.

A1's Team fields represented available/max officers; A2/FO represents
enlisted/max. The compatibility code therefore adjusts only FO's native maximum
at Team offset `+0x164`. Adjusting enlisted officers would incorrectly consume
the newly granted capacity. Restoring the exact A1 available/max display,
starting limit, and resource wording remains part of the broader officer/UI
pass.

The module logs each detected preparation as `Prepared A1 officer quarters`,
including the ODF name, number of matched nodes, highest index, visible count,
and number of flags changed. It also logs registered Starbase policies,
completed upgrades, and rejected over-limit orders. Reports are capped to avoid
unbounded map logs.

FleetOpsHook's music shuffle has an additional data precondition which stock
A1 does not expose by itself. `TMusicPlayer.randomizeTrackOrder` at Fleet Ops
RVA `0x001d4454` mishandles a playlist containing exactly one valid file: its
call at RVA `0x001d448f` asks `TStringList.Exchange` to swap entries `0` and
`1`, raising `EStringListError` because entry `1` does not exist. Each stock A1
race declares its `300`, `115`, and `100` WAV tracks. STA2 Classic provides
only the corresponding `300` files, so using it as a temporary parent without
installing A1 music creates precisely that one-track state. The local parent
must include the original A1 `sounds/music` assets (or otherwise provide zero
or at least two valid entries); the proprietary WAV files remain outside this
repository.

## Next playable milestone

The next milestone is one Federation Instant Action smoke path using local A1
assets:

1. Add the minimum race definition, GUI references, starting units, ODF tree,
   tech tree, models, textures, sprites, sounds, and AI data to the installed
   local parent.
2. Convert one simple stock A1 skirmish map without altering the source map.
3. Launch the Federation, construct and move a ship, fire a weapon, collect a
   resource, and save/load.
4. Record every failure in the compatibility matrix before adding another race
   or copying a broad asset category.
5. Re-run clean FO4 and STA2 regression checks after every engine-level change.

Campaign missions remain out of the first playable milestone.
