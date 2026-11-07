# First Refit-derived modules — 18 October 2026

Two optional modules are implemented and built in the existing main
A2FOHookExtensions tree. Neither has been installed or enabled in a game.

| Module | Behavior | Defaults |
| --- | --- | --- |
| A2FOCameraControls | Multiplies the freshly parsed native right-drag pan coefficient; does not repeatedly multiply its previous output | Disabled; multiplier 2.5 |
| A2FOViewDistance | Raises finite nonnegative far-plane and object-culling limits to a configured minimum before the native culling-restoration copy | Disabled; example minimum 10000 |

These adapt the QOL concepts in Cedric Haase's `armada2-refit/qol/qol.c`,
pinned to `37d60079169b086dba4216fe55e05cd6015d66c0`. They use this
project's SDK module loader and checked patch API. They do not replace working
renderer, shell, animation, or networking modules. No engine file is modified
on disk. The upstream MIT notice is preserved under
`third_party/armada2-refit/` and copied into the candidate package.

## Engine bindings

Addresses below are preferred-base VAs for manual investigation; code uses
RVAs and the actual loaded image base. These bindings were checked against
all three files recorded in `build/refit-module-signatures.json`.

| Binding | Preferred VA | RVA | Evidence / operation |
| --- | --- | --- | --- |
| Pan parsed-key context | 0x00491766 | 0x00091766 | 25 exact bytes around the FASTSCROLL store |
| Pan hook | 0x00491777 | 0x00091777 | Replaces six-byte `FSTP [0x0070FBB4]` with checked CALL plus NOP |
| Pan coefficient | 0x0070FBB4 | 0x0030FBB4 | Original native float destination |
| Right-drag consumers | 0x005268D7, 0x005268F5 | 0x001268D7, 0x001268F5 | Both native FMUL operands independently checked |
| View hook/context | 0x00491BEF | 0x00091BEF | Replaces six-byte `MOV ECX,[0x006FCAC0]`; surrounding 21 bytes checked |
| Far plane | 0x006FCAB0 | 0x002FCAB0 | Raise only; non-finite and negative native values preserved |
| Object-culling limit | 0x006FCAC0 | 0x002FCAC0 | Raise only; native ECX result reproduced |
| Script restoration baseline | 0x006FCAC4 | 0x002FCAC4 | Following original instruction stores the adjusted ECX |

Admission requires the existing reviewed ArmadaL identity/profile, or the
complete pinned clean GOG file SHA-256. A timestamp-only canonical shortcut
is deliberately insufficient. Unknown executables remain passive. All
instruction bytes must match; the only normalized bytes are explicit absolute
operands that also have their own PE HIGHLOW relocation records. Required
operand RVAs: `0x91767`, `0x91779`, `0x1268D9`, `0x1268F7`, `0x91BF1`,
`0x91BF7`, `0x91C00`. Data words must belong to the image and be writable.
Foreign hooks and failed checked patches leave the module passive.

The assembly bridges make no C++ callback. Camera preserves integer
registers/flags and consumes the original x87 ST0 with the native FSTP.
View distance uses integer comparisons of positive IEEE-754 words to preserve
the complete original floating-point state. NaN, infinity, negative values
and negative zero are left untouched. No-op configurations install no hook.
The DLL is pinned before patching because this SDK provides no uninstall
operation. Shutdown disables future adjustments and leaves a resident native
passthrough; already parsed values require a process restart to restore.

## Verification

- Both Windows i386 DLLs built with the existing toolchain and `-Werror`.
- Portable settings/finite-value policy tests passed with assertions active
  even under `-DNDEBUG`.
- File audit: all 11 instruction/relocation checks passed for each of the
  local ArmadaL, clean GOG Armada2 and installed Roots ArmadaL inputs.
- Final headless Wine matrix: **27/27 console processes passed**.
- Native tests load the real built DLLs, map a reference EXE as data, emulate
  its declared HIGHLOW relocations, and exercise private instruction fixtures.
  They never execute the engine entry point, start a game, or automate input.
  All tested reference images mapped at `0x008A0000`, exercising relocation.
- Checks cover passive defaults, invalid/no-op settings, short API rejection,
  altered hook/context/consumer bytes, altered image identity, missing
  required relocation, patch failure, idempotent init, bridge results, parser
  counters and shutdown passthrough. Camera covers 12 x87 precision/rounding
  combinations. View covers 100 far/culling value pairs including NaNs,
  infinities and signed zero, checking register, flag and FP preservation.
- Independent read-only review found no remaining blocker in the reviewed
  runtime, fixture, import/export tables or notice prerequisite.

Evidence: `build/refit-build-tests-20261018.log`,
`build/refit-native-tests-20261018.log`, and
`build/refit-module-signatures.json`. The Wine runner needs an explicit,
already initialized disposable prefix; display variables are removed.

```sh
make refit-modules refit-modules-test build/refit_module_smoke.exe
python3 tools/verify_refit_module_signatures.py --exe ArmadaL.exe \
  --exe reverse_engineering/Armada2/Armada2.exe \
  --exe '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/ArmadaL.exe'
python3 tools/run_refit_native_tests.py --prefix /absolute/disposable/prefix \
  --exe reverse_engineering/Armada2/Armada2.exe \
  --exe '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/ArmadaL.exe'
```

These checks establish source/build, guarded admission and native bridge
behavior in fixtures. They do not establish loader timing in a real session,
visible in-game behavior, mission/save compatibility or multiplayer behavior.
The legacy whole-project suite was not run; this milestone uses focused tests.

## Candidate and manual acceptance

The candidate is `dist/A2FOExtensions-refit-camera-view-20261018.zip`.
It contains the two DLLs, disabled INIs, module READMEs, upstream source/license
notices, evidence logs and a SHA-256 manifest. No game EXE or replacement core
is included. Module selection remains a separate explicit installation step.

Use an existing compatible A2FOExtensions installation. Place the DLLs under
`Data/modules/`, the INIs under `Data/`, and retain the license. Add each
module name to an unused `activeN` entry in `[modules]` in `Data/info.ini`,
preserving the existing list. The example INIs remain `Enabled=0`; enable one
module at a time for acceptance, with a full process restart after changes.

Check fresh module logs for "Armed" and use the optional
`A2FORefit_QueryStatus` diagnostic to distinguish installation, arming and
observed parser visits. A nonzero counter does not establish visible success.
Confirm actual startup reaches these parser sites after modules are loaded.

Manually compare right-drag panning at multipliers 1 and 2.5, including after
configuration reparses; verify keyboard/edge scrolling. For distance, compare
maximum zoom, object clipping, fog/hidden objects, mission-script culling
overrides and performance. Test save/load and multiplayer separately before
claiming those modes supported. Restore disabled settings and restart for the
baseline. Deployment and all game acceptance remain pending.
