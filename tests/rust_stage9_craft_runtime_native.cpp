// Whole frozen/current source is included by the existing headless hover fixture.
#include <windows.h>
extern "C" SIZE_T WINAPI craft_fixture_virtual_query(
    LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T);
#define a2fo_identity_fo_sprite_set_colour craft_fixture_sprite_set_colour
#define a2fo_identity_fo_sprite_draw_scaled_2d craft_fixture_sprite_draw_scaled_2d
#define a2fo_identity_call_thiscall_2 craft_fixture_call_thiscall_2
#define VirtualQuery craft_fixture_virtual_query
#include CRAFT_BASE_SOURCE
#undef a2fo_identity_fo_sprite_set_colour
#undef a2fo_identity_fo_sprite_draw_scaled_2d
#undef a2fo_identity_call_thiscall_2
#undef VirtualQuery
#include <cstdlib>
#include <new>

extern "C" std::uintptr_t __cdecl a2fo_identity_call_thiscall_2(
    void*, void*, std::uintptr_t, std::uintptr_t);

static int join_allocation_budget = -1;
static unsigned join_allocation_count = 0;
__attribute__((noinline)) void* operator new(std::size_t size) {
    if (join_allocation_budget >= 0) {
        ++join_allocation_count;
        if (join_allocation_budget == 0) {
            join_allocation_budget = -1;
            throw std::bad_alloc();
        }
        --join_allocation_budget;
    }
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
__attribute__((noinline)) void operator delete(void* memory) noexcept { std::free(memory); }
__attribute__((noinline)) void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
__attribute__((noinline)) void* operator new[](std::size_t size) { return ::operator new(size); }
__attribute__((noinline)) void operator delete[](void* memory) noexcept { ::operator delete(memory); }
__attribute__((noinline)) void operator delete[](void* memory, std::size_t) noexcept { ::operator delete(memory); }

namespace {
std::array<std::uint8_t, 0x100> sprite_a{}, sprite_b{};
std::array<std::uint8_t, 0x10> frame_sentinel{};
std::vector<std::string> sprite_trace;
std::vector<std::string> lookup_trace;
void** mutate_sprite_slot = nullptr;
bool mutate_on_next_colour = false;
void* sprite_database = nullptr;
bool return_requested_sprite = true;
bool return_fallback_sprite = true;
std::array<std::uint8_t, 0x100> graphics_renderer{};
std::array<std::uint8_t, 0x80> storm_texture{};
std::array<std::uint8_t, 0x10> device_texture{};
alignas(void*) std::array<std::uint8_t, 0x10> texture_object{};
std::array<void*, 18> texture_vtable{};
unsigned texture_desc_calls = 0;
std::array<std::uint8_t, 0x100> captain_component{}, name_component{},
    class_component{}, builder_name_component{}, builder_class_component{};
bool anchor_trace_active = false;
bool anchor_mutate_on_first_rectangle = false;
std::vector<std::string> anchor_queries;

HRESULT __stdcall fixture_get_level_desc(
    IDirect3DTexture8*, UINT level, D3DSURFACE_DESC* description) {
    assert(level == 0 && description);
    ++texture_desc_calls;
    *description = {};
    description->Width = 16;
    description->Height = 16;
    return D3D_OK;
}

int sprite_id(void* sprite) {
    return sprite == sprite_a.data() ? 1 : sprite == sprite_b.data() ? 2 : 0;
}

std::uint32_t float_bits(float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void record_sprite(const char* operation, void* sprite, float width = 0.0f) {
    char line[192]{};
    const Colour tint = read_at<Colour>(sprite, kSpriteColourOffset, Colour{});
    const float window_x = read_at<float>(sprite, kSpriteTextureXOffset, 0.0f);
    const float window_y = read_at<float>(sprite, kSpriteTextureYOffset, 0.0f);
    const float window_width = read_at<float>(sprite, kSpriteTextureWidthOffset, 0.0f);
    const float window_height = read_at<float>(sprite, kSpriteTextureHeightOffset, 0.0f);
    std::snprintf(line, sizeof(line), "%s:%d:%08lx:%08lx:%08lx:%08lx:%08lx:%08lx",
        operation, sprite_id(sprite), static_cast<unsigned long>(float_bits(width)),
        static_cast<unsigned long>(float_bits(window_x)),
        static_cast<unsigned long>(float_bits(window_y)),
        static_cast<unsigned long>(float_bits(window_width)),
        static_cast<unsigned long>(float_bits(window_height)),
        static_cast<unsigned long>(float_bits(tint.red)));
    sprite_trace.emplace_back(line);
}

int anchor_id(void* component) {
    if (component == captain_component.data()) return 1;
    if (component == name_component.data()) return 2;
    if (component == class_component.data()) return 3;
    if (component == builder_name_component.data()) return 4;
    if (component == builder_class_component.data()) return 5;
    return 0;
}

void prepare_sprite(std::array<std::uint8_t, 0x100>& sprite,
                    const Colour& colour) {
    sprite.fill(0);
    put(sprite.data(), kSpriteFrameListOffset, frame_sentinel.data());
    put(sprite.data(), kSpriteColourOffset, colour);
    put(sprite.data(), kSpriteTextureXOffset, 0.125f);
    put(sprite.data(), kSpriteTextureYOffset, 0.25f);
    put(sprite.data(), kSpriteTextureWidthOffset, 0.75f);
    put(sprite.data(), kSpriteTextureHeightOffset, 0.625f);
    put(sprite.data(), kSpriteTextureObjectOffset, storm_texture.data());
}

std::array<std::uint8_t, 0x100> shared_name_component{};
Colour seen_during_native_render{};
bool native_render_called = false;

std::uintptr_t __fastcall capture_native_render(void* self, void*) {
    assert(self == display.data());
    seen_during_native_render = read_at<Colour>(
        shared_name_component.data(), kTextComponentColourOffset, Colour{});
    native_render_called = true;
    return 0;
}

extern "C" std::uintptr_t __cdecl craft_fixture_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2) {
    if (function == at(g_armada, kInterfaceSpriteDatabaseGetRva)) {
        assert(self == sprite_database && argument2 == 0);
        const char* name = reinterpret_cast<const char*>(argument1);
        lookup_trace.emplace_back(name);
        if (std::strcmp(name, "requested_icon") == 0 && return_requested_sprite)
            return reinterpret_cast<std::uintptr_t>(sprite_a.data());
        if (std::strcmp(name, "missing_bar") == 0 && return_requested_sprite)
            return reinterpret_cast<std::uintptr_t>(sprite_a.data());
        if (std::strcmp(name, "large_shield_bar") == 0 && return_fallback_sprite)
            return reinterpret_cast<std::uintptr_t>(sprite_a.data());
        return 0;
    }
    return a2fo_identity_call_thiscall_2(
        function, self, argument1, argument2);
}

extern "C" void craft_fixture_sprite_set_colour(
    void* function, void* sprite, const void* colour) {
    assert(function == at(g_fleet_ops, kFoSpriteSetColourRva));
    std::memcpy(static_cast<std::uint8_t*>(sprite) + kSpriteColourOffset,
                colour, sizeof(Colour));
    record_sprite("colour", sprite);
    if (mutate_on_next_colour) {
        assert(mutate_sprite_slot && *mutate_sprite_slot == sprite_a.data());
        *mutate_sprite_slot = sprite_b.data();
        mutate_on_next_colour = false;
    }
}

extern "C" void craft_fixture_sprite_draw_scaled_2d(
    void* function, void* sprite, const void* position,
    float display_width, float display_height) {
    assert(function == at(g_fleet_ops, kFoSpriteDrawScaled2DRva));
    assert(position && std::isfinite(display_width) && std::isfinite(display_height));
    record_sprite("draw", sprite, display_width);
}

extern "C" SIZE_T WINAPI craft_fixture_virtual_query(
    LPCVOID address, PMEMORY_BASIC_INFORMATION information, SIZE_T length) {
    if (anchor_trace_active) {
        const char* symbol = nullptr;
        const auto* slot = display.data();
        if (address == slot + kInfoDisplayCaptainTextOffset) symbol = "captain-slot";
        else if (address == slot + kInfoDisplayNameTextOffsets[0]) symbol = "name-slot";
        else if (address == slot + kInfoDisplayClassTextOffset) symbol = "class-slot";
        else if (address == slot + kInfoDisplayBuilderNameTextOffset) symbol = "builder-name-slot";
        else if (address == slot + kInfoDisplayBuilderClassTextOffset) symbol = "builder-class-slot";
        else if (address == captain_component.data() + kTextComponentLiveRectangleOffset)
            symbol = "captain-rect";
        else if (address == name_component.data() + kTextComponentLiveRectangleOffset)
            symbol = "name-rect";
        else if (address == class_component.data() + kTextComponentLiveRectangleOffset)
            symbol = "class-rect";
        else if (address == builder_name_component.data() + kTextComponentLiveRectangleOffset)
            symbol = "builder-name-rect";
        else if (address == builder_class_component.data() + kTextComponentLiveRectangleOffset)
            symbol = "builder-class-rect";
        if (symbol) {
            anchor_queries.emplace_back(symbol);
            if (anchor_mutate_on_first_rectangle &&
                std::strstr(symbol, "-rect")) {
                anchor_mutate_on_first_rectangle = false;
                g_ui_configuration.captain_rectangle_found = false;
                g_ui_configuration.single_name_rectangle_found = false;
                g_ui_configuration.single_class_rectangle_found = false;
                g_ui_configuration.builder_name_rectangle_found = false;
                g_ui_configuration.builder_class_rectangle_found = false;
                g_ui_configuration.captain_rectangle = {-70, -80, 20, 10};
                g_ui_configuration.single_name_rectangle = {-7, -8, 20, 10};
                g_ui_configuration.single_class_rectangle = {-9, -10, 20, 10};
            }
        }
    }
    return VirtualQuery(address, information, length);
}

void __fastcall resample_ammunition_visibility(void* target, void*) {
    for (std::size_t index = 0;
         index < g_ammunition_hover_components.components.size(); ++index) {
        if (target != &g_ammunition_hover_components.components[index]) continue;
        update_order.push_back(static_cast<int>(index));
        if (index == 0) g_ammunition_tooltip.visible[1] = false;
        if (index == 2)
            g_ammunition_hover_components.components[index].presented = 1;
        return;
    }
    assert(false && "unexpected resample component");
}
}

int main() {
    stage9_existing_craft_hover_main();

    // The original fixture leaves a synthetic Craft and Armada image live.
    g_craft_identities.clear();
    g_class_policies.clear();
    ClassIdentityPolicy policy{};
    policy.odf_name = "fixture.odf";
    policy.craft_names = {"A", "B"};
    policy.captain_names = {"Cap A", "Cap B"};
    policy.craft_registries = {"NX-1", "NX-2"};
    g_class_policies[&object_class] = policy;
    put(craft.data(), kObjectHandleOffset, std::uint32_t{77});
    put(craft.data(), kCraftNameIndexOffset, std::int32_t{1});
    const CraftIdentity* identity = ensure_craft_identity(craft.data());
    assert(identity && identity->captain_name == "Cap B" &&
           identity->craft_registry == "NX-2");
    std::puts("identity:aligned-row-1");
    const CraftIdentity* cached = ensure_craft_identity(craft.data());
    assert(cached == identity);
    std::puts("identity:cached");
    put(craft.data(), kCraftNameIndexOffset, std::int32_t{4});
    identity = ensure_craft_identity(craft.data());
    assert(identity && identity->captain_name.empty() && identity->craft_registry.empty());
    std::puts("identity:out-of-range-blank");
    g_class_policies.erase(&object_class);
    assert(!ensure_craft_identity(craft.data()));
    assert(g_craft_identities.find(77) == g_craft_identities.end());
    std::puts("identity:missing-policy-erases");
    put(craft.data(), kObjectHandleOffset, std::uint32_t{0});
    assert(!ensure_craft_identity(craft.data()));
    std::puts("identity:zero-handle-gate");

    for (auto& component : g_ammunition_hover_components.components) component.presented = 0;
    g_ammunition_tooltip.active = false;
    update_ammunition_hover_components(display.data());
    assert(!g_ammunition_hover_components.info_display);
    std::puts("hover:inactive-reset");

    // Both native name slots alias one component. Only reverse restoration
    // returns its original colour after the native renderer observes the tint.
    const Colour original_colour{0.11f, 0.22f, 0.33f};
    const Colour selected_colour{0.66f, 0.55f, 0.44f};
    put(shared_name_component.data(), kTextComponentColourOffset, original_colour);
    for (std::size_t offset : kInfoDisplayNameTextOffsets)
        put(display.data(), offset, shared_name_component.data());
    put(display.data(), kInfoDisplaySelectedCraftOffset, static_cast<void*>(nullptr));
    g_runtime_ready = true;
    g_ui_configuration = {};
    g_ui_configuration.loaded = true;
    g_ui_configuration.ship_name_colour_found = true;
    g_ui_configuration.ship_name_colour = selected_colour;
    native_render_called = false;
    render_selected_info_panel(display.data(),
        reinterpret_cast<void*>(&capture_native_render), SelectedPanelKind::single);
    const Colour after_render = read_at<Colour>(
        shared_name_component.data(), kTextComponentColourOffset, Colour{});
    assert(native_render_called);
    assert(std::memcmp(&seen_during_native_render, &selected_colour,
                       sizeof(Colour)) == 0);
    assert(std::memcmp(&after_render, &original_colour, sizeof(Colour)) == 0);
    std::puts("render:aliased-name-reverse-restore");

    // The first native Update changes visibility. The later Rust callback
    // must sample the live runtime again before dispatching the next row.
    put(display.data(), kInfoDisplaySelectedCraftOffset, craft.data());
    g_ammunition_tooltip.active = true;
    g_ammunition_tooltip.info_display = display.data();
    g_ammunition_tooltip.craft = craft.data();
    g_ammunition_tooltip.visible = {{true, true, true}};
    g_ammunition_tooltip.hit_rectangles = {{
        {100, 20, 99, 9}, {100, 40, 99, 9}, {100, 60, 99, 9}}};
    g_ammunition_tooltip.current = {{51, 6, 3}};
    g_ammunition_tooltip.maximum = {{100, 20, 6}};
    auto& hover = g_ammunition_hover_components;
    hover.info_display = display.data();
    hover.craft = craft.data();
    hover.owner = owner.data();
    hover.current[0] = -1;
    const char* stale = "stale native text";
    hover.components[0].normal = stale;
    hover.components[0].verbose = stale;
    put(reinterpret_cast<std::uint8_t*>(g_armada), kCursorXRva, 120);
    put(reinterpret_cast<std::uint8_t*>(g_armada), kCursorYRva, 64);
    g_standard_component_update_hook.gateway =
        reinterpret_cast<void*>(&resample_ammunition_visibility);
    update_order.clear();
    update_ammunition_hover_components(display.data());
    assert(update_order == std::vector<int>({0, 2}));
    assert(hover.components[0].normal == hover.components[0].normal_storage.data());
    assert(hover.components[0].verbose == hover.components[0].verbose_storage.data());
    assert(std::strcmp(hover.components[0].normal,
                       "Photon Torpedo Ammunition: 51/100") == 0);
    assert(std::strcmp(hover.components[0].verbose,
                       "Photon torpedo magazine and recharge reserve.\nAmmunition: 51/100") == 0);
    std::puts("hover:resampled-visibility-and-text-pointers");

    // A synthetic readable image satisfies the checked signature probes.
    // The retry gate exits before any sprite database function is called.
    auto* fleet_ops = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x300000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    assert(fleet_ops);
    g_fleet_ops = reinterpret_cast<HMODULE>(fleet_ops);
    std::memcpy(at(g_armada, kInterfaceSpriteDatabaseGetRva),
                kExpectedInterfaceSpriteDatabaseGet,
                sizeof(kExpectedInterfaceSpriteDatabaseGet));
    std::memcpy(at(g_fleet_ops, kFoSpriteSetColourRva),
                kExpectedFoSpriteSetColour, sizeof(kExpectedFoSpriteSetColour));
    std::memcpy(at(g_fleet_ops, kFoSpriteDrawScaled2DRva),
                kExpectedFoSpriteDrawScaled2D,
                sizeof(kExpectedFoSpriteDrawScaled2D));
    put(reinterpret_cast<std::uint8_t*>(g_armada),
        kInterfaceSpriteDatabasePointerRva, static_cast<void*>(&owner));
    g_directional_shield_sprites = {};
    g_directional_shield_sprite_retry_database = &object_class;
    g_directional_shield_sprite_retry_after = GetTickCount() + 60000;
    assert(!resolve_directional_shield_sprites());
    assert(g_directional_shield_sprite_retry_database == &owner);
    assert(!g_directional_shield_sprites.resolution_attempted);
    const DWORD delayed = g_directional_shield_sprite_retry_after;
    assert(!resolve_directional_shield_sprites());
    assert(g_directional_shield_sprite_retry_after == delayed);
    put(reinterpret_cast<std::uint8_t*>(g_armada),
        kInterfaceSpriteDatabasePointerRva, static_cast<void*>(nullptr));
    g_directional_shield_sprite_retry_database = nullptr;
    g_directional_shield_sprite_retry_after = GetTickCount() - 1;
    assert(!resolve_directional_shield_sprites());
    assert(g_directional_shield_sprites.resolution_attempted);
    assert(!g_directional_shield_sprites.database);
    std::puts("sprite:retry-gate-and-missing-database");

    // The COM vtable and Storm/renderer shells are inert synthetic memory.
    // Their only callable entry returns deterministic dimensions.
    texture_vtable[14] = reinterpret_cast<void*>(&fixture_get_level_desc);
    put(texture_object.data(), 0, texture_vtable.data());
    put(device_texture.data(), kStorm3DDeviceTextureNativeOffset,
        reinterpret_cast<IDirect3DTexture8*>(texture_object.data()));
    put(storm_texture.data(), kStorm3DTextureDeviceArrayOffset,
        device_texture.data());
    put(graphics_renderer.data(), kCurrentDeviceIndexOffset, std::uint32_t{0});
    put(reinterpret_cast<std::uint8_t*>(g_armada),
        kGraphicsEnginePointerRva, graphics_renderer.data());
    sprite_database = &owner;
    put(reinterpret_cast<std::uint8_t*>(g_armada),
        kInterfaceSpriteDatabasePointerRva, sprite_database);
    const Colour saved_a{0.125f, 0.25f, 0.375f};
    const Colour saved_b{0.5f, 0.625f, 0.75f};
    const Colour tint{0.875f, 0.75f, 0.625f};
    const NativeRectangle icon_rectangle{10, 20, 90, 40};
    const RawRectangle crop{3, 5, 7, 9};
    prepare_sprite(sprite_a, saved_a);
    prepare_sprite(sprite_b, saved_b);
    g_ammunition_icon_cache = {};
    g_ammunition_icon_cache.database = sprite_database;
    g_ammunition_icon_cache.names[0] = "requested_icon";
    g_ammunition_icon_cache.sprites[0] = sprite_a.data();
    g_ammunition_icon_cache.attempted[0] = true;
    mutate_sprite_slot = &g_ammunition_icon_cache.sprites[0];
    mutate_on_next_colour = true;
    sprite_trace.clear();
    lookup_trace.clear();
    assert(draw_ammunition_icon(0, "requested_icon", crop, icon_rectangle,
                                tint, 0, 0));
    assert(lookup_trace.empty() && texture_desc_calls == 1);
    assert(g_ammunition_icon_cache.sprites[0] == sprite_b.data());
    assert(sprite_trace.size() == 3);
    assert(sprite_trace[0].rfind("colour:1:", 0) == 0);
    assert(sprite_trace[0].find(":3e400000:3ea00000:3ee00000:3f100000:") !=
           std::string::npos);
    assert(sprite_trace[1].rfind("draw:2:", 0) == 0);
    assert(sprite_trace[2].rfind("colour:2:", 0) == 0);
    const Colour tinted_a = read_at<Colour>(sprite_a.data(), kSpriteColourOffset, Colour{});
    const Colour restored_b = read_at<Colour>(sprite_b.data(), kSpriteColourOffset, Colour{});
    assert(std::memcmp(&tinted_a, &tint, sizeof(tint)) == 0);
    assert(std::memcmp(&restored_b, &saved_a, sizeof(saved_a)) == 0);
    assert(read_at<float>(sprite_a.data(), kSpriteTextureXOffset, 0) == 0.125f);
    assert(read_at<float>(sprite_a.data(), kSpriteTextureYOffset, 0) == 0.25f);
    assert(read_at<float>(sprite_a.data(), kSpriteTextureWidthOffset, 0) == 0.75f);
    assert(read_at<float>(sprite_a.data(), kSpriteTextureHeightOffset, 0) == 0.625f);
    std::puts("sprite:icon-live-cache-slot");
    for (const auto& line : sprite_trace) std::puts(line.c_str());

    sprite_trace.clear();
    assert(!draw_ammunition_icon(0, "requested_icon", {14, 5, 7, 9},
                                 icon_rectangle, tint, 0, 0));
    assert(sprite_trace.empty());
    std::puts("sprite:icon-crop-bounds");
    prepare_sprite(sprite_a, saved_a);
    g_ammunition_icon_cache.sprites[0] = sprite_a.data();
    g_ammunition_icon_cache.attempted[0] = true;
    put(sprite_a.data(), kSpriteFrameListOffset, static_cast<void*>(nullptr));
    lookup_trace.clear();
    assert(!draw_ammunition_icon(0, "requested_icon", crop,
                                 icon_rectangle, tint, 0, 0));
    assert(!g_ammunition_icon_cache.sprites[0] &&
           !g_ammunition_icon_cache.attempted[0] && lookup_trace.empty());
    prepare_sprite(sprite_a, saved_a);
    assert(draw_ammunition_icon(0, "requested_icon", crop,
                                icon_rectangle, tint, 0, 0));
    assert(lookup_trace == std::vector<std::string>({"requested_icon"}));
    std::puts("sprite:icon-stale-quiet-then-retry");

    const PanelElement bar_element = PanelElement::PhotonTorpedoesBar;
    auto& bar_style = g_panel_styles[0][static_cast<std::size_t>(bar_element)];
    bar_style = {};
    bar_style.background_sprite = "requested_bg";
    bar_style.sprite = "requested_bar";
    auto& bar_slots = g_ammunition_bar_cache[static_cast<std::size_t>(bar_element)];
    bar_slots = {};
    prepare_sprite(sprite_a, saved_a);
    prepare_sprite(sprite_b, saved_b);
    put(sprite_b.data(), kSpriteFrameListOffset, static_cast<void*>(nullptr));
    bar_slots[0].database = sprite_database;
    bar_slots[0].name = "requested_bg";
    bar_slots[0].sprite = sprite_b.data();
    bar_slots[0].attempted = true;
    bar_slots[1].database = sprite_database;
    bar_slots[1].name = "requested_bar";
    bar_slots[1].sprite = sprite_a.data();
    bar_slots[1].attempted = true;
    mutate_sprite_slot = &bar_slots[1].sprite;
    mutate_on_next_colour = true;
    sprite_trace.clear();
    assert(draw_ammunition_value_bar(icon_rectangle, 0.5f, tint,
                                     0, 0, nullptr, bar_element));
    assert(bar_slots[1].sprite == sprite_b.data());
    assert(sprite_trace.size() == 3);
    assert(sprite_trace[0].rfind("colour:1:", 0) == 0);
    assert(sprite_trace[1].rfind("draw:1:", 0) == 0);
    assert(sprite_trace[1].find(":3ec00000:") != std::string::npos);
    assert(sprite_trace[2].rfind("colour:1:", 0) == 0);
    assert(sprite_trace[2].find(":3f400000:") != std::string::npos);
    assert(read_at<float>(sprite_a.data(), kSpriteTextureWidthOffset, 0) == 0.75f);
    std::puts("sprite:bar-snapshot-and-width-before-colour");
    for (const auto& line : sprite_trace) std::puts(line.c_str());

    bar_slots = {};
    bar_style.background_sprite = "missing_bar";
    bar_style.sprite = "missing_bar";
    prepare_sprite(sprite_a, saved_a);
    prepare_sprite(sprite_b, saved_b);
    return_requested_sprite = false;
    return_fallback_sprite = true;
    mutate_sprite_slot = nullptr;
    mutate_on_next_colour = false;
    sprite_trace.clear();
    lookup_trace.clear();
    assert(draw_ammunition_value_bar(icon_rectangle, 0.5f, tint,
                                     0, 0, nullptr, bar_element));
    assert(lookup_trace == std::vector<std::string>({"missing_bar",
        "large_shield_bar", "missing_bar", "large_shield_bar"}));
    assert(sprite_trace.size() == 6);
    std::puts("sprite:bar-requested-fallback-and-foreground");
    for (const auto& line : sprite_trace) std::puts(line.c_str());

    const auto set_anchor_component = [](std::size_t offset, void* component,
                                         NativeRectangle live) {
        put(display.data(), offset, component);
        if (component) put(component, kTextComponentLiveRectangleOffset, live);
    };
    const auto run_anchor = [](const char* label, SelectedPanelKind kind) {
        anchor_queries.clear();
        anchor_trace_active = true;
        const SelectedPanelTextAnchor result =
            selected_panel_text_anchor(display.data(), kind);
        anchor_trace_active = false;
        std::printf("anchor:%s:%d:%ld,%ld,%ld,%ld\n", label,
            anchor_id(result.component),
            static_cast<long>(result.captain_rectangle.left),
            static_cast<long>(result.captain_rectangle.top),
            static_cast<long>(result.captain_rectangle.right),
            static_cast<long>(result.captain_rectangle.bottom));
        for (const auto& symbol : anchor_queries)
            std::printf("anchor-query:%s:%s\n", label, symbol.c_str());
        return result;
    };
    g_ui_configuration = {};
    set_anchor_component(kInfoDisplayCaptainTextOffset,
        captain_component.data(), {10, 20, 40, 36});
    set_anchor_component(kInfoDisplayNameTextOffsets[0],
        name_component.data(), {100, 200, 140, 220});
    set_anchor_component(kInfoDisplayClassTextOffset,
        class_component.data(), {50, 60, 90, 80});
    set_anchor_component(kInfoDisplayBuilderNameTextOffset,
        builder_name_component.data(), {110, 210, 150, 230});
    set_anchor_component(kInfoDisplayBuilderClassTextOffset,
        builder_class_component.data(), {55, 65, 95, 85});
    const auto single_captain = run_anchor("single-captain", SelectedPanelKind::single);
    const auto builder_captain = run_anchor("builder-captain", SelectedPanelKind::builder);
    assert(single_captain.component == captain_component.data() &&
           builder_captain.component == captain_component.data());
    assert(single_captain.captain_rectangle.left == 10 &&
           builder_captain.captain_rectangle.bottom == 36);

    put(captain_component.data(), kTextComponentLiveRectangleOffset,
        NativeRectangle{});
    g_ui_configuration.captain_rectangle_found = true;
    g_ui_configuration.captain_rectangle = {5, 6, 30, 12};
    g_ui_configuration.single_name_rectangle_found = true;
    g_ui_configuration.single_name_rectangle = {10, 20, 40, 20};
    const auto name_fallback = run_anchor("single-name-rebased", SelectedPanelKind::single);
    assert(name_fallback.component == name_component.data());
    assert(name_fallback.captain_rectangle.left == 95 &&
           name_fallback.captain_rectangle.top == 186 &&
           name_fallback.captain_rectangle.right == 125 &&
           name_fallback.captain_rectangle.bottom == 198);

    set_anchor_component(kInfoDisplayBuilderNameTextOffset, nullptr, {});
    g_ui_configuration.builder_class_rectangle_found = true;
    g_ui_configuration.builder_class_rectangle = {3, 4, 40, 20};
    const auto class_fallback = run_anchor("builder-class-rebased", SelectedPanelKind::builder);
    assert(class_fallback.component == builder_class_component.data());
    assert(class_fallback.captain_rectangle.left == 57 &&
           class_fallback.captain_rectangle.top == 67 &&
           class_fallback.captain_rectangle.right == 87 &&
           class_fallback.captain_rectangle.bottom == 79);

    g_ui_configuration.captain_rectangle_found = false;
    g_ui_configuration.single_name_rectangle_found = false;
    const auto unconfigured = run_anchor("single-name-unconfigured", SelectedPanelKind::single);
    assert(unconfigured.component == name_component.data());
    assert(unconfigured.captain_rectangle.left == 100 &&
           unconfigured.captain_rectangle.bottom == 220);

    set_anchor_component(kInfoDisplayNameTextOffsets[0], nullptr, {});
    set_anchor_component(kInfoDisplayClassTextOffset, nullptr, {});
    g_ui_configuration.captain_rectangle_found = true;
    g_ui_configuration.captain_rectangle = {-10, -20, 0, 1};
    put(display.data(), 0x1f0, std::int32_t{7});
    const auto hidden = run_anchor("hidden-captain-raw", SelectedPanelKind::single);
    assert(hidden.component == captain_component.data());
    assert(hidden.captain_rectangle.left == -10 &&
           hidden.captain_rectangle.top == -27 &&
           hidden.captain_rectangle.right == -9 &&
           hidden.captain_rectangle.bottom == -26);

    set_anchor_component(kInfoDisplayNameTextOffsets[0],
        name_component.data(), {100, 200, 140, 220});
    set_anchor_component(kInfoDisplayClassTextOffset,
        class_component.data(), {50, 60, 90, 80});
    g_ui_configuration.captain_rectangle_found = true;
    g_ui_configuration.captain_rectangle = {5, 6, 30, 12};
    g_ui_configuration.single_name_rectangle_found = true;
    g_ui_configuration.single_name_rectangle = {10, 20, 40, 20};
    g_ui_configuration.single_class_rectangle_found = true;
    g_ui_configuration.single_class_rectangle = {3, 4, 40, 20};
    anchor_mutate_on_first_rectangle = true;
    const auto mutated = run_anchor("mutation-read-order", SelectedPanelKind::single);
    assert(!anchor_mutate_on_first_rectangle);
    assert(mutated.component == name_component.data());
    assert(usable_native_rectangle(mutated.captain_rectangle));

    const auto absent = selected_panel_text_anchor(nullptr, SelectedPanelKind::builder);
    assert(!absent.component && !usable_native_rectangle(absent.captain_rectangle));
    std::puts("anchor:null-display");

    using JoinPathFn = std::string (*)(const std::string&, const std::string&);
    JoinPathFn volatile join_call = &join_path;
    const std::string long_left = "extensions\\a-very-long-craft-path";
    const std::string long_right = "configuration-with-long-name.cfg";
    struct JoinCase { const char* name; std::string left, right, expected; };
    const std::array<JoinCase, 4> join_cases{{
        {"empty-left", "", long_right, long_right},
        {"empty-right", long_left, "", long_left},
        {"existing-separator", long_left + "\\", long_right,
            long_left + "\\" + long_right},
        {"insert-separator", long_left, long_right,
            long_left + "\\" + long_right},
    }};
    for (const auto& sample : join_cases) {
        for (int budget : {-1, 0, 1, 2}) {
            std::string result;
            bool allocation_failed = false;
            join_allocation_count = 0;
            join_allocation_budget = budget;
            try {
                result = join_call(sample.left, sample.right);
            } catch (const std::bad_alloc&) {
                allocation_failed = true;
            }
            join_allocation_budget = -1;
            if (!allocation_failed) assert(result == sample.expected);
            else assert(result.empty());
            std::printf("join:%s:%d:%u:%u:%s\n", sample.name, budget,
                join_allocation_count, allocation_failed, result.c_str());
        }
    }
}
