# TODO

## Architecture and Refactoring

* [x] Make Lua destruction callbacks declare which ODF fields they require. The legacy Lua form retains a deprecated wreckage compatibility shim; the core itself no longer hard-codes those fields.
* [REFACTOR] Move more cocoon-specific state and selection logic from `A2FOExtensions.dll` into `A2FOFeaturePack.dll`.
* [x] Rename the `ODFRecursive` source folder to `A2FOFeaturePack`.
* [x] Document hook ownership and add a core-owned native destruction dispatcher. See `docs/architecture.md`.
* [x] Add compatible API revision/capability checks for Lua scripts and native modules.
* [x] Make native-module and Lua-script registration transactional so failed initializers cannot leave dangling callbacks.
* [RESEARCH] Investigate safely unloading and reloading Lua scripts during development.

## Build and Repository Cleanup

* [x] Remove `ExampleModule.dll` from release builds while retaining it as an SDK example.
* [x] Add `.gitignore` entries for:
  * `build/`
  * log files
  * generated DLLs and executables
  * temporary compiler files
* [x] Replace temporary Stage notes as the primary documentation with `docs/architecture.md`, retaining the old notes as labelled history.

## Performance and Diagnostics

* [PERFORMANCE] Add optional profiling for:
  * hook invocation counts
  * callback execution time
  * module execution time
  * Lua callback execution time
* [PERFORMANCE] Ensure profiling and verbose logging can be disabled in release builds.

## Features to Investigate

### Expanded Construction Queues

[IMPLEMENTED, PARTIAL MANUAL VALIDATION] Shipyards and construction facilities can use all ten native queue slots conveniently and can repeat an item continuously. Basic Ctrl-fill, Ctrl+Alt refill, and delete-to-cancel behaviour work in game. Save/load and the wider validation matrix remain pending. See `docs/queue-enhancements.md`.

Possible controls:

* **Ctrl + click:** Fill all remaining queue slots with the selected item.
* **Ctrl + Alt + click:** Begin continuously producing the selected item until the option is selected again or cancelled.
* Display a coloured overlay on the build button while continuous production is enabled, such as:
  * green for active continuous production;
  * yellow for paused or waiting.
* Stop continuous production when:
  * [ ] the player selects the command again;
  * [ ] the yard is destroyed;
  * [ ] the item becomes unavailable;
  * [ ] required technology is lost;
  * [x] the player manually deletes a queued item (confirmed in game; the remaining queue drains normally).

Questions to investigate:

* [x] The Producer and UI both have a ten-slot limit.
* [x] Continuous production refills the existing native queue.
* [x] Resource shortages pause production and retry periodically.
* Whether AI-controlled yards should be able to use the same system.
* Whether the controls can be exposed through configurable hotkeys.
* [x] Activation uses a synchronized typed-class marker and state is embedded in Producer save data.
* [x] Confirm basic Ctrl-fill, Ctrl+Alt automatic refill, and delete-to-cancel behaviour in single player.
* [ ] Validate active and resource-paused continuous production across save/load.
* [ ] Confirm a cancelled repeat remains cancelled across save/load and older unmarked saves still load normally.
* [ ] Complete the remaining single-player and two-peer multiplayer validation matrix.
* [ ] Add active/paused build-button overlays.

### Borg Features

#### Race-Specific Assimilation Replacement

[IDEA]

Replace the current mod workaround which uses an automatically firing special
weapon to detect when a vessel changes owner to the Borg and then swaps its
ODF. A native ownership-change/capture event could perform the replacement
directly, without requiring a hidden weapon on every compatible vessel.

Proposed unit ODF commands use indexed race/ODF pairs:

```text
capture0Race = "borg"
capture0Odf = "bor_galaxy"

capture1Race = "romulan"
capture1Odf = "rom_galaxy"
```

When the unit changes owner, the new owner's race would be compared with each
`capture<N>Race` entry. A matching `capture<N>Odf` would replace or transform
the captured unit into the faction-specific version.

Technical questions:

* Which Armada ownership-change function provides a single synchronized event
  for capture, assimilation, transfer, and scripted ownership changes.
* Whether the replacement should preserve position, rotation, damage, crew,
  special energy, veterancy, orders, hotkey groups, and object identity.
* How to prevent the replacement itself from retriggering the capture rule.
* Whether the mapping should apply only to assimilation or to every ownership
  change.
* How indexed pairs should be validated when an entry is incomplete or its
  target ODF cannot be found.
* How the transformation should persist through save/load and remain
  synchronized in multiplayer.

#### Collective Borg Experience

[IDEA]

Add a faction-selectable experience mode. The normal mode retains per-unit
experience, while the collective mode contributes experience to a faction-wide
pool and derives Borg ranks from shared thresholds.

