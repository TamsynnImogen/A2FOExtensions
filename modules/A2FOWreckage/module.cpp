/* Native ODF-driven wreckage replacement module. */

#include "../../sdk/include/a2fo_module_api.h"
#include "wreckage_policy.hpp"

#include <windows.h>

#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace {

constexpr char kModuleName[] = "A2FOWreckage";
constexpr std::array<const char*, 2> kRequiredFields{{
    "wreckage", "wreckageChance"}};

const A2FO_ModuleApi* g_api = nullptr;
thread_local std::array<char, 256> g_replacement_odf{};

void log_line(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message);
}

bool equal_ascii_case_insensitive(
    A2FO_StringView value, const char* expected) noexcept {
    if (!expected) return false;
    const std::size_t expected_size = std::strlen(expected);
    if (!value.data || value.size != expected_size) return false;
    for (std::size_t index = 0; index < expected_size; ++index) {
        if (std::tolower(static_cast<unsigned char>(value.data[index])) !=
            std::tolower(static_cast<unsigned char>(expected[index]))) {
            return false;
        }
    }
    return true;
}

bool find_field(
    const A2FO_ObjectDestroyedEvent* event, const char* name,
    std::string_view* value) noexcept {
    if (value) *value = {};
    if (!event || !name || !value ||
        (!event->odf_fields && event->odf_field_count != 0)) {
        return false;
    }
    for (std::uint32_t index = 0; index < event->odf_field_count; ++index) {
        const A2FO_OdfFieldView& field = event->odf_fields[index];
        if (!equal_ascii_case_insensitive(field.name, name)) continue;
        if (!field.value.data && field.value.size != 0) return false;
        *value = std::string_view(
            field.value.data ? field.value.data : "", field.value.size);
        return true;
    }
    return false;
}

bool A2FO_CALL object_destroyed_handler(
    const A2FO_ObjectDestroyedEvent* event,
    A2FO_ObjectReplacement* replacement, void*) {
    if (!event || event->struct_size < sizeof(*event) || !replacement ||
        replacement->struct_size < sizeof(*replacement)) {
        return false;
    }
    std::string_view wreckage;
    if (!find_field(event, "wreckage", &wreckage)) return false;
    std::string_view chance;
    const bool chance_present = find_field(
        event, "wreckageChance", &chance);
    const a2fo::wreckage::Decision decision =
        a2fo::wreckage::decide_replacement(
            wreckage, chance_present, chance, event->random_seed);
    if (decision.status ==
        a2fo::wreckage::DecisionStatus::invalid_chance) {
        log_line("Ignored invalid wreckageChance; use a value from 0 to 100");
        return false;
    }
    if (decision.status != a2fo::wreckage::DecisionStatus::replace ||
        decision.replacement_odf.empty() ||
        decision.replacement_odf.size() >= g_replacement_odf.size()) {
        return false;
    }
    std::memcpy(g_replacement_odf.data(),
                decision.replacement_odf.data(),
                decision.replacement_odf.size());
    g_replacement_odf[decision.replacement_odf.size()] = '\0';
    replacement->odf = g_replacement_odf.data();
    replacement->flags = A2FO_REPLACEMENT_INHERIT_POSITION |
        A2FO_REPLACEMENT_INHERIT_ROTATION;
    replacement->owner = A2FO_REPLACEMENT_OWNER_NEUTRAL;
    return true;
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log) {
        return false;
    }
    g_api = api;
    if (!A2FO_MODULE_API_HAS(api, register_object_destroyed_handler) ||
        !api->register_object_destroyed_handler ||
        (api->capabilities & A2FO_CAP_OBJECT_DESTROYED_DISPATCH) == 0 ||
        !api->register_object_destroyed_handler(
            kModuleName, kRequiredFields.data(),
            static_cast<std::uint32_t>(kRequiredFields.size()),
            &object_destroyed_handler, nullptr)) {
        log_line("Object-destroyed dispatch is unavailable");
        return false;
    }
    log_line("Native ODF wreckage replacement initialized");
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {}
