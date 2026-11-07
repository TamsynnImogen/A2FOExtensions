# Nexus 0.1 acceptance

Status: **Venture identity display confirmed; frame-inset correction pending retest**.
The 2026-11-01 screenshot shows captain Riley Quinn and registry NCC-428194974
on USS Dart. The fresh log also records the first single-panel identity draw.
The captain row sat above the panel frame; both medium identity rows and the
constructor's top band have now been inset. Final font fit and Mediterranean
display still need visual confirmation.

The first manual
test selected Venture and Mediterranean, which originally had no custom
identity data. Their coverage is added in 0.1.1. The saved 2026-11-01 log
confirmed Nexus configuration and combat-ship feature registration; it did
not record a successful identity draw. Static results are in `validation.json`.

For the immediate retest, start a fresh Federation skirmish and select the
starting Venture alone: captain and NCC registry should appear on the right.
Select the Mediterranean alone: captain and registry should appear side by
side above its queue. Queue a station and check that neither identity field
overlaps its construction controls. A clean full restart is required for the
new ODF data. These two ships have no Nexus ammunition or directional-shield
configuration; use the combat ships below to test those displays.

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
