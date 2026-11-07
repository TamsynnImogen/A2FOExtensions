// Frozen/current native M3D module workflow fixture. Images are synthetic data.
#include MODULE_SOURCE

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
std::uint8_t* image = nullptr;
std::uint8_t* fleet = nullptr;
std::string root;
std::string mode;
unsigned hooks = 0;
unsigned native_calls = 0;
bool active = true;
void* native_result = reinterpret_cast<void*>(0x12345678);
void* expected_geometry = nullptr;
void* expected_resource = nullptr;
void* expected_tree_root = nullptr;
void* expected_mesh = nullptr;

void check(bool good, const char* message) {
    if (!good) throw std::runtime_error(message);
}
void line(const std::string& value) { std::puts(value.c_str()); }
std::string stable(std::string value) {
    const auto at = value.find(root);
    if (!root.empty() && at != std::string::npos) value.replace(at, root.size(), "<root>");
    return value;
}
void A2FO_CALL logger(const char*, const char* message) {
    std::string value = stable(message);
    if (value.find("Finalized M3D model ") == 0 ||
        value.find("Parsed M3D model ") == 0) {
        char geometry_text[32]{};
        std::snprintf(geometry_text, sizeof(geometry_text), "%p", expected_geometry);
        const auto at = value.find(geometry_text);
        check(expected_geometry && at != std::string::npos,
              "M3D model log identity");
        value.replace(at, std::strlen(geometry_text), "<geometry>");
        char resource_text[32]{};
        std::snprintf(resource_text, sizeof(resource_text), "%p", expected_resource);
        const auto resource_at = value.find(resource_text, at);
        check(resource_at != std::string::npos, "M3D resource log identity");
        value.replace(resource_at, std::strlen(resource_text), "<resource>");
    }
    if (value.find("Runtime Storm3D tree ") == 0 &&
        value.find("root=") != std::string::npos) {
        char root_text[32]{};
        std::snprintf(root_text, sizeof(root_text), "%p", expected_tree_root);
        const auto at = value.find(root_text);
        check(expected_tree_root && at != std::string::npos,
              "M3D tree root log identity");
        value.replace(at, std::strlen(root_text), "<root-node>");
    }
    if (value.find("Runtime mesh ") == 0) {
        char mesh_text[32]{};
        std::snprintf(mesh_text, sizeof(mesh_text), "%p", expected_mesh);
        const auto at = value.find(mesh_text);
        check(expected_mesh && at != std::string::npos,
              "M3D mesh log identity");
        value.replace(at, std::strlen(mesh_text), "<mesh-node>");
    }
    line("log:" + value);
}
void* A2FO_CALL armada() {
    line("api:armada");
    if (mode == "init-armada-throw") throw std::runtime_error("armada injected");
    return mode == "init-no-armada" ? nullptr : image;
}
void* A2FO_CALL fleetops() { line("api:fleetops"); return fleet; }
std::uint32_t A2FO_CALL roots() {
    line("api:roots");
    if (mode == "init-roots-throw") throw std::runtime_error("roots injected");
    return 1;
}
const char* A2FO_CALL root_at(std::uint32_t index) {
    line("api:root:" + std::to_string(index));
    return index == 0 ? root.c_str() : nullptr;
}
bool A2FO_CALL install(void* target, void*, std::size_t size,
                       const std::uint8_t* signature, A2FO_InlineHook* hook) {
    ++hooks;
    line("api:hook:" + std::to_string(hooks));
    if (mode == "init-hook-throw") throw std::runtime_error("hook injected");
    check(std::memcmp(target, signature, size) == 0, "hook signature");
    if ((mode == "init-hook-first" && hooks == 1) ||
        (mode == "init-hook-second" && hooks == 2)) return false;
    *hook = {target, reinterpret_cast<void*>(0x87654321),
             static_cast<std::uint32_t>(size)};
    return true;
}
void make_file(const char* name) {
    const std::string path = root + "\\sod\\" + name;
    HANDLE file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    check(file != INVALID_HANDLE_VALUE, "create fixture file");
    DWORD written = 0;
    check(WriteFile(file, "M3D!", 4, &written, nullptr) && written == 4,
          "write fixture file");
    CloseHandle(file);
}
void put_pointer(std::uint8_t* bytes, std::size_t offset, void* value) {
    std::memcpy(bytes + offset, &value, sizeof(value));
}
void hierarchy_case() {
    std::array<std::uint8_t, 0x50> geometry{};
    std::array<std::uint8_t, 0x150> parent{}, mesh{};
    char parent_name[] = "root";
    char mesh_name[32] = "mesh_temp";
    put_pointer(geometry.data(), 0x3c, parent.data());
    put_pointer(parent.data(), 0x08, parent_name);
    put_pointer(parent.data(), 0x1c, mesh.data());
    put_pointer(mesh.data(), 0x08, mesh_name);
    put_pointer(mesh.data(), 0x18, parent.data());
    void* node_vtable[2]{nullptr, reinterpret_cast<void*>(&check)};
    put_pointer(parent.data(), 0, node_vtable);
    put_pointer(mesh.data(), 0, node_vtable);
    expected_tree_root = parent.data();
    expected_mesh = mesh.data();
    a2fo::m3d::ConversionStats stats;
    stats.hierarchy_bindings = {
        {"root", "", "root"},
        {"mesh_temp", "root", "mesh"},
        {"absent", "root", "absent"},
    };
    const auto audit = audit_runtime_hierarchy(geometry.data(), stats, "fixture");
    line("audit:" + std::to_string(audit.discovered) + ":" +
         std::to_string(audit.expected) + ":" +
         std::to_string(audit.mismatched) + ":" +
         std::to_string(audit.missing));
    check(audit.discovered == 2 && audit.expected == 3 &&
          audit.mismatched == 0 && audit.missing == 1, "hierarchy audit");
    stats.texture_bindings.push_back({});
    stats.texture_bindings.back().mesh_name = "mesh_temp";
    stats.texture_bindings.back().diffuse_texture = "legacy.dds";
    const auto bound = bind_runtime_m3d_textures(
        geometry.data(), stats, "fixture");
    line("texture-signature-bound:" + std::to_string(bound));
    check(bound == 0, "texture signature gate");
    const auto tree = inspect_runtime_tree(geometry.data(), "fixture");
    line("tree:" + std::to_string(tree.nodes) + ":" +
         std::to_string(tree.meshes) + ":" +
         std::to_string(tree.missing_renderers));
    check(tree.nodes == 2 && tree.meshes == 1 &&
          tree.missing_renderers == 1, "runtime tree inspection");
    const auto restored = restore_runtime_authored_names(
        geometry.data(), stats, "fixture");
    line("restore:" + std::to_string(restored) + ":" + mesh_name);
    check(restored == 1 && std::strcmp(mesh_name, "mesh") == 0,
          "authored name restoration");
}
void cleanup() {
    if (root.empty()) return;
    for (const char* name : {"legacy.m3d", "legacy.sod", "legacy.sod.a2fo-m3d",
                             "authored.m3d", "authored.sod", "solo.m3d"}) {
        DeleteFileA((root + "\\sod\\" + name).c_str());
    }
    RemoveDirectoryA((root + "\\sod").c_str());
    RemoveDirectoryA(root.c_str());
}
void setup() {
    char temporary[MAX_PATH]{};
    check(GetTempPathA(sizeof(temporary), temporary) != 0, "temp path");
    root = std::string(temporary) + "a2fo-stage8-m3d-native";
    cleanup();
    check(CreateDirectoryA(root.c_str(), nullptr) != 0, "create root");
    check(CreateDirectoryA((root + "\\sod").c_str(), nullptr) != 0,
          "create sod");
    for (const char* name : {"legacy.m3d", "legacy.sod",
                             "legacy.sod.a2fo-m3d", "authored.m3d",
                             "authored.sod", "solo.m3d"}) make_file(name);
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    fleet = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x1e0000,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    check(image && fleet, "synthetic images");
    std::memcpy(image + kMeshUpdateRva, kExpectedMeshUpdate,
                sizeof(kExpectedMeshUpdate));
    std::memcpy(fleet + kFoLoadSodHandlerRva, kExpectedFoLoadSodHandler,
                sizeof(kExpectedFoLoadSodHandler));
    std::memcpy(fleet + kFoLoadSodNoFallbackHandlerRva,
                kExpectedFoLoadSodNoFallbackHandler,
                sizeof(kExpectedFoLoadSodNoFallbackHandler));
    if (mode == "init-signature") fleet[kFoLoadSodHandlerRva] ^= 1;
}
void run() {
    setup();
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = logger;
    api.armada_module = armada;
    api.fleetops_module = fleetops;
    api.extension_root_count = roots;
    api.extension_root = root_at;
    api.install_inline_hook = install;
    if (mode == "init-short") api.struct_size = A2FO_MODULE_API_V4_BASE_SIZE - 1;
    const bool ready = A2FO_ModuleInit(&api);
    line("ready:" + std::to_string(ready) + ":hooks:" + std::to_string(hooks) +
         ":sources:" + std::to_string(g_sources.size()));
    if (!ready) { A2FO_ModuleShutdown(); cleanup(); return; }
    line("winner:legacy:" + std::to_string(g_sources.count("legacy")) +
         ":authored:" + std::to_string(g_sources.count("authored")) +
         ":solo:" + std::to_string(g_sources.count("solo")));
    check(g_sources.count("solo") == 1 && g_sources.count("authored") == 0,
          "source precedence");
    if (mode == "names") {
        line("name:key:" + model_key("Path/Sub\\MiXeD.M3D"));
        line("name:base:" + basename_without_extension("folder/ship.part.sod"));
        line("name:texture:" + legacy_texture_filename("textures\\MiXeD.DDS"));
        line("name:join:" + join_path("left\\", "right"));
        a2fo::m3d::ConversionStats stages{};
        line("name:stage:" + std::string(parser_stage(14, stages)));
    }
    if (mode == "hierarchy") hierarchy_case();
    if (mode == "cached" || mode == "cached-no-fallback" ||
        mode == "cached-invalid") {
        const std::string key = runtime_cache_key("solo", 1.25f);
        std::array<std::uint8_t, 0x80> geometry{}, resource{};
        void* resource_pointer = mode == "cached-invalid" ? nullptr : resource.data();
        std::memcpy(geometry.data() + 0x40, &resource_pointer, sizeof(resource_pointer));
        expected_geometry = geometry.data();
        expected_resource = resource_pointer;
        g_loaded[key] = geometry.data();
        const void* answer = mode == "cached-no-fallback"
            ? load_sod_no_fallback_hook(image + 0x100, nullptr, "solo.m3d", 1.25f)
            : load_sod_hook(image + 0x100, nullptr, "solo.m3d", 1.25f);
        line("resolve:cached:" + std::to_string(answer == geometry.data()) +
             ":native:" + std::to_string(answer == native_result) +
             ":ref:" + std::to_string(
                 *reinterpret_cast<std::uint32_t*>(resource.data() + 0x10)) +
             ":mode:" + std::to_string(resource[0x14]) +
             ":cache:" + std::to_string(g_loaded.count(key)) +
             ":failed:" + std::to_string(g_failed_runtime.count(key)));
    } else if (mode == "resolver-off" || mode == "resolver-null" ||
               mode == "resolver-missing" || mode == "load-fail") {
        active = mode != "resolver-off";
        const char* name = mode == "resolver-null" ? nullptr :
                           mode == "resolver-missing" ? "other.sod" : "solo.m3d";
        const void* answer = load_sod_hook(image + 0x100, nullptr, name, 1.25f);
        line("resolve:native:" + std::to_string(answer == native_result) +
             ":calls:" + std::to_string(native_calls) +
             ":failed:" + std::to_string(g_failed_runtime.size()));
    }
    A2FO_ModuleShutdown();
    line("shutdown:ready:" + std::to_string(g_ready) +
         ":sources:" + std::to_string(g_sources.size()));
    cleanup();
}
} // namespace

