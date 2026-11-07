#include <windows.h>
#include <cstdio>
#include <cstddef>
#include <string>
#include <stdexcept>

static DWORD fixture_attributes(const char* path);
#define GetFileAttributesA fixture_attributes
#include A2FO_FIXTURE_SOURCE
#undef GetFileAttributesA

static DWORD fixture_attributes(const char* path) {
    std::printf("probe:%s\n", path);
    return GetFileAttributesA(path);
}

static void create_file(const std::string& path) {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) std::abort();
    std::fputc('x', file);
    std::fclose(file);
}
static void create_text(const std::string& path, const char* text) {
    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) std::abort();
    std::fputs(text, file);
    std::fclose(file);
}

static void show(const char* name, const std::string& value) {
    std::printf("%s:%s\n", name, value.c_str());
}

static bool g_throw_specular = false;
static int __cdecl mock_specular(void*, const char* const* diffuse,
                                const char* const* paths,
                                std::uint32_t count) {
    std::printf("specular-count:%u\n", count);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::printf("specular:%s:%s\n", diffuse[index], paths[index]);
    }
    if (g_throw_specular) throw std::runtime_error("specular callback");
    return 1;
}
static int __cdecl mock_faction_suffix(const char* suffix) {
    std::printf("race-register:%s\n", suffix);
    return 1;
}

struct MockMesh {
    std::uint8_t padding[0x128]{};
    void* renderer = nullptr;
    std::uint32_t flags = 0;
    void* diffuse = nullptr;
    void* bump = nullptr;
    bool fail_update = false;
};
static_assert(offsetof(MockMesh, renderer) == 0x128);
static_assert(offsetof(MockMesh, flags) == 0x12c);

struct MockTexture {
    std::uint32_t padding0 = 0;
    void* database = nullptr;
    const char* name = nullptr;
    std::uint8_t padding1[0x0c]{};
    std::uint32_t flags = 0;
};
static_assert(offsetof(MockTexture, database) == 4);
static_assert(offsetof(MockTexture, name) == 8);
static_assert(offsetof(MockTexture, flags) == 0x18);
static bool g_mutate_bump = false;
static MockMesh* g_mutate_mesh = nullptr;

static std::uintptr_t __attribute__((thiscall)) mock_mesh_get(
    MockMesh* mesh, std::uintptr_t slot) {
    std::printf("mesh-get:%u\n", static_cast<unsigned>(slot));
    return reinterpret_cast<std::uintptr_t>(slot == 0 ? mesh->diffuse : mesh->bump);
}
static std::uintptr_t __attribute__((thiscall)) mock_mesh_set(
    MockMesh* mesh, std::uintptr_t texture, std::uintptr_t slot) {
    std::printf("mesh-set:%u:%u\n", static_cast<unsigned>(slot),
                texture != 0);
    if (slot == 1) mesh->bump = reinterpret_cast<void*>(texture);
    return 0;
}
static std::uintptr_t __attribute__((thiscall)) mock_mesh_update(
    MockMesh* mesh, std::uintptr_t argument) {
    std::printf("mesh-update:%u:%u\n", static_cast<unsigned>(argument),
                mesh->fail_update);
    if (mesh->fail_update) {
        mesh->fail_update = false;
        mesh->renderer = nullptr;
        return 1;
    }
    mesh->renderer = mesh;
    return 0;
}
static bool __cdecl mock_native_file_exists(const char* path) {
    std::printf("native-exists:%s\n", path);
    if (g_mutate_bump && std::strstr(path, "a_bump")) {
        g_mutate_bump = false;
        g_art_texture_suffix_config.bump_suffix = "_after";
        std::puts("mutated:bump-suffix");
    }
    return std::strstr(path, "a_bump") || std::strstr(path, "b_bump");
}
static void* __cdecl mock_texture_find(
    void*, const char* name, std::uint32_t flags) {
    std::printf("native-find:%s:%u\n", name, flags);
    static int bump;
    if (g_mutate_mesh) {
        g_mutate_mesh->flags = 0x40;
        g_mutate_mesh = nullptr;
        std::puts("mutated:mesh-flags");
    }
    return &bump;
}

