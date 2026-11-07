/*
 * Optional ODF-driven three-dimensional weapon firing arcs.
 *
 * Weapon ODFs which use none of the new commands retain Armada/Fleet Ops'
 * native restrictFireArc/fireArc path. A configured weapon replaces the
 * stock directional gate while retaining Armada's native range, obstruction,
 * and target-validity checks, using the weapon owner's local
 * right/up/forward axes.
 */

#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif
#include "../../sdk/include/a2fo_module_api.h"
#include "fire_arc.hpp"
#include "runtime_config.hpp"
#include "../../rust/stage8_a_fire_arcs_ffi.h"
#include "../../rust/stage10_fire_arcs_runtime_ffi.h"
#include "../../rust/stage10_fire_arcs_text_ffi.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>

extern "C" {
std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_0(
    void* function, void* self);
std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_3(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3);
std::uintptr_t __cdecl a2fo_fire_arc_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);
}

namespace {

using a2fo::fire_arcs::ArcConfig;
using a2fo::fire_arcs::ArcLine;
using a2fo::fire_arcs::ArcLineStyle;
using a2fo::fire_arcs::ArcMode;
using a2fo::fire_arcs::Matrix34;

constexpr char kModuleName[] = "A2FOFireArcs";
constexpr char kRtsConfigFileName[] = "RTS_CFG.h";
constexpr std::streamoff kMaximumRtsConfigSize = 2 * 1024 * 1024;

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kWeaponCanFireAtRva = 0x0026f8c0;
constexpr std::uintptr_t kParameterDbGetFloatRva = 0x00134df0;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x00135350;
constexpr std::uintptr_t kParameterDbGetColorRva = 0x00135ba0;
constexpr std::uintptr_t kGuiParameterDbPointerRva = 0x0036502c;
constexpr std::uintptr_t kEntityGetTransformRva = 0x000cfd50;
constexpr std::uintptr_t kEntityGetWorldTransformRva = 0x000cff90;
constexpr std::uintptr_t kWeaponGetOwnerRva = 0x00271050;
constexpr std::uintptr_t kWeaponGetTargetRva = 0x00271300;
constexpr std::uintptr_t kDisplayInterfaceDrawLineRva = 0x0011b130;
constexpr std::uintptr_t kStandardComponentIsMouseOverRva = 0x0010c140;
constexpr std::uintptr_t kSystemIconRenderRva = 0x000eec90;
constexpr std::uintptr_t kSystemIconVtableRva = 0x002b4aac;

// Fleet Operations detours both entries before extension modules load. The
// new module chains only these exact supported handlers.
constexpr std::uintptr_t kFoWeaponCanFireAtHandlerRva = 0x001358ac;
constexpr std::uintptr_t kFoShipSystemIconRenderRva = 0x001ed458;

constexpr std::size_t kWeaponClassOnWeaponOffset = 0x04;
constexpr std::size_t kUsePrimaryTargetOnCannonImpClassOffset = 0x28c;
constexpr std::size_t kHardpointListOnWeaponOffset = 0x10;
constexpr std::size_t kRestrictFireArcOnWeaponClassOffset = 0x1b7;
constexpr std::size_t kRangeOnWeaponClassOffset = 0x1c0;
constexpr std::size_t kPositionOnGameObjectOffset = 0xac;
constexpr std::size_t kWeaponSystemOnCraftOffset = 0x128;
constexpr std::size_t kWeaponVectorBeginOffset = 0x0c;
constexpr std::size_t kWeaponVectorEndOffset = 0x10;
constexpr std::size_t kCraftOnShipSystemIconOffset = 0x28;
constexpr std::size_t kWeaponIndexOnShipSystemIconOffset = 0x30;
constexpr std::size_t kHardpointOnWeaponListNodeOffset = 0x08;
constexpr std::size_t kSystemIconRenderVtableOffset = 0x10;
constexpr std::size_t kCraftOnSystemIconOffset = 0x28;
constexpr std::size_t kSystemIndexOnSystemIconOffset = 0x2c;
constexpr std::int32_t kWeaponsSystemIndex = 2;

constexpr std::uint8_t kExpectedFoWeaponCanFireAtHandler[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf4, 0x53};
constexpr std::uint8_t kExpectedWeaponCanFireAt[] = {
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x1c};
constexpr std::uint8_t kExpectedEntityGetWorldTransform[] = {
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c, 0x8b, 0x49, 0x04};
constexpr std::uint8_t kExpectedDisplayInterfaceDrawLine[] = {
    0x55, 0x8b, 0xec, 0xa1, 0x08, 0xd5, 0x7a, 0x00};
constexpr std::uint8_t kExpectedStandardComponentIsMouseOver[] = {
    0x8a, 0x41, 0x18, 0x84, 0xc0, 0x74, 0x12};
constexpr std::uint8_t kExpectedWeaponGetTarget[] = {
    0x8b, 0x49, 0x38, 0x51, 0xe8};
constexpr std::uint8_t kExpectedFoShipSystemIconRender[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xbc, 0x53};
constexpr std::uint8_t kExpectedParameterDbGetColor[] = {
    0x55, 0x8b, 0xec, 0x81, 0xec, 0x00, 0x01, 0x00, 0x00};

struct ArcColour {
    float values[3]{};
};

struct ArcUiColours {
    ArcColour boundary{{0.10f, 0.90f, 1.00f}};
    ArcColour centre{{1.00f, 0.82f, 0.12f}};
    ArcColour valid_target{{0.15f, 1.00f, 0.20f}};
};

struct ClassArcPolicy {
    ArcConfig arc{};
    bool cannon_imp = false;
};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
bool g_runtime_ready = false;
bool runtime_enabled() noexcept {
    const auto step = [](void*, std::uint32_t op,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        if (op == 0) return g_runtime_ready;
#ifdef A2FO_BUILTIN_FEATURE
        return a2fo::builtin_module_enabled(A2FO_BUILTIN_FEATURE);
#else
        return 1;
#endif
    };
    return a2fo_rs_stage10_firearc_flow(0, nullptr, step) != 0;
}
bool g_ui_colour_reader_ready = false;
bool g_ui_colours_loaded = false;
void* g_ui_parameter_db = nullptr;
ArcUiColours g_ui_colours{};
bool g_chained_fo_weapon_can_fire_at = false;
void* g_weapon_can_fire_at_original = nullptr;
void* g_ship_system_icon_render_original = nullptr;
void* g_system_icon_render_original = nullptr;
A2FO_InlineHook g_weapon_can_fire_at_hook{};
A2FO_InlineHook g_ship_system_icon_render_hook{};
std::unordered_map<void*, ClassArcPolicy> g_class_arcs;

// These one-shot messages prove that both engine stages reached the module
// without turning the per-frame target loop into an unbounded log stream.
volatile LONG g_logged_first_custom_check = 0;
volatile LONG g_logged_first_direction_allowed_target = 0;
volatile LONG g_logged_first_direction_rejected_target = 0;
volatile LONG g_logged_first_trigger_allowed_target = 0;
volatile LONG g_logged_first_trigger_rejected_target = 0;
volatile LONG g_logged_first_cannon_imp_auto_target = 0;
volatile LONG g_logged_first_hover_visualization = 0;
volatile LONG g_logged_first_system_hover_visualization = 0;

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

std::string join_path(const std::string& left, const std::string& right) {
    const auto action = a2fo_rs_stage10_fire_arcs_join(
        left.size(), right.size(),
        static_cast<std::uint8_t>(left.empty() ? 0 : left.back()));
    switch (action) {
    case 0: return right;
    case 1: return left;
    case 2: return left + right;
    default: return left + "\\" + right;
    }
}

struct ReadTextContext {
    const std::string& path;
    std::string& contents;
    std::optional<std::ifstream> input;
    std::optional<std::ostringstream> stream;
    std::int64_t size = 0;
    std::uint8_t failed = 0;
    std::exception_ptr exception;
};

std::uintptr_t read_text_step(void* opaque, std::uint32_t op,
                              std::uintptr_t) noexcept {
    auto& c = *static_cast<ReadTextContext*>(opaque);
    if (c.failed) return 0;
    try {
        switch (op) {
        case 0: c.contents.clear(); return 0;
        case 1:
            c.input.emplace(c.path, std::ios::binary);
            return static_cast<bool>(*c.input);
        case 2:
            c.input->seekg(0, std::ios::end);
            c.size = static_cast<std::int64_t>(c.input->tellg());
            return 0;
        case 3:
            log_line("Ignored oversized RTS configuration: " + c.path);
            return 0;
        case 4: c.input->seekg(0, std::ios::beg); return 0;
        case 5:
            c.stream.emplace();
            *c.stream << c.input->rdbuf();
            return 0;
        case 6: c.contents = c.stream->str(); return 0;
        case 7: return c.input->good() || c.input->eof();
        default: return 0;
        }
    } catch (...) {
        c.failed = 1;
        c.exception = std::current_exception();
        return 0;
    }
}

bool read_small_text_file(const std::string& path, std::string& contents) {
    ReadTextContext c{path, contents, {}, {}, 0, 0, {}};
    const bool result = a2fo_rs_stage10_fire_arcs_read(
        &read_text_step, &c, &c.size, &c.failed) != 0;
    if (c.exception) std::rethrow_exception(c.exception);
    return result;
}

struct FirearcRootContext {
    std::uint32_t index;
    bool enabled;
    const char* root = nullptr;
    std::optional<std::string> path;
    std::optional<std::string> contents;
    std::uint8_t candidate = 0;
    std::uint8_t failed = 0;
    std::exception_ptr exception;
};

std::uintptr_t firearc_root_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t) noexcept {
    auto& c = *static_cast<FirearcRootContext*>(opaque);
    if (c.failed) return 0;
    try {
        switch (op) {
        case 0:
            c.root = g_api->extension_root(c.index);
            return c.root && *c.root;
        case 1:
            c.path.emplace(join_path(c.root, kRtsConfigFileName));
            c.contents.emplace();
            return 0;
        case 2: return read_small_text_file(*c.path, *c.contents);
        case 3: {
            bool candidate = c.enabled;
            const auto status = a2fo::fire_arcs::parse_firearc_setting(
                *c.contents, &candidate);
            c.candidate = static_cast<std::uint8_t>(candidate);
            if (status == a2fo::fire_arcs::FireArcSettingStatus::valid) return 1;
            if (status == a2fo::fire_arcs::FireArcSettingStatus::invalid) return 2;
            return 0;
        }
        case 4:
            log_line(std::string("Applied firearc=") +
                     (c.candidate ? "1" : "0") + " from " + *c.path);
            return 0;
        case 5:
            log_line("Ignored invalid firearc value in " + *c.path);
            return 0;
        default: return 0;
        }
    } catch (...) {
        c.failed = 1;
        c.exception = std::current_exception();
        return 0;
    }
}

