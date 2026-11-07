# Third-party software

## Armada II Refit camera and view distance reference

`A2FOCameraControls` and `A2FOViewDistance` adapt the optional pan-speed and
render-distance features researched in Cedric Haase's Armada II Refit.
The pinned source, scope and retained MIT notice are in
[`armada2-refit/SOURCE.md`](armada2-refit/SOURCE.md) and
[`armada2-refit/LICENSE.txt`](armada2-refit/LICENSE.txt).
Our modules use A2FO's host admission and checked patch API; Refit's ASIs,
renderer, input automation and networking are not imported.

## armadaNebulaPatch DX8 shaders and renderer design

`A2FONebulaRenderer` is derived from the DX8 renderer in armadaNebulaPatch and
vendors its four shader programs with attribution comments. Its custom startup
proxy, hook toolkit, MinHook binary/source, and experimental DX9 path are not
included.

- Source: <https://github.com/FNSOIDATHQ/armadaNebulaPatch>
- Shader source submodule: <https://github.com/FNSOIDATHQ/shaderPlusArmada>
- Copyright: Copyright (c) 2024 dev gao
- Licence: [`armada-nebula-patch/LICENSE.txt`](armada-nebula-patch/LICENSE.txt)

Upstream was reviewed again on 2026-08-11 at main commit `d01c838` and
`shaderPlusArmada` commit `f867372`. That update exposes additional experimental
DX9 renderer work but does not change the DX8 source used by this project. The
DX9 prototype remains reference-only; its required FX shader assets are not
present upstream and its unchecked hooks/debug paths are not vendored here.
Detailed findings and the deferred integration checklist are recorded in the
Nebula Patch Renderer section of [`../TODO.md`](../TODO.md).

The four upstream repositories were inspected again on 2026-11-02. The pinned
revisions are unchanged; the current lighting/shadow/D3D11 assessment and source
links are in [`../docs/fnsoidathq-renderer-review-2026-11-02.md`](../docs/fnsoidathq-renderer-review-2026-11-02.md).

## Megadroid's Mission Mod (MMM)

The original MMM runtime/assets are retained under `third_party/mmm`.  The
source used by `A2FOMissions.dll` is pinned to upstream commit
`72d6858b22759613e6ce161a834bbd9c99ad7a5f` and is stored under
`third_party/mmm/source` after running `make vendor-mmm-source`.

A2FOExtensions has recorded permission from the project owner to use the
`MMM_loader.dll` source; see `docs/source-permissions.md`.  The integration
compiles MMM directly into `modules/A2FOMissions.dll`; the generated
`missions/mmm_loader.dll` is only a compatibility forwarding shim.


## Rust media codecs

`rust-media/` contains pinned MIT-licensed OxideAV sources used by the production
Rust media sidecar. See `rust-media/provenance.json` for revisions and
`rust-media/PATCHES.md` for local changes. The former `ffmpeg-win32` SDK is
retained as historical/reference material and is not a production dependency.
