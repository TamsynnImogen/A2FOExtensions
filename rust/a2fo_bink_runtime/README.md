# Private Rust Bink runtime

This optional evaluation crate supplies the primary Armada/FleetOpsHook Bink ABI
inside A2FOExtensions. Ordinary builds do not enable it. See
[the integration record](../../docs/bink-rust-stage2.md) for exact scope and gates.

Decoder work uses Rust code, without loading Bink or calling FFmpeg. DirectSound
remains a Windows platform service; the existing FMOD device can be borrowed
under owned COM references. The output worker serializes buffer operations,
respects the driver write cursor, preserves unplayed PCM when rebasing, and joins
on BinkClose. Registering a zero device releases the borrowed reference and defers
any default-device creation until a later audio movie opens.

The eleven legacy export names cover Open, Close, DoFrame, NextFrame, Wait,
CopyToBuffer, Goto, SetVolume, SetSoundSystem, OpenDirectSound and GetError.
The installed `_BinkSetVolume@8` label deliberately maps to a three-argument
stdcall body: the checked FleetOpsHook adapter adds track zero and expects ret12.
Other providers and external BINKSND callback protocols are rejected explicitly.

A handle points to a stable thirteen-DWORD public prefix owned by a registered
Rust Movie. The prefix implements observed dimensions, count, frame/last-frame,
rate, read-error and size fields; native auto-preload flags and unobserved
internal structures are not reproduced. The supported Open flags are zero,
PRELOAD (0x2000), and FROMMEMORY (0x04000000); memory sources are snapshotted.
Single-track audio is supported; Goto flags must be zero and seeks decode prior
frames to preserve codec/audio overlap. The sound clock counts played samples,
with rational video deadlines and wall-clock fallback once audio drains.

CopyToBuffer supports the observed 24/32-bit RGB orders, RGB555/565 and alpha
variants with checked positive pitch, offsets, clipping, extents and mapped
writable destination pages. Integer YUV-to-RGB conversion has not been accepted
as native bit-exact. Alpha is unvalidated against real assets in this corpus.

Compile the game-facing DLL through `tools/build_rust_owned_dll.py --bink-runtime`
with matching macro-enabled core entry object, alternate export contract and
paired startup proxy. Import redirection maps the main DLL before FleetOpsHook;
the proxy therefore calls the optional early-attach bridge after FleetOpsHook
attaches, before the established deferred initialization. Do not mix this core
with an older proxy. The build retains the original 198 exports and adds twelve.

Host tests use `cargo test --locked --manifest-path rust/a2fo_bink_runtime/Cargo.toml`.
Set A2FO_BINK_TEST_MOVIE to an actual single-track, multiframe asset to run the
real-movie seek/skip/loop integration check. Windows unit tests include fake COM
objects and can run under a private Wine prefix without real audio devices or GUI.

The vendored decoder is GPL-3.0-or-later. This is a private evaluation build;
production licensing and redistribution remain an explicit integration gate.

The [stage-three surface experiment](../../docs/bink-surface-compatibility-stage3.md)
adds separate current/older DirectDraw format classifiers. The optional
`--bink-surface-runtime` main build exposes them after the unchanged 210-export
contract. It is isolated from the user-tested installation. The additional
16-bit writers are unaccepted and require the off-by-default
`rgb16-evaluation` feature; ordinary builds reject those modes before writing.