std::uintptr_t process_firearc_root(std::uint32_t index, bool enabled) {
    FirearcRootContext c{index, enabled, nullptr, {}, {}, 0, 0, {}};
    const auto result = a2fo_rs_stage10_fire_arcs_root(
        &firearc_root_step, &c, &c.candidate, &c.failed);
    if (c.exception) std::rethrow_exception(c.exception);
    return result;
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(
              reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    return start >= base && size <= information.RegionSize - (start - base);
}

template <typename T>
T read_at(const void* object, std::size_t offset,
          T fallback = T{}) noexcept {
    const auto* address = object
        ? static_cast<const std::uint8_t*>(object) + offset : nullptr;
    if (!readable_range(address, sizeof(T))) return fallback;
    T value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

template <typename T>
T read_live_at(const void* object, std::size_t offset,
               T fallback = T{}) noexcept {
    if (!object) return fallback;
    T value{};
    std::memcpy(&value,
                static_cast<const std::uint8_t*>(object) + offset,
                sizeof(value));
    return value;
}

bool read_ui_colour(void* parameter_db, const char* key,
                    ArcColour* output) noexcept {
    struct Frame {
        ArcColour loaded, fallback;
        explicit Frame(const ArcColour& source) noexcept
            : loaded(source), fallback(source) {}
    };
    struct Context {
        void* db;
        const char* key;
        ArcColour* output;
        std::optional<Frame> frame;
    } context{parameter_db, key, output, {}};
    const auto step = [](void* opaque, std::uint32_t op,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: return g_ui_colour_reader_ready && c.db && c.key && c.output;
        case 1:
            c.frame.emplace(*c.output);
            return a2fo_fire_arc_call_thiscall_3(
                at(g_armada, kParameterDbGetColorRva), c.db,
                reinterpret_cast<std::uintptr_t>(c.key),
                reinterpret_cast<std::uintptr_t>(&c.frame->loaded),
                reinterpret_cast<std::uintptr_t>(&c.frame->fallback));
        case 2: {
            std::uint32_t words[3];
            std::memcpy(words, c.frame->loaded.values, sizeof(words));
            const auto valid = a2fo_rs_stage10_firearc_colour(words);
            std::memcpy(c.frame->loaded.values, words, sizeof(words));
            return valid;
        }
        case 3: *c.output = c.frame->loaded; return 0;
        default: return 0;
        }
    };
    return a2fo_rs_stage10_firearc_flow(5, &context, step) != 0;
}

struct FireArcColoursContext {
    void* parameter_db = nullptr;
    ArcUiColours loaded{};
    bool found[3]{};
};

std::uintptr_t fire_arc_colours_step(void* opaque, std::uint32_t op,
                                     std::uintptr_t) noexcept {
    auto& context = *static_cast<FireArcColoursContext*>(opaque);
    switch (op) {
    case 0:
        return a2fo_rs_stage10_firearc_flow(4, &context,
            [](void* opaque, std::uint32_t operation,
               std::uintptr_t) noexcept -> std::uintptr_t {
                auto& c = *static_cast<FireArcColoursContext*>(opaque);
                switch (operation) {
                case 0: c.parameter_db = read_at<void*>(
                    at(g_armada, kGuiParameterDbPointerRva), 0, nullptr); return 0;
                case 1: return g_ui_colours_loaded;
                case 2: return g_ui_parameter_db == c.parameter_db;
                case 3: return c.parameter_db != nullptr;
                default: return 0;
                }
            });
    case 1:
        context.found[0] = read_ui_colour(
            context.parameter_db, "fireArcBoundaryColor",
            &context.loaded.boundary);
        return 1;
    case 2:
        context.found[1] = read_ui_colour(
            context.parameter_db, "fireArcCenterColor",
            &context.loaded.centre);
        return 1;
    case 3:
        context.found[2] = read_ui_colour(
            context.parameter_db, "fireArcValidTargetColor",
            &context.loaded.valid_target);
        return 1;
    case 4: {
        g_ui_colours = context.loaded;
        g_ui_parameter_db = context.parameter_db;
        g_ui_colours_loaded = true;
        char message[220]{};
        std::snprintf(message, sizeof(message),
            "Fire-arc UI colours: boundary=%s, centre=%s, validTarget=%s",
            context.found[0] ? "configured" : "default",
            context.found[1] ? "configured" : "default",
            context.found[2] ? "configured" : "default");
        log_line(message);
        return 1;
    }
    default: return 0;
    }
}

void refresh_ui_colours() noexcept {
    FireArcColoursContext context{};
    a2fo_rs_stage8_fire_arcs_refresh_colours(
        &fire_arc_colours_step, &context);
}

struct TrimContext {
    std::string* value;
    std::size_t begin = 0;
    std::uint8_t failed = 0;
    std::exception_ptr exception;
};

std::uintptr_t trim_step(void* opaque, std::uint32_t op,
                         std::uintptr_t arg) noexcept {
    auto& c = *static_cast<TrimContext*>(opaque);
    if (c.failed) return 0;
    try {
        switch (op) {
        case 0: return c.value->size();
        case 1: return std::isspace(
            static_cast<unsigned char>((*c.value)[arg])) != 0;
        case 2: c.begin = arg; return 0;
        case 3: *c.value = c.value->substr(c.begin, arg); return 0;
        default: return 0;
        }
    } catch (...) {
        c.failed = 1;
        c.exception = std::current_exception();
        return 0;
    }
}

void trim_string(std::string* value) {
    if (!value) return;
    TrimContext c{value, 0, 0, {}};
    a2fo_rs_stage10_fire_arcs_trim(&trim_step, &c, &c.failed);
    if (c.exception) std::rethrow_exception(c.exception);
}

std::uintptr_t lower_step(void* opaque, std::uint32_t op,
                          std::uintptr_t arg) noexcept {
    auto* value = static_cast<std::string*>(opaque);
    if (op == 0) return value->size();
    char& character = (*value)[arg];
    character = static_cast<char>(
        std::tolower(static_cast<unsigned char>(character)));
    return 0;
}

void lower_string(std::string* value) {
    if (value) a2fo_rs_stage10_fire_arcs_lower(&lower_step, value);
}

struct QueryStringContext {
    void* parameter_db;
    const char* key;
    std::string* output;
    bool* present;
    std::array<char, 260> value{};
    std::uint8_t failed = 0;
};

std::uintptr_t query_string_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t) noexcept {
    auto& c = *static_cast<QueryStringContext*>(opaque);
    switch (op) {
    case 0: if (c.present) *c.present = false; return 0;
    case 1: if (c.output) c.output->clear(); return 0;
    case 2:
        return c.parameter_db && c.key && *c.key && c.output && g_armada;
    case 3: {
        const std::uintptr_t found = a2fo_fire_arc_call_thiscall_4(
            at(g_armada, kParameterDbGetStringRva), c.parameter_db,
            reinterpret_cast<std::uintptr_t>(c.key),
            reinterpret_cast<std::uintptr_t>(c.value.data()),
            static_cast<std::uintptr_t>(c.value.size()),
            reinterpret_cast<std::uintptr_t>(""));
        c.value.back() = '\0';
        return (found & 0xffu) != 0;
    }
    case 4: if (c.present) *c.present = true; return 0;
    case 5:
        try { *c.output = c.value.data(); }
        catch (...) { c.failed = 1; }
        return 0;
    case 6:
        try { trim_string(c.output); }
        catch (...) { c.failed = 1; }
        return 0;
    case 7: c.output->clear(); return 0;
    default: return 0;
    }
}

