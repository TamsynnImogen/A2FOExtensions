// Frozen/current native mock. Compile once per source under MODULE_SOURCE.
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <fstream>
#include <stdexcept>
#define STRINGIFY2(x) #x
#define STRINGIFY(x) STRINGIFY2(x)
#include MODULE_SOURCE

#ifdef TEST_BUILTIN
static bool builtin_enabled = true;
namespace a2fo {
bool builtin_module_enabled(BuiltinFeature) noexcept { return builtin_enabled; }
}
#endif

namespace {
std::vector<std::string> trace;
void A2FO_CALL logger(const char*, const char* text) { trace.emplace_back(std::string("log:") + text); }
const char* roots[2] = {};
std::uint32_t root_total = 0;
bool reject_registration = false;
const char* A2FO_CALL root_directory() { return roots[0]; }
std::uint32_t A2FO_CALL extension_root_count() { return root_total; }
const char* A2FO_CALL extension_root(std::uint32_t i) {
    return i < root_total ? roots[i] : nullptr;
}
#ifdef TEST_WRECKAGE
A2FO_ObjectDestroyedHandler handler = nullptr;
bool A2FO_CALL register_handler(const char* owner, const char* const* fields,
                                 std::uint32_t count,
                                 A2FO_ObjectDestroyedHandler callback, void*) {
    trace.emplace_back(std::string("register:") + owner + ":" +
                       std::to_string(count) + ":" + fields[0] + ":" + fields[1]);
    handler = callback;
    return !reject_registration;
}
void event(const char* label, const A2FO_OdfFieldView* fields,
           std::uint32_t count, bool valid_fields = true,
           bool valid_event = true, bool valid_replacement = true) {
    A2FO_ObjectDestroyedEvent source{};
    source.struct_size = valid_event ? sizeof(source) : 0;
    source.odf_fields = valid_fields ? fields : nullptr;
    source.odf_field_count = count;
    source.random_seed = 0;
    A2FO_ObjectReplacement output{};
    output.struct_size = valid_replacement ? sizeof(output) : 0;
    output.odf = "sentinel";
    output.flags = 17;
    output.owner = 19;
    const bool accepted = handler(&source, &output, nullptr);
    trace.emplace_back(std::string(label) + ":" + (accepted ? "1:" : "0:") +
        output.odf + ":" + std::to_string(output.flags) + ":" +
        std::to_string(output.owner));
}
A2FO_StringView view(const char* p, size_t n) { return {p, static_cast<std::uint32_t>(n)}; }
void run() {
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = &logger;
    api.capabilities = A2FO_CAP_OBJECT_DESTROYED_DISPATCH;
    api.register_object_destroyed_handler = &register_handler;
    trace.emplace_back(std::string("init:") + (A2FO_ModuleInit(&api) ? "1" : "0"));
    A2FO_OdfFieldView fields[] = {
        {view("WRECKAGE", 8), view("  debris.odf  ", 14)},
        {view("wreckageChance", 14), view("100", 3)}};
    event("replace", fields, 2);
#ifdef TEST_BUILTIN
    builtin_enabled = false;
    event("disabled", fields, 2);
    trace.emplace_back(std::string("disabled-poison:") +
        (handler(reinterpret_cast<const A2FO_ObjectDestroyedEvent*>(1),
                 reinterpret_cast<A2FO_ObjectReplacement*>(1), nullptr) ? "1" : "0"));
    builtin_enabled = true;
#endif
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const auto page = system.dwPageSize;
    auto* guarded = static_cast<unsigned char*>(VirtualAlloc(
        nullptr, page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    if (!guarded) throw std::runtime_error("guard allocation failed");
    DWORD previous = 0;
    if (!VirtualProtect(guarded + page, page, PAGE_NOACCESS, &previous))
        throw std::runtime_error("guard protection failed");
    auto* header = reinterpret_cast<A2FO_ObjectDestroyedEvent*>(guarded + page - 4);
    std::uint32_t size = 0;
    std::memcpy(header, &size, sizeof(size));
    trace.emplace_back(std::string("short-event-poison-output:") +
        (handler(header, reinterpret_cast<A2FO_ObjectReplacement*>(1), nullptr) ? "1" : "0"));
    size = sizeof(*header);
    std::memcpy(header, &size, sizeof(size));
    trace.emplace_back(std::string("null-output-unread-event-tail:") +
        (handler(header, nullptr, nullptr) ? "1" : "0"));
    trace.emplace_back(std::string("null-event-poison-output:") +
        (handler(nullptr, reinterpret_cast<A2FO_ObjectReplacement*>(1), nullptr) ? "1" : "0"));
    VirtualFree(guarded, 0, MEM_RELEASE);
    fields[1].value = view("bad", 3);
    event("invalid", fields, 2);
    fields[1].value = view("0", 1);
    event("zero", fields, 2);
    event("missing-chance", fields, 1);
    fields[0].value = view(nullptr, 3);
    event("null-value", fields, 2);
    fields[0].value = view("x", 1);
    event("bad-fields", fields, 2, false);
    event("bad-event", fields, 2, true, false);
    event("bad-output", fields, 2, true, true, false);
    event("empty-fields", nullptr, 0);
    reject_registration = true;
    trace.emplace_back(std::string("reject-init:") + (A2FO_ModuleInit(&api) ? "1" : "0"));
    api.capabilities = 0;
    trace.emplace_back(std::string("no-cap-init:") + (A2FO_ModuleInit(&api) ? "1" : "0"));
    trace.emplace_back(std::string("null-init:") + (A2FO_ModuleInit(nullptr) ? "1" : "0"));
}
#else
A2FO_InfoIniDefaultsHandler handler = nullptr;
bool A2FO_CALL register_handler(const char* owner,
                                 A2FO_InfoIniDefaultsHandler callback, void*) {
    trace.emplace_back(std::string("register:") + owner);
    handler = callback;
    return !reject_registration;
}
void write_info(const std::string& path, const char* value) {
    std::ofstream file(path, std::ios::binary); file << value;
}
void provide(const char* label, const char* normal, bool output,
             std::uint32_t capacity, bool speed_slots = true) {
    char result[128] = "sentinel";
    std::uint32_t has = 77;
    std::int32_t speed = 88;
    const bool ok = handler(normal, output ? result : nullptr, capacity,
                            speed_slots ? &has : nullptr,
                            speed_slots ? &speed : nullptr, nullptr);
    trace.emplace_back(std::string(label) + ":" + (ok ? "1:" : "0:") +
        std::to_string(has) + ":" + std::to_string(speed) + ":" + result);
}
void run() {
    const std::string base = "Z:\\tmp\\a2fo_runtime_a_base";
    const std::string active = "Z:\\tmp\\a2fo_runtime_a_active";
    CreateDirectoryA(base.c_str(), nullptr);
    CreateDirectoryA(active.c_str(), nullptr);
    write_info(base + "\\info.ini", "[mod]\nSettingsDirectory=C:\\Shared\nDefaultGameSpeed=2\n");
    write_info(active + "\\info.ini", "[mod]\nSettingsDirectory=ModSettings\nDefaultGameSpeed=4\n");
    roots[0] = base.c_str(); roots[1] = active.c_str(); root_total = 2;
    SetEnvironmentVariableA("APPDATA", "C:\\Users\\Fixture\\AppData\\Roaming");
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.api_revision = 5;
    api.capabilities = A2FO_CAP_INFO_INI_DEFAULTS;
    api.log = &logger;
    api.root_directory = &root_directory;
    api.extension_root_count = &extension_root_count;
    api.extension_root = &extension_root;
    api.register_info_ini_defaults_handler = &register_handler;
    trace.emplace_back(std::string("init-overlay:") + (A2FO_ModuleInit(&api) ? "1" : "0"));
    provide("normal", "C:\\Users\\Fixture\\AppData\\Roaming\\mods\\Example", true, 128);
    char aliased[128] = "C:\\Users\\Fixture\\AppData\\Roaming\\mods\\Example";
    std::uint32_t alias_has = 77;
    std::int32_t alias_speed = 88;
    const bool alias_ok = handler(aliased, aliased, sizeof(aliased),
                                  &alias_has, &alias_speed, nullptr);
    trace.emplace_back(std::string("alias-normal-output:") +
        (alias_ok ? "1:" : "0:") + std::to_string(alias_has) + ":" +
        std::to_string(alias_speed) + ":" + aliased);
#ifdef TEST_BUILTIN
    builtin_enabled = false;
    provide("disabled", "C:\\Normal", true, 128);
    provide("disabled-poison-normal", reinterpret_cast<const char*>(1), true, 128);
    builtin_enabled = true;
#endif
    provide("small", "C:\\Users\\Fixture\\AppData\\Roaming\\mods\\Example", true, 2);
    provide("no-buffer", "C:\\Normal", false, 0);
    provide("no-buffer-poison-normal", reinterpret_cast<const char*>(1), false, 0);
    provide("no-speed-poison-normal", reinterpret_cast<const char*>(1), true, 128, false);
    provide("no-normal", nullptr, true, 128);
    provide("no-speed", "C:\\Normal", true, 128, false);
    root_total = 1;
    trace.emplace_back(std::string("init-base:") + (A2FO_ModuleInit(&api) ? "1" : "0"));
    provide("base", "C:\\Normal", true, 128);
    roots[0] = nullptr; root_total = 0;
    trace.emplace_back(std::string("init-no-root:") + (A2FO_ModuleInit(&api) ? "1" : "0"));
    reject_registration = true;
    roots[0] = base.c_str(); root_total = 1;
    trace.emplace_back(std::string("init-reject:") + (A2FO_ModuleInit(&api) ? "1" : "0"));
    trace.emplace_back(std::string("null-init:") + (A2FO_ModuleInit(nullptr) ? "1" : "0"));
    DeleteFileA((base + "\\info.ini").c_str());
    DeleteFileA((active + "\\info.ini").c_str());
    RemoveDirectoryA(base.c_str()); RemoveDirectoryA(active.c_str());
}
#endif
}
int main() {
    run();
    for (const auto& line : trace) std::printf("%s\n", line.c_str());
}
