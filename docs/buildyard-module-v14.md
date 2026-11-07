# BuildYard MODULE v14: selected-tree requirements

**Status: v14 reinstated in the actual game installation; in-game acceptance pending.**
At least one reported pre-menu failure used the incomplete executable copy in
the source workspace. Further launch failure is unresolved; the installed-game
logs still contain no v14 startup. See the deployment receipt for current evidence.

**Headless validation:** v13's direct-object gate and missing-requirement tooltip were accepted
in game on 2026-09-24. v14 passes the portable policy suite and native helper,
bridge and tooltip regression fixtures. **v14 in-game acceptance is pending.**
The [deployment receipt](../BUILDYARD_V14_DEPLOYMENT.md) records the installed
binary and rollback copy. Use the [BuildYard test guide](../BUILDYARD_TECHNOLOGY_ENTRY_TEST.md)
for the established build, backup, and v13 regression procedure; the
[tooltip ABI reference](buildyard-tooltip-abi.md) records the MSVCP60 stream
layout and provider validation retained by v14.

`moduleXTechnologyEntry` (or the compatibility alias
`moduleXPseudoTechnology`) binds a BuildYard slot to an XML `<object MODULE=...>`
label. The label needs no dummy ODF. `moduleXRequiredTechnology`, when present,
remains a separate native gate and must also pass. The existing native tooltip
text is retained; an unavailable MODULE can append a `Requires: ...` line through
the validated native stream path.

v14 evaluates MODULE definitions from the **technology tree Fleet Operations
actually selects and loads for each team**. Two signature-validated call patches
observe the native tree loader and its borrowed XML text. Native loading retains
control of `include`, `parentInclude`, archives, and file selection; the extension
does not choose those files. The first team in a new load invalidates all old
snapshots. Each team's new definitions become available only after its native
loader returns normally; a native exception skips that commit. Loose XML enumeration reserves symbolic labels for configuration
and display only; it cannot enable gameplay gates. Missing, unreadable, malformed,
or unsupported active MODULE definitions fail closed. Duplicate attributes in
ordinary `ODF=` entries do not block MODULE extraction; duplicate attributes
inside a MODULE are unsupported.

## Supported rule shape

This example describes two alternatives: `(shared AND branch A) OR (shared AND
branch B)`. Replace the sample ODF and race names with exact names from the
selected mod; native race lookup is case-sensitive.

```xml
<object MODULE="example_module_gate.odf">
    <requirement logicGroups="1,2">
        <obj>shared_station.odf</obj>
    </requirement>
    <requirement logicGroups="1" race="borg">
        <obj mustNotHave="true">blocked_upgrade.odf</obj>
        <ally race="unitedfederation">
            <obj>ally_station.odf</obj>
            <obj mustNotHave="true">ally_blocker.odf</obj>
        </ally>
    </requirement>
    <requirement logicGroups="2">
        <obj>alternate_station.odf</obj>
        <enemy race="romulanempire">
            <obj>enemy_station.odf</obj>
        </enemy>
    </requirement>
</object>
```

Objects and peer blocks inside one `<requirement>` are AND conditions. Without
`logicGroups`, requirements are OR alternatives. If any requirement has nonempty
`logicGroups`, v14 evaluates only grouped requirements: requirements sharing a
group ID are ANDed, and group IDs are ORed. An ungrouped requirement is ignored
in that mode. A requirement can list comma-separated group IDs to share a
condition between branches. An empty `logicGroups=""` is ungrouped.

Direct `<obj>` checks the owning team's **completed** count; queued objects do
not satisfy it. `<obj mustNotHave="true">` requires count exactly zero.
Unresolved or unreadable counts fail closed, including negated objects. Each
`<ally>` or `<enemy>` block needs one eligible, active, non-owning team from
teams 1–8 that satisfies its race and **all** nested objects. Conditions are
never pooled across peers. Multiple peer blocks are conjunctive. A neutral team
is neither an ally nor an enemy. Race predicates use native, case-sensitive
lookup. `cap`, `capList`, `buttonHideUnavailable=true`, unknown MODULE attributes
or children, and malformed `logicGroups` values are unsupported and leave the
MODULE unavailable. An enemy block containing only a negated object still needs
an eligible enemy: it succeeds if one eligible enemy lacks that object. It does
not require every enemy to lack it.

## Manual acceptance still needed

1. After a reviewed v14 build is installed with a backup, launch a fresh FOtest
   skirmish. Confirm the log reports the selected native tree, its captured XML,
   and the expected MODULE definition for the owning team. Repeat with a tree
   loaded through an include or archive, and with a different selected tree.
2. With one grouped branch incomplete, confirm the module cannot install and
   its tooltip retains native text plus only the missing conditions. Completing
   the shared condition alone must not unlock it. Completing either full group
   must unlock it and remove the `Requires:` line.
3. Exercise a local race mismatch, an eligible ally and enemy, then remove the
   eligible peer. Check that one peer satisfies all scoped objects, neutral and
   inactive teams do not count, and `mustNotHave` changes when the completed
   count crosses zero. Queuing an object alone must not satisfy a positive
   requirement.
4. Switch or reload the selected tree and verify the prior team's MODULE rules
   do not survive. Check another team's tree separately. Repeat the accepted
   v13 prerequisite and tooltip regression from the linked guide.