bool query_parameter_string(void* parameter_db, const char* key,
                            std::string* output,
                            bool* present) noexcept {
    QueryStringContext c{parameter_db, key, output, present};
    return a2fo_rs_stage10_fire_arcs_query(
        &query_string_step, &c, &c.failed) != 0;
}

struct FloatTextContext {
    const std::string& text;
    float* output;
    char* end = nullptr;
    float parsed = 0.0f;
    bool range_error = false;
};

std::uintptr_t float_text_step(void* opaque, std::uint32_t op,
                               std::uintptr_t) noexcept {
    auto& c = *static_cast<FloatTextContext*>(opaque);
    switch (op) {
    case 0:
        if (!c.output || c.text.empty()) return 0;
        errno = 0;
        c.parsed = std::strtof(c.text.c_str(), &c.end);
        c.range_error = errno == ERANGE;
        return 1;
    case 1: return c.end == c.text.c_str();
    case 2: return c.range_error;
    case 3: return std::isfinite(c.parsed);
    case 4: return static_cast<unsigned char>(*c.end);
    case 5: ++c.end; return 0;
    case 6: return *c.end && std::isspace(static_cast<unsigned char>(*c.end));
    case 7: *c.output = c.parsed; return 0;
    default: return 0;
    }
}

bool parse_float(const std::string& text, float* output) noexcept {
    FloatTextContext c{text, output};
    return a2fo_rs_stage10_fire_arcs_float(&float_text_step, &c) != 0;
}

struct FireArcFloatContext {
    void* parameter_db;
    const char* key;
    A2foFireArcClassField* field;
    std::string text;
};

std::uintptr_t fire_arc_float_step(void* opaque, std::uint32_t op,
                                   std::uintptr_t) noexcept {
    auto& context = *static_cast<FireArcFloatContext*>(opaque);
    switch (op) {
    case 0: {
        float native_value = 0.0f;
        const std::uintptr_t native_found = a2fo_fire_arc_call_thiscall_3(
            at(g_armada, kParameterDbGetFloatRva), context.parameter_db,
            reinterpret_cast<std::uintptr_t>(context.key),
            reinterpret_cast<std::uintptr_t>(&native_value), 0);
        if ((native_found & 0xffu) == 0) return 0;
        *context.field = {1, static_cast<std::uint8_t>(
            std::isfinite(native_value)), native_value, nullptr, 0};
        return 1;
    }
    case 1: {
        bool present = false;
        const bool valid = query_parameter_string(
            context.parameter_db, context.key, &context.text, &present);
        context.field->present = static_cast<std::uint8_t>(present);
        return valid;
    }
    case 2:
        context.field->valid = static_cast<std::uint8_t>(
            parse_float(context.text, &context.field->value));
        return 1;
    default: return 0;
    }
}

void log_invalid_policy(const std::string& description,
                        const char* reason) noexcept {
    char message[420]{};
    std::snprintf(
        message, sizeof(message),
        "Ignoring custom fire arc on '%s': %s; native behavior retained",
        description.c_str(), reason ? reason : "invalid configuration");
    log_line(message);
}

struct FireArcClassContext {
    void* weapon_class;
    void* parameter_db;
    A2foFireArcClassField field{};
    A2foFireArcClassConfig config{};
    std::string mode_text;
    std::string description;
    std::string class_label;
    std::exception_ptr exception;
};

static_assert(sizeof(A2foFireArcClassConfig) == sizeof(ArcConfig));

ArcConfig arc_config_from_pod(
    const A2foFireArcClassConfig& pod) noexcept {
    ArcConfig config{};
    config.mode = pod.mode == 1 ? ArcMode::cone : ArcMode::box;
    config.yaw_degrees = pod.yaw;
    config.pitch_degrees = pod.pitch;
    config.yaw_angle_degrees = pod.yaw_angle;
    config.pitch_angle_degrees = pod.pitch_angle;
    config.cone_angle_degrees = pod.cone_angle;
    return config;
}

