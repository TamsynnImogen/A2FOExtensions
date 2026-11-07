# Shared production for identical yards

Select two or more of your own yards of the same ODF class, open Build, and
choose vessels normally. Each click sends one normal synchronized build order
to the yard with the fewest queued ships. Ties rotate through the selected
yards. Five orders to three initially empty yards distribute as 2 + 2 + 1.
The Cancel command targets one longest queue. Single selections and other
object combinations retain their native dispatch behavior.

This adapts Armada II Refit QOL-6, pinned to
`37d60079169b086dba4216fe55e05cd6015d66c0`, with the retained MIT notice in
`third_party/armada2-refit/`. The C++/i386 integration checks the actual local
ArmadaL and FleetOpsHook instructions before installing any memory patches.
Game executables on disk are untouched.

Fleet Ops has separate checks for enabling Build and retaining its palette.
The module extends both checks for identical owned yards. The palette check
at FleetOpsHook RVA `0x1e6fe5` otherwise resets menu 2 back to Root whenever
more than one yard is selected, leaving an enabled button that cannot open
its list. Research and Special palettes retain their native selection rules.

Enable the module in the selected mod's `[modules]` list, and opt in under
`[mod]` in its `info.ini`:

```ini
[mod]
MultiYardBuild = 1

[modules]
active35 = "A2FOMultiYardBuild"
```

Use an unused active index and preserve the existing sections/entries. Both
the module and flag are required; the explicit flag defaults off so legacy
load-all mods are unaffected. Settings follow the ordered mod inheritance
chain, with a child's `MultiYardBuild = 0` disabling an inherited opt-in.

Build from the repository root:

```sh
make build/modules/A2FOMultiYardBuild.dll build/modules/A2FOFeaturePack.dll
make build/multi_yard_policy_test build/multi_yard_native_test.exe build/multi_yard_queue_load_test.exe
build/multi_yard_policy_test
WINEPREFIX=/home/tamsynn/.cache/a2fo-squadrons-wine WINEDEBUG=-all \
  wine build/multi_yard_queue_load_test.exe
WINEPREFIX=/home/tamsynn/.cache/a2fo-squadrons-wine WINEDEBUG=-all \
  wine build/multi_yard_native_test.exe \
  'Z:\home\tamsynn\NVMeData\Fleet Ops Roots\Data\ArmadaL.exe' \
  'Z:\home\tamsynn\NVMeData\Fleet Ops Roots\FleetOpsHook.dll'
```

The updated FeaturePack adds only a read-only queue-size export for this
feature, including deferred jobs beyond the ten-node native FIFO. When that
export is unavailable, the native build-queue length is used. Queue admission,
payment, technology, collective rank resolution, and completion remain in the
existing synchronized Producer path. The sender's last-chosen yard is UI state;
there is no new network packet, simulation timer, or save field.

The Field Yard's automatic whole-complex selection contains different ODF
classes (main station plus extension types). It does not meet this feature's
identical-yard rule. This pass does not create a union build menu for mixed
station classes.

Manual acceptance after a full restart:

1. Select three identical completed yards; verify Build remains available and
   five slow-vessel orders produce 2 + 2 + 1 across their individual queues.
2. Start with uneven queues; check the shortest receives the next order, and
   Cancel targets one longest queue. Repeat with one yard full and with logical
   queues beyond ten jobs, existing Ctrl-fill, and continuous production.
3. Check enemy/captured/destroyed yards, mixed classes, individual yard queues,
   and the previous Field Yard pod selection behavior.
4. Check normal save/reload and multiplayer on matching module installations.
   Headless signature, policy, and assembly tests do not establish live UI or
   multiplayer acceptance.

Native patch evidence and deployment receipts are recorded under
`build/multi-yard-build/`. The installation backup preserves the previous
FeaturePack and `FOtest/info.ini`; restore those and remove the newly added
MultiYardBuild DLL to roll back.
