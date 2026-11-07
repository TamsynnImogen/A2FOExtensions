// Stage-ten extension of the original full-source native module fixture.
// The included fixture retains all stage-eight module integration scenarios.
#define main stage8_nebula_main
#include "rust_stage8_c_nebula_native.cpp"
#undef main
#include <cerrno>
#include <cfenv>
#include <limits>

static std::uint32_t bits(float value) {
    std::uint32_t result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
static float from_bits(std::uint32_t value) {
    float result;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}
static void hex_text(const char* tag, const std::string& value) {
    std::printf("%s:%u:", tag, static_cast<unsigned>(value.size()));
    for (unsigned char c : value) std::printf("%02x", c);
    std::putchar('\n');
}
static std::string g_query_value;
static bool g_query_found = false;
static std::uint32_t g_float_value = 0;
static bool g_float_found = false;
static std::string g_colour_text;
static std::uint32_t g_colour_packed = 0;
static std::uint32_t g_colour_used = 0;
static std::uint32_t g_colour_calls = 0;
static std::uintptr_t __attribute__((thiscall)) mock_query_string(
    void*, const char*, char* output, std::uintptr_t capacity, const char*) {
    if (!g_query_found || g_query_value.size() >= capacity) return 0;
    std::memcpy(output, g_query_value.data(), g_query_value.size());
    output[g_query_value.size()] = 0;
    return 1;
}
static std::uintptr_t __attribute__((thiscall)) mock_query_float(
    void*, const char*, float* output, std::uint32_t) {
    if (!g_float_found) return 0;
    std::memcpy(output, &g_float_value, sizeof(g_float_value));
    return 1;
}
static std::uintptr_t __attribute__((thiscall)) mock_colour_string(
    void*, const char* command, char* output, std::uintptr_t capacity,
    const char*) {
    const char* value = nullptr;
    if (std::strcmp(command, "logoDecal1Hardpoint") == 0) value = "hP";
    if (std::strcmp(command, "logoDecal1ColourKey") == 0)
        value = g_colour_text.c_str();
    if (!value || std::strlen(value) >= capacity) return 0;
    std::strcpy(output, value);
    return 1;
}
static int __cdecl mock_colour_register(
    void*, const LogoDecalDescriptor* descriptors, std::uint32_t count) {
    g_colour_calls = count;
    if (count) {
        g_colour_packed = descriptors[0].colour_key;
        g_colour_used = descriptors[0].use_colour_key;
    }
    return 1;
}
static void text_edges() {
    g_race_texture_suffixes.clear();
    g_race_texture_suffixes[reinterpret_cast<void*>(1)] = "_a_b";
    g_race_texture_suffixes[reinterpret_cast<void*>(2)] = "_b";
    g_race_texture_suffixes[reinterpret_cast<void*>(3)] = "_B";
    for (const char* name : {"ship_a_b.dds", "ship_b.dds",
                             "ship_B.dds", "ship_a_B.dds"}) {
        const FactionDiffuseName split = split_faction_diffuse_name(name);
        std::printf("suffix-priority:%s:%s:%s\n", name,
                    split.base_diffuse.c_str(), split.faction_suffix.c_str());
    }
    g_race_texture_suffixes.clear();
    for (const std::string& value : {std::string(), std::string(" \t\r\n"),
             std::string(" \t/a/b \r"), std::string("\x80 /\xff ", 5),
             std::string(" x\0/y ", 7)}) {
        hex_text("trim", trim(value));
    }
    for (const auto& pair : {std::pair<const char*, const char*>{"", ""},
             {"", "b"}, {"a", ""}, {"a", "b"}, {"a/", "b"},
             {"a\\", "b"}, {"a", "/b"}}) {
        hex_text("join", join_path(pair.first, pair.second));
    }
    std::string output = "sentinel";
    for (const std::string& input : {std::string(), std::string(" \t"),
             std::string(" a/b "), std::string(1023, 'x'),
             std::string(1024, 'x')}) {
        std::string native = input;
        native.push_back('\0');
        output = "sentinel";
        const bool ok = bounded_texture_name(native.c_str(), &output);
        std::printf("bounded:%u:%u:%u:%u\n", static_cast<unsigned>(input.size()),
                    ok, static_cast<unsigned>(output.size()),
                    output == "sentinel");
    }
    output = "sentinel";
    std::printf("bounded-null:%u:%s\n", bounded_texture_name(nullptr, &output),
                output.c_str());
    void* unreadable = VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE,
                                    PAGE_NOACCESS);
    if (!unreadable) std::abort();
    output = "sentinel";
    const bool unreadable_ok = bounded_texture_name(
        static_cast<const char*>(unreadable), &output);
    std::printf("bounded-unreadable:%u:%u\n", unreadable_ok,
                output == "sentinel");
    VirtualFree(unreadable, 0, MEM_RELEASE);
    MockNode node{};
    for (const std::string& input : {std::string(), std::string("HpX"),
             std::string(127, 'A'), std::string(128, 'A'),
             std::string("H\x80P", 3)}) {
        node.name = input.c_str();
        output = "sentinel";
        const bool ok = normalized_node_name(&node, &output);
        std::printf("node:%u:%u:%u:%u\n", static_cast<unsigned>(input.size()),
                    ok, static_cast<unsigned>(output.size()),
                    output == "sentinel");
        if (input == "HpX") hex_text("node-fold", output);
    }
    for (const std::string& input : {std::string("1,2,3"),
             std::string(" 1 , 2 ,3tail"), std::string("1,"),
             std::string("1,nan"), std::string("1,inf"),
             std::string("1,1e999"), std::string("-0,1e-45")}) {
        float values[3]{from_bits(0x3f123456), from_bits(0x3f234567),
                        from_bits(0x3f345678)};
        errno = 17;
        const bool ok = parse_float_values(input, values, 3);
        std::printf("parse:%s:%u:%08x:%08x:%08x:%d\n", input.c_str(), ok,
                    bits(values[0]), bits(values[1]), bits(values[2]), errno);
    }
    float none = from_bits(0x3f123456);
    const bool zero_ok = parse_float_values("1", &none, 0);
    std::printf("parse-zero:%u:%08x\n", zero_ok, bits(none));
    for (const auto& sample : {std::pair<bool, std::string>{false, ""},
             {true, ""}, {true, " \t "}, {true, " /foo/ "},
             {true, std::string(1023, 'x')},
             {true, std::string(1024, 'x')}}) {
        auto* image = static_cast<std::uint8_t*>(VirtualAlloc(
            nullptr, 0x00400000, MEM_COMMIT | MEM_RESERVE,
            PAGE_EXECUTE_READWRITE));
        if (!image) std::abort();
        g_armada = reinterpret_cast<HMODULE>(image);
        install_callable(image, kParameterDbGetStringRva,
                         reinterpret_cast<void*>(&mock_query_string));
        g_query_found = sample.first;
        g_query_value = sample.second;
        output = "sentinel";
        int parameter = 0;
        const bool ok = read_parameter_string(&parameter, "edge", &output);
        std::printf("query:%u:%u:%u:%u:%u\n", sample.first,
                    static_cast<unsigned>(sample.second.size()), ok,
                    static_cast<unsigned>(output.size()), output == "sentinel");
        if (sample.second == " /foo/ ") hex_text("query-text", output);
        g_armada = nullptr;
        VirtualFree(image, 0, MEM_RELEASE);
    }
    std::puts("completed:text-edges");
}
static void numeric_edges() {
    // Set x87 control word directly; preserve the caller's environment.
    unsigned short saved_cw;
    asm volatile("fnstcw %0" : "=m"(saved_cw));
    const std::uint32_t input[] = {0x00000000u, 0x80000000u,
        0x00000001u, 0x80000001u, 0x3f800000u, 0xbf800000u,
        0x7f7fffffu, 0x7f800000u, 0xff800000u, 0x7fc00001u,
        0x7f800001u};
    auto* image = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x00400000, MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE));
    if (!image) std::abort();
    g_armada = reinterpret_cast<HMODULE>(image);
    install_callable(image, kParameterDbGetFloatRva,
                     reinterpret_cast<void*>(&mock_query_float));
    int parameter = 0;
    MockNode colour_node{};
    colour_node.name = "Hp";
    MockGeometry colour_geometry{};
    colour_geometry.root = &colour_node;
    MockCraft colour_craft{};
    colour_craft.geometry = &colour_geometry;
    g_register_logo_decal_class = mock_colour_register;
    g_parameter_mode = 1;
    install_callable(image, kParameterDbGetStringVectorRva,
                     reinterpret_cast<void*>(&mock_parameter_vector));
    for (unsigned precision : {0u, 2u, 3u}) {
        for (unsigned rounding = 0; rounding < 4; ++rounding) {
            const unsigned short cw = static_cast<unsigned short>(
                (saved_cw & ~0x0f00u) | (precision << 8) | (rounding << 10));
            asm volatile("fldcw %0" : : "m"(cw));
            for (const char* colour : {"0,0,0", "255,255,255",
                                       "128.49,255.6,-1", "0.49,0.5,254.5",
                                       "0.49999997,0.50000006,254.49998",
                                       "254.50002,255.5,-0.5",
                                       "nan,1,1", "inf,1,1"}) {
                install_callable(image, kParameterDbGetStringRva,
                                 reinterpret_cast<void*>(&mock_colour_string));
                g_colour_text = colour;
                g_colour_calls = g_colour_used = g_colour_packed = 0;
                g_float_found = false;
                unsigned short sw_before, sw_after, cw_before, cw_after;
                std::uint32_t mx_before, mx_after;
                asm volatile("fnstsw %0" : "=m"(sw_before));
                asm volatile("fnstcw %0" : "=m"(cw_before));
                asm volatile("stmxcsr %0" : "=m"(mx_before));
                errno = 29;
                register_logo_decals(&colour_craft, &parameter);
                const int after_errno = errno;
                asm volatile("fnstsw %0" : "=m"(sw_after));
                asm volatile("fnstcw %0" : "=m"(cw_after));
                asm volatile("stmxcsr %0" : "=m"(mx_after));
                std::printf("colour:%u:%u:%s:%u:%u:%08x:%04x:%04x:%04x:%04x:%08x:%08x:%d\n",
                            precision, rounding, colour, g_colour_calls,
                            g_colour_used, g_colour_packed, cw_before,
                            cw_after, sw_before, sw_after, mx_before,
                            mx_after, after_errno);
            }
            for (std::uint32_t raw : input) {
                g_float_value = raw;
                g_float_found = true;
                float queried = from_bits(0x3f123456u);
                errno = 29;
                const bool float_ok = read_parameter_float(
                    &parameter, "edge", &queried);
                std::printf("float:%u:%u:%08x:%u:%08x:%d\n",
                            precision, rounding, raw, float_ok,
                            bits(queried), errno);
                float value = from_bits(raw);
                float offset[3]{value, value, value};
                float size[2]{value, value};
                DamageDecalDescriptor descriptor{};
                std::memcpy(descriptor.offset, offset, sizeof(offset));
                std::memcpy(descriptor.size, size, sizeof(size));
                unsigned short before_sw, after_sw, before_cw, after_cw;
                std::uint32_t before_mxcsr, after_mxcsr;
                asm volatile("fnstsw %0" : "=m"(before_sw));
                asm volatile("fnstcw %0" : "=m"(before_cw));
                asm volatile("stmxcsr %0" : "=m"(before_mxcsr));
                errno = 29;
                apply_model_scale(descriptor, from_bits(0x40000000u));
                const int after_errno = errno;
                asm volatile("fnstsw %0" : "=m"(after_sw));
                asm volatile("fnstcw %0" : "=m"(after_cw));
                asm volatile("stmxcsr %0" : "=m"(after_mxcsr));
                std::printf("scale:%u:%u:%08x:%08x:%08x:%08x:%08x:%08x:%04x:%04x:%04x:%04x:%08x:%08x:%d\n",
                            precision, rounding, raw, bits(descriptor.offset[0]),
                            bits(descriptor.offset[1]), bits(descriptor.offset[2]),
                            bits(descriptor.size[0]), bits(descriptor.size[1]),
                            before_cw, after_cw,
                            before_sw, after_sw, before_mxcsr, after_mxcsr,
                            after_errno);
            }
        }
    }
    g_float_found = false;
    float queried = from_bits(0x3f123456u);
    const bool missing_ok = read_parameter_float(&parameter, "edge", &queried);
    std::printf("float-missing:%u:%08x\n",
                missing_ok, bits(queried));
    g_armada = nullptr;
    VirtualFree(image, 0, MEM_RELEASE);
    asm volatile("fldcw %0" : : "m"(saved_cw));
    std::puts("completed:numeric-edges");
}
int main() {
    // The old fixture performs its own class, registration and shutdown probes.
    const int result = stage8_nebula_main();
    if (result) return result;
    // The inherited fixture's API object was stack-local to its main().
    g_api = nullptr;
    text_edges();
    numeric_edges();
    std::puts("completed:nebula-full-source");
    return 0;
}