std::uintptr_t fire_arc_class_step(void* opaque, std::uint32_t op,
                                   std::uintptr_t value) noexcept {
    auto& context = *static_cast<FireArcClassContext*>(opaque);
    static constexpr const char* kFloatFields[] = {
        "fireArcCenter", "fireArcWidth", "fireArcYaw", "fireArcPitch",
        "fireArcYawAngle", "fireArcPitchAngle", "fireArcAngle"};
    static constexpr const char* kInvalidReasons[] = {
        "one or more angle values are malformed",
        "fireArcMode must be 'box' or 'cone'",
        "cone mode requires fireArcAngle",
        "box mode requires fireArcWidth, fireArcYawAngle, or fireArcPitchAngle",
        "an arc centre requires a width/angle command"};
    try {
        if (op < 7) {
            context.field = {};
            FireArcFloatContext float_context{
                context.parameter_db, kFloatFields[op], &context.field, {}};
            a2fo_rs_stage8_fire_arcs_optional_float(
                &fire_arc_float_step, &float_context, &context.field);
            return 1;
        }
        switch (op) {
        case 7: {
            bool present = false;
            const bool valid = query_parameter_string(
                context.parameter_db, "fireArcMode", &context.mode_text,
                &present);
            context.field = {static_cast<std::uint8_t>(present),
                             static_cast<std::uint8_t>(valid), 0.0f,
                             reinterpret_cast<const std::uint8_t*>(
                                 context.mode_text.data()),
                             context.mode_text.size()};
            return 1;
        }
        case 8:
        case 14: {
            bool present = false;
            const bool valid = query_parameter_string(
                context.parameter_db,
                op == 8 ? "wpnName" : "ordName",
                &context.description, &present);
            return valid && present && !context.description.empty();
        }
        case 15:
            context.description = "<unnamed weapon>";
            return 1;
        case 9:
            if (value < std::size(kInvalidReasons)) {
                log_invalid_policy(context.description, kInvalidReasons[value]);
            }
            return 1;
        case 10: {
            ArcConfig config = arc_config_from_pod(context.config);
            a2fo::fire_arcs::normalize_config(&config);
            context.config = {
                config.mode == ArcMode::cone ? 1 : 0,
                config.yaw_degrees, config.pitch_degrees,
                config.yaw_angle_degrees, config.pitch_angle_degrees,
                config.cone_angle_degrees};
            return 1;
        }
        case 11: {
            bool present = false;
            const bool valid = query_parameter_string(
                context.parameter_db, "classLabel", &context.class_label,
                &present);
            if (valid && present) lower_string(&context.class_label);
            context.field = {static_cast<std::uint8_t>(present),
                             static_cast<std::uint8_t>(valid), 0.0f,
                             reinterpret_cast<const std::uint8_t*>(
                                 context.class_label.data()),
                             context.class_label.size()};
            return 1;
        }
        case 12: {
            ArcConfig config = arc_config_from_pod(context.config);
            const ClassArcPolicy policy{config, value != 0};
            try {
                g_class_arcs[context.weapon_class] = policy;
            } catch (...) {
                log_line("Could not retain a weapon fire-arc policy");
                return 1;
            }
            char message[420]{};
            if (config.mode == ArcMode::cone) {
                std::snprintf(message, sizeof(message),
                    "Registered cone fire arc on '%s': yaw %.1f, pitch %.1f, angle %.1f",
                    context.description.c_str(), config.yaw_degrees,
                    config.pitch_degrees, config.cone_angle_degrees);
            } else {
                std::snprintf(message, sizeof(message),
                    "Registered box fire arc on '%s': yaw %.1f x %.1f, pitch %.1f x %.1f",
                    context.description.c_str(), config.yaw_degrees,
                    config.yaw_angle_degrees, config.pitch_degrees,
                    config.pitch_angle_degrees);
            }
            log_line(message);
            return 1;
        }
        case 13:
            trim_string(&context.mode_text);
            lower_string(&context.mode_text);
            context.field.text = reinterpret_cast<const std::uint8_t*>(
                context.mode_text.data());
            context.field.len = context.mode_text.size();
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

void register_class_arc(void* weapon_class,
                        void* parameter_db) noexcept {
    if (!g_runtime_ready || !weapon_class || !parameter_db) return;
    FireArcClassContext context{weapon_class, parameter_db, {}, {},
                                {}, {}, {}, {}};
    a2fo_rs_stage8_fire_arcs_class(
        &fire_arc_class_step, &context, &context.field, &context.config);
    if (context.exception) std::rethrow_exception(context.exception);
}

void A2FO_CALL weapon_class_loaded_handler(
    const A2FO_WeaponClassLoadedEvent* event, void*) {
    if (!event || event->struct_size < sizeof(*event)) return;
    register_class_arc(event->weapon_class, event->parameter_db);
}

bool call_native_can_fire_at(void* weapon, void* firing_context,
                             void* target, void* distance_output) noexcept {
    return (a2fo_fire_arc_call_thiscall_3(
                g_weapon_can_fire_at_original, weapon,
                reinterpret_cast<std::uintptr_t>(firing_context),
                reinterpret_cast<std::uintptr_t>(target),
                reinterpret_cast<std::uintptr_t>(distance_output)) &
            0xffu) != 0;
}

const ArcConfig* configured_arc(void* weapon,
                                void** weapon_class_output) noexcept {
    struct Context { void* weapon; void** output; } context{weapon, weapon_class_output};
    const auto step = [](void* opaque, std::uint32_t op,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        const auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: if (c.output) *c.output = nullptr; return 0;
        case 1: return runtime_enabled();
        case 2: return c.weapon != nullptr;
        case 3: {
            void* cls = read_live_at<void*>(c.weapon, kWeaponClassOnWeaponOffset, nullptr);
            if (c.output) *c.output = cls;
            const auto found = g_class_arcs.find(cls);
            return found == g_class_arcs.end() ? 0 :
                reinterpret_cast<std::uintptr_t>(&found->second.arc);
        }
        default: return 0;
        }
    };
    return reinterpret_cast<const ArcConfig*>(a2fo_rs_stage10_firearc_flow(1, &context, step));
}

const ClassArcPolicy* configured_class_policy(
    void* weapon, void** weapon_class_output = nullptr) noexcept {
    struct Context { void* weapon; void** output; } context{weapon, weapon_class_output};
    const auto step = [](void* opaque, std::uint32_t op,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        const auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: if (c.output) *c.output = nullptr; return 0;
        case 1: return runtime_enabled();
        case 2: return c.weapon != nullptr;
        case 3: {
            void* cls = read_live_at<void*>(c.weapon, kWeaponClassOnWeaponOffset, nullptr);
            if (c.output) *c.output = cls;
            const auto found = g_class_arcs.find(cls);
            return found == g_class_arcs.end() ? 0 :
                reinterpret_cast<std::uintptr_t>(&found->second);
        }
        default: return 0;
        }
    };
    return reinterpret_cast<const ClassArcPolicy*>(a2fo_rs_stage10_firearc_flow(1, &context, step));
}

void draw_world_line(const ArcLine& line,
                     std::uintptr_t colour) noexcept {
    if (!g_armada) return;
    // DisplayInterface::DrawLine accepts two world-space Vector3 values and
    // an RGB float triplet. The active camera performs projection/clipping.
    using DrawLineFn = void (__cdecl*)(const float*, const float*,
                                      const float*);
    const auto draw_line = reinterpret_cast<DrawLineFn>(
        at(g_armada, kDisplayInterfaceDrawLineRva));
    const float* colours[] = {g_ui_colours.boundary.values,
        g_ui_colours.centre.values, g_ui_colours.valid_target.values};
    const float* color = colours[a2fo_rs_stage10_firearc_colour_index(colour)];
    draw_line(line.start.values, line.end.values, color);
}

float visualization_radius(void* weapon_class) noexcept {
    const float weapon_range = read_at<float>(
        weapon_class, kRangeOnWeaponClassOffset, 300.0f);
    std::uint32_t word;
    std::memcpy(&word, &weapon_range, sizeof(word));
    word = a2fo_rs_stage10_firearc_radius(word);
    float result;
    std::memcpy(&result, &word, sizeof(result));
    return result;
}

struct FireArcLineContext {
    const ArcConfig& policy;
    const Matrix34& owner_transform;
    const float* origin;
    float radius;
    std::array<ArcLine, 128> lines{};
    std::size_t index = 0;
};

std::uintptr_t fire_arc_line_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t value) noexcept {
    auto& context = *static_cast<FireArcLineContext*>(opaque);
    switch (op) {
    case 0:
        return a2fo::fire_arcs::build_visualization_lines(
            context.policy, context.owner_transform, context.origin,
            context.radius, context.lines.data(), context.lines.size());
    case 1:
        context.index = value;
        return context.lines[value].style == ArcLineStyle::centre ? 1 : 0;
    case 2:
        draw_world_line(context.lines[context.index], value);
        return 1;
    default: return 0;
    }
}

void draw_arc_at_origin(const ArcConfig& policy,
                        const Matrix34& owner_transform,
                        const float origin[3], float radius,
                        bool target_inside_arc) noexcept {
    FireArcLineContext context{policy, owner_transform, origin, radius};
    a2fo_rs_stage8_fire_arcs_render_lines(
        &fire_arc_line_step, &context,
        static_cast<std::uint8_t>(target_inside_arc));
}

struct FireArcRenderWeaponContext {
    void* weapon;
    void* weapon_class = nullptr;
    const ArcConfig* policy = nullptr;
    void* owner = nullptr;
    Matrix34 owner_transform{};
    bool target_inside_arc = false;
    float radius = 0.0f;
    void* sentinel = nullptr;
    void* node = nullptr;
};