Proposed faction ODF commands:

```text
xpMode = "collective"
xpMode = "individual"

collectiveXPRequired1 = 5000
collectiveXPRequired2 = 15000
collectiveXPRequired3 = 35000
collectiveXPRequired4 = 70000
collectiveXPRequired5 = 120000
```

`individual` remains the default and therefore does not normally need to be
declared. In `collective` mode, eligible units contribute to the shared pool
and receive the collective rank reached by the faction.

Proposed unit ODF command:

```text
inheritCollectiveRank = 1
```

The default is `1`. This allows newly built or assimilated units to inherit the
current collective rank immediately. It is particularly useful for multiphase
assimilation, where an initially assimilated unit later changes into its fully
assimilated ODF and should retain the rank already reached by the Collective.
Setting the command to `0` would allow special units to opt out of collective
rank inheritance.

Technical questions:

* Which combat, destruction, assimilation, or support events add experience to
  the collective pool and how duplicate awards are prevented.
* Whether existing eligible units update immediately when a threshold is
  crossed or only when they are created or transformed.
* How collective ranks interact with ODF-specific veteran bonuses and units
  which have fewer supported rank levels.
* Whether an assimilated unit contributes its previous experience to the Borg
  pool and whether its former faction's collective state is affected.
* How rank inheritance survives intermediate and fully assimilated ODF
  replacements.
* How faction-wide XP, thresholds, and inherited ranks are stored in saves and
  synchronized in multiplayer.

### Noxter Features

#### Seed and Breeder Production System

[IDEA]

* Replace conventional shipyards with Breeder organisms.
* Produce or unlock Seeds, each containing the genetic blueprint for a particular Noxter species.
* A Breeder consumes a Seed and undergoes metamorphosis.
* After metamorphosis, that Breeder becomes permanently specialised in producing the selected organism.
* The transformation is irreversible; a specialised Breeder cannot consume a different Seed.
* A specialised Breeder continually produces organisms rather than accepting ordinary one-off construction orders.
* Production consumes resources continuously because the Breeder must be fed.
* Allow production to be halted temporarily, but not indefinitely without consequences.
* A Breeder that remains unfed should eventually weaken or die.

Technical questions:

* Whether continuous production should use the existing construction queue or a separate spawning timer.
* How starvation state should be stored and restored in save games.
* Whether Breeder specialisation must be synchronised explicitly in multiplayer.
* Whether Seeds should be physical units, research items, abilities, or ODF-defined transformations.

#### Mother Influence Network

[IDEA] The Mother projects an initial influence area around herself.

Inside that area, the Noxter can:

* place most stations;
* specialise or activate Breeders;
* regenerate more effectively;
* use certain advanced abilities;
* receive coordination or combat bonuses;
* communicate with the wider swarm.

Specialised stations then extend the field:

```text
Mother
  └── influence area
        └── Relay organism
              └── extended influence area
                    └── Nest / Breeder / defensive organism
```

Each relay must remain connected to the Mother through overlapping influence areas. This would make it both a territorial system and a network system.

##### Disconnected Areas

For example:

```text
Mother → Relay A → Relay B
```

If Relay A is destroyed:

```text
Mother    Relay B
   ✕ connection
```

Relay B remains physically alive, but:

* it stops projecting influence;
* stations depending on its field become inactive;
* linked abilities stop functioning;
* nearby units lose swarm bonuses;
* Breeders may pause production;
* regeneration may stop or slow.

##### Visual Representation

* Display active influence as a translucent, animated nebula-like cloud.
* Make the field denser and brighter near Mothers and relay organisms.
* Fade or desaturate disconnected fields before they disappear.
* Use subtle spores, pulses, or biological particles to distinguish it from ordinary map nebulae.
* Ensure the edge remains readable enough for station placement.
* Consider different visual states for:
  * fully connected influence;
  * weakened or recently disconnected influence;
  * hostile or corrupted influence;
  * influence projected by different Mother strains.

Technical questions:

* Whether influence is calculated as overlapping circles, a connected graph, or both.
* How frequently connectivity should be recalculated.
* Whether moving units can temporarily project influence.
* How inactive stations behave when disconnected.
* Whether influence state must be stored directly in save games or rebuilt on load.
* How the influence network should be synchronised in multiplayer.

#### Bacteria Mining

[IDEA]

The loop becomes:

```text
Resource node
    ↓ infected by Noxter organism
Infested node grows a harvesting structure
    ↓ produces resource sacs over time
Collector gathers the sacs
    ↓
Digester consumes them
    ↓
Player receives dilithium or tritanium
```

The main difference is that the collector no longer extracts resources directly. The infected node performs the extraction and packages the material biologically.

