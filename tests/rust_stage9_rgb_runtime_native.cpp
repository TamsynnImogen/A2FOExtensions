#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <iostream>
#include <new>
#include <string>
#include <vector>

static bool fail_next_allocation = false;
static bool fail_second_root = false;
static int exception_destructors = 0;
struct FixtureAllocationFailure final : std::bad_alloc {
    ~FixtureAllocationFailure() noexcept override { ++exception_destructors; }
};
void* operator new(std::size_t size) {
    if (fail_next_allocation) {
        fail_next_allocation = false;
        throw FixtureAllocationFailure{};
    }
    if (void* memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc{};
}
void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }

#include MODULE_SOURCE

extern "C" std::uintptr_t a2fo_rgb_call_thiscall_0(void*, void*) { return 0; }
extern "C" std::uintptr_t a2fo_rgb_call_thiscall_1(
    void*, void*, std::uintptr_t) { return 0; }
extern "C" std::uintptr_t a2fo_rgb_call_thiscall_2(
    void*, void*, std::uintptr_t, std::uintptr_t) { return 0; }
extern "C" std::uintptr_t a2fo_rgb_call_thiscall_4(
    void*, void*, std::uintptr_t, std::uintptr_t,
    std::uintptr_t, std::uintptr_t) { return 0; }

static void check(bool condition) {
    if (!condition) std::exit(2);
}

static void header_file(const char* path, std::uint16_t width,
                        std::uint16_t height, std::uint8_t pixel_depth = 24) {
    unsigned char bytes[18]{};
    bytes[2] = 2;
    bytes[12] = static_cast<unsigned char>(width);
    bytes[13] = static_cast<unsigned char>(width >> 8);
    bytes[14] = static_cast<unsigned char>(height);
    bytes[15] = static_cast<unsigned char>(height >> 8);
    bytes[16] = pixel_depth;
    FILE* file = std::fopen(path, "wb");
    check(file != nullptr);
    check(std::fwrite(bytes, 1, sizeof(bytes), file) == sizeof(bytes));
    std::fclose(file);
}

static void pixel_file(const char* path, std::uint16_t width,
                       std::uint16_t height) {
    header_file(path, width, height);
    FILE* file = std::fopen(path, "ab");
    check(file != nullptr);
    for (std::size_t index = 0; index <
         static_cast<std::size_t>(width) * height * 3; ++index) {
        std::fputc(static_cast<int>(index & 0xffu), file);
    }
    std::fclose(file);
}

