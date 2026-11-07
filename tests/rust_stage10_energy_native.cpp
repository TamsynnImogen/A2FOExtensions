// Stage 10 full-source EnergySystems fixture. Reuse every Stage 8 mode, then
// exercise the remaining local numeric and UI helpers in the same translation
// unit as the frozen/current module source.
#define main stage8_energy_main
#include "rust_stage8_a_energy_native.cpp"
#undef main

#include <limits>

namespace {
std::uint16_t x87_control() {
    std::uint16_t value;
    __asm__ __volatile__("fnstcw %0" : "=m"(value));
    return value;
}

std::uint16_t x87_status() {
    std::uint16_t value;
    __asm__ __volatile__("fnstsw %0" : "=am"(value));
    return value;
}

float bits_float(std::uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

void residual_numeric(const char* mode) {
    const float inputs[] = {
        -1.0f, -0.0f, 0.0f, bits_float(1), 0.00009f, 0.00011f,
        0.9999f, 1.0f, 1.00001f, 1.9999f, 2.0f,
        2147483000.0f, static_cast<float>(std::numeric_limits<int>::max()),
        std::numeric_limits<float>::quiet_NaN(), bits_float(0x7f800001u),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
    };
    const auto before = x87_control();
    const auto before_top = x87_status() & 0x3800u;
    for (unsigned index = 0; index < sizeof(inputs) / sizeof(inputs[0]); ++index) {
        __asm__ __volatile__("fnclex");
        const int result = whole_ammunition_amount(inputs[index]);
        const auto status = x87_status();
        const auto control = x87_control();
        assert(before == control && (status & 0x3800u) == before_top);
        if (index < 3 || index >= 13) assert(result == 0);
        if (index == 7) assert(result == 1);
        if (index == 10) assert(result == 2);
        std::printf("%s whole %u %08x %d cw=%04x flags=%02x\n", mode, index,
                    float_bits(inputs[index]), result, control,
                    static_cast<unsigned>(status & 0x023f));
    }
    // These public paths dispatch through age op 3/4 and amount op 9 in the
    // current source; the frozen source performs the same operations locally.
    g_runtime_ready = true;
    int craft_class = 0;
    CraftFixture craft(&craft_class, 1, {{0, 0, 0}});
    CraftPolicy policy{};
    policy.photon.maximum = 1000.0f;
    g_craft_policies[&craft_class] = policy;
    const float starts[] = {
        0.5f, 0.125f, 0.126f, 1.0f, 0.00001f, 0.99999f, 100.0f, 0.0f,
        bits_float(0x7fc00001u), bits_float(0x7f800001u),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), 0.5f, 0.5f, 0.5f,
    };
    const float elapsed[] = {
        0.125f, 0.125f, 0.125f, 0.5f, 0.00001f, 0.5f, 0.25f, 0.0f,
        0.125f, 0.125f, 0.125f, 0.125f,
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        bits_float(0x7f800001u),
    };
    for (unsigned index = 0; index < sizeof(starts) / sizeof(starts[0]); ++index) {
        g_resupply_activity[craft.get()] = {{craft.get(), starts[index]}};
        __asm__ __volatile__("fnclex");
        age_resupply_activity(craft.get(), elapsed[index]);
        const auto status = x87_status();
        const auto control = x87_control();
        const auto it = g_resupply_activity.find(craft.get());
        const bool active = it != g_resupply_activity.end();
        const float remaining = active ? it->second[0].remaining_seconds : 0.0f;
        std::printf("%s age %u %08x %08x %u %08x cw=%04x flags=%02x\n",
                    mode, index, float_bits(starts[index]),
                    float_bits(elapsed[index]), static_cast<unsigned>(active),
                    float_bits(remaining), control,
                    static_cast<unsigned>(status & 0x023f));
        assert(control == before && (status & 0x3800u) == before_top);
    }
    const float colours[][3] = {
        {0.25f, 0.5f, 0.75f},
        {bits_float(0x7fc00001u), 0.5f, 0.75f},
        {0.25f, bits_float(0x7fc00001u), 0.75f},
        {0.25f, 0.5f, bits_float(0x7fc00001u)},
        {bits_float(0x7f800001u), 0.5f, 0.75f},
        {0.25f, bits_float(0x7f800001u), 0.75f},
        {0.25f, 0.5f, bits_float(0x7f800001u)},
    };
    std::array<std::uint8_t, 0x100> text{};
    for (unsigned index = 0; index < 7; ++index) {
        put(text.data(), 0x70, Colour{colours[index][0], colours[index][1],
                                      colours[index][2]});
        __asm__ __volatile__("fnclex");
        const Colour value = text_component_colour(text.data());
        const auto status = x87_status();
        const auto control = x87_control();
        std::printf("%s colour %u %08x %08x %08x cw=%04x flags=%02x\n",
                    mode, index, float_bits(value.red), float_bits(value.green),
                    float_bits(value.blue), control,
                    static_cast<unsigned>(status & 0x023f));
        assert(control == before && (status & 0x3800u) == before_top);
        assert(value.red == (index == 0 ? 0.25f : 1.0f));
    }
    const float bases[] = {
        0.0f, 0.5f, 1.0f, 1.00001f, 10.0f, 999.9f, 1000.0f, 0.00001f,
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
    };
    const float additions[] = {
        0.5f, 0.125f, -0.125f, 0.00001f, -10.0f, 0.2f, 1.0f, -0.00001f,
        bits_float(0x7fc00001u), bits_float(0x7f800001u),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(), bits_float(1),
        std::numeric_limits<float>::max(),
        -std::numeric_limits<float>::max(),
    };
    for (unsigned index = 0; index < sizeof(bases) / sizeof(bases[0]); ++index) {
        g_craft_stores[craft.get()].photon = bases[index];
        __asm__ __volatile__("fnclex");
        A2FOEnergySystems_AddPhotonTorpedoes(craft.get(), additions[index]);
        const auto status = x87_status();
        const auto control = x87_control();
        std::printf("%s add %u %08x %08x %08x cw=%04x flags=%02x\n",
                    mode, index, float_bits(bases[index]),
                    float_bits(additions[index]),
                    float_bits(g_craft_stores[craft.get()].photon), control,
                    static_cast<unsigned>(status & 0x023f));
        assert(control == before && (status & 0x3800u) == before_top);
    }
    assert(x87_control() == before);
    std::printf("completed:residual-numeric:%s\n", mode);
}

void residual_ui() {
    const NativeRectangle rectangles[] = {
        {10, 20, 110, 40}, {10, 20, 10, 40},
        {10, 20, 110, 20}, {10, 20, 9, 19},
    };
    for (unsigned index = 0; index < 4; ++index) {
        const bool usable = usable_native_rectangle(rectangles[index]);
        assert(usable == (index == 0));
        std::printf("ui usable %u %u\n", index, static_cast<unsigned>(usable));
    }
    const RawRectangle configured_anchor{3, 5, 100, 20};
    const RawRectangle target{13, 35, 75, 15};
    const auto translated = translated_rectangle(rectangles[0], configured_anchor, target);
    std::printf("ui translated %d %d %d %d\n", translated.left,
                translated.top, translated.right, translated.bottom);
    assert(translated.left == 20 && translated.top == 50);
    assert(translated.right == 95 && translated.bottom == 65);
    std::array<std::uint8_t, 0x100> text{};
    put(text.data(), 0x70, Colour{0.25f, 0.5f, 0.75f});
    auto colour = text_component_colour(text.data());
    std::printf("ui colour-finite %08x %08x %08x\n", float_bits(colour.red),
                float_bits(colour.green), float_bits(colour.blue));
    assert(colour.red == 0.25f && colour.green == 0.5f && colour.blue == 0.75f);
    put(text.data(), 0x70, Colour{std::numeric_limits<float>::quiet_NaN(),
                                  0.5f, 0.75f});
    colour = text_component_colour(text.data());
    std::printf("ui colour-fallback %08x %08x %08x\n", float_bits(colour.red),
                float_bits(colour.green), float_bits(colour.blue));
    assert(colour.red == 1 && colour.green == 1 && colour.blue == 1);
    g_runtime_ready = true;
    int quoted_class = 0;
    load_weapon(&quoted_class, {{"classLabel", "\"Re\"supplyWeapon\""}});
    const bool quoted = g_resupply_weapon_policies.count(&quoted_class) == 1;
    std::printf("ui quoted-label %u\n", static_cast<unsigned>(quoted));
    assert(quoted);
    std::printf("completed:residual-ui\n");
}

void residual_ui_default() {
    fixture_mode = "ui-auto";
    run_ui();
    unsigned drawn = 0;
    for (const auto& line : ordered_trace) {
        std::printf("%s\n", line.c_str());
        if (line.rfind("draw:", 0) == 0) {
            const auto suffix = line.substr(line.rfind(':') + 1);
            const int top = std::stoi(suffix);
            assert(top == 76 + static_cast<int>(drawn) * 28);
            ++drawn;
        }
    }
    assert(drawn == 3);
    std::printf("completed:residual-ui-default\n");
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    const std::string mode = argv[1];
    std::printf("ready:%s\n", mode.c_str());
    std::fflush(stdout);
    if (mode == "residual-ui") {
        residual_ui();
        return 0;
    }
    if (mode == "residual-ui-default") {
        residual_ui_default();
        return 0;
    }
    if (mode.rfind("residual-numeric-", 0) == 0) {
        const auto precision = mode.substr(17, 4);
        const auto rounding = mode.substr(22);
        unsigned pc = precision == "pc24" ? _PC_24 :
                      precision == "pc53" ? _PC_53 : _PC_64;
        unsigned rc = rounding == "near" ? _RC_NEAR :
                      rounding == "down" ? _RC_DOWN :
                      rounding == "up" ? _RC_UP : _RC_CHOP;
        assert((precision == "pc24" || precision == "pc53" || precision == "pc64") &&
               (rounding == "near" || rounding == "down" ||
                rounding == "up" || rounding == "chop"));
        _controlfp(pc | rc, _MCW_PC | _MCW_RC);
        residual_numeric(mode.c_str());
        return 0;
    }
    const int result = stage8_energy_main(argc, argv);
    if (result == 0) std::printf("completed:inherited:%s\n", mode.c_str());
    return result;
}
