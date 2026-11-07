________________________________________________________________________________
Fleet Operations Version 3.2.7 - Armada II Total Conversion
Copyright (C) 2003-2012 Fleet Operations Development Team
http://www.fleetops.net
¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
________________________________________________________________________________
Contents:
¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
1. Introduction
2. Features
3. Changelog
4. Guide
5. Additional Credits
6. Contact Information
________________________________________________________________________________
1.  Introduction
¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
Fleet Operations is a popular skirmish and multiplayer orientated modification
for Activision's Star Trek Armada II. The project target was not only to create
a simple "modification" but trying to make Armada II a worthy successor to the
second best selling Star Trek game Star Trek: Armada (behind Interplay's
Star Trek: 25th Anniversary by a small, but fair margin).

As Armada II is much different from what everybody thought the successor of
Armada I should be, we started  to change the philosophy of the game. The
objective of our mod is to bring the game back to the Star Trek root.
We do not believe in the sense of creating big fleets within seconds and battles
based on the pure number of materials. We do not see benefits of an unbalanced
weight between the races. Our mod is based on the conviction that each ship
should have its defined task and does not loose its importance during the game.
We designed each ship new with more realistic textures. Each race has its single
new designed techtree. We introduced a credit system that gives you the
possibility to create a certain number of race specific special ships.
We designed new surroundings for the maps, more realistic asteriod fields,
more animated star maps to increase perception of the game. 

________________________________________________________________________________
2.  Features
¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
Key features of the current Release

- 5 balanced races: Federation, Klingon, Borg, Romulan and Dominion
- More than 200 new ships and stations
- Dynamic tactics: Avatars, ship experience system, mixed tech, warp-in, ...
- Fully supported and improved AI
- Community driven and active in development since 2003
- Game engine enhancements: Widescreen support, Tunngle online gaming,
  map manager, ...
- Not the Armada you played: 99% of all models, graphics, sounds, scripts
  rewritten or replaced
- Patched to include latest fixes up to Windows 7
- Easy Installation: Installs right off your Armada II CD - no
  pre-installation required

________________________________________________________________________________
3.  Changelog
¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
--------------------------------------------------------------------------------
Version 3.2.7                                                  21th October 2012
--------------------------------------------------------------------------------

Changes
-------
- Updated Tunngle API to work with latest client versions

Fixes
-----
- Fixed 'uxtheme.dll' crash on game launch
- Multiplayer
  - Fixed 'Out of Sync' issue in consecutive games
  - Fixed 'Out of Sync' issue while boarding vessels
  - Fixed 'Out of Sync' issue with Borg AI players present
- Mapping
  - Fixed Directional light source color reverting back to white (again)

--------------------------------------------------------------------------------
Version 3.2.6                                                 10th December 2011
--------------------------------------------------------------------------------

Changes
-------
- Updated Tunngle API to work with new Client Versions

Fixes
-----
- Fixed FPQEdit extraction not working on some systems
- rts_cfg.h wasn't included in multiplayer crc check


--------------------------------------------------------------------------------
Version 3.2.5                                                      7th June 2011
--------------------------------------------------------------------------------

Changes
-------
- Mouse and keyboard scroll speeds now have more logical default values.
  Also the maximum adjustable scroll speed has been increased
- Modding
- AssetVersion layout has been changed. Now also being displayed in Mod Settings

Fixes
-----
- After game start the view area could scroll to the left until the mouse was
  moved
- FPQEdit
  - Column sorting wasn't working properly
- Modding
  - AIP and Formation files still weren't loaded properly when using
    modification modules
- Mapping
  - Directional light source color always reverted back to white

--------------------------------------------------------------------------------
Version 3.2.4                                                      31th May 2011
--------------------------------------------------------------------------------

Additions
---------
- FPQEdit
  - Added functions for moving and renaming files
  - File list can be filtered with a mask

- Fixes
  - Fixed a old Armada crash bug with the "DeathChantWeapon"
  - Minor memory leaks fixed
  - Modding
    - ODF.fpq now has correct file structure
    - AIP files weren't loaded properly when using modification modules

--------------------------------------------------------------------------------
Version 3.2.3                                                      30th May 2011
--------------------------------------------------------------------------------

Additions
---------
- A new Generix Refit is available: The Singularity Refit
- Installing two Interception Modules into a Dodecahedron now adds the ability
  to engage axial assault patterns in order to perform attacks against
  multiple targets
- A resource load bar for freighters is now shown on mouse hover
- Engine
  - Tremendously reduced Multiplayer Asset checking time
  - Improved overall loading times
  - Simplified crash report sending by removing the mail client requirement
  - Updated FMOD Ex sound engine to 4.34.04
- Modding
  - The ODF folder has been converted into the new FPQ container format.
- New/Updated Maps
  - Duel II by Boggz

Changes
-------
- Adjusted the available player colors for better diversity
- Time in the Admirals Log is now expressed in real time
- General Gameplay
- Reduced the hull and shield reduction units suffer while being refitted or
  upgraded
- Borg
  - Increased the cooldown between Auto Adaption Scanner sweeps to 5/10
    seconds (bf.: 4/8 seconds)
  - The Auto Adaption Scanner now also scans and adapts to hostile Borg vessels
- Dominion
  - Dominion Workerships now reflect their current operation mode in their
    class name
  - Reduced the construction time of B-8 War Frigates slightly
  - Passive probes do no longer produce a blocking footprint for station
    construction and are no longer selectable to better blend with the
    environment and make spotting them harder
- Federation
  - The Fleet Supply ability of the Rhode Island Refit Nova Class is now called
    Extended Fleet Supply and is considered a seperate weapon, which means
    that buttons will no longer stack
  - The Distortion Field no longer affects stationary objects. It will attempt
    to affect maneuverable targets first
  - Reduced the construction time of the basic Platform, but upgrading now has
    a construction time
  - Starfleet Command construction time increased to 140 seconds
    (bf.: 120 seconds)
  - Reduced Akira Class construction time and costs
  - Reduced the offensive value of Nebula Class vessels of the San Francisco
    type to 18 (bf.: 23), but Full Scan now reduces the offensive value
    by 15 (bf.: 20)
  - Excelsior Class offensive value reduced to 16 (bf.: 18)
  - Increased the defensive and system value of Platforms
    to 23 / 10 (bf.: 12 / 5)
- Klingon
  - Slightly increased the construction costs and time of Sang' Class
    Bird of Preys
- Romulan
  - Adjusted the chance of the Arlterium Disruptor to drain special energy
    slightly
  - Reduced the visual size of Plasma Torpedoes fired by the Generix Class.
    Just the visual size!
  - Unstable Plasma Torpedo duration increased to 10 seconds (bf.: 8 seconds)
    and it will no longer miss
  - Phase Plates damage reduction reduced slightly and added a 30 second
    cooldown

Fixes
-----
- Fixed several typos and added some clarifications to tooltips
- Auto-Assimilator energy costs are now correctly displayed in the tooltip
- Defense Batteries are now correctly highlighted again if placing their
  footprint near a Singularity Transmitter
- Fixed a rare crash in the sound engine
- Fixed the overlapping hotkeys for Workerships
- The Borg Dodecahedron is now considered a Support Vessel in the Admirals Log
- Admiral Maysons Avatar Bonus is now correctly displayed when upgrading to
  a Pulse Platform
- Double-clicking a vessel could sometimes select all vessels on screen but
  vessels of the same type
- Game Mechanics
  - The additional ECM scramblers secretly installed on Centaur Class vessels
    built at the McKinley Yard were discovered and confiscated
  - The chance to disable weapons with the Arlterium Disruptor no longer
    decays upon level up
  - The Secondary Buffer System officer ability now yields its defensive
    value bonus correctly
  - The Specter Generix can no longer be promoted to veteran if there are
    too less free slots available
  - Veteran Norway Class vessels now deal correct damage
  - Fleet Supply now also correctly affects artillery and strategic
    range vessels
  - The Metaphasic Disruptor now obeys its cooldown as intended
  - Only Borg vessels may now benefit from Adaption Scanners as intended
  - Removed special energy from units which do not have a way to use it.
    Obviously, those units are also no longer affected by energy
    restoring or draining effects
  - General Martok's Cho'naQ Class vessels no longer lose their avatar bonuses
    when promoted to veteran rank
- Modding
  - Height post drawing now correctly influenced by the DISABLE_HEIGHT_CHANGE
    rts_cfg.h option


--------------------------------------------------------------------------------
Version 3.2.2                                                    26th April 2011
--------------------------------------------------------------------------------

Additions
---------
- Modding
  - A devtech techtree is now available in editor to remove all techtree-related
    restrictions for testing and development purpose. See the devtech.tt file

Changes
-------
- Borg
  - The Auto Adaption Scanner no longer attempts to scan targets which vessels
    can not adapt to

Fixes
-----
- The Centaur Class veteran ability is now called a Refitted Hyperimpulse Drive
- Game Mechanics
  - Burst fire weapons could reset their shot cycler timer unintended in games
    with high latency which resulted in firing shots too fast
  - Freetech no longer ignores faction requirements
  - General Helev's Specter Generix Class now ranks up correctly
  - Some passives were not correctly applied to special weapons
  - The Adaption Module did not always cause the adaption effect to
    nearby Borg ships
  - The Dodecahedron Relay Module subsystem icon no longer overlaps with other
    subsystem icons

--------------------------------------------------------------------------------
Version 3.2.1                                                    26th April 2011
--------------------------------------------------------------------------------

Fixes
-----
- Fixed a crash bug in the "CannonImp" weapon classlabel

--------------------------------------------------------------------------------
Version 3.2.0                                                    25th April 2011
--------------------------------------------------------------------------------

Additions
---------
- A new module for the Scout Cube is available: The Peripheral Module
- The Adaptor is no longer a mixed-tech vessel and joined the regular Borg
  fleets with a new collection of modules! A dedicated Borg mixed-tech vessel
  will return in a future patch
- An all-new Norexan Class model is ready to hunt down its prey!
- Rare Warp-Ins are now available! Uncommon starship classes might respond to
  your distress call
- A brand-new Generix Class, including a new refit. Two additional refits will
  arrive shortly
- A new Warp-In option is available: Request Strategic Support. It acts similar
  to the experimental support (the Descent Class) and yields a Teutoburg
  Class strategic cruiser
- A new weapon range beyond artillery range has
  been introduced: strategic range
- Additional new visual and audio effects
- Defense Batteries are now highlighted during footprint placement if within
  the effect radius of a Singularity Transmitter
- Improved ship AI for vessels using transporter attack
- Engine
  - Sound output device and speaker configuration can now be set independently
    from system defaults
- New maps and map redos
  - Blue Skies by Stardust
  - Broken Borders by Stardust
  - Centauri Epsilon by NeroDan
  - Duel II by Boggz
  - Just a little Frontier Run by Boggz
  - Little Little Universe by Stardust
  - Nebula Stream by Stardust
  - Plasma Conduit by Tryptic
  - Star Traffic by Boggz
  - The Final Frontier II by Boggz

Changes
-------
- "Advanced Game Options" dialog layout cleanup. "Default Settings" button
  available again.
- Lossy sound decoding from MP3s during installation removed. Voice-over sounds
  are now distributed in OGG format, sound effects are distributed in WAV
  format.
- As per Windows guideline for versions Vista and newer, no start menu entries
  are created on these systems anymore. Fleet Operations registers with
  the Windows Game Explorer
- General Gameplay
  - Beaming boarding parties to an enemy vessel now aborts if the vessel
    is cloaking and out of range of a cloaking sensor
  - New Veteran and Officer abilities were added or altered
  - Single Stage Energy Conduits now reduces the resistance against
    weapon disabling effects by 50 percent (bf.: 80)
  - Ablative Armor Plates now reduces the repair rate by 50 percent (bf.: 20)
  - Scouts are no longer selected along with your other military vessels
    if drawing a selection box
  - It now takes 25 seconds to deploy fighters (bf.: 18 seconds)
- Borg
  - The Beam Module for Scout Cubes got replaced with the new Peripheral Module,
    including a new special ability to activate bonus effects on nearby Scout
    Cubes with different modifications
  - A subsystem icon has been added to the Relay Module for
    easier identification
  - Reduced the hibernation time of Refresh Cycles to 10 seconds (bf.: 25)
  - Reduced the chance to cause subsystem failure while in a nanite field
  - Reduces the amount of crew assimilated per second by the Auto-Assimilator
    to 2 / 4 / 6 (bf.: 4 / 8 / 12) but reduces its energy
    costs to 50 (bf.: 115)
- Dominion
  - The Hyperspace Artillery has been changed to strategic weapon range
  - Cascade Feedback now drains 100 energy (bf.: 50) and deals 30 percent
    additional damage
  - T-15 Heavy Cruiser officer ability Implosion Field Device now deals 12
    percent less damage
  - Tetryon Disruptor now deals 16 percent less damage
  - Coreblast disabling effect duration increased to 14 seconds (bf.: 10)
  - The native carrier increase in supply costs were not correctly applied to
    the B-5 Battle Cruiser. This has been fixed, resulting in increased
    supply costs (about 20 percent)
  - Slightly reduced the movement speed of the B-5 Battle Cruiser
  - Increased the C-11 Interception Destroyer movement speed, removed its
    weapon arc requirement and slightly increased its costs
  - Reduced damage dealt by the Hyperspace Artillery by about 10 percent
    and increased its supply costs
  - Workerships may now directly built in supply mining mode
  - Collide no longer requires active weapon systems
  - Captured Hyperspace Sensor Systems may no longer use Deep Scan if the
    capturer is not of the Dominion faction
  - Hyperspace Artillery damage to vessels has been reduced by about 55 percent,
    but damage against stations has been increased by about 25 percent. Also
    reduced the effect radius of the damage-over-time component. Supply costs
    reduced to 40 (bf.: 60)
  - Tetryon Disruptor now has a 20 second cooldown
- Federation
  - Sensor Synergy is now considered a Synergy mechanic
  - EC3M now also reduces damage bonuses of hostile ranged-based passives
    by 33 percent
  - Shield Reset is now disabled while the veteran ability Harmonics Burst
    is charging
  - The Steamrunner Class has been removed from Warp-In. It will be available
    in a different role in one of the next patches
  - Slightly reduced Ambassador Class movement speed
  - Tricobalt Torpedoes now deal 50 percent increased damage to stations
    (bf.: 100), but cause area damage
  - Sixth Generation Shield Generators resistance reduced to 6 / 12
    percent (bf.: 12 / 24)
  - Reduced damage dealt by Proximity Torpedoes by approximately 33 percent
    against medium and 50 percent against large targets. Damage dealt is now
    considered torpedo damage
  - Hyper Impulse Drive now reduces the Offensive Value by 3 (bf.: 2) and has
    a slightly reduced chance to cause the Cover Fire effect
  - Nebula Class variants are now identified with a subtype name relating to
    their construction authority
  - Adjusted the chances at which warp-in vessels arrive. It is no longer
    possible to receive 2 heavy cruisers or battleships at once
  - Captured Starfleet Command stations may no longer use warp-in if the
    capturer is not of the Starfleet faction
  - Captured veteran Nebula Class vessels of the San Francisco Type may no
    longer use warp-in if the capturer is not of the Starfleet faction
  - Sensor Synergy and EC3M switched places. Sensor Synergy is now the level
    2 ability and EC3M the level 1 ability
  - It is now possible to upgrade multiple Newton Class vessels by clicking on
    the refit button while the ships are selected
- Klingon
  - Klingon Cloaking Devices do no longer consume energy to be activated
  - Field of Fire now deals 50 percent increased damage compared to the
    normal Cluster Torpedoes
  - Compound Torpedoes damage increased by 20 percent
  - The Vor'cha Class now has a limited attack arc and its costs were
    adjusted slightly
  - Captured MoQbara' Stations may no longer use battle plan if the capturer
    is not of the Klingon faction
  - The BortaS abilities switched places: Attack Probe is now the level 1
    ability, Advanced Cloak the level 2 ability and Ion Storm the level 3
    ability. Reduces the energy costs of Artificial Ion Storm
    to 612 (bf.: 810)
  - BortaS Class defensive value increased to 26 (bf.: 22). Costs
    adjusted slightly
  - Adjusted the effect range of the ion Storm to better match the visual effect
- Romulan
  - Adjusted most Romulan construction times and costs
  - Multi Targeting Disruptor is now researched at the Tal'Shiar Academy
  - Vector Torpedo is now researched at the Tal'Shiar Academy
  - Generix Refits are now researched at the Upgrade Center
  - Generix Refits do now take a few seconds to be installed. Upgrading is only
    available while cloak is offline and renders the Generix vulnerable during
    the process
  - Unstable Plasma Torpedo no longer has a limited firing arc to be used and
    the engine disabling effects now lasts 8 seconds (bf.: 5)
  - Stealth Field now reduces damage received by opponents by 15
    percent (bf.: 10)
  - Silent Resolve now deals 55% increased damage
  - Chain Reaction Charger damage reduced by 33 percent. In addition, Chain
    Reaction Charger now add a 20 percent reduced resistance against
    medium range vessels
  - Adjusted the damage dealt by Sensor Jammer against vessels with
    extreme sensor ranges
  - Increased the spike damage of the Radiation Array by 66 percent
  - It is no longer possible to transfer energy to the Intelligence Center by
    using special weapons like Energy Beacon
  - Captured Intelligence Stations may no longer use their abilities if the
    capturer is not of the Romulan faction
  - Metaphasic Disruptor now has a 10 second cooldown

