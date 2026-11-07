# Plan: BC1–BC7 textures and specular maps in the Refit renderer

Claude, 2026-11-07, at the owner's request ("are specular maps and BC5/BC7 being incorporated?").
This is a plan plus verified facts.

**Status:** BC work done and fixture- and live-qualified on private trials; see
[refit-bc-textures.md](refit-bc-textures.md). Specular maps are in progress.

## Current state (verified)

- **Specular maps** exist only on the **legacy D3D8/DXVK path**:
  - `modules/A2FONebulaRenderer` parses `#define A2FO_SPECULAR_SUFFIX "_specular"` from the mod's
    `ART_CFG.h` (`art_texture_suffix_config.*`).
  - It resolves `<diffuse><suffix>` loose files per CraftClass material and registers them with Core
    (`register_nebula_specular_materials`, `core/nebula_renderer.*`).
  - It draws a quarter-strength additive "broad gloss" overlay on bumped materials, which is not
    view-dependent.
  - **FO4 Nexus and Fleet Ops 4.0 do not opt in and ship no maps.** Real assets exist in the SFC mod
    (`fconnieEnterprise_Hull1/2_Cannon_specular.dds`, 4096² DXT1) and the test mod
    (`Fyard_specular.dds`, 256² DXT5): 12 files in total.
- **BC5/BC7** exist only in the **isolated D3D11 backend**: `renderer/textures/dds.*` (DX10 headers,
  BC1–7, sRGB, mips) and `renderer/d3d11/compressed_texture.*`. See
  `docs/renderer/bc-textures-20261021.md`.
- **Refit GPU has no block-compressed formats** (`gpu/src/types.rs` `Format`). Every DDS, even
  DXT1/5, is CPU-decoded by `a2fmt::dds::decode` to BGRA8. A 4096² DXT1 becomes 64 MB instead of 8 MB.
- BC7 and BC5/ATI2 DDS cannot load at all. Loose mod DDS: DXT5 1377, DXT1 1016, **BC7 227, ATI2 180**
  (all in `FO4 BC Texture Test`), DXT3 27, uncompressed 173.
- Vulkan BC needs the `textureCompressionBC` device feature. The **Display** DLL creates the device
  with only `samplerAnisotropy` (`display/src/vk/mod.rs:371`).

## BC work (first)

1. Display: enable `texture_compression_bc` when the physical device reports it (Lavapipe and
   desktop GPUs do). Rebuild and qualify the Display; Root `display/` currently equals `d323e9c1`.
2. GPU crate:
   - `Format::{Bc1, Bc1Srgb, Bc2, Bc3, Bc4, Bc5, Bc7, Bc7Srgb}` with block size.
   - Block-aware `regions()` in `vulkan/tex.rs` and `dx12/tex.rs`: row bytes are
     `ceil(w/4)*block`, rows are `ceil(h/4)`.
   - DXGI 71/72/74/77/80/83/98/99 and VK 133/134/135/137/139/141/145/146 mappings, plus a
     format-supported query.
3. `a2fmt::dds`: parse DX10 headers, and add an `encoded()` that returns format, mips and blocks
   without decoding. The existing `decode()` stays the fallback for DXT on devices without BC.
4. Renderer `upload_encoded_texture`: use compressed upload when supported, CPU decode for
   DXT1/3/5 otherwise, and `CONTENT` for BC5/BC7 when unsupported (fail-safe: that part stays
   native).
5. GPU fixture: BC1/BC3/BC5/BC7 encoded upload and sample controls on Vulkan and DX12. Report VRAM
   bytes before and after.

## Specular maps (second, on top of per-pixel Phong)

- Shader: a second texture slot holds the specular mask (RGB intensity, diffuse UVs) and multiplies
  the per-pixel Blinn-Phong highlight. White means unmasked (the current behaviour).
- ABI3: `A2FORefitPartMaterial3` gains a specular-map texture name and revision, uploaded through
  `upload_encoded_texture`. No map means the white fallback.
- Core: look up the object's material in the existing `A2FONebulaRenderer` registry
  (`register_nebula_specular_materials`), so the suffix convention, root precedence and
  case-insensitive lookup are reused exactly. Read the loose file and upload it.
  - Specular colour and power come from the Phong material when colours are on.
  - Otherwise, decide whether maps alone should enable a default highlight.
- Validate on the test mod (`Fyard_specular`) and SFC assets; visual acceptance.
