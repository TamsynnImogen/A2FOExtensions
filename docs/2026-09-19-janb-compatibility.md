# Jan_B September binary compatibility

The installed `Data/A2FOExtensions.log` stopped at:

```text
ArmadaL.exe version mismatch (timestamp=3c4c76bd, image=00405000)
No hooks installed
```

Both game-image timestamps still match their previous values. Armada's
`.test` section grew from `0x9999` to `0xa999`, increasing `SizeOfImage`
from `0x403999` to `0x405000`. The section RVAs remain unchanged.

The shared Armada identity gate now recognizes the exact September profile
using its layout and on-disk `.text`, `.rdata`, and `.test` fingerprints.
CraftIdentity's object editor now uses this shared gate too. The existing
per-hook instruction checks remain enabled. Exact identities are recorded
in [addresses.md](addresses.md).

FleetOpsHook also reuses the former money-cheat routine at RVA `0x1fc320`
for icon rendering and removes its native chat registration. A2FOCheats
recognizes that variant without detouring the repurposed routine, then
registers its own `showmethemoney` handler by name. The legacy inline path
remains available for older builds.

Validation passed: release build, `make test`, `make verify`, headless Wine
DLL loading, and ODF/module initialization against both the old and new
FleetOpsHook fixtures. `supported_armada_smoke.exe` accepts the current and
previous Armada executables and rejects seven modified-code/header cases.
The fixtures are mapped without running either game's entry point. Module
smoke uses synthetic Armada engine state; it does not validate gameplay.

Build `20260919-janb-september-image-compat-1` is deployed under
`/home/tamsynn/NVMeData/Fleet Ops Roots/Data`. The core, helper, six affected
modules, and the STA1 Classic Mod RGBTextures override were backed up and
hash-verified after deployment. Rollback copies and the per-file manifest are
in `Data/rollback/2026-09-19-janb-compat-112314/`.

Immy reports that the deployment seems to be working. The subsequent live
log reaches race resource registration and the mission selector. This is an
initial manual confirmation; comprehensive gameplay regression coverage is
not claimed. No agent-driven game launch or input-device automation was
performed.
