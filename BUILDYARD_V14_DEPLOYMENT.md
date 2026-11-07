# BuildYard MODULE v14 deployment — awaiting in-game acceptance

Current installation: `/home/tamsynn/NVMeData/Fleet Ops Roots/Data/modules/A2FOFeaturePack.dll`.
SHA-256: `b2e48590614a7e721a0aaf1891dbc3daaab4cfc040c6ebe06b99d1534a6195ea` (v14).

The reported pre-menu failure persisted for the user with restored
v13 too. The user then identified the executable being launched as
`/home/tamsynn/A2FOHookExtensions/ArmadaL.exe`: this is the analysis copy in the
source workspace, which lacks binkw32.dll, NetworkManager.dll,
Win2kDisableTaskSwitch.dll, RTS_CFG.h and the deployed FeaturePack DLL. It is not
the installed game. The installed-game logs never changed during those attempts.
The v14 DLL was reinstated in the complete game installation after that discovery;
no hook changes were made to address the wrong-path launch.

Launch `/home/tamsynn/NVMeData/Fleet Ops Roots/Data/ArmadaL.exe` for testing.
The saved Armada L application and PortProton shortcuts also referenced an old,
nonexistent Games path; both were backed up and corrected to the NVMeData path.
Temporary per-game Wine diagnostics were removed and the original ppdb contents
verified byte-for-byte. No process named ArmadaL.exe was running at replacement.

Accepted v13 rollback DLL:
`.deployment-backups/buildyard-module-v14-20260924/A2FOFeaturePack.dll`.
SHA-256: `bd75c17307058417afd4edb74220637a1740cc8d29cae752bbbf3b8ee0e67cd3`.
Investigation copies of v14, old logs, profile and shortcuts are preserved in
`.deployment-backups/buildyard-module-v14-failed-20260924/`.

Portable policy, native bridge and tooltip fixtures pass. A separate headless
LoadLibrary/FreeLibrary check of v14 also passes. These do not establish in-game
loader, include/archive, diplomacy, save/load or multiplayer acceptance.
See [v14 rules, ABI and tests](docs/buildyard-module-v14.md).

Rollback with Fleet Ops closed:

```sh
cp '/home/tamsynn/A2FOHookExtensions/.deployment-backups/buildyard-module-v14-20260924/A2FOFeaturePack.dll' '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/modules/A2FOFeaturePack.dll'
```

The user subsequently reported another pre-menu failure when asked to launch
from the full installation. The game logs still did not change. A workspace
ArmadaL.exe.ppdb profile was created/updated at 14:53, showing recent activity
against the analysis copy too. The wrong path is a confirmed problem, but it
has not been established as the sole cause of every reported attempt.
Next diagnostic is an explicit PortProton --launch command with PW_LOG=1 and
WINEDEBUG=err+all,+seh,+loaddll, supplied for the user to run manually.
