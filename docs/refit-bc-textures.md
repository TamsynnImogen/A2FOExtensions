# Refit renderer: BC1–BC7 compressed textures

Implemented 2026-11-07 by Claude at the owner's request ("are BC5/BC7 being incorporated?").
Plan and starting state: [refit-bc-and-specular-maps-plan.md](refit-bc-and-specular-maps-plan.md).
Private trials and fixture only. Not staged, and not seen on a physical GPU or in gameplay.

## What changed

- **Display** (`display/src/vk/mod.rs`, `dx12/mod.rs`, contract `gpu/src/display.rs`):
  - New feature bit `FEATURE_BC = 8`.
  - Vulkan enables `textureCompressionBC` when the physical device has it (Lavapipe and desktop
    GPUs do) and then sets the bit.
  - Direct3D 12 always sets it (BC1–7 are mandatory there).
  - Build `build/refit/display-bc/` = `740b0122…`. It equals Root `d323e9c1` plus this change.
- **GPU crate** (`gpu/src/types.rs`, `vulkan/`, `dx12/`):
  - `Format::{Bc1, Bc1Srgb, Bc2, Bc3, Bc4, Bc5, Bc7, Bc7Srgb}`.
  - Block-aware `row_layout`, which upload regions and staging on both backends use. Mips below 4×4
    take one block.
  - DXGI 71/72/74/77/80/83/98/99 and the matching `VK_FORMAT_BC*_BLOCK` formats.
  - BC4 is swizzled to (R,R,R,1) and BC5 to (R,G,0,1), as the engine reads them.
  - Creation guard: 2D only, single sample, base edges a multiple of 4, sampled only.
  - `Gpu::has_bc()`.
- **`a2fmt::dds::encoded()`**:
  - Parses legacy FOURCCs (DXT1/3/5, ATI1/BC4U, ATI2/BC5U) and DX10 headers (the eight formats
    above; 2D, no cube, array size 1).
  - Validates the whole supplied mip chain and returns the blocks undecoded.
  - `decode()` is unchanged and stays the fallback.
- **Renderer `upload_encoded_texture`** (`render3d/core/src/a2fo_host.rs`):
  - DDS on a BC-capable device with base edges a multiple of 4 is uploaded as blocks with every
    supplied level (`upload_blocks`). The texture budget counts compressed bytes.
  - Otherwise the CPU decode path runs as before.
  - On a device without BC, BC4/BC5/BC7 fail with `CONTENT`, so that mesh stays native (fail-safe).
  - The ABI is unchanged. Core needs no change: `renderer/refit/resources.cpp` already passes the
    captured DDS file bytes through untouched.

## Evidence

| Check | Result |
|---|---|
| GPU fixture (`tests/refit_parts_runtime_test.cpp`; `build/refit-bc-checks/20261107T123344Z/`, Renderer `de7bf1fb`, Display `740b0122`) | **PASS on Vulkan and DX12**: every earlier check, plus solid-block BC1, BC1 red control, BC2, BC3, BC4 (white), BC5 (FOURCC and DX10), BC7, BC7 sRGB, BC1 with a full chain down to 1×1, BC7 with 2 levels, and 2×2 DXT1 (non-multiple-of-4, CPU path). Six malformed inputs (truncated, cube, volume, array, BC6H, too many levels) are refused with `CONTENT` and publish nothing. Revision replace works and stale revisions are refused |
| Negative control: same Renderer on Display `d323e9c1` (no `FEATURE_BC`) | BC1–3 still pass through CPU decode. BC4 is refused (`CONTENT`), which proves the passing run took the block path |
| Same fixture after the branch-free `blinn()` change (Renderer `cc30dc81`, `build/refit-bc-checks/20261107T131928Z/`) | PASS on both backends |
| Live, private Vulkan trial. Mediterranean given a child-mod `federation_mediterranean.dds` (magenta-tinted DXT5, 11 mips) | Display-bc + Renderer-bc: every candidate replaced (`single=4670`), ship drawn tinted. Display `d323` (CPU decode): the same. Ship-crop channel means 15.7/5.3/16.3 vs 15.5/5.1/16.0 |

Receipts: trial `revisions/bc-*` and `restore-bc-fixture-*`. The trial was restored afterwards (Codex Core
`85439152`, Display `3c317ae7`, Renderer `e9a78291`, config `5f69dffd`, probe `d040db6a`).

## Findings worth keeping

- **The block path samples the DDS's own mips, as native D3D8 does.** The CPU path uploads level 0
  only, so distant hulls alias there; the block path looks smoother and is closer to native.
- The consequence: **a DDS with bad mips now shows them.** The first live test file came from
  ImageMagick's DDS writer, which resizes with associated alpha. Its lower levels were near black
  (level 1 mean 15 vs level 0 109), and the replaced hull went black except its self-illumination.
  Rebuilding with colour and alpha resized apart (`-define dds:mipmaps=fromlist`) fixed it. Mod
  authors' DDS files with bad mips would also look wrong natively, so this is parity, not a regression.
- Live **BC7/ATI2 files cannot be tested on the native path yet.** The engine itself must create the
  texture and draw the checked mesh before Refit can replace it, and D3D8 has no BC7/BC5 formats. The
  fixture covers them. In-game use needs the engine-side texture route (the old D3D11 bridge work,
  `docs/renderer/fo4-bc-test-20261021.md`).
- `rom_norexan` (real 1536² DXT5, 11 mips) did not make a reliable live probe. Its two-group
  replacement appeared in 1 of 4 runs, on both BC and CPU Displays: the variance is in instance
  qualification, not textures.

## Not done

- Staging, physical-GPU and gameplay acceptance.
- CPU-path mips (generate them for TGA and decoded DDS), a separate quality item.