##### Proposed Stages

###### 1. Infection

A Noxter seeding organism targets an unoccupied resource moon or asteroid.

During infection:

* the node cannot yet produce resources;
* the infecting organism may remain attached and vulnerable;
* construction can be interrupted;
* the infection may require Mother Influence.

After the growth timer completes, the node receives an organic structure wrapped around it.

###### 2. Maturation

The structure could begin with low productivity and mature over time:

```text
Larval colony → mature extractor → engorged extractor
```

Possible effects:

* faster sac production at later stages;
* more sacs can wait at the node;
* visual growth around the asteroid;
* destroying it resets all maturation progress.

###### 3. Sac Production

The infected node periodically creates a physical sac object.

For example:

```text
sacInterval = 12
maximumStoredSacs = 4
sacResourceValue = 75
```

If all storage points are occupied, production pauses until a collector removes one.

That gives enemies a reason to raid the extractor even when they cannot destroy it: accumulated sacs represent resources that have been produced but not yet secured.

###### 4. Collection

The collector travels to the infected node, attaches to or approaches a sac, and carries it back.

Possible visual representations:

* an object attached beneath the collector;
* a swollen cargo organ;
* a small object following the collector;
* an internal cargo count with a pickup animation.

The collector could potentially carry several sacs, depending on balance.

###### 5. Digestion

The collector delivers the sacs to a Digester organism.

The Digester destroys or consumes each sac and credits the resource to the player.

This means the Digester replaces the normal refinery role, but the fiction is entirely biological.

Technical questions:

* Whether the infected structure replaces the resource node or attaches to it.
* Whether stored sacs are physical map objects or abstract cargo slots.
* Whether enemy players can destroy or steal accumulated sacs.
* Whether collectors can carry multiple sacs.
* Whether standard mining AI can be adapted or requires a new order type.
* How sac production, collection, and delivery should be synchronised in multiplayer.
* Whether disconnected extractors stop producing, decay, or retain stored sacs.

#### Enemy Infestation Cycle

[IDEA]

```text
Enemy vessel
    ↓ infected by larva or spore
Incubation period
    ↓ internal damage and visible organic growth
Larva burst
    ↓
Ship destroyed or disabled
    ↓
Noxter organisms spawn from the wreck
```

##### 1. Infection

A specialised Noxter unit uses an infestation weapon on a valid target.

Possible restrictions:

* target must be below a health threshold;
* shields must be down;
* only certain ship sizes can be infected;
* stations may require a stronger infestation organism;
* Borg or fully synthetic targets could be resistant.

The infection should be a status effect rather than immediate control.

##### 2. Incubation

While infected, the enemy vessel remains under its original owner’s control, but suffers escalating effects:

* gradual hull damage;
* reduced crew;
* slower repairs;
* reduced weapon or engine performance;
* periodic loss of special energy;
* visible organic growth across the hull.

The owner then has time to react rather than losing the ship instantly.

Possible countermeasures:

* return to a repair yard;
* use a medical or cleansing ability;
* sacrifice or decommission the ship;
* destroy the infecting organism before implantation completes.

##### 3. Larva Burst

When the incubation timer completes—or when the infected vessel dies—the infestation erupts.

The burst could:

* destroy the host;
* spawn several small Noxter larvae;
* create one larger organism based on the host’s size;
* damage nearby vessels;
* leave an infected wreckage object;
* temporarily spread spores to nearby damaged enemies.

For example:

```text
Scout host       → 1 larva
Destroyer host   → 2–3 larvae
Cruiser host     → 1 mature combat organism
Battleship host  → multiple larvae plus a larger organism
Station host     → temporary nest or breeder
```

Technical questions:

* How infection state should be attached to and removed from engine objects.
* Whether infection timers should run through Lua, native modules, or a generic status-effect dispatcher.
* How host size or class maps to burst results.
* What happens when an infected target is destroyed before incubation completes.
* How cleansing, repair-yard treatment, or immunity should work.
* Whether visible organic growth requires model swapping, overlays, attachments, or particle effects.
* How infection state and spawned results are synchronised in multiplayer and save games.

##### Shared Infestation Framework

[API] Investigate a generic infestation system that can support both resource-node infection and enemy-vessel infestation.

The shared framework should consider:

* target validation;
* incubation timers;
* visible host effects;
* interruption and cleansing;
* transformation or spawned offspring;
* behaviour when the host dies prematurely;
* ownership and team handling;
* save-game persistence;
* multiplayer synchronisation;
* generic callbacks for Lua and native modules.

A generic API could allow feature scripts or modules to define:

```text
target type
infection duration
required conditions
periodic effects
completion result
premature-destruction result
visual state
cleansing conditions
```
