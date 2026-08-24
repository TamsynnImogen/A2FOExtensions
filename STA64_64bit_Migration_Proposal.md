# STA64 64-Bit Migration Proposal

## Status

**Document type:** Architecture / feasibility proposal
**Project:** STA64 / A2FOExtensions
**Target:** Star Trek: Armada II / Fleet Operations compatibility stack
**Primary goal:** Gradually migrate Armada/Fleet Operations from a 32-bit engine dependency to a native 64-bit architecture without requiring a complete engine rewrite in one step.

---

# 1. Executive Summary

STA64 should adopt a staged hybrid-engine architecture inspired by the general approach used by modern remasters that retain an original gameplay engine while introducing a newer presentation or systems layer.

Rather than immediately replacing the entirety of Armada II / Fleet Operations, STA64 would initially keep the existing 32-bit game responsible for core simulation while moving selected high-memory and high-complexity systems into a separate 64-bit process.

Over time, individual systems would be reimplemented on the 64-bit side until the original Armada executable becomes unnecessary.

The transition would therefore look approximately like:

```text
Stage 1
Armada / Fleet Operations 32-bit
        +
A2FOExtensions

Stage 2
Armada / Fleet Operations 32-bit simulation
        |
        | IPC / shared state bridge
        v
STA64 64-bit services and renderer

Stage 3
STA64 64-bit engine
        |
        +-- Armada-compatible gameplay simulation
        +-- ODF interpreter
        +-- AI
        +-- pathfinding
        +-- missions
        +-- physics
        +-- rendering
        +-- audio
        +-- UI

Armada.exe no longer required
```

The main benefit is that STA64 does not need to become a complete replacement engine before it provides useful improvements.

Instead, the project can gradually "hollow out" the original engine.

---

# 2. Current Problem

Armada II and Fleet Operations remain fundamentally 32-bit applications.

With Large Address Aware enabled, a 32-bit process can normally address up to approximately:

```text
4 GB virtual address space
```

That address space must contain everything used by the game process, including:

```text
Game simulation
AI
Craft objects
Weapons
Pathfinding
Maps
ODFs
SOD models
Textures
Audio
Direct3D resources
Fleet Operations systems
A2FOExtensions
Additional modules
Runtime allocations
```

As STA64/A2FOExtensions adds new systems, the amount of memory pressure increases.

Higher-resolution assets make this worse.

For example, a heavily modified installation may eventually require substantially more memory for:

- 2K/4K textures
- normal maps
- emissive maps
- specular maps
- additional model data
- larger maps
- new UI assets
- additional factions
- custom resource systems
- expanded weapon and effect systems
- swarm units
- larger AI states
- new mission scripting
- additional runtime modules

Even if the CPU and GPU are capable of handling these systems, the 32-bit virtual address space remains a hard architectural restriction.

---

# 3. Important Distinction: Memory Limits vs Engine Limits

Moving to 64-bit removes address-space restrictions, but it does **not** automatically remove every existing Armada engine limit.

For example, the original engine may internally contain structures equivalent to:

```cpp
Craft* craftTable[2048];

uint16_t objectCount;

WeaponEntry weaponTable[4096];
```

A 64-bit process could have hundreds of gigabytes of available virtual memory and still remain limited to:

```text
2048 craft
4096 weapons
65535 indexed objects
```

if those limits are defined by fixed arrays, narrow integer types, or engine logic.

Therefore STA64 must treat two classes of limitations separately.

## 3.1 Memory-address limits

Examples:

- 2 GB / 4 GB process space
- allocation failures
- texture memory pressure
- large map memory pressure
- model cache size

These are substantially improved by moving systems into 64-bit processes.

## 3.2 Engine-architecture limits

Examples:

- maximum craft count
- maximum map objects
- hardcoded faction counts
- fixed command tables
- fixed weapon tables
- legacy save structure limits

These require explicit reimplementation or patching.

---

# 4. Proposed Architecture

The proposed design separates the application into two conceptual domains.

