// Included inside module.cpp's anonymous namespace; shares its checked access
// helpers and initialization gate. No state is attached to class/ODF objects.
constexpr std::uintptr_t kActionRva = 0x04ab70;
constexpr std::uintptr_t kBuildRva = 0x0355d0;
constexpr std::uint8_t kBuildBytes[] = {0x55,0x8b,0xec,0x83,0xec,0x4c};
constexpr std::uintptr_t kRenderFootprintsRva = 0x073930;
constexpr std::uintptr_t kFoRenderFootprintsRva = 0x1fb3b0;
constexpr std::uint8_t kFoRenderFootprintsBytes[] = {0x55,0x8b,0xec,0x81,0xc4,0x48,0xff,0xff,0xff};
constexpr std::uintptr_t kFootprintRva = 0x0c0240;
constexpr std::uintptr_t kCanPlaceRva = 0x0c01b0;
constexpr std::uintptr_t kPlacementClearRva = 0x094de0;
constexpr std::uintptr_t kFoCanPlaceRva = 0x10db80;
constexpr std::uintptr_t kFoAddFootprintRva = 0x10df84;
constexpr std::uint8_t kFoCanPlaceBytes[] = {0x55,0x8b,0xec,0x83,0xc4,0x84};
constexpr std::uint8_t kFoAddFootprintBytes[] = {0x55,0x8b,0xec,0x51,0x89,0x4d,0xfc};
constexpr std::uintptr_t kAddFootprintRva = 0x0c0e40;
constexpr std::uintptr_t kRemoveFootprintRva = 0x0c0f40;
constexpr std::uint8_t kActionBytes[] = {0x55,0x8b,0xec,0x6a,0xff};
constexpr std::uint8_t kRenderFootprintsBytes[] = {0x55,0x8b,0xec,0x83,0xec,0x3c};
constexpr std::uint8_t kFootprintBytes[] = {0x55,0x8b,0xec,0x8b,0x81,0xd8,0x01,0x00,0x00};
constexpr std::uint8_t kCanPlaceBytes[] = {0x55,0x8b,0xec,0x8b,0x81,0xd8,0x01,0x00,0x00};
constexpr std::uint8_t kAddFootprintBytes[] = {0x55,0x8b,0xec,0x56,0x8b,0xf1};
constexpr std::uint8_t kRemoveFootprintBytes[] = {0x55,0x8b,0xec,0x53,0x56,0x8b,0xf1};
// The placement clear signature is verified alongside its argument pushes in
// CraftClass::CanPlaceHere. All nine stack arguments remain native.
constexpr std::uint8_t kPlacementClearBytes[] = {0x55,0x8b,0xec,0x83,0xec,0x4c};
A2FO_InlineHook g_action{}, g_render_footprints{}, g_footprint{}, g_can_place{}, g_placement_clear{};
A2FO_InlineHook g_build{};
const void* g_build_process = nullptr;
const void* g_placement_class = nullptr;
unsigned g_placement_turns = 0;
unsigned g_ui_footprint_depth = 0;
using Action = void* (__cdecl*)(void*, const void*, const float*, bool*);
using RenderFootprints = void (__fastcall*)(void*, void*, void*, float);
using Footprint = void (__fastcall*)(void*, void*, float, float, float*, float*, float*, float*);
using CanPlace = bool (__fastcall*)(void*, void*, const float*, const void*, bool);
using PlacementClear = bool (__fastcall*)(void*, void*, float, float, float, float,
                                         float, float, float, const void*, bool);
using PathFootprint = void (__fastcall*)(void*, void*, const float*, const float*, bool);
using Build = void (__fastcall*)(void*, void*);

void __fastcall build_hook(void* self, void*) {
    const void* previous = g_build_process;
    g_build_process = self;
    reinterpret_cast<Build>(g_build.gateway)(self, nullptr);
    g_build_process = previous;
}

unsigned construction_turns(const float* position) {
    if (!g_ready || !g_build_process || !accessible(position, 12)) return 0;
    const void* producer = read<const void*>(g_build_process, 0x30);
    const auto* placement = read<const std::uint8_t*>(producer, 0x2a4);
    if (!placement || !accessible(placement, 0x3c) || !placement[0x38] ||
        std::memcmp(position, placement+0x28, 12)) return 0;
    float matrix[12]; std::memcpy(matrix, placement+4, sizeof(matrix));
    return cardinal_turns(matrix);
}

