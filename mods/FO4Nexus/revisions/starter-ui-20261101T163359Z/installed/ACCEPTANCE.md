# Nexus 0.1 acceptance

Status: **pending manual game test**. No GUI automation or game launch was
performed during creation. Static results are in `validation.json`.

1. Fully exit Fleet Ops. Open the usual Roots launcher, choose Mods and
   **Fleet Operations: Nexus**. Confirm required modules are available.
2. Start a new Federation Instant Action game. Confirm maps, faction selection,
   tooltips, costs, construction and native resource bars still work.
3. Build a Saber, Intrepid and Akira; later build a Sovereign. Select each
   alone: check names, captain/registry, shield F/A/P/S rows, ammunition and
   XP bar fit without overlapping native controls. Check multiple selection.
4. Fight a stationary target. Hover over a normal torpedo icon: inspect the
   forward arc. Confirm firing consumes one round per projectile, preserves
   the Sovereign volley timing, stops at zero and resumes after replenishment.
5. Damage one facing; turn the ship. Confirm exposed-side hull damage while
   other shields remain, then verify recharge and the aggregate native bar.
6. Exhaust an Akira or Sovereign. Move within 350 units of an owned Antares
   or Eraudi yard. Check reload and repair concurrently, queue construction,
   leave range, and confirm an enemy yard does not replenish it.
7. Promote a partially depleted ship. Check the new rank still has the systems
   and record whether ammunition/facing state transfers or resets. Repeat
   with both avatars and available mixed-tech variants.
8. Save and reload with partial ammunition and shield damage. Confirm restored
   magazines and expected proportional shield redistribution. Check capture.
9. Repeat panel checks at 16:9, 16:10 and 4:3; test a captured non-Federation
   shipyard to ensure the native build queue remains visible.
10. Exit and preserve `Data/A2FOExtensions.log` before another launch: startup
    overwrites it. Inspect module admissions, the shared Craft::Damage bridge,
    directional policies and any rejected ODF/configuration messages.

Record date, runtime hash, map, avatar, resolution and each result here after
the manual test. A missing/overlapping display or rejected policy is a failed
case even if the game reaches the map. Multiplayer requires a later peer test.
