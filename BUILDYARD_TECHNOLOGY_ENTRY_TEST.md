# BuildYard MODULE v13 — requirements and completed-object gating

For the v14 selected-tree, grouped and team conditions extension, see
[the v14 guide](docs/buildyard-module-v14.md). This file retains the accepted
v13 baseline and its regression tests.

Phase 1 is accepted: the user confirmed the existing tooltip plus `TEST`, with a
screenshot. v13 replaces TEST with `Requires: Incubation Center` and corrects the
reported ability to install without `bor_incubationZ`. The old `<obj>` evaluator
checked build availability; v13 uses the same completed-object count as native
XML requirements. Queued objects do not satisfy it.

Only `A2FOFeaturePack.dll` changes at runtime. No core, renderer, startup proxy,
FleetOpsHook replacement, dummy ODF or XML/config edit is needed for the current
module0 setup. Do not deploy the old FO3 runtime used by the native test fixture;
the extension uses the game's already-loaded MSVCP60 imports.

## Build and install

```sh
cd /home/tamsynn/A2FOHookExtensions
make build/modules/A2FOFeaturePack.dll
```

With Armada closed, back up `Data/modules/A2FOFeaturePack.dll`, then replace it
with `build/modules/A2FOFeaturePack.dll`. This checkout's live installation is
`/home/tamsynn/NVMeData/Fleet Ops Roots/Data/modules/A2FOFeaturePack.dll`.
The source/DLL overlay package is `dist/A2FOExtensions-buildyard-MODULE-v13.zip`.
See the deployment receipt alongside this guide for exact hashes and backup.

## Manual acceptance

Accepted by the user on 2026-09-24: requirement text, installation denial without
bor_incubationZ, and disappearance/unlock after completing the prerequisite.
The screenshot confirms `Requires: Incubation Center` alongside native content.
The checklist below remains available for regression testing; save/load and
multiplayer are not covered by this acceptance.

1. Launch FOtest and start a fresh Borg skirmish without an Incubation Center.
2. Hover assimilator module0. Expect the complete native Beam Module tooltip
   plus **Requires: Incubation Center**, with no TEST. If the ODF name cache is
   unavailable, the fallback is `Requires: bor_incubationz`.
3. Attempt to install the module: it must be blocked. Merely unlocking the
   ability to build the Incubation Center must not unlock this module.
4. Queue the Incubation Center. The module must remain locked until the native
   completed count becomes positive. Finish it; the module must unlock and the
   Requires line must disappear. Try a fresh module slot eligible for this test.
5. Remove the last prerequisite and test another uninstalled module slot. The
   gate should lock again. Existing installed modules are not removed by this
   extension.
6. Repeatedly hover, including verbose tooltip display if enabled. Check for no
   duplicated lines, missing native text, clipping or crash. Ungated modules and
   ordinary buttons retain stock text/behavior, including shared tooltip keys.
7. Check `Data/A2FOExtensions.log` for the lines below. Save/load and multiplayer
   need separate verification; use the same extension build on every participant.

Existing example (MODULE is a symbolic label, not an ODF lookup):

```xml
<object MODULE="bor_assimilator_moduleB.odf">
    <requirement>
        <obj>bor_incubationZ.odf</obj>
    </requirement>
</object>
```

```ini
module0PseudoTechnology = "bor_assimilator_moduleB"
module0ButtonTooltip = "AUTOTOOLTIP-bor_assimilator_moduleB.odf"
```

For additional fixtures, multiple `<obj>` entries in a block mean AND and
multiple `<requirement>` blocks mean OR. Only unsatisfied conditions appear:
`Requires: (A AND B) OR (C)`. Satisfying either branch removes the line entirely.
`<obj mustNotHave="true">a.odf</obj>` is blocked while A exists and displays
`Requires: Not A`. Unknown/unreadable requirements fail closed even when negated.

## Expected diagnostics

Every line has the `[A2FOFeaturePack]` prefix. Pointers and byte counts vary.
Entry/result messages appear once per module/key/gate/team/result/text, not every
hover. The actual requirement ODF's cached unitName supplies the display name.

```text
MODULE <obj> count verified: FleetOpsHook+0x11F0CC EAX=team tree EDX=project ID ECX=0; signed EAX completed count at item+0x14, queued excluded
BuildYard tooltip native ABI: provider=... ECX=button stack=stringstream ret4 ESI-clobber; resolve=... cdecl(sret,key) [stock only]; insert=... cdecl(ostream,string&); tidy=... thiscall(string,bool); assign=... thiscall(string,chars,len); str=... thiscall(stream,sret)
BuildYard MODULE requirement tooltip enabled (phase 1 TEST visually accepted; native stream readback)
BuildYard symbolic MODULE technology support v13 initialized (native object counts and requirement text; no dummy ODF required)
BuildYard MODULE tooltip provider invoked for module0 'AUTOTOOLTIP-bor_assimilator_moduleB.odf' (gate=0x70000000, native=..., stream=...)
BuildYard MODULE tooltip module0 'AUTOTOOLTIP-bor_assimilator_moduleB.odf' gate=0x70000000 team=1 append stream-verified: original prefix and appended literal verified in native stream (bytes=N->N+28); chosen=Requires: Incubation Center
```

The appended newline plus `Requires: Incubation Center` is 28 bytes. Once the
requirement is satisfied, expect a once-only result containing:

```text
append skipped/FAILED: all MODULE requirements satisfied (bytes=0->0); chosen=<none>
```

That specific skip is expected success, not a failure. Failure diagnostics include:

```text
MODULE <obj> count signature mismatch; object requirements fail closed
BuildYard tooltip local-team signature mismatch; requirement text disabled
BuildYard module tooltip-provider full-body signature mismatch; requirement text disabled, MODULE gates retained
BuildYard requirement append disabled: provider helper slot/thunk signature mismatch
BuildYard requirement append disabled: import mismatch: <exact decorated MSVCP60 export>
```

Per-hover failure reasons include `local team unavailable or outside native table`,
`native object-count signature not verified`, `null or unreadable native stringstream`,
or `stream readback differs from original plus literal`. An unresolved real
requirement ODF also logs its failed project ID lookup once. Keep the complete
log if a gate remains locked or text does not render.

## Headless verification

```sh
make build/buildyard_tooltip_native_test.exe build/buildyard_tooltip_provider_test.exe
export WINEPREFIX=/home/tamsynn/.cache/a2fo-buildyard-tooltip-native-wine
export WINEDEBUG=-all
export WINEDLLOVERRIDES=msvcp60=n
wine build/buildyard_tooltip_native_test.exe 'Z:\home\tamsynn\Games\FO3\Data\MSVCP60.DLL'
wine build/buildyard_tooltip_provider_test.exe 'Z:\home\tamsynn\Games\FO3\Data\MSVCP60.DLL' 'Z:\home\tamsynn\A2FOHookExtensions\FleetOpsHook.dll'
```

The helper test covers 9 real stream lifetimes / 27 variable-text append calls,
plus missing-helper/null/empty/embedded-NUL rejection. The provider test executes
the supplied native provider and count routine with controlled localization and
team data. It covers buildable-but-unowned, queued-only and completed gate states,
unitName/fallback, AND/OR/negation, native RequiredTechnology compatibility,
ungated/shared-key behavior, repeated-hover logging and signature/import checks.
These tests do not replace the fresh-skirmish installation check above.

See [ABI explanation](docs/buildyard-tooltip-abi.md) and
[disassembly evidence](docs/buildyard-tooltip-disassembly.txt). Inactive v6–v9
experiments remain uninstalled; no global tooltip hooks are enabled.
