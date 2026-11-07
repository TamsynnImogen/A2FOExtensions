# BC5 / BC7 compressed texture slice

BC5 and BC7 now work in the isolated D3D11 backend: DDS parsing, typed immutable
GPU resources, full supplied mip chains, sampling and BC5 normal reconstruction.
**The installed game still uses the legacy resource path. This slice does not
yet reduce Armada/FO VRAM or enable the full `Backend=d3d11` setting.**

## Resource path and compatibility boundary

`renderer/textures/dds.*` reads bounded little-endian DDS headers and preserves
encoded blocks. It supports legacy DXT1–5, ATI1/2, BC4U/S and BC5U/S identifiers,
and typed DX10 BC1/2/3/4/5/7 formats, including applicable sRGB variants. It
preserves alpha-mode metadata, accepts partial mip chains, calculates each block
row and mip extent rather than trusting DDS pitch hints, and rejects truncated
payloads, invalid mip counts and dimensions, arrays, cubes, volumes, typeless
formats, BC6 and uncompressed input. Unsupported input leaves the output intact.
This is deliberately a compressed 2D reader; existing uncompressed/indexed
texture paths remain separate.

`renderer/d3d11/compressed_texture.*` validates those descriptors and device format
support, then submits the encoded mip bytes directly to `CreateTexture2D` with
`D3D11_USAGE_IMMUTABLE`. The SRV retains the exact typed format and supplied mip
count. It neither expands the blocks to RGBA nor retains a second CPU copy;
the caller owns the parsed byte vector and can discard it after creation.
Reported byte counts are logical texture payloads, not driver allocation or
residency measurements. Failures identify operation, HRESULT, format and size.
BC7 requires feature level 11; unsupported hardware returns a clear failure.
`UiRenderer::upload_compressed_texture` makes this path available to the existing
modern draw backend without changing the legacy upload API.

`renderer/shaders/bc_normals.hpp` supplies explicit UNORM and SNORM two-channel
normal decoding. UNORM XY is remapped to [-1,1], SNORM XY is already signed;
Z uses the positive hemisphere and the result is normalized. There is no implicit
Y flip. Only normal-map consumers should call this helper. Ordinary RG data,
colour, emissive, alpha and packed masks must not be treated as normals.
No new lighting model or normal-map game material has been enabled.

RGBTextures currently redirects filenames; it does not own DDS decoding or
GPU resources. Existing supplemental material loading in `core/nebula_renderer.cpp`
uses `D3DXCreateTextureFromFileExA` from the D3D8-era helper. Its emissive path
also explicitly requests A8R8G8B8. A file alias or a BC7-to-RGBA fallback would
not establish compressed GPU residency. BC7 needs the modern resource owner and
sampling path; it cannot simply become a native D3D8/D3D9 texture. Existing
shader/material handles and original loaders must remain authoritative until
that ownership boundary is implemented. ATI2 naming alone is not proof that the
current legacy provider and loaders support BC5.

The subsequent [FO4 child/resource bridge](fo4-bc-test-20261021.md) adds immutable
D3D8 texture/surface interfaces and an encoded read-lock shadow. The parser also
accepts the unused depth field of one in undeclared 2D ATI2 files; declared volumes
and depth greater than one remain rejected. Game device routing remains pending.

## Memory findings

