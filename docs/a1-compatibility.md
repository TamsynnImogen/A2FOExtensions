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

This first milestone establishes the parent metadata and the optional
`A1Compat.dll` boundary. It does not yet claim playable stock A1 Instant Action
or campaign compatibility.

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
Its optional diagnostic setting is:

```ini
[A1Compat]
SafeMode = 0
```

It accepts `1/true/yes/on/enabled` and `0/false/no/off/disabled`.
Safe mode retains the `wingman` alias, missing-only class defaults, and
`Addon` overlay while withholding the riskier executable diagnostics and
officer-quarter runtime hooks.

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

The module requires the versioned native SDK and registers its alias
transactionally. Rejected registration causes initialization to fail without
leaving partial A1 policy behind. It also installs the A1-scoped,
signature-checked nebula sprite-node guard and temporary map/GUI diagnostics
documented below.

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
The local stock parent currently patches its four playable race definitions;
general synthesis for unmodified third-party A1 races remains future A1Compat
work.

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
gateway unchanged. This guard is deliberately parent-scoped while diagnostics
establish why the legacy SOD node did not receive sprite data.

The guard identified 81 missing A2-era nodes, including the `fluid*`,
`tachyon*`, and `big*` nebula families. A1's `nebula.spr` is a strict subset of
the STA2 table but overrides the recursive parent file by basename. The local
compatibility parent therefore uses STA2 Classic's superset `nebula.spr`, which
retains every A1 definition and adds the nodes required by the shared
hardware SODs and A2/FO map objects. The original A1 table remains a local
backup outside the active mod tree.

After bypassing that fault, Armada reached `RtimeClass::Load(FileReader&)` and
failed at RVA `0x0013c334` because a serialized map/save object type had no
registered runtime factory. A temporary mid-function diagnostic at RVA
`0x0013c2da` checks the already-read 40-byte class name against
`RtimeClass::Find` at RVA `0x0013c1a0`, logs a missing name, and otherwise
preserves native execution unchanged. Its five-byte signature starts after an
absolute-address instruction, keeping the checked bytes independent of the
image base. Non-textual failures include the exact 40 input bytes and native
caller RVA so serialization misalignment can be distinguished from a missing
registered textual type. The diagnostic also records the `FileReader` binary
and labelled-stream flags, buffer cursor/remaining byte counts, the second
read's result, and bytes surrounding the stream cursor. This identifies the
unexpected serialized record without changing native load behaviour.

After correcting the moon resource declarations, map loading advances beyond
that serialization fault and reaches `StandardText::InitializeConfiguration`.
Fleet Operations crashes at Armada RVA `0x0010ad39` when an ST3D sprite lookup
returns null and native code immediately reads the sprite's field at offset
`0x50`. A temporary non-bypassing diagnostic at RVA `0x0010ad23` records the
live GUI configuration string, generated sprite name, item index, and caller
RVA. It then runs the displaced temporary-string cleanup setup and preserves
the original null result so the upstream CFG/SPR mismatch can be corrected
without disguising it.

The first report names `buttonBackgroundPanel.0`. The active child
`gui_interface.cfg` is byte-identical to STA2 Classic and requests both
`buttonBackgroundPanel.0` and `.1`, but the child's overriding
`gui_global.spr` omits them. The local compatibility parent therefore uses
STA2 Classic's complete `gui_global.spr`; the displaced A1 table is retained
outside the active tree as `gui_global.a1-original.spr`. This follows the
existing rule that FO/A2 GUI structures remain authoritative while A1 game
assets are ported around them.

With the missing panel sprites restored, game opening advances into
`CraftEnhancement.Craft_mLevelUp` and faults at Fleet Ops RVA `0x001dbdcb`.
The function follows `Craft+0xf0` to its `Side`, then `Side+0x244` to its
`Race`, and reads the RaceEnhancement `canGainXP` flag at `Race+0x634`. The
observed read of address `0x00000634` proves that this craft's Side has a null
Race pointer; it does not indicate a missing RaceEnhancement sidecar. A
temporary non-bypassing diagnostic at the failing instruction records the
craft ODF, handle/team, Side and Race pointers, class and craft-enhancement
pointers, force flag, and caller Fleet Ops RVA. It then executes the displaced
read unchanged so the object/data contract can be fixed from evidence rather
than hidden with a broad null bypass.

The first report identifies `zferscav.odf` on neutral team 0 during
`Craft_Init_Callback` (`force=0`). Stock A2 registers `norace.odf` specifically
for uninhabited objects, but A1's overriding ten-entry `races.odf` does not
contain it. The local compatibility registry therefore appends `norace.odf` as
entry 10. Appending rather than inserting preserves all ten original A1 race
indices and the explicit Instant Action slots already added to the four
playable A1 race ODFs.

The corrected eleven-entry registry passes the former Fleet Ops
`Craft_mLevelUp` fault and allows `mp08walr.bzn` to enter live gameplay. The
initial scene renders the A1 units and textures against the inherited map with
the deliberately retained A2-compatible interface, confirming the first full
map-open path through this chain.

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
events through the native API. A1Compat recognises the retained
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
A1 Starbase officer upgrade. The build timer, resource charging, queue state,
and A1-scoped Starbase completion remain active.

Starbase policy/menu registration is deliberately independent of the officer
target and completion bridge. If either optional officer boundary fails its
signature check, ordinary construction and the restored A1 `builder_ship`
menu remain active; officer buttons are withheld and the exact unavailable
boundary is logged.

A1 selected the visible command through each race ODF's `officerUpgradeODF`.
A2/FO no longer consumes that command. Native visibility diagnostics report
all four retained slots as visible even when `race` is declared in `fedoff`,
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

Producer diagnostics confirmed that Fleet Operations retains all stock A1
starbase `buildItem` entries after parsing. Moving A1's `techlvl.odf` from its
recursive `odf/other` location to the conventional `odf/system` namespace
produced no palette change, proving that the recursive copy was already being
resolved and was not the missing-command cause. A bounded diagnostic now wraps
Fleet Ops' `ProducerClass::mBuildButtonIsVisible` at FleetOpsHook RVA
`0x0011d8f8`. For registered A1 starbases it records the team, slot, target
project ID, technology-item state, requirement count, and unmodified native
visibility result.

That diagnostic proved every stock A1 Starbase item was natively visible. The
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