#ifdef A2FO_BUILTIN_FEATURE
bool a2fo::builtin_module_enabled(a2fo::BuiltinFeature) noexcept { return active; }
#endif

extern "C" void* a2fo_m3d_call_load_sod(void*, void*, const char*, float) {
    ++native_calls;
    line("native:load");
    return native_result;
}
extern "C" void* a2fo_m3d_parse_sod_stream(void*, void*, float,
                                               const char*) {
    line("native:parse");
    return nullptr;
}
extern "C" std::uintptr_t a2fo_m3d_call_thiscall_0(void*, void* self) {
    return self == expected_mesh ? 1 : 0;
}
extern "C" std::uintptr_t a2fo_m3d_call_thiscall_1(void*, void*,
                                                        std::uintptr_t) {
    line("native:mesh-update");
    return 0;
}
extern "C" std::uintptr_t a2fo_m3d_call_thiscall_2(void*, void*,
                                                        std::uintptr_t,
                                                        std::uintptr_t) { return 0; }
extern "C" std::uintptr_t a2fo_m3d_call_thiscall_4(void*, void*,
                                                        std::uintptr_t,
                                                        std::uintptr_t,
                                                        std::uintptr_t,
                                                        std::uintptr_t) { return 0; }

int main(int argc, char** argv) {
    try {
        check(argc == 2, "one fixture mode required");
        mode = argv[1];
        run();
        return 0;
    } catch (const std::exception& error) {
        cleanup();
        std::fprintf(stderr, "fixture: %s\n", error.what());
        return 1;
    }
}