static int g_parameter_mode = 0;
static std::uintptr_t __attribute__((thiscall)) mock_parameter_string(
    void*, const char* command, char* output, std::uintptr_t capacity,
    const char*) {
    std::printf("parameter-string:%s\n", command);
    const char* value = nullptr;
    if (g_parameter_mode == 0) {
        if (std::strcmp(command, "sensorsScorch1") == 0)
            value = "scorch.dds";
        if (std::strcmp(command, "sensorsScorch1Hardpoint") == 0)
            value = "hp";
        if (std::strcmp(command, "sensorsScorch1Offset") == 0)
            value = "1,2,3";
        if (std::strcmp(command, "sensorsScorch1Size") == 0)
            value = "4,5";
    }
    if (g_parameter_mode == 0 || g_parameter_mode == 2) {
        if (std::strcmp(command, "logoDecal1Hardpoint") == 0)
            value = "hp";
        if (std::strcmp(command, "logoDecal1Offset") == 0)
            value = "1,2,3";
        if (std::strcmp(command, "logoDecal1Size") == 0)
            value = "4,5";
        if (std::strcmp(command, "logoDecal1ColourKey") == 0)
            value = "128.49,255.6,-1";
        if (std::strcmp(command, "logoDecal1Suffix") == 0)
            value = "_x";
    }
    if (g_parameter_mode == 3 &&
        std::strcmp(command, "emissiveWarp") == 0)
        value = "warp.dds";
    if (g_parameter_mode == 4) {
        if (std::strcmp(command, "texture0") == 0)
            value = "ship.dds";
        if (std::strcmp(command, "emissive0Warp") == 0)
            value = "warp.dds";
    }
    if (!value || std::strlen(value) >= capacity) return 0;
    std::strcpy(output, value);
    return 1;
}
static std::uintptr_t __attribute__((thiscall)) mock_parameter_float(
    void*, const char* command, float* output, std::uint32_t) {
    std::printf("parameter-float:%s\n", command);
    if (std::strcmp(command, "ScaleSOD") == 0) *output = 2.0f;
    else if (std::strcmp(command, "damageThreshold") == 0)
        *output = g_parameter_mode == 0 ? 0.25f : -1.0f;
    else if (std::strcmp(command, "damageDecalPreview") == 0)
        *output = 1.0f;
    else if (std::strcmp(command, "logoDecal1FlipU") == 0)
        *output = 1.0f;
    else return 0;
    return 1;
}
static std::uintptr_t __attribute__((thiscall)) mock_parameter_vector(
    void*, const char* command, NativeStringVector* output,
    std::uintptr_t) {
    std::printf("parameter-vector:%s\n", command);
    if (std::strcmp(command, "logoFileNames") != 0 ||
        g_parameter_mode != 0) return 0;
    output->begin = static_cast<char**>(std::malloc(3 * sizeof(char*)));
    if (!output->begin) std::abort();
    const char* values[] = {"flag.dds", "", "missing.dds"};
    for (std::size_t index = 0; index < 3; ++index) {
        output->begin[index] = static_cast<char*>(
            std::malloc(std::strlen(values[index]) + 1));
        if (!output->begin[index]) std::abort();
        std::strcpy(output->begin[index], values[index]);
    }
    output->end = output->begin + 3;
    output->capacity = output->end;
    return 1;
}
static void __cdecl mock_engine_delete(void* value) {
    std::puts("engine-delete");
    std::free(value);
}
static int __cdecl mock_register_damage(
    void*, float threshold, const DamageDecalDescriptor* descriptors,
    std::uint32_t count) {
    std::printf("damage-register:%u:%.2f\n", count,
                static_cast<double>(threshold));
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto& d = descriptors[index];
        std::printf("damage:%u:%u:%s:%.2f:%.2f:%.2f:%.2f:%.2f\n",
                    d.system_index, d.threshold_index, d.texture_path,
                    static_cast<double>(d.offset[0]),
                    static_cast<double>(d.offset[1]),
                    static_cast<double>(d.offset[2]),
                    static_cast<double>(d.size[0]),
                    static_cast<double>(d.size[1]));
    }
    return 1;
}
static int __cdecl mock_register_logo(
    void*, const LogoDecalDescriptor* descriptors, std::uint32_t count) {
    std::printf("logo-register:%u\n", count);
    for (std::uint32_t index = 0; index < count; ++index) {
        const auto& d = descriptors[index];
        std::printf("logo:%u:%u:%u:%.2f:%.2f\n",
                    d.texture_path_count, d.colour_key, d.flip_u,
                    static_cast<double>(d.offset[0]),
                    static_cast<double>(d.size[0]));
        for (std::uint32_t path = 0; path < d.texture_path_count; ++path) {
            std::printf("logo-path:%s\n", d.texture_paths[path] ?
                        d.texture_paths[path] : "<empty>");
        }
    }
    return 1;
}
static int __cdecl mock_emissive_class(
    void*, const char* const* paths, std::uint32_t count) {
    std::printf("emissive-class:%u\n", count);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::printf("emissive-class-path:%s\n",
                    paths[index] ? paths[index] : "<empty>");
    }
    return 1;
}
static int __cdecl mock_emissive_materials(
    void*, const char* const* names, const char* const* paths,
    std::uint32_t count, std::uint32_t per_material) {
    std::printf("emissive-materials:%u:%u\n", count, per_material);
    for (std::uint32_t index = 0; index < count; ++index) {
        std::printf("emissive-name:%s\n", names[index]);
        for (std::uint32_t system = 0; system < per_material; ++system) {
            const char* path = paths[index * per_material + system];
            std::printf("emissive-path:%s\n", path ? path : "<empty>");
        }
    }
    return 1;
}