std::uintptr_t fire_arc_render_weapon_step(
    void* opaque, std::uint32_t op, std::uintptr_t) noexcept {
    auto& context = *static_cast<FireArcRenderWeaponContext*>(opaque);
    switch (op) {
    case 0:
        context.policy = configured_arc(
            context.weapon, &context.weapon_class);
        return context.policy && context.weapon_class;
    case 1: {
        context.owner = reinterpret_cast<void*>(
            a2fo_fire_arc_call_thiscall_0(
                at(g_armada, kWeaponGetOwnerRva), context.weapon));
        const auto* live_transform = context.owner
            ? reinterpret_cast<const Matrix34*>(
                  a2fo_fire_arc_call_thiscall_0(
                      at(g_armada, kEntityGetTransformRva), context.owner))
            : nullptr;
        if (!readable_range(live_transform, sizeof(Matrix34))) return 0;
        std::memcpy(&context.owner_transform, live_transform,
                    sizeof(context.owner_transform));
        return 1;
    }
    case 2: {
        // Weapon::GetTarget validates a live native handle before the read.
        void* target = reinterpret_cast<void*>(
            a2fo_fire_arc_call_thiscall_0(
                at(g_armada, kWeaponGetTargetRva), context.weapon));
        const auto* position = target
            ? static_cast<const std::uint8_t*>(target) +
                  kPositionOnGameObjectOffset
            : nullptr;
        if (readable_range(position, sizeof(float) * 3)) {
            float target_position[3]{};
            std::memcpy(target_position, position, sizeof(target_position));
            context.target_inside_arc = a2fo::fire_arcs::allows_target(
                *context.policy, context.owner_transform, target_position);
        }
        return 1;
    }
    case 3:
        context.radius = visualization_radius(context.weapon_class);
        context.sentinel = read_at<void*>(
            context.weapon, kHardpointListOnWeaponOffset, nullptr);
        context.node = read_at<void*>(context.sentinel, 0, nullptr);
        return 1;
    case 4:
        return context.sentinel && context.node &&
            context.node != context.sentinel;
    case 5: {
        void* hardpoint = read_at<void*>(
            context.node, kHardpointOnWeaponListNodeOffset, nullptr);
        if (!hardpoint) return 0;
        Matrix34 transform{};
        const std::uintptr_t result = a2fo_fire_arc_call_thiscall_2(
            at(g_armada, kEntityGetWorldTransformRva), context.owner,
            reinterpret_cast<std::uintptr_t>(&transform),
            reinterpret_cast<std::uintptr_t>(hardpoint));
        if (result != reinterpret_cast<std::uintptr_t>(&transform)) return 0;
        draw_arc_at_origin(
            *context.policy, context.owner_transform, &transform.values[9],
            context.radius, context.target_inside_arc);
        return 1;
    }
    case 6: {
        void* next = read_at<void*>(context.node, 0, nullptr);
        if (!next || next == context.node) return 0;
        context.node = next;
        return 1;
    }
    case 7:
        draw_arc_at_origin(
            *context.policy, context.owner_transform,
            &context.owner_transform.values[9], context.radius,
            context.target_inside_arc);
        return 1;
    default: return 0;
    }
}

bool draw_weapon_arcs(void* weapon,
                      std::size_t* hardpoint_count_output) noexcept {
    if (hardpoint_count_output) *hardpoint_count_output = 0;
    FireArcRenderWeaponContext context{weapon};
    const std::size_t result = a2fo_rs_stage8_fire_arcs_render_weapon(
        &fire_arc_render_weapon_step, &context);
    if (result == 0) return false;
    if (hardpoint_count_output) *hardpoint_count_output = result - 1;
    return true;
}

struct FireArcHoveredContext { void* weapon; };

std::uintptr_t fire_arc_hovered_step(void* opaque, std::uint32_t op,
                                     std::uintptr_t value) noexcept {
    const auto& context = *static_cast<FireArcHoveredContext*>(opaque);
    if (op == 0) {
        std::size_t count = 0;
        return draw_weapon_arcs(context.weapon, &count) ? count + 1 : 0;
    }
    if (op == 1) {
        if (InterlockedCompareExchange(
                &g_logged_first_hover_visualization, 1, 0) == 0) {
            char message[180]{};
            std::snprintf(message, sizeof(message),
                "Rendered first weapon-icon fire-arc hover (%u hardpoint%s)",
                static_cast<unsigned>(value), value == 1 ? "" : "s");
            log_line(message);
        }
        return 1;
    }
    return 0;
}

void draw_hovered_weapon_arcs(void* weapon) noexcept {
    FireArcHoveredContext context{weapon};
    a2fo_rs_stage8_fire_arcs_render_hovered(
        &fire_arc_hovered_step, &context);
}

bool weapon_vector_for_craft(void* craft, void*** begin_output,
                             std::size_t* count_output) noexcept {
    static_assert(sizeof(void*) == 4, "native weapon vector uses i386 pointers");
    struct Context {
        void* craft;
        void*** begin_output;
        std::size_t* count_output;
        void* system = nullptr;
        void** begin = nullptr;
    } context{craft, begin_output, count_output};
    const auto step = [](void* opaque, std::uint32_t op,
                         std::uintptr_t count) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0:
            if (c.begin_output) *c.begin_output = nullptr;
            if (c.count_output) *c.count_output = 0;
            return 0;
        case 1: c.system = read_at<void*>(c.craft, kWeaponSystemOnCraftOffset, nullptr); return 0;
        case 2:
            c.begin = read_at<void**>(c.system, kWeaponVectorBeginOffset, nullptr);
            return reinterpret_cast<std::uintptr_t>(c.begin);
        case 3: return reinterpret_cast<std::uintptr_t>(
            read_at<void**>(c.system, kWeaponVectorEndOffset, nullptr));
        case 4: return readable_range(c.begin, count * sizeof(void*));
        case 5:
            if (c.begin_output) *c.begin_output = c.begin;
            if (c.count_output) *c.count_output = count;
            return 0;
        default: return 0;
        }
    };
    return a2fo_rs_stage10_firearc_flow(3, &context, step) != 0;
}

struct FireArcAllContext {
    void* craft;
    void** weapons = nullptr;
};

std::uintptr_t fire_arc_all_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t value) noexcept {
    auto& context = *static_cast<FireArcAllContext*>(opaque);
    switch (op) {
    case 0: {
        std::size_t count = 0;
        return weapon_vector_for_craft(
            context.craft, &context.weapons, &count) ? count + 1 : 0;
    }
    case 1: {
        void* weapon = read_at<void*>(
            context.weapons, value * sizeof(void*), nullptr);
        std::size_t hardpoints = 0;
        return draw_weapon_arcs(weapon, &hardpoints)
            ? (hardpoints << 1) | 1 : 0;
    }
    case 2: {
        const unsigned configured = static_cast<unsigned>(value >> 16);
        const unsigned hardpoints = static_cast<unsigned>(value & 0xffff);
        if (InterlockedCompareExchange(
                &g_logged_first_system_hover_visualization, 1, 0) == 0) {
            char message[220]{};
            std::snprintf(message, sizeof(message),
                "Rendered first weapons-system fire-arc hover "
                "(%u configured weapon%s, %u hardpoint%s)",
                configured, configured == 1 ? "" : "s",
                hardpoints, hardpoints == 1 ? "" : "s");
            log_line(message);
        }
        return 1;
    }
    default: return 0;
    }
}

void draw_all_weapon_arcs(void* craft) noexcept {
    FireArcAllContext context{craft};
    a2fo_rs_stage8_fire_arcs_render_all(&fire_arc_all_step, &context);
}

struct FireArcSelectContext {
    void* icon;
    void* weapon = nullptr;
    void** begin = nullptr;
    void* craft = nullptr;
};

