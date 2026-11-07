# Species 8472 breeder mock-up

Target: `Fleet Ops Roots/Data/Mods/test`, displayed as **STA2 Classic: Test**.
This is a local test fixture using that mod's inherited 8472 assets, not a
standalone faction or a package for redistribution. No new art is supplied.

## Try it

1. Restart the game, choose **STA2 Classic: Test**, and start a fresh game as
   **Species 8472** with **Standard** starting units and the normal technology
   tree. A Mother and an **Unseeded Breeder** are included. Minimal starting
   units also include the Mother and a breeder. Existing saves are not updated.
2. Keep the Mother within **800 world units** of the breeder. The Mother is
   mobile, so move it closer if necessary. For additional breeders, move a
   **Passive Embryo** into position and choose its added **Unseeded Breeder**
   evolution button (300 biomatter, 25 seconds, 1 officer).
3. Select the breeder and choose **Scout Strain** or **Destroyer Strain**.
   The genetic selection itself is free. An Active-Embryo-shaped seed should
   appear at the Mother and move to this breeder. Repeated clicks must not
   generate additional seeds while one is travelling.
4. When the seed comes within **120 world units**, it is consumed and the
   breeder becomes **Scout Breeder** or **Destroyer Breeder**. Both use the
   existing Fluidic Gate model. The genetic buttons disappear permanently.
5. The committed breeder should repeatedly produce the chosen offspring,
   using native costs, build times, officers and output handling:

| Offspring | Biomatter | Officers | Build time |
|---|---:|---:|---:|
| Brood Scout | 55 | 1 | 15 seconds |
| Brood Destroyer | 150 | 1 | 40 seconds |

Those values are inherited from the parent ODFs, not duplicated balance values.
The two offspring retain their original 8472 weapons and physics. Prototype
technology rows have no research prerequisites, so an Active Embryo is not
needed to keep automatic production available.

## Acceptance checks still requiring the game

- No nearby same-team Mother: selection creates no seed, queue item or charge.
- Destroy the travelling seed: the virgin breeder unlocks for another choice.
- Allow arrival: verify the seed disappears, the name changes, the genetic
  palette disappears, and **at least two offspring actually finish**.
- Exhaust biomatter or officer capacity: new production must stop; restoring
  capacity should let it resume. Native admission and spawn handling must be
  verified in the client.
- Two breeders may specialise independently. The original Mother, Active and
  Passive Embryo build choices should still work.

The module README explicitly identifies admission into a committed breeder
without a public build list as an unresolved first in-game test. The ODF
fixture does not resolve that native-engine uncertainty.

Do not save while a seed is travelling: that transient module state is not
serialised yet. Metabolism, starvation, pause controls, AI use, multiplayer,
and save/load acceptance are outside this mock-up. The radius is checked when
selecting genes; there is no Mother-radius overlay or disabled-button display.

## Files and installation

- `odf/`: three local overrides (Mother, Passive Embryo, faction) and nine
  prototype ODFs. Shared breeder fields contain **no buildItem list**.
- `tech1.append.tt`: seven unrestricted prototype entries for `techtree/tech1.tt`.
- `gui_global.append.spr`: aliases for seven buttons and their five-layer
  wireframes, using the test mod's existing sprite artwork.
- `info.ini` needs the next free `activeX = "A2FOBreeders"` in `[modules]`.
  The existing shared `Data/modules/A2FOBreeders.dll` supplies the implementation.

All dynamically referenced seed/result classes are listed in the faction's
`preloadUnits` entries. The faction overrides add one breeder to Standard and
Minimal starts; Super Side and custom map units are unchanged.

See `DEPLOYMENT.json` for installed paths, original/installed hashes, backup
location, and validation results. The installed mod also has a copy of this
guide as `BREEDER_MOCKUP.md`. Parent-mod files and shared DLLs are untouched.

Build the existing module, if needed:

```bash
cd /home/tamsynn/A2FOHookExtensions
make build/modules/A2FOBreeders.dll
```

After manually launching the mod, inspect the current log without GUI control:

```bash
rg 'A2FOBreeders|Breeder prototype|Breeder .*seed|Breeder committed|Breeder genetic|Breeder seed was lost' \
  '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/A2FOExtensions.log'
```

Expected startup text: `Breeder prototype initialized: genetic choices, mother
seeds, metamorphosis and automatic production enabled`. Matching deployed
hashes and static signature checks do not prove that runtime registration or
offspring production succeeded.

## Rollback

Close the game. Use the paths recorded in `DEPLOYMENT.json`: restore files with
a non-null `before_sha256` from the corresponding backup-relative path; remove
only files whose `before_sha256` is null. Check that their current hashes still
match `after_sha256` first, so later changes are not overwritten. This restores
the previous module list, sprites and technology tree and removes the added
local overrides. The parent mod and shared module need no rollback.
