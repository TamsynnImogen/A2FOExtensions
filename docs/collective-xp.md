# Collective faction XP

`A2FOCollectiveXP.dll` implements per-player collective XP by routing native
Fleet Operations XP rewards into a shared pool and resolving the owner's next
native rank when a craft is admitted to a Producer queue. Promotion calls the
game's native rank helper, which replaces the craft through the native
`ReplaceWeapon_mCreateReplacementVessel` path. This is the first native
integration, not a live-game-verified or feature-complete release.

## Configuration and rank data

Select the feature modules in the active mod's `info.ini`, retaining existing
rows and using free indices:

```ini
[modules]
required0 = "A2FOCollectiveXP"
required1 = "A2FOFeaturePack"
active0 = "A2FOCraftIdentity"
```

The optional CraftIdentity entry displays collective progress in its selected
panel XP bar when the progress export is available. Add these fields to a race
ODF to opt in:

```ini
collectiveXP = 1
collectiveXPModifier = 1
```

An absent or `0` `collectiveXP` leaves stock behavior. Only `0` and `1` are
accepted. The modifier defaults to `1` and must be finite and positive; invalid
configuration leaves the faction stock. It scales only the next-rank threshold
derived from contributing craft. Awarded XP is added to the pool unscaled.

Each participating class family uses Fleet Operations' explicit class data:

```ini
XPlevel = 1
nextrankXP = 100
nextrankClass = "mycraft_veteran"
```

`XPlevel` is one-based: the base class is level 1. Each `nextrankClass` link
must resolve to the next level, and its `nextrankXP` must be positive. The last
rank has no next class and contributes no threshold. The adapter reads
`originalClass` when an observed craft is already at a higher rank, then checks
that it resolves to a valid chain. It does not infer families from filenames.
Ambiguous, missing, malformed, or cyclic chains are omitted from contribution.

An enabled player's next-rank threshold is the sum of observed native craft's
next-rank XP costs, scaled by the faction modifier. The adapter currently
registers live Craft through `craft_event` and `observe`, requires positive
native XP metadata, and derives rank families from class data. Native
`ConstructionObject` instances are explicitly excluded by the engine type flag
`0x40000`; normal Producer construction creates the real Craft at completion.
Custom construction paths still require in-game verification. With no
current threshold, the collective level cannot advance and unspent XP stays in
the pool. Awards at maximum rank can still accumulate. Native craft are never
demoted: a captured craft already above its new owner's collective level
retains its native rank, and its effective threshold is based on the higher of
native rank and owner level.

## Award, admission, and promotion behavior

Ordinary, Sleeper, and Saboteur native XP award routes are intercepted for an
enabled owner. The adapter suppresses the individual craft XP write on the
collective route, including invalid reward cases, so the same reward is not
also applied as local XP. Craft observation and reconciliation run at native
simulation callbacks and lifecycle events; the module does not scan all craft
every frame. Bounded world/fleet reconciliation occurs at bootstrap, after
load, and after native promotion; craft membership changes are observed at
simulation and cleanup callbacks. Thresholds are cached and invalidated by
membership or configuration changes, then recalculated at rank changes.

One XP award can advance the collective level across several thresholds.
Individual craft catch up through the native rank callback by at most one
native rank per simulation tick. Promotion uses the native `Craft_mLevelUp`
helper and its replacement-vessel path, rather than custom craft allocation or
deletion. Queue admission resolves
the current owner's target class and checks technology for both the family base
and resolved class. The Producer admission snapshot fixes the target class,
cost, and build time at synchronized admission. If the owner levels while that
item waits, a deferred item is not reranked. On completion, the normal native
promotion path catches it up one rank per simulation tick; construction return
does not instantaneously grant all missed ranks.

## Known limits

- Fleet Operations' shared build menu does not have a safe per-owner class-list
  seam here. Menu icon, displayed name, cost, and tooltip remain those of the
  base class even when the synchronized Producer admission resolves a higher
  rank.
- Captures cannot be safely demoted. A craft keeps its native rank, and the
  current threshold uses the higher of the owner's level and the craft's native
  level.
- Preservation and transfer of existing module sidecars through the native
  replacement path have not been proven. Native replacement owns its normal
  state transfer; no custom delete or replacement state-copy logic is added.
- Saves append a 33-byte `CXP1` record for each enabled team, including enabled
  teams with no ships. With the module loaded and a team opted in, a missing or
  invalid record makes the module's load hook reject the load instead of
  resetting XP. Loading without the module or opt-in configuration is
  unsupported; stock load may not recognize the extension record and rejection
  is not guaranteed. Enabling the feature mid-match does not migrate an older
  save.
- The native hooks install only when the Armada identity and exact Jan_B
  FleetOpsHook `.text` CRC and instruction signatures match. Unsupported images
  remain on the stock/pass-through path.
- The native shipyard queue hook requires the updated FeaturePack. Without the
  optional CollectiveXP module the new resolver leaves the original queued
  class unchanged. CraftIdentity integration is optional.

## Native research record

