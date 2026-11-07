# A2FOBreeders

Prototype Noxter-style biological production for Fleet Operations.

## Behaviour

`classLabel = "breeder"` aliases to the native `shipyard` Producer so a virgin
Breeder receives Fleet Ops' ordinary build palette. The `buildItem` entries are
**genetic choices**, not jobs built by the Breeder.

When the player clicks one:

1. `A2FO_PRODUCER_EVENT_ADMIT` intercepts the synchronized build request before
   Fleet Ops queues or charges it.
2. The module finds the nearest same-team object with `breederMother = 1` whose
   `breederRadius` contains the Breeder.
3. The chosen unit's `breederSeed` object is constructed at the Mother and given
   a native movement order toward the Breeder.
4. The Breeder is locked while that Seed is alive. If the Seed is destroyed,
   the Breeder unlocks and may choose again.
5. When the Seed enters `breederConsumeRadius`, the virgin Breeder is swapped
   into the chosen unit's `breederResult` ODF and the Seed expires.
6. The committed Breeder has no `buildItem` list. Its `breederProduces` entry is
   repeatedly admitted through Fleet Ops' native `Producer::PushChecked`, so the
   produced unit's ordinary build time and resource costs are paid each cycle.
   If admission fails (for example because resources are short), the Breeder
   remains idle and retries.
7. A successful admission counts as feeding the committed Breeder because the
   organism's normal resource cost has just been paid. While a paid production
   job is active the Breeder is fed. If it becomes idle and cannot admit its
   next organism, its starvation grace period begins. If production resumes in
   time, starvation resets; otherwise the Breeder expires.

Cannibalisation deliberately uses the game's ordinary decommission behaviour;
A2FOBreeders does not add a separate consume-Breeder command.

Only one new semantic classlabel is needed: `breeder`.

## ODF example

### Mother

The Mother can keep its existing classLabel.

```ini
breederMother = 1
breederRadius = 600
```

### Virgin Breeder

For this first increment the Breeder should inherit the normal fields required
by a working Fleet Ops shipyard, including a usable build/output setup.

```ini
classLabel = "breeder"

buildItem0 = "noxwarrior"
buildItem1 = "noxflyer"
buildItem2 = "noxtank"

// Optional; default 40 world units.
breederConsumeRadius = 40
```

### Buildable Noxter unit

The normal unit ODF tells the genetic-selection layer which physical Seed to
spawn and which committed Breeder ODF that Seed will create.

```ini
// noxwarrior.odf
breederSeed = "noxseed_warrior"
breederResult = "noxbreeder_warrior"
```

Its existing cost and `buildTime` remain authoritative. They are charged every
time the committed Breeder reproduces this unit.

### Seed

The Seed needs no custom classlabel or breeder commands. For phase one it should
be a normal movable Craft-compatible ODF because the module gives it a native
move order.

```ini
// noxseed_warrior.odf
classLabel = "craft"
```

### Committed Breeder

Do **not** give this ODF a `buildItem` list; that is what makes the genetic menu
disappear after metamorphosis.

```ini
// noxbreeder_warrior.odf
classLabel = "breeder"
breederProduces = "noxwarrior"

// Optional; default 180 seconds. 0 disables starvation for this breeder form.
breederStarvationTime = 180
```

It should otherwise inherit the same safe shipyard/build-output fields as the
virgin Breeder.

## Starvation / feeding

Starvation deliberately reuses the committed unit's **existing production
economics** rather than introducing a second upkeep currency. A successful
`Producer::PushChecked` means the next organism's normal costs were accepted,
so the Breeder is considered fed for that production cycle.

When the Breeder is idle and admission of the next organism fails, the module
starts `breederStarvationTime`. It continues retrying production every 0.5
seconds. Any successful admission resets the timer immediately. If the timer
reaches its limit first, the Breeder expires.

This means deliberately exhausting the swarm's resources can pause breeding for
a short time, but a committed Breeder cannot be left unfed indefinitely. A
production job already in progress never starves merely because its build time
is long: its resources were already consumed when the job was admitted.

`breederStarvationTime = 0` disables starvation for a particular committed
Breeder ODF.

## Determinism

The player click reaches the module through the existing synchronized Producer
command path. Mother choice is deterministic: shortest squared distance, then
lowest object handle on an exact tie. Seed construction and the local movement
order therefore occur while each peer is processing the same synchronized
request; the module does not emit a second network command.

## Phase-one limitations / test targets

- Waiting-for-Seed state is runtime-only. Saving while a virgin Breeder is
  waiting for its Seed is not supported yet. Committed state is represented by
  the committed ODF itself and therefore does not require the transient map.
- Mother range is enforced at click/admission time, but phase one does not yet
  grey or hide the genetic buttons when no Mother is nearby.
- Seeds must currently be movable Craft-compatible objects. Pod/non-Craft Seeds
  need a separate travel/attachment path.
- Starvation currently treats any failed admission as an unfed cycle. That is
  intentionally simple for the first test: resource shortage, a cap/tech block,
  or another native admission rejection all begin the same grace period.
- Automatic committed production currently calls Fleet Ops'
  `Producer::PushChecked` directly. The first Roots test must confirm that a
  committed breeder with no public `buildItem` list is still admitted. If FO
  insists that the target be in the class build list, the next increment should
  give FeaturePack a tiny hidden-production bridge rather than exposing the
  unit button again.
- Metamorphosis uses the same native Evolver identity-swap path already proven
  by A2FORefitYards.

## First test

Use one Mother, one virgin Breeder, one final unit, one Seed and one committed
Breeder ODF. Keep the Mother and Breeder close together and test:

1. Virgin Breeder shows the final unit button.
2. Clicking it does not put that unit into the Breeder queue and charges no
   unit cost.
3. A Seed appears at the Mother and moves to the Breeder.
4. Destroying the Seed before arrival unlocks the Breeder.
5. Allowing the Seed to arrive swaps to the committed Breeder and removes the
   build palette.
6. The committed Breeder repeatedly builds the final unit, paying its normal
   cost/build time each cycle; removing resources pauses new admissions and
   starts starvation.
7. Restore resources before `breederStarvationTime`; production resumes and the
   starvation timer resets.
8. Repeat without restoring resources and confirm the Breeder expires when the
   grace period ends.
9. Decommission a committed Breeder and confirm native decommission remains the
   cannibalisation/recovery path.
