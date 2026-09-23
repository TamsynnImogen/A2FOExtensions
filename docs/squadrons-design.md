# Squadrons: feasibility and proposed design

Status: **native paid-reinforcement candidate, 2026-09-23**. The
[configuration and state foundation](../modules/A2FOSquadrons/README.md)
includes member/count parsing, membership, logical selection planning,
replacement reservations and structured snapshots. The squadron DLL now has a
checked outer Starbase completion path that builds real members. The user has
confirmed builds complete. This revision creates the members sequentially,
waiting for each native yard exit before constructing the next ship. Member
selection expands through Fleet Ops' checked selection path, with additive
peer calls after the initial click. A read-only ShipDisplay projection shows
one tile per squad while preserving physical command handles; the icon has
a live/configured count and the native health-bar providers combine surviving
members. Accepted native repair visits now enqueue one paid replacement at a
time after all survivors clear repair. The exact native queue ID and produced
Craft bind each replacement to its missing slot. Initial cost and build time
are derived from the count-weighted member totals. The repair hook installer
now tolerates FeaturePack's existing deletion hook and uses its exact-ID
cancellation export. Seventeen policy scenarios, seven x86 adapter groups,
resource/payment and grouped-cancellation checks, and offline checks of the
installed executable addresses pass. In-game reinforcement acceptance and native save
persistence remain outstanding.
This design is for **A2FOExtensions** in
`A2FOHookExtensions`, using its supported native Armada/Fleet Operations
runtime. It does not use the separate STA64 Rust simulation.

The user's design reference is **Warhammer 40,000: Dawn of War**. The intended
result is a squad-level build/selection identity backed by individual combat
units. This is an Armada adaptation; it does not copy Dawn of War's file
formats or imply that its other squad mechanics are required.

## Requested behaviour

| Action | Result |
| --- | --- |
| Build one squadron at a yard | One build-list entry and one construction job produce the authored group of real ships. |
| Look at its build image | A counter in the upper-right corner shows the full authored complement, e.g. `3`. |
| Select any member | The whole surviving squadron is selected and occupies **one logical selection slot**, including in mixed selections. |
| Look at its selected image | The same corner shows live/configured membership, e.g. `3/3`, then `2/3` after a casualty. |
| Move or fight | Members use their own native movement, collision, weapons, targets, damage and destruction. |
| Lose a member | The live count drops; its configured slot becomes vacant. |
| Repair at a compatible yard | Missing members are replenished **automatically during the repair cycle**. |

Upper-right placement and `live/max` formatting are proposed defaults; the
badge should be positioned relative to the image and configurable through
the GUI layout. It is drawn over the existing image, so modders do not need
separate artwork for every possible casualty count. A queued build's squad
size badge must remain distinct from the native queue quantity/progress UI.

## Feasibility

**Viable, but a substantial gameplay feature rather than a small UI change.**
The native engine can already simulate the individual ships. The extension
must coordinate their construction, identity, selection, replacement and
persistence. The counter itself is comparatively small work.

Current source provides useful integration points, but does not yet prove
the whole squadron lifecycle:

| Existing code | Useful capability | Work still needed |
| --- | --- | --- |
| [Producer API](../sdk/include/a2fo_module_api.h) and [FeaturePack ownership](architecture.md) | Shared admission, completion, cancellation and destruction events; one owner of queue hooks. | Atomic multi-member construction, cap reservation and reliable association of each output with its build job. |
| [RefitYards](../modules/A2FORefitYards/README.md) | Synchronized yard jobs, staging, native launch queues, selection/identity handoff and cancellation. | Squad-aware repair/reinforcement jobs. Its documented travelling/active refit state is not serialized, so it is not a complete persistence solution to copy. |
| [Turrets](../modules/A2FOTurrets/module.cpp) and shared Craft lifecycle dispatch | Linked real-object construction, cleanup and post-load reconnection patterns. | Independent moving members, persistent squad/slot identities and an association that survives representative death. |
| [A1Compat multi-ship UI](../modules/A1Compat/module.cpp), [CraftIdentity](../modules/A2FOCraftIdentity/README.md), [UI addresses](addresses.md) | Native selection tiles, wireframe layout, text rendering and checked presentation hooks. | Logical selection grouping, hit testing and command expansion; displaying one tile must not silently omit other members from orders. |
| [EnergySystems](../modules/A2FOEnergySystems/README.md) | Same-team yard/provider identification and live resupply checks. | A real repair-cycle detector: ammunition resupply is proximity-based, which alone does not establish that a squad is being repaired. |

One specific SDK gap: `A2FO_ProducerEvent` currently supplies `producer` and
`target_class`, but no completed Craft handle or unique queue-job token.
`FINISHING` is documented as an in-place completion claim, not a general
multi-output factory. A prototype must establish an output association at the
actual construction/publication boundary. Matching the nearest new ship or
all ships of the same class would be unreliable with simultaneous builds.

## Representation