struct MockNode {
    std::uint8_t padding0[8]{};
    const char* name = nullptr;
    std::uint8_t padding1[0x10]{};
    void* sibling = nullptr;
    void* child = nullptr;
};
static_assert(offsetof(MockNode, name) == 8);
static_assert(offsetof(MockNode, sibling) == 0x1c);
static_assert(offsetof(MockNode, child) == 0x20);
struct MockGeometry {
    std::uint8_t padding[0x3c]{};
    void* root = nullptr;
};
static_assert(offsetof(MockGeometry, root) == 0x3c);
struct MockCraft {
    std::uint8_t padding[0x1d8]{};
    void* geometry = nullptr;
};
static_assert(offsetof(MockCraft, geometry) == 0x1d8);
struct MockSceneMesh {
    void** vtable = nullptr;
    std::uint32_t padding4 = 0;
    const char* name = nullptr;
    std::uint8_t padding12[0x10]{};
    void* sibling = nullptr;
    void* child = nullptr;
    std::uint8_t padding24[0x104]{};
    void* renderer = nullptr;
    std::uint32_t flags = 0;
    void* diffuse = nullptr;
    void* bump = nullptr;
};
static_assert(offsetof(MockSceneMesh, name) == 8);
static_assert(offsetof(MockSceneMesh, sibling) == 0x1c);
static_assert(offsetof(MockSceneMesh, child) == 0x20);
static_assert(offsetof(MockSceneMesh, renderer) == 0x128);
static std::uintptr_t __attribute__((thiscall)) mock_scene_type(
    MockSceneMesh*) { return 1; }
static std::uintptr_t __attribute__((thiscall)) mock_scene_get(
    MockSceneMesh* mesh, std::uintptr_t slot) {
    std::printf("scene-get:%u\n", static_cast<unsigned>(slot));
    return reinterpret_cast<std::uintptr_t>(slot == 0 ? mesh->diffuse :
                                            mesh->bump);
}

struct MockGraphics {
    std::uint8_t padding[0x104]{};
    void* options = nullptr;
};
static_assert(offsetof(MockGraphics, options) == 0x104);
struct MockOptions {
    std::uint8_t padding[5]{};
    std::uint8_t disable_dot3 = 1;
};
static_assert(offsetof(MockOptions, disable_dot3) == 5);
static bool A2FO_CALL mock_patch_bytes(
    void*, const std::uint8_t* replacement,
    const std::uint8_t* expected, std::size_t size) {
    std::printf("neutral-patch:%u:%u:%u\n",
                static_cast<unsigned>(size),
                static_cast<unsigned>(*expected),
                static_cast<unsigned>(*replacement));
    return true;
}
static void* g_fixture_armada = nullptr;
static void A2FO_CALL mock_module_log(const char* module,
                                      const char* message) {
    std::printf("module-log:%s:%s\n", module, message);
}
static void* A2FO_CALL mock_armada_module() {
    std::puts("module-armada");
    return g_fixture_armada;
}
static std::uint32_t A2FO_CALL mock_extension_root_count() {
    std::puts("module-root-count");
    return 2;
}
static const char* A2FO_CALL mock_extension_root(std::uint32_t index) {
    std::printf("module-root:%u\n", index);
    return index == 0 ? "nebula-base" : "nebula-mod";
}
static bool A2FO_CALL mock_race_subscribe(
    const char*, const char* const*, std::uint32_t count,
    A2FO_RaceLoadedHandler, void*) {
    std::printf("module-race-subscribe:%u\n", count);
    return true;
}

