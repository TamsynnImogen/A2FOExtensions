# A2FOSwarmSystem

`A2FOSwarmSystem.dll` adds dynamic, ambient visual traffic to any Armada II or
Fleet Operations `GameObject` ODF. It deliberately does not spawn ships.
Each member is one shared-model `ST3D_Instance`, a transform, and a small
sidecar movement state.

```cpp
swarm0 = "fbee"
swarm0Count = 10
swarm0Scale = 1.0
swarm0Radius = 50.0
swarm0Hardpoint = "dock01" "dock02"
swarm0Interaction = "hp01" "hp02"
swarm0InteractionTime = 3.0
swarm0AvoidHost = 1
swarm0Separation = 1.0
swarm0InteractionCapacity = 1

swarm1 = "repairdrone"
swarm1Count = 4
```

Definitions may use indices `0` through `63`; gaps are allowed. A host is
limited to 256 members per definition and 1024 members in total so a malformed
ODF cannot accidentally exhaust memory during a match.

## Commands

| Command | Default | Meaning |
| --- | ---: | --- |
| `swarmX` | required | SOD/database name, normally without `.sod` |
| `swarmXCount` | `1` | Number of visual instances |
| `swarmXScale` | `1.0` | Uniform model scale |
| `swarmXRadius` | `50.0` | Maximum roaming distance beyond the host exclusion surface |
| `swarmXMinRadius` | `0.0` | Minimum distance beyond the host exclusion surface |
| `swarmXMaxRadius` | radius | Explicit maximum roaming distance |
| `swarmXMinSpeed` | `6.0` | Minimum movement speed |
| `swarmXMaxSpeed` | `10.0` | Maximum movement speed |
| `swarmXTurnRate` | `2.0` | Direction blend rate per second |
| `swarmXHardpoint` | host origin | Launch/return hardpoint list |
| `swarmXHardpointCapacity` | `1` | Concurrent return reservations per launch hardpoint; `0` is unlimited |
| `swarmXInteraction` | none | Interaction hardpoint list |
| `swarmXInteractionCapacity` | `1` | Concurrent approach/dwell reservations per interaction hardpoint; `0` is unlimited |
| `swarmXInteractionChance` | `0.35` | Chance to choose an interaction after a leg |
| `swarmXInteractionTime` | `3.0` | Seconds spent at an interaction point |
| `swarmXInteractionRadius` | `2.0` | Random offset around an interaction point |
| `swarmXReturnToHardpoint` | `1` | Allow occasional launch-point returns |
| `swarmXAvoidHost` | `1` | Keep visual members outside the host model's bounding sphere |
| `swarmXHostClearance` | `0.5` | Extra gap beyond the host and swarm-model radii |
| `swarmXSeparation` | `1.0` | Extra centre spacing beyond two members' model radii |

`swarmXHardpoint` and `swarmXInteraction` are read through Armada's native
ODF string-vector parser, including inherited definitions. The whole ODF is
re-opened through native `ParameterDB`, so loose files, FPQs, includes, and the
active Fleet Operations parent-mod chain retain normal engine semantics.

Positions and destinations are stored in host-local space and transformed on
every render. Swarms therefore follow translating and rotating ships as well
as stationary bases. Hardpoint destinations are refreshed while approaching
or dwelling. They follow native animated ancestors, and they follow their own
matrix channels when `A2FOAnimatedHardpoints` is active.

Host avoidance reuses the bounding sphere already calculated by Armada for the
host model and adds the swarm model's own scaled radius plus
`swarmXHostClearance`. Destinations inside it are projected to its surface;
movement that reaches it becomes tangential and slides around it. Segment
testing also prevents a very fast member from crossing the entire volume in a
single update. This remains lightweight and conservative: on a very hollow or
strongly concave SOD the sphere may keep traffic farther from visible geometry
than desired. Such a definition can reduce `swarmXHostClearance` or set
`swarmXAvoidHost = 0`, which restores the original host-origin radius meaning
and exact hardpoint destinations.

Members in the same numbered definition also receive short-range separation.
Their scaled model radii prevent visual overlap and `swarmXSeparation` adds a
small gap. This is resolved through three bounded sidecar relaxation passes,
not engine collision or physics. A member dwelling at a hardpoint retains its
separation offset rather than being reset onto the shared point every frame;
animated hardpoint movement is still applied to the separated formation.

Hardpoint visits are reservation-limited. By default only one member may be
approaching or dwelling at each interaction point and one may be returning to
each launch point. When every suitable point is occupied, the member chooses a
roaming destination instead. Completing any interaction or return dwell always
starts a roaming leg, preventing a member from repeatedly selecting nearby
hardpoints and turning them into permanent gathering areas. Set a capacity to
`0` only when unlimited congregation is specifically wanted.

Swarm members are omitted from save files. On load, the module discovers the
restored host and deterministically reconstructs its visual swarm from the ODF.
Removing or destroying a host releases all of its instances on the next bee
simulation pass.

## Runtime boundary

The module chains the stock `BeeColony::Simulate` and `BeeColony::Render`
passes after exact executable-signature checks. Armada's own worker-bee
visibility test is reused before rendering, so ambient traffic follows the
same visibility/spectator policy and cannot reveal a hidden host. Unsupported
executables leave both stock passes unchanged and log that the runtime is
disabled.