int main() {
    const std::vector<std::pair<const char*, const char*>> requests = {
        {"Textures\\RGB\\Foo.TGA", "rb"},
        {".\\.\\Textures\\Index8\\foo.tga", "r"},
        {"x/Textures/Compressed/x.tga", "rb"},
        {"C:\\mods\\Textures\\DDS\\F_Akira_A.dds", "rb"},
        {"foo.tga", "rb"}, {"foo.bmp", "rb"},
        {"Textures\\RGB\\", "rb"},
        {"Textures\\RGB\\foo.tga", "wb"},
        {"x\\textures\\y\\textures\\z.dds", "r"},
    };
    for (const auto& request : requests) {
        std::string key = "untouched";
        const auto route = texture_request(request.first, request.second, key);
        std::cout << "request:" << static_cast<int>(route) << ':' << key << '\n';
    }
    const std::vector<std::string> mips = {
        "foo_1.tga", "foo_8.tga", "foo_0.tga", "foo_9.tga",
        "foo_01.tga", "foo_123.tga", "path\\foo_2.tga",
        "path\\_2.tga", "foo_1.dds", "foo_-1.tga", "foo_1.TGA",
        "foo_1_2.tga", "foo_1.tga\\x", "a_1.tga",
    };
    for (const auto& value : mips) {
        std::string stem = "old";
        std::uint32_t level = 99;
        bool valid = parse_manual_mip_key(value, stem, level);
        std::cout << "mip:" << valid << ':' << stem << ':' << level << '\n';
    }
    std::cout << "directory:" << is_legacy_texture_directory("rGb")
              << ':' << is_legacy_texture_directory("DDS") << '\n';
    std::uint8_t texture[64]{};
    for (const char* value : {
             ".\\Textures\\RGB\\Foo.dds", "Textures/Index8/Foo",
             "Compressed\\Bar.tga", "other.dds"}) {
        std::memcpy(texture + 8, &value, sizeof(value));
        std::string name, key;
        const bool ok = texture_name_and_key(texture, name, key);
        std::cout << "texture-key:" << ok << ':' << key << '\n';
    }
    const char* alias_source = "Textures\\Compressed\\Shared.dds";
    std::memcpy(texture + 8, &alias_source, sizeof(alias_source));
    std::string aliased = "prior";
    const bool alias_key = texture_name_and_key(texture, aliased, aliased);
    std::cout << "texture-key-alias:" << alias_key << ':' << aliased << '\n';
    const char* absent_source = nullptr;
    std::memcpy(texture + 8, &absent_source, sizeof(absent_source));
    std::string absent_name = "prior-name", absent_key = "prior-key";
    const bool absent = texture_name_and_key(
        texture, absent_name, absent_key);
    std::cout << "texture-key-absent:" << absent << ':' << absent_name
              << ':' << absent_key << '\n';
    const std::string long_texture_name(90, 'Q');
    const char* long_source = long_texture_name.c_str();
    std::memcpy(texture + 8, &long_source, sizeof(long_source));
    std::string failed_name, failed_key = "prior-key";
    fail_next_allocation = true;
    try {
        texture_name_and_key(texture, failed_name, failed_key);
        std::cout << "texture-key-allocation:none\n";
    } catch (const FixtureAllocationFailure&) {
        std::cout << "texture-key-allocation:typed:" << failed_name.size()
                  << ':' << failed_key << '\n';
    } catch (...) {
        std::cout << "texture-key-allocation:other\n";
    }
    g_rgb_files.clear(); g_index8_files.clear(); g_compressed_files.clear();
    for (int owner = 0; owner < 4; ++owner) {
        if (owner == 0) g_rgb_files["same.tga"] = "rgb";
        if (owner == 1) g_index8_files["same.tga"] = "index8";
        if (owner == 2) g_compressed_files["same.tga"] = "compressed";
        std::cout << "key-exists:" << owner << ':'
                  << legacy_texture_key_exists("same.tga") << ':'
                  << legacy_texture_key_exists("missing.tga") << '\n';
        g_rgb_files.clear(); g_index8_files.clear();
        g_compressed_files.clear();
    }
    for (unsigned code : {0u, 1u, 2u, 3u, 4u, 99u}) {
        const auto request = static_cast<TextureRequest>(code);
        const auto route = static_cast<RootTgaRoute>(code);
        std::cout << "labels:" << code << ':'
                  << (direct_legacy_files(request) != nullptr) << ':'
                  << direct_legacy_folder_name(request) << ':'
                  << root_tga_route_name(route) << '\n';
    }

    header_file("base.tga", 64, 32);
    header_file("base_1.tga", 32, 16);
    header_file("base_2.tga", 16, 8);
    std::unordered_map<std::string, std::string> files = {
        {"base.tga", "base.tga"}, {"base_1.tga", "base_1.tga"},
        {"base_2.tga", "base_2.tga"},
    };
    std::cout << "chain:valid:"
              << !validated_manual_mip_base(files, "base_2.tga").empty() << '\n';
    header_file("base_2.tga", 16, 16);
    std::cout << "chain:aspect:"
              << !validated_manual_mip_base(files, "base_2.tga").empty() << '\n';
    header_file("base_2.tga", 32, 16);
    std::cout << "chain:decrease:"
              << !validated_manual_mip_base(files, "base_2.tga").empty() << '\n';
    header_file("base_2.tga", 16, 8, 32);
    std::cout << "chain:format:"
              << !validated_manual_mip_base(files, "base_2.tga").empty() << '\n';
    files.erase("base_1.tga");
    header_file("base_2.tga", 16, 8);
    std::cout << "chain:missing:"
              << !validated_manual_mip_base(files, "base_2.tga").empty() << '\n';

    std::vector<std::uint8_t> source(18, 0), output;
    source[2] = 3; source[12] = 2; source[14] = 1; source[16] = 8;
    source.push_back(13); source.push_back(29);
    auto result = prepare_tga_for_rgb_loader(source, output);
    std::cout << "gray:" << static_cast<int>(result) << ':' << output.size();
    for (std::size_t index = 18; index < output.size(); ++index)
        std::cout << ':' << static_cast<int>(output[index]);
    std::cout << '\n';

    source.assign(18, 0);
    source[1] = 1; source[2] = 1; source[5] = 2;
    source[7] = 24; source[12] = 2; source[14] = 1; source[16] = 8;
    source.insert(source.end(), {1, 2, 3, 4, 5, 6, 0, 1});
    result = prepare_tga_for_rgb_loader(source, output);
    std::cout << "palette:" << static_cast<int>(result) << ':' << output.size();
    for (std::size_t index = 18; index < output.size(); ++index)
        std::cout << ':' << static_cast<int>(output[index]);
    std::cout << '\n';
    source.back() = 2;  // Palette index outside the two-entry table.
    result = prepare_tga_for_rgb_loader(source, output);
    std::cout << "palette-index:" << static_cast<int>(result) << ':'
              << output.size() << '\n';
    source.pop_back();  // Truncated index stream after the first pixel.
    result = prepare_tga_for_rgb_loader(source, output);
    std::cout << "palette-truncated:" << static_cast<int>(result) << ':'
              << output.size() << '\n';

    source.assign(18, 0);
    source[2] = 11; source[12] = 3; source[14] = 1; source[16] = 8;
    source.insert(source.end(), {0x81, 7, 0, 9});
    result = prepare_tga_for_rgb_loader(source, output);
    std::cout << "rle:" << static_cast<int>(result) << ':' << output.size();
    for (std::size_t index = 18; index < output.size(); ++index)
        std::cout << ':' << static_cast<int>(output[index]);
    std::cout << '\n';
    source.pop_back();
    result = prepare_tga_for_rgb_loader(source, output);
    std::cout << "truncated:" << static_cast<int>(result) << ':'
              << output.size() << '\n';
    source.push_back(9);
    source[18] = 0x83;  // Packet expands beyond declared 3-pixel image.
    result = prepare_tga_for_rgb_loader(source, output);
    std::cout << "rle-overrun:" << static_cast<int>(result) << ':'
              << output.size() << '\n';

    source.assign(18, 0);
    source[2] = 3; source[12] = 2; source[14] = 1; source[16] = 8;
    source.insert(source.end(), {13, 29});
    std::vector<std::uint8_t> exception_output;
    fail_next_allocation = true;
    try {
        prepare_tga_for_rgb_loader(source, exception_output);
        std::cout << "exception:none\n";
    } catch (const FixtureAllocationFailure&) {
        std::cout << "exception:typed:" << exception_destructors << '\n';
    } catch (...) {
        std::cout << "exception:other\n";
    }
    std::cout << "exception:after:" << exception_destructors << '\n';

    source.assign(18, 0);
    source[2] = 2; source[12] = 4; source[14] = 2; source[16] = 24;
    for (int pixel = 0; pixel < 8; ++pixel) {
        source.push_back(static_cast<std::uint8_t>(pixel * 10));
        source.push_back(static_cast<std::uint8_t>(pixel * 10 + 1));
        source.push_back(static_cast<std::uint8_t>(pixel * 10 + 2));
    }
    const bool resized = resize_true_colour_tga(source, 2, 1, output);
    std::cout << "resize:" << resized << ':' << output.size();
    for (std::size_t index = 18; index < output.size(); ++index)
        std::cout << ':' << static_cast<int>(output[index]);
    std::cout << '\n';
    std::cout << "resize-invalid:"
              << resize_true_colour_tga(source, 3, 1, output) << ':'
              << output.size() << '\n';
    std::vector<std::uint8_t> alias = source;
    result = prepare_tga_for_rgb_loader(alias, alias);
    std::cout << "prepare-alias:" << static_cast<int>(result) << ':'
              << alias.size() << '\n';
    alias = source;
    const bool alias_resized = resize_true_colour_tga(alias, 2, 1, alias);
    std::cout << "resize-alias:" << alias_resized << ':'
              << alias.size() << '\n';

    pixel_file("parent_1.tga", 2, 2);
    pixel_file("child.tga", 16, 16);
    g_flattened_tga_candidates["craft.tga"].push_back(
        {"child.tga", {}, RootTgaRoute::root, 1});
    g_flattened_tga_candidates["craft_1.tga"].push_back(
        {"parent_1.tga", "parent.tga", RootTgaRoute::rgb, 0});
    InitializeCriticalSection(&g_conversion_lock);
    g_conversion_lock_ready = true;
    std::string physical;
    RootTgaRoute route = RootTgaRoute::none;
    bool selected = select_root_tga("craft_1.tga", physical, route);
    TgaHeaderSummary summary{};
    check(selected && read_tga_header_summary(physical, summary));
    std::cout << "synth:" << static_cast<int>(route) << ':'
              << summary.width << 'x' << summary.height << '\n';
    const std::string cached = physical;
    FILE* changed_base = std::fopen("child.tga", "r+b");
    check(changed_base != nullptr);
    check(std::fseek(changed_base, 18, SEEK_SET) == 0);
    check(std::fputc(255, changed_base) != EOF);
    std::fclose(changed_base);
    selected = select_root_tga("craft_1.tga", physical, route);
    std::cout << "synth-cache:" << selected << ':' << (cached == physical)
              << ':' << g_generated_texture_files.size() << '\n';
    pixel_file("broken.tga", 16, 16);
    // The effective child base passes header admission but its truncated
    // payload fails synthesis; the undersized inherited image stays rejected.
    header_file("broken.tga", 16, 16);
    g_flattened_tga_candidates["broken.tga"].push_back(
        {"broken.tga", {}, RootTgaRoute::root, 1});
    g_flattened_tga_candidates["broken_1.tga"].push_back(
        {"parent_1.tga", "parent.tga", RootTgaRoute::rgb, 0});
    selected = select_root_tga("broken_1.tga", physical, route);
    std::cout << "synth-failure:" << selected << ':'
              << static_cast<int>(route) << ':' << physical.empty() << '\n';
    g_rgb_files["child.tga"] = "child.tga";
    bool mapped = mapped_tga_path("Textures\\RGB\\child.tga", physical);
    std::cout << "mapped-rgb:" << mapped << ':'
              << (physical == "child.tga") << '\n';
    g_root_texture_files["unmapped.dds"] = "child.tga";
    mapped = mapped_tga_path("Textures\\RGB\\unmapped.tga", physical);
    std::cout << "mapped-dds-fallback:" << mapped << ':'
              << (physical == "child.tga") << '\n';
    g_original_fopen = &std::fopen;
    FILE* routed = legacy_texture_fopen("Textures\\RGB\\child.tga", "rb");
    std::cout << "fopen-rgb:" << (routed != nullptr) << '\n';
    if (routed) std::fclose(routed);
    routed = legacy_texture_fopen("Textures\\unmapped.dds", "rb");
    std::cout << "fopen-dds:" << (routed != nullptr) << '\n';
    if (routed) std::fclose(routed);
    source.assign(18, 0);
    source[2] = 3; source[12] = 2; source[14] = 1; source[16] = 8;
    source.insert(source.end(), {13, 29});
    FILE* gray_file = std::fopen("gray.tga", "wb");
    check(gray_file != nullptr);
    check(std::fwrite(source.data(), 1, source.size(), gray_file) ==
          source.size());
    std::fclose(gray_file);
    std::string prepared_gray;
    const bool gray_prepared = prepare_legacy_tga("gray.tga", prepared_gray);
    std::string cached_gray;
    const bool gray_cached = prepare_legacy_tga("gray.tga", cached_gray);
    check(gray_prepared && gray_cached && prepared_gray == cached_gray);
    std::cout << "prepare-cache:" << gray_prepared << ':' << gray_cached
              << ':' << (prepared_gray == cached_gray) << ':'
              << g_generated_texture_files.size() << '\n';
    source[2] = 99;
    gray_file = std::fopen("bad.tga", "wb");
    check(gray_file != nullptr);
    check(std::fwrite(source.data(), 1, source.size(), gray_file) ==
          source.size());
    std::fclose(gray_file);
    std::string bad_path;
    const bool bad_first = prepare_legacy_tga("bad.tga", bad_path);
    const bool bad_second = prepare_legacy_tga("bad.tga", bad_path);
    check(!bad_first && !bad_second &&
          g_unusable_texture_files.count("bad.tga") == 1);
    std::cout << "prepare-bad:" << bad_first << ':' << bad_second << ':'
              << g_unusable_texture_files.count("bad.tga") << '\n';
    clear_conversion_cache();

    CreateDirectoryA("low", nullptr);
    CreateDirectoryA("low\\Textures", nullptr);
    CreateDirectoryA("low\\Textures\\RGB", nullptr);
    CreateDirectoryA("high", nullptr);
    CreateDirectoryA("high\\Textures", nullptr);
    CreateDirectoryA("high\\Textures\\Index8", nullptr);
    CreateDirectoryA("high\\Textures\\DDS", nullptr);
    pixel_file("low\\Textures\\RGB\\ship.tga", 8, 8);
    pixel_file("high\\Textures\\Index8\\ship.tga", 4, 4);
    header_file("high\\Textures\\DDS\\legacy.dds", 4, 4);
    A2FO_ModuleApi api{};
    api.extension_root_count = []() -> std::uint32_t { return 2; };
    api.extension_root = [](std::uint32_t index) -> const char* {
        if (fail_second_root && index == 1) throw FixtureAllocationFailure{};
        return index == 0 ? "low" : "high";
    };
    g_api = &api;
    LegacyTextureDiscovery discovery{};
    bool discovered = discover_legacy_texture_files(discovery);
    const auto candidates = g_flattened_tga_candidates.find("ship.tga");
    check(discovered && discovery.roots_found == 2 &&
          discovery.rgb_roots == 1 && discovery.index8_roots == 1 &&
          candidates != g_flattened_tga_candidates.end() &&
          candidates->second.size() == 2 &&
          g_root_texture_files.count("legacy.dds") == 1);
    std::cout << "discovery:" << discovered << ':' << discovery.roots_found
              << ':' << discovery.rgb_roots << ':' << discovery.index8_roots
              << ':' << discovery.compressed_roots << ':'
              << (candidates == g_flattened_tga_candidates.end()
                      ? 0 : candidates->second.size()) << ':'
              << (g_root_texture_files.count("legacy.dds") != 0) << '\n';
    g_rgb_files.clear(); g_index8_files.clear();
    g_compressed_files.clear(); g_root_texture_files.clear();
    g_flattened_tga_candidates.clear();
    fail_second_root = true;
    exception_destructors = 0;
    try {
        discover_legacy_texture_files(discovery);
        std::cout << "discovery-exception:none\n";
    } catch (const FixtureAllocationFailure&) {
        std::cout << "discovery-exception:typed:"
                  << exception_destructors << ':' << discovery.roots_found
                  << ':' << g_rgb_files.size() << ':'
                  << g_flattened_tga_candidates["ship.tga"].size() << '\n';
    } catch (...) {
        std::cout << "discovery-exception:other\n";
    }
    std::cout << "discovery-exception:after:"
              << exception_destructors << '\n';
    fail_second_root = false;
    api.extension_root_count = []() -> std::uint32_t { return 5000; };
    discovered = discover_legacy_texture_files(discovery);
    std::cout << "discovery-invalid:" << discovered << ':'
              << discovery.roots_found << '\n';
    g_generated_texture_files.push_back("cleanup-file.tga");
    header_file("cleanup-file.tga", 2, 2);
    g_prepared_texture_files["cleanup"] = "cleanup-file.tga";
    clear_conversion_cache();
    std::cout << "clear-unlocked:" << g_generated_texture_files.size()
              << ':' << g_prepared_texture_files.size() << ':'
              << (GetFileAttributesA("cleanup-file.tga") !=
                  INVALID_FILE_ATTRIBUTES) << '\n';
    DeleteFileA("cleanup-file.tga");
    CreateDirectoryA("cleanup-cache", nullptr);
    header_file("cleanup-cache\\generated.tga", 2, 2);
    InitializeCriticalSection(&g_conversion_lock);
    g_conversion_lock_ready = true;
    g_generated_texture_files.push_back("cleanup-cache\\generated.tga");
    g_conversion_cache_directory = "cleanup-cache";
    clear_conversion_cache();
    std::cout << "clear-locked:" << g_conversion_lock_ready << ':'
              << g_generated_texture_files.size() << ':'
              << (GetFileAttributesA("cleanup-cache\\generated.tga") !=
                  INVALID_FILE_ATTRIBUTES) << ':'
              << (GetFileAttributesA("cleanup-cache") !=
                  INVALID_FILE_ATTRIBUTES) << '\n';
    g_rgb_files["reset.tga"] = "reset.tga";
    g_redirect_log_count = 7;
    clear_state();
    std::cout << "clear-state:" << g_rgb_files.size() << ':'
              << g_redirect_log_count << ':' << (g_api != nullptr) << '\n';
    return 0;
}