- Fixes
  - Players were unable to join a game when a host player was in the map
    selection or advanced game options dialogs
  - Map previews created in PNG format would not show up in the Game Setup
    screen
  - Graviton Torpedoes are now correctly displayed on all graphic configurations
  - Multimedia pack shell menu music would not play
  - All units should respond with their correct voicesets again
  - Fixed a crash when saving a game
  - Fixed missing loading-tip images
  - Command-line parameters passed from the launcher to Armada got truncated
  - Loader should be more AV false-positive resistant
  - No sound was played when a multiplayer chat message is received in-game
  - Fixed a minor Armada 2 shell UI font rendering bug
  - In some circumstances audio was muted after a new game has been started
  - A blank preview image was generated when a custom map has been saved from
    the 'escape shell'
  - "Hardware Vertex Processing" and "Bump Mapping" settings were not saved
    between sessions and always got reenabled
  - Fixed a crash when exiting Fleet Operations
  - Game Mechanics
    - Some passive abilities (especially defense oriented ones) yield larger
      resistances than intended
    - Newton Class vessels are no longer renamed when upgraded
    - Cover Fire will now only produce one buff icon
    - Warp-In ships built at the McKinley Yard do no longer costs supplies
      if destroyed
    - Some inconsistencies were removed which caused a few damage modifiers
      to not stack correctly
    - Some subsystem disabling effects lasted too long
    - The Cover Fire effect no longer resets the cooldown of the Hyper
      Impulse Drive
    - In certain circumstances, an area effect weapon could have missed
      a moving target
    - Singularity Transmitter Field now shows the correct range when its
      subsystem icon is hovered
    - Industrial Field Replicators system icon is now always displayed correctly
    - TaQ'roja's Heavy Disruptor on Kahless Stations can no longer
      unintentionally be manually controlled
    - Imperial Stance does now correctly grant +3 System Value as mentioned in
      the tooltip
    - The Newton Class no longer incorrectly counts as a medium range ship
    - Dominion Prototypes and some other vessels were not correctly affected by
      some special abilities
    - Shield repair capable repair ships no longer try to repair a cloaked
      vessel's shields
    - General Martok's Cho'naQ Class vessels now use the correctly reduced
      energy costs while Energy Buffer is active
    - B-5 Battle Cruiser prototypes had incorrect costs
    - The T-15 Heavy Cruiser was not affected by the Ablative Armor Plates
      repair rate reduction
    - The Newton Class is no longer considered a medium range vessel
    - Sensor Synergy did not obey the 5 second cooldown for special weapons
    - Space key may again be used to jump to recent events, like being under
      attack
    - Passive abilities which trigger an effect in certain time intervals are no
      longer reset when a ship ranks up or is promoted to veteran rank
    - Some subsystem icons were messed up while Last Ditch Assault was active
    - Allied repair upgrade could when queued be purchased several times on one
      shipyard
    - The fighters of Dominion Puretech B-5 Battle Cruiser are now correctly
      considered medium range for damage calculation formulas
    - Fixed a rare crashbug which could be caused while Newton Class vessels
      are being refitted
    - Rare crash fixed when changing team on a Ferengi Marauder
    - Breen S-2 Escort Cruiser no longer loses its avatar bonus if ranking up
    - It is no longer possible to call in Descent Class vessels via warp-in
      while the warp-in cap is full and to have them warp-out immediately
      after arrival
    - Cloaked ships now get properly decloaked when entering a shipyard queue
      in all circumstances
    - The extra footprint on Starfleet Outposts is now at the correct direction
    - The mixed-tech Talon Class now uses the correct repair rate
    - The Nova Class now has the correct sensor range
    - Officer Avalon Class Elite Squadrons do now correctly obey damage
      reduction from weapon range based passives
    - Fixed some overlapping subsystem icons at the Vutpa' Class
- Modding
  - The small blocking trigger no longer causes crashes
  - WormholeGenerator weapon can now be fired again

--------------------------------------------------------------------------------
Version 3.1.5                                                 25th December 2010
--------------------------------------------------------------------------------

Additions
---------
- Additional weapon effects have been added or redone, including more
  perceptable effects for Cover Fire and similar effects
- Ingame Multiplayer chat sound notifications
- Modding
  - The GUI files (data/misc) now contain prefab settings for three avatar
    buttons. Check the aspect-ratio-specific GUI files for
    reference (gui_4x3.cfg, gui_5x4.cfg, ..)

Changes
-------
- Increased the time a carrier requires to replace lost fighters to
  18 seconds (bf.: 10)
- For the purpose of damage calculation, fighters are now treated as having
  the weapon range of their carrier. This does not affect their actual
  weapon range
- Replaced shell menu button hover and click sounds
- Fluidic Nebulas no longer hide vessels inside
- Dominion
  - Slightly reduced the movement speed of Workerships
- Federation
  - A new level 3 ability for the Remore replacing ECM: Point Defense
  - Vessels with modified Photon Torpedoes are now also affected by Fleet
    Supply, the Nova Class special
- Klingon
  - General Martok's Kahless Station is now armed with a Micro Photon Torpedo
    instead of a Torpedo Drone
- Modding
  - Renamed Stock A2 ODF commands have been reverted

Fixes
-----
- Stopped some incorrect Borg Uplink spawning in Borg bases
- The Nova build button is now always visible as intended
- Fixed several tooltip typos
- Fixed a rare crash when selecting a vessel
- Fixed a rare crash when using a special weapon
- Fixed a crash when using custom maps with empty map descriptions
- Fixed a crash when trying to sort the map list with very large custom maps present
- Game Mechanics
  - Mixed Tech caps are working again
  - The Carriers, Troopships and Repair Ships caps may no longer be
    exploited by transfering them to an ally
  - Fixed some messed up weapons on Taq'rojas Bird of Preys
  - The accuracy of Quantum Pulses is now correctly reduced while Blockade
    Breaker is active
  - Transwarp now shows a range indicator
  - Dominion and Romulan Adaption Modules are now correctly displayed
  - Veteran S-2 Escort Cruisers no longer disable the subsystems on units
    without a torpedo launcher

--------------------------------------------------------------------------------
Version 3.1.4                                                 30th November 2010
--------------------------------------------------------------------------------

Additions
---------
- Many new officer and veteran abilities have been added or existent ones have
  been reworked.
- Important gameplay features of passive abilities are now summerized in their
  tooltips similar to the damage and reload time on regular weaponry
- Several new voicesets have been added
- Improved fighter carriers: Fighter squadrons can now be controlled
  through their carriers
- Fighter squadron ship AI vastly improved
- AI Warp-In usage improved
- AI Players may now recover from their main base being destroyed
- Improved the way weapon system icon reload times are displayed
- Game observers are now able to see the amount of resources a team has
- Engine
  - Replaced the integrated sound engine with FMOD Ex
  - Armada 2 now utilizes RAM more efficiently on x64 systems
  - Windows 7 specific enhancements
- Modding
  - Increased number of available ship display system icons to 32 (bf.: 20)
  - GameEvents can now handle up to 64 race voice soundsets instead of 16
  - Map Editor: Map preview is now generated at all screen
    resolutions (Use Ctrl+s to save)

Changes
-------
- Many special weapons which are not directly using weapons for their effect
  are no longer disabled if a starships weapon subsystem fails
- Vessels do not block station constructions anymore
- Most technological vessels (including Borg ships) may now be repaired again.
  The repair rate of a repair ship is now reduced by 50 percent on alien
  vessels (which do not belong to the same faction as the repair ship) and
  75 percent on Borg vessels. Range of the repair beam has been increased to
  short (bf.: dogfight)
- Borg
  - Incubation Center resource costs and supply rate increased by 50 percent
  - Transmission Matrix no longer has technology requirements
  - The EM-Field effect of Scout Cubes is more reliable in detecting
    cloaked vessels
  - The Holding Beam Module does now also increase a vessels maximum drone
    capacity
  - The Dodecahedron does now feature the same relative energy regeneration
    rate as all support ships
  - Reduced the damage reduction from adaption modules to 15 percent (bf.: 35)
- Dominion
  - Reduced the attribute bonuses of the Auto-Repair and Maintenance Systems
    officer ability to 2 / 4 (bf.: 3 / 6)
  - Decreased the base damage of the Implosion Field Device officer ability,
    but it is now more reliable. Damage does now correctly increase upon
    rank up
  - B-8 War Frigates are now equipped with civil shuttlebays, allowing them
    to support fighters if their carrier should be lost. Vessels with civil
    shuttlebays do not launch fighters on their own
  - The B-5 Battle Cruiser is now equipped with a new special
    ability: Bio-Warheads
  - Increased B-8 War Frigate construction time
- Federation
  - You may no longer receive two Steamrunners at a time via warp-in
  - Nova Class Fleet Supply now fires additional photons instead of
    increasing the weapon rate of fire. Now affects 4 targets besides
    the Nova (bf.: 5) but no longer affects less in certain situations
  - Admiral Risner's Akira and Sovereign Class battleships are now equipped
    with civil shuttlebays, allowing them to support fighters if their carrier
    should be lost. Vessels with civil shuttlebays do not launch fighters on
    their own
  - A new Hyper Impulse Drive as been added. Now increases the efficiency of
    the Tiny ECM Suite and gives attacks of the Saber a chance to provide cover
    fire for Saber, Canaveral and Excelsior Class ships, allowing them to avoid
    all weapon fire for 4 Seconds. If a Saber with deactivated Hyper Impulse is
    affected, its Offensive Value will increase for the same duration
  - Starfleet Command now costs 75 supplies to be built to equip the many offices
  - If a warp-in ship is destroyed, you will now have to pay some supplies
  - New Field Diagnostics upgrades for the Newton Class. Level 1 increases the
    service efficiency on friendly non-Federation targets and improves the shield
    generators, level 2 increases the overall repair rate and increases the system
    value and level 3 allows the Newton to restore shields. No longer
    requires research
  - Reduced the Tactical Torpedo Pod energy regeneration rate boost by 25 percent
  - Reduced the combat efficiency of several defense platforms
- Klingon
  - The chance to disable an engine per volley now remains the same for a veteran
    K'Vort and no longer increases due to the veteran ability
  - Field Yard Defensive Value increased to 135 and System Value increased
    to 30 (bf.: 100 / 23)
  - Field Yard Extensions Defensive Value reduced to 50 and System Value reduced
    to 11 (bf.: 75 / 17)
  - Dispatching a Topmey freighters storage containers now grants the resulting
    K't'inga Class a temporary attribute bonus. If the K't'inga levels up while
    the effect is active, it will be granted a permanent attribute bonus.
    Dispatching storage containers is only available while in the vicinity of
    a Klingon Mining Station
  - Increased the amount of energy drained by the Draining Disruptor Cannons
    officer ability to 6 / 12 (bf.: 3 / 6)
  - Weapon Fatigue no longer reduces the experience required to level up, but
    now grants a chance to immediately rank up if the effect ends
  - Adjusted the attribute bonuses K'Vorts gain from ranking up
- Romulan
  - The Intelligence Center's Spy ability no longer 'pulls' ships
  - Reduced the supply costs of Prefab Defense Components to 20 (bf.: 50)
  - Reduced the chance to disable engines with the Generix veteran ability
    Arlterium Disruptor Lense to 20 percent (bf.: 50)
  - Slightly increased the Generix Spectre Refit costs
  - Slightly increased the Rhienn Disruptor Refit costs
  - Reduced damage and energy costs of the Multi Targeting Disruptor

Fixes
-----
- Fixed a bug causing out of sync problems in multiplayer games
- Fixed a crash bug on Dominion veteran Dreadnoughts and V-15
- Fixed several tooltip typos
- Missing entries to the map edit menu fixed
- Fixed a crash with multi weapon requirements (fixes map object placable
  Prometheus)
- Fixed overlapping icons in the starship subsystem display
- Minor memory leak fixed
- Under certain conditions, vessels in waiting queues (repair, decommission,
  mining) could cause a crash
- The cloak hotkey is reenabled again
- Fixed several "double click selection" issues
- Map Editor: Fixed a crash when editing an object's label
- Modding: The PointDefenseLaser crashed the game
- Armada 2 Bug: Transwarpdrive jump location bug fixed
- Armada 2 Bug: Special weapon buttons were visible on Ferengi Marauders
  for observers
- Game Mechanics
  - Freighters should no longer try to attack vessels
  - Dominion Puretech Warfrigates may now be built correctly and is affected
    by its veteran ability as intended
  - Romulan silent resolve deals damage as intended
  - Admiral Risners Sabers no longer receive Admiral Maysons defense buff
  - On rare occasions ship experience points could get wrongfully accounted for
    vessels actually destroyed by a nebula but the shooter
  - The Federation Veteran ability Extended Engineering is now correctly
    working on Avalon Class Veterans and no longer affects shield regeneration
    as intended
  - The Klingon Officer ability Armor Piercing does now correctly increase its
    damage bonus on officer rank 2
  - The energy regeneration bonuses of Tactical Torpedo Pod was incorrectly
    applied to Nebulas which are not under the effect of the special

--------------------------------------------------------------------------------
Version 3.1.3                                                     14th July 2010
--------------------------------------------------------------------------------

Features
--------
- Tunngle peer-to-peer VPN client auto login support (www.tunngle.net)
- Vutpa' Class vessels may now be equipped with the Concussion Missile
  Launchers special ability
- Troopship Vutpa' Class refits may now be equipped with the Mass Seeking
  Missiles special ability to reveal cloaked ships
- A new special weapon for the B'rel Class Bird of Prey has been
  added: Weapon Fatigue
- Starfleet reinforced Warp-In with new vessels
- AI now uses cloak more offensively if appropriate
- AI ship and special weapon handling improved
- The Shrike Class may now be equipped with Unstable Plasma Torpedoes
- More weapon and special ability effects have been added
- Weapon range circles are now also shown when hovering a weapon system
  of a vessel
