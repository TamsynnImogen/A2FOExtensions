# Stage 4: embedded Lua host

> Historical design note. The current Lua surface is documented in
> [`lua-api.md`](lua-api.md), and current ownership in
> [`architecture.md`](architecture.md).

Lua 5.4.8 is compiled directly into `A2FOExtensions.dll`; no separate Lua DLL
is required. During deferred initialization the host selects `scripts\*.lua`
from the same extension roots as native modules. Matching basenames use the
Data -> parent -> active-mod overlay order, and selected scripts execute in
deterministic filename order.

The initial script surface contains:

```lua
a2fo.lua_version
a2fo.extension_roots
a2fo.log(message)
a2fo.on_classlabel(callback)
a2fo.on_evolver_cocoon(callback)
odf:get_string(command, optional_default)
```

The host deliberately omits Lua's file I/O, operating-system, package-loading,
and debug libraries. `dofile` and `loadfile` are removed, bytecode is rejected,
individual files are capped at 1 MiB, the Lua state is capped at 32 MiB, and an
instruction budget prevents startup scripts and runtime callbacks from looping
forever. A failed startup script is logged with a traceback and does not
prevent later scripts or Fleet Operations from continuing. A failed runtime
callback is disabled and native Fleet Operations behaviour remains available.

Callbacks are registered during deferred startup and run synchronously from
the checked ParameterDB/Evolver hooks during ODF class loading. The later
object-destruction API also receives a copied ODF snapshot and transform-backed
native replacement action. Lua receives no
raw pointer. Instead, a generation-checked ODF view permits bounded string
reads only during that invocation. The core serializes Lua-state access and
keeps all object lifetime, SOD loading, caching, and fallback operations native.
Higher-precedence mods replace a policy by overriding the same script basename.

The callback surface was proven with wingman and cocoon, but those one-line
policies did not justify production Lua scripts. Both built-in behaviours now
live in `A2FOFeaturePack.dll`; the production `scripts` folder is empty. The Lua
host remains for mod-specific logic that combines ODF reads, conditions, and
semantic actions. `scripts\examples\ModSpecificRules.lua` demonstrates new
per-ODF compatibility and computed-cocoon commands without loading by default.