The adapter's native offsets are tied to the verified Jan_B images. They are
guarded by Armada identity, the FleetOpsHook `.text` CRC, and byte signatures;
they are not portable RVAs for arbitrary game builds.

| Evidence point | Address / symbol | Use |
| --- | --- | --- |
| Race configuration callback | `A2FOCollectiveXP_ModuleInit` registers `race_loaded`; fields `collectiveXP`, `collectiveXPModifier` | Parse opt-in and modifier; enable native `Race::canGainXP` gate for opted factions |
| Rank-class callback | `A2FOCollectiveXP_ModuleInit` registers `class_loaded`; fields `XPlevel`, `nextrankXP`, `nextrankClass`, `originalClass` | Build and validate explicit rank chains |
| Ordinary XP award | FleetOpsHook RVA `0x1dcfc3`; `a2fo_collective_award` | Add native reward, unscaled, to owner pool |
| Sleeper XP award | FleetOpsHook RVA `0x13722c`; `a2fo_collective_sleeper_bridge` | Route special reward to owner pool |
| Saboteur XP award | FleetOpsHook RVA `0x1387dd`; `a2fo_collective_saboteur_bridge` | Route special reward to owner pool |
| Craft observation and native fields | `craft_event`, `observe`; craft enhancement pointer `+0x1a4`; enhancement rank-class pointer `+0x4`; `XPlevel` `+0xac`, `nextrankXP` `+0xb0`, victim award worth `+0xb4` | Track live Craft and derive rank contribution |
| Construction eligibility | Armada RVA `0xa3440` `ConstructionObject::ConstructionObject` sets type bit `0x40000`; `Producer::FinishBuild` at `0xb8f50` calls `GameObjectClass::Construct` at `0xcd390`; ConstructionRig finish at `0xaff90` disposes its construction object and calls Producer finish | Exclude construction objects; normal built Craft appears at completion, with custom construction paths still requiring live checks |
| Native rank transition | FleetOpsHook RVA `0x1dbda4`; `a2fo_collective_level`; `Craft_mLevelUp` -> `ReplaceWeapon_mCreateReplacementVessel` | Promote through native replacement, at most one native rank per Craft per tick |
| Team persistence and reset | FleetOpsHook RVAs `0x10e9c4` / `0x10ea90`; Armada RVA `0x96300`; `save_team`, `load_team`, `clean_teams` | Append/read 33-byte `CXP1` state and reset match state |
| Technology validation | FleetOpsHook RVA `0x120680`; `tech_allows` | Check family base and resolved rank class |
| Producer queue admission and pricing | FeaturePack synchronized queue callbacks; dynamic export `A2FOCollectiveXP_ResolveBuildClass`; `push_native_checked_once`, `queue_logical_one_checked`, `materialize_pending_queue` | Resolve at synchronized admission; resolved class binds native cost/build time; deferred jobs keep that class snapshot |
| Build menu presentation | Shared class-list path has no safe per-owner seam | Icon, name, cost, and tooltip stay on base class despite higher-rank admission |

Research fingerprints recorded for the implementation: Armada SHA-256
`63bd645d82aec75cdd7ee3e3271fb67bb54f4ae1d1636a736b0db620bd997e36` and
FleetOpsHook SHA-256
`0843058788c6ffd1138d69871e2d9b1911bb6be8ee06db42f59f56b9a4494a97`.

## Focused fixtures and build

The existing focused targets are:

```sh
make collective-xp-test
make collective-xp-native-test
make build/modules/A2FOCollectiveXP.dll
make build/modules/A2FOFeaturePack.dll
make build/modules/A2FOCraftIdentity.dll  # optional progress display
```

`collective-xp-test` runs the policy/persistence and queue-resolver fixtures.
`collective-xp-native-test` runs the x86/Wine bridge, native adapter, and actual
FeaturePack queue-receiver fixtures with display environment variables unset.
They execute bridge code and synthetic callbacks, not a live match. The queue
fixture checks that pending jobs materialize with their admitted class even
after the resolver policy changes. No deployment is implied by these
commands.

Verification on 2026-09-25: both focused targets passed, and CollectiveXP,
FeaturePack and CraftIdentity DLLs built. CollectiveXP imports only KERNEL32
and msvcrt. `git diff --check` passed. The broader `make test` reached the
documentation checks with one unrelated pre-existing failure: the untracked
`A2FOBreeders` README is absent from the module catalogue. Existing FeaturePack
compiler warnings remain. No DLL was deployed and no game was launched.

The code paths represented by fixtures include one-based rank-chain checks,
malformed/missing/cyclic links, threshold-only modifiers and unscaled awards,
maximum-rank accumulation, no-threshold pool retention, fleet loss, capture
without demotion, ineligible and disabled owners, transactional persistence
rejection, queue resolver fallback/rejection, class-chain admission, native
bridge paths, and simulated craft lifecycle. Fixtures are not evidence of
game-level visual behavior, live native object replacement, synchronization
across peers, or old-save migration.

## Implementation file inventory

Collective XP-specific module files:

