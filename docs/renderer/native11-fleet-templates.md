# Fleet constructor ownership for load templates

The later [native animation fallback checkpoint](native11-animation-fallback.md)
qualifies the owned playback hook when no clip classes are configured. The
custom-clip and complete-world authority limits described below remain open.

The load-template reader accepts the pinned ArmadaL.exe image (`4300ab…b00de`)
with the pinned FleetOpsHook.dll (`3fd18e…e4c11`) only when
`native11_template_fleet::qualify` verifies the live constructor chain. The
generated [profile](../../core/native11_template_fleet_profile.hpp) and
[checker](../../tools/check_native11_template_fleet_profile.py) pin the original
20-byte Root parser block at RVA `0x23faac` and both Fleet constructor wrappers.
This is an exact-image, exact-chain allowance, not a general patched-code rule.

The [helper](../../core/native11_template_fleet.cpp) checks that the parser
block has the expected prefix and calls the Fleet outer wrapper; the Root
constructor entry pushes the Fleet inner wrapper and returns; the Fleet pointer
chain resolves back to the Root constructor; and Fleet's saved-original slot
points to an executable 16-byte stub containing the relocated first ten native
constructor bytes followed by a return to the native continuation. Both wrapper
byte ranges must match after HIGHLOW normalization and reside in executable
Fleet image memory. Qualification also requires exact Root and Fleet identities
and pins the Fleet module.

`native11_template_fleet::normalize` restores the original parser bytes only
in the profile **comparison copy**. It requires the full 20-byte overlap and
current chain ownership; it does not rewrite game code. The animation clock
checks that ownership again when a cached `model_template` is requested, so
loss of the constructor chain refuses template access. The permitted result is
copied, immutable metadata from a successful completed model load. It does not
establish complete scene geometry or presentation authority.

Load-template admission is separate from runtime playback admission. The live
diagnostics found A2FOAnimations later detouring matrix playback at Root
`0x6161c0`. Load metadata still checks both channel-loading regions, the full
template construction profile, node/load observer ownership and model lifetime.
It does not normalize the playback detour or accept its animation behavior.
Time, runtime channels, view and LOD sampling retain the full clock profile and
remain unavailable when that profile changes. Explicit shutdown revokes both
domains; an observed node-owner loss also revokes both, including a loss found
after capture. Startup admission still requires the complete original clock
profile before callbacks begin.

The clock guard covers template capture after playback suspension, refusal of
runtime output, loader ownership loss/recovery, explicit disable, and node
ownership loss discovered during each time/channel capture. Fresh cache wrappers
are compared by retained immutable data; failed calls preserve the caller's
previous wrapper.

The reviewed Fleet constructor differs from the native constructor around a
removed null check and an SEH-related write. The chain check does not establish
whole-constructor equivalence or out-of-memory behavior. The saved stub check
pins the native continuation at constructor +10, not the remainder of that
loaded constructor body; resulting metadata still passes header and two-pass
node/LOD validation.

The final strict build passes [124/124 private runtime cases](../../build/native11/fleet-template-final-full-results.json).
Core SHA-256 is
`15b9a60885d454058aeac7db4329d2bf18902ccc2aae576c652027a7e2db0ae3`.
The [saved 90-second private run](../../build/native11/private-game-fleet-template90/bootstrap-receipt.json)
reports model 1 captured with **30 nodes**, one cached template and **25,610**
copied model bytes. The final image shows the animated legacy menu; the game
remained alive at the observation limit, the new fault-log delta is zero and
private process/display cleanup passed. All seven protected installed hashes
remain unchanged. The [checkpoint receipt](../../build/native11/fleet-template-milestone.json)
binds sources, artifacts and this private legacy evidence.

A2FOAnimations playback remains unqualified for the runtime pose producer.
No complete native frame, D3D11 gameplay, presentation takeover or deployment
is claimed. All implementation gates 53–61 remain open.
