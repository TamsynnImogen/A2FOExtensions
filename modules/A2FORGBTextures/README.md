# A2FORGBTextures native module

`A2FORGBTextures.dll` restores Armada 1/Armada 2 assets kept in the optional
legacy texture subfolders:

```text
Textures/RGB/
Textures/Index8/
Textures/Compressed/
```

The DLL name is retained for compatibility with existing installs even though
the module now covers all three folders.

Fleet Operations normally tries `Textures/<name>.dds` first. It also rewrites
Armada's original `Textures\RGB\` literal to `Textures\`, so the retained TGA
fallback asks for `Textures\<name>.tga`. That fallback opens the file directly
through Armada's MSVCRT `fopen` import and does not use the Fleet Operations
virtual filesystem. Merely registering the legacy directories there therefore
cannot restore every texture.

When at least one extension root contains any supported folder, this module
builds case-insensitive maps for root textures and for each exact legacy folder.
Explicit `Textures\RGB\...`, `Textures\Index8\...`, and
`Textures\Compressed\...` requests are resolved through their matching maps.
If Fleet Operations completes a root-level `Textures\<name>.tga` request, the
highest-precedence extension root containing that name wins first. Inside that
one root the tie-break order is a real loose or packaged root texture, RGB,
Index8, then Compressed. Consequently an active child mod's Index8 or
Compressed file correctly overrides a parent's RGB version.

Index8 is not interchangeable with RGB: Armada 1's Index8 files are genuine
8-bit colour-mapped TGAs, while the retained Fleet Operations route enters
Armada's true-colour pixel loader. Every flattened TGA candidate is therefore
validated regardless of its physical folder. Colour-mapped, 16-bit, grayscale,
and RLE types 9, 10, and 11 are expanded into a bounded uncompressed 24/32-bit
temporary TGA. This includes RLE-compressed TGAs placed directly in `Textures`
or `Textures/RGB`, as well as `Textures/Compressed`; explicit
`Textures/RGB/...` requests receive the same preparation. Already-safe
uncompressed 24/32-bit files are used directly. Unsupported or malformed legacy
TGAs fail closed rather than being fed to the wrong decoder.

Armada 1 can also store manually authored mip companions as `name_1.tga`,
`name_2.tga`, and so on. Fleet Operations may request one of those flattened
names while its native texture object still retains the base image dimensions;
passing the smaller file to that row decoder causes a pixel overread. For the
flattened root route only, the module recognizes a complete, same-format chain
whose levels are progressively smaller with the same aspect ratio and serves
its base image. The levels do not need to use exact power-of-two dimensions. A
folder-qualified request still receives the named mip file, and same-sized
animation frames ending in `_1` are not classified as mip levels.

Extension-root precedence is applied to the base and companion independently.
If a child mod replaces the base but inherits a validated mip chain whose
normalized source is too small for the child's expected mip dimensions, the
module downsamples the winning child base into a temporary 24/32-bit TGA of the
required size. This avoids native row-buffer overreads without modifying the
mod, while compatible inherited chains continue to use their existing files.

The root-TGA route hooks Armada's `ST3D_FileStream_FileExists` and
`ST3D_BinaryFileStream::OpenRead` boundaries, leaving Fleet Operations'
callbacks, texture objects, and both loader implementations untouched. DDS
requests are never supplied TGA bytes. Unrelated filenames and all writes pass
to the retained native functions unchanged.

Extension roots use the standard A2FO order: shared Data first, then parent
mods, then the active mod. A same-named file in a later root wins even when its
legacy folder differs from the parent candidate. The maps remain separate so
precedence and format preparation are deterministic; folder tie-breaking
occurs only inside each root after the native request has completed. Directory
and filename matching is case-insensitive.

Prepared files live in a process-specific system temporary directory and are
cached for the rest of the run. Normal module shutdown deletes them. A crash
may leave that harmless temporary directory for later housekeeping.

If a texture still remains unavailable, separately validated guards skip an
XRGB888 blend whose source pixel pointer is null and skip a scan-grid sprite
copy when either locked texture cannot be read. These prevent a failed minimap
texture from becoming an access violation while preserving valid operations.