Use an optional **`A2FOSquadrons.dll`** policy module. A squad is an
extension-owned identity with a fixed set of member slots. Each occupied
slot refers to a normal, mission-published native Craft.

```text
Squad identity: 42       Selection image: [Squadron       2/3]
  slot 0 -> ship A       own movement, hull, shields and target
  slot 1 -> vacant       casualty; eligible for yard replacement
  slot 2 -> ship C       own movement, hull, shields and target
```

There is no extra invisible controller ship and no shared health pool. A
stable squad identity belongs to the extension; it must not depend on the
continued existence of the first member. Choose the lowest surviving slot
as a deterministic native representative where a UI/helper needs one object.
Its death selects a new representative without changing squad identity.

Membership must not consume Armada's ten player control groups. A fleet can
contain several squadrons and ordinary ships, and a squad can belong to the
same control groups that normal ships can. Squadron membership and native
fleet membership are separate relationships.

## Building, selecting and fighting

The buildable squad definition owns the aggregate image, name and prerequisites.
Initial cost and build time are the sum of the members' values multiplied by
their configured counts. Member ODFs own individual ship statistics,
models, physics and weapons. One order constructs the complete initial complement. Admission
must validate all member definitions, prerequisites and the full unit-cap
requirement, with a reservation policy preventing competing jobs from using
that capacity before completion. Failed output must not leave a partly paid,
partly published initial squad.

For the smallest prototype, use **three identical ships**. The intended
configuration also supports mixed compositions; a three-fighter/two-bomber
squad is the next required authoring and replacement fixture. Total physical
ships still count toward simulation/unit caps even though the squad occupies
one selection slot.

Clicking or box-selecting any member selects its whole live squad once.
Shift-selection adds/removes the squad as a unit. Mixed selection displays one
tile per squad plus one per ordinary ship; the same rule applies to control-
group recall and tile clicks. Two squads of the same ship type remain two
distinct logical units.

Commands expand to live member handles in deterministic squad/slot order.
Use native movement and loose formation destinations: ships share the order
but steer and collide individually. Autonomous targeting remains per ship;
do not copy a representative's current target into all other members every
frame. An explicit player attack order can still name a common target, with
each ship executing it through its own weapon and movement logic.

The native selection and command payload limits must be measured. One UI
slot cannot be treated as proof that arbitrarily many physical members fit
the native command buffers. If expansion exceeds a verified limit, the
command bridge needs deterministic supported batching or an explicitly
validated extension protocol. Never silently drop excess members or raise an
array bound without evidence.

## A dedicated squadron ODF

The preferred authoring model is a **separate configuration ODF**, referenced
by the yard's build list. It names the ship ODFs and how many of each belong
to the squad. Membership is not enabled globally on those ship definitions:
the same ship ODF can be used in several squad compositions or built as an
ordinary independent ship elsewhere.

**Recognized by the policy parser and native ODF/build-list adapter:**

```odf
// fed_patrol_squad.odf: the buildable squad definition
classLabel = "squadron"
unitName = "Federation Patrol Squadron"
// Initial cost and build time are derived from the members below.

squadMember0 = "fed_fighter"
squadMemberCount0 = 3

squadMember1 = "fed_bomber"
squadMemberCount1 = 2

squadReinforceAtYard = 1
```

```odf
// On a shipyard, reference the squad definition once.
buildItem0 = "fed_patrol_squad"
```

This job produces **five real ships**, displayed as **one squad with `5/5`**.
Losing a bomber leaves `4/5`; repair restores a bomber to its vacant slot,
not whichever ship happens to be the current representative. Losing a
fighter restores a fighter. The counter derives its maximum from the sum of
the configured quantities, avoiding a second conflicting squad-size field.

| Configuration owner | Responsibility |
| --- | --- |
| Squad ODF | Build/selection identity and image, tooltip, prerequisites, member ODF/count pairs and reinforcement policy. |
| Member ODF | Hull, shields, crew, systems, weapons, physics, model, normal new-ship defaults, and costs/time used for initial aggregation and individual replacement. |
| Yard ODF | Offers the squad definition in its normal build list; supplies its normal production/repair/launch capabilities. |
| GUI configuration | Counter corner/offset, font, colour and optional `current` versus `current/max` formatting. |

The same squad definition should be the public reference for technology-tree
entries, AI production choices and supported scripted/editor spawning. Those
entry points need explicit adapters; a new classlabel alone does not make
every native factory understand a multi-ship result. Individual member ODFs
need not be exposed in the yard's public build list merely to replenish them.
The proposed default is that a compatible yard can build the **squad ODF**,
with an explicit compatibility mechanism only if later needed for dedicated
repair yards.

Require a valid ODF and positive integer count for each member row. Apply
strict per-row/total limits and deterministic numeric row ordering; reject
missing files, incompatible classes and recursive squad-within-squad entries.
Stable slots use `(member row, ordinal within row)`, retaining a vacant slot's
configured ship type through casualties and save/load.