static void install_callable(std::uint8_t* image, std::uintptr_t rva,
                             void* function) {
    image[rva] = 0xb8;
    std::memcpy(image + rva + 1, &function, sizeof(function));
    image[rva + 5] = 0xff;
    image[rva + 6] = 0xe0;
}

int main() {
    CreateDirectoryA("nebula-base", nullptr);
    CreateDirectoryA("nebula-base\\Textures", nullptr);
    CreateDirectoryA("nebula-base\\Textures\\RGB", nullptr);
    CreateDirectoryA("nebula-mod", nullptr);
    CreateDirectoryA("nebula-mod\\Textures", nullptr);
    CreateDirectoryA("nebula-mod\\Textures\\RGB", nullptr);
    DeleteFileA("nebula-mod\\Textures\\missing.dds");
    create_file("nebula-config-small.txt");
    FILE* large_config = std::fopen("nebula-config-large.txt", "wb");
    if (!large_config) std::abort();
    std::fseek(large_config, 2 * 1024 * 1024, SEEK_SET);
    std::fputc('x', large_config);
    std::fclose(large_config);
    std::string config_contents;
    std::printf("config-small:%u\n",
                read_small_text_config("nebula-config-small.txt",
                                       &config_contents));
    show("config-body", config_contents);
    std::printf("config-large:%u\n",
                read_small_text_config("nebula-config-large.txt",
                                       &config_contents));
    create_file("nebula-base\\Textures\\RGB\\ship.dds");
    create_file("nebula-mod\\Textures\\ship.tga");
    create_file("nebula-base\\Textures\\explicit.png");
    g_extension_roots = {"nebula-base", "nebula-mod"};
    create_text("nebula-base\\ART_CFG.h",
                "#define A2FO_EMISSIVE_SUFFIX \"_emissive_\"\n");
    create_text("nebula-mod\\ART_CFG.h",
                "#define A2FO_SPECULAR_SUFFIX \"_specular\"\n");
    load_art_texture_suffix_config();
    std::printf("art:%s:%s\n",
                g_art_texture_suffix_config.emissive_suffix.c_str(),
                g_art_texture_suffix_config.specular_suffix.c_str());
    CreateDirectoryA("nebula-base\\settings_saves_logs", nullptr);
    create_text("nebula-base\\settings_saves_logs\\Settings.xml",
                "<disable_bump>true</disable_bump>");
    bool disabled = false;
    const bool known = load_native_bump_disabled(&disabled);
    std::printf("native-bump-setting:%u:%u\n", known, disabled);
    create_text("nebula-base\\A2FORenderer.ini",
                "[Compatibility]\nNeutralBumpWhenDisabled=0\n");
    load_neutral_bump_compatibility_policy();
    std::printf("neutral-policy:%u\n",
                g_neutral_bump_when_disabled_requested);
    show("reverse-root", resolve_texture(" \tship "));
    DeleteFileA("nebula-mod\\Textures\\ship.tga");
    show("cache-hit", resolve_texture("ship"));
    show("prefixed", resolve_texture("textures/explicit"));
    // Frozen trim() normalizes both spellings before precedence/cache lookup.
    // The root-relative file must win over the competing generic path.
    CreateDirectoryA("nebula-mod\\Textures\\textures", nullptr);
    create_file("nebula-mod\\Textures\\textures\\priority.dds");
    create_file("nebula-mod\\Textures\\priority.tga");
    show("slash-priority", resolve_texture("textures/priority"));
    show("backslash-priority", resolve_texture("textures\\priority"));
    DeleteFileA("nebula-mod\\Textures\\textures\\priority.dds");
    DeleteFileA("nebula-mod\\Textures\\priority.tga");
    show("slash-cache", resolve_texture("textures/priority"));
    show("backslash-cache", resolve_texture("textures\\priority"));
    show("missing", resolve_texture("missing"));
    create_file("nebula-mod\\Textures\\missing.dds");
    show("missing-cache", resolve_texture("missing"));
    char absolute[MAX_PATH]{};
    GetFullPathNameA("nebula-base\\Textures\\explicit.png",
                     MAX_PATH, absolute, nullptr);
    show("absolute", resolve_texture(absolute));
    const char race_command[] = "FactionTextureSuffix";
    const char race_suffix[] = " _K ";
    A2FO_OdfFieldView race_fields[]{{
        {"unrelated", 9}, {"ignored", 7}},
        {{race_command, static_cast<std::uint32_t>(
                            std::strlen(race_command))},
         {race_suffix, static_cast<std::uint32_t>(
                           std::strlen(race_suffix))}}};
    int race_token = 0;
    A2FO_RaceLoadedEvent race_event{
        sizeof(A2FO_RaceLoadedEvent), &race_token, race_fields, 2};
    g_register_faction_texture_suffix = mock_faction_suffix;
    race_loaded_handler(&race_event, nullptr);
    const auto race_stored = g_race_texture_suffixes.find(&race_token);
    if (race_stored == g_race_texture_suffixes.end()) std::abort();
    std::printf("race-stored:%s\n",
                race_stored->second.c_str());
    race_event.odf_fields = nullptr;
    race_event.odf_field_count = 0;
    race_loaded_handler(&race_event, nullptr);
    std::printf("race-cleared:%u\n",
                static_cast<unsigned>(g_race_texture_suffixes.count(
                    &race_token)));
    g_race_texture_suffixes[reinterpret_cast<void*>(1)] = "_B";
    g_race_texture_suffixes[reinterpret_cast<void*>(2)] = "_long";
    g_race_texture_suffixes[reinterpret_cast<void*>(3)] = "_b";
    g_race_texture_suffixes[reinterpret_cast<void*>(4)] = "_K";
    for (const std::string& suffix : faction_texture_suffixes()) {
        show("suffix", suffix);
    }
    for (const char* diffuse : {
             "Textures\\ship_b.dds", "ship_LONG.tga", "ship_K",
             "folder\\ship.dds", "empty\\", "ship._b"}) {
        const FactionDiffuseName split = split_faction_diffuse_name(diffuse);
        std::printf("split:%s:%s:%s\n", diffuse,
                    split.base_diffuse.c_str(), split.faction_suffix.c_str());
        show("key", normalized_material_key(diffuse));
    }
    create_file("nebula-mod\\Textures\\ship_specular.dds");
    create_file("nebula-base\\Textures\\ship_specular_b.dds");
    create_file("nebula-mod\\Textures\\ship_specular_K.tga");
    g_art_texture_suffix_config.specular_suffix = "_specular";
    index_loose_texture_names();
    std::printf("index:%u:%u:%u\n",
                static_cast<unsigned>(g_loose_texture_keys.size()),
                g_loose_texture_keys.count("ship_specular_b"),
                g_loose_texture_keys.count("ship_specular_k"));
    g_loose_texture_keys = {
        "ship_specular", "ship_specular_b", "ship_specular_k"};
    g_register_specular_materials = mock_specular;
    const std::vector<ClassMeshMaterial> materials{
        {nullptr, nullptr, "ship.dds"},
        {nullptr, nullptr, "ship_b.dds"},
        {nullptr, nullptr, "ship.dds"},
        {nullptr, nullptr, "ship_missing.dds"}};
    register_global_specular(reinterpret_cast<void*>(1), materials);
    g_throw_specular = true;
    register_global_specular(reinterpret_cast<void*>(1), materials);
    g_throw_specular = false;
    std::puts("specular:exception-contained");
    MockTexture diffuse_a{};
    diffuse_a.name = "a.dds";
    diffuse_a.flags = 4;
    MockTexture diffuse_b{};
    diffuse_b.name = "b.dds";
    diffuse_b.flags = 8;
    MockMesh mesh_a{};
    mesh_a.diffuse = &diffuse_a;
    MockMesh mesh_b{};
    mesh_b.diffuse = &diffuse_b;
    mesh_b.fail_update = true;
    MockMesh mesh_existing{};
    mesh_existing.diffuse = &diffuse_b;
    mesh_existing.bump = &diffuse_a;
    g_mesh_get_texture = reinterpret_cast<void*>(&mock_mesh_get);
    g_mesh_set_texture = reinterpret_cast<void*>(&mock_mesh_set);
    g_mesh_update = reinterpret_cast<void*>(&mock_mesh_update);
    g_texture_file_exists = mock_native_file_exists;
    g_texture_find = mock_texture_find;
    g_mesh_texture_runtime_supported = true;
    g_art_texture_suffix_config.bump_suffix = "_bump";
    const std::vector<ClassMeshMaterial> bump_materials{
        {&mesh_a, &diffuse_a, "a.dds"},
        {&mesh_b, &diffuse_b, "b.dds"},
        {&mesh_existing, &diffuse_b, "b.dds"},
        {nullptr, &diffuse_a, "a.dds"}};
    apply_global_bump_suffix(bump_materials);
    std::printf("bump-state:%u:%u:%u:%u\n", mesh_a.flags,
                mesh_a.bump != nullptr, mesh_b.flags, mesh_b.bump != nullptr);
    mesh_a.bump = mesh_b.bump = nullptr;
    mesh_a.flags = mesh_b.flags = 0;
    g_mutate_bump = true;
    g_mutate_mesh = &mesh_a;
    apply_global_bump_suffix(bump_materials);
    std::printf("bump-mutated-state:%u:%u:%u:%u:%s\n", mesh_a.flags,
                mesh_a.bump != nullptr, mesh_b.flags, mesh_b.bump != nullptr,
                g_art_texture_suffix_config.bump_suffix.c_str());
    g_art_texture_suffix_config.bump_suffix = "_bump";
    auto* image = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, 0x00400000, MEM_COMMIT | MEM_RESERVE,
        PAGE_EXECUTE_READWRITE));
    if (!image) std::abort();
    g_armada = reinterpret_cast<HMODULE>(image);
    install_callable(image, kParameterDbGetStringRva,
                     reinterpret_cast<void*>(&mock_parameter_string));
    install_callable(image, kParameterDbGetFloatRva,
                     reinterpret_cast<void*>(&mock_parameter_float));
    install_callable(image, kParameterDbGetStringVectorRva,
                     reinterpret_cast<void*>(&mock_parameter_vector));
    install_callable(image, kEngineOperatorDeleteRva,
                     reinterpret_cast<void*>(&mock_engine_delete));
    MockNode node{};
    node.name = "hp";
    MockGeometry geometry{};
    geometry.root = &node;
    MockCraft craft{};
    craft.geometry = &geometry;
    create_file("nebula-mod\\Textures\\scorch.dds");
    g_register_damage_decal_class = mock_register_damage;
    int parameter_db = 0;
    register_damage_decals(&craft, &parameter_db);
    g_parameter_mode = 1;
    register_damage_decals(&craft, &parameter_db);
    create_file("nebula-mod\\Textures\\flag_x.dds");
    g_register_logo_decal_class = mock_register_logo;
    g_parameter_mode = 0;
    register_logo_decals(&craft, &parameter_db);
    g_parameter_mode = 2;
    register_logo_decals(&craft, &parameter_db);
    create_file("nebula-mod\\Textures\\warp.dds");
    create_file("nebula-mod\\Textures\\ship_emissive_warp.dds");
    create_file("nebula-mod\\Textures\\ship_emissive_warp_b.dds");
    create_file("nebula-base\\Textures\\ship_emissive_impulse.dds");
    g_art_texture_suffix_config.emissive_suffix = "_emissive_";
    g_art_texture_suffix_config.bump_suffix.clear();
    index_loose_texture_names();
    MockTexture diffuse_ship{};
    diffuse_ship.name = "ship.dds";
    MockSceneMesh scene{};
    void* scene_vtable[2]{nullptr,
                           reinterpret_cast<void*>(&mock_scene_type)};
    scene.vtable = scene_vtable;
    scene.name = "hp";
    scene.diffuse = &diffuse_ship;
    MockGeometry class_geometry{};
    class_geometry.root = &scene;
    MockCraft class_craft{};
    class_craft.geometry = &class_geometry;
    g_mesh_get_texture = reinterpret_cast<void*>(&mock_scene_get);
    g_register_damage_decal_class = nullptr;
    g_register_emissive_class = mock_emissive_class;
    g_register_emissive_materials = mock_emissive_materials;
    g_core_renderer_available = true;
    g_parameter_mode = 3;
    std::puts("class-wildcard");
    A2FONebulaRenderer_RegisterClass(&class_craft, &parameter_db);
    g_parameter_mode = 4;
    std::puts("class-indexed");
    A2FONebulaRenderer_RegisterClass(&class_craft, &parameter_db);
    std::puts("class-late");
    A2FONebulaRenderer_RegisterClass(&class_craft, nullptr);
    MockOptions options{};
    MockGraphics graphics{};
    graphics.options = &options;
    void* graphics_pointer = &graphics;
    std::memcpy(image + kGraphicsEnginePointerRva,
                &graphics_pointer, sizeof(graphics_pointer));
    g_neutral_bump_when_disabled_requested = true;
    g_native_bump_disabled = true;
    g_core_renderer_available = true;
    g_fast_nonbump_core_enabled = true;
    g_api = nullptr;
    const bool fallback_armed = arm_neutral_bump_compatibility();
    std::printf("neutral-fallback:%u:%u\n",
                fallback_armed, options.disable_dot3);
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.patch_bytes = mock_patch_bytes;
    g_api = &api;
    std::printf("neutral-patched:%u\n",
                arm_neutral_bump_compatibility());
    g_api = nullptr;
    HMODULE core = LoadLibraryA("A2FOExtensions.dll");
    if (!core) std::abort();
    using SetReject = void (__cdecl*)(int);
    const SetReject set_reject = imported_function<SetReject>(
        core, "A2FOFixture_SetReject");
    if (!set_reject) std::abort();
    std::memcpy(image + kMeshGetTextureRva,
                kExpectedMeshGetTexture.data(),
                kExpectedMeshGetTexture.size());
    std::memcpy(image + kMeshSetTextureRva,
                kExpectedMeshSetTexture.data(),
                kExpectedMeshSetTexture.size());
    std::memcpy(image + kMeshUpdateRva,
                kExpectedMeshUpdate.data(), kExpectedMeshUpdate.size());
    std::memcpy(image + kTextureFindRva,
                kExpectedTextureFind.data(), kExpectedTextureFind.size());
    WritePrivateProfileStringA("Compatibility",
                               "NeutralBumpWhenDisabled", "1",
                               "nebula-base\\A2FORenderer.ini");
    g_fixture_armada = image;
    A2FO_ModuleApi module_api{};
    module_api.struct_size = sizeof(module_api);
    module_api.api_version = A2FO_MODULE_API_VERSION;
    module_api.log = mock_module_log;
    module_api.armada_module = mock_armada_module;
    module_api.extension_root_count = mock_extension_root_count;
    module_api.extension_root = mock_extension_root;
    module_api.capabilities = A2FO_CAP_RACE_LOADED;
    module_api.register_race_loaded_handler = mock_race_subscribe;
    module_api.patch_bytes = mock_patch_bytes;
    std::puts("init-success");
    const bool init_success = A2FO_ModuleInit(&module_api);
    std::printf("init-result:%u:%u:%u\n", init_success,
                g_core_renderer_available,
                g_fast_nonbump_core_enabled);
    A2FO_ModuleShutdown();
    std::printf("shutdown-state:%u:%u:%u\n",
                g_core_renderer_available,
                g_fast_nonbump_core_enabled,
                static_cast<unsigned>(g_extension_roots.size()));
    set_reject(1);
    std::puts("init-rejected");
    const bool init_rejected = A2FO_ModuleInit(&module_api);
    std::printf("init-rejected-result:%u\n", init_rejected);
    A2FO_ModuleShutdown();
    FreeLibrary(core);
    VirtualFree(image, 0, MEM_RELEASE);
    g_armada = nullptr;
    return 0;
}
