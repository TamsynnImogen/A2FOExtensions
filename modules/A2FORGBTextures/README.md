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
