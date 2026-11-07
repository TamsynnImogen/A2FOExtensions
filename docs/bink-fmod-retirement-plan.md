# Retiring Bink and FMOD Ex

Implementation update: see the [Rust Bink integration record](bink-rust-stage2.md).
The assessment below records the earlier baseline; the optional private runtime
and selected-mod import migration now exist. FMOD replacement is unchanged.

Assessment date: 30 October 2026. Target: eventually move the required movie and game-audio implementations into Rust in `A2FOExtensions.dll`, so `binkw32.dll` and `fmodex.dll` need not be distributed. This pass is read-only for installed files: no game/DLL execution, dependency installation, implementation change or removal.

## Decision

**Both are technically plausible replacements; neither is currently removable.** Bink is the more bounded first implementation target. FMOD is a game-audio engine replacement, beyond the modern movie playback already implemented in Rust.

| DLL | Observed minimum contract | Current gap |
| --- | --- | --- |
| `binkw32.dll` | Ten distinct imported functions for the primary Armada/FleetOpsHook pair; fifteen across the previously inspected older FCM/Orb variants. Installed library has 73 named exports. | Rust BIK video/audio decoding, engine-compatible frame/handle API, timing/seeking/volume/audio-provider behavior, buffer/texture presentation, and removal of all load-time/dynamic filename requirements. |
| `fmodex.dll` | FleetOpsHook imports 36 distinct flat C API names. Installed FMOD Ex 4.38.06 has 698 named exports, including C++ exports that are not in that scoped imported set. | Rust sound/music decoding, streaming, mixer/voices, 3D listener and sources, attenuation/doppler, priorities/audibility groups, pause/fade/volume, device/output behavior and lifecycle compatibility. |

The scoped imported sets are verified minima, not a promise that third-party modules or every installed mod use no other API. Replacing all 698 FMOD exports is unnecessary if the supported product's complete reachable contract is smaller; that reachability still needs to be established.

## Current binaries and contracts

The main import inventory is [the 75-image audit](../build/dll-consolidation-audit-20261030/remaining-dll-evidence.json). Its limits include dynamic/plugin loads, actual DLL search-path selection, unexamined bundled launcher payloads and one uninstaller delay-table diagnostic. Refreshed source and binary pins, import lists and asset observations for this assessment are in [the retirement evidence](../build/bink-fmod-retirement-20261030/evidence.json).

| Live file | SHA-256 |
| --- | --- |
| `Data/binkw32.dll` | `9c2b75defd3bf13737fdb6e8b7cd3b67d7b2d6fe0bbf48ec6dc2ecf1b03e559b` |
| `Data/fmodex.dll` | `ab58aa4597acd57036764f386c2925d5d51708e582286b6a51156a6c82ca9721` |
| `FleetOpsHook.dll` | `3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11` |
| `Data/ArmadaL.exe` | `4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de` |

The base, Fleet Ops 4.0 and STA2 Classic FMOD copies are byte-identical, 32-bit, 1,086,464-byte images. Version **4.38.06** is resource metadata; the runtime-reported version was not queried by executing the library. The observed imports use the flat C API, rather than the DLL's C++ decorated interfaces. Calling conventions, parameter sizes, result codes and handle lifetimes still require native ABI tests before implementation; modern FMOD Studio documentation is not an ABI specification for this old version.

## Bink: decoding is only part of the work

Current `rust/a2fo_media/src/codec.rs` registers H.264/VP9 video and AAC/Opus/Vorbis audio in MP4/MOV and MKV/WebM containers. It has no BIK reader or decoder. A2FOVideo still falls back to native Bink, and the stage-eight Rust movie workflow still invokes native playback/presentation callbacks. Those Rust components do not replace `binkw32.dll`.