std::uintptr_t fire_arc_select_step(void* opaque, std::uint32_t op,
                                    std::uintptr_t value) noexcept {
    auto& context = *static_cast<FireArcSelectContext*>(opaque);
    switch (op) {
    case 0: return context.icon && g_armada;
    case 1:
        return (a2fo_fire_arc_call_thiscall_0(
            at(g_armada, kStandardComponentIsMouseOverRva),
            context.icon) & 0xffu) != 0;
    case 2: {
        const std::int32_t index = read_at<std::int32_t>(
            context.icon, kWeaponIndexOnShipSystemIconOffset, -1);
        context.craft = read_at<void*>(
            context.icon, kCraftOnShipSystemIconOffset, nullptr);
        return index < 0 ? 0 : static_cast<std::uintptr_t>(index) + 1;
    }
    case 3: {
        std::size_t count = 0;
        return weapon_vector_for_craft(context.craft, &context.begin, &count)
            ? count + 1 : 0;
    }
    case 4:
        context.weapon = read_at<void*>(
            context.begin, value * sizeof(void*), nullptr);
        return context.weapon != nullptr;
    default: return 0;
    }
}

void* weapon_for_ship_system_icon(void* icon) noexcept {
    FireArcSelectContext context{icon};
    return a2fo_rs_stage8_fire_arcs_select_weapon(
        &fire_arc_select_step, &context) != 0
        ? context.weapon : nullptr;
}

struct FireArcIconContext {
    void* icon;
    bool system_icon;
    void* weapon = nullptr;
};

std::uintptr_t fire_arc_icon_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t) noexcept {
    auto& context = *static_cast<FireArcIconContext*>(opaque);
    switch (op) {
    case 0:
        a2fo_fire_arc_call_thiscall_0(
            context.system_icon ? g_system_icon_render_original
                                : g_ship_system_icon_render_original,
            context.icon);
        return 1;
    case 1: return runtime_enabled();
    case 2:
        refresh_ui_colours();
        return 1;
    case 3:
        context.weapon = weapon_for_ship_system_icon(context.icon);
        return context.weapon != nullptr;
    case 4:
        draw_hovered_weapon_arcs(context.weapon);
        return 1;
    case 5: return context.icon && g_armada;
    case 6:
        return read_at<std::int32_t>(
            context.icon, kSystemIndexOnSystemIconOffset, -1) ==
            kWeaponsSystemIndex;
    case 7:
        return (a2fo_fire_arc_call_thiscall_0(
            at(g_armada, kStandardComponentIsMouseOverRva),
            context.icon) & 0xffu) != 0;
    case 8:
        draw_all_weapon_arcs(read_at<void*>(
            context.icon, kCraftOnSystemIconOffset, nullptr));
        return 1;
    default: return 0;
    }
}

void __attribute__((fastcall)) ship_system_icon_render_hook(
    void* icon, void*) noexcept {
    FireArcIconContext context{icon, false};
    a2fo_rs_stage8_fire_arcs_render_icon(
        &fire_arc_icon_step, &context, 0);
}

void __attribute__((fastcall)) system_icon_render_hook(
    void* icon, void*) noexcept {
    FireArcIconContext context{icon, true};
    a2fo_rs_stage8_fire_arcs_render_icon(
        &fire_arc_icon_step, &context, 1);
}

class ScopedNativeArcBypass {
public:
    explicit ScopedNativeArcBypass(void* weapon_class) noexcept {
        address_ = weapon_class
            ? static_cast<std::uint8_t*>(weapon_class) +
                  kRestrictFireArcOnWeaponClassOffset
            : nullptr;
        // Configured WeaponClasses are process-lifetime engine objects and
        // this scope is entered only from their live CanFireAt callback.
        if (!address_) return;
        original_ = *address_;
        if (original_ == 0) {
            // Most custom weapons already leave the native restriction off.
            // Avoid writing shared class memory when no bypass is required.
            address_ = nullptr;
            return;
        }
        *address_ = 0;
    }

    ~ScopedNativeArcBypass() noexcept {
        if (address_) *address_ = original_;
    }

    ScopedNativeArcBypass(const ScopedNativeArcBypass&) = delete;
    ScopedNativeArcBypass& operator=(const ScopedNativeArcBypass&) = delete;

private:
    std::uint8_t* address_ = nullptr;
    std::uint8_t original_ = 0;
};

bool evaluate_arc_policy(void* weapon, const void* target,
                         const ArcConfig& policy) noexcept {
    // Geometry failure is deliberately fail-open. Native CanFireAt has already
    // validated the target; refusing every shot because one reverse-engineered
    // pointer is unavailable would be a much more damaging failure mode.
    struct Context {
        void* weapon;
        const void* target;
        const ArcConfig& policy;
        void* owner = nullptr;
        const Matrix34* transform = nullptr;
        const std::uint8_t* position = nullptr;
    } context{weapon, target, policy};
    const auto step = [](void* opaque, std::uint32_t op,
                         std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: return c.weapon != nullptr;
        case 1:
            c.owner = reinterpret_cast<void*>(a2fo_fire_arc_call_thiscall_0(
                at(g_armada, kWeaponGetOwnerRva), c.weapon));
            return reinterpret_cast<std::uintptr_t>(c.owner);
        case 2:
            c.transform = reinterpret_cast<const Matrix34*>(a2fo_fire_arc_call_thiscall_0(
                at(g_armada, kEntityGetTransformRva), c.owner));
            return reinterpret_cast<std::uintptr_t>(c.transform);
        case 3:
            c.position = c.target ? static_cast<const std::uint8_t*>(c.target) +
                kPositionOnGameObjectOffset : nullptr;
            return reinterpret_cast<std::uintptr_t>(c.position);
        case 4: {
            // Both engine values are copied before floating-point work.
            Matrix34 owner_copy{};
            float position[3]{};
            std::memcpy(&owner_copy, c.transform, sizeof(owner_copy));
            std::memcpy(position, c.position, sizeof(position));
            return a2fo::fire_arcs::allows_target(c.policy, owner_copy, position);
        }
        default: return 0;
        }
    };
    return a2fo_rs_stage10_firearc_flow(2, &context, step) != 0;
}

struct FireArcTriggerContext {
    const A2FO_WeaponTriggerEvent* event;
    void* weapon;
    const void* target;
    const ArcConfig* policy = nullptr;
    void* weapon_class = nullptr;
};

std::uintptr_t fire_arc_trigger_step(void* opaque, std::uint32_t op,
                                     std::uintptr_t value) noexcept {
    auto& context = *static_cast<FireArcTriggerContext*>(opaque);
    switch (op) {
    case 0: return runtime_enabled();
    case 1:
        return context.event && context.event->struct_size >=
            sizeof(*context.event);
    case 2: return static_cast<std::uintptr_t>(context.event->kind);
    case 3: {
        if (context.event) {
            context.weapon = context.event->weapon;
            context.target = context.event->target;
        }
        const ClassArcPolicy* policy = configured_class_policy(
            context.weapon, &context.weapon_class);
        return !policy ? 0 : policy->cannon_imp ? 2 : 1;
    }
    case 9: {
        const bool use_primary_target = read_live_at<std::uint8_t>(
            context.weapon_class, kUsePrimaryTargetOnCannonImpClassOffset, 1) != 0;
        return a2fo::fire_arcs::defer_arc_to_candidate_selection(
            true, use_primary_target);
    }
    case 4:
        // The native candidate scan consumes CanFireAt after this activation.
        if (InterlockedCompareExchange(
                &g_logged_first_cannon_imp_auto_target, 1, 0) == 0) {
            log_line(
                "CannonImp automatic targeting deferred its trigger arc check to candidate selection");
        }
        return 1;
    case 5: return runtime_enabled() && context.weapon;
    case 6:
        context.policy = configured_arc(context.weapon, nullptr);
        return context.policy != nullptr;
    case 7:
        return evaluate_arc_policy(
            context.weapon, context.target, *context.policy);
    case 8: {
        volatile LONG* logged_result = value != 0
            ? &g_logged_first_trigger_allowed_target
            : &g_logged_first_trigger_rejected_target;
        if (InterlockedCompareExchange(logged_result, 1, 0) == 0) {
            log_line(value != 0
                ? "Full 3D fire arc allowed its first weapon trigger"
                : "Full 3D fire arc suppressed its first weapon trigger");
        }
        return 1;
    }
    default: return 0;
    }
}

