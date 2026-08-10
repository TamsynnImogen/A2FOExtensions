/*
 * File: modules/A2FORGBTextures/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Legacy RGB/Index8/Compressed texture fallback/redirector via
 * FileExists/OpenRead interception with safe null-source handling.
 */

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr const char* kModuleName = "A2FORGBTextures";
constexpr const char* kTexturesPrefix = "textures\\";
constexpr const char* kRgbPrefix = "rgb\\";
constexpr const char* kIndex8Prefix = "index8\\";
constexpr const char* kCompressedPrefix = "compressed\\";

constexpr std::uint32_t kArmadaTimestamp = 0x3c4c76bd;
constexpr std::uint32_t kArmadaImageSize = 0x00403999;

// Armada II 1.1 locations. These are validated before the IAT pointer is
// changed; docs/addresses.md is the human-readable register for them.
constexpr std::uintptr_t kFileExistsRva = 0x2400d0;
constexpr std::uintptr_t kOpenReadRva = 0x240150;
constexpr std::uintptr_t kReadFileParametersRva = 0x242ee0;
constexpr std::uintptr_t kReadFileExistsCallRva = 0x242fa5;
constexpr std::uintptr_t kLoadPixelDataRva = 0x2434b0;
constexpr std::uintptr_t kLoadFileExistsCallRva = 0x243553;
constexpr std::uintptr_t kLockSurfaceRva = 0x242780;
constexpr std::uintptr_t kBlendPixelsXrgb888Rva = 0x0e7c80;
constexpr std::uintptr_t kUpdateScanGridSpriteRva = 0x0e96c0;
constexpr std::uintptr_t kSpriteGetTextureRva = 0x23b150;
constexpr std::uintptr_t kUnlockSurfaceRva = 0x2437b0;
constexpr std::uintptr_t kRendererGlobalRva = 0x3ad508;
constexpr std::uintptr_t kRgbLiteralRva = 0x32d178;
constexpr std::uintptr_t kFopenIatRva = 0x3b7f8c;

