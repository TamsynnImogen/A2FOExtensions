# Nexus 0.1 acceptance

Status: **restored 0.1.1 launch and Mediterranean identity display confirmed**.
The manual PortProton DEBUG run on 2026-11-01 reached gameplay. The screenshot
shows USS Blondel, captain Jordan Vale and NCC-264543711 inside the information
panel. The first identity submission is also present in the new extension log.
The Mediterranean uses the ordinary single-selected panel in this observed
state, despite being a construction ship; a construction role alone does not
establish use of the tall producer panel.

The original failed start produced no fresh Armada log. The successful retry
changed both the installed UI (rollback) and the PortProton launch mode
(DEBUG), so it does not isolate the cause. No fatal startup exception was
found in the captured successful run. The 0.2 candidate remains withdrawn.
Snapshots are under `diagnostics/restored-ui-working-20261101T201139Z`.

Venture identity display was confirmed before the 0.2 update.
The 2026-11-01 screenshot shows captain Riley Quinn and registry NCC-428194974
on USS Dart. The fresh log also records the first single-panel identity draw.
The captain row sat above the panel frame; both medium identity rows and the
constructor's top band have now been inset. Version 0.2 separates identities
and ammunition from a graphical four-facing shield display and widens the XP
track. Those candidate graphics and the tall producer header still need
visual confirmation. They are not active in the restored version.

The first manual
test selected Venture and Mediterranean, which originally had no custom
identity data. Their coverage is added in 0.1.1. The saved 2026-11-01 log
confirmed Nexus configuration and combat-ship feature registration; it did
not record a successful identity draw. Static results are in `validation.json`.

For a repeat check, select the starting Venture and Mediterranean separately:
captain and registry should appear on the right. Queue a station and check
that neither identity field overlaps its construction controls. These two
ships have no Nexus ammunition or directional-shield configuration; use the
combat ships below to test those displays. The next isolated experiment is
`probes/sprite-registration`, which adds sprite definitions and textures while
inheriting the restored layout and ODFs.

1. Fully exit Fleet Ops. Open the usual Roots launcher, choose Mods and
   **Fleet Operations: Nexus**. Confirm required modules are available.
2. Start a new Federation Instant Action game. Confirm maps, faction selection,
   tooltips, costs, construction and native resource bars still work.
3. Build a Saber, Intrepid and Akira; later build a Sovereign. Select each
   alone: check names, captain/registry, four shield arcs and percentage labels, ammunition and
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