bool allow_weapon_trigger(void* weapon, const void* target) noexcept {
    FireArcTriggerContext context{nullptr, weapon, target};
    return a2fo_rs_stage8_fire_arcs_trigger(
        &fire_arc_trigger_step, &context, 0) != 0;
}

bool A2FO_CALL weapon_trigger_handler(
    const A2FO_WeaponTriggerEvent* event, void*) {
    FireArcTriggerContext context{event, nullptr, nullptr};
    return a2fo_rs_stage8_fire_arcs_trigger(
        &fire_arc_trigger_step, &context, 1) != 0;
}

struct FireArcCanFireContext {
    void* weapon;
    void* firing_context;
    void* target;
    void* distance_output;
    void* weapon_class = nullptr;
    const ArcConfig* policy = nullptr;
#ifdef A2FO_BUILTIN_FEATURE
    std::uint64_t activation = 0;
#endif
};

std::uintptr_t fire_arc_can_fire_step(void* opaque, std::uint32_t op,
                                      std::uintptr_t value) noexcept {
    auto& context = *static_cast<FireArcCanFireContext*>(opaque);
    switch (op) {
    case 0:
#ifdef A2FO_BUILTIN_FEATURE
        context.activation = a2fo::builtin_module_activation(
            A2FO_BUILTIN_FEATURE);
#endif
        return 1;
    case 1:
        context.policy = configured_arc(context.weapon,
                                        &context.weapon_class);
        return context.policy != nullptr;
    case 2:
        return call_native_can_fire_at(
            context.weapon, context.firing_context,
            context.target, context.distance_output);
    case 3: {
        // The class byte is restored before the activation epoch is reread.
        ScopedNativeArcBypass bypass(context.weapon_class);
        return call_native_can_fire_at(
            context.weapon, context.firing_context,
            context.target, context.distance_output);
    }
    case 4:
#ifdef A2FO_BUILTIN_FEATURE
        return context.activation &&
            context.activation == a2fo::builtin_module_activation(
                A2FO_BUILTIN_FEATURE);
#else
        return 1;
#endif
    case 5:
        return evaluate_arc_policy(
            context.weapon, context.target, *context.policy);
    case 6:
        if (InterlockedCompareExchange(
                &g_logged_first_custom_check, 1, 0) == 0) {
            log_line("First configured weapon target check reached");
        }
        return 1;
    case 7: {
        volatile LONG* logged_result = value != 0
            ? &g_logged_first_direction_allowed_target
            : &g_logged_first_direction_rejected_target;
        if (InterlockedCompareExchange(logged_result, 1, 0) == 0) {
            log_line(value != 0
                ? "Custom 3D arc accepted its first target authorization"
                : "Custom 3D arc rejected its first target authorization");
        }
        return 1;
    }
    default: return 0;
    }
}

bool __attribute__((fastcall)) weapon_can_fire_at_hook(
    void* weapon, void*, void* firing_context, void* target,
    void* distance_output) noexcept {
    FireArcCanFireContext context{
        weapon, firing_context, target, distance_output};
    return a2fo_rs_stage8_fire_arcs_can_fire_at(
        &fire_arc_can_fire_step, &context) != 0;
}

template <std::size_t Size>
bool signature_matches(HMODULE module, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    const void* address = at(module, rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected, Size) == 0;
}

void* existing_detour_destination(const void* site,
                                  std::size_t* patch_length) noexcept {
    if (patch_length) *patch_length = 0;
    if (!site || !readable_range(site, 5)) return nullptr;
    const auto* bytes = static_cast<const std::uint8_t*>(site);
    if (bytes[0] == 0xe9) {
        std::int32_t displacement = 0;
        std::memcpy(&displacement, bytes + 1, sizeof(displacement));
        if (patch_length) *patch_length = 5;
        return const_cast<std::uint8_t*>(bytes + 5 + displacement);
    }
    if (readable_range(site, 6) && bytes[0] == 0x68 &&
        bytes[5] == 0xc3) {
        std::uint32_t destination = 0;
        std::memcpy(&destination, bytes + 1, sizeof(destination));
        if (patch_length) *patch_length = 6;
        return reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(destination));
    }
    return nullptr;
}

bool supported_fleet_ops_weapon_can_fire_at_detour(
    void** destination, std::size_t* patch_length) noexcept {
    void* resolved = existing_detour_destination(
        at(g_armada, kWeaponCanFireAtRva), patch_length);
    void* expected = at(g_fleet_ops,
                        kFoWeaponCanFireAtHandlerRva);
    const bool supported = resolved == expected &&
        signature_matches(
            g_fleet_ops, kFoWeaponCanFireAtHandlerRva,
            kExpectedFoWeaponCanFireAtHandler);
    if (destination) *destination = supported ? resolved : nullptr;
    return supported;
}

bool weapon_can_fire_at_supported() noexcept {
    if (signature_matches(
            g_armada, kWeaponCanFireAtRva,
            kExpectedWeaponCanFireAt)) {
        return true;
    }
    void* destination = nullptr;
    std::size_t patch_length = 0;
    if (supported_fleet_ops_weapon_can_fire_at_detour(
            &destination, &patch_length)) {
        return true;
    }
    char message[360]{};
    std::snprintf(
        message, sizeof(message),
        "Weapon target authorization has neither the stock prologue nor "
        "Fleet Ops' supported detour (destination=%p, expected=%p)",
        existing_detour_destination(
            at(g_armada, kWeaponCanFireAtRva), nullptr),
        at(g_fleet_ops, kFoWeaponCanFireAtHandlerRva));
    log_line(message);
    return false;
}

bool preflight_signatures() noexcept {
    bool supported = weapon_can_fire_at_supported();
    g_ui_colour_reader_ready = signature_matches(
            g_armada, kParameterDbGetColorRva,
            kExpectedParameterDbGetColor) &&
        readable_range(at(g_armada, kGuiParameterDbPointerRva),
                       sizeof(void*));
    if (!g_ui_colour_reader_ready) {
        // Colour configuration is cosmetic. Keep the firing feature and its
        // built-in colours available when only this optional reader differs.
        log_line("UI colour reader is unavailable; using default arc colours");
    }
    if (!readable_range(at(g_armada, kParameterDbGetStringRva), 5)) {
        log_line("ParameterDB::GetString entry is unreadable");
        supported = false;
    }
    if (!readable_range(at(g_armada, kParameterDbGetFloatRva), 5)) {
        log_line("ParameterDB::GetFloat entry is unreadable");
        supported = false;
    }
    if (!readable_range(at(g_armada, kEntityGetTransformRva), 5)) {
        log_line("Entity::GetTransform entry is unreadable");
        supported = false;
    }
    if (!readable_range(at(g_armada, kWeaponGetOwnerRva), 5)) {
        log_line("Weapon::GetOwner entry is unreadable");
        supported = false;
    }
    if (!signature_matches(
            g_armada, kWeaponGetTargetRva,
            kExpectedWeaponGetTarget)) {
        log_line("Weapon::GetTarget signature is unsupported");
        supported = false;
    }
    if (!signature_matches(
            g_armada, kEntityGetWorldTransformRva,
            kExpectedEntityGetWorldTransform)) {
        log_line("Entity::GetWorldTransform signature is unsupported");
        supported = false;
    }
    if (!signature_matches(
            g_armada, kDisplayInterfaceDrawLineRva,
            kExpectedDisplayInterfaceDrawLine)) {
        log_line("DisplayInterface::DrawLine signature is unsupported");
        supported = false;
    }
    if (!signature_matches(
            g_armada, kStandardComponentIsMouseOverRva,
            kExpectedStandardComponentIsMouseOver)) {
        log_line("StandardComponent mouse-over signature is unsupported");
        supported = false;
    }
    if (!signature_matches(
            g_fleet_ops, kFoShipSystemIconRenderRva,
            kExpectedFoShipSystemIconRender)) {
        log_line("Fleet Operations ShipSystemIcon render signature is unsupported");
        supported = false;
    }
    return supported;
}

