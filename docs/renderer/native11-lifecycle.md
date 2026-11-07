# Native model and texture retirement

Status: private integration candidate; **gate 56 remains open** for device,
restart and native game acceptance. No installed game files changed.

The exact Roots ArmadaL image (`4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de`)
has startup-only observers at database destructor RVA `0x220690` and base texture
destructor RVA `0x2427d0`. Complete reviewed bodies and relocation operands are
pinned by `tools/check_native11_lifecycle_profile.py`. Each observer preserves
entry GPRs, flags and FX state, calls retirement with a clean callback environment,
then tail-forwards a trampoline containing the five displaced bytes (three complete instructions).
The native destructor remains responsible for native cleanup.

The pinned FleetOpsHook image (`3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11`)
changes the model entry to `push 0x5a9d4bb4; ret`. That wrapper owns the
reference-count check; its saved original is a 16-byte executable stub with
the first ten native bytes followed by `push native+10; ret`. The checked
terminal cleanup route swaps Fleet's original-call slot at RVA `0x246a1c`
with an aligned compare-and-swap, retaining the saved stub as the observer's
forward target. Fleet's wrapper, native model destructor body and reference
counting remain untouched. The Fleet DLL identity, wrapper relocation, entry,
stub, slot and startup engine state are checked before arming. In the private
Wine mapping, `VirtualQuery` reported `MEM_IMAGE` with `PAGE_WRITECOPY`
(`Protect=8`) for the slot; the guard accepts that protection as well as
`PAGE_READWRITE`.

Copied model records are retired under the model store lock and its source epoch
advances. Texture retirement removes its current record and pending IO ticket, so
an old DDS/TGA callback cannot publish after destruction or pointer reuse. Existing
immutable snapshot owners stay readable. A changed record also revokes the world
pipeline source ticket, preventing pending work from publishing as a fresh frame.

Requested lifecycle ownership is armed before either asset producer can publish.
Missing or changed ownership stops capture. Trampolines have process lifetime. If
a patch writer may have changed the entry but ownership cannot be read back, its
continuation remains allocated and observation stays inactive. Only a successful
read proving the original entry remains permits releasing an unused trampoline.

## Verification

- Strict core/helper and private fixture builds pass.
- The earlier native retirement candidate passed **108/108** private Wine/Xvfb
  runtime cases with D3D8, D3D9 and D3DX disabled. The current candidate
  passes all [118 private runtime cases](../../build/native11/live-bootstrap-full-results.json),
  including the three bridge/native/Fleet lifecycle cases in
  [targeted results](../../build/native11/fleet-lifetime-targeted-results-3.json).
- The bridge fixture checks both observers across 12 x87 precision/rounding modes,
  full entry state and distinct fake-original return effects.
- A private pinned image fixture checks profile/relocation rejection, denied and
  partial patching, unreadable post-write entries, lost ownership and disable.
  It never invokes a native destructor.
- The separate Fleet guard checks the wrapper, saved-original stub and slot,
  startup refusal, atomic slot ownership, and mutation/recovery paths. It does
  not call the Fleet reference-counting wrapper or prove the slot swap in a live
  game process.
- The private 90-second candidate bootstrap log reports checked Fleet terminal
  cleanup and texture observers armed. It does not prove a destruction callback
  ran; model, TGA and sprite capture producers refused admission, so complete
  asset capture and native D3D11 takeover remain unavailable.
- The newer [live-ownership checkpoint](native11-live-ownership.md) passes
  122/122 private runtime cases. Its saved legacy-menu log reports scene,
  model, DDS, TGA, sprite, material, node and clock diagnostics armed, including
  one 25,610-byte model copy. Asset fixtures check reactivation and removal of
  stale database records after Fleet FOFS ownership loss. The first load
  template still refuses its loaded profile/model ownership, and no live
  destructor callback or device/restart cycle is established.
- Model/DDS/TGA fixtures check retained owners, unknown and repeated retirement,
  new versions after address reuse, and retirement during an in-flight commit.

The original bridge test's binary32-versus-x87 comparison failure is preserved
in the evidence. Its corrected test compares occupied 80-bit registers and tags.

## Remaining acceptance

Static destructor boundary ownership and private producer regressions do not prove
all native subclass cleanup paths, whole-game resource use, device loss or repeated
live match/menu transitions. Those remain required by gate 56 and the
[fixed completion gates](native11-stage51-65.md). Complete game-frame production,
presentation takeover and independent startup are still unavailable.
