# Native module SDK v4 revision 5

A module must be a 32-bit Windows DLL exporting:

```cpp
extern "C" __declspec(dllexport)
bool __cdecl A2FO_ModuleInit(const A2FO_ModuleApi* api);
```

It may optionally export:

```cpp
extern "C" __declspec(dllexport)
void __cdecl A2FO_ModuleShutdown();
```

Modules must validate `api_version` and only require the struct prefix they use.
Original v4 modules should require `A2FO_MODULE_API_V4_BASE_SIZE`, not the size
of the newest header. Modules using an appended member must first test
`A2FO_MODULE_API_HAS(api, member)`, then its capability bit when applicable.
The `A2FO_ModuleApi` itself has process lifetime.

The core owns hook-site coordination. API v1 exposes low-level checked patch
helpers for the first reference module; later API revisions should add semantic
hook registration to reduce collisions between independently developed modules.

API v2 adds the first semantic hook boundary:

```cpp
api->register_fofs_item_lookup_handler(
    "MyModule", &my_lookup_handler, my_context);
```

Only one module may own this dispatcher in API v2. The handler runs before
Fleet Operations' native hash lookup and returns `true` only when it supplies a
replacement result. Returning `false` preserves native lookup. Modules must not
throw exceptions across the C ABI or retain pointers whose documented lifetime
has ended.

API v3 exposes the deterministic extension-root list through
`extension_root_count()` and `extension_root(index)`. Roots are ordered from
lowest to highest precedence: the shared Data folder, parent mods, then the
selected mod. Native modules with the same basename follow that overlay order.

API v4 adds startup-only semantic registration for classlabel aliases and the
Evolver cocoon ODF command. The core retains the checked engine hooks while a
native module supplies optional declarative policy. Duplicate ownership is
rejected so conflicts fail visibly rather than depending on load timing. Lua
uses its separate callback API described in `../docs/lua-api.md`; it does not call
these native ABI functions.

Revision 1 keeps `api_version == 4` for binary compatibility and appends
`api_revision`, `capabilities`, and the destroyed-object registration function.
For example:

```cpp
if (!api || api->api_version != A2FO_MODULE_API_VERSION ||
    api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE) {
    return false;
}

if (A2FO_MODULE_API_HAS(api, register_object_destroyed_handler) &&
    (api->capabilities & A2FO_CAP_OBJECT_DESTROYED_DISPATCH) != 0) {
    const char* fields[] = {"replacementOdf", "replacementChance"};
    if (!api->register_object_destroyed_handler(
            "MyModule", fields, 2, &on_destroyed, nullptr)) {
        return false;
    }
}
```

The core copies requested field names during initialization. Event strings,
field arrays, and the event object expire when the callback returns. The
handler must initialize `replacement.struct_size`; if it claims the event, the
core immediately validates and copies `replacement.odf`. Handlers run in
deterministic module/registration order, before Lua handlers, and the first
valid claim wins.

All registrations made by one module initializer form a transaction. Returning
`false` or failing initialization rolls them all back before the DLL unloads.
Registration functions are startup-only. Callbacks and module exports must not
throw across the C ABI; catch exceptions inside the module and return a safe
failure value.

Revision 2 appends the dynamic upgrade-pod tier policy query. Revision 3
appends `get_original_classlabel(parameter_db, output, output_size)` and the
`A2FO_CAP_ORIGINAL_CLASSLABEL` capability. The core records the normalized ODF
source before applying a classlabel alias, allowing an aliased feature to keep
its public identity while Armada constructs the selected native base class.
The caller supplies the output buffer; no core-owned string escapes the call.

Revision 4 appends
`associate_evolver_cocoon_class(class_object, parameter_db)` and
`A2FO_CAP_COCOON_CLASS_ASSOCIATION`. It applies the registered Evolver cocoon
policy to an aliased/native class object without requiring that object to pass
through `EvolverClass::BuildClass`. This is intended for a checked adapter that
provides its own safe construction-effect storage; it does not make foreign
Evolver subclass methods safe on an arbitrary class.

Revision 5 appends `register_info_ini_defaults_handler` and
`A2FO_CAP_INFO_INI_DEFAULTS`. The core installs the timing-critical Fleet Ops
settings hooks before module loading, waits for deferred registration at the
first settings call, and copies the handler's results. The module owns parsing
and path policy; returning an empty settings path and clearing the speed flag
preserves Fleet Ops' native behavior.