`classLabel = "squadron"` is an extension-owned production descriptor, not a
sixth ship or an invisible aggregate Craft. The native queue still needs a
validated class/metadata adapter for cost, technology and presentation, and a
construction boundary that instantiates/publishes the member Craft instead
of trying to build the descriptor as one ordinary ship. Identifying that
boundary is part of the feasibility prototype, not a solved API assumption.
Initial construction charges the count-weighted sum of all members' normal
costs once, with build time summed the same way. The squad ODF's own cost/time
values are overridden. Publishing the real members must not charge them again.

## Casualties and repair replenishment

Destruction empties only the affected slot and updates the badge immediately.
Damage to a surviving ship does not lower the member count. If the last
member dies, the squad ceases to exist and outstanding replacement jobs are
cancelled; a yard cannot recreate an entirely destroyed squad from a UI icon.

The requested replacement is part of **repair**, not a separate manual
Reinforce button. The intended sequence is:

1. A repair order assigns a compatible same-team yard and sends all surviving
   members to its repair/staging area using native movement.
2. All survivors must arrive and remain assigned to that yard. One member
   near a yard cannot replenish a squad still fighting across the map.
3. During the accepted repair cycle, repair survivors and fill missing slots
   automatically, one completed replacement at a time. Restore the lowest
   vacant slot first; reserve it so concurrent callbacks cannot duplicate it.
4. Each replacement is a real yard output, launched through the normal exit
   path and joined to the existing squad. It moves into formation normally.
5. Complete the cycle when the surviving/replacement members have finished
   their required repair service and the complement is restored.

A **healthy but depleted** squad must also be eligible for repair. The native
individual-ship damage check may otherwise refuse a `2/3` squad whose two
survivors have full hull and shields. A replacement must not inherit a dead
ship's damage, XP or previous native handle; normal new-ship defaults are the
recommended starting policy.

Reinforcement should use the existing synchronized yard-job machinery while
the squad remains in repair. Exact scheduling against ordinary construction
and berth release is a prototype gate: prevent a squad waiting for its
replacement from occupying the only berth needed to launch that replacement.
Do not patch a second competing Producer queue hook or exceed its ten slots.

If repair is cancelled, the yard is destroyed/captured, survivors leave, or
the squad disappears, cancel outstanding jobs and release reservations.
Already published replacements remain real ships in the squad. Resource/cap
shortages should pause or reject the next replacement cleanly. Costs charged
for an unfinished job must follow a defined native-compatible refund policy.

**Chosen cost/timing (2026-09-23):** each replacement consumes its member
ODF's normal ship cost and build time, as requested by the user. Native queue
admission, progress and refunds handle these jobs. The current adapter waits
until all survivors clear native repair output, then builds while they remain
near the yard. Stop/idle keeps waiting; cancelling the replacement queue item
or issuing a movement order ends replenishment. See the module README for
staging limits and the in-game acceptance checklist.

## Save/load, ownership and special actions

Persist squad identity, composition, stable slots, ownership, pending
replacement reservations and repair association. Reconnect native handles
after load; never serialize raw pointers or assume post-load object order.
The existing label-reconnection pattern is useful evidence, but its use must
preserve mission labels and user-visible craft identity. Queued reinforcement
also needs a stable job identity to prevent a second charge or duplicate
member after loading.

Recommended ownership policy for review: a captured/assimilated member
detaches and becomes an ordinary ship of the new owner; its old slot becomes
vacant. Ownership changes must never let a squad command enemy ships.
Refit, separation/merge weapons, scripted replacement, transport and recrewing
need explicit policies before their combinations are enabled. No free-form
merging/splitting or squad-wide veterancy is required for the first version.

Simulation decisions must use synchronized events and stable ordering. UI
selection, render timing, local wall-clock time and unordered pointer sets
must not decide who spawns or pays. Manual in-client and two-peer checks are
required before claiming visual completion or multiplayer support.

## Prototype order and acceptance

1. **Construction and identity:** one authored three-member job; all three
   are ordinary Craft; no partial output or cap/cost bypass; deterministic
   membership survives the first member's death. Then build a mixed
   three-fighter/two-bomber config referenced by one `buildItem` and confirm
   that the same member ODF can still be built independently elsewhere.
2. **Selection and counter:** select any member, show one image with `3/3`,
   then `2/3`; mix two squads and an ordinary ship in one selection/control
   group; verify every survivor receives orders and can target independently.
3. **Repair:** a `2/3` squad returns, repairs and automatically becomes `3/3`;
   repeat with full-health survivors, a blocked yard queue, cancellation and
   yard destruction. In a mixed squad, restore the exact missing member type.
   Check native launch and repair-berth deadlock handling.
4. **Persistence and compatibility:** save/load during partial construction,
   combat, repair and replacement; verify costs, stable slots and no duplicates.
   Exercise FeaturePack queues, RefitYards, HybridBuild and A1-style layouts,
   then deterministic multiplayer and pathfinding performance.

The first user-visible prototype must include the one-slot selection and
counter, plus automatic yard replacement. Intermediate engineering tests may
use native multi-selection, but that is not completion of the requested UI.
