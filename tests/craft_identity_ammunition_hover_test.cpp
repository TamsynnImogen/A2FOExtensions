// Headless regression for the production ammunition hover ordering and text.
// All native call targets below are synthetic; no game binary is loaded.
#ifndef A2FO_AMMUNITION_MODULE_SOURCE
#define A2FO_AMMUNITION_MODULE_SOURCE "../modules/A2FOCraftIdentity/module.cpp"
#endif
#include A2FO_AMMUNITION_MODULE_SOURCE

#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

#ifdef NDEBUG
#error "This regression requires active assertions"
#endif

namespace {
std::array<std::uint8_t, 0x210> display{};
std::array<std::uint8_t, 0x300> craft{};
std::array<std::uint8_t, 0x68> owner_vtable{};
std::array<std::uint8_t, 0x10> owner{};
NativeRectangle owner_rectangle{5, 7, 805, 607};
int object_class;
int selected_index = -1;
std::vector<int> update_order;

template <class T> void put(void* base, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value, sizeof(value));
}

NativeRectangle* __fastcall get_owner_rectangle(void*, void*) {
    return &owner_rectangle;
}

void __fastcall capture_update(void* component_ptr, void*) {
    for (std::size_t index = 0;
         index < g_ammunition_hover_components.components.size(); ++index) {
        auto& component = g_ammunition_hover_components.components[index];
        if (&component != component_ptr) continue;
        update_order.push_back(static_cast<int>(index));
        if (static_cast<int>(index) == selected_index) {
            component.presented = 1;
        }
        return;
    }
    assert(false && "unexpected native update target");
}

void check_cursor(std::uint8_t* image, int x, int y, int expected) {
    put(image, kCursorXRva, x);
    put(image, kCursorYRva, y);
    selected_index = expected;
    update_order.clear();
    update_ammunition_hover_components(display.data());

    std::vector<int> expected_order;
    for (int index = 0; index < 3; ++index) {
        if (index != expected && g_ammunition_tooltip.visible[index]) {
            expected_order.push_back(index);
        }
    }
    if (expected >= 0) expected_order.push_back(expected);
    if (update_order != expected_order) {
        std::fprintf(stderr, "cursor=%d,%d selected=%d updates=", x, y, expected);
        for (int index : update_order) std::fprintf(stderr, "%d,", index);
        std::fprintf(stderr, " expected=");
        for (int index : expected_order) std::fprintf(stderr, "%d,", index);
        std::fprintf(stderr, "\n");
    }
    assert(update_order == expected_order);
    for (std::size_t index = 0; index < 3; ++index) {
        const auto& component = g_ammunition_hover_components.components[index];
        assert(component.normal && component.verbose);
        assert(component.rectangle.left == 95);
        assert(component.rectangle.top == 13 + static_cast<int>(index) * 20);
        assert(component.rectangle.right == 193);
        assert(component.rectangle.bottom == 21 + static_cast<int>(index) * 20);
    }
    if (expected >= 0) {
        assert(g_ammunition_hover_components.components[expected].presented == 1);
    }
}
}

int main() {
    auto* image = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x400000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    assert(image);
    g_armada = reinterpret_cast<HMODULE>(image);
    put(image, kActiveInterfaceOwnerRva, owner.data());
    put(owner.data(), 0, owner_vtable.data());
    put(owner_vtable.data(), 0x64,
        reinterpret_cast<void*>(&get_owner_rectangle));
    put(display.data(), kInfoDisplaySelectedCraftOffset, craft.data());
    put(craft.data(), kObjectClassOffset, &object_class);
    g_directional_shield_wireframe_tooltips_available = true;
    g_standard_component_update_hook.gateway =
        reinterpret_cast<void*>(&capture_update);

    AmmunitionUiPolicy policy{};
    policy.stores[2].label = "Fightercraft";
    policy.stores[2].tooltip = "Fightercraft complement";
    policy.stores[2].verbose_tooltip = "Fightercraft reserve";
    g_ammunition_ui_policies[&object_class] = policy;

    auto& runtime = g_ammunition_tooltip;
    runtime.active = true;
    runtime.info_display = display.data();
    runtime.craft = craft.data();
    runtime.visible = {{true, true, true}};
    runtime.hit_rectangles = {{
        {100, 20, 99, 9}, {100, 40, 99, 9}, {100, 60, 99, 9}}};
    runtime.current = {{50, 6, 3}};
    runtime.maximum = {{100, 20, 6}};

    check_cursor(image, 120, 24, 0);
    check_cursor(image, 120, 44, 1);
    check_cursor(image, 120, 64, 2);
    assert(std::strcmp(g_ammunition_hover_components.components[0].normal,
        "Photon Torpedo Ammunition: 50/100") == 0);
    assert(std::strcmp(g_ammunition_hover_components.components[1].normal,
        "Quantum Torpedo Ammunition: 6/20") == 0);
    assert(std::strcmp(g_ammunition_hover_components.components[2].normal,
        "Fightercraft complement: 3/6") == 0);
    assert(std::strcmp(g_ammunition_hover_components.components[2].verbose,
        "Fightercraft reserve\nAmmunition: 3/6") == 0);
    check_cursor(image, 400, 400, -1);

    runtime.visible[1] = false;
    check_cursor(image, 120, 64, 2);
    assert(update_order == std::vector<int>({0, 2}));
    std::puts("A2FOCraftIdentity ammunition hover: indices 0/1/2, selected-last, Fightercraft text, off-row and hidden row PASS");
}