struct UiFootprintScope {
    UiFootprintScope() { ++g_ui_footprint_depth; }
    ~UiFootprintScope() { --g_ui_footprint_depth; }
};
unsigned ui_turns(const void* cls) {
    if (!g_ready || !g_ui_footprint_depth || cls != placement_class()) return 0;
    g_control.observe(reinterpret_cast<std::uintptr_t>(cls));
    return g_control.turns;
}
Margins class_margins(const void* cls) {
    return {read<float>(cls, 0x218), read<float>(cls, 0x21c),
            read<float>(cls, 0x220), read<float>(cls, 0x224)};
}
void* __cdecl action_hook(void* result, const void* object, const float* position, bool* valid) {
    UiFootprintScope scope;
    return reinterpret_cast<Action>(g_action.gateway)(result, object, position, valid);
}
void __fastcall render_footprints_hook(void* self, void*, void* camera, float elapsed) {
    UiFootprintScope scope;
    reinterpret_cast<RenderFootprints>(g_render_footprints.gateway)(self, nullptr, camera, elapsed);
}
void __fastcall footprint_hook(void* cls, void*, float x, float z,
                               float* min_x, float* min_z, float* max_x, float* max_z) {
    reinterpret_cast<Footprint>(g_footprint.gateway)(cls, nullptr, x, z, min_x, min_z, max_x, max_z);
    const unsigned turns = ui_turns(cls);
    if (!turns) return;
    const auto rotated = rotate_rectangle({*min_x-x, *min_z-z, *max_x-x, *max_z-z}, turns);
    *min_x = x + rotated.min_x; *min_z = z + rotated.min_z;
    *max_x = x + rotated.max_x; *max_z = z + rotated.max_z;
}
bool __fastcall can_place_hook(void* cls, void*, const float* position, const void* ignored, bool test) {
    const void* previous_class = g_placement_class;
    const unsigned previous_turns = g_placement_turns;
    g_placement_class = cls;
    g_placement_turns = g_build_process ? construction_turns(position) : ui_turns(cls);
    // Preserve FO snapping, build-near restrictions and all native checks.
    const bool result = reinterpret_cast<CanPlace>(g_can_place.gateway)(cls, nullptr, position, ignored, test);
    g_placement_class = previous_class;
    g_placement_turns = previous_turns;
    return result;
}

bool __fastcall placement_clear_hook(void* self, void*, float min_x, float min_z,
        float max_x, float max_z, float min_y, float max_y, float clearance,
        const void* ignored, bool test) {
    const auto* geometry = read<const std::uint8_t*>(g_placement_class, 0x1d8);
    if (g_ready && g_placement_turns && geometry && accessible(geometry + 4, 24)) {
        float bounds[6]; std::memcpy(bounds, geometry + 4, sizeof(bounds));
        const auto local = padded_footprint(bounds, class_margins(g_placement_class));
        // Derive the pivot from the bounds actually passed by FO, after its
        // grid snapping. Rotating around the unsnapped cursor would drift.
        const float x = min_x - local.min_x;
        const float z = min_z - local.min_z;
        const auto r = rotate_rectangle(local, g_placement_turns);
        min_x = x+r.min_x; min_z = z+r.min_z;
        max_x = x+r.max_x; max_z = z+r.max_z;
    }
    return reinterpret_cast<PlacementClear>(g_placement_clear.gateway)(self, nullptr,
        min_x, min_z, max_x, max_z, min_y, max_y, clearance, ignored, test);
}

void path_footprint(std::uintptr_t native_rva, void* cls, const float* position,
                    const float* bounds, bool flag, const void* object) {
    const auto* instance = read<const std::uint8_t*>(object, 4);
    float matrix[12]{};
    if (g_ready && instance && accessible(instance + 0x44, sizeof(matrix)))
        std::memcpy(matrix, instance + 0x44, sizeof(matrix));
    const unsigned turns = cardinal_turns(matrix);
    std::array<float, 6> adjusted{};
    if (turns && accessible(bounds, sizeof(adjusted))) {
        std::memcpy(adjusted.data(), bounds, sizeof(adjusted));
        planner_bounds(adjusted.data(), class_margins(cls), turns);
        bounds = adjusted.data();
    }
    reinterpret_cast<PathFootprint>(at(native_rva))(cls, nullptr, position, bounds, flag);
}