- `modules/A2FOCollectiveXP/README.md`
- `modules/A2FOCollectiveXP/module.cpp`
- `modules/A2FOCollectiveXP/native_bridge.S`
- `modules/A2FOCollectiveXP/native_layout.hpp`
- `modules/A2FOCollectiveXP/collective_xp.cpp`
- `modules/A2FOCollectiveXP/collective_xp.hpp`
- `modules/A2FOCollectiveXP/queue_resolver.hpp`

Focused tests and native fixtures:

- `tests/collective_xp_test.cpp`
- `tests/collective_xp_queue_test.cpp`
- `tests/collective_xp_runtime_test.cpp`
- `tests/collective_xp_bridge_test.cpp`
- `tests/collective_xp_bridge_fixture.S`
- `tests/collective_xp_queue_runtime_test.cpp`
- `tests/collective_xp_queue_fixture.S`

Integration touchpoints also include `Makefile`, `README.md`,
`docs/modder-command-reference.md`, `modules/A2FOCraftIdentity/module.cpp`,
and `modules/A2FOFeaturePack/queue_enhancement.cpp`. This reference is the new
`docs/collective-xp.md`. Other pre-existing working-tree changes were retained.

## Manual game acceptance matrix

Run in a backed-up local test setup with the exact supported binaries. Record
Fleet Operations and Armada build identities and keep module logs with results.

| Case | Action | Expected observation |
| --- | --- | --- |
| Stock default | Load a faction with `collectiveXP` absent, then with `0` | Native per-craft XP behavior remains; no collective pool display or queue substitution |
| Opt-in and modifier | Enable one faction with modifier `1`, then test another at a different positive modifier | Only opted owners pool awards; awards remain unscaled and each owner's threshold uses its modifier |
| Invalid configuration | Try an enabled value other than `0`/`1`, and modifier `0`, negative, NaN, or infinity | Faction remains on stock behavior and log reports invalid configuration |
| Rank-chain data | Test a normal chain, missing next class, wrong `XPlevel`, cycle, and a max-rank craft with valid `originalClass` | Valid family contributes; malformed or ambiguous family is omitted; validated original family is recognized |
| Shared threshold | Add/remove live craft with different next-rank costs; destroy the fleet while retaining pool XP | Threshold follows adapter-observed membership; without a threshold the level does not advance and unspent pool persists. Verify in-progress construction exclusion separately |
| One and identical ships | Start empty, add one ship, then several of the same class | Threshold is zero when empty and the sum of each eligible ship's next-rank requirement otherwise |
| Decommission and transfer | Decommission a ship, then transfer another to an allied player | Old owner loses each contribution; receiving owner gains only the transferred ship; neither operation transfers pool XP |
| Same faction and AI | Use two human players and an AI with the same opted-in faction | Each player progresses independently through the same native award path |
| Late yard and completion | Build a yard after levelling, then queue and complete a ship | New admissions resolve the owner's level; completed ship adds its next-rank contribution on its first native simulation observation |
| Reward routes | Earn ordinary, Sleeper, and Saboteur XP; include an invalid reward fixture in controlled testing | Enabled owner pool changes once per valid reward; individual craft XP is not also granted |
| Promotion | Reach a threshold with multiple eligible crafts and observe the replacement | Pool overflow may cross several collective levels at once; individual craft catch up through native replacement, at most one rank per simulation tick |
| Capture | Capture a higher-rank craft with a lower-level owner, then give ownership to a higher-level owner | No demotion; effective target/threshold follows the higher native/owner rank |
| Queue snapshot | Queue a class, raise owner level while item is waiting, then finish it | Admitted class/cost/time stay fixed; native promotion catches up after completion one rank per tick |
| Technology | Make the family base available but resolved higher class unavailable, then reverse the case | Queue admission rejects when either required base or resolved class technology is unavailable |
| Menu display | Inspect icon, name, cost, and tooltip for a queue whose admitted class resolves higher | Shared menu remains base-class presentation; record this known limitation |
| Sidecar preservation | Promote a craft carrying existing modules and inspect all modules after replacement | Record outcome; preservation is unproven and blocks claiming full feature completion if it fails |
| Save/load | Save enabled teams with and without ships; load with module and same opt-in, then test a removed/corrupted record | With module/config enabled, a missing or invalid per-team record is rejected rather than silently resetting XP |
| Configuration mismatch | Load with module or opt-in absent | Unsupported; stock load may not recognize extension records and rejection is not guaranteed |
| Old-save boundary | Load a pre-feature save after enabling the feature mid-match | No migration is supported; behavior is unsupported and must not be assumed to reject safely |
| Network peers | Repeat queueing, award, promotion, and save/load with all peers using the same modules/configuration | Outcomes and synchronized admissions agree across peers; record separately from local fixture success |
| Unsupported image | Start with different Armada or FleetOpsHook signature/CRC | Hooks stay uninstalled/pass-through and native stock behavior remains |

Only mark the feature game-verified after the native replacement, persistence,
queue, peer, and sidecar cases have been exercised in game. The menu limitation
and capture behavior remain design limits until a safe native seam is
established.