```text
+--------------------------------------------------+
| Armada / Fleet Operations 32-bit                 |
|                                                  |
| Legacy simulation                               |
| Legacy object state                             |
| AI                                               |
| Existing command logic                          |
| Existing ODF behaviour                          |
+-----------------------+--------------------------+
                        |
                        | STA64 Bridge
                        |
                        v
+--------------------------------------------------+
| STA64 64-bit Host                                |
|                                                  |
| Renderer                                         |
| Asset manager                                    |
| Texture system                                   |
| Model system                                     |
| UI                                               |
| Audio                                            |
| New effects                                      |
| Optional new simulation systems                  |
+--------------------------------------------------+
```

The STA64 bridge would synchronise the state required by both sides.

Examples include:

```text
Object ID
Object type
Race/faction
Position
Rotation
Velocity
Health
Shield state
Subsystem state
Orders
Target
Weapon fire events
Construction state
Resource state
Visibility
Selection state
Animation state
```

The 64-bit host would not initially need to understand every detail of Armada's internal simulation.

It would only need enough information to represent the current game state.

---

# 5. Memory Architecture

## 5.1 Current 32-bit model

A simplified example:

```text
armada2.exe / FleetOps.exe
---------------------------------
Simulation               500 MB
AI                       300 MB
Maps                     300 MB
ODFs                     100 MB
Models                   500 MB
Textures                1400 MB
Audio                    300 MB
A2FOExtensions           200 MB
DirectX/runtime          300 MB
---------------------------------
Total                   3900 MB
```

At this point the application is dangerously close to the 4 GB address-space ceiling.

Fragmentation may cause allocation failures even before the theoretical maximum is reached.

---

## 5.2 Hybrid STA64 model

After moving asset-heavy systems outside the Armada process:

```text
32-bit Armada process
---------------------------------
Simulation               500 MB
AI                       300 MB
ODFs                     100 MB
Legacy game state        250 MB
Bridge buffers           100 MB
Miscellaneous            200 MB
---------------------------------
Total                   1450 MB
```

STA64 could separately use:

```text
64-bit STA64 process
---------------------------------
Textures                5000 MB
Models                  2000 MB
Materials               1000 MB
Effects                  800 MB
UI                       500 MB
Audio                   1000 MB
Caches                  1000 MB
---------------------------------
Total                  11300 MB
```

The two processes have independent virtual address spaces.

The fact that STA64 is using 11 GB would therefore not directly consume the 32-bit Armada process's address space.

The practical limit becomes the machine's available RAM and swap/page-file capacity rather than the original 4 GB process ceiling.

---

# 6. GPU Memory

Moving rendering to STA64 would also provide a cleaner GPU resource model.

The 64-bit renderer could own:

- vertex buffers
- index buffers
- textures
- normal maps
- emissive maps
- specular maps
- material data
- particle buffers
- shadow maps
- render targets

Armada would only need to provide simulation state.

Conceptually:

```text
Armada
   |
   | object position / state / events
   v
STA64 Renderer
   |
   +-- mesh cache
   +-- texture cache
   +-- material system
   +-- GPU buffers
   +-- shaders
```

This would dramatically reduce the need for the legacy process to hold modern graphical resources.

---

# 7. Bridge / IPC Layer

Communication between the 32-bit engine and STA64 would require some form of Inter-Process Communication.

Possible options include:

- shared memory
- memory-mapped files
- named pipes
- local sockets
- ring buffers
- event queues
- a combination of the above

For high-frequency simulation data, shared memory is likely the most appropriate option.

For example:

```text
Shared Memory
|
+-- Frame Header
|
+-- Object State Array
|
+-- Weapon Event Queue
|
+-- Effect Event Queue
|
+-- Resource State
|
+-- Selection State
|
+-- UI State
```

A ring buffer could handle transient events:

```text
Ship fires weapon
Ship destroyed
Shield hit
Subsystem disabled
Unit constructed
Research completed
Resource depleted
Warp event
Transporter event
```

The bridge should use explicit versioned structures.

Example:

```cpp
struct STA64ObjectStateV1
{
    uint64_t object_id;

    float position[3];
    float rotation[4];

    float hull;
    float shields;

    uint32_t flags;
    uint32_t faction;
};
```

