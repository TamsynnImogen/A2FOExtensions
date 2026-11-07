# A2FORecoveryTests

## Built-in integration

The core-provided module name has an independent menu policy, while this
diagnostic still requires its INI opt-in at startup. Enabling policy after an
INI-disabled startup does not install hooks. Pending snapshots are invalidated
across activation generations and the observer releases its busy guard; the
report directory is created only when active capture begins. The original
pinned SHA, base, and complete-body admission checks remain in force. Built-in
hooks stay resident and are not unpatched; the standalone SDK target remains
available.

Optional diagnostic module for checking recovered ArmadaL routines during play.
It calls the original routine once, captures its result, and runs recovered C++
on an isolated snapshot. It retains the original return value and live writes.
This first version covers two routines; it does not replace the executable or
establish whole-game decompilation correctness.

| Routine | Original VA | What is compared |
| --- | --- | --- |
| Engine context lookup | `0x0062b710` | Three accessed words and raw return value |
| Node transform lookup | `0x00639460` | Raw return value and all 136 node bytes, including the scaled transform cache |

The only supported input SHA-256 is
`63bd645d82aec75cdd7ee3e3271fb67bb54f4ae1d1636a736b0db620bd997e36`.
Initialization also requires PE32/i386, loaded base `0x00400000`, and the complete
17/187-byte original bodies. An existing detour or changed body makes the module
passive. See [implementation and evidence](../../docs/recovery-tests.md).

## Build and headless checks

From the repository root, with MinGW i686, an x86 GCC host compiler, Python 3,
and Wine installed:

```sh
make recovery-tests
make recovery-tests-signatures
make recovery-tests-test
make recovery-tests-native-test
```

Output: `build/modules/A2FORecoveryTests.dll`. These explicit targets do not add
the module to the default release or deploy it. The signature check reads the
installed EXE without changing it; an alternate input can be supplied with
`python3 tools/verify_recovery_test_signatures.py --exe /path/to/ArmadaL.exe`.

The native fixture uses private executable copies and an isolated temporary
report directory. It does not launch ArmadaL or validate live-game acceptance.

## Manual activation

1. Fully exit Fleet Operations. Preserve any existing configuration and copy
   the previous `A2FOExtensions.log` before another launch; it is overwritten.
2. Copy the DLL to the game's global `Data/modules/A2FORecoveryTests.dll`.
3. Copy [the example](A2FORecoveryTests.ini.example) to
   `Data/A2FORecoveryTests.ini`. This configuration is global, not inherited
   from the active mod. Leave `Enabled=0` for the initial passive check.
4. Select the module for an existing test mod. In that mod's `info.ini`, add
   `activeN=A2FORecoveryTests.dll` under `[modules]`, replacing `N` with an unused
   numeric index and retaining existing entries. Native DLLs belong in global
   `Data/modules`; a mod-local module directory is ignored.
5. Launch manually and check the log for
   `Disabled; passive, no hooks installed`. Exit fully, set `Enabled=1`, and
   restart to begin observation. For a short diagnostic run, `SampleEvery=1`
   and `MaxSamplesPerRoutine=100` make coverage easier to see. Restore the
   less frequent sampling setting for longer runs.

The module is passive when its configuration is absent or disabled. Invalid
values also make it passive. Boolean values must be `0` or `1`; integer values
must contain decimal digits only.

| Key | Default | Range / meaning |
| --- | --- | --- |
| `Enabled` | `0` | Master opt-in |
| `EngineContext` | `1` | Observe context lookup |
| `NodeTransform` | `1` | Observe node transform lookup |
| `CaptureFirstMatch` | `1` | Save the first match for each routine |
| `SampleEvery` | `1000` | `1..1000000`; attempt every Nth visit |
| `MaxSamplesPerRoutine` | `1000` | `1..1000000`; per-process attempt budget |
| `MaxReports` | `16` | `0..256`; shared file budget, including successful captures |

Hooks cannot be uninstalled through this SDK. The module and SDK host remain
resident after installation. Shutdown stops new observations; a capture already
writing may finish. Configuration changes and removing instrumentation require
a full process restart. This is a startup diagnostic, not a hot-reload module.

## Reports and replay

`A2FOExtensions.log` reports installed/passive status and per-routine counters:
`visited`, `attempted`, `matched`, `mismatched`, `skipped`, and `busy`. Summaries
appear after the first attempt, every 32 attempts, at the budget, and shutdown.
Reaching the attempt budget does not remove the pass-through hooks.
The bridge still saves/restores state on unsampled calls, so this diagnostic
adds overhead even after the comparison budget is exhausted.

JSON captures go to `Data/A2FORecoveryTests/`. Filenames include routine,
outcome, process ID, session tick, and sequence number. Existing files are never
overwritten. Each runtime record includes the input SHA and module build ID,
entry floating-point control, snapshots, actual and expected return values, and
the comparison outcome. Capture attempts consume the file budget even if a
write fails; log output reports the failure.

Replay a copied capture from the repository root:

```sh
make recovery-tests-test
python3 tests/recovery_tests_replay.py /path/to/capture.json
```

Exit zero means the saved comparison is reproduced, including a recorded
**mismatch**. It does not mean every capture matched the original. Inspect the
printed `outcome`. Exit one means malformed input or changed replay results.
The host replay needs the same x87 arithmetic support as the build checks.

## Manual acceptance still required

| Check | Expected evidence |
| --- | --- |
| Passive launch | Disabled message; ordinary test mod still works |
| Enabled launch | `Observation hooks installed`; no identity/body rejection |
| Load map, show several ships, move camera, issue ordinary orders | Both routine attempt/match counters increase; no new crash or visual change |
| Models using exact unit scale and non-unit scale | Node observations cover both paths; captures replay consistently |
| Budget exhausted | Attempts stop at the configured cap; ordinary play continues |
| Disable and restart | Passive message; no observation hooks installed in the new process |

Copy the log and captures before repeating the run. Zero attempts means no
coverage, not a passing result. A mismatch is a candidate for investigation:
the observer cannot lock game state against other threads. Changed captured
inputs are skipped, but cache interference with stable inputs can still produce
a candidate mismatch. Inspect the snapshot and reproduce it before changing the
recovered implementation.

Scaled arithmetic is compared only for finite basis/scale inputs, an empty x87
stack, masked x87 exceptions, and valid precision modes. Unsupported states and
failed memory captures are skipped while the original still executes. Exact
unit scale has no arithmetic and permits raw nonfinite matrix/translation bits.
Headless results and artifact identities are recorded in
[VALIDATION.json](VALIDATION.json). On 2026-09-30 the user reported that the
module ran without issues. The inspected game log confirms passive loading
(`Disabled; passive, no hooks installed`); the configuration file was absent.
At that stage, enabled observations and live comparisons were still pending. This
[runtime evidence](RUNTIME_VALIDATION.json) preserves the inspected log and
installed DLL identity separately from the earlier headless artifact.

The subsequent enabled run on 2026-09-30 installed both observation hooks. The
saved log records 288 engine-context matches and 64 node-transform matches,
with zero mismatches, skips, or contention in the latest summaries. Both saved
first-match captures replayed successfully offline. These are sampled results
at log capture time, not final session totals. The saved node capture used exact
unit scale; the aggregate counters do not establish live scaled-arithmetic
coverage. The runtime evidence retains both the passive and enabled runs.
