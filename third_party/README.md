# Third-party software

## Lua 5.4.8

The unmodified Lua 5.4.8 source distribution is vendored in `lua-5.4.8` and
compiled directly into `A2FOExtensions.dll`.

- Source: <https://www.lua.org/ftp/lua-5.4.8.tar.gz>
- Published SHA-256:
  `4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae`
- License and attribution: `lua-5.4.8/doc/readme.html`

The embedded host opens only the base, table, string, math, and UTF-8
libraries. File, process, package-loading, and debug libraries are not exposed
to mod scripts.

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