- New maps
  - Solar Outposts by Beat2k10
  - Champagne Supernova by Boggz
  - Early Bird by Boggz
  - Star Traffic by Boggz
  - Andromeda v2 by funnystuffpictures
  - Booby Trap by Njm1983
  - Passages by Njm1983
  - Solar Storm by Phoenix
  - Tau Ceti by Phoenix
  - Battlefield by Stardust
  - Nebula Stream by Stardust
  - (Phoenix's Map Making Extension is now integrated)

Changes
-------
- Game Shell locked at window mode
- Multiplayer: The button for the connection dialog of the antiquated IPX/SPX
  network protocol has been removed. IPX/SPX is still accessible by right
  clicking the 'Local Area Network' connect button.
- Multiplayer: The button for Internet connections over GameSpy has been
  removed. GameSpy is still accessible by right clicking the 'Tunngle'
  connect button.
- Multi-monitor action window mode has been removed
- Cloaking hotkeys have been disabled due to technical problems.
  They will return soon
- System Value has now a greater effect on weapon disabling weaponry.
  Starships with high System Values are more resistant to losing their
  weapon control
- Borg
  - Stacking Interception Modules on the Dodecahedron will no longer affect
    stations and will be less effective against vessels designed to
    counter the Dodecahedron
  - Adjusted the damage coefficients of Adapted Photon Torpedoes
  - Increased the rate at which Assimilation Incubation Centers board attackers
  - Nanites damage and area of effect reduced. It is now much more effective
    in affecting hostile engines to slow fleets down. Cloaked vessels will
    also suffer from nanites. It is now on a 12 second cooldown
- Dominion
  - Mixed-Tech C-11 Interception Destroyers (Romulan) are now capped like
    Generix Class Support Refits
  - Phaser Sentry attributes increased to 34 Offensive Value, 38 Defensive
    Value and 20 System Value (bf.: 25 / 29 / 15). Costs increased
  - Puretech now reduces the construction costs of all Construction Yard
    prototypes by 50 percent (bf.: reduced the construction time of most
    construction yard vessels by 25 percent)
  - Reduced the construction costs of Breen Battleships significantly
  - Breen Energy Torpedoes on Breen Cruisers now have a limited attack arc.
    The construction costs were adjusted to reflect this change. Slightly
    adjusted the damage coefficients of all Energy Torpedoes
  - The Hyperspace Artillery is now time-based on a fast cooldown
    (bf.: energy-based)
  - Tetryon Singularity is now considered a torpedo weapon
  - The T-15 Heavy Cruiser Implosion Field Device is now capped at a
    maximum effect
- Federation
  - Increased the damage per second of Admiral Mayson’s Torpedo Platform
  - Ablative Armor has been moved from the Phaser Platform to the
    Pulse Platform
  - Increased the construction time and costs for Akira Class vessels
  - Hyper Impulse Drive offensive value reduction reduced to -2 (bf.: -3)
  - Admiral Risner now increases the Offensive Value of Saber Class vessels by
    1 and by an additional point for every two ranks
  - Damage bonuses granted from Admiral Mayson’s altered armaments have been
    converted into Offensive Value increases for better comparability.
    Gameplay characteristics remain unchanged
- Klingon
  - Some minor cost adjustments on most Klingon units
  - Polaron Field disables less systems per second
  - The cloak detection of Manual Targeting now lasts 7 seconds (bf.: 6)
  - Heavy Disruptor and Torpedo Drone are now on a 1 minute cooldown
  - The Vutpa' Class and the Heavy Weapons Platform are now armed with a
    new Photon Rack weapon
  - The Vutpa' Class gains higher Defensive Values on rankup
  - The veteran ability Battle-tried Energy Circuits now also adds Manual
    Targeting to the Vor'cha, LuSpet and Cho'naQ Classes
  - Increased TaQ'roja's B'rel Class disruptor damage
  - The Kahless Stations may now build the Qaw'Duj Class instead of K'beajQ
    warships for an increased construction time
  - Manual Targeting now also reduces the drawbacks of offensive or defensive
    passive abilities by about 50 percent
  - Adjusted the amount of supplies granted on unit rank-up. You should see
    an overall increase. Also reduced the costs to buy more supplies
    in early game stages
  - Field Yard extensions are now specialized in producing certain vessel
    subsets. Costs and technology requirements adjusted. The Field Yard is
    built with an installed own-vessels-only repair dock and one of the
    extensions offers a normal repair dock with allied repair capabilities
  - SuS'a' Class damage reduced while Last Ditch Assault is active (the damage
    per second is still higher compared to normal operation mode). Increased
    the 'drawback phase' of Last Ditch Assault to 20 seconds (bf.: 10)
  - Reduced the construction time of most early game units
  - TaQ'roja's B'rels yield additional supplies during combat
  - Reduced the number of subsystems disabled by the Scrambling Probe
  - K'Vort officer and veteran ranks no longer deal direct hull damage because
    of Verteron Particle Loader
- Romulan
  - Slightly increased the relative damage per second of Defense Batteries
    while in alternate fire mode
  - Prefine Defense Components renamed to Prefab Defense Components
  - The Generix Support Refit gains higher Defensive Values on rankup
  - Defense Battery attributes increased to 28 Offensive Value, 36 Defensive
    Value and 25 System Value (bf.: 21 / 27 / 19). Costs increased
  - Sleeper Agent energy costs and supply costs reduced
  - Saboteur Agent supply costs reduced and energy costs increased, but no
    longer yields experience and has a reduced duration, use Sleeper Agent
    to gain higher levels. The Saboteur no longer destroys large units like
    Borg Cubes, but may now also damage stations
  - Multi Targeting Disruptor hits one target less and deals slightly
    less damage
  - Slightly reduced the Generix Class Spectre Refit torpedo damage reduction
  - Quadcobalt Torpedoes are now considered torpedo weapons
  - Shield Recharge is now short ranged (bf.: medium)
  - Generix Class Support Refit costs increased
  - Increased the supply costs for the Rhienn Disruptor Refit
  - Defense Batteries may stay online for a longer period of time if
    disconnected of a Singularity Transmitter
  - Rhienn Class Phase Plates now also add a resistance against Adapted
    Photon Torpedoes
  - Radiation Device no longer hits cloaked targets

Fixes
-----
- Fixed a crash with research stations having more than 14 buildable upgrades
  (Admiral Mayson's Starfleet Science)
- Fixed an old Armada bug that caused shipyards to loop their repair sound
  despite being idle
- Added alternate fullscreen mode when using the Map Editor to prevent dialog
  boxes from being invisible
- The Romulan Singularity Implosion special ability shows the correct area of
  effect if hovered over
- Fixed some tooltip typos
- Map Editor: Maps can now be correctly sorted by their size
- Game mechanic
  - Derelict vessels could end up with activated cloak under certain conditions
  - Federation warp-in vessels aren't transferred to an already eliminated
    player anymore
  - Fixed a problem with "growing" repair facility queue entry points
  - Romulan defense batteries won't decloak themselves anymore when their
    operation mode is changed
  - Officer rank Novas can no longer be promoted to Veteran if the veteran cap
    is full
  - The Serkas Class' Silent Resolve special ability will no longer cause it to
    decloak in certain situations
  - Nova Class vessels are no longer double-click-selected with Saber Class
    vessels
  - Support Generix refits are now capped correctly
  - Ships built at the McKinley yard now correctly count towards the veteran
    limit
  - AI derelicts shouldn't switch their operation mode anymore as if still
    commanded
  - The Borg EM-Field may no longer be exploited by firing on allied vessels
  - Fixed some slightly incorrect supply costs for "direct build" Romulan
    Generix Class refits
  - The Dominion Perimeter has artillery range weapons as intended
  - Solved some issues with Klingon SuS'a' Class Veterans using their special
    abilities
  - The cooldowns of time-based special ships (like the Descent Class) could
    be shortened by exploiting energy-restoring weaponry
  - General Helev's supply cost reduction is now correctly taken into account
    for Rhienn Class refits directly built at the Staryard
  - Fixed several bugs triggering a hidden cooldown on veteran promotion
  - The SuS'a' may now be promoted into the ranks of a veteran if Last Ditch
    Assault is active
  - Fixed a bug related to the Hyperspace Artillery dealing too little damage
    on some rare occasions

--------------------------------------------------------------------------------
Version 3.1.2                                                       3rd May 2010
--------------------------------------------------------------------------------

Features
--------
- New weapon and impact effects
- Armada Game Engine fixes to improve overall game performance and memory
  consumption
- Federation Venture Class scouts have been refitted with special sensor
  beacons, allowing them to engage progressive scan patterns
- Dominion A-4 Scouts may now drop special Passive Probes
- Romulan Defense Batteries are now powered by a Singularity Transmitter that
  will also regenerate energy on nearby friendly Romulan vessels. There
  are also rumors about particle weapons being deployed in the Romulan
  defense grids
- Admiral Mayson is now deploying a squadron of Nova Class vessels to
  assist the Fleets
- A new Borg Dodecahedron has been spotted assisting the assimilation
  capabilities of Borg fleets. The collective also adapted the Scout Cube
  by adding a new module
- Dominion Sensor Stations may now activate the Remote Tachyon Beacons of
  friendly vessels
- Hovering special weapons when having a group of ships selected now shows
  the appropriate subsystem icon in the ship group display
- Station construction placeholders can now also be seen by allied players
- Model or texture redos
  - Romulan Defense Battery (formerly known as Disruptor Turret)
- New maps
  - Duel by Boggz
  - The Final Frontier II by Boggz

Changes
-------
- Adjusted some parameters in the balancing formulas, which caused incorrect
  calculation of movement speeds and some other attributes. That causes
  adjustments of most unit speeds and some slight cost changes
- More special weapons will now show their area of effect when hovered
- Constructors have tractor beams now to rescue damaged ships from
  the frontlines
- Reduces research time and costs for Analyze Alien Technology
- Certain temporary crafts like fighters will no longer be repairable
  by repair ships
- Borg vessels are far to complex now for repair ships to repair
- Replaced the construction delays with a "halt construction" button that
  will stop the queue permanently until canceled
- Reordered icons in the menu palette to consume less space
- Adjusted the effectiviness of some passive abilities, Single Stage Energy
  Conduits do now also increase the resource costs. Passive abilities that
  include a damage vulnerability are more effective compared to their
  counterparts. High Density Shield Generators and Ablative Armor Plates do
  now also include a small damage reduction against short / long range
  targets. The Autonomous Defense AI will now also reduce the combat
  effectiveness against support ships
- Changed the targeting priorities for several vessels, including fighters
- Reduced the Defensive and System Values of fighters
- The "Normal AI" is now less difficult to beat to be more beginner friendly
- Troopships no longer need to lower their shields to transport.
  Troopship cap reduced to 6 (bf.: 10)
- Reduced the construction costs of experimental yards to 600 dilithium
  and 150 tritanium (bf.: 800 / 200)
- Tachyon Scan has been removed on all sensor stations. All factions got
  other methods to track cloaked vessels in the field. Stationary Tachyon
  Pings are still available and the respective structures are cheaper
  to produce
- Area weapons that do not have an efficient effect on temporary crafts
  will no longer count them as valid targets and try to find other, more
  valuable victims (like Distortion Field on fighters for example)
- Added some additional info to the tooltips of burst-volley and
  multi-target weapons
- Adjusted the construction time of mixed-tech vessels
- Most aim-to-fire special weapons to no longer have limited attack arcs
  to ease their manual usage
- Reduced the Offensive Value of all scouts, but increased their
  Defensive Value
- A new hotkey is available for cloak
- Borg
  - Assimilation will now cause Incubation Centers to slowly assimilate
    hostiles (bf.: granted Matrix Teleport)
  - Borg vessels may no longer be recrewed like stations. Allied
    vessels are now automatically recrewed if close to an
    Incubation Complex
  - Matrix Teleport is now available to all avatars on incubation centers
    and restores a large amount of drones to all friendly ships at the
    target location
  - Borg Adaption Matrixes do now have proper Borg-like designations
  - Increased the damage reduction granted by stacking Beam Modules
  - Perfect Logistics generates resources much faster now
  - Dilithium and Tritanium Synthesis generates resources much faster now
    and the supply-resource transmutation rate has been improved
  - Adaption resistances granted from mixed-tech modules have been increased
  - Scout Cube System Value and construction costs reduced
  - Adaptor System Value and construction costs reduced
  - Establish Energy Nodes research costs adjusted
  - Slightly increased the Holding Beam boarding rates
  - Bring order to chaos damage boost reduced to 70 percent (bf.: 100)
  - Sensor Relays no longer emit Tachyon Pings
  - Graviton Relay has been removed. The Dodecahedron may transform into a
    defense station if a Relay Module is installed
- Dominion
  - Puretech: Now increases all attributes by 3 (bf.: 6), but also affects
    the S-7 Defender. The construction-time-reduction is more effective now
    due to formula changes, but no longer affects the war frigate
  - B-8 Warfrigate construction costs and construction time reduced
  - B-5 Battle Cruiser fighter carriers now support one less fighter.
    Prototypes only support half the fighters of regular carriers
  - Added a better display for the Worker Ship modes to keep track on
    cooldowns and the current state
  - Perimeter ping range increased to medium. Construction costs adjusted
  - The Sensor Station is now equipped with Graviton-Tachyon Pings
  - The Phaser Sentry is now equipped with a new passive ability called
    EPS Control Nodes, increasing shield damage
  - Phaser Sentry and Perimeter construction time reduced
  - T-15 Heavy Cruiser gains Ablative Armor. Construction costs adjusted
  - Cascade Feedback damage increased, special energy drain chance reduced
  - Breen Torpedoes will now deal increased damage to battleships and reduced
    damage to small ships
- Federation
  - Admiral Mayson: ACS Torpedoes have been converted into a passive ability
    and therefore no longer requires research. The construction time
    reduction has been removed, but all defense Plattforms are now equipped
    with altered weaponry
  - The Eraudi Yard no longer requires Star Fleet Science to be built. All
    Eraudi Yard vessels got their resource costs and construction times
    slightly adjusted accordingly
  - Plasma Coil area of effect reduced to match the visual effect
  - Officer rank Defiant Class vessels deal now more damage with their
    Quantum Torpedoes, but slightly reduced damage with their Pulse Phasers
  - Probe (Canaveral Class level 1 ability) has been replaced by Sensor Synergy
  - Tachyon Scan (Canaveral Class level 2 ability) has been replaced by EC3M
  - Sensor Research has been moved to Starfleet Engineering and special
    weapon research is located at Starfleet Science again
  - Excelsior II Class construction costs reduced, but Quantum Torpedoes
    are now on a limited attack arc
  - Guided Quantum Torpedoes converted into an active ability
  - Hyper Impulse Drive now also reduces the combat effectiveness of short
    range attackers, but now reduces Offensive Value by 3 (bf.: 2)
  - Avalon Class fighter carriers now support one less fighter
  - Dual Pulse Phasers replaced with normal pulse phasers. Gameplay
    characteristics remain unchanged
  - Starfleet Command now requires Starfleet Engineering and Starfleet Science
    to be built (bf.: just Starfleet Science)
  - The Torpedo Platform is now a baseline turret and available to all
    avatars. It is now armed if Photon Torpedoes per default
  - Sensor Platform ping range increased to medium. Construction costs increased
  - Assault Mode is now on a 30 second cooldown
- Klingon
  - General Martok Defensive Value bonuses reduced to 4 / 3 / 2
    (bf.: 8 / 6 / 4) but now reduces the supply costs of yard
    extensions by half
  - Shield strength reduced, hull strength increased
  - B'rel Class and the Vutpa' Class are now equipped with manual targeting,
    tracking down cloaked targets. Construction time and construction costs
    adjusted
  - K'beajQ construction time adjusted
  - K'Vort construction time adjusted
  - SuS'a' construction time adjusted
  - Veqlaragh construction time adjusted
  - Taqrojas Bird of Preys deals now more damage with its torpedoes and
    reduced damage with disruptors
  - Ion Torpedoes reload times and damage adjusted. They do now deal
    variable damage with a chance to deal high extra damage on a
    successful hit
  - Micro Photon Torpedoes replaced by normal Photon Torpedoes, but they
    will now fire faster
  - The Negh'var Class is now armed with Photon Torpedoes and gains
    Ion Torpedoes on officer rank
  - The Chor Class is now equipped with a special civil disruptor defense
    weapon, which is more effective against early destroyer attacks
  - The NoQ'Duj Class does now also deal increased damage to
    fighters. Sensor range reduced
  - Reduced the research costs and research time of Large Scale
    Cloaking Devices
  - Compound Torpedoes damage increased
  - Cho'naQ Class damage increased
  - Polaron Field now only affects up to 5 targets
  - Scout Station ping range increased to medium
  - Adjusted the construction time of Field Yard extensions
- Romulan
  - General Mijural: Now increases the Defensive Value of Defense Batteries
    by 5 and increases the Defensive Value of Singularity Transmitters by 10.
    Defense Batteries remain active for a longer time span if disconnected
    from a Singularity Transmitter (bf.: Extra Plasma Torpedo and bonus
    to all attributes)
  - Reduced all Dilithium and Tritanium costs
  - Shrike Class construction costs and construction time reduced
  - Shield strength reduced, hull strength increased
  - The Rei'karansu Covert-Ops Team no longer substracts marines from the
    Tavara's crew. The Tavara may no longer be field recrewed.
    The Covert-Ops Team is now in a 20 second cooldown and consumes
    400 energy (bf.: 250)
  - If you researched a refit for Generix or Rhienn Class vessels,
    they will now also be directly available at the yards
  - Metaphasic Disruptor damage reduced and special energy costs adjusted
    slightly. Is now a target-ability
  - The Talon Class is now equipped with an advanced cloaking device.
    Sensor range reduced
  - The Spy Satellite has been removed. The Singularity Transmitter will
    now function as a long-range sensor station. Use refitted Talon Class
    vessels for Tachyon pings. Intelligence Report has been moved to
    the Talon Class refit and it is now equipped with
    Graviton-Tachyon Pings
  - Disruptor Turret renamed to Defense Battery. The Defense Battery no
    longer requires a Staryard to be built. Stationary Cloaking on Defense
    Batteries no longer consumes energy. Singularity Transmitters may
    not cloak
  - Radiation Device energy costs and area of effect increased
  - Eresis Class movement speed and construction costs increased
  - Suppressive Shielding effectiveness reduced
  - Spy now reveals a sensor radius around all hostile stations.
    The radius and the duration is random
  - The Generix Class Support Refit is now capped to 6 vessels.
    Attribute bonuses changed to reduce Offensive Value by 3 and
    increase the Defensive Value by 17 (bf.: 2 Defensive Value).
    Costs adjusted.
  - The Talon Refit no longer requires research
  - Prefine Defense Components construction time shortend, but no longer
    reduces dilithium and tritanium costs. Now reduces construction time
    slightly and reduces the supply costs to a minimum
- Misc and Non-Playables
  - The Federation McKinley Yard may now produce all Warp-In
    vessels uncapped
  - Unity One may now use Intelligence Report and Tachyon Scan and is
    equipped with a medium range Graviton-Tachyon Ping

Fixes
-----
- Fixed a crash when loading a savegame when vessels on the map were affected
  by special object effects like disabled shields or objects revealed
  by cloak detection
- Admiral's Log: Ships destroyed by some special abilities were not
  accounted correctly to the attacker
- Fixed a memory leak
- Solved several tooltip issues
- Breen Prototypes now also show their correct passive subsystem icons
- Fighters where invisibly counted against a fighter carrier's group
  number, making the Ctrl+Shift+(groupnumber) hotkeys ineffective
  in some cases
- Construction delays (or the new halt construction option) replaced a build
  item instead of moving it to the next queue slot in some cases
- Romulan Combat Cloak showed a 'stuck' cooldown delay on it's system icon
- Game mechanic
  - Romulan Cloak no longer consumes energy if the Leahval Class'
    Advanced Energy Reeling special is active
  - Dominion Cascade Feedback drains the intended amounts of special
    energy now
  - Klingon K'Vort Mixed-Tech (Federation) now has a correct weapon range
  - Fighters do not block construction anymore
  - Removed a station constructor exploit
  - Extra Energy Stores work as intended now
  - Klingon Scout Stations got their shield generators back
  - The chance that Graviton Torpedoes will deal hull damage now correctly
    improves with more modules installed
  - Many vessels which are refits had incorrectly high supply costs,
    like Rhienn Refits for example
  - Starfleet Warp-In reinforcements will no longer inherit the
    shield/hull damage ratio of the Starfleet Command structure
  - Fixed several cloak-detect issues with neutral targets
  - Fixed some bugs with the Federation Tricobalt Triloader
  - Romulan Vmax Projectile Weapons do now correctly increase damage
    against targets with higher Defensive Values
  - Some special weapons were no longer usable on veterans once the veteran
    cap was full
  - Klingon ranked up Call-to-Arms-Vutpa' Class troopships are now
    correctly double-click-selected along with unranked crafts
  - You should now get the correct amount of resources back from recycling
  - Cloaked stations will now decloak when the Romulan Cloaking Generator
    is destroyed
  - Romulan Stationary Cloak can no longer be exploited. There is now
    a cooldown on all cloaking devices
  - If a vessel gains a new rank while a temporary special ability is
    active, those effects will no longer be canceled
  - Vessels sent to a repair facitlity shouldn't 'forget' their given command
    anymore when they were previously part of a ship formation
  - Fixed the docking positions for many shipyards and repair facilities
  - Corrected some incorrectly calculated prices for Borg Adaptor modules
  - Dominion Tetryon-Photon Torpedo is now artillery range as intended
  - Klingon K'Vort Class Bird of Preys should no longer decloak when
    using their special ability
  - You may now refit Romulan Talon Class scouts during cease fire


--------------------------------------------------------------------------------
Version 3.1.1                                                  19th January 2010
--------------------------------------------------------------------------------

Features
--------
- There are disturbing reports about Romulans experimenting with phase armor
  plates on Rhienn Class vessels. Check the Research Institute for details
- AI Borg ship handling improved
- Subsystem icons of active-over-time special weapons will now flash to show
  their changed status
- New maps
  - Rain by Boggz
  - Malta II by Boggz
  - Hazard by Chris Bacchi
  - Allies by funnystuffpictures

Changes
-------
- Build queue delays don't abort the currently constructed ship anymore and get
  pushed on position 2 of the queue instead
- Default "instant trading" values changed to more commonly used amounts
- Multiplayer AI takeover now uses the Merciless AI
- Federation
  - Assault Mode energy costs reduced to 159 (bf.: 249)
  - Canaveral Class costs increased to 308 dilithium, 343 tritanium and 14
    supplies (bf.: 300, 285, 13) and construction time increased to 86
    seconds (bf.: 80)
  - Norway Class (Admiral Mayson) costs increased to 329 dilithium, 304
    tritanium and 14 supplies (bf.: 300, 285, 13)
  - Hyper Impulse Drive evasion boost is now more effective against
    small attackers
  - Platform construction time increased to 84 seconds (bf.: 70 seconds)
- Dominion
  - Phase Shield energy costs reduced to 332 (bf.: 346)
  - T-15 Heavy Cruiser movement speed increased and costs increased to 382
    dilithium, 107 tritanium and 40 supply (bf.: 330, 88, 34) and construction
    time increased to 80 seconds (bf.: 71)
  - A-26 Bomber movement speed increased and costs increased to 313 dilithium,
    96 tritanium and 33 supply (bf.: 286, 86, 30) and construction time
    increased to 71 seconds (bf.: 66)
  - A-20 Attack Destroyer movement speed increased and costs increased to 220
    dilithium, 70 tritanium and 28 supply (bf.: 201, 63, 24) and construction
    time increased to 61 seconds (bf.: 57)
  - S-2 Escort Cruiser movement speed increased and costs increased to 327
    dilithium, 111 tritanium and 35 supply (bf.: 313, 105, 33) and construction
    time increased to 70 seconds (bf.: 67)
  - The Hyperspace Sensor System is now equipped with a long range
    Graviton-Tachyon Ping (bf.: short)
- Klingon
  - Scout Station attributes increased to 20 Defensive Value and 9 System Value
    (bf.: 15 / 7) which allows the Scout Station to use all abilities as
    intended.
    Costs slightly increased
  - Imperial Stance energy costs reduced to 186 (bf.: 290)
  - SuS'a' Class movement speed decreased and costs reduced to 380 dilithium,
    131 tritanium and 15 supply (bf.: 415, 145, 16) and construction time
    reduced to 67 seconds (bf.: 73)
- Romulan
  - Slightly reduced all Romulan research costs
  - Auto Repair System energy costs reduced to 60 per second (bf.: 150). It is
    now the level 2 Leahval Class ability. It switched places with
    Advanced Energy Reeling
  - Advanced Energy Reeling redone: Now grants 10 Offensive Value and 10
    Defensive Value. Reduces special energy regeneration, but improves all
    special abilities with additional effects. Lasts 20 seconds and costs
    less energy
  - Metaphasic Disruptor energy costs reduced to 125 (bf.: 139)

Fixes
-----
- Fixed a crash when saving or loading a game
- Some passive ability icons were not shown correctly on Dominion prototypes
- Fixed a subsystem display issue on leveled up Federation Excelsior II Class
  vessels
- Federation Veteran Galaxy Class vessels with Type 3 Torpedoes enabled now
  still show the correct Modified Photon Torpedoes icon
- Fixed a crash loading a custom map with yard-objects
- Solved several tooltip issues
- Only up to 16 Ships could be added to a ship group by using
  ctrl+shift+(group number) instead of 30
- Fixed crash with the map "Nebula Gate"
- Fixed a minor sound file loading problem
- Game mechanic
  - Breen research now remains unlocked if a Breen vessel ranks up
  - Some weapon effects were blocking station construction while firing
  - Maysons Eraudi Yards got their 30 second delays back
  - Fixed a bug causing special abilities to sometimes consume one point
    more energy as intended
  - Several location-bound area weapons dealt too much damage
  - The initial Dominion Worker Ships are now free as intended
  - Veteran Dominion Tetryon Spread now works as intended

--------------------------------------------------------------------------------
Version 3.1.0                                                  10th January 2010
--------------------------------------------------------------------------------

Features
--------
- Game in-game interface complete overhaul
  - Brand new interface graphics
  - Detailed weapon and system information in single ship display
  - Up to 30 selectable vessels (bf: 16)
  - New Font
- Be warned, AI players may now build special units of all avatars
- Further improved AI weapon usage
- A new Klingon research option is available: Large Scale Cloaking Devices.
  Take a look at the Imperial Research for details
- As honor and experience plays an important role for the Klingons, their supply
  system has been changed: Klingon vessels will now refund a fraction of their
  supply costs with each level up. Normal supply routes upgrades are more costy
  for them. Qapla'!
- A complete new cloak and cloak detection! Detectors will now send out a
  tachyon ping, that reveals cloaked vessels. But stay alert! There are reports
  of some Romulan vessels using advanced cloaking, that is not detectable by
  normal pings
- Many passive abilities have been changed or renamed and a lot of new passive
  abilities have been added to vessels.
- AI now uses allied repair contracts
- Cease fire option now also available in Skirmish mode
- Improved 'physics' for the Dominion Attack Destroyer Collision ability
- Map Editor: Added 'simple mode' to the gameobject inspector
- Integrated online update check
- Reimplemented support for some special characters in chat mode
- Updated models/textures or complete remakes
  - Federation Galaxy Class
  - Federation Norway Class
  - Federation Nebula Class
  - Federation Fighters
  - Klingon B'rel Class
  - Klingon K'Vort Class
  - Klingon Attack Probe

Changes
-------
- Graphic settings are now set to 'high' by default. Default resolution is
  determined by chosen desktop setting.
- General
  - Stations (including turrets) will now chose their targets automatically
    and ignore given attack orders. Special Weapons will still require
	interaction of the player, of course. This change also fixes several
	issues where stations or turrets stopped to fire
  - Intelligence Report and Tachyon scan have been moved from the main
    starbases to the sensor stations (or the Borg Probe)
  - Vessels cannot be traded anymore when they are affected by a Romulan Spy
    or Klingon Graviton Mine
  - Slight changes in global construction costs and movement speeds,
    especially on short-range vessels, in order to give those units a better
	stand against long-range-kiting. Many short range vessels are much faster
	now, in order to give each faction certain anti-kite vessels. Scouts move
	faster now too
  - Fighters are now launched automatically and will guard their carrier.
    You do no longer need to manually launch fighters and they don't cost
	resources anymore
  - Most special weapons that cause the unit to enter a different mode do now
    have a 10 second cooldown
  - Refits may now be purchased while the unit is cloaked
  - Troopships are now capped at a maximum of 10
  - When a shipyard has been successfully boarded, it now loses it's allied
    repair contact upgrade
  - Analyze Alien Technology in Mixed-Tech Yards now costs more resources and
    takes longer to research
  - Recycling starships or stations now yields the correct amounts of resources
    (calculated with uniform formulas), including tritanium
  - Main base weapon upgrades bonus increased to 30 (bf.: 20) and defense
    upgrade bonus increased to 90 (bf.: 60)
- Avatars
  - Borg Assimilate Collective: Connections costs reduction for Assimilators
    decreased to 20 percent (bf.: 50 percent)
  - Federation Admiral Risner: System Value bonus for Eraudi-Yard vessels
    reduced to 4, but now also grants a small Offensive Value bonus epending
	on the military status of a vessel (bf.: only plus 5 System Value)
- Borg
  - Probe's Devestating Attack damage and energy costs reduced
  - "Resistance is futile" costs increased to 500 dilithium and 700 tritanium
    (bf.: 400 / 400), but may now be purchased repeatedly
  - Beam Modules now only add the ability to strike at two targets. The
    individual beams deal more damage. Additional Beam Modules do now also
	add damage reduction from beam weapons (13% per module). Costs increased
  - Direct hull damage caused by Torpedo Modules is now affected by damage
    reduction effects. Additional Torpedo Modules do now also add damage
	reduction from torpedo weapons (9% per module). Costs increased
  - Holding Beam Modules do now also add damage reduction from special weapons
    (20% per module). Costs increased
  - Tactical Armor Modules now decrease damage received based on the Offensive
    Value of attackers. the weaker the the attack is, the less damage is
	received. Costs increased (bf.: beam damage reduction)
  - Nanites no longer disturb the interface, but have a chance for subsystem
    failure. Special energy costs increased to 600 for the Cube and 700 for the
	Diamond (bf.: 400 / 500)
  - Order to Chaos energy costs increased to 770 (bf.: 717)
- Dominion
  - Collision base damage reduced. The damage will now increase with each rank
    of the Attack Destroyer. Deals half damage to civil targets and stations.
	There is now also a chance of collision to fail
  - B-5 Battle Cruiser officer ability Extended Hangars changed to increase the
    maximum number of fighters by 1 / 2
  - Fighter Energy Core Upgrades now increase all attributes by 5 and increases
    damage to stations by 50 percent (bf.: 2 and 20 percent). Costs increased
  - Active Tachyon Defense has been converted into a passive advanced tachyon
    ping
  - B-5 Battle Cruiser Prototypes no longer count to the fightercarrier cap
  - Energy Beacon no longer affects targets that may use Energy Beacon on
    their own. Cooldown removed
  - Cascade Feedback will now only affect up to 5 individual targets
  - IS Bomb will now only affect up to 5 individual targets
  - Alpha Ketracel White shotdelay boost reduced to 30 percent (bf.: 45).
    Energy costs increased to 114 (bf.: 96)
  - EM Blast will now only affect up to 3 individual targets. Shotdelay malus
    reduced to 50 percent (bf.: 100). Energy costs decreased to 260 (bf.: 340)
- Federation
  - The Mixed-Tech Monsoon (Borg) passive ability Tactical Armor has been
    adjusted acording to the Tactical Armor changes for the Borg. See above
  - Avalon Class carriers (Admiral Risner) only require level 2 chassis to
    be built (bf.: level 3)
  - Hyperimpulse Drive speed bonus increased, but has a 30 second cooldown now
  - Avalon Class officer ability Elite Squadrons will now increase the damage
    dealt by fighters to small units by 30 / 60 percent (bf.: fighter
	deployment time reduction)
  - Beam and Pulse Platform have switched positions in the techtree and in
    their strategic lineup
  - Sensor Blackout will now only affect up to 5 individual targets. Shotdelay
    malus reduced to 35 percent (bf.: 75). Energy costs increased
	to 429 (bf.: 256)
  - Increased the movement speed reduction of Energy Rerouting
  - Increased the movement speed reduction of Tricobalt Triloader. Energy costs
    reduced, duration reduced to 5 seconds (bf.: 6) and costs 3 supplies
	to be activated
  - Organize Defense has been removed, but additional passive abilities have
    been added to the Torpedo Platform to compensate
  - Emergency Power no longer affects targets that may use Emergency Power
    on their own
  - Distortion Field will now only affect up to 5 individual targets. Damage
    per target increased slightly
  - Vector Calculation energy costs increased to 425 (bf.: 73)
  - Barrage will now only affect up to 4 individual targets
- Klingon
  - The Qaw'Duj class has a new replacement for Sword of Kahless - Imperial
    Stance: Gains +3 to all attributes and becomes immune to most special
	effects
  - Bortas' Class Attack Probes cost supplies to be deployed now, but are
    permanent and may be targetted and destroyed. As the probe no longer has
	a duration, the officer abilities have been adjusted
  - Scrambling Probe duration reduced to 60 seconds (bf.: 80) and
    system-failure durations reduced to 4 seconds (bf.: 5). Supply costs
	increased to 4 (bf.: 3) and energy costs reduced to 114 (bf.: 129)
  - Cho'naQ Class damage per second increased slightly
  - Field of Fire will now only affect up to 5 individual targets. Energy
    costs have been adjusted
  - Scrambling Probe will now only affect up to 5 individual targets
  - Battle Plan energy costs increased to 50 per second (bf.: 25)
  - Condensed Proton Warheads will now only affect up to 4 individual targets
- Romulan
  - The Saeihr's Concave Plasma Coils special ability damage bonus on particle
    weapons has been converted into a pure Offensive Value bonus for better
	comperability. This leads to a total +25 Offensive Value bonus. Special
	Energy costs slightly adjusted to 183 (bf.: 187). May now be used while
	cloaked and no longer disables cloaking
- Plasma Bolt minimum shot delay increased to 15 seconds (bf.: 10)
- Multi Purpose Weapons may now be toggeled while cloaked
- Rhienn Class Refits have been moved to the Upgrade Facility along with their
  special ability research. Rhienn Refits grant much higher attribute bonuses
  now. Costs increased. Both refits will now also increase the crew of the Rhienn
- Refined Pulse Disruptors (Rhienn Class, officer rank) will now increase the
  reload time of pulse disruptors, but the damage got increased
- The Talon Class Refit now increases defensive value by 5, emits a mobile
  Tachyon Ping, but has no longer cloaking ability
- Research Institute construction costs increased to 600 Dilithium and
  300 Tritanium (bf.: 400 / 200). Techtree contribution increased
- Tal'Shiar Academy construction costs increased to 600 Dilithium and
  300 Tritanium (bf.: 400 / 200). Techtree contribution increased.
  Does now require the Staryard to be built
- D'deridex type Warbird shield regeneration rate increased by 5 percent
- Sensor Jammer will now only affect up to 8 individual targets. Damage per
  target increased. Energy costs have been adjusted
- Stealth Field will now only affect up to 10 individual targets. Shotdelay
  malus reduced to 10 percent (bf.: 25) and duration reduced
  to 10 seconds (bf.: 15). Energy costs increased to 345 (bf.: 326)

Fixes
-----
  - Either the 1280x960 or 1280x1024 resolution wasn't available on some
    systems where both resolutions are supported by the hardware
  - Cloaking animation doesn't 'jump' anymore when a cloaked vessel levels up
  - Fixed some more "double-click selection" issues
  - Fixed a minor display issue of the map's name in the game setup screen
    with game settings on default
  - The game lock checkbox in the advanced options dialog in multiplayer now
    correctly states "Game closed" instead of "Game open"
  - Fixed some invalid rank display on several ships
  - Fixed Avatar icon being missing after loading a savegame
  - When focused on a vessel in tactical mode, the focus would be lost when the
    vessel was being upgraded or gained a new rank
  - In tactical view rank icons or healthbars of distant vessels could be seen
    that were not in render radius
  - Disabled Armada 2's gameobject mouse over delay to solve the issue of
    giving move or attack commands unintentionally
  - Fixed several tooltip issues and typos. Thanks for reporting!
  - Added the range overlay to many special weapons that didn't had one yet
  - Klingon Scrambling Probe subsystem failure is now in sync with the
    optical effect
  - Fixed some minimap issues on low graphic settings
  - Fixed the gameobject inspector in map editor mode not taking the
    full window height
  - Fixed a rare game startup crash
  - Map Editor: It was not possible to set a Wormhole's name
- Game mechanic
  - Fixed some formulas causing inconsistant damage to be dealt to certain
    ship classes
  - Changed the formulas for calculation rate of fire reductions. A 100%
    reduction of fire rates results in a doubled reload time between shots
  - Fixed a bug causing Dominion Cascade Feedback to deal too much damage and
    to consume too much special weapon energy
  - Effects which are based on the weapons of a unit (for example on the number
    of special abilities) do now work properly and are capped
	at a maximum effect
  - Under some circumstances, vessels lost their previously given command when
    leveling up
  - A vessel doesn't lose it's guard target anymore when the guarded vessel
    levels up or is being upgraded
  - Vessels don't get stuck in repair queue anymore when their current shields
    value exceed the intended maximum
  - Klingon mixed-tech K'Vorts (Federation) will now have a correct evasive
    chance to be missed on all ranks
  - Klingon Attack Probe deals correct damage now
  - Dominion Fighters have the correct - reduced - attribtues now
  - Federation Warp-In vessels sometimes were delayed or never transferred
    to a player
  - Federation Warp-In vessels sometimes were sent to a wrong location
  - Federation Warp-In vessels were created with the same damage ratio as
    Starfleet Command
  - Federation Warp-In is now usable during the "cease fire" phase
  - Federation Saber Class vessels have the correct dodge chances now
  - The Romulan Talon Repair Refit cannot repair anymore when its cloaking
    device is active
  - Some attack modes were unintentionally deactivated when a vessel lost its
    weapon control system
  - Dominion Collision special weapon shouldn't be able to "track" cloaked
    targets anymore
  - Romulan Mixed-Tech Generix Class Frigates (Federation) will keep its
    Critical Shot ability on higher ranks
  - Romulan Mixed-Tech Serkas Class (Dominion) vessels will now keep their
    Mixed-Tech bonuses when leveling up
  - It is no longer possible to research Breen technologies on a captured
    Dominion Technology Lab
  - Officer rank Federation Remore Class vessels no longer reduce damage of
    non-special weapons that consume special weapon energy to be fired
  - Romulan Multi Purpose Weapons consume the correct amounts of special
    energy on all ranks
  - A Borg Diamond's Nanites consume the correct amount of special energy now
  - The Superiority Fighter and Superiority Destroyer passive abilities
    work as intended now
  - All Avalon class ranks do now count to the unit limit
  - Fixed an exploit allowing players to remove the footprint of stations
    in certain situations
  - Klingon Cho'naQ Class vessels may now deactive their Energy Buffers
    on Veteran rank
  - Federation Excelsior Class vessels no longer suddenly lose all their
    special weapon energy
  - Romulan veteran Experimental Pulse Disruptors work correctly against
    cruisers now
  - Romulan Mixed-Tech Leahval Class (Federation) vessels no longer gain
    invalid weaponry if Advanced Energy Reeling is activated
  - Norexan Class vessels deal now correct damage on all ranks for all avatars
  - D'deridex Class vessels deal now correct damage on veteran rank
    for all avatars
  - Romulan Shrike Class vessels now actually have medium weapon range
    as intended
  - Klingon Vor'cha Class ships deal consistent damage
  - Romulan Mixed-Tech Rhienn Class (Klingon) is now medium range as intended
  - Romulan Spectre-Generix Veterans do now fire their weapons at correct
    weapon ranges and deal correct damage
  - Dominion Battlecruisers may now correctly upgraded if reaching veteran rank
  - Timed shield disabling effects were not correctly applied to a vessel while
    being upgraded or gaining a new rank
  - Vessels were able to use a destroyed weapon system after gaining a new rank
  - Fixed several maps
    - Destiny Board
    - No Way Out
    - Clear Sky

Modding
-------
- Backward compatibility for most 3rd party Armada 2 SOD model files improved
  to allow usage without further file modifications
- Added switch to enable Single Player campaign screen button. Note: Requires
  some bitmap and bink files from stock Armada 2 (otherwise it will crash)

--------------------------------------------------------------------------------
Version 3.0.7                                                      7th June 2009
--------------------------------------------------------------------------------

Additions
---------
- Hovering Special Weapon buttons now shows their range on selected ships

Changes
-------
- General
  - Further AI adjustments and improvements. AI players have been renamed to
    Normal, Hard and Merciless
- Dominion
  - Collision deals greatly reduced damage against stations
- Borg
  - Rescaled the Borg construction costs on most vessels, causing reduced cost
    for smaller Borg units, but reduced the rate at which Connections accumulate
  - Sensor relay energy costs reduced and resource costs slightly increased
  - Adaption Matrixes may now also acquire a service contract to allow allies
    to repair
  - Borg vessels won't continue movement after using "Refresh Cycle"
- Federation
  - Starfleet Command construction time increased to 120 seconds (bf.: 90)
  - It is now possible to use normal warp-in if you have less then three free
    warp-in slots. As Star Fleet has no ships to spare, vessels exceeding your
    warp-in cap will leave the sector immediately
- Klingon
  - Shield Breaking Torpedo may now target stations, energy costs increased

Fixes
-----
- All Dominion prototypes have the correct attributes now
- In rare situations a previously owned derelict vessel could get command-able
- The Dominion Attack Destroyer now explodes on ramming right away again,
  instead of floating dead in space first
- The Dominion Attack Destroyer won't use it's Collision attack when on high
  weapon autonomy anymore
- Fixed a bug in the craft ai causing a multiplayer out of sync issue
- Fixed a bug that prevented the AI from recrewing recrew-capable stations
  and vessels
- Under certain circumstances vessels using special weapon toggles could
  cause a multiplayer out of sync issue while cloaking or docking to a
  repair station
- Some special weapon toggles unintentionally would deactivate when
  cloaking or docking to a repair station
- Fixed a rare crash on loading a savegame
- Station shuttle-traffic lost it's animation after loading a (save)game
- Explosions didn't expire correctly and were still visible and
  appeared 'frozen' after a loading a savegame
- Federation Phalanx Class vessels no longer produce an "Error"
  graphic when firing
- Federation Veteran rank Saber Class vessels may now use their
  hyperimpulse correctly
- Romulan Generix refits use the correct ammount of Veteran slots now
- General Helev's Intelligence Center still counts as a special station
  once leveled up
- Canaveral Class Tachyon Scans works as intended again
- Canaveral Class officer abilities are working as intended now
- Borg Detectors may refresh their cycles now
- Dominion Cascade Feedback deals damage as intended now - finally
- Klingon Alternative Armament disables the engines correctly
- Veteran Mixed-Tech Akiras (Dominion) will still fire two torpedoes
  on Veteran rank
- Veteran Mixed-Tech Defiants (Romulan and Klingon) will keep their new
  special abilities on Veteran rank
- Fixed the insane use of Nanites when on high special weapon autonomy
- Corrected some invalid Defensive Values for certain Tavara configurations
- Leveled up Federation Akira Class vessels will show the correct visual
  effect once Defensive Patterns are activated
- Fixed various issues with vessels of the same type not being selected when
 double-clicking them
- Tooltips
  - Federation Warp-In will now display the current vessel limit again
  - Removed obsolete references to priority from some Borg tooltips
  - Upgraded starbases will now show their correct weaponry

--------------------------------------------------------------------------------
Version 3.0.6                                                      20th May 2009
--------------------------------------------------------------------------------

Additions
---------
- Veteran Ranks for all your vessels! Check www.fleetops.net for details!
- Improved Borg vessel construction: A new build-menu allows you to select
  your modules on one screen and to preview the final vessels without
  spending resources
- Reduced game loading times
- An new upgrade has been added to all repair-capable shipyards allowing
  them to be used by allied players as repair-stations
- A complete new Klingon Imperial Yard model
- Added hints to the game loading screen
- The Ambassador is ready to support Federation players via Warp-In
- System Value does now also give a unit resistences against subsystem failure.
  Higher System Values reduce both the duration and the chance to be affected
  by a subsystem disabling effect
- The Borg Detector popular from Armada 1 has been added as a new Borg scout

Changes
-------
- General
  - Vessels do not automatically open fire on enemy targets that an allied
    player is beaming to anymore
  - Double-clicking units now does select units of the same type regardless of
    their rank
  - Selectable teamcolors have been changed to get a wider variety
  - Vessel Team/Race affiliation badge behavior improved for combat
  - Improved AI vessel handling and cloaking behavior
- All Races
  - Slightly redone technology-tree formulas, causing smaller cost changes on
    nearly all vessels
  - The firing arc of vessels has now greater influence on their construction
    costs
  - Starbase weapon upgrades will now add new weapons instead of improving the
    existing ones, including a multitarget weapon
  - Either reduced the costs or increased the attributes on all stationary
    defense structures. They cost supplies to be build now
  - Many special weapons that disable subsystems work different now or have
    adjusted energy costs or durations
  - Many special weapons that deal damage have now adjusted damage values
    or energy costs
  - Many special weapons that deal area damage do now split their damage
    across all possible targets. Those changes are noted in the specific
    tooltips
- Federation
  - Descent Class cooldown for Shield Reset decreased to 300 seconds (bf.: 360)
  - Quantum Torpedoes will now always fire volleys instead of single torpedoes
  - Blockade Breaker (Miranda II Class) special energy costs increased and
    duration reduced to 20 seconds (bf.: 30)
  - Precise Volley (Remore Class) changed to deal shield damage (bf.: disabling
    the shield subsystem). Cooldown removed
  - Critical Shot (Defiant Class) now deals additional damage
  - Admiral Mayson's Canaveral Class gains now the same bonuses from ranking
    up as all other Canaverals
  - Saber Class vessels gain higher overall attributes when ranking up to
    keep in line with other starting vessels
  - Nebula Class vessels gain higher Defensive Values when ranking up
  - Hypercharge Magnan Emitter (Phalanx Class) effectiviness increased
  - Type 3 Torpedoes (Galaxy Class) may now miss, but lowered the shield
    regeneration rate reduction
  - Warp-In limit increased to 10 slots (bf.: 9)
  - ACS-Torpedo (Excelsior-II Class Admiral Mayson) damage reduced
  - Tricobalt Torpedo (Intrepid Class) damage reduced slightly
  - Proximity Torpedo (Monsoon Class) damage reduced
  - Torpedo Barrage (Akira Class Officer ability) damage greatly increased
    and deals direct hull damage on officer rank 2
- Borg
  - Directive: Assimilate does now also increase the Defensive Value for
    Pyramids and Assimilators by 20
  - Directive: Optimize does now reduce the costs of selecting a Borg
    chassis by 30 percent instead of reducing the costs of the first module
    installed
  - Borg Sensor Relay sensorrange reduced
  - The Nanites Collective Feature consumes two slots now and the Uni-Complex
    Collective Feature one slot. They switched places
  - The Transwarp Feature consumes three slots now and the Perfect Logistics
    Collective Feature four slots. They switched places. Perfect Logisticst
    changed to unlock a Resource Assimilator upgrade
  - Sensor Relays no longer count towards the victory condition and are
    placed more carefully by AI players
  - Datamining sensorrange bonus lowered to 30 percent
  - Devestating Attack now deals high shield damage (bf.: disabling shield
    subsystem). Cooldown removed
  - Regeneration Modules do now grant the vessel a chance to completely resist
    subsystem failure caused by special weapons and the like. Each
    Regeneration Module installed increases the chance to resist
  - Collective Uplink Defensive Value greatly increased
  - Borg Probe sensor range reduced and requires the Conduction Matrix now
    to be built. System Value and construction costs reduced
- Romulan
  - General Mijural now increases the Offensive and Defensive value of all
    Warbirds by 3 (bf.: 4)
  - Romulan vessels do now fire faster when opening combat. The rate of fire
    drops after each shot. If a vessel stays out of combat for 15 seconds the
    effect is refreshed. Does not apply to the quadcobalt torpedo
  - The Fakairu Class may now also use the Stationary Cloak special ability
    once a Cloaking Generator is present
  - Stationary Cloak special energy costs reduced to 10 per second (bf.: 15)
  - Griffin Class (General Helev) base attributes increased to 25 Offensive
    Value, 21 Defensive Value and 14 System Value (bf.: 20 / 16 / 11).
    Costs increased
  - Sensor Jammer (Griffin Class) has now a chance to fail, but duration has
    been increased to 6 seconds (bf.: 3 seconds) and there is a chance that
    weaponsystems will be disabled. The Grifin Class officer ability will
    eliminate the chance of the Sensor Jammer to fail
  - Reduces the energy costs of the Sleer Agent and Saboteur Agent special
    abilities (Intelligence Center)
  - The Rhienn Class Torpedo Refit officer ability Infiltrate has been changed
    to have a certain chance to disable a stations weapons with a fixed period
    of a few seconds (bf.: fixed chance with a variable duration)
  - The Generix Class Support Refit officer ability Plasma Storage duration
    bonus increased to 2/4 seconds (bf.: 1/2)
  - Vector Torpedo (Rhienn Class Torpedo Refit) range increased to artillery
    range (bf.: medium) and special energy costs increased
  - Cehlaer Class (General Mijural) base attributes reduced to 37 Offensive
    Value, 31 Defensive Value and 19 System Value (bf.: 39 / 33 / 21).
    Costs greatly reduced
  - The Tavara Class officer ability Modularization now increases the attribute
    bonuses from upgrades by 20/40 percent (bf.: granted free offensive and
    defensive upgrades)
- Dominion
  - Reduced dilithium and tritanium costs for all vessels
  - The C-17 Siege Cruiser gains higher System Values while ranking up. Base
    attributes changed to 38 Offensive Value, 30 Defensive Value and 22 System
    Value (bf.: 26 / 20 / 32). Costs increased
  - Phaseshield (C-17 Siege Cruiser) special energy costs reduced to 345
    (bf.: 432) and duration reduced to 8 seconds (bf.: 10)
  - Perimeter AT-Charges damage increased
  - Excessive Strike (V-13 Battleship) changed to deal high hull damage
    (bf.: disabling shield subsystem and dealt normal damage)
  - Dampening Missile (Breen Battleship) now deals additional shield damage
    and may disable up to two subsystems
  - Breen Cruisers and Battleships are now armed with special Breen Torpedoes
  - Breen Battleship officer ability Breen Technology has now a chance to
    disable subsystems per shot for a longer duration, but no longer drains
    special weapon energy
  - A-20 Attack Destroyer base attributes increased to 14 Offensive Value,
    11 Defensive Value (bf.: 13 / 10). Costs increased. Collision damage
    increased and changed to direct hull damage
  - A-26 Bomber base attributes increased to 21 Offensive Value, 15 Defensive
    Value (bf.: 20 / 14). Costs increased
  - The S-2 Escort Cruiser gains higher Offensive and Defensive Values while
    ranking up, but no longer gains System Value. Base attributes increased to
    25 Offensive Value, 18 Defensive Value and 10 System Value
    (bf.: 22 / 16 / 9).	Costs increased
  - Adjusted the costs of Breen research to be in line with other
    Dominion upgrades
  - The Mixed-Tech EM Blast (Federation) engine shutdown duration reduced
    to 3 seconds (bf.: 12)
  - The C-17 Siege Cruiser officer ability Bombardement Loadout now increases
    the damage of the Tetrion Disruptor by 10 / 20 percent (bf.: 30/60)
  - Tetrion Photon Torpedo (A-26 Bomber) damage reduced
  - Cascade Feedback (C-11 Interception Destroyer) has now a chance to drain
    special weapon energy
  - Borg and Klingon Mixed-Tech Dreadnoughts are now equipped with additional
    torpedo launchers instead of beam weapons
  - The T-15 Heavy Cruiser Officer Ability Impulse Implosion deals highly
    increased damage
- Klingon
  - Klingon pulse disruptors do now fire bursts of multiple shots. Each shot
    can hit or miss seperately
  - Weapon Overload (Negh'var Class) torpedo cooldown increased to 30 seconds
    (bf.: 6) and weaponrange increased to long (bf.: medium)
  - Alternative Ammunition (Veqlaragh Class) torpedoes may now miss
  - The K'Vort Class officer ability Verteron Particle Loads has been changed
    to have a certain chance to disable engines with a fixed period of 4
    seconds (bf.: fixed chance with a variable duration)
  - Polaron Torpedo (Vorcha Class) now deals additional direct hull damage
  - Compound Torpedoes (Sang Class) may now miss and no longer targets cloaked
    vessels but deal increased damage
  - The Mixed-Tech Sang' Class (Romulan) now improves the compound torpedoes
    and gains multi targeting, but no longer always hits
  - Torpedo Drone and Heavy Disruptor (K'beajQ Class) damage reduced
  - Last Ditch Assault (SuS'a'Class) positive effect duration increased to
    35 seconds (bf.: 25)
  - K'beajQ Class base attributes increased to 19 Offensive Value, 19 Defensive
    Value and 7 System Value (bf.: 13 / 13 / 5). Costs increased
  - K'Vort Class base attributes increased to 23 Offensive Value, 20 Defensive
    Value and 8 System Value (bf.: 20 / 18 / 7). Costs increased
  - Vutpa' Class base attributes increased to 34 Offensive Value, 25 Defensive
    Value and 11 System Value (bf.: 32 / 23 / 10). Costs increased

Fixes
-----
- Rankup sound is not played with maximum sound volume anymore
- Map Editor: Map Type is now saved correctly
- Manual IP connections should now work properly again
- Savegames did unintentionally appear in the map selection
- Fixed a rare crash when starting a game or loading a map for editing
- A leaving player in multiplayer games could cause the game to crash for the
  remaining participants
- Fixed a rare crash that occurred right after launching the game executable
  on some systems
- Fixed Borg Sensor Relay explosions
- Supply buttons visibility in free tech mode corrected
- Ship selection shouldn't "shuffle" anymore when a ship in a group gains a
  new rank
- Fixed Warp-In handler that should prevent warp-in vessels from sometimes
  being not command-able
- Fixed a possible crash when a race other than Federation was using the
  Starfleet Command Warp-In
- Gameplay
  - Cloaked vessels gaining a new rank don't lose their cloaked state anymore
  - Romulan Sleeper Agent special ability is no longer able to infiltrate
    objects unrelated to gameplay
  - TaQ´rojas B'rel Class offensive value bonus is now correctly applied
    at ranked-up B'rels
  - Federation Plasma Bolts (Canaveral Class Mixed Tech: Romulan) do now last
    the correct duration as mentioned in the tooltip
  - Qaw'Duj Class vessels are no longer built in an invalid officer rank and
    gain experience correctly
  - Mixed-Tech Sang' Class vessels (Borg) no longer switch to long range
    when ranking up
  - Mixed-Tech Cho'naQ Class vessels (Dominion) are now artillery range
    as stated by the tooltip
  - Mixed-Tech Defiant Class vessels (Klingon) now stick to their rapid pulse
    phaser even while ranking up and all Defiant Mixed-Tech variants will
    increase their quantum torpedo damage on max officer rank correctly
  - Fixed a bug causing the Defiant to get invalid stats in certain situations
  - Mixed-Tech Rhienn Class vessels (Klingon) rank up now correctly
  - D'deridex Class warbirds gain the correct attributes while ranking up
  - It is no longer possible to build more then one Intelligence Center or
    Cloaking Generator under certain conditions
  - The Breen Battleship no longer incorrectly gains the Breen Cruiser officer
    ability when ranking up (increased shield regeneration rate)
  - Most vessels with quantity limitations, including veteran vessels, can no
    longer be transfered to friendly players. You may transfer some ships like
    repair vessels or carriers, but the receiving player has to meet the
    limitation requirements
  - Refined Disruptor (Vor'cha Class Mixed Tech (Federation)) deals now
    correct damage
  - The Mixed Tech NoQ'Duj Class (Dominion) gains correct speed and sensorrange
  - Dominion Cascade Feedback deals damage as intended now
  - Dominion Active Tachyon Scan works as intended now and costs
    proper special energy
  - Repair vessels will no longer incorrectly target hostiles with repair beams
  - Romulan Multi Purpose Weapons consume correct amounts of energy now
  - Romulan Vector Torpedo is now correctly affected by damage reduction
    effects
  - Fixed a bug causing many incorrect hull repair rates
  - Corrected the Federation Class Mixed-Tech (Borg) Intrepid Class attributes
  - Tooltips
    - Corrected several spelling and formating errors
    - Klingon Heavy Disruptor and Torpedo Drone research no longer incorrectly
      states to apply to the K't'inga Class
    - Admiral Risners avatar bonus is now correctly displayed in the Avalon
      Class tooltip
    - Galaxy Class vessels will show their correct attributes if Type-3
      Torpedoes are active and the Galaxy ranked up
    - Combat Cloak (Generix Class Specter Refit) will now show the correct
      energy costs
    - The Mixed-Tech Leahval Class (Federation) tooltip shows the correct
      weapons on officer ranks now
    - Corrected the spelling of Tachyon and Tetryon particles

--------------------------------------------------------------------------------
Version 3.0.5a                                                   16th March 2009
--------------------------------------------------------------------------------

Fixes
-----
- Fixed a crash in the AI

--------------------------------------------------------------------------------
Version 3.0.5                                                     8th March 2009
--------------------------------------------------------------------------------

Changes
-------
- General
  - The AI will now use cloaking
  - Some changes to the AI attack behavior
- Federation
  - The Saber Class gets now higher Offensive Value bonuses through rank-ups,
    but reduced Defensive Value bonuses
  - The Torpedo Platform (Admiral Mayson) is now artillery range. Costs
    slightly increased
- Dominion
  - The Fighter Energy Core Upgrade can now be acquired even if you already have
    the maximum number of carriers
- Borg
  - Holding Beam Special Ability does now transport 4/8/12/16 drones per
    second (bf.: 2/4/6/8) but consumes 175 special energy per second (bf.: 75).
    The Starbase Holding Beam granted through Collective Features has also been
    adjusted respectively
  - Auto Assimilator Special Ability does now assimilate 4/8/12 drones per
    second (bf.: 2/4/6) but consume 115 special energy per second (bf.: 50)
- Romulan
  - Rei´karansu Covert-Ops Team (Tavara Class Warbird) special energy costs
    reduced to 250 (bf.: 400)
  - Cloaking Generator costs decreased to 600 dilithium and 300
   tritanium (bf.: 800 / 400)
  - The Intelligence Center now gains experience from deploying agents and will
    gain higher defense and a higher special energy pool with each rank-up.
    Costs increased to 1000 dilithium and 500 tritanium (bf.: 600 / 300)
  - Sleeper Agent (Intelligence Center) special energy costs increased
    to 375 (bf.: 290) and does now cost 3 supplies. Grants expierence to
    the Intelligence Center
  - Sabotage (Intelligence Center) replaced with the Saboteur Agent special
    ability (600 special energy and 10 supplies). The Saboteur Agent has a
    chance to eliminate a vessel which has an active Sleeper Agent onboard.
    Grants expierence to the Intelligence Center
  - Spy (Intelligence Center) special energy costs increased to 1000
    (bf.: 380). Is now the officer ability of the Intelligence Center
- Avatars
  - General Helev will now reduce the experience required for the Intelligence
    Center to level up instead of reducing the special energy costs of
    Sabotage, Spy and Sleeper Agent

Fixes
-----
- Fixed a crash that could occur when issuing orders while a vessel is
  leveling up
- Allied race requirement is now correctly not satisfied if your ally has
  been eliminated
- Fixed Admiral Maysons Mediterranean Tooltip
- Fixed the invalid Breen Large Yard Construction menu
- Federation Platform addon hotkeys are working properly now

--------------------------------------------------------------------------------
Version 3.0.4                                                     5th March 2009
--------------------------------------------------------------------------------
Additions
---------
- A complete new Klingon Heavy Weapon Platform model along with new weaponry

Changes
-------
- General
  - Credits costs have been removed. All vessels which did cost credits
    prevously are now limited. Some unit prices have been adjusted
  - You do no longer need to toggle into mixed-tech mode on captured vessels.
    Build menus are instantly updated
- Federation
  - Changed the probability to get certain Warp-In vessels. At a very low
    chance you may now even get two "large" Warp-In vessels like Galaxies
- Romulan
  - The Cloaking Generator itself may now also cloak
- Borg
  - Weapon Subsystem is now indestructible, but may still be disabled due to
    the effects of special weapons

Bugfixes
--------
- Fixed two severe issues causing 'out of sync' problems in multiplayer games
- Fixed broken Graphic Options dialog from 3.0.3
- Ship caps are now also accounted for vessels that are under construction
- Craft system damage is now correctly applied after vessel upgrade or rankup
- Fixed a save game loading crash
- Cloaked vessels could not be attacked when revealed by a sensor station
- The Klingon AI is now able to advance through its complete tech-tree
- Fixed invalid selection area for the Klingon Field Yard
- Fixed some asteroid belts on variouse maps
- The Extend Supply Routes hotkey is working now
- Borg Assimilators may now correctly use the High Energy Slizer
  Collective Feature

--------------------------------------------------------------------------------
Version 3.0.3                                                 18th February 2009
--------------------------------------------------------------------------------

Additions:
----------
- Two new maps have been added.
- "The Edgeworth-Kuiper Belt" created by Phoenix
- "Swings and Roundabouts" created by Phoenix

Changes:
--------
- Federation
  - The Full Scan special ability no longer disables weapons, but reduces
    the Offensive Value
  - Warp-In has now a cooldown of 250 seconds as intended
  - Dominion-Mixed-Tech Mandril Class bonus changed to 25 percent increased
    storage capacity (bf.: static 25 storage increase)

Bugfixes:
---------
- Fixed a crash when a Defiant Class reaches rank 5
- Building a Miranda II Class crashed the game
- Fixed some tooltips, including the Avalon Class officer ability
- In rare cases vessels got wrong command info after upgrading, which either
  resulted in unwanted vessel behavior or an "out of sync" error

--------------------------------------------------------------------------------
Version 3.0.2                                                 16th February 2009
--------------------------------------------------------------------------------

Additions:
----------
- Reimplemented Save Game mode (experimental)
- Avatar icon now has a tooltip with info about the previously selected Avatar
- Global Unit build Cost slider range increased (max 5x)
- New model for Klingon Mining Station
- The Excelsior is ready to support Federation players via Warp-In

Changes:
--------
- General
  - Increases the robustness of the shield-generator-subsystem when hit once the
    shields are down
  - All Mixed-Tech unique designes do no longer cost credits, but are limited
    to 20. Their resource costs got slightly adjusted. Same applies to some other
    credit-costy units like the Phalanx Class
  - Maximum count of fighter carriers reduced to 5 (bf.: 7)
- Dominion
  - Dampening Missile special ability duration reduced to 3 seconds (bf: 5) and
    special energy costs increased to 57 (bf: 48)
  - Breen Technology officer ability subsystem failure duration reduced to
    1 / 2 seconds (bf: 2 / 4) and drained special energy increased
    to 13 / 26 (bf: 7 / 14)
  - The Hyperspace Artillery will now only require a Technology Lab to be
    built (bf: Ketracel White Facility and Large Construction Yard)
  - Collision special ability (A-20 Attack Destroyer) damage increased
  - IS-Bomb special ability supply costs decreased to 10 (bf.: 20)
- Romulan
  - Tavara Class credits costs increased to 8 (bf: 6) and construction costs
    decreased to 832 dilithium, 282 tritanium and 35 supply (bf: 962 / 331 / 40).
    Construction time increased
  - Sabotage special ability (Intelligence Center) damage increased a bit
  - Greatly reduced the Serkas Class Quadcobalt Torpedo rumble effect
  - Concave Plasma Coil special ability supply costs decreased to 1 (bf.: 2)
- Federation
  - Starfleet Chassis costs increased to 400 dilithium and 200 tritanium for
    level 1, 800 dilithium and 400 tritanium for level 2 and 1200 dilithium
    and 600 tritanium for level 3 (bf: 300/150, 600/300, 900/450).
    Construction times increased slightly
  - Changed the formula at which Starfleet Chassis were incorporated into the
    Federation Techtree costs, causing a very slight increase in Federation
    construction costs
  - Each vessel has its unique set of vessel names now
  - Warp-In will now send 3 ships at once, but the cooldown has been increased
    to 250 seconds (bf: 180). As the ships still cost 2 credits each, the total
    credit costs of Warp-in has been increased to 6. Increased the chance to get
    larger vessels like the Galaxy Class
  - Slightly increased Federation research times, as they can research multiple
    technologies at once, because parts of their research tree takes place in
    the yards
  - Increased the Phalanx Class base attributes to 44 Offensive, 46 Defensive
    and 20 System (bf.: 42,44,19)
  - Avalon Class (Fleet Admiral Risner) has been moved from Experimental Warp-In
    to the Eraudi Yard as a credit-free vessel and got a new Officer ability
- Klingon
  - Polaron Torpedo special ability duration reduced to 5 seconds (bf: 8 )
    and special energy costs increased to 225 (bf: 104)
  - Polaron Field special ability special energy costs increased to 423 (bf: 76)
  - Scrambling Probe special ability supply costs decreased to 3 (bf.: 20)

Fixes:
------
- Fixed a bug that causes a crash while integrating Regeneration Modules in a
  Borg Scout Cube
- Some ships wouldn't turn correctly to face an enemy vessel
- The costs shown and utiliized for the ReplaceWeapon and DetectorWeapon are
  now correctly calculated when playing with a custom global build cost setting.
  That applies to most vessel upgrades and the Sensor Ping special ability.
- Corrected behavior of the Avatar icon when boarding an enemy starbase
- The Klingon Field Yard Extensions do now have correct hit positions
- The Klingon Dominion-Mixed-Tech Vutpa' has now the correct crew capacity
- Max Rank Defiant Class torpedoes do now deal correct damage
- Map Editor
  - Fixed the messed up layout of the map options dialog in the editor
  - Map descriptions are now saved correctly

--------------------------------------------------------------------------------
Version 3.0.1                                                 31th December 2008
--------------------------------------------------------------------------------

Features:
---------
- General
  - A completely reworked AI
- Romulan
  - Staryards and Warbird Yards may now be used to prefine shield generators and
    weapon systems, speeding up defense-turret construction
- Borg
  - A new Interception Module is available for the Scout Cube granting
    anti-destroyer weaponry

Changes:
--------
- General
  - Supply costs will now increase the more supply you order until reaching a
    maximum. The starting price of Supply has been reduced
  - Construction Delays will now be placed at the beginning of the build queue
  - Increased the activation costs of Intelligence Report to 250 Dilithium,
    250 Tritanium and 50 Supplies (bf: 50/50/10)
- Federation
  - The Shield Matrix Reset special ability (Descent Class) is now
    cooldown-based instead of special-energy-based
  - Increased the cool-down of Warp-In to 180 seconds (bf: 100)
  - Precise Volley (Remore Class) duration reduced to 5 seconds (bf: 6),
    special energy costs increased to 540 (bf: 156) and cooldown increased to
    40 seconds (bf: 25)
- Dominion
  - Prototype construction buttons do now appear at the same positions as their
    normal ship buttons
  - Very slightly increased all Supply costs
  - Research costs now a few Supplies
  - Excessive Strike (V-14 Battleship) duration reduced to 2 seconds (bf: 3),
    special energy costs increased to 270 (bf: 181) and damage increased by
    about 200 percent
  - Defender Prototypes no longer cost credits (as intended)
- Borg
  - AI players will try to build Pyramids and Cubes earlier
  - Reduced all module and chassis costs slightly
  - Reduced the costs of the Incubation Complex to 450 dilithium and tritanium
    (bf: 800) and reduced the costs of the maximization upgrade to 180
    dilithium and tritanium (bf: 250)
  - Decreased the area of effect of the Nanites special ability and increased
    its energy costs based on chassis size (500 for the Cube, 600 for the
    Diamond) (bf: 250 Cube, 300 Diamond)
  - Devestating Attack duration reduced on all chassis and special energy
    costs increased to 250 (bf: 150)
  - Damage reduction from adaption modules increased to 25 percent per module
    (bf: 20)
- Romulan
  - Decreased the damage dealt by Quadcobalt torpedoes by 19 percent

Fixes:
------
- Fixed black screen when trying to enter escape menu from game when using AA
  modes on some systems
- Using Fraps shouldn't crash Fleet Operations anymore
- Fixed a crash with the RamWeapon when the target vessel has already been
  destroyed before the weapon could activate
- Fixed a crash that occoured under some circumstances while using defensive
  or offensive upgrades on stations
- Fixed a crash while loading the map editor
- Fixed a bug causing Out of Sync issues in multiplayer games
- Fixed shell interface for systems using non default Windows UI DPI resolutions
- Fixed an issue causing Fleet Operations to hang on systems with DEP enabled
- Fixed a bug that could cause game crashes in certain situations while
  using Federation Warp-In
- Borg Graviton Relays are now named correctly
- Klingon K't'inga Class vessels may now use their cloaking device correctly
- Fixed a bug causing the Borg Optimize Avatar bonus (cost reduction on the
  first module) to yield a too small bonus on several modules
- Station footprints will no longer be removed incorrectly when a
  Klingon Field Yard extension is destroyed

--------------------------------------------------------------------------------
Version 3.0                                                   15th December 2008
--------------------------------------------------------------------------------

New Features:
----------
- The Borg are back, prepare for assimilation!
- Complete new shield effects are included with specific animations for
  each race
- Added new audio content (Dominion voicesets, special weapons and much more)
- Several UI improvements
- Many new special effects, including dynamic light sources for objects
- Complete new AI with new features and possibilities
- Mixed Tech and Technology Trade
	You will now be able to gain special units by capturing alien technology
	 For more details visit www.fleetops.net
- Starbase Upgrades
	Starbases can now be upgraded in Offense and Defense
	 For more details visit www.fleetops.net
- Experience System
	All military vessels do now gain experience in combat and will level up
	 to reach higher ranks.
	 For more details visit www.fleetops.net
- Game Interface
  - Overhauled Vessel Selection Display
    - Healthbars optimized for better visiblity
	- Healthbars are now always visible on friendly damaged vessels
	- Reintroduced the damaged systems display from Armada 1
	- Construction bars are now visible when hovering producers
  - Unit selection cursor rectangle now semi transparent and colored
  - An icon next to the resource display shows your selected Avatar
  - A green circle shows the sensor range of a station while placing it from
    a constructor
  - Hovering the sensor or weapon icon in the vessel info display shows their
    ranges around the vessels
- Screenshots
  - "Cleanshot": New interface-free screenshot mode: Ctrl + Alt + P
  - Screenshots are now saved in the more space friendly PNG file format
    instead of BMP.
- Game Loader rewritten from scratch
- Integrated Map Manager/Editor
  - New Map Settings dialog
  - New Game Object Editor dialog
- Exception handling engine powered by madshi
- Custom graphic resolutions support
- Experimental native multisampling support
- Windows Vista UAC compiliant
  Settings, Savegames and Logs are now saved in the Users Profile folder.
  Screenshots are saved in the "My Pictures" folder
- Maps
  - "The Da'ka Corridor" by T-Man
- Installer: Added an option to install without CD by using an existing
  Armada 2 installation
- Modding related engine changes
  - Gui files now make use of dynamic values to allow correct aspect ration
    when using non-4:3 resolutions
  - More interface elements can now be customized through the gui files
    - Vessel names and class names color
	- ship group label textcolor and backgroundcolor
	- Colors of ingame resources
	- Race specific selection rectangle color and transparency
	- The new Avatar box
  - Loading screen bar can now be customized with a special odf
    - position can now be adjusted
	- start and end color can now be defined
	- start and end transparency can be defined
	- loadbar direction can be adjusted
  - Vessels created with the Utritium beam now have an AI (can be moved)
  - All Gameobjects
     New ODF commands: ignoreInterface, extraWeaponHardpoints, invincible,
	 ignoredAsTarget, invalidAsTarget, clearsFog, selectHighlight,
	 lightColor, lightFalloffStart, lightFalloffRange
  - All Crafts
     New ODF commands: ShieldHit, ShieldDown, ShieldAdd, ShieldSub,
	 ShieldSpecial1, ShieldSpecial2, ShieldSpecial3, ShieldSpecial4,
	 ShieldHitCritical, buildableByOwnerRaceOnly, requiredAlliedRace,
	 createFootprint, destroyFootprint, destroyMeWithResearchStation,
	 avatarBoxIcon, worthXP, nextrankXP, nextrankClass, XPlevel,
	 allowBuildQueuePop, showChoiceDialog, fleetCapFile, 
	 SpecialEnergyDisplayMode, overlaySod, IamGameToTheDeathCondition,
	 constructionVoice, turnToFire, canShipyardRepair, perceivedNeutral
  - ResearchStations
    - can now be build with modules specified in their odf
      that spawn build with the station
	  New ODF command: buildItemXprovided
	- ResearchStations can now handle other objects besides pods as modules
  - ResearchPod
     New ODF command: isResearchedByYard
  - Producers
     All types of Producers can now handle more build items.
  - Shipyards/Starbases
    - a build queue can be defined in the odf that gets processed after the
	  shipyard is built
	  New ODF command: pushBuildItemX
  - Shields
    - Shield sod display geometry has been changed in order to allow better
	  effects
	- Shields aren't colored with the current vessels health color anymore
  - All weapons
     New ODF commands: overrideCeaseFire
  - Special Weapons
    - Special weapon buttons of the same type on a vessel are now shown both
  - AreaEffectCannon
     Now has 9 different target modes instead of 3 for handling cloaked vessels
	 and better target selection
  - New weapons
    - ReplaceWeapon
	   ODF commands: replacementMode, replacementkeepName, replacementSelect,
       replacementInstantPlayer, replacementInstantDerelict,
	   replacementInstantAI, replacementDontRemoveFootprint, replacementEvent,
	   replacementFractionSOD, DilithiumCost, TritaniumCost, MetalCost,
	   SupplyCost, CrewCost, RequiredCredits, requiredDilithium,
	   requiredTritanium, requiredSupply, requiredMetal, requiredCrew,
	   replacementUseLabelAsClass, replacementResetCrew, replacementInstantDelay,
	   replacementKeepOwner, replacementXClassXPosition, replacementCreationType,
	   replacementWarpIn, replacementAfterWarpIn, replacementResetSpecialEnergy,
	   replacementInstantCycle, replacementMoveToTarget, replacementEnemyOfAll,
       replacementMaintainOrders, replacementAnimate, replacementRegisterAsNew, 
	   replacementIgnoreSameClass, replacementXClassYRandomWeight,
 	   replacementFindOpenSpot
	- ResourceWeapon
	   ODF commands: oneTimeMode, addDilithium, addDilithiumThreshold, addMetal,
	   addMetalThreshold, addCrew, addCrewThreshold, addTritanium,
	   addTritaniumThreshold, addSupply, addSupplyThreshold, alwaysActivePlayer,
	   alwaysActiveAI
	- DetectorWeapon
	   ODF commands: DilithiumCost, MetalCost, CrewCost, TritaniumCost,
	   SupplyCost, MarkRadius, MarkShrink, MarkTime, MarkColor
	- RepairWeapon
	   ODF commands: DilithiumCost, MetalCost, CrewCost, TritaniumCost,
	   SupplyCost, standbyTime, eventFire
	- CloakingDeviceImp
	   ODF commands: disableShield, disableWeapons, phaseCloak,
	   decloakShieldDelay
	- CloakingDeviceControl
	   ODF commands: disableShield, disableWeapons, phaseCloak, energyDrainRate
	- RamWeapon
	   ODF commands: explosiondamage
	- CannonImp
	   ODF commands: targetTerrainOnly
  - Ordnance
    - All ordnance
	   New ODF commands: explosionHit, explosionShieldHit, explosionSizeFactor,
	   explosionDuration, explosionSprite
  - New Explosion type
    - RumbleExplosion
	   ODF commands: rumbleDuration, rumbleArea, rumbleStrength
  - Build caps/limitations can be created with FleetCap files
     ODF commands: maxCap, fleetCapClassX, fleetCapClassXcapValue, capTooltip,
	 capTooltipReached, capTooltipOver, affectsAI, simpleCapMode
  - Race ODF new commands:
     allowGiveDilithium, allowGiveTritan, allowGiveMetal, allowGiveSupply,
	 allowGiveCrew, canGainXP, AIFreightersPerMoon, preloadUnitsX

Changes:
--------
- Many new models and texture improvements or complete reworks
- Torpedo-, pulse- and beam-based special abilities do now have a normalized
   reload time of 5 seconds
- Adjusted the construction times of many units, especially destroyers
- Adjusted the attributes and costs of many units, especially battleships
- Many special abilities got their behaivor and effects changed
- The number of supplies required to build units has been increased but
   the amount you start with and get through Supply Routes has also
   been adjusted
- Balance changes:
  - All Starbases may now mark cloaked vessels on the mini-map
  - You will now get 2 free constructors after selecting an avatar
  - All Support Vessels do now feature increased energy regeneration rates
- Special Energy
  - Special Energy is now a precise number that increases with System Value
     rather then a percentage
- Unit Limitations
  - Some units are now unique, meaning you can only build one of them.
     These are:
     - Romulan Intelligence Center
     - Romulan Cloaking Generator
     - Klingon MoQbara' Station
  - Repair Vessels are now limited to 3
  - Fighter Carriers are now limited to 7
- Special vessel types
  - Fighter Carriers got reworked and Troopships are now available
     For more details visit www.fleetops.net
- All research is now permanent and will not be lost if losing the 
   corresponding research station
- All construction options are now limited to their race. That means
   you can no longer build alien starships in captured yards
- Reduced the time it takes to repair damaged life-support

  Avatars:
  - Most Avatar bonuses have been rebalanced to achieve a better gameplay.
     Their effects got increased in general, granting each avatar a deeper
     profile

  Federation:
  - Plasma Coil Special Ability (Norway Class - Avatar: Admiral Mayson)
	Relative damage per second increased, area increased,
	 duration reduced
  - Defense Patterns Special Ability (Akira Class)
	Mechanic reworked
  - Platform
	Federation Defense Platforms are now build from a basic Platform hub
  - Phaser Platform
	A new early-game Defense Platform has been added
  - Sensor Satellite
	Renamed to Sensor Platform
  - Torpedo Platform
	Does now require the Eraudi Yard, but its stats have been increased
  - Distortion Field Special Ability (Sovereign Class)
	Does now deal a small amount of damaged based on the targets speed
  - Descent Class
	The Descent got a complete rework, including new gameplay elements
         and a new special ability
  - Saber Class
	A new special ability is available - the Hyper Impulse Drive
  - Warp-In
	A new Warp-In System is in place, including an earlier availability
	 of the Descent and the Avalon
  - Warp-In Vessels
	All  vessels do now have their own special abilities
  - Sensor Blackout Special Ability (Canaveral Class)
	Effect greatly increased, range reduced, is now a target-area-effect
  - Research
	Special Ability research has been moved to the yards
	Sensor Upgrades are now available at Star Fleet Science

  Klingon:
  - Shield Breaking Torpedo Special Ability
	May no longer target stations
  - NoQ'Duj Class
	Is now equipped with the Superiority Fighter passive ability
  - Sus'a' Class (Avatar: Chancellor Martok)
	Is now correctly identified as short range
	A new special ability is available - Last Ditch Assault
  - Cho'naQ Class
	Does now deal constant damage but consumes special weapon energy
	A new special ability is available - Energy Buffer
  - Veqlaragh Class (Avatar: TaQ'roja)
	A new special ability is available - Alternative Armament
  - Klingon K'beajQ Class
	The K't'inga got out of date, here comes the replacement!
	The K't'inga may still be gained by dispatching a Topmay's storage
  - Vor'cha
	Is now considered a battleship rather then a cruiser
  - Vutpa'
	May now be upgraded into a Troopship

  Romulan:
  - Sensor Jammer Special Ability (Griffin Class - Avatar: General Helev)
	Deals a small portion of damage based on sensorrange
  - Norexan Class
	The Multi Purpose Weapons are now an active special ability
  - Rhienn Refits
	Booth Rhienn Refits got new passive abilities
  - Tavara Class
	The Tavara Class may now be upgraded similar to main bases
  - Cloaking Generator
	The Cloaking Generator now longer requires tech prerequisites and
         got its credits costs reduced to 2
        It does now allow to use Stationary Cloak on all starbases rather
         then its old effect
  - D'deridex Class
	The D'deridex got a new, defensive passive ability and his Defensive
	 Value got boosted. It's now considered the more durable one of the
	 warbird family
  - Cehlaer Class
	All stats increased
  - Phase Cloak Special Ability (Generix Class Dreadnought Refit)
	Replaced by a new ability: Combat-Cloak
  - Intelligence Center
	A new special ability is available - the Sleeper Agent
  - Generix Refits
	The attribute bonuses granted by refits have been increased
  - Generix Class - Dreadnought Refit
	Renamed to Spectre Refit

  Dominion:
  - The relative construction time of all prototypes has been reduced
  - Prototypes may no longer use special abilities
  - Raised the overall Dilithium and Tritanium prices for vessels
  - Workerships
	Mining Mode Workerships may now switch into a Supply Mining submode.
	 Vessels in Supply Mining mode will only dock on Ketracel Synthesizers
	 to ease organisation. They will also show the precise Supply Value
	 they will produce in their selection info. Mining Supply takes much
	 less time then dilithium mining, but deploying it at the
	 Synthesizer will take much longer. This allows parallel mining
  - Perimeter
	May now be upgraded similar to the new starbase upgrades
	One of the upgrades features a anti-destroyer area weapon
  - A new Troopship is available: The B-8 War Frigate
- Disabled Game splash screen
- Game Shell
  - Complete graphic overhaul
  - New non-animated main menu
  - Enhanced Option Menus
  - New Instant Action/Multiplayer Map Selection
  - now locked at 32bpp instead of 16bpp
- Game Interface
  - Improved Special Weapon button display when having more than one
    vessel selected
  - Increased number of special weapon/action buttons shown
  - Default game font color now white instead of green
  - Tooltips
	- Are now semi-transparent
	- Crew cost info has been removed
  - Cursor scaling disabled
  - Switched to Billinear rendering mode
  - Changed size and color of blocks in "Grid view"
  - Colors of game resources have been slightly altered
  - Multiplayer trading screen has better adjusted default values for the new
    resources used
- Cinematic camera switch speed reduced
- Vessel Warp effect strech disabled
- Armada engine memory handling with a large number of object definition
  files has been improved
- Modern CPU instruction set optimized Armada engine functions
- GameSpy Lobby
  Fleet Operations uses it's own GameSpy lobby instead of the Armada 2 lobby.
  So it should be easier for you to find other Fleet Operations players.

Fixes:
------
- Civil units were still be damaged by some abilities which shouldn't
- Units which cost credits can no longer be transfered to an ally
- Fixed several tooltip errors
- Units do now show their correct subsystems in the mini selection
- Fixed a bug causing avatar bonuses to not properly apply to avatar units
- Fixed an exploit with the Dominion Collision special ability
- Fixed a bug causing the Klingon Attack Probe special ability to crash the game
- Romulan Plasma Torpedo special ability consumed to less energy
- Fixed some vanilla Armada 2 flaws for path-finding
- Fixed some vanilla Armada 2 bugs allowing AI players to attack cloaked units
- AIs now can build Harvesting stations next to Tritanium Moons without
  disassembling them afterwards
- Special vessels cannot be traded with other players in multiplayer anymore
- The tactical view camera has a stopping point at the top and bottom of the
  screen again. That feature was bugged in the Armada 2 1.1 patch.
- Fixed an Armada 2 bug causing either a crash and/or out of sync when having
  more than 900 object defintion files in use
- Points given to players in the debriefing data screen for vessel kills should
  now make a lot more sense
- Vessels shouldn't bump into their targets anymore when an attack is initiated
- Vessels now use the correct value to slow down when nearing their target
  location
- Level 2 Shield upgrades now increases shields hitpoints instead of doing 
  nothing
- Civilian and cloaked vessels now don't target enemy vessels anymore
- AI players now correctly evaluate enemy vessel threat values
- Vessels don't open fire on enemy vessels anymore which you are currently
  boarding
- AI players now can correctly recrew ships which have this ability
- Many shield disabling effects have been fixed to not disable a vessels shield
  longer than intended
- Artillery vessels don't open fire anymore while using a special weapon with
  manual target location
- Fixed graphics flicker when using bump mapped models
- Fixed a bug that causes weapons to hit the root hardpoint instead of a 
  designated hit hardpoint of vessel
- Fixed a multiplayer exploit that allowed to remote-build stations across a map
- Fixed "Sorry, there is not enough memory to continue" under Windows x64
  systems
- Fixed a crash to desktop due to an stack overflow while starting the game
- Windows Vista Multiplayer/Online gaming crash fix

Known Issues (To be fixed):
---------------------------
- The Random Map generator is not available
- Savegames are currently disabled
- Win/Lost Message is shown distorted on Widescreen configurations
- The integrated AA mode may give a black screen on some configurations. Try to
  enable AA via your graphic card driver instead.
- It is recommended not to change graphic resolutions ingame in order to avoid
  the game interface to be streched
  
--------------------------------------------------------------------------------
Version 3 Preview Release                                         17th June 2006
--------------------------------------------------------------------------------

Additions:
----------
- Fleet Operations intro video
- New player race: Dominion
- Added the ability to select an avatar at game start, which modifies the
  tech tree and grants several bonuses. Each race has two different avatars
  to chose.
    Federation:
    - Admiral Mayson
    - Admiral Risner
    Klingon:
    - TaQ'roja
    - General Martok
    Romulan:
    - General Mijural
    - General Helev
    Dominion:
    - Dominion Puretech
    - Alliance: Breen
- Added Support Vessels
  Support Vessels are special ships which have an initial special ability and
   can be enhanced with 2 upgrades, giving them additional abilities.
   Support Vessels:
    Federation:
    - Canaveral Class
    - Remore Class
    - Norway Class (Avatar Unit)
    Klingon:
    - Qaw'Duj Class
    - BortaS Class
    Romulan:
    - Generix Class (Support Refit)
    - Leahval Class
    Dominion:
    - Interception Destroyer
    - Siege Cruiser
- Added Supply resource
  visit www.fleetops.net for further informations on the new supply resource
- Added "Ice" environment set
- Added additional mapobjects
- Added race specific selection markers
- Added glow effects for warp drives and similar special effects
- Added weapon fire effects
- Added new voicesets
- Added new vessels and stations
  Federation:
  - Torpedo Platform (for Admiral Mayson)
  - Norway Class (for Admiral Mayson)
  - Avalon Class (for Admiral Risner)
  - Phalanx Class (for Admiral Risner)
  Klingon:
  - Veqlaragh Class (for TaQ'roja)
  - Chargh Class (for TaQ'roja)
  - LuSpet Class (for General Martok)
  - SuS'a' Class (for Geberal Martok)
  Romulan:
  - Shrike Class (for General Mijural)
  - Cehlaer Class (for General Mijural)
  - Griffin Class (for General Helev)
  - Eresis Class (for General Helev)
  Non Player:
    Ferengi:
    - Marauder
- A standard set of textures is loaded when race detection fails

Changes:
--------
- Increased AI fleet efficency
- New Mainmenu 'beeping' sounds
- Advanced environment
  - changed camera handling
  - changed physic system
  - changed textures mipmap levels
  - new tooltip and ingame font
- Improved graphical effects
  - Improved explosion effects
  - Improved shield effects
  - Improved mining beam effects
  - Improved transporter effects
  - Improved special weapon effects
- Model and texture improvements or complete reworks
    - Federation Mediterranean
    - Federation Mandril Class
    - Federation Newton Class
    - Federation Monsoon Class
    - Federation Canaveral Class
    - Federation Akira Class
    - Federation Remore Class
    - Federation Outpost
    - Federation Storage Dock
    - Federation Antares Type Yard
    - Federation Eraudi Type Yard
    - Federation Starfleet Science
    - Federation Starfleet Engineering
    - Federation Defenceplatform
    - Federation Satellite
    - Klingon K't'inga Class
    - Klingon Topmey Class
    - Non Player Breen Cruiser
    - Non Player Ferengi Marauder
    - Mapobject Dilithium Moons
    - Mapobject Tritanium Moons
    - Mapobject Planets
    - Mapobject Nebulas
- Increased the differences between weapon ranges
- Changed the system your points are being calculated in the Admirals Log
- Several button upgrades
- Changed the Hotkeys of most units. They are now displayed in the construction
   tooltips
- Moved the resource panel in mapeditor mode do avoid menu blending
- Added new Teamcolors
- Balance changes:
  General:
  - Noxter
    The Noxter are not included in this Version. There were some critical
     bugs within the Beta 2 Noxter. The Noxter will of course come back
     after a global redo in a future version of Fleet Operations!
     Visit www.fleetops.net for updates on the Noxter
  - Borg
    The Borg are not included in this Version. We are still working on a
     full scale redo with many new features for the final Version 3.0
     release. Visit www.fleetops.net for updates
  - Changed the hitpoints and firepower of most vessels & stations according
     to the new balancing system. See "Unit attributes"
  - Changed the Dilithium and Tritanium costs of most vessels & stations
     according to the new balancing system
  - Changed the special energy costs of most special abilities and adjusted
     their effects according to the new balancing system
  - Dilithium
	stock at game start increased to 4000 (bf: 3500)
  - Crew
	Crew is no longer a resource. The "costs" will still be displayed
	to show the crew capacity of the starship or station.
  - Starting Units
	construction vessels must be build after the selection of an avatar
  - Hull HP regeneration is now calculated independet from shield HPs
  - Changed the Credit system. The standard startup creditcount is 20, but
      credit costs have also been scaled.
  - Added an option to turn credits off (= 0 startig credits).
  - Reduced the rate at which special energy regenerates. See "Unit attributes"
  - Tachyon Scan energy costs reduced to 30 (bf: 100)
  - Tachion Detection Grid research does now contain 2 upgrades.
     One for the Tachion Scan special ability and
     one for the passive Tachion Detection Grid ability
  - Crew Mechanics
    If a starships enters combat and gets hits on its hull it may lose crew. As
     soon as the crew drops below a certain limit (the limit differs from ship
     to ship and from race to race) it enters yellow crew state (the crew
     display turns yellow). If this vessel loses even more crew it enters
     red crew state (the crew display turns red).
     If a vessel enters yellow crew state the repair rate of that vessel
     is slightly reduced and the rate of fire is reduced by half.
     If the vessels enters red crew state the time it takes to repair damage
     is even doubled and the combat capabilities of that vessel get down to
     a minimum.
     The only exception to this rule is the Federation. A Federation vessels
     gets no repair rate reduction on yellow state and only half of the normal
     reduction on red state.
  - Unit attributes
    Added 3 dynamic unit attributes. These attributes describe the
     capabilities of vessels or stations.
     Offensive Value:
      The offensive capabilities of a starship or station. Units with higher
      offensive values can deal higher ammounts of damage or have faster
      reload times
     Defensive Value:
      The defensive capabilities of a starship or station. Units with higher
      defensive values can sustain higher ammounts of damage before getting
      critically damaged or destroyed
     System Value:
      The special system capabilities of a starship or station. Units with
      higher system values can reload their shields and repair their hulls
      faster. The system value does also define the special weapon energy
      recharge rate.

  Federation:
  - Newton Class
	is now armed with a Phaser
	can no longer be equipped with the Repair Team special ability
  - Descent Class
	can now be equipped with the Critical Shot Special Ability
	crew capacity increased to 600 (bf: 325)
  - Nebula Class
	now has the Vector Calculation special ability
	is now a Cruiser (bf: Battleship)
  - Probe Special Ability
	does no longer require a target but flies in the direction the Canaveral
	  is facing
  - Repair Beam
	does no longer use special energy
	repair rate reduced
  - Critical Shot Special Ability
	Does no longer deal damage
	does now last 7 seconds (bf: 5)
  - Defiant Class
	is now armed with 2 Rapid Fire Pulse Phasers
  - Saber Class
	can no longer be equipped with the Hyperimpulse Drive special ability
	is now initially equipped with the Evasive Maneuvers passive ability
  - Monsoon Class
	can no longer be equipped with the Hyperimpulse Drive special ability
	can now be equipped with the Proximity Photon Torpedo special ability
  - Starfleet Command
	crew capacity increased to 750 (bf: 600)
  - Galaxy Class
	crew capacity increased to 350 (bf: 250)
  - Steamrunner Class
	crew capacity increased to 150 (bf: 125)
  - Canaveral Class
	is now armed with 1 additional Light Quantum Pulse
  - Space Dock
	the Space Dock has been removed. It will come back as an enhanced version
	for special maps in a future Fleet Operations version
  - Remore Class
	combat effectiveness has been greatly increased
  - Akira Class
	can no longer be equipped with the Point Defense special ability
	can now be equipped with the Defense Patterns special ability

  Klingon:
  - Incrased the costs for all Klingon Special Ability research
  - Cho'naQ Class
	Weaponrange changed to long range (bf: medium)
  - Ion Storm Special Abilitiy
	reduced damage per second 
	duration decreased to 40 seconds (bf: 45)
  - Combat Tractorbeam Special Ability
	The Combat Tractorbeam is now a special ability of the Chargh Class
	  (Avatar Unit). The Heavy Weapons Platform is no longer equipped with
	  this ability.
  - Vutpa' Class
	is now a Battleship (bf: Cruiser)
  - Kahless Station
	may now produce K't'inga Class

  Romulan:
    The Romulan techtree has been changed. The Generix Yard does no longer
      exist. Generix Class vessels can be produced at the Staryard and then be
      refitted just like Rhienn Class vessels.
  - Mogai Class
	is no longer equipped with the Cloak special ability
  - Stealth Field Special Ability
	does no longer reduce weapon accuracy
  - Sabotage Special Ability
	reduced the chance to do lesser damage
	can no longer be used to destroy civil vessels
  - Generix Class (Dreadnought)
	is no longer equipped with a normal cloaking device
  - Quadcobalt Torpedo
	increased area damage radius to 350 (bf: 250)
  - Multi Targetting Disruptor
	Weaponrange changed to long range (bf: medium)	
  - Tal'Shiar Academy
	crew capacity increased to 400 (bf: 350)
  - Serkas Class
	does now require the Tal'Shiar Academy to be built
  - Cloak Special Ability (Romulan only)
	reduced the time it takes to cloak and decloak to 1 second (bf: 3)
- Updated a few button graphics
- Updated symbols for Dilithium an Tritanium
- Updated GUI Files

Fixes:
------
- Cleared some code that writes an obsolete registry string not required anymore
- Fleet Operations only showed Splash screen on some computer environments.
- Fleet Operations starts with an empty map @ IntroEnabled=0 & WindowMode=1
- Launcher: Fixed shortcut to Network Help file
- Installer: Uninstall of previous detected version now working again.
- Corrected several tooltip and string errors
- Fixed a bug causing white squares to appear in some nebulas
- Added missing projectile effect for the Borg Nanite special ability
- Romulan Phase cloak does no longer consume 20 initial special energy
- Added Tooltips for construction delays
- Fixed a bug causing an arterror to appear while using construction delays
- Fixed a bug still caused out of sync problems
- Fixed a bug related to game crashes at the end of a game
- Fixed a bug causing game crashes during research process
- Corrected the wrong aligned Klingon Kahless Station footbrint buffer
- Loader: Rebuild from scratch
- Launcher: Rebuild from scratch
- Map Manager: Import dialog bitmap position fixed

--------------------------------------------------------------------------------
Version Beta 2 Patch 2                                         December 31, 2003
--------------------------------------------------------------------------------

Additions:
----------
- Added new maps:
  Maps by Creative2012
  - Homeworlds
  - Old Space
  Maps by Detektor:
  - Cube
  - Mutaracombat
  - Wheel of War
  Maps by Eufnoc
  - The Badlands
  Maps by SisQ
  - Total war
  Maps by Trekki
  - Blue

Fixes:
------
- Fixed a bug causing romulan pulse type weapons cause incorrect damage
- Fixed a bug causing 'out of sync' errors while using Tachyon Scan
- Saving Admiral's Logs is working again
- Readded the Noxter Breeder build button

--------------------------------------------------------------------------------
Version Beta 2 Patch 1                                          October 13, 2003
--------------------------------------------------------------------------------

Changes:
--------
- Improved Noxter-AI station-organism handling
- Balance changes:
  Noxter:
  - All Noxter organisms are now mobile
  - Larva
	Increased construction time to 25 seconds (bf: 20)
  - Scourge
	Weapon damage reduced to 3 (bf: 5)

Fixes:
------
- Noxter Stinger now dies in a blood explosion
- Fixed a bug causing crashes when leaving a game
- Fixed a bug causing crashes during game
- 60 second and 90 second construction delays fixed
- fixed a bug which marks an area where a swarmnebula has
  been used as unsuitable for station construction
- Corrected several tooltip and string errors
- Launcher: Now Team Colors can correctly be enabled/disabled

--------------------------------------------------------------------------------
Version Beta 2 Hotfix 1                                         October 04, 2003
--------------------------------------------------------------------------------

Fixes:
------
Missing Federation Descent class textures which caused slowdowns


--------------------------------------------------------------------------------
Version Beta 2                                                  October 04, 2003
--------------------------------------------------------------------------------

Additions:
----------
- Multiplayer name is set to Windows logon name after install
- Launcher: Added option for choosing between low and high res textures
- Launcher: Added option for disabling ship and station team colors
- Launcher: Added option for disabling the splash screen transition effects
- Launcher: Added option for long multiplayer nicknames (up to 64 chars)
- Launcher: New splash logo available: Ciadan
- Launcher: New splash logo available: Dominion
- Launcher: Updated splash logo: Borg
- Map Manager: Delete option now available
- Fleet Operations menu Cursors
- New building underlays when placing new buildings
- Added several new multiplayer maps
Gaming additions:
- New player race: Romulans
- New player race: Noxter
- Added 'buildable' delays (30/60/90 sec) for all shipyards
- Assimilated ships and stations now have modeled borg installations
- Added new ship, resource and build status bars
- Added new race specific cursors
- Added shield hit sounds for torpedo and pulse type weapons
- Added new map objects for map environment
- Added new formations
    - Lines formation
    - Square formation
    - Wedge formation
- Added new vessels and stations
  Playable:
    Federation:
    - Excelsior-II Class

    Borg:
    - Command Cube

    Klingon:
    - BortaS Class
    - Vutpa' Class

  Non Playable:
    Federation:
    - Premonition Class
    - Constellation Class
    - McKinley Type Yard

    Romulan:
    - Delevhas Class

    Bajoran
    - Impulse Ship

    Breen
    - Starbase
    - Yard
    - Energypost
    - Cruiser
    - Battleship

    Species 8472
    - Bioship

Changes:
--------
- Increased AI build efficency
- Race GUI changes
- Vessel and station model and texture improvements or complete reworks
    Replaced Models and Textures:
    - Federation Descent Class
    - Federation Galaxy Class
    - Federation Nebula Class
    - Federation Saber Class
    - Federation Starfleet Command
    - Borg Detector
    - Borg Interceptor
    - Borg Assimilator
    - Borg Adaptor
    - Borg Sphere
    - Borg Cube
    - Borg Fusion Cube
    - Borg Scout Cube
    - Borg Tactical Cube
    Updated Models
    - Klingon Bird of Prey
    Updated Textures
    - Federation Intrepid
- Replaced asteroid textures
- Ship pathfinding optimized
- MOTD now shows title of latest homepage news
- Added a new animation for collapsed shields
- Federation photon torpedos updated to 'First Contact' style
- Replaced explosion shockwave effect
- Balance changes:
  General:
  - Changed the Dilithium and Tritanium costs of most vessels according
    to the new balancing system
  - Changed the hitpoints and firepower of most vessels according
    to the new balancing system
  - Changed Engine speed for all vessels
  - Increased the speed of several special weapon projectiles
  - Pulse type weapons do now fire bursts of 3 shots
  - Sensor range increased for all vessels
  - Hull and System HP repair rate is now based on units hitpoints
  - Weaponrange firing system has been rebalanced to 5 weaponrange categories
	meele
	short range
	medium range
	long range
	artillery range
  - Tritanium
	storage capacity reduced to 100 per freighter (bf: 150)
	stock at game start increased to 2000 (bf: 1000)
  - Crew
	stock at game start decreased to 1500 (bf: 2000) (except: Borg)
	all races do now have the same boarding party strength
	crewrate per second reduced to 4.25 per starbase (bf: 5)
  - Special weapons and Special abilities
	reload time for most special weapons changed to 5 seconds
	unresearched special weapons are marked with an '+' in tooltips
  - Movement and combat behavior
	added movement autonomy to the AI menu.
	view tooltips for further information.

  Federation:
  - Venture Class
	removed Tachion Detection Grid ability
  - Tachion Detection Grid research
	costs increased to 400 Dilithium, 400 Tritanium (bf: 100/100)
	construction time increased to 100 seconds (bf: 25)
  - Defence Platform
	costs increased to 500 Dilithium, 250 Tritanium (bf: 300/250)
  - Tricobalt Torpedo
	damage reduced against ships
	double damage to buildings
  - ECM
	effect range decreased to 400 (bf: 600)
	enemy hit chance reduction decreased to 35 percent (bf: 50)
  - Distortion Field
	time decreased to 35 seconds (bf: 45)
  - Repair Beam
	special energy costs increased to 10 (bf: 5)
  - Eraudi Type Yard
	costs increased to 1000 Dilithium, 600 Tritanium (bf: 800/500)
  - Newton Class
	crew costs decreased to 90 (bf: 250)
  - Outpost
	can now be equipped with the Tachion Scan special ability
  - Spacedock
	can now be equipped with the Tachion Scan special ability

  Klingon:
  - NoQ'Duj Class
	removed Tachion Detection Grid ability
  - Chor Class
	costs decreased to 370 Dilithium, 110 Tritanium (bf: 450/125)
	damage increased to 10 (bf: 3)
  - Tachion Detection Grid research 
	costs increased to 400 Dilithium, 400 Tritanium (bf: 100/100)
	build time increased to 100 seconds (bf: 25)
  - Heavy Weapons Platform 
	costs increased to 600 Dilithium, 300 Tritanium (bf: 500/400)
	weaponry changed to Torpedo Drone
  - Kahless Station
	weaponry changed to Torpedo Drone
	can now be equipped with the Tachion Scan special ability
  - Combat Tractor Beam research
	does no longer use 1 credit to be researched
  - Ionizing Torpedo
	damage reduced to 5 (bf: 10-30)
	special energy costs reduced to 25 (bf: 75)
	targeting range reduced
  - K't'inga Class
	can now be equipped with the Torpedo Drone special ability
  - Field Research
	construction time increased to 60 seconds (bf: 50)
  - Armory
	construction time increased to 70 seconds (bf: 50)
  - Imperial Research
	construction time increased to 80 seconds (bf: 50)
  - Ordnance Depot
	costs decreased to 800 Dilithium, 600 Tritanium (bf: 1000/600)
	construction time increased to 80 seconds (bf: 50)
  - Field Yard
	costs increased to 650 Dilithium, 400 Tritanium (bf: 650/300)
  - Battle Yard
	costs decreased to 750 Dilithium, 500 Tritanium (bf: 1000/600)
  - Imperial Yard
	costs decreased to 1000 Dilithium, 600 Tritanium (bf: 1350/850)
  - Battleplan
	now uses 2 special energy per second (bf: 100 once)
  - Shield Breaking Torpedo
	energy costs reduced to 50 from 75
  - Mining Station
	costs increased to 450 Dilithium, 150 Tritanium (bf: 400/150)

  Borg:
  - Borg vessels do no longer need to lower shields to beam
  - Cube
	added the Regenerative Systems passive ability
  - Tactical Cube
	added the Regenerative Systems passive ability
	weaponry changed to 2x Gravimetric Torpedo
  - Fusion Cube
	added the Regenerative Systems passive ability
  - Detector
	removed Tachion Detection Grid ability
  - Tachion Detection Grid research
	costs increased to 400 Dilithium, 400 Tritanium (bf: 100/100)
	build time increased to 100 seconds (bf: 25)
  - Phaser Relay
	costs increased to 600 Dilithium, 300 Tritanium (bf: 400/200)
  - Assembler
	costs increased to 400 Dilithium, 100 Tritanium (bf: 400/0)
  - Adaptor
	costs increased to 350 Dilihtium, 100 Tritanium (bf: 300/100)
	damage decreased to 12 damage (bf: 15)
	reload time changed to 2 seconds (bf: 3)
	build requirements changed to Incubation Complex
  - Assimilation Matrix
	construction time decreased to 150 seconds (bf: 225)
	can now be equipped with the Tachion Scan special ability
  - Technology Assimilator
	targeting range reduced to 600 (bf: 800)
  - Holding Beam
	targeting range reduced to 600 (bf: 800)
  - Ultridium Burst
	damage reduced to 40 (bf: 50)
	energy costs reduced to 75 from 100
  - Nanites
	increased movement speed of GUI elements
	energy costs reduced to 10 from 40
  - Transwarp Hub
	construction costs reduced to 3000 Dilithium, 1500 (bf: 3500/3500)
  - Extended Life Support Nodes Upgrade
	research costs reduced to 450 Dilithium, 450 Tritanium (bf: 800/800)
	research time decreased to 50 seconds (bf: 100)
  - Processing Matrix
	costs increased to 650 Dilithium, 200 Tritanium (bf: 500/200)
  - Resource Assimilator
	costs increased to 400 Dilithium, 100 Tritanium (bf: 150/0)
	construction time increased to 40 seconds (bf: 20)
	loading capacity increased to 300 Dilithium, 200 Tritanium (bf: 150/100)
  - Assembly Matrix
	costs increased to 900 Dilithium, 500 Tritanium (bf: 900/450)
  - Adv Assembly Matrix
	costs increased to 1200 Dilithium, 700 Tritanium (bf: 1000/700)

Fixes:
------
- Map Manager: GUI fix
- Map Manager: Some glitches with creating new maps corrected
- Map Manager: Maps outsite the bzn folder were not correctly identified
- Map Editor: Envoirement settings for new maps corrected
- Launcher: Splash screens now appear really random when selected
- Added missing Admiral's Log ship and station images
Gaming fixes:
- Fixed several AI problems
- Vessels can now move again with "Trek Physics" enabled
- Fixed tooltip display errors
- Fixed display errors with some special effects
- Klingon Ionizing Torpedo can now be assimilated by the 
  Borg Technology Assimilator
- Federation Starfleet Chassis Tooltips now show +25 percent sensorrange
  improvement correct
- Added missing selection sounds for some stations
- Ships, stations and upgrades can now be reached via the F hotkeys

--------------------------------------------------------------------------------
Version Beta 1.1 Patch 2                                      September 21, 2003
--------------------------------------------------------------------------------

Fixes:
------
- Solved remaining issues causing "out of sync" errors
- Fusion Cube upgrade can no longer be targeted separately

--------------------------------------------------------------------------------
Version Beta 1.1 Patch 1                                         August 21, 2003
--------------------------------------------------------------------------------

Fixes:
------
- Fixed a bug causing "out of sync" errors in multiplayer games
- Fixed a bug preventing scoutcubes for forming up for fusion
- vessels no longer try to auto form up during movement

Changes:
--------
- Improved Pathfinding

--------------------------------------------------------------------------------
Version Beta 1.1                                                    May 06, 2003
--------------------------------------------------------------------------------

Additions:
----------
- Added 4 player map 'Circle' by Detektor
- Added 6 player team play map 'Nebulagate' by Detektor

Changes:
--------
- Minor GUI changes
- Added a new button for the Intrepid Class
- AI improvements
Balance changes:
- Start starbases are now fully recrewed
- Reduces the damage done by the Cho'naQ Assault Disruptor from 75 to 55
- Increased the fire rate bonus of High Energy Reflex Grid, Advanced Weapons
  and Nadion Weapons upgrades from 10 percent to 15 percent

Fixes:
------
- Installer now works with newer Armada CDs
Gaming fixes:
- Fixed minor interface display errors
- Fixed minor map errors
- "Critical Shot" can now be fired correctly
- "Critical Shot" can now be assimilated by the Borg Technology Assimilator

--------------------------------------------------------------------------------
Version Beta 1 Patch 1                                              May 02, 2003
--------------------------------------------------------------------------------

Changes:
--------
 
- Added missing ship images for Admiral's Log
- Added a new image for the Intrepid Class in Admiral's Log
- Increased Klingon Neutron Impulse Engine cost to 900 Dilithium, 900 Tritanium

Fixes:
------
 
- Fix for the DLL starting issues
- Fixed CD Check problems
 
Note: This patch includes all fixes from previous Hotfixes.
 
--------------------------------------------------------------------------------
Version Beta 1                                                    April 30, 2003
--------------------------------------------------------------------------------

First public release

________________________________________________________________________________
4.  Guide
¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯

    The unofficial Fleet Operations Guide can
    be found at http://guide.fleetops.net/
    
________________________________________________________________________________
5.  Additional Credits
¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
Special thanks to...
  all the people around the world who sent in bug reports and suggestions.
  Deemon for his Prometheus
  Fahres for his Intrepid
  Juhan Uus for permission to use his image art for Fleet Operations
  madshi for great development support
  Nullsoft and the NSIS Development Team for their great support
  Cleeve for his Vorcha textures
  Dak Phoenix for allowing us to use his Phalanx design
  MasterOfLan for offering webspace to host our big stuff :)
  Andrew J. Hodges for his Borg "Prime Sphere" design
  William Burningham for his B'rel textures
  damnedest creature for voice acting
  COB, DarkestVorlon, Dragi, Eufnoc, Overlord, SisQ, Trekki, admiralmax,
  Eximus, General_Martok, hyperion, omega, riker47988, Sfera, volume,
  zeich, Uberdan, Jan, Dominus_Noctis, Black Baron and all the others we
  might have forgotten to mention :)

  Also thanks to Firelight Technologies for making their FMOD Sound System free
  to non-commercial projects like Fleet Operations.

________________________________________________________________________________
6.  Contact Information
¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯
If you have any questions, comments or suggestions, we would like to hear from
you. For reporting bugs, please visit our forum.

    Web:    http://www.fleetops.net/