# A1Compat native module

`A1Compat.dll` owns Armada 1-specific compatibility policy for the
`STA1 Classic` parent mod. The DLL is installed centrally in `Data/modules`
and selected as a required module by that parent's `info.ini`. The parent and
its children activate the policy through the separate marker described below;
ordinary mods cannot inherit it accidentally.

Initialization also requires `a1compat.ini` in one of the active extension
roots. Selecting the DLL for an unrelated mod therefore rejects initialization
rather than enabling A1 policy. The marker is owned by the parent and is
inherited automatically by its child addons.

The marker may also contain one compatibility-isolation setting:

```ini
[A1Compat]
SafeMode = 0
```

`SafeMode` defaults to false and accepts `1/true/yes/on/enabled` or
`0/false/no/off/disabled`, case-insensitively. When enabled, the module keeps
the `wingman` alias, missing-only class defaults, `Addon` overlay, essential
GUI sprite-table loader, neutral-Race registry default, and legacy race-menu
fallback, including its starting-resource defaults, but does not install its
riskier executable compatibility, native BZN schema bridge, or officer-quarter
runtime hooks. This is intended for isolating a
conversion-specific startup failure, not for ordinary play.

The initial implementation registers the Armada 1 classlabel alias:

```ini
classLabel = "wingman"
```

as Armada's compatible native `craft` class. The checked ParameterDB hook and
transactional registration remain owned by `A2FOExtensions.dll`; this module
owns the declarative A1 policy at that shared site.

The same missing-only class-default mechanism supplies `research = 1` and
`transporter = 1` to raw A1 `research` stations. Fleet Operations therefore
creates their outer Research command and exposes the existing pod list without
rewriting `fresear.odf`, `fresear2.odf`, or any converted-mod ODF.

The retained A1 `scout = 1` command controls AI/default behaviour and does not
create A2's Scout/Search button. A shared completed-class callback detects that
legacy marker and adds only the missing A2 ScoutBase menu capabilities:
`combat`, `alert`, `can_sandd`, and `can_explore`. Explicit and inherited
values, including zero, are never replaced.

A1 also names its shared scout ship ODF `scout.odf`, which can shadow A2's
Explore command ODF of the same basename. A checked CommandInfo bridge repairs
only the collision case where normal parsing leaves `buttonName` empty. A real
command definition remains authoritative, and neither A1 ODF is rewritten.

The same callback detects A1's inherited `is_starbase = 1` marker and supplies
only missing `facility`, `has_crew`, and `has_hitpoints` capability bits. This
restores A2/Fleet Operations' Recrew command on raw A1 stations while preserving
any explicit or inherited converted-mod choice, including zero.

Raw A1 ODFs remain unchanged. Where Armada 2 added constructor state that an
inherited A2 map serializes, A1Compat supplies the corresponding A2 Classic
command only after the normal ODF/include lookup reports it missing. The first
such bridge covers the exact A2 Classic moon families: `mdmoon` plus numbered
variants receives `resource = "ResourceMoon"`, while `mmooninf` plus numbered
variants receives `resource = "ResourceMoonInf"`. Project-ID identity is used
because this lookup occurs before the completed-class/class-label cache is
available. The exact ODF-family check excludes ordinary asteroids and comets.
The same exact families receive missing-only `spatial_object` and
`has_resource` capability bits so their gameplay objects are visible and
targetable by freighters. Explicit or inherited values always win.

The module also declares `Addon` as an override-priority ODF directory. The
FeaturePack's shared recursive filesystem index consumes that declaration, so
A1 ODFs remain in their original folder and an `Addon` file wins over a
same-basename structured ODF in the same mod root. Child/parent mod precedence
still wins before this within-root rule.

