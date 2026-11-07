# Fleet Operations: Nexus

Nexus is a child of **Fleet Ops 4.0**, built as a playable showcase for
A2FOExtensions. Version 0.1 is the Federation tactical-systems milestone;
0.1.1 adds visible identities to the starting ships. Version 0.2 gives the
extension readouts a dedicated layout and graphical shield quadrants.
It inherits the parent fleet, maps, AI, technology trees and assets.

## What to try

Choose **Fleet Operations: Nexus** in the Roots Mods list, then start an
Instant Action game as either Federation avatar. Fully restart the game
when switching mods; use a new skirmish rather than an existing save.

| Ship family | Nexus role and systems |
| --- | --- |
| Venture | Captain and registry readout, including its existing variants |
| Mediterranean | Captain and registry readout in the constructor panel, for both avatars and starting variants |
| Saber | Manoeuvrable escort with four independent shield facings; the two existing torpedo-equipped veteran variants also receive a 48-photon store |
| Intrepid | Patrol cruiser: 48 photons, regenerating 0.15 rounds/second |
| Akira | Torpedo cruiser: 96 photons, replenished at friendly yards |
| Sovereign | Heavy assault ship: 64 quantum torpedoes, replenished at friendly yards |

All four families receive directional shields, aligned captain/registry
identities and a selected-ship tactical readout. The overlay covers both
avatars, ranks, and the existing mixed-tech/refit variants of these families.
Total shield capacity stays at the parent value: forward approximately 35%,
aft 15%, port 25%, starboard the remainder after whole-point rounding.
Turning away can expose a weaker shield;
an exhausted facing admits hull damage while other facings still hold.

Standard photon/quantum launchers on these ships consume one round per
projectile. Their firing volume is 120 degrees horizontally and 90 degrees
vertically, centred forwards. Hover over the launcher icon for its arc
preview. Phasers and special abilities keep their existing behaviour.
Launchers are cloned privately so shared parent weapons remain unchanged.

Federation shipyards provide resupply within 350 world units. Akira magazines
reload at 3 rounds/second and Sovereign magazines at 2 rounds/second while
inside a same-team provider's range. Other provider types retain the runtime's
existing defaults. Move these ships back to a yard when their stores run low;
phasers remain available when torpedoes are exhausted.

The selected-ship readout has an identity/ammunition column and a separate
shield quadrant display, with a wider XP bar under the identity column.
The four arcs drain independently and display remaining percentages. Their
normal cyan changes to amber at half strength and red at quarter strength.
Hover over a facing for its detailed tooltip. Only configured systems appear.
The Venture and Mediterranean receive identity fields only, keeping their
original gameplay. The constructor panel places captain and registry side by
side above the queue. It uses native FO4 sprites and fonts, plus two existing
local shield texture assets. See [layout and asset details](UI_LAYOUT.md).
Producer panels retain their build queues. Existing ship names remain intact;
the additional officers and registries are fictional showcase data.

## Build and validate

Run from the source directory
`/home/tamsynn/A2FOHookExtensions/mods/FO4Nexus` (build tools stay there):

```sh
python3 tools/build_package.py --parent '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/Fleet Ops 4.0'
python3 tools/validate.py --parent '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/Fleet Ops 4.0' --package package --report validation.json
```

`package/` is the child-mod payload. `content-manifest.json` and
`integration-manifest.json` pin parent inputs and generated files.
`ui-assets-manifest.json` pins the GUI table and reused shield artwork. Deployment
receipts record installed hashes and the protected parent/runtime checks.
The installed folder is `Data/Mods/FO4 Nexus`. Settings use `A2FO Nexus`.
No executable, global runtime configuration or parent content is replaced.

Install into a previously absent mod folder with:

```sh
python3 tools/deploy.py --parent '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/Fleet Ops 4.0' --target '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Mods/FO4 Nexus'
```

The installer refuses to overwrite an existing mod. For rollback, close the
game and move the complete `FO4 Nexus` directory outside `Data/Mods`.
No original-file restoration is needed because this is a new child folder.

The required module list makes the gameplay dependencies explicit. Install
the shared A2FOExtensions runtime centrally; mod-local DLLs are unsupported.
This package is currently a local development mod dependent on the installed
FO4 parent. Original game/FO assets and authors' credits remain applicable;
this is not a standalone redistribution package.

## Acceptance and current limits

Static content validation proves references, configuration and hash integrity.
It does not prove native engine loading, rendering, combat or save compatibility.
The first manual acceptance pass is recorded in `ACCEPTANCE.md`.

The AI has no new magazine-aware retreat orders. It can exhaust a ship's
yard-replenished ammunition. Balance values are provisional. Directional
facing distribution is not separately saved by the current extension: loading
redistributes saved aggregate shields proportionally. Promotion and save/load
must be checked with the installed runtime. Use the same package and compatible
runtime on every peer; multiplayer is not yet accepted.

## Following milestones

Future third avatars and possible Cardassian, Species 8472/Undine and Noxter
factions are recorded in [the candidate roster](AVATAR_CANDIDATES.md), with
links to the fourteen original portraits. These remain proposals.

1. Accept the Federation systems in game, including promotion, capture,
   repair/resupply, multiple screen ratios and save/load.
2. Give each faction its own mechanics: Klingon attack facings, Romulan
   ambush/refit choices, Dominion logistics and Borg adaptation. These are
   design candidates, not implemented features.
3. Add authored yard submenus and refit progression using the FO4 technology
   tree and A2FO production tools after testing queue/save behaviour.
4. Add damage feedback and material improvements against verified model
   nodes, then develop the Nexus menu identity and presentation.
5. Tune AI and multiplayer balance before expanding experimental features.

Each milestone should have a visible gameplay purpose and an in-game
acceptance record before the next major system is enabled.