Versioning would allow the bridge format to evolve without breaking every subsystem simultaneously.

---

# 8. Object Identity

A critical design requirement is stable object IDs.

Pointers from the 32-bit process must **never** be treated as persistent IDs by the 64-bit process.

Instead:

```text
Armada pointer
      |
      v
Bridge object registry
      |
      v
64-bit stable ObjectID
```

Example:

```text
0x04A91C20 -> STA64 Object 0000000000000452
```

If Armada destroys and reuses the original memory address, STA64 must not mistake the new object for the previous craft.

A generation counter may therefore be useful:

```text
ObjectID:
index      = 452
generation = 7
```

---

# 9. Stage 1 - Existing Architecture

The starting point remains:

```text
Fleet Operations
        +
A2FOExtensions
```

A2FOExtensions continues providing:

- hooks
- extended classlabels
- new ODF commands
- new resources
- texture extensions
- rendering experiments
- compatibility systems
- Armada I compatibility
- module loading
- experimental gameplay features

This stage remains valuable because it provides information about how the original engine behaves.

That knowledge is essential for later reimplementation.

---

# 10. Stage 2 - 64-Bit Companion Process

Introduce:

```text
STA64.exe
```

as a native 64-bit process.

Initially STA64 may perform non-critical services.

Possible first systems include:

- diagnostics
- telemetry
- performance logging
- asset indexing
- texture conversion
- mod database handling
- mission preprocessing
- background asset preparation

This establishes IPC and lifecycle management without risking the core renderer.

---

# 11. Stage 3 - External Asset Manager

The next logical migration target is asset management.

STA64 could manage:

```text
Textures
Meshes
Materials
Shaders
Animation metadata
Audio assets
```

The legacy process could reference assets using lightweight IDs rather than owning the complete asset data.

Example:

```text
Legacy craft
   |
   +-- model ID 241
   +-- material ID 91
   +-- texture set ID 44
```

STA64 then resolves those IDs to 64-bit asset objects.

---

# 12. Stage 4 - External Renderer

The most significant memory improvement would occur when rendering moves into STA64.

At this point:

```text
Armada
|
+-- Simulation
+-- Commands
+-- AI
+-- Gameplay
|
+----> frame state ----> STA64
                        |
                        +-- Renderer
                        +-- UI
                        +-- Effects
```

The original renderer may initially remain active but hidden or minimised while STA64 proves that it can reproduce the world.

Eventually STA64 could become the primary visual output.

Potential rendering technologies include:

- Bevy
- wgpu
- Vulkan
- Direct3D 12
- another Rust-compatible rendering framework

A Rust/Bevy implementation has the additional benefit of aligning with the broader STA64/Sol Conquest development direction.

---

# 13. Stage 5 - Move High-Cost Simulation Systems

Once the external architecture is stable, computational systems can begin moving to STA64.

Good candidates include:

- pathfinding
- spatial partitioning
- collision queries
- projectile simulation
- swarm logic
- advanced formations
- new AI systems

Armada could initially request the result:

```text
Legacy AI:
"find path from A to B"

        |
        v

STA64 pathfinder

        |
        v

waypoint list returned
```

Eventually the old implementation can be disabled entirely.

---

# 14. Stage 6 - STA64 Gameplay Simulation

The final major stage is to replicate Armada-compatible gameplay systems natively.

This includes:

- ODF loading
- craft definitions
- stations
- weapons
- special weapons
- construction
- research
- resources
- faction logic
- AI
- physics
- commands
- missions
- saves

At this point STA64 becomes capable of loading Armada content without using the Armada executable.

```text
STA64
|
+-- ODF parser
+-- Tech tree
+-- Race loader
+-- Map loader
+-- SOD / replacement model loader
+-- Armada-compatible simulation
+-- AI
+-- Lua mission system
+-- Renderer
+-- Audio
+-- UI
```

The original executable is then only a compatibility reference.

---

# 15. Final Architecture

The desired final architecture is:

```text
                    STA64
                     |
       +-------------+-------------+
       |             |             |
       v             v             v
  Simulation      Rendering       Audio
       |
       +-- AI
       +-- Physics
       +-- Weapons
       +-- Resources
       +-- Commands
       +-- Missions
       +-- Pathfinding
       +-- Saves
       |
       v
Compatibility Layer
       |
       +-- Armada I
       +-- Armada II
       +-- Fleet Operations
       +-- A2FOExtensions
       +-- STA64-native extensions
```

STA64 would become a native 64-bit engine capable of reproducing Armada gameplay while adding systems that the original executable could never reasonably support.

---

# 16. Compatibility Philosophy

A major design goal should be:

> Existing mods should not require conversion unless they use behaviour that cannot reasonably be emulated.

Ideally STA64 should understand the original formats directly:

```text
ODF
BZN
SOD
TGA
DDS
SPR
TT
AIP
GUI files
race ODFs
tech trees
mission files
```

New formats may be supported alongside them.

For example:

```text
Legacy:
SOD
TGA
BZN

STA64-native:
GLTF / GLB
DDS / modern texture formats
Lua-enhanced maps
new save format
```

---

# 17. Advantages

## 17.1 Immediate memory relief

Moving high-memory assets out of the 32-bit process substantially reduces the chance of hitting the legacy address-space limit.

## 17.2 Incremental development

The entire game does not need to be rewritten before STA64 becomes useful.

## 17.3 Compatibility testing

Each new subsystem can be compared directly against the original implementation.

## 17.4 Safer reverse engineering

Existing behaviour can be observed while the legacy process remains authoritative.

## 17.5 Modern rendering

The project gains access to:

- modern shaders
- physically based materials
- modern texture formats
- improved lighting
- better particle systems
- modern GPU APIs
- higher-resolution assets

## 17.6 Larger future simulations

Once simulation systems move to the 64-bit side, STA64 can support substantially larger data structures.

For example:

```text
More craft
More projectiles
More factions
More resources
Larger maps
More AI state
Larger pathfinding graphs
More complex formations
```

## 17.7 Native Linux potential

A 64-bit Rust/Bevy core would make native Linux support substantially easier than attempting to modernise the original Windows-only binary indefinitely.

---

# 18. Disadvantages and Risks

## 18.1 Synchronisation cost

Two processes must remain synchronised.

Incorrect timing could produce:

- visual jitter
- stale objects
- duplicated effects
- command latency
- mismatched object state

## 18.2 CPU overhead

Copying large amounts of state every frame may become expensive.

The bridge should therefore transmit only required data and use compact memory layouts.

## 18.3 Reverse-engineering complexity

The Armada side must expose enough state for STA64 to reconstruct the game.

## 18.4 Renderer replacement difficulty

The legacy engine may assume that rendering and gameplay objects are tightly coupled.

## 18.5 Input ownership

Only one process should ultimately own keyboard, mouse, controller, and UI focus.

## 18.6 Save compatibility

As systems migrate away from Armada, save-game data will eventually require translation.

## 18.7 Determinism

If multiplayer compatibility is desired, authoritative simulation ownership must be very clearly defined.

---

# 19. Performance Considerations

The bridge should avoid copying every possible property every frame.

Instead, state can be divided into:

## High-frequency data

Updated every frame or simulation tick:

```text
Position
Rotation
Velocity
Animation state
Shield state
Current target
```

## Medium-frequency data

Updated only when changed:

```text
Orders
Health
Subsystem status
Resource amounts
Construction state
```

## Static data

Transferred once:

```text
Class definition
Model
Texture set
Faction
Hardpoints
Weapon definitions
Physics constants
```

This reduces bandwidth substantially.

---

# 20. Suggested Shared-Memory Model

```text
STA64_BRIDGE
|
+-- Header
|   +-- version
|   +-- frame_number
|   +-- object_count
|   +-- event_count
|
+-- ObjectState[]
|
+-- WeaponEvents[]
|
+-- EffectEvents[]
|
+-- ResourceState[]
|
+-- UIState
|
+-- CommandQueue
```

Commands can travel in both directions.

Example:

```text
STA64 UI
   |
   | Attack target 441
   v
Command queue
   |
   v
A2FOExtensions
   |
   v
Armada command system
```

Later, when STA64 owns gameplay:

```text
STA64 UI
   |
   v
STA64 command system
```

The user-facing interface does not need to change.

---

# 21. Migration Principle

The guiding architectural rule should be:

> Never replace two tightly coupled systems simultaneously unless required.

Example migration order:

```text
Asset Manager
      |
Renderer
      |
UI
      |
Audio
      |
Effects
      |
Pathfinding
      |
Physics
      |
Weapons
      |
AI
      |
Gameplay simulation
```

This keeps the original engine available as an oracle for expected behaviour.

---

# 22. Recommended Proof of Concept

The first serious STA64 proof of concept should be deliberately small.

## Phase 1

Create a 64-bit `STA64.exe`.

Establish:

- launch/attach lifecycle
- shared memory
- basic heartbeat
- version negotiation

## Phase 2

Expose a single Armada craft.

Transfer:

```text
Object ID
Position
Rotation
Class name
Race
Hull
Shields
```

Display the data in STA64.

## Phase 3

Render a primitive representation.

For example:

```text
Armada ship
      |
      v
STA64 cube / debug mesh
```

The cube should follow the Armada craft in real time.

## Phase 4

Replace the cube with the correct model.

```text
ODF
 |
 v
SOD / converted GLB
 |
 v
STA64 renderer
```

## Phase 5

Add:

- multiple units
- selection
- weapon events
- shield events
- destruction
- construction

If this works reliably, the architecture is viable.

---

# 23. Possible Development Milestones

```text
M0 - IPC experiment
M1 - craft telemetry
M2 - external debug renderer
M3 - real model rendering
M4 - texture/material pipeline
M5 - external effects
M6 - external UI
M7 - external audio
M8 - pathfinding service
M9 - physics service
M10 - weapon simulation
M11 - AI migration
M12 - native STA64 gameplay core
M13 - Armada executable optional
M14 - Armada executable removed
```

---

# 24. Relationship to A2FOExtensions

A2FOExtensions remains valuable throughout this transition.

Its role changes over time.

Initially:

```text
A2FOExtensions
=
feature extension DLL
```

Later:

```text
A2FOExtensions
=
legacy-engine integration layer
```

Eventually:

```text
A2FOExtensions
=
compatibility shim for original Fleet Operations
```

Features developed for A2FOExtensions also serve as behavioural documentation for STA64.

Each new feature answers questions such as:

- where is the original state stored?
- when is it updated?
- what systems consume it?
- what hidden assumptions exist?
- what data would STA64 need to reproduce it?

This makes current reverse-engineering work directly useful to the future 64-bit engine.

---

# 25. Long-Term Vision

STA64 should not be viewed merely as:

> "Armada II but recompiled for 64-bit."

The stronger goal is:

> A modern 64-bit engine that treats Armada I, Armada II and Fleet Operations as compatibility targets.

That distinction allows STA64 to retain the behaviour and mod ecosystem that makes Armada valuable while removing limitations imposed by an engine designed around late-1990s and early-2000s hardware.

The final result could support:

- native 64-bit memory
- modern multicore processing
- modern rendering
- native Linux
- modern controller input
- new UI systems
- larger maps
- larger fleets
- new resources
- directional shields
- advanced damage systems
- modern shaders
- higher-resolution assets
- new mission scripting
- improved mod loading
- legacy Armada compatibility

without requiring the entire system to appear in one rewrite.

---

# 26. Conclusion

The proposed STA64 architecture is effectively a gradual engine replacement.

The original Armada/Fleet Operations process would initially remain the authoritative simulation.

A 64-bit STA64 process would then take ownership of systems that benefit most from modern memory and processing capabilities.

```text
Legacy engine
    |
    | progressively loses responsibilities
    v
STA64
    |
    | progressively gains responsibilities
    v
Native 64-bit Armada-compatible engine
```

The process can continue until the legacy executable has no remaining responsibility.

At that point it can simply be removed.

In other words:

> **Do not rewrite Armada all at once. Replace it one subsystem at a time until nothing remains to replace.**

This approach provides a realistic bridge between A2FOExtensions as it exists today and the eventual goal of a completely native 64-bit STA64 engine.
