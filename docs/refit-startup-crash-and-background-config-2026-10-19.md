# Startup exception, rollback and background switch — 19 October 2026

The user reported the application's madExcept error screen following the 00:31
menu deployment. The new menu DLL was withdrawn as a containment step. The
previous DLL was restored from its verified backup:

- Restored menu DLL: `acda0f91b8f6458253cead3aadc4d346909dce7f273659a6de8a66c842b1df0d`.
- Withdrawn candidate: `9fa4e5d0b5ea53d7fd79c02cd5b6a992e4c1277d86b05f32a7d30f51bbc38d8c`.
- [Rollback receipt](refit-startup-crash-20261019-003354-rollback.json).
- Evidence and quarantined DLL:
  `.deployment-backups/refit-startup-crash-20261019-003354/`.

The exception report is dated 00:32:53, with a process uptime of 50 ms. It names
`EOSError`, error 299, in `FleetOpsHook.dll`. Its stack reaches
`RtlVclOptimize.InitDMTBinSearch` through dynamic-method lookup,
`TBitmap.Create`, GR32 initialization and `System.@StartLib`. Neither the menu
DLL nor its renderer provider appears in that report's module list.

Static disassembly of the unchanged, pinned FleetOpsHook confirms that
`InitDMTBinSearch` begins at RVA `0x000d8ddc`. It allocates/copies/sorts a dynamic
method table, then calls the imported `WriteProcessMemory` at VA `0x5a8d8e50`
to update a four-byte pointer in the current process. The zero-result branch
calls `RaiseLastOSError` at `0x5a8d8e59`, matching the report. Import thunks and
names were checked directly against the installed PE. This identifies the
failing operation; why that write failed in the reported launch is unresolved.
File section flags alone do not establish live page permissions.

During investigation, the live extension/provider logs advanced to 00:33:41–45
and included scene-worker initialization, after the exception report's time.
Those logs describe a subsequent launch, not proof that the 50-ms failure
reached menu initialization. Both the report and subsequent logs were saved.
The rollback is verified by hashes, but has not been accepted in-game. No game
binary, FleetOpsHook patch or core DLL was changed, and no game was launched or
controlled by automation. Refit and font attribution notices remain intact.

## Independent background configuration

The existing `LiveScene` switch controls only Refit's 3D menu backgrounds.
The live `Data/A2FORefitMenus.ini` is now explicitly configured as:

```ini
[RefitMenus]
Enabled=1
LiveScene=0
```

- `LiveScene=0`: use installed menu background images; do not start the 3D
  provider, its device or a scene worker. The menu module stays enabled.
- `LiveScene=1`: request animated Refit 3D backgrounds, retaining artwork fallback.
- `Enabled=0`: disable the Refit menu module's presentation/skin altogether.

Fully quit/relaunch after changing these values. The current DLL already
supports this switch, so no new DLL deployment is needed. The example INI and
module README now document it explicitly. Source-level early returns in the
main-scene and native-background paths were checked; in-game acceptance of the
restored/static configuration remains pending.

The configuration change is recorded in
[the static-background deployment receipt](refit-static-backgrounds-20261019-003700-deployment.json),
with its prior INI backup and nine unchanged protected-file hashes.
