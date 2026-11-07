# Field Yard selection in Fleet Operations: Test

These two ODF overrides target `Data/Mods/FOtest`, whose parent is
`Fleet Ops 4.0`. Both Klingon avatar variants retain their complete original
Field Yard definitions with only `selectPodsTogether = 1` appended. The
required implementation is in `A2FOSquadrons.dll`, already enabled by FOtest.

The three extension types belong to the **Field Yard**, not the Imperial Yard.
The parent ODF offers P1/P2/P3 twice each and P4 once; grouping follows the
actual attached shipyards regardless of which extension choices were built.

Restart Fleet Ops, choose **Fleet Operations: Test**, and start a Klingon game.
Build a Field Yard and its extensions. Click the main yard, then each attached
yard: each should select the same complete complex. Build a second complex
nearby and verify it stays independent. Check Shift-toggle, a destroyed pod,
capture, save/reload, and each yard's production controls. Runtime/visual
acceptance is manual; headless tests do not prove those controls work in-game.

Parent source SHA-256:

- `kli_field_yardY.odf`: `369d6d5d3149496c45d4b09fd00e50936cc939c427368a4c5c0212133991e786`
- `kli_field_yardZ.odf`: `e82f5ed413b9241135eadf937b84c43f65c87f3f62a527300286e648bf7eec93`

Native relationship evidence in installed ArmadaL (SHA-256
`817b1ddb9876cfb9c5d781b42d2513de48fef9cbdbc408aa43c878c573581a18`):
RVA `b9a5a`/`b9a6c` initializes the ResearchStation count/list at `+2ac/+2b0`;
`ba68f` writes the constructed child pointer; `ba7ea` clears the exact child.
RVA `b99e7` installs the root vtable at RVA `2b32a4`. The implementation checks
the count/list instructions and the live root vtable before reading a list.
It uses Entity handles, class opt-in, same-owner filtering and cleanup events;
it does not infer relationships from proximity or assume ResearchPod fields
exist on the shipyard objects.

Build/check:

```sh
make build/modules/A2FOSquadrons.dll build/pod_group_selection_test.exe
WINEPREFIX=/home/tamsynn/.cache/a2fo-squadrons-wine WINEDEBUG=-all \
  wine build/pod_group_selection_test.exe
```

Deployment receipts and original files belong in the installation's
`.deployment-backups/` directory. Restore its original DLL and remove the two
new FOtest overrides to roll back; leave the parent mod's ODFs untouched.