Both BC5 and BC7 use 16 bytes per 4×4 block (8 bits/pixel before small-mip
rounding). They offer about 75% less payload than RGBA8. BC3/DXT5 has the same
block size; BC1/DXT1 has half that size, so conversion is not automatically a
saving. BC5 preserves two channels and suits appropriate normal/data maps;
BC7 preserves RGB/alpha with lossy compression. See Microsoft's
[block-compression reference](https://learn.microsoft.com/en-us/windows/desktop/direct3d11/texture-block-compression-in-direct3d-11).

The read-only census tool, `tools/renderer_texture_census.py`, scanned 17,336 loose
DDS/TGA/PNG files beneath the installed base and mod texture roots, with zero
scan errors. It does not open archives, follow symlink files/directories, resolve
which mod assets are active, or deduplicate physical copies. Header dimensions
are not full image validation. There were no existing BC5/BC7 DDS headers in this
scan. The 102 canonical BGRA8 DDS files total 798,980,812 logical payload bytes;
BC7 at the same dimensions/mip counts would occupy 199,748,864 bytes. These are
cross-mod inventory estimates, **not measured live VRAM savings**. TGA palette,
compression, material semantics and actual runtime formats need separate review.
Filename tokens are only normal-map search hints.

The implementation and fixture block generators are original code. No Refit,
d3d8to11 or external codec source was copied or vendored. The already-installed
external Compressonator distribution has an AMD MIT-style permissive licence;
its binary and licence hashes are recorded with the experiment, and it is not
a runtime dependency or included in the candidate DLL.

One installed texture, `Textures/yamaguchi05_glow.dds`, was CPU-encoded to an
isolated BC7 copy with the already-installed Compressonator CLI 4.5.52, quality
0.8, producing ten mip levels. A later authored-level test found that the CLI
regenerated lower mips in that command; only its base-level pixel/quality results
are valid as source comparisons. The child conversion now preserves each supplied
level separately (see the linked child report). No installed texture was replaced.

| 512×512 texture, ten supplied mips | Logical payload | DDS file size |
| --- | ---: | ---: |
| Original BGRA8 | 1,398,100 B | 1,398,228 B |
| BC7 test copy | 349,552 B | 349,700 B |

The payload reduction is 1,048,548 bytes, approximately 75%. Small mip tails still
occupy complete blocks. Compression is lossy: CPU-decoded base-mip RMSE in
8-bit channel levels was R 1.775, G 1.458, B 1.606, A 1.142; maximum individual
errors were 40/26/28/54. Those metrics do not establish acceptable in-game alpha
or emissive appearance. This one example alone does not establish quality for a bulk conversion.

## Validation and reproduction

```sh
make -j2 renderer-bc-tests build/renderer_ui_native.exe build/A2FOExtensions.dll
python3 tools/run_renderer_validation.py \
  --prefix /home/tamsynn/.cache/a2fo-refit-native-prefix-20261018 \
  --output build/renderer-bc-textures-20261021/dds --fixture dds
python3 tools/run_renderer_validation.py \
  --prefix /home/tamsynn/.cache/a2fo-refit-native-prefix-20261018 \
  --output build/renderer-bc-textures-20261021/gpu-final --fixture bc \
  --texture-dds build/renderer-bc-textures-20261021/real-asset/yamaguchi05_glow_bc7.dds \
  --texture-reference-rgba build/renderer-bc-textures-20261021/real-asset/point_reference.rgba
```

The isolated real-asset files were produced with the existing external CLI:

```sh
/home/tamsynn/.local/bin/compressonatorcli -fd BC7 -EncodeWith CPU -Quality 0.8 -miplevels 10 \
  '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/Textures/yamaguchi05_glow.dds' \
  build/renderer-bc-textures-20261021/real-asset/yamaguchi05_glow_bc7.dds
/home/tamsynn/.local/bin/compressonatorcli -fd ARGB_8888 \
  build/renderer-bc-textures-20261021/real-asset/yamaguchi05_glow_bc7.dds \
  build/renderer-bc-textures-20261021/real-asset/yamaguchi05_glow_bc7_decoded.dds
python3 build/renderer-bc-textures-20261021/real-asset/make_reference.py
```

The existing dedicated Wine prefix and private headless compositor are required.
No game entrypoint, visible desktop window or input automation is used. Fixture
SKIP is distinct from PASS. Per-process receipts hash executables, DDS and
reference inputs. `make renderer-bc-tests` builds tests; it does not run them.

The CPU fixture covers legacy and DX10 formats, signed/sRGB/alpha metadata,
partial and tiny mips, odd dimensions, missing/unreliable pitch flags, truncated
headers/payloads, unsupported topology/formats, forged descriptors and atomic
failure. Strict `-Werror` compilation and the Wine console run pass. Independent
read-only review found no confirmed parser/uploader defect.

The final GPU run identified Intel UHD Graphics 630 (vendor 0x8086, device
0x3e9b) through the D3D11 device, under Wine. It is not a native Windows
or discrete-GPU result. The GPU fixture checks typed immutable texture and SRV descriptors, exact mip
count, no CPU-access flags, metadata and invalid-upload refusal. Original test
block generators supply BC5 UNORM/SNORM and BC7 UNORM/sRGB. All four mip levels
are sampled; BC5 is checked both raw and reconstructed, including signed values
outside the unit disk. Each image compares all 64 RGBA pixels at tolerance one.
There are 24 format/mip/mode comparisons plus a BC7 draw through the existing
transformed-UI backend. Linear cases match exactly; sRGB differs by at most one
8-bit channel level. This tests sampling, alpha and decoding, not lighting parity.
The synthetic BC7 blocks use mode 6; arbitrary BC7 encoding modes are decoded by
the D3D11 provider rather than a custom decoder.

The real BC7 texture is also uploaded with ten mips. An independent CPU
Compressonator decode supplies a fixed 8×8 point-sampled RGBA reference; the
GPU fixture compares all 256 readback bytes within one level; the final real-asset
comparison was exact (maximum delta zero). An intentionally zeroed reference
was also rejected with 144 channel mismatches, proving the comparison fails
when the supplied expected pixels are wrong. Its expected-failure receipt is
under `reference-negative-final/`; an earlier sandbox compositor failure is
retained separately and is not counted as this negative-test result. Optional external
DDS runs without a reference explicitly report checksum-only mode and do not
claim pixel equality. The reference generator, hashes, CPU quality metrics and
encode/decode logs are retained in `build/renderer-bc-textures-20261021/real-asset/`.
This sparse comparison checks GPU decoding of that file; full-image quality and
in-game appearance remain unaccepted.

The existing 19 paired D3D8/D3D11 UI scenes pass unchanged after integration.
Core linking passes with pre-existing warnings elsewhere and still imports no
mandatory D3D11, DXGI or shader compiler DLL. The candidate is staged separately;
no installed DLL, INI, wrapper or asset was changed by this slice.

## Next integration gates

1. Give the D3D8-facing compatibility device a modern texture/resource owner with
   correct COM identity, levels/surfaces, lock/update, state-block references and
   Reset/lost-device behavior. Keep mutable legacy resources distinct from
   immutable external compressed assets; do not recreate textures per frame.
2. Bridge resolved mod-aware file/material loads to that owner. Preserve legacy
   fallback and establish explicit colour-space, alpha and normal-channel intent.
   A filename or a new header alone cannot determine those semantics.
3. Replay an actual material draw with the new owner, then route complete frames
   through D3D11 before claiming compressed textures work in Armada/FO. Preserve
   emissive, indexed formats, stock materials, Fleet Ops UI and the minimap fix.
4. Measure deduplicated live texture bytes/CPU shadows, upload timing and GPU
   residency where supported, against the same scene and legacy baseline.
   Convert a backed-up, opt-in asset set only after visual and alpha comparison.

No additional manual game test is needed for this isolated component. Full
Armada/FO rendering, device-loss handling and native Windows acceptance remain
separate migration gates. The compatibility matrix records helper progress,
not an implemented `IDirect3DDevice8::CreateTexture` replacement.
