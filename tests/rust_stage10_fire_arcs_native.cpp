// Full frozen/current FireArcs module fixture with synthetic native image.
#include <windows.h>
#include <float.h>
#include <cerrno>
#include <cmath>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include "../core/builtin_modules.hpp"
#include MODULE_SOURCE

namespace {
std::vector<std::string> trace;
std::string mode;
std::uint8_t* image = nullptr;
std::uint8_t* fleet = nullptr;
std::array<std::uint8_t, 0x300> weapon{}, klass{}, target{}, icon{},
    craft{}, weapon_system{};
Matrix34 owner_matrix{{1,0,0, 0,1,0, 0,0,1, 0,0,0}};
std::unordered_map<std::string, std::string> fields;
A2FO_WeaponClassLoadedHandler class_callback = nullptr;
A2FO_WeaponTriggerHandler trigger_callback = nullptr;
unsigned hook_count = 0, native_count = 0;
bool native_result = true, enabled = true;
bool mock_colour_enabled = false;
ArcColour mock_colour{};
std::uint64_t activation = 1;
std::string fixture_root_base;
std::array<std::string, 2> fixture_roots;

void record(const std::string& value) { trace.push_back(value); }
std::uint32_t bits(float value) {
    std::uint32_t output = 0;
    std::memcpy(&output, &value, sizeof(output));
    return output;
}
void A2FO_CALL logger(const char*, const char* value) {
    record(std::string("log:") + value);
}
void* A2FO_CALL armada() {
    record("armada");
    if (mode == "init-module-throw") throw std::runtime_error("armada throw");
    return mode == "init-no-module" ? nullptr : image;
}
void* A2FO_CALL fleetops() { record("fleetops"); return fleet; }
std::uint32_t A2FO_CALL root_count() {
    record("root-count");
    return mode == "root-too-many" ? 4097 :
        mode.rfind("root-", 0) == 0 ? 2 : 0;
}
const char* A2FO_CALL root_at(std::uint32_t index) {
    record("root:" + std::to_string(index));
    if (index == 0 && mode == "root-null") return nullptr;
    if (index == 0 && mode == "root-empty-path") return "";
    return fixture_roots[index].c_str();
}
bool A2FO_CALL register_class(const char*, const char* const*, std::uint32_t,
        A2FO_WeaponClassLoadedHandler callback, void*) {
    record("register-class");
    class_callback = callback;
    if (mode == "init-class-throw") throw std::runtime_error("class throw");
    return mode != "init-class-false";
}
bool A2FO_CALL register_trigger(const char*, A2FO_WeaponTriggerHandler callback,
                                void*) {
    record("register-trigger");
    trigger_callback = callback;
    if (mode == "init-legacy-trigger-throw")
        throw std::runtime_error("trigger throw");
    return mode != "init-trigger-false";
}
bool A2FO_CALL register_masked(const char*, std::uint32_t mask,
        A2FO_WeaponTriggerHandler callback, void*) {
    record("register-masked:" + std::to_string(mask));
    trigger_callback = callback;
    if (mode == "init-trigger-throw")
        throw std::runtime_error("trigger throw");
    return mode != "init-trigger-false";
}
bool A2FO_CALL install_hook(void* site, void*, std::size_t,
        const std::uint8_t*, A2FO_InlineHook* output) {
    ++hook_count;
    record("hook:" + std::to_string(hook_count) + ":" +
           std::to_string(site == fleet + kFoShipSystemIconRenderRva));
    if (mode == "init-hook-throw" && hook_count == 2)
        throw std::runtime_error("hook throw");
    if (mode == "init-hook-first-false" && hook_count == 1) return false;
    if (mode == "init-hook-third-false" && hook_count == 2) return false;
    output->gateway = reinterpret_cast<void*>(
        hook_count == 1 ? 0x10001 : 0x10002);
    return true;
}
bool A2FO_CALL patch_jump(void*, void*, const std::uint8_t*, std::size_t) {
    record("patch-jump"); return true;
}
template<std::size_t N>
void signature(std::uint8_t* base, std::uintptr_t rva,
               const std::uint8_t (&expected)[N]) {
    std::memcpy(base + rva, expected, N);
}
void seed_image() {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x330000,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!image || !fleet) throw std::runtime_error("VirtualAlloc");
    signature(image, kWeaponCanFireAtRva, kExpectedWeaponCanFireAt);
    signature(image, kEntityGetWorldTransformRva, kExpectedEntityGetWorldTransform);
    signature(image, kDisplayInterfaceDrawLineRva, kExpectedDisplayInterfaceDrawLine);
    signature(image, kStandardComponentIsMouseOverRva, kExpectedStandardComponentIsMouseOver);
    signature(image, kWeaponGetTargetRva, kExpectedWeaponGetTarget);
    signature(image, kParameterDbGetColorRva, kExpectedParameterDbGetColor);
    signature(fleet, kFoShipSystemIconRenderRva, kExpectedFoShipSystemIconRender);
    void* render = image + kSystemIconRenderRva;
    std::memcpy(image + kSystemIconVtableRva + kSystemIconRenderVtableOffset,
                &render, sizeof(render));
    if (mode == "init-signature") image[kWeaponGetTargetRva] ^= 1;
}
void write_roots() {
    CreateDirectoryA(fixture_root_base.c_str(), nullptr);
    for (const auto& root : fixture_roots) CreateDirectoryA(root.c_str(), nullptr);
    if (mode == "root-overlay" || mode == "root-disabled" ||
        mode == "root-invalid" || mode == "root-empty" ||
        mode == "root-exact-size" || mode == "root-oversize") {
        std::ofstream first{fixture_roots[0] + "\\RTS_CFG.h",
                            std::ios::binary};
        first << (mode == "root-invalid" ? "firearc = bad\n" :
                  mode == "root-empty" ? "" : "firearc = 0\n");
        const std::string second_path = fixture_roots[1] + "\\RTS_CFG.h";
        std::ofstream second{second_path, std::ios::binary};
        if (mode == "root-exact-size" || mode == "root-oversize") {
            second << "firearc = 1\n";
            second << std::string(2 * 1024 * 1024 - 12 +
                (mode == "root-oversize"), ' ');
            second.close();
            std::ifstream verify{second_path, std::ios::binary | std::ios::ate};
            const auto expected = static_cast<std::streamoff>(
                2 * 1024 * 1024 + (mode == "root-oversize"));
            if (!verify || verify.tellg() != expected)
                throw std::runtime_error("fixture root size mismatch");
        } else {
            second << "firearc = " << (mode == "root-overlay" ? "1" : "0") << "\n";
        }
    }
}
A2FO_ModuleApi make_api() {
    A2FO_ModuleApi api{};
    api.struct_size = mode == "init-short" ? 0 : sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.capabilities = A2FO_CAP_WEAPON_CLASS_LOADED |
                       A2FO_CAP_WEAPON_TRIGGER_EVENTS;
    api.log = logger;
    api.armada_module = armada;
    api.fleetops_module = fleetops;
    api.extension_root_count = root_count;
    api.extension_root = root_at;
    api.register_weapon_class_loaded_handler = register_class;
    api.register_weapon_trigger_handler = register_trigger;
    api.register_weapon_trigger_handler_masked = register_masked;
    api.install_inline_hook = install_hook;
    api.patch_jump = patch_jump;
    if (mode == "init-events-missing") api.capabilities = 0;
    if (mode == "init-legacy-trigger" ||
        mode == "init-legacy-trigger-throw") api.struct_size =
        offsetof(A2FO_ModuleApi, register_weapon_trigger_handler_masked);
    return api;
}
void dump_state() {
    record("ready:" + std::to_string(g_runtime_ready));
    record("hooks:" + std::to_string(hook_count));
    record("classes:" + std::to_string(g_class_arcs.size()));
    record("class-callback:" + std::to_string(class_callback != nullptr));
    record("trigger-callback:" + std::to_string(trigger_callback != nullptr));
}
void* parameter_db = reinterpret_cast<void*>(0x1133);
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void text_case() {
    const std::string input = mode == "text-high" ?
        std::string(" \t\xA0" "Ab\xFF" "Z \n") :
        mode == "text-nul" ? std::string(" \tAB\0CD \n", 9) :
        std::string(" \tMiXeD \r\n");
    std::string value = input;
    trim_string(&value);
    lower_string(&value);
    std::string hex;
    char byte[3]{};
    for (unsigned char character : value) {
        std::snprintf(byte, sizeof(byte), "%02x", character);
        hex += byte;
    }
    record("text:" + hex);
    if (mode == "text-normal") require(value == "mixed", "text trim/lower");
    if (mode == "text-nul")
        require(value == std::string("ab\0cd", 5), "text embedded NUL");
    trim_string(nullptr);
    lower_string(nullptr);
}
void float_case() {
    static const char* const values[] = {
        "", "0", "-0", "1f", "1F ", "1e3", "1e-999", "1e999",
        "NaN", "INF", "0x1p-149", "1ff", "  +2.5f\t", "1e+",
        "3.4028235e38", "1e-45", "-1e-45", "2.5 trailing"};
    for (const char* value : values) {
        float output = -17.0f;
        errno = 81;
        const bool valid = parse_float(value, &output);
        if (std::strcmp(value, "0") == 0)
            require(valid && bits(output) == 0, "float zero");
        if (std::strcmp(value, "-0") == 0)
            require(valid && bits(output) == 0x80000000, "float negative zero");
        if (std::strcmp(value, "1f") == 0)
            require(valid && bits(output) == 0x3f800000, "float suffix");
        if (std::strcmp(value, "NaN") == 0 ||
            std::strcmp(value, "INF") == 0 ||
            std::strcmp(value, "1ff") == 0 ||
            std::strcmp(value, "") == 0)
            require(!valid && bits(output) == bits(-17.0f), "float output guard");
        record(std::string("float:") + value + ":" +
               std::to_string(valid) + ":" + std::to_string(bits(output)) +
               ":" + std::to_string(errno));
    }
    require(!parse_float("1", nullptr), "float null output");
    record("float-null:0");
}
void query_case() {
    std::string output = "uncleared";
    bool present = true;
    fields.clear();
    if (mode == "query-empty") fields["test"] = "";
    if (mode == "query-trim") fields["test"] = " \tValue\r\n";
    if (mode == "query-259") fields["test"] = std::string(259, 'q');
    if (mode == "query-260") fields["test"] = std::string(260, 'q');
    if (mode == "query-nul") fields["test"] = std::string("AB\0CD", 5);
    const bool result = query_parameter_string(parameter_db, "test", &output,
                                               &present);
    require(result, "query call");
    if (mode == "query-missing")
        require(!present && output.empty(), "query missing clear");
    if (mode == "query-empty")
        require(present && output.empty(), "query present empty");
    if (mode == "query-trim")
        require(present && output == "Value", "query trim");
    if (mode == "query-259" || mode == "query-260")
        require(present && output == std::string(259, 'q'), "query capacity");
    if (mode == "query-nul")
        require(present && output == "AB", "query embedded NUL");
    record("query:" + std::to_string(result) + ":" +
           std::to_string(present) + ":" +
           std::to_string(output.size()) + ":" +
           std::to_string(output.empty() ? 0 :
               static_cast<unsigned char>(output.front())));
    output = "uncleared";
    present = true;
    const bool invalid = query_parameter_string(nullptr, "test", &output,
                                                &present);
    require(!invalid && !present && output.empty(), "query invalid clear");
    record("query-invalid:" + std::to_string(invalid) + ":" +
        std::to_string(present) + ":" + std::to_string(output.size()));
}
void vector_case() {
    void* system = weapon_system.data();
    std::memcpy(craft.data() + kWeaponSystemOnCraftOffset, &system,
                sizeof(system));
    static void* entries[257]{};
    entries[0] = weapon.data();
    void** begin = entries;
    void** end = entries + 1;
    if (mode == "vector-empty") end = begin;
    if (mode == "vector-reversed") { begin = entries + 1; end = entries; }
    if (mode == "vector-misaligned")
        end = reinterpret_cast<void**>(reinterpret_cast<char*>(entries) + 5);
    if (mode == "vector-256") end = entries + 256;
    if (mode == "vector-257") end = entries + 257;
    if (mode == "vector-unreadable") {
        begin = reinterpret_cast<void**>(0x10001);
        end = begin + 1;
    }
    std::memcpy(weapon_system.data() + kWeaponVectorBeginOffset, &begin,
                sizeof(begin));
    std::memcpy(weapon_system.data() + kWeaponVectorEndOffset, &end,
                sizeof(end));
    void** found = reinterpret_cast<void**>(0x1111);
    std::size_t count = 999;
    bool valid = weapon_vector_for_craft(craft.data(), &found, &count);
    if (mode == "vector-256")
        require(valid && found == begin && count == 256, "vector 256");
    else require(!valid && found == nullptr && count == 0,
                 "vector invalid output guard");
    record("vector:" + std::to_string(valid) + ":" +
           std::to_string(found == begin) + ":" + std::to_string(count));
}
struct FpState {
    unsigned short cw{}, sw{};
    std::uint32_t mxcsr{};
    int error{};
};
FpState fp_state() {
    FpState result{};
    __asm__ __volatile__("fnstcw %0" : "=m"(result.cw));
    __asm__ __volatile__("fnstsw %0" : "=m"(result.sw));
    __asm__ __volatile__("stmxcsr %0" : "=m"(result.mxcsr));
    result.error = errno;
    return result;
}
void clear_fp_flags() {
    __asm__ __volatile__("fnclex");
    std::uint32_t mxcsr{};
    __asm__ __volatile__("stmxcsr %0" : "=m"(mxcsr));
    mxcsr &= ~std::uint32_t{0x3f};
    __asm__ __volatile__("ldmxcsr %0" : : "m"(mxcsr));
}
std::string fp_text(const FpState& state) {
    return std::to_string(state.cw) + ":" + std::to_string(state.sw) +
        ":" + std::to_string((state.sw >> 11) & 7) + ":" +
        std::to_string(state.mxcsr) + ":" + std::to_string(state.error);
}
void numeric_case() {
    if (mode.find("pc24") != std::string::npos) _controlfp(_PC_24, _MCW_PC);
    if (mode.find("pc53") != std::string::npos) _controlfp(_PC_53, _MCW_PC);
    if (mode.find("pc64") != std::string::npos) _controlfp(_PC_64, _MCW_PC);
    if (mode.find("rcdown") != std::string::npos) _controlfp(_RC_DOWN, _MCW_RC);
    if (mode.find("rcup") != std::string::npos) _controlfp(_RC_UP, _MCW_RC);
    if (mode.find("rcchop") != std::string::npos) _controlfp(_RC_CHOP, _MCW_RC);
    const std::uint32_t words[] = {
        0x00000000, 0x80000000, 0x00000001, 0x80000001,
        0x42fffffe, 0x43000000, 0x43649248, 0x43649249, 0x4364924a,
        0x44160000, 0x44b29248, 0x44b29249, 0x44b2924a, 0x44800000,
        0x7f7fffff, 0x7f800000, 0xff800000, 0x7f800001,
        0xff800001, 0x7fc00001, 0xffc00001, 0x3f800000, 0x3eaaaaab};
    g_ui_colour_reader_ready = true;
    mock_colour_enabled = true;
    for (std::uint32_t word : words) {
        float value{};
        std::memcpy(&value, &word, sizeof(value));
        std::memcpy(klass.data() + kRangeOnWeaponClassOffset, &value,
                    sizeof(value));
        for (unsigned channel = 0; channel != 3; ++channel) {
            mock_colour = ArcColour{{0.5f, 0.5f, 0.5f}};
            std::memcpy(&mock_colour.values[channel], &word, sizeof(word));
            ArcColour output{};
            clear_fp_flags();
            errno = 91;
            const FpState radius_before = fp_state();
            const float radius = visualization_radius(klass.data());
            const FpState radius_after = fp_state();
            clear_fp_flags();
            errno = 92;
            const FpState colour_before = fp_state();
            const bool valid = read_ui_colour(parameter_db,
                                              "fireArcBoundaryColor", &output);
            const FpState colour_after = fp_state();
            if (word == 0x00000000 || word == 0x80000000 ||
                word == 0x7f800000 || word == 0xff800000)
                require(bits(radius) == bits(120.0f), "radius fallback");
            if (word == 0x7f800000 || word == 0xff800000 ||
                word == 0x7f800001 || word == 0xff800001 ||
                word == 0x7fc00001 || word == 0xffc00001)
                require(!valid, "nonfinite colour guard");
            if (word == 0x3f800000)
                require(valid && bits(output.values[channel]) == word,
                        "colour one");
            record("numeric:" + std::to_string(word) + ":" +
                std::to_string(channel) + ":" + std::to_string(bits(radius)) +
                ":" + std::to_string(valid) + ":" +
                std::to_string(bits(output.values[0])) + ":" +
                std::to_string(bits(output.values[1])) + ":" +
                std::to_string(bits(output.values[2])) + ":" +
                fp_text(radius_before) + ":" + fp_text(radius_after) +
                ":" + fp_text(colour_before) + ":" + fp_text(colour_after));
        }
    }
}
void class_case() {
    fields = {{"wpnName", "Test Weapon"}, {"classLabel", "CannonImp"}};
    if (mode == "class-box") {
        fields["fireArcCenter"] = "15";
        fields["fireArcWidth"] = "60";
        fields["fireArcPitchAngle"] = "80";
    } else if (mode == "class-alias") {
        fields["fireArcYaw"] = "45";
        fields["fireArcYawAngle"] = "20";
        fields["fireArcMode"] = " BOX ";
    } else if (mode == "class-invalid") {
        fields["fireArcAngle"] = "bad";
    } else if (mode == "class-mode-invalid") {
        fields["fireArcAngle"] = "45";
        fields["fireArcMode"] = "strange";
    } else if (mode == "class-no-coverage") {
        fields["fireArcCenter"] = "25";
    } else if (mode == "class-box-angle") {
        fields["fireArcMode"] = "box";
        fields["fireArcWidth"] = "55";
        fields["fireArcAngle"] = "20";
    } else if (mode == "class-cone-width") {
        fields["fireArcMode"] = "cone";
        fields["fireArcAngle"] = "70";
        fields["fireArcWidth"] = "25";
    } else if (mode.rfind("class-pc", 0) == 0) {
        fields["fireArcCenter"] = "359.99997";
        fields["fireArcWidth"] = "180.000015";
        fields["fireArcPitch"] = "89.99999";
    } else {
        fields["fireArcAngle"] = "60";
    }
    A2FO_WeaponClassLoadedEvent event{};
    event.struct_size = sizeof(event);
    event.weapon_class = klass.data();
    event.parameter_db = parameter_db;
    class_callback(&event, nullptr);
    auto found = g_class_arcs.find(klass.data());
    record("stored:" + std::to_string(found != g_class_arcs.end()));
    if (found != g_class_arcs.end()) {
        const ArcConfig& arc = found->second.arc;
        record("arc:" + std::to_string(static_cast<int>(arc.mode)) + ":" +
            std::to_string(bits(arc.yaw_degrees)) + ":" +
            std::to_string(bits(arc.pitch_degrees)) + ":" +
            std::to_string(bits(arc.yaw_angle_degrees)) + ":" +
            std::to_string(bits(arc.pitch_angle_degrees)) + ":" +
            std::to_string(bits(arc.cone_angle_degrees)));
        record("cannon:" + std::to_string(found->second.cannon_imp));
    }
}
void trigger_case() {
    fields = {{"fireArcAngle", "60"}, {"classLabel", "CannonImp"}};
    A2FO_WeaponClassLoadedEvent loaded{};
    loaded.struct_size = sizeof(loaded);
    loaded.weapon_class = klass.data();
    loaded.parameter_db = parameter_db;
    class_callback(&loaded, nullptr);
    void* class_pointer = klass.data();
    std::memcpy(weapon.data() + kWeaponClassOnWeaponOffset,
                &class_pointer, sizeof(class_pointer));
    float position[3]{0, 0, mode == "trigger-reject" ? -10.0f : 10.0f};
    std::memcpy(target.data() + kPositionOnGameObjectOffset,
                position, sizeof(position));
    if (mode == "trigger-auto")
        klass[kUsePrimaryTargetOnCannonImpClassOffset] = 0;
    else klass[kUsePrimaryTargetOnCannonImpClassOffset] = 1;
    void* event_target = mode == "trigger-null-target" ? nullptr : target.data();
    A2FO_WeaponTriggerEvent event{sizeof(event), A2FO_WEAPON_TRIGGER_PRECHECK,
                                   weapon.data(), event_target};
    if (mode == "trigger-short") event.struct_size = 0;
    if (mode == "trigger-committed") event.kind = A2FO_WEAPON_TRIGGER_COMMITTED;
    if (mode == "trigger-disabled") enabled = false;
    record("trigger:" + std::to_string(trigger_callback(&event, nullptr)));
    record("allow:" + std::to_string(A2FOFireArcs_AllowWeaponTrigger(
        weapon.data(), event_target)));
}
void prepare_weapon() {
    fields = {{"fireArcAngle", "60"}, {"wpnName", "Visible Weapon"}};
    A2FO_WeaponClassLoadedEvent loaded{};
    loaded.struct_size = sizeof(loaded);
    loaded.weapon_class = klass.data();
    loaded.parameter_db = parameter_db;
    class_callback(&loaded, nullptr);
    void* class_pointer = klass.data();
    std::memcpy(weapon.data() + kWeaponClassOnWeaponOffset,
                &class_pointer, sizeof(class_pointer));
    float position[3]{0, 0,
        mode == "canfire-reject" ? -10.0f : 10.0f};
    std::memcpy(target.data() + kPositionOnGameObjectOffset,
                position, sizeof(position));
    klass[kRestrictFireArcOnWeaponClassOffset] = 1;
}
void canfire_case() {
    prepare_weapon();
    if (mode == "canfire-native-false") native_result = false;
    if (mode == "canfire-no-policy") g_class_arcs.clear();
    if (mode == "canfire-disabled") enabled = false;
    const bool result = weapon_can_fire_at_hook(
        weapon.data(), nullptr, craft.data(), target.data(), icon.data());
    record("can-fire:" + std::to_string(result));
    record("native-count:" + std::to_string(native_count));
    record("restriction:" + std::to_string(
        klass[kRestrictFireArcOnWeaponClassOffset]));
}
void __cdecl mock_draw_line(const float* start, const float* end,
                            const float* colour) {
    record("draw:" + std::to_string(bits(start[2])) + ":" +
           std::to_string(bits(end[2])) + ":" +
           std::to_string(bits(colour[0])));
}
void ui_case() {
    prepare_weapon();
    const std::uint8_t jump[]{0xb8, 0, 0, 0, 0, 0xff, 0xe0};
    std::memcpy(image + kDisplayInterfaceDrawLineRva, jump, sizeof(jump));
    void* draw = reinterpret_cast<void*>(&mock_draw_line);
    std::memcpy(image + kDisplayInterfaceDrawLineRva + 1,
                &draw, sizeof(draw));
    void* db = parameter_db;
    std::memcpy(image + kGuiParameterDbPointerRva, &db, sizeof(db));
    void* system_pointer = weapon_system.data();
    std::memcpy(craft.data() + kWeaponSystemOnCraftOffset,
                &system_pointer, sizeof(system_pointer));
    static void* weapons[1];
    weapons[0] = weapon.data();
    void** begin = weapons;
    void** end = weapons + 1;
    std::memcpy(weapon_system.data() + kWeaponVectorBeginOffset,
                &begin, sizeof(begin));
    std::memcpy(weapon_system.data() + kWeaponVectorEndOffset,
                &end, sizeof(end));
    void* craft_pointer = craft.data();
    std::memcpy(icon.data() + kCraftOnShipSystemIconOffset,
                &craft_pointer, sizeof(craft_pointer));
    const std::int32_t index = mode == "ui-invalid-index" ? -1 : 0;
    std::memcpy(icon.data() + kWeaponIndexOnShipSystemIconOffset,
                &index, sizeof(index));
    const std::int32_t system_index = kWeaponsSystemIndex;
    std::memcpy(icon.data() + kSystemIndexOnSystemIconOffset,
                &system_index, sizeof(system_index));
    if (mode == "ui-disabled") enabled = false;
    if (mode == "ui-system")
        system_icon_render_hook(icon.data(), nullptr);
    else
        ship_system_icon_render_hook(icon.data(), nullptr);
    record("ui-loaded:" + std::to_string(g_ui_colours_loaded));
}
void run() {
    if (mode.find("pc24") != std::string::npos)
        _controlfp(_PC_24, _MCW_PC);
    else if (mode.find("pc53") != std::string::npos)
        _controlfp(_PC_53, _MCW_PC);
    else if (mode.find("pc64") != std::string::npos)
        _controlfp(_PC_64, _MCW_PC);
    seed_image();
    write_roots();
    A2FO_ModuleApi api = make_api();
    try {
        record("init:" + std::to_string(A2FO_ModuleInit(&api)));
    } catch (const std::exception& error) {
        record(std::string("throw:") + error.what());
    }
    dump_state();
    if (mode == "root-overlay" || mode == "root-exact-size" ||
        mode == "root-missing" || mode == "root-null" ||
        mode == "root-empty-path")
        require(g_runtime_ready, "root should enable FireArcs");
    if (mode == "root-disabled" || mode == "root-invalid" ||
        mode == "root-empty" || mode == "root-oversize")
        require(!g_runtime_ready, "root should disable FireArcs");
    const bool active_case = mode.rfind("class-", 0) == 0 ||
        mode.rfind("trigger-", 0) == 0 ||
        mode.rfind("canfire-", 0) == 0 ||
        mode.rfind("ui-", 0) == 0 ||
        mode.rfind("text-", 0) == 0 ||
        mode.rfind("query-", 0) == 0 ||
        mode.rfind("vector-", 0) == 0 ||
        mode.rfind("numeric-", 0) == 0 || mode == "float-direct";
    if (active_case) require(g_runtime_ready, "active case did not initialize");
    if (!g_runtime_ready) return;
    if (mode.rfind("text-", 0) == 0) text_case();
    if (mode == "float-direct") float_case();
    if (mode.rfind("query-", 0) == 0) query_case();
    if (mode.rfind("vector-", 0) == 0) vector_case();
    if (mode.rfind("numeric-", 0) == 0) numeric_case();
    if (mode.rfind("class-", 0) == 0) class_case();
    if (mode.rfind("trigger-", 0) == 0) trigger_case();
    if (mode.rfind("canfire-", 0) == 0) canfire_case();
    if (mode.rfind("ui-", 0) == 0) ui_case();
    if (active_case) record("completed:" + mode);
}
}