Useful log lines in `Data/A2FOExtensions.log` include:

```text
MODULE selected-tree observers enabled: FO+1206F9 EAX=tree EDX=Delphi filename ECX=parentDepth; FO+11FC20 native borrowed XML text capture
MODULE selected native tree loading: <name>; previous definitions cleared
MODULE native XML captured: <source> parentDepth=<n> definitions=<n>
BuildYard symbolic MODULE technology support v14 initialized (selected native tree, race/groups/ally/enemy; no dummy ODF required)
```

`MODULE active XML rejected: <source>: <reason>` and `MODULE active XML capture
failed: ...` indicate fail-closed paths. `MODULE native tree reader signature
mismatch; observers not installed` stops initialization before gate hooks. If
only the text call patch succeeds, its bridge remains a native pass-through.
The existing provider continues to log the module index, tooltip key, synthetic
ID and append readback; see the v13 ABI guide for those exact messages.

## Native ABI evidence

[Disassembly excerpts](buildyard-tree-disassembly.txt) supplement the retained
[tooltip ABI](buildyard-tooltip-abi.md). All addresses here are RVAs. Native
helpers and the complete loader/reader instruction ranges are compared against
the supplied binaries, adjusting only operands listed in PE relocation tables.
No helper signature is inferred from an address alone.

| Native operation | Verified ABI and ownership |
|---|---|
| FO `+1206F9` root call to `+11F284` | EAX=tree, EDX=Delphi ANSI filename payload, ECX=parent depth. Filename borrowed. Tree +8 is team. Outer loop runs 0 through 8. Assembly calls original dispatcher without a C++ frame, then commits only on normal return. |
| FO `+11FC20` call to `+1E359C` | EAX=file object; getter calls thiscall vtable slot 0 and returns borrowed NUL-terminated C text in EAX. Reader frame has tree at EBP-8, filename at EBP-1C, depth at EBP-20. Copy text during the call; return the original pointer unchanged. Native owns file/text cleanup. |
| Armada `+96340` | cdecl(team) returns borrowed Team pointer. |
| Armada `+8B100` | cdecl(race name) returns borrowed Race pointer or null; case-sensitive. Team +244 is Race pointer; Race +448 is ID. |
| Armada `+97250` / `+972D0` | thiscall, ECX=owner Team, other team on callee-cleaned stack, AL=ally/enemy. Combine both diplomacy directions: minimum relation is 2 for ally, 0 for enemy; 1 is neutral. |
| FO `+1DF1D8` | EAX=team, AL=active. Used with teams 1–8, excluding owner. |
| FO `+11F0CC` | EAX=team tree, EDX=ODF project ID, ECX=0; signed EAX completed count. Existing v13 count path retained. |

Native peer evaluation at FO `+121404` establishes that all objects belong to one
eligible peer. Native `+11EAEC`, `+1220FC`, and `+122270` establish grouped-mode
OR-of-AND semantics and exclusion of ungrouped requirements in that mode.
The engine retains ownership of Delphi strings, file objects, tree objects and
native buffers. The extension owns only its copied C++ definitions. No borrowed
pointer from XML capture is retained. Tooltip MSVCP60 string cleanup is unchanged.

## Headless validation and limits

```sh
make module-technology-test build/modules/A2FOFeaturePack.dll \
  build/buildyard_tree_runtime_test.exe build/buildyard_tooltip_provider_test.exe

env WINEPREFIX=/home/tamsynn/.cache/a2fo-buildyard-tooltip-native-wine \
  WINEDEBUG=-all WINEDLLOVERRIDES=msvcp60=n \
  wine build/buildyard_tree_runtime_test.exe \
  'Z:\home\tamsynn\A2FOHookExtensions\ArmadaL.exe' \
  'Z:\home\tamsynn\A2FOHookExtensions\FleetOpsHook.dll'

env WINEPREFIX=/home/tamsynn/.cache/a2fo-buildyard-tooltip-native-wine \
  WINEDEBUG=-all WINEDLLOVERRIDES=msvcp60=n \
  wine build/buildyard_tooltip_provider_test.exe \
  'Z:\home\tamsynn\Games\FO3\Data\MSVCP60.DLL' \
  'Z:\home\tamsynn\A2FOHookExtensions\FleetOpsHook.dll'
```

The portable policy suite checks parsing, groups, AND/OR, negation, race and
peer conditions. The unmodified FOtest `tech1.xml` parses to one supported MODULE
definition. The native fixture executes supplied race, relationship and active
helpers against controlled engine data, plus the real file-text getter and both
assembly bridges. It checks team isolation, inactive/neutral exclusions, reload
reset, rules remaining unavailable before commit, and partial-patch pass-through.
Its patch callback checks bytes but does not install live callsite patches; it
also substitutes the expensive native dispatcher for the reload check.
The provider fixture uses the supplied provider and real MSVCP60 stream routines.

These checks do not establish in-game include/archive traversal, diplomacy,
save/load, actual Delphi exception unwinding, or multiplayer acceptance. Gameplay
uses the supplied owning-team index and native counts/relations, with no random
or wall-clock decisions; local-player lookup is used only for tooltip display.
All multiplayer participants must run matching code and mod data. Caps, capList
and button hiding are deferred. No XML/ODF configuration is rewritten by v14.
