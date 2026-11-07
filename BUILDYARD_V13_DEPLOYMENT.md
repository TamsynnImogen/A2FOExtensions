# BuildYard v13 deployment — 2026-09-24

Built and installed DLL SHA-256: `bd75c17307058417afd4edb74220637a1740cc8d29cae752bbbf3b8ee0e67cd3`.

Installed: `/home/tamsynn/NVMeData/Fleet Ops Roots/Data/modules/A2FOFeaturePack.dll`.

Phase 1 TEST build preserved: `/home/tamsynn/A2FOHookExtensions/.deployment-backups/buildyard-tooltip-v13-20260924/A2FOFeaturePack.dll`.
Backup SHA-256: `c5fa195175f4fece5ad4983cc43af60f764d5bef746d4fa4cb7985f2b403db62`.

Armada was not running at replacement. Built/deployed hashes match.
Phase 1 TEST was visually confirmed by the user. Phase 2 was accepted by the user on 2026-09-24: requirement text appears,
installation is blocked without bor_incubationZ, and completing the prerequisite
removes the line and unlocks installation. A supplied screenshot confirms
`Requires: Incubation Center`. Save/load and multiplayer are not yet verified.

Rollback with the game closed:

```sh
cp '/home/tamsynn/A2FOHookExtensions/.deployment-backups/buildyard-tooltip-v13-20260924/A2FOFeaturePack.dll' '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/modules/A2FOFeaturePack.dll'
```

Original v11 backup also remains under
`.deployment-backups/buildyard-tooltip-v12-phase1-20260924/`.