namespace a2fo {
bool builtin_module_enabled(BuiltinFeature) noexcept { return enabled; }
std::uint64_t builtin_module_activation(BuiltinFeature) noexcept {
    return enabled ? activation : 0;
}
}

extern "C" std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_0(
        void* function, void* self) {
    if (function == image + kWeaponGetOwnerRva) {
        record("native-owner");
        return mode == "trigger-owner-missing" ? 0 :
            reinterpret_cast<std::uintptr_t>(craft.data());
    }
    if (function == image + kEntityGetTransformRva) {
        record("native-transform");
        if (mode == "trigger-transform-missing") return 0;
        if (mode == "trigger-transform-mutate") {
            float position[3]{0, 0, -10.0f};
            std::memcpy(target.data() + kPositionOnGameObjectOffset,
                        position, sizeof(position));
        }
        return reinterpret_cast<std::uintptr_t>(&owner_matrix);
    }
    if (function == image + kWeaponGetTargetRva) {
        record("native-target"); return reinterpret_cast<std::uintptr_t>(target.data());
    }
    if (function == image + kStandardComponentIsMouseOverRva) {
        record("native-hover"); return 1;
    }
    if (function == reinterpret_cast<void*>(0x10001) ||
        function == g_system_icon_render_original) {
        record("native-render:" + std::to_string(self == icon.data()));
        return 1;
    }
    record("native-0-unknown"); return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_2(
        void*, void*, std::uintptr_t, std::uintptr_t) { return 0; }
extern "C" std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_3(
        void* function, void* self, std::uintptr_t one, std::uintptr_t two,
        std::uintptr_t) {
    if (function == image + kParameterDbGetFloatRva) {
        const char* key = reinterpret_cast<const char*>(one);
        record(std::string("float:") + key);
        return 0;
    }
    if (function == image + kParameterDbGetColorRva) {
        record(std::string("color:") + reinterpret_cast<const char*>(one));
        if (mock_colour_enabled) {
            std::memcpy(reinterpret_cast<void*>(two), &mock_colour,
                        sizeof(mock_colour));
            return 1;
        }
        return 0;
    }
    if (function == reinterpret_cast<void*>(0x10002)) {
        ++native_count;
        record("native-can-fire:" + std::to_string(self == weapon.data()) +
               ":" + std::to_string(
                   klass[kRestrictFireArcOnWeaponClassOffset]));
        if (mode == "canfire-mutate") ++activation;
        return native_result;
    }
    (void)two;
    record("native-3-unknown"); return 0;
}
extern "C" std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_4(
        void* function, void*, std::uintptr_t key_address,
        std::uintptr_t value_address, std::uintptr_t capacity,
        std::uintptr_t) {
    if (function != image + kParameterDbGetStringRva) return 0;
    const std::string key = reinterpret_cast<const char*>(key_address);
    record("string:" + key);
    auto found = fields.find(key);
    if (found == fields.end()) return 0;
    std::snprintf(reinterpret_cast<char*>(value_address), capacity,
                  "%s", found->second.c_str());
    return 1;
}

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    if (argc != 2) return 2;
    const char* root = std::getenv("A2FO_FIRE_ARCS_ROOT_DIR");
    if (!root || !*root) return 3;
    fixture_root_base = root;
    fixture_roots = {fixture_root_base + "\\root0",
                     fixture_root_base + "\\root1"};
    mode = argv[1];
    std::set_terminate([] {
        for (const auto& line : trace) std::puts(line.c_str());
        try {
            auto exception = std::current_exception();
            if (exception) std::rethrow_exception(exception);
            std::puts("terminate:none");
        } catch (const std::exception& error) {
            std::printf("terminate:%s\n", error.what());
        } catch (...) {
            std::puts("terminate:other");
        }
        std::fflush(stdout);
        std::_Exit(mode == "init-hook-throw" ? 0 : 88);
    });
    try {
        run();
        for (const auto& line : trace) std::puts(line.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "fixture: %s\n", error.what());
        return 1;
    }
}