The earlier 438-file loose BIK list was refreshed: all files remain present with the same sizes and four-byte tags (345 `BIKf`, 93 `BIKi`). This does not hash/validate complete contents or discover newly added files. The selected base/Fleet Ops 4.0/STA2 Classic/FOtest/test prefixes contain 93 of that list, all `BIKi`: eight base bitmap movies and 85 Classic animation/bitmap movies. The 345 `BIKf` occurrences are in other installed mods. Archive-contained or differently named movies remain unknown.

The primary ten functions cover open/close, decode/advance, wait, copy-to-buffer, seek, volume, sound-system selection and DirectSound setup. Older variants add buffer lock/unlock/blit, DirectDraw surface type and changed rectangles. Preserve input modes, flags, pitch/layout/orientation/alpha, errors, EOF/loops, frame counters, seek state and public fields that native callers read directly. `modules/A2FOShellDisplay/native_intro.cpp` also checks the old Bink provider identity and uses Bink-handle dimensions; those assumptions must change when ownership moves.

An existing [pure-Rust Bink decoder](https://docs.rs/infinitier_bik_decoder/latest/infinitier_bik_decoder/) advertises BIKf/BIKi, alpha, DCT/RDFT audio and frame-oriented output. It is a candidate for evaluation, not an accepted dependency. The [examined workspace manifest](https://raw.githubusercontent.com/ufoplay/infinitier/main/Cargo.toml) declares GPL-3.0-or-later; reconcile source/provenance and distribution licensing before selection. The live documentation and examined main-branch manifest show different package versions, so pin and review one exact revision instead of mixing their claims. No candidate was downloaded into the project or built here. Its advertised support does not establish actual Armada asset parity, Windows x86 support or Bink API compatibility.

## FMOD: the actual game-audio workload

Recovered FleetOpsHook code supplies useful behavioral evidence:

- `rebuild/src/audio/audio_manager.cpp`: version/driver checks, sound/no-sound initialization, output/channel/buffer configuration, listener updates, pause and shutdown.
- `audio_sound_2d.cpp`, `audio_sound.cpp`: cached/streamed sounds, priorities, completion/pause, per-sound volume and fades.
- `audio_sound_3d_entity.cpp`, `audio_sound_3d_ambient.cpp`: entity/ambient sources, position/velocity, distance limits, sound groups and audibility.
- `jukebox.cpp`: unpausable streamed music, race-selected tracks, track transitions and fades.

These sources are under `reverse_engineering/FleetOpsHook/rebuild/src/audio/`. They are supporting recovery evidence; this assessment does not claim that the rebuilt implementation is identical to the pinned installed DLL. Exact installed call sites, flags, enums and return/error-dependent branches must be frozen before compatibility implementation.

The three selected loose `sounds` trees contain **4,979 file occurrences**:

| Sound tree | PCM WAV | Ogg with Vorbis identification marker | MP3-signature files |
| --- | ---: | ---: | ---: |
| Base Data | 494 | 1,090 | 128 |
| Fleet Ops 4.0 | 195 | 1,232 | 6 |
| STA2 Classic Mod | 1,834 | 0 | 0 |
| Total | 2,523 | 2,322 | 134 |

All WAV format tags were PCM (1), with no header-parse errors; observed samples are 8/16-bit, mono/stereo, at 11.025–48 kHz. Ogg identification used at most the first 512 bytes; MP3 checks found frame sync or ID3. No loose `.fsb` files were present in these trees. This is header evidence, not decode success, unique content, runtime reachability or proof that archives/other mods need no additional format support.

[Symphonia](https://github.com/pdeljanov/Symphonia) is one pure-Rust decoder candidate for PCM WAV, MP3 and Ogg/Vorbis. Existing OxideAV infrastructure is another reuse opportunity where it supports the required formats. Neither supplies the full FMOD-compatible mixer, voices, priorities, 3D model, device behavior or errors. Evaluate exact dependencies and 32-bit Windows builds before choosing; bindings to FMOD, OpenAL Soft, SDL_mixer or other native audio engines would retain native implementation dependencies.

## The shared DirectSound dependency

Recovered `audio_manager.cpp:205–230` gets FMOD's output type and, for DirectSound, its output handle. It then passes that handle through `BinkSetSoundSystem(BinkOpenDirectSound, handle)`. Shutdown reconfigures Bink audio as well.

The pinned installed FleetOpsHook also confirms this specific bridge: at VA `0x5a9e123b` it calls the imported GetOutput thunk, compares output to 6, conditionally calls GetOutputHandle at `0x5a9e1253`, then passes the handle and BinkOpenDirectSound thunk to BinkSetSoundSystem at `0x5a9e1268`. The [bounded current-binary disassembly](../build/bink-fmod-retirement-20261030/current-output-handle-call.txt) includes the import thunks. This verifies this static call path, not the entire recovered audio implementation or its runtime behavior.

This is a concrete reason to coordinate the replacements. Returning a made-up output pointer or unconditional success would not preserve the contract. If FMOD is replaced while native Bink remains, its Rust replacement must either provide the real output/device interface the Bink path expects or migrate that path deliberately. If Rust Bink is accepted first, both Rust subsystems can subsequently share PCM mixing/output without native Bink requiring FMOD's DirectSound handle. Windows/Wine audio device APIs may remain platform services; no extra native game-audio engine is required by the intended architecture.

## Implementation milestones and exit gates

1. **Freeze contracts and baselines.** Inventory supported EXE/mod/plugin callers, dynamic loads and ordinals, public Bink fields, FMOD flags/results/handles, archive readers and settings. Record native frame/audio/behavior baselines and exact hashes. Preserve current working DLLs and existing trial results.
2. **Prove BIK decoding headlessly.** Select and pin a decoder, compile for Windows x86, compare real BIKf/BIKi video/audio to independent baselines, and test alpha, audio tracks, seek/reset, EOF, truncation, limits and performance. This milestone adds no live hooks or installation changes.
3. **Implement the Rust Bink compatibility layer.** Put the required frame/audio/handle API in the main DLL, preserve observable layout/flags/error behavior, and update source provider checks. Reuse existing Rust pixel/audio infrastructure where the contracts match. Test the actual native caller ABI, not only Rust unit tests.
4. **Retire the Bink load requirement in an isolated installation.** Checked, reversible import migration must cover Armada, both FleetOpsHook Bink descriptors and supported alternate executables. Preserve IAT/caller contracts and existing A2FO exports/ordinals. Avoid A2FO↔FleetOpsHook static dependency cycles; initialize compatibility services at a verified point. Post-load IAT hooks alone cannot fix a missing required startup DLL. Accept menus/textures/movies/skip/seek/audio/transitions before removing Bink from a release.
5. **Implement and accept the Rust game-audio backend.** Prove PCM/MP3/Vorbis decoding/streaming; implement a bounded mixer, voices/handles, resampling, 3D/listener behavior, priorities/groups, pause/fades and device/no-sound behavior. Implement the reachable FMOD Ex compatibility API or move every caller onto a deliberate Rust audio API. Preserve archive/file routing. Use PCM captures and behavior tests for channel stealing, invalid/stale handles, volume/fades, listener movement, repeated playback and shutdown.
6. **Retire FMOD and accept the combined product.** Migrate FleetOpsHook's load-time imports/dynamic assumptions using checked copies. Test with both legacy files absent from all relevant search paths and no renamed/embedded native substitutes. Fold reusable media/audio code into the intended retained DLLs, then verify music, UI/voice/weapon/ambient audio, positional movement, movie/game transitions, save/load, multiplayer and repeated startup/shutdown. Multiplayer remains pending until a second matched-build peer is available. Deploy only through a separately authorized, backed-up action after these gates pass.

The recommended next implementation slice is **a headless BIK decoder proof against the actual movies**, alongside the remaining FMOD contract census. It does not require removing either working DLL first. This document records a viable route and the current gaps; it does not mark either replacement implemented or tested in-game.
