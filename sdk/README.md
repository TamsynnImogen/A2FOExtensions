# Native module SDK v4 revision 17

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
selected mod. This root list remains available for asset and policy lookup,
but native DLL discovery is deliberately restricted to `Data/modules`.
Per-mod `info.ini` `[modules]` entries select those global DLLs; mod-local
`modules` directories are ignored and cannot override executable code.

API v4 adds startup-only semantic registration for classlabel aliases and the
Evolver cocoon ODF command. The core retains the checked engine hooks while a
native module supplies optional declarative policy. Duplicate ownership is
rejected so conflicts fail visibly rather than depending on load timing.

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
deterministic module/registration order, and the first valid claim wins.

All registrations made by one module initializer form a transaction. Returning
`false` or failing initialization rolls them all back before the DLL unloads.
Registration functions are startup-only. Callbacks and module exports must not
throw across the C ABI; catch exceptions inside the module and return a safe
failure value.

Revision 2 retains a legacy upgrade-pod tier query. Current FeaturePack builds
read `upgradePodMaximumTier` from inherited `RTS_CFG.h` files directly.
Revision 3
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

Revision 6 appends the transactional ODF-overlay registry and
`A2FO_CAP_ODF_OVERLAY_DIRECTORIES`. Policy modules register safe relative
directories during startup; the filesystem-owner module may query the copied
registry later while building its lazy index. Higher overlay precedence wins
only between files belonging to the same mod root, so normal child/parent mod
priority remains authoritative.

Revision 7 appends `register_producer_event_handler`,
`dispatch_producer_event`, and `A2FO_CAP_PRODUCER_EVENTS`. FeaturePack remains
the sole owner of the checked Fleet Ops Producer hooks and dispatches three
semantic events: admission before a queue insertion, completion after the
native finish callback, and destruction before the native Producer destructor.
Every admission handler must return true; notification return values are
ignored. This lets optional modules impose class-specific production policy
without installing a second hook at the same executable address.

Revision 8 adds the claimable `A2FO_PRODUCER_EVENT_FINISHING` event without
changing the API structure. FeaturePack dispatches it immediately before its
native completion gateway. If any registered handler returns false, that
handler has consumed an in-place completion and FeaturePack suppresses the
native gateway, then still emits `FINISHED`. Existing handlers which return
true for unknown event kinds retain the revision-7 path unchanged. The event
is intended for legacy non-object build classes whose `Build()` cannot return
the `GameObject` required by A2/FO's ordinary completion path.

Revision 9 adds the claimable `A2FO_PRODUCER_EVENT_STARTING_EFFECT` event,
also without changing the API structure. The shared owner of Armada's
`Producer::mStartConstructionEffect` hook dispatches it after
`currentBuildClass` has been assigned but before Armada creates the cosmetic
Craft construction instance. Returning false suppresses only that effect; the
timed synchronized build continues. This prevents a non-Craft legacy policy
class from being passed to the Craft renderer while preserving ordinary ship,
station, research, and evolution effects.

Revision 10 appends `register_classlabel_odf_defaults` and
`A2FO_CAP_CLASSLABEL_ODF_DEFAULTS`. A module registers copied command/value
pairs against a source classlabel during startup. The core consults them from
its shared string, integer, float, and boolean ParameterDB hooks only after the
native lookup reports the command missing, so explicit and inherited ODF
values retain precedence.

Revision 11 appends `patch_bytes(target, replacement, expected, length)`. It
changes a fixed-size data, pointer, or constant range only when all original
bytes still match. Modules must check the appended member boundary before use:

```cpp
if (!A2FO_MODULE_API_HAS(api, patch_bytes) || !api->patch_bytes) {
    return false;
}
```

Executable branches should continue to use `install_inline_hook`,
`patch_jump`, or `patch_call`; `patch_bytes` exists for checked changes which
do not encode a branch.

Revision 12 appends `get_settings_directory(output, output_size)`. It copies
Fleet Operations' resolved per-mod settings directory after the registered
info.ini defaults provider has run, allowing runtime modules to reuse the
core's `SettingsDirectory` policy without retaining core-owned storage.

Revision 13 appends `register_game_object_class_loaded_handler` and
`register_race_loaded_handler`, with
`A2FO_CAP_GAME_OBJECT_CLASS_LOADED` and `A2FO_CAP_RACE_LOADED`. A module
registers the inherited ODF field names it needs during startup; the core
copies those names, owns the shared Armada/Fleet Operations loader hooks, and
supplies temporary field views at the completed class or Race boundary.
Revision 13 also adds notification-only Producer events for cancellation,
single queued-item deletion, and queue clearing so sidecar cost systems can
mirror native refunds without taking ownership of FeaturePack's hooks.

Revision 14 appends `register_weapon_class_loaded_handler`,
`register_weapon_trigger_handler`, and `register_craft_event_handler`, with
their matching capability bits. The core is the sole owner of the generic
WeaponClass constructor, `Weapon::Trigger(GameObject)`, and Craft simulation /
cleanup / post-load entries. Trigger handlers receive a rejection-capable
`PRECHECK` followed by a notification-only `COMMITTED` event only when the
native trigger ran. Craft simulation notifications bracket the complete
native/Fleet Operations call. WeaponClass callbacks receive copied requested
ODF fields plus the live ParameterDB and parent-class pointer for typed native
lookups and inherited policies.

Revision 15 appends `register_craft_event_handler_masked`. It preserves the
revision-14 callback ABI while allowing a module to request only the Craft
event kinds it consumes. The core therefore avoids entering cleanup-only
handlers twice for every simulated craft. Modules fall back to the unmasked
registration when running against a revision-14 core.

Revision 16 appends `register_weapon_trigger_handler_masked`. It preserves the
revision-14 callback ABI while allowing a module to request only precheck or
committed trigger events. FireArcs, NormalWeaponTech, and Turrets request only
prechecks, avoiding three no-op cross-DLL callbacks after every accepted shot.

Revision 17 appends `register_race_odf_defaults`, guarded by
`A2FO_CAP_RACE_ODF_DEFAULTS`. Modules register copied command/value pairs at
startup. At the completed-Race boundary, the core consults each fallback only
after the live ParameterDB reports that requested command missing, then exposes
the resolved value to every Race-loaded callback. A command has one policy
owner, and explicit or inherited Race ODF values always win.
