/* Native ODF-driven wreckage replacement module. */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/module_runtime_a_ffi.h"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif

#include <windows.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <exception>

namespace {

constexpr char kModuleName[] = "A2FOWreckage";
constexpr std::array<const char*, 2> kRequiredFields{{
    "wreckage", "wreckageChance"}};

const A2FO_ModuleApi* g_api = nullptr;
thread_local std::array<char, 256> g_replacement_odf{};
std::exception_ptr g_register_error;

void log_line(const char* message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message);
}

uint8_t field_at(void* context, std::uint32_t index,
                 A2foRsRuntimeField* out) noexcept {
    const auto* event = static_cast<const A2FO_ObjectDestroyedEvent*>(context);
    if (!out || index >= event->odf_field_count) return 0;
    const A2FO_OdfFieldView& field = event->odf_fields[index];
    out->name = {reinterpret_cast<const std::uint8_t*>(field.name.data),
                 field.name.size};
    out->value = {reinterpret_cast<const std::uint8_t*>(field.value.data),
                  field.value.size};
    out->value_valid = field.value.data || field.value.size == 0;
    return 1;
}

uint8_t register_handler(void*) noexcept;
void log_init(void*, std::uint32_t code) noexcept {
    if (g_register_error) return;
    log_line(code == 0 ? "Object-destroyed dispatch is unavailable" :
               "Native ODF wreckage replacement initialized");
}

bool A2FO_CALL object_destroyed_handler(
    const A2FO_ObjectDestroyedEvent* event,
    A2FO_ObjectReplacement* replacement, void*) {
    const bool enabled =
#ifdef A2FO_BUILTIN_FEATURE
        a2fo::builtin_module_enabled(A2FO_BUILTIN_FEATURE);
#else
        true;
#endif
    // Preserve admission order before borrowing any native event fields.
    if (!enabled || !event || event->struct_size < sizeof(*event) ||
        !replacement || replacement->struct_size < sizeof(*replacement)) {
        return false;
    }
    A2foRsRuntimeBytes selected{};
    const std::uint32_t status = a2fo_rs_runtime_wreckage_handler(
        enabled, event && event->struct_size >= sizeof(*event),
        replacement && replacement->struct_size >= sizeof(*replacement),
        event && (event->odf_fields || event->odf_field_count == 0),
        event ? event->odf_field_count : 0, event ? event->random_seed : 0,
        g_replacement_odf.size(), &field_at,
        const_cast<A2FO_ObjectDestroyedEvent*>(event), &selected);
    if (status == 2) {
        log_line("Ignored invalid wreckageChance; use a value from 0 to 100");
        return false;
    }
    if (status != 1) return false;
    std::memcpy(g_replacement_odf.data(), selected.data, selected.size);
    g_replacement_odf[selected.size] = '\0';
    replacement->odf = g_replacement_odf.data();
    replacement->flags = A2FO_REPLACEMENT_INHERIT_POSITION |
        A2FO_REPLACEMENT_INHERIT_ROTATION;
    replacement->owner = A2FO_REPLACEMENT_OWNER_NEUTRAL;
    return true;
}

uint8_t register_handler(void*) noexcept {
    try {
        return g_api->register_object_destroyed_handler(
            kModuleName, kRequiredFields.data(),
            static_cast<std::uint32_t>(kRequiredFields.size()),
            &object_destroyed_handler, nullptr);
    } catch (...) {
        g_register_error = std::current_exception();
        return 0;
    }
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    const bool valid = api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
        api->api_version == A2FO_MODULE_API_VERSION && api->log;
    if (!valid) return false;
    g_api = api;
    const bool available = A2FO_MODULE_API_HAS(api, register_object_destroyed_handler) &&
        api->register_object_destroyed_handler &&
        (api->capabilities & A2FO_CAP_OBJECT_DESTROYED_DISPATCH) != 0;
    g_register_error = nullptr;
    const bool result = a2fo_rs_runtime_wreckage_init(
        valid, available, &register_handler, &log_init, nullptr) != 0;
    if (g_register_error) std::rethrow_exception(g_register_error);
    return result;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {}
