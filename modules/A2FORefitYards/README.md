# A2FORefitYards

`A2FORefitYards.dll` adds synchronized ship refits through an ordinary
Shipyard Producer queue.

On a source ship:

```odf
refitItem0 = "fgalaxy"
refitItem1 = "fanotherrefit"
```

On a `classLabel = "shipyard"` station:

```odf
buildHardpoint = "build"
refitHardpoint = "build"
```

The two hardpoint names must currently match. A source ship receives one Refit
navigation button. It is inserted into a currently unused root-palette control
after native and special-weapon actions are bound, so existing controls such as
teleport are preserved; pressing it opens a destination palette with the listed
classes and a native Back control. Selecting a destination orders the ship to
the nearest live same-team refit yard. Once it arrives, the destination enters
that yard's native ten-slot queue, so normal resource costs, `buildTime`, queue
wireframes, cancellation, and progress UI apply. The ship is held at the
shared build/refit hardpoint while its job is active. Its per-instance native
`avoidMe` state is temporarily disabled at that point, so the yard, source,
and construction result do not push one another away from the hardpoint. A
normal `GO_SINGLE` path first takes the source to an outside staging point on
the negative forward axis of `refitHardpoint`, matching the approach convention
used by Armada's repair and freighter queues. A short synchronized transition
then eases both position and orientation into the exact hardpoint over five
seconds of simulation time instead of teleporting or rapidly dragging it.
Halt or yard-side cancellation of an active refit inserts the original source
into the same native build-output queue used for a completed ship, so it leaves
through the yard's ordinary launch route. The saved `avoidMe` value is restored
after native `QueueEnter` has made the overlap safe. Yard destruction and
module shutdown also restore the value. On completion, the newly built object
inherits the original's selection/name/owner handoff and replaces it without a
combat death or wreckage event.

This first implementation intentionally does not serialize the refit sidecar.
Saving during a travelling, waiting, queued, active, or ejecting refit is
unsupported.
