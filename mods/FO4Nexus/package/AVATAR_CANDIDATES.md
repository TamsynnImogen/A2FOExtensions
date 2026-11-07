# Future avatar and faction candidates

Captured from Immy's suggestions on 2026-11-01. These are candidates for
later Nexus milestones, not an instruction to implement them now. The first
five portraits are possible third avatars; the other groups are possible
new factions with three avatar choices each. Final inclusion, faction
assignments, bonuses and fleet rosters remain open.

## Possible third avatars

| Candidate | Original portrait | Planning note |
| --- | --- | --- |
| Donatra | [avatars_Donatra.png](/home/tamsynn/Pictures/avatars_Donatra.png) | Proposed Romulan third avatar |
| Kuvahmagh | [avatars_Kuvahmagh.png](/home/tamsynn/Pictures/avatars_Kuvahmagh.png) | Proposed Klingon third avatar |
| Renagade | [avatars_Renagade.png](/home/tamsynn/Pictures/avatars_Renagade.png) | Faction and final display spelling to confirm; preserve supplied filename |
| Ross | [avatars_Ross.png](/home/tamsynn/Pictures/avatars_Ross.png) | Proposed Federation third avatar |
| Son'a | [avatars_Sona.png](/home/tamsynn/Pictures/avatars_Sona.png) | Faction/alliance placement to confirm |

These faction placements are working suggestions, not confirmed decisions.

## Cardassians

| Candidate | Original portrait | Optional design direction |
| --- | --- | --- |
| Damar | [avatars_Damar.png](/home/tamsynn/Pictures/avatars_Damar.png) | Resistance, resilience and rebuilding |
| Dukat | [avatars_Dukat.png](/home/tamsynn/Pictures/avatars_Dukat.png) | Military power and territorial control |
| Garak | [avatars_Garak.png](/home/tamsynn/Pictures/avatars_Garak.png) | Intelligence and asymmetric warfare |

The design directions are brainstorming only. An existing local Cardassian
implementation may be a useful reference, but compatibility, asset provenance
and suitability for this FO4 child must be assessed before reuse.

## Species 8472 / Undine

| Portrait text | Original portrait | Optional design direction |
| --- | --- | --- |
| We Must Defend | [avatars_Defend.png](/home/tamsynn/Pictures/avatars_Defend.png) | Defensive survival |
| The Weak Shall Perish | [avatars_Perish.png](/home/tamsynn/Pictures/avatars_Perish.png) | Aggressive biological warfare |
| Your Galaxy will be Purged | [avatars_Purged.png](/home/tamsynn/Pictures/avatars_Purged.png) | Purge and expansion |

Consider these as collective doctrines or stances rather than requiring
named individual commanders. The label Species 8472 versus Undine remains
a presentation choice.

## Noxter

| Candidate | Original portrait | Optional design direction |
| --- | --- | --- |
| Exodus | [avatars_Exodus.png](/home/tamsynn/Pictures/avatars_Exodus.png) | Migration and survival |
| Expansion | [avatars_Expansion.png](/home/tamsynn/Pictures/avatars_Expansion.png) | Growth and map spread |
| Retaliation | [avatars_Retaliation.png](/home/tamsynn/Pictures/avatars_Retaliation.png) | Reactive defence and counterattack |

These could become biological strategic paths, with distinct production and
growth mechanics, after their runtime behaviour and save support are checked.

## Portrait inspection and implementation gates

All fourteen supplied files were present and visually inspected. Each is a
256-by-256 RGB PNG, with the FO-style frame and title already baked in.
The original files remain untouched in Pictures; none has been converted,
copied into the mod payload or registered with the game.

The Renagade image contains an orange circular overlay across the face.
Determine whether that is intentional before preparing final art. Noxter's
dark title text could use a readability check at the game's displayed size;
the long Undine titles also need an in-game fit check.

Before implementation:

1. Finish the initial Nexus combat and panel acceptance pass.
2. Confirm the candidate lineup and faction placement; define an actual
   gameplay distinction for each avatar.
3. Inspect the FO4 avatar-selection data, UI and AI for support of a third
   choice. A portrait alone does not establish three-avatar compatibility.
4. Assess fleet assets, technology trees, sounds, starting units, AI,
   multiplayer identifiers and any mixed-tech requirements for each new race.
5. Verify the extension modules needed by the chosen mechanics and their
   save/load behaviour; build and test one faction at a time.
6. Prepare derived portrait assets while retaining the originals, then test
   selection, starting fleets, bonuses, research and AI in a fresh skirmish.

This note is source-project planning only. The installed Nexus 0.1 gameplay
and its current package are unchanged.