const std::uint8_t kExpectedFileExists[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
const std::uint8_t kExpectedOpenRead[] =
    {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
const std::uint8_t kExpectedReadFileParameters[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};
const std::uint8_t kExpectedReadFileExistsCall[] =
    {0xe8, 0x26, 0xd1, 0xff, 0xff};
const std::uint8_t kExpectedLoadPixelData[] =
    {0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};
const std::uint8_t kExpectedLoadFileExistsCall[] =
    {0xe8, 0x78, 0xcb, 0xff, 0xff};
const std::uint8_t kExpectedLockSurface[] =
    {0x55, 0x8b, 0xec, 0x56, 0x57};
const std::uint8_t kExpectedBlendPixelsXrgb888[] =
    {0x55, 0x8b, 0xec, 0x83, 0xec, 0x40};
const std::uint8_t kExpectedUpdateScanGridSprite[] =
    {0x55, 0x8b, 0xec, 0x83, 0xec, 0x28};
const std::uint8_t kExpectedSpriteGetTexture[] =
    {0x8b, 0x41, 0x58, 0xc3};
const std::uint8_t kExpectedUnlockSurface[] =
    {0x55, 0x8b, 0xec, 0x56, 0x57};
const char kExpectedRgbLiteral[] = "Textures\\RGB\\";

using FopenFn = FILE* (__cdecl*)(const char*, const char*);
using FileExistsFn = bool (__cdecl*)(const char*);
using BlendPixelsXrgb888Fn = void (__cdecl*)(
    std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t,
    std::uintptr_t, std::uintptr_t, void*, const std::uint8_t*,
    std::uintptr_t, std::uintptr_t, std::uintptr_t);

enum class RootTgaRoute {
    none,
    root,
    rgb,
    index8,
    compressed,
};

struct RootTgaCandidate {
    std::string physical;
    RootTgaRoute route = RootTgaRoute::none;
};

extern "C" std::uintptr_t a2fo_rgb_call_thiscall_0(
    void* target, void* self);
extern "C" std::uintptr_t a2fo_rgb_call_thiscall_1(
    void* target, void* self, std::uintptr_t argument1);
extern "C" std::uintptr_t a2fo_rgb_call_thiscall_2(
    void* target, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
extern "C" std::uintptr_t a2fo_rgb_call_thiscall_4(
    void* target, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
FopenFn g_original_fopen = nullptr;
void** g_fopen_slot = nullptr;
bool g_fopen_patched = false;
A2FO_InlineHook g_lock_surface_hook{};
A2FO_InlineHook g_blend_pixels_hook{};
A2FO_InlineHook g_scan_grid_hook{};
A2FO_InlineHook g_file_exists_hook{};
A2FO_InlineHook g_open_read_hook{};
std::unordered_map<std::string, std::string> g_rgb_files;
std::unordered_map<std::string, std::string> g_index8_files;
std::unordered_map<std::string, std::string> g_compressed_files;
std::unordered_map<std::string, std::string> g_root_texture_files;
std::unordered_map<std::string, std::vector<RootTgaCandidate>>
    g_flattened_tga_candidates;
std::unordered_map<std::string, std::string> g_prepared_texture_files;
std::unordered_set<std::string> g_unusable_texture_files;
std::vector<std::string> g_generated_texture_files;
std::string g_root_directory;
std::string g_conversion_cache_directory;
CRITICAL_SECTION g_conversion_lock{};
bool g_conversion_lock_ready = false;
volatile LONG g_redirect_log_count = 0;
volatile LONG g_missing_log_count = 0;
volatile LONG g_recovery_log_count = 0;
volatile LONG g_blend_guard_log_count = 0;
volatile LONG g_root_redirect_log_count = 0;
volatile LONG g_scan_grid_guard_log_count = 0;
volatile LONG g_conversion_log_count = 0;
volatile LONG g_conversion_file_counter = 0;

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) {
    return reinterpret_cast<T*>(
        reinterpret_cast<std::uint8_t*>(module) + rva);
}

void log_line(const std::string& text) {
    if (g_api && g_api->log) g_api->log(kModuleName, text.c_str());
}

bool readable_range(const void* pointer, std::size_t size) {
    return pointer && size != 0 && !IsBadReadPtr(pointer, size);
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

std::string lower_normalized(std::string value) {
    std::transform(
        value.begin(), value.end(), value.begin(), [](unsigned char ch) {
            if (ch == '/') return '\\';
            return static_cast<char>(std::tolower(ch));
        });
    return value;
}

std::uint16_t little_u16(const std::uint8_t* bytes) {
    return static_cast<std::uint16_t>(bytes[0]) |
           (static_cast<std::uint16_t>(bytes[1]) << 8);
}

enum class TgaPreparation {
    compatible,
    converted,
    unsupported,
};

TgaPreparation prepare_tga_for_rgb_loader(
    const std::vector<std::uint8_t>& source,
    std::vector<std::uint8_t>& output) {
    output.clear();
    if (source.size() < 18) return TgaPreparation::unsupported;

    const std::uint8_t id_length = source[0];
    const std::uint8_t color_map_type = source[1];
    const std::uint8_t image_type = source[2];
    const std::uint16_t color_map_first = little_u16(source.data() + 3);
    const std::uint16_t color_map_length = little_u16(source.data() + 5);
    const std::uint8_t color_map_depth = source[7];
    const std::uint16_t width = little_u16(source.data() + 12);
    const std::uint16_t height = little_u16(source.data() + 14);
    const std::uint8_t pixel_depth = source[16];

    if (width == 0 || height == 0) return TgaPreparation::unsupported;
    if (image_type == 2 && color_map_type == 0 &&
        (pixel_depth == 24 || pixel_depth == 32)) {
        return TgaPreparation::compatible;
    }

    enum class PixelKind { color_mapped, true_color, grayscale };
    PixelKind kind{};
    bool rle = false;
    switch (image_type) {
    case 1:
        kind = PixelKind::color_mapped;
        break;
    case 2:
        kind = PixelKind::true_color;
        break;
    case 3:
        kind = PixelKind::grayscale;
        break;
    case 9:
        kind = PixelKind::color_mapped;
        rle = true;
        break;
    case 10:
        kind = PixelKind::true_color;
        rle = true;
        break;
    case 11:
        kind = PixelKind::grayscale;
        rle = true;
        break;
    default:
        return TgaPreparation::unsupported;
    }

    std::size_t cursor = 18u + id_length;
    if (cursor > source.size()) return TgaPreparation::unsupported;

    std::size_t palette_offset = 0;
    std::size_t palette_entry_bytes = 0;
    if (kind == PixelKind::color_mapped) {
        if (color_map_type != 1 || color_map_length == 0 ||
            (pixel_depth != 8 && pixel_depth != 16) ||
            (color_map_depth != 15 && color_map_depth != 16 &&
             color_map_depth != 24 && color_map_depth != 32)) {
            return TgaPreparation::unsupported;
        }
        palette_entry_bytes = (color_map_depth + 7u) / 8u;
        const std::size_t palette_size =
            static_cast<std::size_t>(color_map_length) * palette_entry_bytes;
        if (palette_size > source.size() - cursor) {
            return TgaPreparation::unsupported;
        }
        palette_offset = cursor;
        cursor += palette_size;
    } else if (color_map_type != 0) {
        return TgaPreparation::unsupported;
    }

    std::size_t input_pixel_bytes = 0;
    std::size_t output_pixel_bytes = 3;
    if (kind == PixelKind::color_mapped) {
        input_pixel_bytes = pixel_depth / 8u;
        output_pixel_bytes = color_map_depth == 32 ? 4u : 3u;
    } else if (kind == PixelKind::true_color) {
        if (pixel_depth != 16 && pixel_depth != 24 && pixel_depth != 32) {
            return TgaPreparation::unsupported;
        }
        input_pixel_bytes = (pixel_depth + 7u) / 8u;
        output_pixel_bytes = pixel_depth == 32 ? 4u : 3u;
    } else {
        if (pixel_depth != 8 && pixel_depth != 16) {
            return TgaPreparation::unsupported;
        }
        input_pixel_bytes = pixel_depth / 8u;
        output_pixel_bytes = pixel_depth == 16 ? 4u : 3u;
    }

    const std::size_t pixel_count =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    constexpr std::size_t kMaximumPreparedTgaSize = 256u * 1024u * 1024u;
    if (pixel_count >
        (kMaximumPreparedTgaSize - 18u) / output_pixel_bytes) {
        return TgaPreparation::unsupported;
    }

    output.assign(18u, 0);
    output[2] = 2;  // Uncompressed true-colour TGA.
    std::memcpy(output.data() + 8, source.data() + 8, 8);
    output[16] = static_cast<std::uint8_t>(output_pixel_bytes * 8u);
    output[17] = static_cast<std::uint8_t>(
        (source[17] & 0x30u) | (output_pixel_bytes == 4u ? 8u : 0u));
    output.reserve(18u + pixel_count * output_pixel_bytes);

    const auto expand_5_bit = [](std::uint16_t value) -> std::uint8_t {
        value &= 0x1fu;
        return static_cast<std::uint8_t>((value << 3u) | (value >> 2u));
    };
    const auto read_pixel = [&](std::size_t& input,
                                std::array<std::uint8_t, 4>& pixel) -> bool {
        if (input > source.size() ||
            input_pixel_bytes > source.size() - input) {
            return false;
        }
        pixel = {0, 0, 0, 255};
        if (kind == PixelKind::color_mapped) {
            std::uint32_t palette_index = source[input];
            if (input_pixel_bytes == 2) {
                palette_index |= static_cast<std::uint32_t>(
                    source[input + 1]) << 8u;
            }
            input += input_pixel_bytes;
            if (palette_index < color_map_first ||
                palette_index >=
                    static_cast<std::uint32_t>(color_map_first) +
                        color_map_length) {
                return false;
            }
            const std::size_t entry = palette_offset +
                (palette_index - color_map_first) * palette_entry_bytes;
            if (color_map_depth == 24 || color_map_depth == 32) {
                pixel[0] = source[entry];
                pixel[1] = source[entry + 1];
                pixel[2] = source[entry + 2];
                if (color_map_depth == 32) pixel[3] = source[entry + 3];
            } else {
                const std::uint16_t packed = little_u16(source.data() + entry);
                pixel[0] = expand_5_bit(packed);
                pixel[1] = expand_5_bit(packed >> 5u);
                pixel[2] = expand_5_bit(packed >> 10u);
            }
            return true;
        }
        if (kind == PixelKind::true_color) {
            if (pixel_depth == 16) {
                const std::uint16_t packed = little_u16(source.data() + input);
                pixel[0] = expand_5_bit(packed);
                pixel[1] = expand_5_bit(packed >> 5u);
                pixel[2] = expand_5_bit(packed >> 10u);
            } else {
                pixel[0] = source[input];
                pixel[1] = source[input + 1];
                pixel[2] = source[input + 2];
                if (pixel_depth == 32) pixel[3] = source[input + 3];
            }
            input += input_pixel_bytes;
            return true;
        }
        pixel[0] = pixel[1] = pixel[2] = source[input];
        if (pixel_depth == 16) pixel[3] = source[input + 1];
        input += input_pixel_bytes;
        return true;
    };
    const auto append_pixel = [&](const std::array<std::uint8_t, 4>& pixel) {
        output.insert(output.end(), pixel.begin(),
                      pixel.begin() + output_pixel_bytes);
    };

    std::size_t emitted = 0;
    while (emitted < pixel_count) {
        std::size_t packet_count = pixel_count - emitted;
        bool repeated = false;
        if (rle) {
            if (cursor >= source.size()) return TgaPreparation::unsupported;
            const std::uint8_t packet = source[cursor++];
            packet_count = (packet & 0x7fu) + 1u;
            repeated = (packet & 0x80u) != 0;
            if (packet_count > pixel_count - emitted) {
                return TgaPreparation::unsupported;
            }
        }
        if (repeated) {
            std::array<std::uint8_t, 4> pixel{};
            if (!read_pixel(cursor, pixel)) return TgaPreparation::unsupported;
            for (std::size_t index = 0; index < packet_count; ++index) {
                append_pixel(pixel);
            }
        } else {
            for (std::size_t index = 0; index < packet_count; ++index) {
                std::array<std::uint8_t, 4> pixel{};
                if (!read_pixel(cursor, pixel)) {
                    return TgaPreparation::unsupported;
                }
                append_pixel(pixel);
            }
        }
        emitted += packet_count;
        if (!rle) break;
    }
    return emitted == pixel_count
        ? TgaPreparation::converted
        : TgaPreparation::unsupported;
}

bool read_binary_file(const std::string& path,
                      std::vector<std::uint8_t>& bytes) {
    bytes.clear();
    HANDLE file = CreateFileA(
        path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    constexpr LONGLONG kMaximumSourceTgaSize = 128ll * 1024ll * 1024ll;
    bool ok = GetFileSizeEx(file, &size) && size.QuadPart >= 18 &&
              size.QuadPart <= kMaximumSourceTgaSize;
    if (ok) {
        bytes.resize(static_cast<std::size_t>(size.QuadPart));
        DWORD read = 0;
        ok = ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()),
                      &read, nullptr) && read == bytes.size();
    }
    CloseHandle(file);
    if (!ok) bytes.clear();
    return ok;
}

class ConversionLockGuard {
public:
    ConversionLockGuard() {
        if (g_conversion_lock_ready) {
            EnterCriticalSection(&g_conversion_lock);
            locked_ = true;
        }
    }
    ~ConversionLockGuard() {
        if (locked_) LeaveCriticalSection(&g_conversion_lock);
    }
    bool locked() const { return locked_; }

private:
    bool locked_ = false;
};

bool ensure_conversion_cache_directory() {
    if (!g_conversion_cache_directory.empty()) return true;
    char temporary[MAX_PATH + 1]{};
    const DWORD length = GetTempPathA(MAX_PATH, temporary);
    if (length == 0 || length > MAX_PATH) return false;
    char leaf[64]{};
    std::snprintf(leaf, sizeof(leaf), "A2FORGBTextures-%lu",
                  static_cast<unsigned long>(GetCurrentProcessId()));
    const std::string directory = join_path(temporary, leaf);
    if (!CreateDirectoryA(directory.c_str(), nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS) {
        return false;
    }
    g_conversion_cache_directory = directory;
    return true;
}

bool write_prepared_tga(const std::vector<std::uint8_t>& bytes,
                        std::string& path) {
    path.clear();
    if (!ensure_conversion_cache_directory()) return false;
    char leaf[64]{};
    const LONG number = InterlockedIncrement(&g_conversion_file_counter);
    std::snprintf(leaf, sizeof(leaf), "prepared-%08lx.tga",
                  static_cast<unsigned long>(number));
    const std::string candidate =
        join_path(g_conversion_cache_directory, leaf);
    HANDLE file = CreateFileA(candidate.c_str(), GENERIC_WRITE, 0, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    const bool ok = WriteFile(file, bytes.data(),
                              static_cast<DWORD>(bytes.size()),
                              &written, nullptr) && written == bytes.size();
    CloseHandle(file);
    if (!ok) {
        DeleteFileA(candidate.c_str());
        return false;
    }
    path = candidate;
    return true;
}

bool prepare_non_rgb_texture(const std::string& source,
                             std::string& prepared) {
    prepared.clear();
    ConversionLockGuard lock;
    if (!lock.locked()) return false;
    const std::string cache_key = lower_normalized(source);
    const auto cached = g_prepared_texture_files.find(cache_key);
    if (cached != g_prepared_texture_files.end()) {
        prepared = cached->second;
        return true;
    }
    if (g_unusable_texture_files.find(cache_key) !=
        g_unusable_texture_files.end()) {
        return false;
    }

    std::vector<std::uint8_t> input;
    std::vector<std::uint8_t> output;
    const bool read = read_binary_file(source, input);
    const TgaPreparation result = read
        ? prepare_tga_for_rgb_loader(input, output)
        : TgaPreparation::unsupported;
    if (result == TgaPreparation::compatible) {
        g_prepared_texture_files[cache_key] = source;
        prepared = source;
        return true;
    }
    if (result == TgaPreparation::converted &&
        write_prepared_tga(output, prepared)) {
        g_prepared_texture_files[cache_key] = prepared;
        g_generated_texture_files.push_back(prepared);
        const LONG count = InterlockedIncrement(&g_conversion_log_count);
        if (count <= 16) {
            log_line("Prepared non-RGB legacy TGA for the true-colour loader: " +
                     source + " -> " + prepared);
        } else if (count == 17) {
            log_line("Further non-RGB legacy TGA preparation logs suppressed");
        }
        return true;
    }
    g_unusable_texture_files.insert(cache_key);
    log_line("Unsupported legacy TGA cannot use the true-colour fallback: " +
             source);
    return false;
}

bool ascii_equal(const std::string& left, const char* right) {
    if (!right || left.size() != std::strlen(right)) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(left[index])) !=
            std::tolower(static_cast<unsigned char>(right[index]))) {
            return false;
        }
    }
    return true;
}

bool is_legacy_texture_directory(const std::string& name) {
    return ascii_equal(name, "RGB") || ascii_equal(name, "Index8") ||
           ascii_equal(name, "Compressed");
}

std::string find_child_directory(const std::string& parent,
                                 const char* wanted) {
    WIN32_FIND_DATAA data{};
    HANDLE search = FindFirstFileA(join_path(parent, "*").c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) return {};

    std::string result;
    do {
        const std::string name = data.cFileName;
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0 &&
            name != "." && name != ".." && ascii_equal(name, wanted)) {
            result = join_path(parent, name);
            break;
        }
    } while (FindNextFileA(search, &data));
    FindClose(search);
    return result;
}

void scan_texture_directory(const std::string& directory,
                            const std::string& relative,
                            std::uint32_t depth,
                            bool exclude_top_level_legacy,
                            std::unordered_map<std::string, std::string>& files) {
    if (depth > 32) return;

    WIN32_FIND_DATAA data{};
    HANDLE search = FindFirstFileA(join_path(directory, "*").c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) return;

    do {
        const std::string name = data.cFileName;
        if (name == "." || name == ".." ||
            (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            continue;
        }
        const std::string physical = join_path(directory, name);
        const std::string child_relative =
            relative.empty() ? name : join_path(relative, name);
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (exclude_top_level_legacy && relative.empty() &&
                is_legacy_texture_directory(name)) {
                continue;
            }
            scan_texture_directory(physical, child_relative, depth + 1,
                                   false, files);
        } else {
            files[lower_normalized(child_relative)] = physical;
        }
    } while (FindNextFileA(search, &data));
    FindClose(search);
}

struct LegacyTextureDiscovery {
    std::uint32_t roots_found = 0;
    std::uint32_t rgb_roots = 0;
    std::uint32_t index8_roots = 0;
    std::uint32_t compressed_roots = 0;
};

bool discover_legacy_texture_files(LegacyTextureDiscovery& discovery) {
    discovery = {};
    if (!g_api || !g_api->extension_root_count || !g_api->extension_root) {
        return false;
    }

    const std::uint32_t count = g_api->extension_root_count();
    if (count > 4096) {
        log_line("Extension-root count is invalid");
        return false;
    }
    for (std::uint32_t index = 0; index < count; ++index) {
        const char* root = g_api->extension_root(index);
        if (!root || !*root) continue;
        const std::string textures = find_child_directory(root, "Textures");
        if (textures.empty()) continue;

        std::unordered_map<std::string, std::string> root_files;
        std::unordered_map<std::string, std::string> rgb_files;
        std::unordered_map<std::string, std::string> index8_files;
        std::unordered_map<std::string, std::string> compressed_files;
        scan_texture_directory(textures, {}, 0, true, root_files);
        for (const auto& file : root_files) {
            // Roots arrive from lowest to highest precedence, so the active
            // mod remains the winner for direct root/DDS lookups.
            g_root_texture_files[file.first] = file.second;
        }

        bool root_has_legacy_textures = false;
        const auto scan_legacy_folder = [&](
            const char* folder_name,
            std::unordered_map<std::string, std::string>& root_files,
            std::unordered_map<std::string, std::string>& exact_files,
            std::uint32_t& folder_roots) {
            const std::string folder =
                find_child_directory(textures, folder_name);
            if (folder.empty()) return;
            root_has_legacy_textures = true;
            ++folder_roots;
            scan_texture_directory(folder, {}, 0, false, root_files);
            for (const auto& file : root_files) {
                exact_files[file.first] = file.second;
            }
        };

        // Keep each legacy folder as a separate namespace so precedence and
        // pixel-format preparation remain explicit. Fleet Operations rewrites
        // Armada's only generated pathname to the root Textures directory;
        // Index8/RLE candidates are expanded before they enter that retained
        // true-colour loader path.
        scan_legacy_folder("Compressed", compressed_files,
                           g_compressed_files,
                           discovery.compressed_roots);
        scan_legacy_folder("Index8", index8_files, g_index8_files,
                           discovery.index8_roots);
        scan_legacy_folder("RGB", rgb_files, g_rgb_files,
                           discovery.rgb_roots);

        // Pick one format per key inside this extension root, then append that
        // root's winner to the precedence chain. A child Index8/Compressed
        // file must override a parent's RGB copy; folder preference is only a
        // tie-breaker when the same root contains more than one variant.
        std::unordered_map<std::string, RootTgaCandidate> root_candidates;
        const auto publish = [&](
            const std::unordered_map<std::string, std::string>& files,
            RootTgaRoute route) {
            for (const auto& file : files) {
                root_candidates[file.first] = {file.second, route};
            }
        };
        publish(compressed_files, RootTgaRoute::compressed);
        publish(index8_files, RootTgaRoute::index8);
        publish(rgb_files, RootTgaRoute::rgb);
        publish(root_files, RootTgaRoute::root);
        for (const auto& candidate : root_candidates) {
            g_flattened_tga_candidates[candidate.first].push_back(
                candidate.second);
        }
        if (root_has_legacy_textures) ++discovery.roots_found;
    }
    return discovery.roots_found != 0;
}

template <std::size_t Size>
bool signature_matches(std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) {
    const void* location = at(g_armada, rva);
    return readable_range(location, Size) &&
           std::memcmp(location, expected, Size) == 0;
}

std::string byte_string(const void* pointer, std::size_t size) {
    if (!readable_range(pointer, size)) return "<unreadable>";
    const auto* bytes = static_cast<const std::uint8_t*>(pointer);
    std::string result;
    char byte[4]{};
    for (std::size_t index = 0; index < size; ++index) {
        std::snprintf(byte, sizeof(byte), "%02x", bytes[index]);
        if (!result.empty()) result.push_back(' ');
        result += byte;
    }
    return result;
}

template <std::size_t Size>
void observe_signature(const char* name, std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) {
    if (signature_matches(rva, expected)) return;
    log_line(std::string("Runtime legacy-texture provenance differs at ") +
             name +
             " (RVA 0x" + [&]() {
                 char value[16]{};
                 std::snprintf(value, sizeof(value), "%08lx",
                               static_cast<unsigned long>(rva));
                 return std::string(value);
             }() + ", expected " + byte_string(expected, Size) +
             ", found " + byte_string(at(g_armada, rva), Size) +
             "); continuing because Fleet Operations may prepatch this "
             "provenance site");
}

template <typename Function>
void* function_address(Function function) {
    static_assert(sizeof(function) == sizeof(void*),
                  "32-bit function and object pointers must match");
    void* address = nullptr;
    std::memcpy(&address, &function, sizeof(address));
    return address;
}

FopenFn fopen_function(void* address) {
    FopenFn function = nullptr;
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

template <typename Function>
Function function_from_address(void* address) {
    static_assert(sizeof(Function) == sizeof(void*),
                  "32-bit function and object pointers must match");
    Function function = nullptr;
    std::memcpy(&function, &address, sizeof(function));
    return function;
}

bool validate_armada() {
    if (!g_armada) {
        log_line("ArmadaL.exe is not available");
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_armada);
    if (!readable_range(dos, sizeof(*dos)) ||
        dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) {
        log_line("ArmadaL.exe has no valid DOS header");
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const std::uint8_t*>(g_armada) + dos->e_lfanew);
    if (!readable_range(nt, sizeof(*nt)) ||
        nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp != kArmadaTimestamp ||
        nt->OptionalHeader.SizeOfImage != kArmadaImageSize) {
        char message[192]{};
        std::snprintf(
            message, sizeof(message),
            "ArmadaL.exe version mismatch (timestamp=%08lx, image=%08lx)",
            readable_range(nt, sizeof(*nt))
                ? static_cast<unsigned long>(nt->FileHeader.TimeDateStamp) : 0,
            readable_range(nt, sizeof(*nt))
                ? static_cast<unsigned long>(nt->OptionalHeader.SizeOfImage) : 0);
        log_line(message);
        return false;
    }
    // Fleet Operations installs its own texture callbacks before native A2FO
    // modules load and may rewrite code in this chain. These bytes are useful
    // provenance diagnostics, not safe runtime gates. The exact PE identity,
    // legacy literal, and fopen import ownership below are the hard gates for
    // the one pointer this module actually modifies.
    observe_signature("file-existence helper", kFileExistsRva,
                      kExpectedFileExists);
    observe_signature("read-parameters entry", kReadFileParametersRva,
                      kExpectedReadFileParameters);
    observe_signature("read-parameters helper call", kReadFileExistsCallRva,
                      kExpectedReadFileExistsCall);
    observe_signature("pixel-data entry", kLoadPixelDataRva,
                      kExpectedLoadPixelData);
    observe_signature("pixel-data helper call", kLoadFileExistsCallRva,
                      kExpectedLoadFileExistsCall);
    constexpr char kFleetOpsTextureLiteral[] = "Textures\\";
    const void* literal = at(g_armada, kRgbLiteralRva);
    const bool original_literal =
        readable_range(literal, sizeof(kExpectedRgbLiteral)) &&
        std::memcmp(literal, kExpectedRgbLiteral,
                    sizeof(kExpectedRgbLiteral)) == 0;
    const bool fleetops_literal =
        readable_range(literal, sizeof(kFleetOpsTextureLiteral)) &&
        std::memcmp(literal, kFleetOpsTextureLiteral,
                    sizeof(kFleetOpsTextureLiteral)) == 0;
    if (!original_literal && !fleetops_literal) {
        log_line("Armada texture pathname literal is neither the original "
                 "Textures\\RGB\\ nor Fleet Operations' Textures\\ rewrite");
        return false;
    }
    if (fleetops_literal) {
        log_line("Detected Fleet Operations' Textures\\ legacy-TGA rewrite; "
                 "RGB will be used after root-texture lookup");
    }

    g_fopen_slot = at<void*>(g_armada, kFopenIatRva);
    if (!readable_range(g_fopen_slot, sizeof(*g_fopen_slot))) {
        log_line("Armada fopen import slot is unreadable");
        g_fopen_slot = nullptr;
        return false;
    }
    HMODULE msvcrt = GetModuleHandleA("msvcrt.dll");
    FARPROC exported = msvcrt ? GetProcAddress(msvcrt, "fopen") : nullptr;
    void* exported_address = nullptr;
    static_assert(sizeof(exported) == sizeof(exported_address),
                  "32-bit FARPROC and object pointers must match");
    std::memcpy(&exported_address, &exported, sizeof(exported_address));
    if (!exported_address || *g_fopen_slot != exported_address) {
        log_line("Armada fopen import does not resolve to msvcrt!fopen");
        g_fopen_slot = nullptr;
        return false;
    }
    g_original_fopen = fopen_function(exported_address);
    return g_original_fopen != nullptr;
}

enum class TextureRequest {
    none,
    direct_rgb,
    direct_index8,
    direct_compressed,
    fleetops_root_tga,
    fleetops_root_dds,
};

const std::unordered_map<std::string, std::string>*
direct_legacy_files(TextureRequest request) {
    switch (request) {
    case TextureRequest::direct_rgb:
        return &g_rgb_files;
    case TextureRequest::direct_index8:
        return &g_index8_files;
    case TextureRequest::direct_compressed:
        return &g_compressed_files;
    default:
        return nullptr;
    }
}

const char* direct_legacy_folder_name(TextureRequest request) {
    switch (request) {
    case TextureRequest::direct_rgb:
        return "RGB";
    case TextureRequest::direct_index8:
        return "Index8";
    case TextureRequest::direct_compressed:
        return "Compressed";
    default:
        return "legacy";
    }
}

const char* root_tga_route_name(RootTgaRoute route) {
    switch (route) {
    case RootTgaRoute::root:
        return "root Textures";
    case RootTgaRoute::rgb:
        return "RGB";
    case RootTgaRoute::index8:
        return "Index8 (prepared true-colour)";
    case RootTgaRoute::compressed:
        return "Compressed (prepared true-colour)";
    default:
        return "none";
    }
}

bool select_root_tga(const std::string& key, std::string& physical,
                     RootTgaRoute& route) {
    physical.clear();
    route = RootTgaRoute::none;
    const auto found = g_flattened_tga_candidates.find(key);
    if (found == g_flattened_tga_candidates.end()) return false;
    for (auto candidate = found->second.rbegin();
         candidate != found->second.rend(); ++candidate) {
        if (candidate->route == RootTgaRoute::index8 ||
            candidate->route == RootTgaRoute::compressed) {
            if (!prepare_non_rgb_texture(candidate->physical, physical)) {
                continue;
            }
        } else {
            physical = candidate->physical;
        }
        route = candidate->route;
        return true;
    }
    return false;
}

bool strip_prefix(std::string& value, const char* prefix) {
    const std::size_t length = std::strlen(prefix);
    if (value.compare(0, length, prefix) != 0) return false;
    value.erase(0, length);
    return true;
}

bool strip_legacy_texture_prefix(std::string& value) {
    return strip_prefix(value, kRgbPrefix) ||
           strip_prefix(value, kIndex8Prefix) ||
           strip_prefix(value, kCompressedPrefix);
}

bool tga_filename(const std::string& filename) {
    constexpr const char* suffix = ".tga";
    return filename.size() >= std::strlen(suffix) &&
           filename.compare(filename.size() - std::strlen(suffix),
                            std::strlen(suffix), suffix) == 0;
}

bool dds_filename(const std::string& filename) {
    constexpr const char* suffix = ".dds";
    return filename.size() >= std::strlen(suffix) &&
           filename.compare(filename.size() - std::strlen(suffix),
                            std::strlen(suffix), suffix) == 0;
}

TextureRequest texture_request(const char* filename, const char* mode,
                               std::string& key) {
    if (!filename || !*filename || !mode || mode[0] != 'r') {
        return TextureRequest::none;
    }
    std::string normalized = lower_normalized(filename);
    while (normalized.size() >= 2 && normalized[0] == '.' &&
           normalized[1] == '\\') {
        normalized.erase(0, 2);
    }
    const std::size_t textures = normalized.rfind(kTexturesPrefix);
    if (textures != std::string::npos) {
        key = normalized.substr(textures + std::strlen(kTexturesPrefix));
    } else {
        const std::size_t separator = normalized.find_last_of('\\');
        key = separator == std::string::npos
            ? normalized
            : normalized.substr(separator + 1);
    }
    if (strip_prefix(key, kRgbPrefix)) {
        return key.empty() ? TextureRequest::none
                           : TextureRequest::direct_rgb;
    }
    if (strip_prefix(key, kIndex8Prefix)) {
        return key.empty() ? TextureRequest::none
                           : TextureRequest::direct_index8;
    }
    if (strip_prefix(key, kCompressedPrefix)) {
        return key.empty() ? TextureRequest::none
                           : TextureRequest::direct_compressed;
    }
    if (!key.empty() && tga_filename(key)) {
        return TextureRequest::fleetops_root_tga;
    }
    constexpr const char* dds_suffix = ".dds";
    if (key.size() >= std::strlen(dds_suffix) &&
        key.compare(key.size() - std::strlen(dds_suffix),
                    std::strlen(dds_suffix), dds_suffix) == 0) {
        return TextureRequest::fleetops_root_dds;
    }
    return TextureRequest::none;
}

FILE* __cdecl legacy_texture_fopen(const char* filename,
                                   const char* mode) noexcept {
    FopenFn original = g_original_fopen;
    if (!original) return nullptr;

    try {
        std::string key;
        const TextureRequest request = texture_request(filename, mode, key);
        if (request != TextureRequest::none) {
            if (request == TextureRequest::fleetops_root_dds) {
                const auto root = g_root_texture_files.find(key);
                if (root != g_root_texture_files.end()) {
                    if (FILE* file = original(root->second.c_str(), mode)) {
                        if (InterlockedCompareExchange(
                                &g_root_redirect_log_count, 1, 0) == 0) {
                            log_line(std::string("First root texture redirect: ") +
                                     filename + " -> " + root->second);
                        }
                        return file;
                    }
                }
                // Never present TGA bytes to Fleet Ops' DDS enhancement path.
                return original(filename, mode);
            }

            if (request == TextureRequest::fleetops_root_tga) {
                std::string physical;
                RootTgaRoute route = RootTgaRoute::none;
                if (select_root_tga(key, physical, route)) {
                    if (FILE* file = original(physical.c_str(), mode)) {
                        if (route == RootTgaRoute::root) {
                            if (InterlockedCompareExchange(
                                    &g_root_redirect_log_count, 1, 0) == 0) {
                                log_line(std::string(
                                    "First root texture redirect: ") +
                                    filename + " -> " + physical);
                            }
                        } else if (InterlockedCompareExchange(
                                       &g_redirect_log_count, 1, 0) == 0) {
                            log_line(std::string(
                                "First legacy texture fallback (") +
                                root_tga_route_name(route) + "): " +
                                filename + " -> " + physical);
                        }
                        return file;
                    }
                }
                const LONG missing =
                    InterlockedIncrement(&g_missing_log_count);
                if (missing <= 16) {
                    log_line(std::string("Legacy texture file not mapped: ") +
                             filename);
                } else if (missing == 17) {
                    log_line("Further unmapped legacy texture filenames "
                             "suppressed");
                }
                return original(filename, mode);
            }

            const auto* direct_files = direct_legacy_files(request);
            if (direct_files) {
                const auto legacy = direct_files->find(key);
                if (legacy == direct_files->end()) {
                    return original(filename, mode);
                }
                if (InterlockedCompareExchange(
                        &g_redirect_log_count, 1, 0) == 0) {
                    std::string route = "First legacy texture fallback (";
                    route += direct_legacy_folder_name(request);
                    route += ")";
                    log_line(route + ": " + filename + " -> " +
                             legacy->second);
                }
                return original(legacy->second.c_str(), mode);
            }
        }
    } catch (...) {
        // Never throw through msvcrt or the Armada caller.
    }
    return original(filename, mode);
}

bool mapped_tga_path(const char* filename, std::string& physical) {
    physical.clear();
    std::string key;
    const TextureRequest request = texture_request(filename, "rb", key);
    const auto* direct_files = direct_legacy_files(request);
    if (!direct_files && request != TextureRequest::fleetops_root_tga) {
        return false;
    }
    if (request == TextureRequest::fleetops_root_tga) {
        RootTgaRoute route = RootTgaRoute::none;
        return select_root_tga(key, physical, route);
    }
    const auto legacy = direct_files->find(key);
    if (legacy == direct_files->end()) return false;
    physical = legacy->second;
    return true;
}

bool __cdecl file_exists_hook(const char* filename) noexcept {
    FileExistsFn original = function_from_address<FileExistsFn>(
        g_file_exists_hook.gateway);
    if (!original) return false;
    try {
        std::string physical;
        if (mapped_tga_path(filename, physical)) {
            const bool exists = original(physical.c_str());
            if (exists && InterlockedCompareExchange(
                    &g_redirect_log_count, 1, 0) == 0) {
                log_line(std::string("First legacy TGA existence redirect: ") +
                         filename + " -> " + physical);
            }
            return exists;
        }
    } catch (...) {
        // Never throw through an Armada file-stream caller.
    }
    return original(filename);
}

int __attribute__((fastcall)) open_read_hook(
    void* stream, void*, const char* filename, int binary) noexcept {
    void* original = g_open_read_hook.gateway;
    if (!original) return 1;
    try {
        std::string physical;
        if (mapped_tga_path(filename, physical)) {
            return static_cast<int>(a2fo_rgb_call_thiscall_2(
                original, stream,
                reinterpret_cast<std::uintptr_t>(physical.c_str()),
                static_cast<std::uintptr_t>(binary)));
        }
    } catch (...) {
        // Never throw through an Armada file-stream caller.
    }
    return static_cast<int>(a2fo_rgb_call_thiscall_2(
        original, stream, reinterpret_cast<std::uintptr_t>(filename),
        static_cast<std::uintptr_t>(binary)));
}

bool write_fopen_slot(void* expected, void* replacement) {
    if (!g_fopen_slot) return false;
    DWORD old_protection = 0;
    if (!VirtualProtect(g_fopen_slot, sizeof(*g_fopen_slot), PAGE_READWRITE,
                        &old_protection)) {
        return false;
    }
    bool changed = false;
    if (*g_fopen_slot == expected) {
        *g_fopen_slot = replacement;
        FlushInstructionCache(GetCurrentProcess(), g_fopen_slot,
                              sizeof(*g_fopen_slot));
        changed = true;
    }
    DWORD restored = 0;
    VirtualProtect(g_fopen_slot, sizeof(*g_fopen_slot), old_protection,
                   &restored);
    return changed;
}

bool install_fopen_bridge() {
    if (!g_original_fopen || !g_fopen_slot) return false;
    const void* original = function_address(g_original_fopen);
    FopenFn hook = &legacy_texture_fopen;
    if (!write_fopen_slot(const_cast<void*>(original),
                          function_address(hook))) {
        log_line("Armada fopen import changed before legacy texture bridge "
                 "installation");
        return false;
    }
    g_fopen_patched = true;
    return true;
}

bool bounded_string(const char* source, std::string& value) {
    value.clear();
    if (!source) return false;
    constexpr std::size_t kMaximumName = 512;
    for (std::size_t index = 0; index < kMaximumName; ++index) {
        if (!readable_range(source + index, 1)) return false;
        const char character = source[index];
        if (character == '\0') return !value.empty();
        value.push_back(character);
    }
    return false;
}

bool texture_name_and_key(void* texture, std::string& name,
                          std::string& key) {
    if (!texture || !readable_range(
            static_cast<const std::uint8_t*>(texture) + 0x08,
            sizeof(const char*))) {
        return false;
    }
    const char* source = *reinterpret_cast<const char* const*>(
        static_cast<const std::uint8_t*>(texture) + 0x08);
    if (!bounded_string(source, name)) return false;

    key = lower_normalized(name);
    while (key.size() >= 2 && key[0] == '.' && key[1] == '\\') {
        key.erase(0, 2);
    }
    if (key.compare(0, std::strlen(kTexturesPrefix),
                    kTexturesPrefix) == 0) {
        key.erase(0, std::strlen(kTexturesPrefix));
    }
    strip_legacy_texture_prefix(key);
    if (dds_filename(key)) {
        key.replace(key.size() - 4, 4, ".tga");
    } else if (!tga_filename(key)) {
        key += ".tga";
    }
    return !key.empty();
}

bool legacy_texture_key_exists(const std::string& key) {
    return g_rgb_files.find(key) != g_rgb_files.end() ||
           g_index8_files.find(key) != g_index8_files.end() ||
           g_compressed_files.find(key) != g_compressed_files.end();
}

#if 0
// Callback-level routing was prototyped here, but both the Fleet Ops enhanced
// TGA path and direct re-entry into Armada's loader proved ABI/format unsafe.
// Keep it excluded while the lower FileExists/OpenRead route is validated.
bool absolute_path(const std::string& path, std::string& result) {
    result.clear();
    const DWORD required = GetFullPathNameA(
        path.c_str(), 0, nullptr, nullptr);
    if (required == 0 || required > 32767) return false;
    std::vector<char> buffer(static_cast<std::size_t>(required) + 1);
    const DWORD written = GetFullPathNameA(
        path.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(),
        nullptr);
    if (written == 0 || written >= buffer.size()) return false;
    result.assign(buffer.data(), written);
    return true;
}

bool fleetops_absolute_retry_name(const std::string& physical,
                                  std::string& retry_name) {
    if (!absolute_path(physical, retry_name) ||
        !tga_filename(lower_normalized(retry_name))) {
        retry_name.clear();
        return false;
    }
    retry_name.resize(retry_name.size() - 4);
    return true;
}

class ScopedTextureNameOverride {
public:
    ScopedTextureNameOverride(void* texture, const char* replacement) {
        if (!texture || !replacement || !*replacement) return;
        auto* slot = reinterpret_cast<const char**>(
            static_cast<std::uint8_t*>(texture) + 0x08);
        if (!readable_range(slot, sizeof(*slot)) ||
            IsBadWritePtr(slot, sizeof(*slot))) {
            return;
        }
        slot_ = slot;
        original_ = *slot;
        replacement_ = replacement;
        *slot_ = replacement_;
    }

    ~ScopedTextureNameOverride() {
        if (slot_ && *slot_ == replacement_) *slot_ = original_;
    }

    bool active() const { return slot_ != nullptr; }

private:
    const char** slot_ = nullptr;
    const char* original_ = nullptr;
    const char* replacement_ = nullptr;
};

BOOL CALLBACK initialize_fo_texture_path_lock(
    PINIT_ONCE, PVOID, PVOID*) {
    InitializeCriticalSection(&g_fo_texture_path_lock);
    return TRUE;
}

bool ensure_fo_texture_path_lock() {
    return InitOnceExecuteOnce(
        &g_fo_texture_path_once, &initialize_fo_texture_path_lock,
        nullptr, nullptr) != FALSE;
}

class ScopedTexturePathLock {
public:
    ScopedTexturePathLock() {
        EnterCriticalSection(&g_fo_texture_path_lock);
        active_ = true;
    }

    ~ScopedTexturePathLock() {
        if (active_) LeaveCriticalSection(&g_fo_texture_path_lock);
    }

    ScopedTexturePathLock(const ScopedTexturePathLock&) = delete;
    ScopedTexturePathLock& operator=(const ScopedTexturePathLock&) = delete;

private:
    bool active_ = false;
};

class ScopedArmadaTexturePrefix {
public:
    ScopedArmadaTexturePrefix() {
        literal_ = at<char>(g_armada, kRgbLiteralRva);
        if (!readable_range(literal_, 1) || *literal_ != 'T') {
            literal_ = nullptr;
            return;
        }
        active_ = write('\0');
    }

    ~ScopedArmadaTexturePrefix() {
        if (active_ && !write('T')) {
            log_line("Could not restore Armada texture prefix literal");
        }
    }

    bool active() const { return active_; }

    ScopedArmadaTexturePrefix(const ScopedArmadaTexturePrefix&) = delete;
    ScopedArmadaTexturePrefix& operator=(
        const ScopedArmadaTexturePrefix&) = delete;

private:
    bool write(char value) {
        DWORD old_protection = 0;
        if (!VirtualProtect(literal_, 1,
                            PAGE_EXECUTE_READWRITE, &old_protection)) {
            return false;
        }
        *literal_ = value;
        DWORD restored = 0;
        VirtualProtect(literal_, 1, old_protection, &restored);
        return true;
    }

    char* literal_ = nullptr;
    bool active_ = false;
};

bool fleetops_legacy_route(void* texture, std::string& logical_name,
                           std::string& retry_name) {
    std::string tga_key;
    if (!texture_name_and_key(texture, logical_name, tga_key)) return false;
    const auto rgb = g_rgb_files.find(tga_key);
    return rgb != g_rgb_files.end() &&
           fleetops_absolute_retry_name(rgb->second, retry_name);
}

std::uintptr_t __attribute__((fastcall))
fo_read_file_parameters_hook(void* texture, void*) noexcept {
    void* original = g_fo_read_parameters_hook.gateway;
    if (!original) return static_cast<std::uintptr_t>(-1);

    bool called = false;
    bool routed = false;
    std::uintptr_t result = static_cast<std::uintptr_t>(-1);
    std::string logical_name;
    std::string retry_name;
    try {
        if (!ensure_fo_texture_path_lock()) {
            return a2fo_rgb_call_thiscall_0(original, texture);
        }
        {
            ScopedTexturePathLock path_lock;
            // Let Fleet Ops try its complete native chain first. This
            // preserves loose and packaged DDS/TGA files. Only a failure is
            // retried through Armada's original, pixel-compatible TGA path.
            result = a2fo_rgb_call_thiscall_0(original, texture);
            called = true;
            if (result == 0) {
                g_fo_rgb_parameter_routes.erase(texture);
            } else {
                routed = fleetops_legacy_route(
                    texture, logical_name, retry_name);
                if (routed) {
                    ScopedArmadaTexturePrefix empty_prefix;
                    if (!empty_prefix.active()) routed = false;
                    ScopedTextureNameOverride name_override(
                        texture, routed ? retry_name.c_str() : nullptr);
                    if (routed && !name_override.active()) routed = false;
                    if (routed) {
                        result = a2fo_rgb_call_thiscall_0(
                            at(g_armada, kReadFileParametersRva), texture);
                        if (result == 0) {
                            g_fo_rgb_parameter_routes[texture] = retry_name;
                        } else {
                            g_fo_rgb_parameter_routes.erase(texture);
                        }
                    }
                }
                if (!routed) g_fo_rgb_parameter_routes.erase(texture);
            }
        }
        if (routed) {
            const LONG count = InterlockedIncrement(
                &g_fo_read_route_log_count);
            if (count <= 16) {
                log_line("Armada TGA parameter route: " + logical_name +
                         " -> " + retry_name + ".tga (result=" +
                         std::to_string(result) + ")");
            } else if (count == 17) {
                log_line("Further Fleet Ops legacy parameter-route logs "
                         "suppressed");
            }
        }
        return result;
    } catch (...) {
        log_line("Fleet Ops legacy parameter route threw; using native "
                 "result");
        return called ? result
                      : a2fo_rgb_call_thiscall_0(original, texture);
    }
}

int __stdcall fo_load_pixel_data_hook(void* texture, int device) noexcept {
    FoLoadPixelDataFn original =
        function_from_address<FoLoadPixelDataFn>(g_fo_load_pixels_hook.gateway);
    if (!original) return -1;

    bool called = false;
    bool routed = false;
    int result = -1;
    std::string logical_name;
    std::string retry_name;
    try {
        if (!ensure_fo_texture_path_lock()) return original(texture, device);
        {
            ScopedTexturePathLock path_lock;
            const bool mapped = fleetops_legacy_route(
                texture, logical_name, retry_name);
            const auto parameter_route =
                g_fo_rgb_parameter_routes.find(texture);
            routed = mapped &&
                parameter_route != g_fo_rgb_parameter_routes.end() &&
                parameter_route->second == retry_name;
            if (routed) {
                ScopedArmadaTexturePrefix empty_prefix;
                if (!empty_prefix.active()) routed = false;
                ScopedTextureNameOverride name_override(
                    texture, routed ? retry_name.c_str() : nullptr);
                if (routed && !name_override.active()) routed = false;
                if (routed) {
                    result = static_cast<int>(a2fo_rgb_call_thiscall_1(
                        at(g_armada, kLoadPixelDataRva), texture,
                        static_cast<std::uintptr_t>(device)));
                }
            }
            if (!routed) result = original(texture, device);
            called = true;
        }
        if (routed) {
            const LONG count = InterlockedIncrement(
                &g_fo_load_route_log_count);
            if (count <= 16) {
                log_line("Armada TGA pixel route: " + logical_name +
                         " -> " + retry_name + ".tga (result=" +
                         std::to_string(result) + ")");
            } else if (count == 17) {
                log_line("Further Fleet Ops legacy pixel-route logs "
                         "suppressed");
            }
        }
        return result;
    } catch (...) {
        log_line("Fleet Ops legacy pixel route threw; using native result");
        return called ? result : original(texture, device);
    }
}
#endif

std::int32_t texture_dimension(void* texture, std::size_t offset) {
    const auto* field = static_cast<const std::uint8_t*>(texture) + offset;
    return readable_range(field, sizeof(std::int32_t))
        ? *reinterpret_cast<const std::int32_t*>(field)
        : INT32_MIN;
}

std::uint8_t* __attribute__((fastcall)) lock_surface_hook(
    void* texture, void*, int device, std::uint32_t* pitch, RECT* rectangle,
    int flags) noexcept {
    void* original = g_lock_surface_hook.gateway;
    if (!original) return nullptr;

    std::uint8_t* pixels = reinterpret_cast<std::uint8_t*>(
        a2fo_rgb_call_thiscall_4(
            original, texture, static_cast<std::uintptr_t>(device),
            reinterpret_cast<std::uintptr_t>(pitch),
            reinterpret_cast<std::uintptr_t>(rectangle),
            static_cast<std::uintptr_t>(flags)));
    if (!pixels) {
        const LONG count = InterlockedIncrement(&g_recovery_log_count);
        if (count <= 16) {
            std::string name;
            std::string key;
            if (texture_name_and_key(texture, name, key) &&
                legacy_texture_key_exists(key)) {
                log_line("Legacy texture remains unavailable: " + name +
                         " (dimensions=" +
                         std::to_string(texture_dimension(texture, 0x1c)) +
                         "x" +
                         std::to_string(texture_dimension(texture, 0x20)) +
                         ")");
            }
        } else if (count == 17) {
            log_line("Further unavailable legacy texture logs suppressed");
        }
    }
    return pixels;
}

void __cdecl blend_pixels_xrgb888_hook(
    std::uintptr_t argument1, std::uintptr_t argument2,
    std::uintptr_t argument3, std::uintptr_t argument4,
    std::uintptr_t argument5, std::uintptr_t argument6,
    void* destination, const std::uint8_t* source,
    std::uintptr_t argument9, std::uintptr_t argument10,
    std::uintptr_t argument11) noexcept {
    if (!source) {
        const LONG count = InterlockedIncrement(&g_blend_guard_log_count);
        if (count <= 16) {
            log_line("Skipped XRGB888 texture blend with null source pixels");
        } else if (count == 17) {
            log_line("Further null XRGB888 texture-blend logs suppressed");
        }
        return;
    }
    BlendPixelsXrgb888Fn original =
        function_from_address<BlendPixelsXrgb888Fn>(g_blend_pixels_hook.gateway);
    if (original) {
        original(argument1, argument2, argument3, argument4, argument5,
                 argument6, destination, source, argument9, argument10,
                 argument11);
    }
}

int current_texture_device() {
    void** renderer_slot = at<void*>(g_armada, kRendererGlobalRva);
    if (!readable_range(renderer_slot, sizeof(*renderer_slot)) ||
        !*renderer_slot) {
        return -1;
    }
    const auto* device = static_cast<const std::uint8_t*>(*renderer_slot) +
        0xc0;
    if (!readable_range(device, sizeof(std::int32_t))) return -1;
    const std::int32_t value = *reinterpret_cast<const std::int32_t*>(device);
    return value >= 0 && value < 32 ? value : -1;
}

void* sprite_texture(void* sprite) {
    if (!sprite) return nullptr;
    return reinterpret_cast<void*>(a2fo_rgb_call_thiscall_0(
        at(g_armada, kSpriteGetTextureRva), sprite));
}

bool probe_texture_lock(void* texture, int device) {
    if (!texture || device < 0) return false;
    std::uint32_t pitch = 0;
    auto* pixels = reinterpret_cast<std::uint8_t*>(
        a2fo_rgb_call_thiscall_4(
            at(g_armada, kLockSurfaceRva), texture,
            static_cast<std::uintptr_t>(device),
            reinterpret_cast<std::uintptr_t>(&pitch), 0, 0));
    if (pixels) {
        a2fo_rgb_call_thiscall_1(
            at(g_armada, kUnlockSurfaceRva), texture,
            static_cast<std::uintptr_t>(device));
    }
    return pixels && pitch != 0 && texture_dimension(texture, 0x1c) > 0 &&
           texture_dimension(texture, 0x20) > 0;
}

void __attribute__((fastcall)) update_scan_grid_sprite_hook(
    void* radar, void*, std::uintptr_t flags, void* scan_grid,
    void* source_sprite, void* destination_sprite) noexcept {
    void* original = g_scan_grid_hook.gateway;
    if (!original) return;

    bool ready = false;
    try {
        const int device = current_texture_device();
        void* source_texture = sprite_texture(source_sprite);
        void* destination_texture = sprite_texture(destination_sprite);
        ready = probe_texture_lock(source_texture, device) &&
                probe_texture_lock(destination_texture, device);
    } catch (...) {
        ready = false;
    }
    if (!ready) {
        const LONG count = InterlockedIncrement(&g_scan_grid_guard_log_count);
        if (count <= 16) {
            log_line("Skipped scan-grid sprite update with unavailable pixels");
        } else if (count == 17) {
            log_line("Further unavailable scan-grid update logs suppressed");
        }
        return;
    }
    a2fo_rgb_call_thiscall_4(
        original, radar, flags, reinterpret_cast<std::uintptr_t>(scan_grid),
        reinterpret_cast<std::uintptr_t>(source_sprite),
        reinterpret_cast<std::uintptr_t>(destination_sprite));
}

void install_legacy_tga_stream_hooks() {
    if (!g_api || !g_api->install_inline_hook) {
        log_line("Inline-hook API unavailable; legacy TGA stream route "
                 "inactive");
        return;
    }
    if (!signature_matches(kFileExistsRva, kExpectedFileExists) ||
        !signature_matches(kOpenReadRva, kExpectedOpenRead)) {
        log_line("Armada file-stream signatures differ; legacy TGA stream "
                 "route inactive");
        return;
    }

    // OpenRead first makes a FileExists-only partial installation impossible.
    const bool open_installed = g_api->install_inline_hook(
        at(g_armada, kOpenReadRva), function_address(&open_read_hook),
        sizeof(kExpectedOpenRead), kExpectedOpenRead, &g_open_read_hook);
    if (!open_installed) {
        log_line("Could not install legacy TGA OpenRead route");
        return;
    }

    const bool exists_installed = g_api->install_inline_hook(
        at(g_armada, kFileExistsRva), function_address(&file_exists_hook),
        sizeof(kExpectedFileExists), kExpectedFileExists,
        &g_file_exists_hook);
    if (exists_installed) {
        log_line("Armada legacy TGA FileExists/OpenRead routes enabled");
    } else {
        log_line("Legacy TGA OpenRead route installed, but FileExists route "
                 "could not be installed");
    }
}

void install_texture_recovery_hooks() {
    if (!g_api || !g_api->install_inline_hook) {
        log_line("Inline-hook API unavailable; null minimap guard inactive");
        return;
    }
    if (!signature_matches(kLockSurfaceRva, kExpectedLockSurface) ||
        !signature_matches(kBlendPixelsXrgb888Rva,
                           kExpectedBlendPixelsXrgb888)) {
        log_line("Texture recovery hook signatures differ; null minimap guard "
                 "inactive");
        return;
    }

    const bool blend_installed = g_api->install_inline_hook(
        at(g_armada, kBlendPixelsXrgb888Rva),
        function_address(&blend_pixels_xrgb888_hook),
        sizeof(kExpectedBlendPixelsXrgb888), kExpectedBlendPixelsXrgb888,
        &g_blend_pixels_hook);
    if (!blend_installed) {
        log_line("Could not install null XRGB888 texture-blend guard");
        return;
    }

    const bool lock_installed = g_api->install_inline_hook(
        at(g_armada, kLockSurfaceRva), function_address(&lock_surface_hook),
        sizeof(kExpectedLockSurface), kExpectedLockSurface,
        &g_lock_surface_hook);
    if (lock_installed) {
        log_line("Legacy texture retry and null minimap guard enabled");
    } else {
        log_line("Null minimap guard enabled; legacy texture lock retry "
                 "unavailable");
    }

    if (!signature_matches(kUpdateScanGridSpriteRva,
                           kExpectedUpdateScanGridSprite) ||
        !signature_matches(kSpriteGetTextureRva,
                           kExpectedSpriteGetTexture) ||
        !signature_matches(kUnlockSurfaceRva, kExpectedUnlockSurface)) {
        log_line("Scan-grid texture guard dependency differs; guard inactive");
        return;
    }
    if (g_api->install_inline_hook(
            at(g_armada, kUpdateScanGridSpriteRva),
            function_address(&update_scan_grid_sprite_hook),
            sizeof(kExpectedUpdateScanGridSprite),
            kExpectedUpdateScanGridSprite, &g_scan_grid_hook)) {
        log_line("Null scan-grid texture guard enabled");
    } else {
        log_line("Could not install null scan-grid texture guard");
    }
}

void restore_fopen_bridge() {
    if (!g_fopen_patched || !g_fopen_slot || !g_original_fopen) return;
    FopenFn hook = &legacy_texture_fopen;
    if (!write_fopen_slot(function_address(hook),
                          function_address(g_original_fopen))) {
        log_line("Armada fopen import was changed by another owner; not restored");
    }
    g_fopen_patched = false;
}

void clear_conversion_cache() {
    if (g_conversion_lock_ready) {
        EnterCriticalSection(&g_conversion_lock);
        for (const std::string& file : g_generated_texture_files) {
            DeleteFileA(file.c_str());
        }
        g_generated_texture_files.clear();
        g_prepared_texture_files.clear();
        g_unusable_texture_files.clear();
        if (!g_conversion_cache_directory.empty()) {
            RemoveDirectoryA(g_conversion_cache_directory.c_str());
            g_conversion_cache_directory.clear();
        }
        LeaveCriticalSection(&g_conversion_lock);
        DeleteCriticalSection(&g_conversion_lock);
        g_conversion_lock_ready = false;
    } else {
        g_generated_texture_files.clear();
        g_prepared_texture_files.clear();
        g_unusable_texture_files.clear();
        g_conversion_cache_directory.clear();
    }
}

void clear_state() {
    restore_fopen_bridge();
    clear_conversion_cache();
    g_rgb_files.clear();
    g_index8_files.clear();
    g_compressed_files.clear();
    g_root_texture_files.clear();
    g_flattened_tga_candidates.clear();
    g_root_directory.clear();
    g_fopen_slot = nullptr;
    g_original_fopen = nullptr;
    g_armada = nullptr;
    InterlockedExchange(&g_redirect_log_count, 0);
    InterlockedExchange(&g_missing_log_count, 0);
    InterlockedExchange(&g_recovery_log_count, 0);
    InterlockedExchange(&g_blend_guard_log_count, 0);
    InterlockedExchange(&g_root_redirect_log_count, 0);
    InterlockedExchange(&g_scan_grid_guard_log_count, 0);
    InterlockedExchange(&g_conversion_log_count, 0);
    InterlockedExchange(&g_conversion_file_counter, 0);
    g_api = nullptr;
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module ||
        !api->extension_root_count ||
        !api->extension_root) {
        return false;
    }

    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    if (api->root_directory) {
        const char* root = api->root_directory();
        if (root) g_root_directory = root;
    }
    LegacyTextureDiscovery discovery;
    try {
        if (!discover_legacy_texture_files(discovery)) {
            log_line("No legacy Textures\\RGB, Textures\\Index8, or "
                     "Textures\\Compressed folder found; module inactive");
            clear_state();
            return true;
        }
    } catch (...) {
        log_line("Legacy texture file discovery failed");
        clear_state();
        return false;
    }

    if (!InitializeCriticalSectionAndSpinCount(&g_conversion_lock, 2000)) {
        log_line("Could not initialize legacy TGA preparation lock");
        clear_state();
        return false;
    }
    g_conversion_lock_ready = true;

    if (!validate_armada() || !install_fopen_bridge()) {
        clear_state();
        return false;
    }
    install_legacy_tga_stream_hooks();
    install_texture_recovery_hooks();
    log_line("Legacy texture fopen bridge enabled: " +
             std::to_string(g_rgb_files.size()) + " RGB, " +
             std::to_string(g_index8_files.size()) + " Index8, " +
             std::to_string(g_compressed_files.size()) + " Compressed, " +
             std::to_string(g_rgb_files.size() + g_index8_files.size() +
                            g_compressed_files.size()) +
             " safe flattened TGA candidates, and " +
             std::to_string(g_root_texture_files.size()) +
             " root-texture mappings across " +
             std::to_string(discovery.roots_found) +
             " legacy extension roots (RGB=" +
             std::to_string(discovery.rgb_roots) + ", Index8=" +
             std::to_string(discovery.index8_roots) + ", Compressed=" +
             std::to_string(discovery.compressed_roots) + ")");
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    clear_state();
}