The bridge is presence-based. Removing the DLL, or omitting all three legacy
texture directories, leaves native Fleet Operations behaviour unchanged. The
supported Armada identity, exact original-or-Fleet-Ops pathname state, and
original `msvcrt!fopen` pointer are hard gates before the bridge is installed.
The stream route additionally requires exact `ST3D_FileStream_FileExists` and
`ST3D_BinaryFileStream::OpenRead` entry signatures. The null guards require
exact `ST3D_Texture::LockSurface`, `BlendPixels_XRGB888`, and
`RadarComponent::UpdateScanGridSprite` signatures. `A2FO_ModuleShutdown`
restores the fopen pointer if it still owns it. Inline hooks, like the project's
other engine hooks, are process-lifetime changes.


## Checkpoint — 2026-10-28: six render/UI families complete behind native adapters

This batch completes the remaining portable controllers and source/headless
validation for A2FOCraftIdentity (module and object editor),
A2FODirectionalShields, A2FOFeaturePack (recursive ODF, buildyard pseudo-technology,
queue enhancements and upgrade pods), A2FOShellDisplay (Bink and canvas),
A2FORGBTextures and A2FOTextureVariants: eleven inventory units in total.

Rust now controls CraftIdentity first-match fields/atlas lookup, full-byte string
trim, display-mode suffix admission, drawn predicates and visible hover setup;
RGB DDS/TGA publication traversal and precedence; FeaturePack ODF hash-chain
membership; directional effect-tree traversal; canvas ancestry, point-map pass
order and reverse lease selection; and texture scorch admission, publication
comparison and hardpoint selection. Native failure/reset paths and lazy reads
are preserved. Canvas ancestry releases each borrowed record before the next
lookup. Full u32 scorch bands and raw effect pointers remain data, separate
from callback error metadata. Display-mode suffix whitespace is classified
before the parsed-range decision, as in the original CRT path.

This is portable-controller completion, not a pure-Rust binary. Native code
retains Win32/COM/GDI/CRT and engine calls, hooks/signatures/addresses, checked
fields, native strings/maps/iterators/locks, exception scopes, exact GNU x87
projection/geometry/rounding primitives, and FeaturePack's existing STL sort.
The sort retains casefold-equivalent filename ordering and comparator/allocation
exception behavior. Object-editor and Bink controllers were already in Rust;
this batch supplies their current build and headless validation. Existing native
kernel files and ABI boundaries are pinned rather than rewritten speculatively.

Validation passed: 249 Rust tests; formatting, host/i686 Windows Clippy, both
release archives, integrated DLL and all six standalone DLLs. Twenty native
suites produced 267 matching before/current trace pairs (534 executions,
6,832 stdout lines), with inherited mutation and allocation-failure cases.
A direct production Bink fixture compares 276 numeric rows across 12 x87 modes;
canvas fixtures cover ancestry depth bounds, reverse/scoped leases and point
mapping. Seven additional directional tree cases cover signed branches, missing
and unreadable nodes, the head sentinel and cycle bound in both module forms.
A separate production scorch comparison passes 228 rows across all 12 x87
precision/rounding modes, including guards, boundary bands, modulo hardpoint
selection, publication state, raw sticky/C1 status, CW and stack balance.
Built-in batch9 admission also passes. Runtime fixture assets are isolated per
execution and retained; both sides see the same logical working-directory path.

The source boundary check preserves 56 complete native files and all text
outside 16 approved controller bodies plus new adapter helpers in six files.
All 417 source-referenced Rust implementations link (the verifier checks 420
including three defaults). Core 197 exports/ordinals and each standalone export
set remain identical, with no new system imports. The build pins 791 product
inputs including owned .inl files. Six objects depending on changed sources
were rebuilt; remaining cache entries and 7,851 third-party source pins were
verified against the passed Squadrons predecessor. Original/current fixtures
use independently pinned pre-batch and candidate Rust archives. Established
fixture-only GNU ABI/custom allocator diagnostic flags remain recorded in the
compile commands. Failed link/probe attempts are preserved.

Evidence: build/rust-migration-stage10/render-ui-batch/checkpoint/validation.json.
The eleven units are completed_pending_census. Official totals remain 116/151
with 35 uncredited units: seventeen completed units await the shared strict
census refresh, and eighteen units remain unfinished. Frozen membership,
baseline provenance and strict archive/common-DLL bindings are unchanged.
Canonical artifacts and earlier checkpoints are preserved. No deployment or
real game launch occurred; GPU and in-game acceptance remain pending.