struct FootprintCall { std::uintptr_t rva; std::uintptr_t native; void* replacement; };
const FootprintCall kFootprintCalls[] = {
    {0x07e534, kAddFootprintRva, reinterpret_cast<void*>(&a2fo_station_add_esi)},
    {0x0a34be, kAddFootprintRva, reinterpret_cast<void*>(&a2fo_station_add_ebx)},
    {0x0a36fb, kAddFootprintRva, reinterpret_cast<void*>(&a2fo_station_add_ebx)},
    {0x0b0440, kAddFootprintRva, reinterpret_cast<void*>(&a2fo_station_add_esi)},
    {0x0b0f1a, kAddFootprintRva, reinterpret_cast<void*>(&a2fo_station_add_esi)},
    {0x0b125c, kAddFootprintRva, reinterpret_cast<void*>(&a2fo_station_add_esi)},
    {0x0d05db, kAddFootprintRva, reinterpret_cast<void*>(&a2fo_station_add_esi)},
    {0x0a3560, kRemoveFootprintRva, reinterpret_cast<void*>(&a2fo_station_remove_ebx)},
    {0x0b0f99, kRemoveFootprintRva, reinterpret_cast<void*>(&a2fo_station_remove_esi)},
    {0x0b1003, kRemoveFootprintRva, reinterpret_cast<void*>(&a2fo_station_remove_esi)},
    {0x0b129d, kRemoveFootprintRva, reinterpret_cast<void*>(&a2fo_station_remove_esi)},
    {0x0c217e, kRemoveFootprintRva, reinterpret_cast<void*>(&a2fo_station_remove_edi)},
};
std::array<std::uint8_t, 5> call_bytes(const FootprintCall& call) {
    std::array<std::uint8_t, 5> result{0xe8,0,0,0,0};
    const auto displacement = static_cast<std::int32_t>(call.native - (call.rva + 5));
    std::memcpy(result.data()+1, &displacement, 4);
    return result;
}
bool preflight_footprints() {
    if (!matches(kBuildRva, kBuildBytes) || !matches(kActionRva, kActionBytes) ||
        !matches(kFootprintRva, kFootprintBytes) || !matches(kRemoveFootprintRva, kRemoveFootprintBytes) ||
        !matches(kPlacementClearRva, kPlacementClearBytes)) return false;
    if (g_fleet) {
        if (!matches_image(g_fleet, kFoCanPlaceRva, kFoCanPlaceBytes, sizeof(kFoCanPlaceBytes)) ||
            !matches_image(g_fleet, kFoAddFootprintRva, kFoAddFootprintBytes, sizeof(kFoAddFootprintBytes)) ||
            !matches_image(g_fleet, kFoRenderFootprintsRva, kFoRenderFootprintsBytes, sizeof(kFoRenderFootprintsBytes))) return false;
        const auto* entry = static_cast<const std::uint8_t*>(at(kAddFootprintRva));
        if (!accessible(entry, sizeof(kAddFootprintBytes))) return false;
        if (std::memcmp(entry, kAddFootprintBytes, sizeof(kAddFootprintBytes))) {
            // FO's HookCodeNt uses PUSH absolute target / RET, while some
            // installations use E9. Accept only the exact checked FO callback.
            const void* target = nullptr;
            if (entry[0] == 0xe9) target = entry+5+read<std::int32_t>(entry, 1);
            else if (entry[0] == 0x68 && entry[5] == 0xc3)
                target = reinterpret_cast<void*>(read<std::uintptr_t>(entry, 1));
            if (target != g_fleet+kFoAddFootprintRva) {
                log("Station rotation rejected unknown AddToPathPlanners detour at Armada RVA 0x000C0E40");
                return false;
            }
        }
    } else if (!matches(kCanPlaceRva, kCanPlaceBytes) || !matches(kAddFootprintRva, kAddFootprintBytes) ||
               !matches(kRenderFootprintsRva, kRenderFootprintsBytes)) return false;
    for (const auto& call : kFootprintCalls) {
        const auto bytes = call_bytes(call);
        if (!matches_image(g_armada, call.rva, bytes.data(), bytes.size())) return false;
    }
    return true;
}
bool install_footprints() {
    if (!install(kBuildRva, reinterpret_cast<void*>(&build_hook), kBuildBytes, g_build) ||
        !install(kActionRva, reinterpret_cast<void*>(&action_hook), kActionBytes, g_action) ||
        !g_api->install_inline_hook(g_fleet ? g_fleet+kFoRenderFootprintsRva : at(kRenderFootprintsRva),
            reinterpret_cast<void*>(&render_footprints_hook),
            g_fleet ? sizeof(kFoRenderFootprintsBytes) : sizeof(kRenderFootprintsBytes),
            g_fleet ? kFoRenderFootprintsBytes : kRenderFootprintsBytes, &g_render_footprints) ||
        !install(kFootprintRva, reinterpret_cast<void*>(&footprint_hook), kFootprintBytes, g_footprint) ||
        !g_api->install_inline_hook(g_fleet ? g_fleet+kFoCanPlaceRva : at(kCanPlaceRva),
            reinterpret_cast<void*>(&can_place_hook),
            g_fleet ? sizeof(kFoCanPlaceBytes) : sizeof(kCanPlaceBytes),
            g_fleet ? kFoCanPlaceBytes : kCanPlaceBytes, &g_can_place) ||
        !install(kPlacementClearRva, reinterpret_cast<void*>(&placement_clear_hook), kPlacementClearBytes, g_placement_clear)) return false;
    for (const auto& call : kFootprintCalls) {
        const auto bytes = call_bytes(call);
        if (!g_api->patch_call(at(call.rva), call.replacement, bytes.data(), bytes.size())) return false;
    }
    return true;
}