bool install_ship_system_icon_render_hook(
    const A2FO_ModuleApi* api) noexcept {
    if (!api->install_inline_hook(
            at(g_fleet_ops, kFoShipSystemIconRenderRva),
            reinterpret_cast<void*>(&ship_system_icon_render_hook),
            sizeof(kExpectedFoShipSystemIconRender),
            kExpectedFoShipSystemIconRender,
            &g_ship_system_icon_render_hook)) {
        return false;
    }
    g_ship_system_icon_render_original =
        g_ship_system_icon_render_hook.gateway;
    return g_ship_system_icon_render_original != nullptr;
}

bool install_system_icon_render_vtable_hook() noexcept {
    if (!g_armada) return false;
    auto** slot = reinterpret_cast<void**>(
        static_cast<std::uint8_t*>(
            at(g_armada, kSystemIconVtableRva)) +
        kSystemIconRenderVtableOffset);
    void* const native_render = at(g_armada, kSystemIconRenderRva);
    if (!readable_range(slot, sizeof(void*)) ||
        !readable_range(native_render, 1) || *slot != native_render) {
        log_line("SystemIcon render vtable signature is unsupported");
        return false;
    }

    // Fleet Operations detours the native render entry itself. Keeping that
    // entry as the original vtable target preserves its range visualization;
    // this stable virtual boundary adds the aggregate custom arcs afterward.
    DWORD old_protect = 0;
    if (!VirtualProtect(
            slot, sizeof(void*), PAGE_READWRITE, &old_protect)) {
        log_line("SystemIcon render vtable protection change failed");
        return false;
    }
    void* const previous = InterlockedExchangePointer(
        reinterpret_cast<PVOID volatile*>(slot),
        reinterpret_cast<void*>(&system_icon_render_hook));
    DWORD restored = 0;
    VirtualProtect(slot, sizeof(void*), old_protect, &restored);
    if (previous != native_render) {
        DWORD rollback_protect = 0;
        if (VirtualProtect(
                slot, sizeof(void*), PAGE_READWRITE,
                &rollback_protect)) {
            InterlockedExchangePointer(
                reinterpret_cast<PVOID volatile*>(slot), previous);
            DWORD rollback_restored = 0;
            VirtualProtect(
                slot, sizeof(void*), rollback_protect,
                &rollback_restored);
        }
        log_line("SystemIcon render vtable changed during installation");
        return false;
    }
    g_system_icon_render_original = previous;
    return true;
}

bool install_weapon_can_fire_at_hook(
    const A2FO_ModuleApi* api) noexcept {
    void* site = at(g_armada, kWeaponCanFireAtRva);
    if (signature_matches(
            g_armada, kWeaponCanFireAtRva,
            kExpectedWeaponCanFireAt)) {
        if (!api->install_inline_hook(
                site,
                reinterpret_cast<void*>(&weapon_can_fire_at_hook),
                sizeof(kExpectedWeaponCanFireAt),
                kExpectedWeaponCanFireAt,
                &g_weapon_can_fire_at_hook)) {
            return false;
        }
        g_weapon_can_fire_at_original =
            g_weapon_can_fire_at_hook.gateway;
        g_chained_fo_weapon_can_fire_at = false;
        return g_weapon_can_fire_at_original != nullptr;
    }

    void* destination = nullptr;
    std::size_t patch_length = 0;
    if (!supported_fleet_ops_weapon_can_fire_at_detour(
            &destination, &patch_length) || patch_length > 6) {
        return false;
    }
    std::uint8_t expected_detour[6]{};
    std::memcpy(expected_detour, site, patch_length);
    if (!api->patch_jump(
            site, reinterpret_cast<void*>(&weapon_can_fire_at_hook),
            expected_detour, patch_length)) {
        return false;
    }
    g_weapon_can_fire_at_original = destination;
    g_chained_fo_weapon_can_fire_at = true;
    return true;
}

struct FireArcInitContext {
    const A2FO_ModuleApi* api;
    std::exception_ptr exception;
};

std::uintptr_t fire_arc_init_step(void* opaque, std::uint32_t op,
                                  std::uintptr_t value) noexcept {
    auto& context = *static_cast<FireArcInitContext*>(opaque);
    const A2FO_ModuleApi* api = context.api;
    try {
        switch (op) {
        case 0:
            return api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
                api->api_version == A2FO_MODULE_API_VERSION && api->log &&
                api->armada_module && api->fleetops_module &&
                api->install_inline_hook && api->patch_jump &&
                api->extension_root_count && api->extension_root;
        case 1:
            g_api = api;
            g_armada = static_cast<HMODULE>(api->armada_module());
            g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
            return g_armada && g_fleet_ops;
        case 3:
            log_line("Fire-arc module disabled by RTS_CFG.h firearc=0");
            return 1;
        case 4:
            return A2FO_MODULE_API_HAS(api, register_weapon_class_loaded_handler) &&
                A2FO_MODULE_API_HAS(api, register_weapon_trigger_handler) &&
                api->register_weapon_class_loaded_handler &&
                api->register_weapon_trigger_handler &&
                (api->capabilities & A2FO_CAP_WEAPON_CLASS_LOADED) != 0 &&
                (api->capabilities & A2FO_CAP_WEAPON_TRIGGER_EVENTS) != 0;
        case 5:
            log_line("Shared weapon event registration is unavailable");
            return 1;
        case 6:
            return A2FO_MODULE_API_HAS(api, register_weapon_trigger_handler_masked) &&
                api->register_weapon_trigger_handler_masked
                    ? api->register_weapon_trigger_handler_masked(
                          kModuleName, A2FO_WEAPON_TRIGGER_EVENT_MASK_PRECHECK,
                          &weapon_trigger_handler, nullptr)
                    : api->register_weapon_trigger_handler(
                          kModuleName, &weapon_trigger_handler, nullptr);
        case 7:
            return api->register_weapon_class_loaded_handler(
                kModuleName, nullptr, 0, &weapon_class_loaded_handler, nullptr);
        case 8:
            return api && api->install_inline_hook && api->patch_jump &&
                g_armada && g_fleet_ops;
        case 9: return preflight_signatures();
        case 10:
            log_line("Supported ArmadaL signatures were not found; runtime disabled");
            return 1;
        case 11: return install_ship_system_icon_render_hook(api);
        case 12: return install_system_icon_render_vtable_hook();
        case 13: return install_weapon_can_fire_at_hook(api);
        case 14:
            log_line("A fire-arc hook could not be installed; hooks fail closed");
            return 1;
        case 15:
            g_runtime_ready = value != 0;
            return 1;
        case 16: return g_chained_fo_weapon_can_fire_at;
        case 17:
            log_line("3D fire-arc runtime initialized and chained through Fleet Operations target authorization");
            return 1;
        case 18:
            log_line("3D fire-arc runtime initialized");
            return 1;
        case 19:
            log_line("Fire-arc module loaded with runtime disabled");
            return 1;
        case 20:
            return a2fo_rs_stage10_firearc_root_count(api->extension_root_count());
        case 21:
            return process_firearc_root(
                static_cast<std::uint32_t>(value >> 1), (value & 1) != 0);
        case 22:
            log_line("Extension-root count is invalid; firearc remains enabled");
            return 1;
        case 23:
            log_line(std::string("RTS_CFG.h firearc setting: ") +
                     ((value & 1) != 0 ? "enabled" : "disabled") +
                     ((value & 2) != 0 ? "" : " (default)"));
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    FireArcInitContext context{api, {}};
    const bool result = a2fo_rs_stage8_fire_arcs_init(
        &fire_arc_init_step, &context) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return result;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FOFireArcs_AllowWeaponTrigger(
    void* weapon, const void* target) {
    return allow_weapon_trigger(weapon, target);
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Process-lifetime inline hooks are intentionally not removed.
}