Legacy A1 configuration files may name relative engine directories with a
leading `.\\`, for example `AI_DIRECTORY = ".\\AI"`. Fleet Operations keeps
that prefix when it resolves `AI\\formations`, but its virtual filesystem
registers only the canonical name without `.\\`. While A1Compat is active it
normalizes these lookup names at `TFOFS.getVirtualDirectory`, preserving the
mod's original configuration and preventing the native `Search Directory not
found` assertion.

Before Race loading, two checked calls in `Race::InitAll` supply the missing A2
neutral-Race registry contract. If the declared A1 race list contains no
`norace` entry and the next index is unoccupied, A1Compat increases the runtime
count by one and resolves that final entry as the inherited `norace.odf`.
Original A1 indices remain unchanged and `norace` has no playable menu slot.
An existing declared `norace` remains authoritative, and no ODF is rewritten.
Both lookups chain the live ParameterDB entries, including the checked core
A2FOExtensions detours installed before native modules initialize.

The shared Race-loaded callback supplies a runtime-only Instant Action fallback
for unmodified A1 faction ODFs. If every Race referenced by `races.odf` has a
distinct in-range `instantActionSlot`, A1Compat leaves the complete native FO
slot and `displayKey` policy unchanged. If any referenced record has a missing,
negative, duplicate, or out-of-range slot, the fallback walks the loaded Race
records in `race0` through `race<numberOfRaces - 1>` order. Records which define
`interfaceConfiguration`, including through inheritance, receive contiguous
menu slots; other records stay loaded but receive no playable slot. A missing
`displayKey` on a playable record is supplied from its resolved `displayName`.
No ODF is rewritten, and the policy remains confined to mod chains carrying
the `a1compat.ini` marker.

Stock A1 faction ODFs also predate A2/FO's `normal*` and `lots*` starting-
resource commands. A1Compat reads the effective `SHOWMETHEMONEY_*` values from
each active `RTS_CFG.h` in extension-root order and registers them as
missing-only Race defaults. Crew, Dilithium, Metal, Tritanium, and Supply use
those values for the normal setup; each corresponding lots value is 1.5 times
normal, rounded to the nearest whole resource. The current Roots configuration
therefore produces 20,000 normal and 30,000 lots for all five. Crew,
Dilithium, and Metal are written into Armada's native Race matrix;
`A2FOResources`, when selected, receives Tritanium and Supply through the same
shared Race snapshot. Latinum, Officers, Biomatter, Credits, and Collective
Connections are not invented because `showmethemoney` defines no matching
grant. Explicit or inherited Race ODF commands remain authoritative, and no
A1 faction ODF is changed on disk.

Armada 1's `teamcolor.odf` names its thirteen player colours `white`, `red`,
`blue`, `green`, `yellow`, `purple`, `cyan`, `brown`, `orange`, `pink`,
`magenta`, `gray`, and `black`. Fleet Operations instead initializes the
Instant Action/minimap palette from `mpcolor01` through `mpcolor16`, so an
unchanged A1 table otherwise leaves those selectors black. Fleet Operations
relocates the live table to `FOTeamColor` in `FleetOpsHook.dll`; Armada's
original array is no longer read by the patched IA/minimap paths. A1Compat
merges the effective `teamcolor.odf` layers in extension-root order and
translates the A1 names to the corresponding first thirteen live slots. Parent `mpcolor14`
through `mpcolor16` values remain available, a child layer still overrides its
parents, and an explicit `mpcolorXX` wins over the matching legacy name when
both occur in one file. The resolved palette is applied immediately and after
each native `TeamColor_Init`, covering later graphics/system resets. Armada's
original array is maintained only as a compatibility mirror. This is
runtime-only compatibility; no `teamcolor.odf` is rewritten, and the separate
model `Team Colors` graphics option is unaffected.

Raw A1 physics ODFs likewise predate A2's separate `combatSpeed` and `physics`
commands. A2 rejects a newly parsed physics record when `combatSpeed` remains
zero, leaving otherwise valid A1 ships stationary. The two engines also assign
different meanings to the old speed names: A1 `impulseSpeed` is its turning
tier and A1 `warpSpeed` is ordinary aligned cruise, while A2 uses
`impulseSpeed` for cruise and reserves `warpSpeed` for strategic warp. When
`combatSpeed` is missing, A1Compat translates those three speed slots at the
PhysicsClass boundary: combat speed is the larger of the legacy impulse tier
and half the legacy cruise tier, A2 impulse receives the legacy cruise speed,
and A2 strategic warp is disabled. A missing `physics` selector becomes `borg`
when the legacy `borgPhysics` flag is nonzero and `smooth` otherwise. Ordinary
A1 records then receive the A2 Classic smooth-controller fields associated
with their shared physics-file family: `cnstphys` uses the construction
profile, `battphys` the battle profile, and `destphys` (plus unknown ordinary
families) the Federation destroyer profile. Partial modern controller values
remain authoritative. A declared or inherited `combatSpeed` identifies an
A2/FO record and bypasses the translation entirely. Defaults are applied to
the parsed shared PhysicsClass, not individual craft instances, and no ship or
physics ODF is changed on disk.

The module makes `a2_gui_global.spr` a second required startup GUI table. A
signature-checked CALL replacement in `DisplayInterface::PostLoadAll` reads it
into the newly constructed interface sprite database before reading the active
mod chain's winning `gui_global.spr`. Loading the active table second preserves
child-mod sprite overrides while preventing a child `gui_global.spr` from
hiding A2/FO interface records required by inherited CFG files. The loader
checks `buttonBackgroundPanel.0` after both stages and records both native
`ReadTable` results in `A2FOExtensions.log`. This is the GUI database, not the
later world-sprite database loaded from `sprites.spr`.

An existing `@include a2_gui_global.spr` remains compatible during migration,
but is redundant once A1Compat owns the essential load. New A1 compatibility
parents should keep the file in their inherited `Sprites` chain and let the
module load it directly.

Raw Armada 1 gameplay CFGs are also recognized at runtime. After the native
gameplay `ParameterDB` constructor has loaded the selected Race CFG, A1Compat
checks for both `speedPanelArea` and `controlPanelArea`. When those legacy
panels are present and neither A2 `screenWidth` nor `screenHeight` is declared,
the database's reference size is changed from A2's default 1600x1200 to A1's
original 640x480 before any interface component reads its rectangle. This
allows every legacy rectangle that A2 already understands to use the correct
screen scale without editing the CFG. A modern or partially converted CFG
that declares either screen-size key remains authoritative and follows the
native A2 path.

This scaling bridge is the first gameplay-UI compatibility layer, not an A1/A2
hybrid skin. The intended A1Compat policy is to reproduce the A1 gameplay UI's
styling and behavior from its CFG/SPR data. A modder who wants A2-only interface
features can instead provide an A2-compatible gameplay UI.

A2's ShipDisplay asks for separate low, middle, and tall panel/background
names, while A1 supplies one Status Report panel for all selection modes.
For a detected raw-A1 gameplay database, A1Compat maps only those A2-only
names back to A1's `infoPanelArea`, `infoBlackArea`,
`infoBackgroundPanelArea`, and `infoBackgroundPanel` entries. The adapter runs
through the shared ArmadaL loaders, preserves the original CFG/SPR assets, and
does not affect a modern gameplay database.

The module additionally owns a signature-checked inline guard for
`NebulaClass::s_SetTexturesRecursive` at Armada RVA `0x0009dd40`. A legacy SOD
can produce an `ST3D_SpriteNode` whose type-specific-data pointer at offset
`0xc0` is null; native code dereferences it at RVA `0x0009dd62`. A1Compat logs
the node and parent names and skips only that invalid node. Valid nodes execute
through the native gateway unchanged. See `docs/addresses.md` for the complete
address and byte contract.

Armada 2 retains most of Armada 1's BZN front-matter and old-version readers,
including `saveGameDesc`, `binarySave`, and versions 2050 through 2053. Observed
A1 Instant Action maps declare 40-byte runtime-class fields just like A2;
A1Compat validates the complete A1 header and live FileReader version, then
uses the labelled field's own declared size at RVA `0x0013c2c3`. A shorter
32-byte legacy record is supported only when the live field declares it.
A2/FO streams keep their native contract.

A1's header also serializes map bounds as min/max X, Y, and Z floats. The
Instant Action list does not translate them into A2's `MapDetails`, producing a
false `0x0` map size. The checked GameSetup and `KnownMaps` calls at RVAs
`0x00147a59` and `0x001b7da9` now preserve native
`MapDetailsFactory::Load` and fill only `MPDMinExtent` and
`MPDSize` for a validated A1 BZN. The same parsed values refresh Armada's live
world-bound globals immediately before the native A1 object-loading bridge.
The source X/Z bounds are preserved, while Y is widened only as needed to
include A2's native `-1250..1250` scanner envelope. This keeps low A1 moons and
wormholes inside the gameplay visibility volume.
This timing is required because Armada transforms every serialized object
position against those globals as it constructs the object; publishing the
bounds later would preserve the map size but misplace its contents. No BZN is
converted or edited.
Header inspection is capped at 4 KiB: still bounded independently of map size,
but large enough for the final extent records in retail A1 maps such as
`2blue.bzn`, which cross byte 512. Live `FileReader` recognition first validates
the original 512-byte prefix, then uses the extended span only when that whole
backing-window range is readable. Failure to expose the optional extent tail
therefore cannot disable A1 object or neutral-mission loading.

A1 multiplayer positions live in the companion `.mdf`, not in A2's BZN
`StartLocationDetails` records. For the same validated map, A1Compat reads
`StartLocations` and `StartN = X Y`, validates Armada 1's inclusive `0..117`
minimap grid, converts it to world XYZ using the parsed bounds and inverted
minimap Y axis, and fills the native A2 start records. Remaining slots are
marked empty. The MDF and BZN remain unchanged on disk.

An A1 map load can overwrite the live Side relationships A2 derived from the
Instant Action Team selectors. A checked non-bypassing hook at
`GameTypeToTheDeath::CheckAll` (Armada RVA `0x0007dad0`) restores A2's own
setup rule once per selected A1 map: side zero is neutral, slots with the same
`GameSetup::GetTeam` value are allied, and different teams are enemies. This
prevents the first deathmatch check from treating Team 1 and Team 2 as an
already victorious alliance. The same hook executes the displaced prologue
through the native gateway unchanged. Native A2 maps do not activate the
relationship repair.

Fleet Operations asks `AIP_Manager` for race plans named
`<race>_instant_action_build_list` (including numbered variants), while A1
mods conventionally provide `<race>_build_list`. A checked A1-only lookup hook
at Armada RVA `0x00025a50` now prefers the legacy plan when the A1 marker layer
or a child layer supplies it without a modern counterpart. An explicit modern
file at the same or a higher mod-chain priority remains authoritative, and
Data-layer Fleet Ops plans are never substituted for an A1 plan merely because
their filename happens to match.
If an inherited or legacy AIP still contains a unit absent from the active
technology tree, A1Compat skips only that unresolved build-list element and
logs its plan/unit names; Armada's native update would otherwise dereference
the null class pointer at RVA `0x000248a9`.

The runtime-class width bridge replaces only the fixed-character input call at
RVA `0x0013c2c3`.
It first validates the complete in-memory A1 header and confirms that its
parsed version matches the live `FileReader`; a version number by itself is
not sufficient. It reads the live labelled field header and accepts its
declared 40-byte size, or zero-pads the native buffer only for a declared
32-byte legacy record. A2/FO maps, save streams, and unrecognized old files
retain the original requested width.

Static comparison with the original Armada 1 executable shows one concrete
load-order difference. A2 inserts a craft-class table loader between the
shared Teams section and `AiMission::LoadMission`; A1 has no equivalent stage.
The checked call replacement at RVA `0x002025c0` bypasses only that A2-only
loader for a validated A1 reader and otherwise calls its native RVA
`0x000767a0` unchanged.

That bypass exposed a separate object-section mismatch: on `2blue.bzn`, A2's
primary object count leaves a trailing A1 neutral-object block unread. It
contains 20 valid records: ten `mnebula5`, two `mmooninf`, four `mdmoon`, and
four `mwrmhole` objects. A1Compat validates the unique later `EmptyMission`,
counts only complete object prefixes before it, and feeds that block through
the same native A2 object loader using a temporary in-memory count record. The
overwritten bytes are restored immediately and the source BZN is unchanged.
The following native Teams stage then receives the actual Teams section instead
of an `mnebula5` record.

At RVA `0x00202608`, the unique-marker mission resynchronization remains as a
guarded fallback if an unimplemented A1 section still leaves the cursor short;
normally Teams should now finish directly on `EmptyMission`. A2/FO streams are
never scanned, moved, or replayed. The bridges do not substitute the header's
source map filename. Campaign-aware mission translation remains future work.

Temporary runtime-class instrumentation identified stream misalignment at the
first inherited `mdmoon` record: the raw A1 ODF correctly lacks Armada 2's `resource` command,
but the A2 map contains the resulting resource object. The missing-only
Scrap/moon bridge above restores the A2 runtime contract without modifying the
A1 file on disk. Its ODF-name lookup chains the live
`cPrjID::GetOdfName` entry, including Fleet Operations' pre-existing checked
detour; it does not require that runtime entry to retain the untouched Armada
prologue.

Temporary StandardText instrumentation identified the missing GUI sprite which
led to the essential GUI-table bridge described above; that observation hook is
no longer installed. Fleet Ops RVA `0x001dbdcb` retains the actual defensive
Craft level-up bridge: valid Race objects use the native `Race+0x634`
`canGainXP` read, while a null Race receives `canGainXP = false` and continues.
No playable race or on-disk ODF is added.

Officer-quarter compatibility restores the A1 starbase progression on top of
FO's retained A2 classes. Numeric A1 commands are read through the validated
`ParameterDB::GetString` entry and parsed locally. Armada RVA `0x00135200` is
`ParameterDB::GetProjectId`; treating it as `GetInt` corrupts project-ID
registration and is explicitly forbidden. The module does not detour that site.
The module parses `maximumUpgrades` and `officerGain`
after `StarbaseClass::BuildClass`, tracks completion per starbase, and prepares
exact `oq<number>` model nodes in `Starbase::InitializeGeometry`. Models with no
numbered `oq` nodes remain unchanged.

FeaturePack owns Fleet Ops' Producer hooks and emits admission,
claimable pre-completion, post-completion, and destruction events. A1Compat
uses the shared admission/destruction channel when available. It also guards
Fleet Operations' writable native Producer queue target cell directly, so an
A1-only chain still rejects wrong-race orders and enforces the A1 maximum
before insertion. Because A2's retained upgrade `Build()` returns null, the
actual officer completion is intercepted at
Armada's A2 `Starbase::FinishBuild` RVA `0x000bbd90`, matching the boundary used
by A1's removed special case. This prevents A2's outer Starbase routine from
sending a null finished object to `OutputQueueManager`; all ordinary builds
chain through the native gateway. Starbase policy/menu hooks are installed
independently, so an unavailable completion bridge cannot remove ordinary
construction. Starbase SetTeam/ClearTeam hooks add and remove
base/quarter capacity on creation, capture, removal, and destruction.
Versioned sidecar state stores completed count and cumulative gain before the
native Starbase save payload.

API revision 9 also dispatches `STARTING_EFFECT` from HybridBuild's shared
Armada Producer effect hook. A1Compat claims it for matching officer upgrades
when that optional emitter is active. A1Compat additionally guards the live
derived Starbase `mStartConstructionEffect` vtable slot at `+0x16c` directly,
so the same protection remains available when FeaturePack and HybridBuild are
not loaded. `OfficerUpgradeClass` is not a CraftClass and cannot safely back
the cosmetic construction renderer. Only the visual effect is skipped; build
time, costs, queue state, and in-place completion are preserved.

A1's officer counter represented available/max officers, whereas A2/FO uses
enlisted/max. This stage changes only FO's native maximum; adding the A1 gain to
FO's enlisted count would falsely consume the newly granted capacity. The
separate A1 officer/UI compatibility task still needs to restore the original
available/max presentation and officer starting limits.

Failure to install an optional compatibility hook is logged but does not discard
the required declarative registrations. The checked
`a2_gui_global.spr` loader is different: its installation is required because
continuing without the A2 interface table can make native GUI initialization
dereference a missing sprite.

Future A1 compatibility belongs here when it cannot be handled safely by
unchanged data or an offline converter. Shared facilities such as recursive ODF
lookup and optional `Textures/RGB`, `Textures/Index8`, and
`Textures/Compressed` loading remain in their general modules.

Install `A1Compat.dll` centrally, but select it only through the `STA1 Classic`
parent's `[modules]` policy. Ordinary FO4 and STA2 mods remain unaffected when
the marker is absent.

Do not keep backup `.dll` files in `Data/modules`. They become separate
installed module candidates and legacy load-all mods may initialize them.
Store backups outside the game tree or give them a non-DLL extension.
