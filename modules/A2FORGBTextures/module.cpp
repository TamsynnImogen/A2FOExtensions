/*
 * File: modules/A2FORGBTextures/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Legacy RGB/Index8/Compressed texture fallback/redirector via
 * FileExists/OpenRead interception with safe null-source handling.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "../../rust/stage9_rgb_runtime_ffi.h"
#include "../../rust/stage10_rgb_completion_ffi.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

constexpr const char* kModuleName = "A2FORGBTextures";

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
    // Armada 1 stores manually authored mip levels as name_1.tga,
    // name_2.tga, and so on. Keep the validated base relationship so inherited
    // cross-root mip chains can be checked/synthesized safely, but preserve the
    // authored mip file itself for ordinary same-root requests. This avoids
    // substituting the full-resolution base image during distant LOD rendering.
    std::string legacy_mip_base;
    RootTgaRoute route = RootTgaRoute::none;
    // Extension roots are enumerated from lowest to highest precedence. Keep
    // the owner so an inherited mip chain cannot silently pair with a larger
    // base image supplied by a child mod.
    std::uint32_t root_index = 0;
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
std::unordered_set<std::string> g_compatible_texture_files;
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
volatile LONG g_manual_mip_redirect_log_count = 0;
volatile LONG g_cross_root_mip_synthesis_log_count = 0;

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
    switch (a2fo_rs_stage9_rgb_join_policy(
            reinterpret_cast<const std::uint8_t*>(left.data()),
            left.size(), right.size())) {
    case 0: return right;
    case 1: return left;
    case 2: return left + right;
    default: return left + "\\" + right;
    }
}

std::uint8_t native_ascii_lower(std::uint8_t value) noexcept {
    return static_cast<std::uint8_t>(std::tolower(value));
}

std::string lower_normalized(std::string value) {
    a2fo_rs_stage9_rgb_normalize(
        reinterpret_cast<std::uint8_t*>(value.data()), value.size(),
        &native_ascii_lower);
    return value;
}

bool native_true_colour_tga_header(const std::uint8_t* source,
                                   std::size_t size) {
    return a2fo_rs_stage9_rgb_native_tga_header(source, size);
}

enum class TgaPreparation {
    compatible,
    converted,
    unsupported,
};

struct TgaOutputContext {
    std::vector<std::uint8_t>* output;
    std::exception_ptr failure;
};

bool tga_emit(void* opaque, std::uint32_t operation,
              const std::uint8_t* bytes, std::size_t length,
              std::size_t capacity) noexcept {
    auto& context = *static_cast<TgaOutputContext*>(opaque);
    try {
        if (operation == 0) {
            context.output->assign(bytes, bytes + length);
            context.output->reserve(capacity);
        } else {
            context.output->insert(context.output->end(), bytes,
                                   bytes + length);
        }
        return true;
    } catch (...) {
        context.failure = std::current_exception();
        return false;
    }
}

TgaPreparation prepare_tga_for_rgb_loader(
    const std::vector<std::uint8_t>& source,
    std::vector<std::uint8_t>& output) {
    output.clear();
    TgaOutputContext context{&output, {}};
    const auto result = a2fo_rs_stage9_rgb_prepare_tga(
        source.data(), source.size(), &tga_emit, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return static_cast<TgaPreparation>(result);
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

bool read_binary_prefix(const std::string& path, std::uint8_t* bytes,
                        std::size_t size) {
    if (!bytes || size == 0 ||
        size > static_cast<std::size_t>(
                   std::numeric_limits<DWORD>::max())) {
        return false;
    }
    HANDLE file = CreateFileA(
        path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    DWORD read = 0;
    const bool ok = ReadFile(file, bytes, static_cast<DWORD>(size),
                             &read, nullptr) && read == size;
    CloseHandle(file);
    return ok;
}

using TgaHeaderSummary = A2foRgbTgaHeader;

bool tga_filename(const std::string& filename);

bool read_tga_header_summary(const std::string& path,
                             TgaHeaderSummary& summary) {
    std::array<std::uint8_t, 18> header{};
    if (!read_binary_prefix(path, header.data(), header.size())) return false;
    return a2fo_rs_stage9_rgb_header_summary(
        header.data(), header.size(), &summary);
}

bool same_tga_pixel_format(const TgaHeaderSummary& left,
                           const TgaHeaderSummary& right) {
    return a2fo_rs_stage9_rgb_same_pixel_format(&left, &right);
}

bool parse_manual_mip_key(const std::string& key, std::string& stem,
                          std::uint32_t& level) {
    stem.clear();
    level = 0;
    A2foRgbMip mip{};
    if (!a2fo_rs_stage9_rgb_manual_mip(
            reinterpret_cast<const std::uint8_t*>(key.data()),
            key.size(), &mip)) return false;
    stem.assign(key, 0, mip.stem_length);
    level = mip.level;
    return true;
}

struct MipChainContext {
    MipChainContext(const std::unordered_map<std::string, std::string>& entries,
                    const std::string& requested_stem,
                    const TgaHeaderSummary& original)
        : files(entries), stem(requested_stem), base(original),
          previous(original) {}
    const std::unordered_map<std::string, std::string>& files;
    const std::string& stem;
    TgaHeaderSummary base;
    TgaHeaderSummary previous;
    std::exception_ptr failure;
};

std::uint32_t rgb_mip_chain_step(void* opaque, std::uint32_t,
                                 std::size_t current) noexcept {
    auto& context = *static_cast<MipChainContext*>(opaque);
    try {
        const std::string current_key = context.stem + "_" +
            std::to_string(current) + ".tga";
        const auto mip = context.files.find(current_key);
        if (mip == context.files.end()) return 0;
        TgaHeaderSummary header{};
        if (!read_tga_header_summary(mip->second, header) ||
            !a2fo_rs_stage9_rgb_mip_step(
                context.base.width, context.base.height,
                context.previous.width, context.previous.height,
                header.width, header.height,
                same_tga_pixel_format(context.base, header))) return 0;
        context.previous = header;
        return 1;
    } catch (...) {
        context.failure = std::current_exception();
        return UINT32_MAX;
    }
}

std::string validated_manual_mip_base(
    const std::unordered_map<std::string, std::string>& files,
    const std::string& key) {
    std::string stem;
    std::uint32_t level = 0;
    if (!parse_manual_mip_key(key, stem, level)) return {};
    const auto base = files.find(stem + ".tga");
    if (base == files.end()) return {};
    TgaHeaderSummary base_header{};
    if (!read_tga_header_summary(base->second, base_header)) return {};
    // Preserve the complete, same-format, decreasing-aspect chain contract.
    MipChainContext context{files, stem, base_header};
    const bool valid = a2fo_rs_stage9_rgb_validate_mip_chain(
        level, &rgb_mip_chain_step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return valid ? base->second : std::string{};
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

struct TgaResizeContext {
    std::vector<std::uint8_t>* output;
    std::exception_ptr failure;
};

std::uint8_t* tga_resize_allocate(void* opaque, std::size_t size) noexcept {
    auto& context = *static_cast<TgaResizeContext*>(opaque);
    try {
        context.output->assign(size, 0);
        return context.output->data();
    } catch (...) {
        context.failure = std::current_exception();
        return nullptr;
    }
}

bool resize_true_colour_tga(const std::vector<std::uint8_t>& source,
                            std::uint16_t target_width,
                            std::uint16_t target_height,
                            std::vector<std::uint8_t>& output) {
    output.clear();
    TgaResizeContext context{&output, {}};
    const bool result = a2fo_rs_stage9_rgb_resize_tga(
        source.data(), source.size(), target_width, target_height,
        &tga_resize_allocate, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return result;
}

struct TgaWorkflowContext {
    TgaWorkflowContext(const std::string& source_value,
                       std::string& prepared_value,
                       const std::string& cache_key_value,
                       std::uint16_t width = 0,
                       std::uint16_t height = 0,
                       bool resized = false)
        : source(source_value), prepared(prepared_value),
          cache_key(cache_key_value), target_width(width),
          target_height(height), resizing(resized) {}

    const std::string& source;
    std::string& prepared;
    const std::string& cache_key;
    std::uint16_t target_width = 0;
    std::uint16_t target_height = 0;
    bool resizing = false;
    std::vector<std::uint8_t> input;
    std::vector<std::uint8_t> converted;
    std::vector<std::uint8_t> resized;
    TgaPreparation result = TgaPreparation::unsupported;
    std::exception_ptr failure;
};

std::uint32_t tga_workflow_step(void* opaque, std::uint32_t operation,
                                std::size_t) noexcept {
    auto& context = *static_cast<TgaWorkflowContext*>(opaque);
    try {
        if (context.resizing) {
            switch (operation) {
            case 0: {
                const auto cached = g_prepared_texture_files.find(
                    context.cache_key);
                if (cached == g_prepared_texture_files.end()) return 0;
                context.prepared = cached->second;
                return 1;
            }
            case 1:
                return g_unusable_texture_files.count(context.cache_key) != 0;
            case 2:
                return read_binary_file(context.source, context.input);
            case 3:
                context.result = prepare_tga_for_rgb_loader(
                    context.input, context.converted);
                return static_cast<std::uint32_t>(context.result);
            case 4: {
                const auto* pixels = context.result == TgaPreparation::compatible
                    ? &context.input
                    : context.result == TgaPreparation::converted
                        ? &context.converted : nullptr;
                return pixels && resize_true_colour_tga(
                    *pixels, context.target_width, context.target_height,
                    context.resized) &&
                    write_prepared_tga(context.resized, context.prepared);
            }
            case 5:
                g_prepared_texture_files[context.cache_key] = context.prepared;
                g_generated_texture_files.push_back(context.prepared);
                return 1;
            case 6:
                g_unusable_texture_files.insert(context.cache_key);
                return 1;
            default:
                return 0;
            }
        }
        switch (operation) {
        case 0: {
            const auto cached = g_prepared_texture_files.find(context.cache_key);
            if (cached == g_prepared_texture_files.end()) return 0;
            context.prepared = cached->second;
            return 1;
        }
        case 1:
            if (g_compatible_texture_files.count(context.cache_key) == 0)
                return 0;
            context.prepared = context.source;
            return 1;
        case 2:
            return g_unusable_texture_files.count(context.cache_key) != 0;
        case 3: {
            std::array<std::uint8_t, 18> header{};
            if (!read_binary_prefix(context.source, header.data(), header.size()) ||
                !native_true_colour_tga_header(header.data(), header.size()))
                return 0;
            g_compatible_texture_files.insert(context.cache_key);
            context.prepared = context.source;
            return 1;
        }
        case 4: {
            const bool read = read_binary_file(context.source, context.input);
            context.result = read
                ? prepare_tga_for_rgb_loader(context.input, context.converted)
                : TgaPreparation::unsupported;
            return static_cast<std::uint32_t>(context.result);
        }
        case 5:
            g_compatible_texture_files.insert(context.cache_key);
            context.prepared = context.source;
            return 1;
        case 6:
            return write_prepared_tga(context.converted, context.prepared);
        case 7: {
            g_prepared_texture_files[context.cache_key] = context.prepared;
            g_generated_texture_files.push_back(context.prepared);
            const LONG count = InterlockedIncrement(&g_conversion_log_count);
            if (count <= 16) {
                log_line("Prepared legacy TGA for the true-colour loader: " +
                         context.source + " -> " + context.prepared);
            } else if (count == 17) {
                log_line("Further legacy TGA preparation logs suppressed");
            }
            return 1;
        }
        case 8:
            g_unusable_texture_files.insert(context.cache_key);
            log_line("Unsupported legacy TGA cannot use the true-colour fallback: " +
                     context.source);
            return 1;
        default:
            return 0;
        }
    } catch (...) {
        context.failure = std::current_exception();
        return UINT32_MAX;
    }
}

bool prepare_resized_legacy_tga(const std::string& source,
                                std::uint16_t target_width,
                                std::uint16_t target_height,
                                std::string& prepared) {
    prepared.clear();
    ConversionLockGuard lock;
    if (!lock.locked()) return false;
    const std::string cache_key = lower_normalized(source) + "|mip=" +
        std::to_string(target_width) + "x" +
        std::to_string(target_height);
    TgaWorkflowContext context{source, prepared, cache_key,
                               target_width, target_height, true};
    const bool result = a2fo_rs_stage9_rgb_resize_workflow(
        &tga_workflow_step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return result;
}

bool prepare_legacy_tga(const std::string& source,
                        std::string& prepared) {
    prepared.clear();
    ConversionLockGuard lock;
    if (!lock.locked()) return false;
    const std::string cache_key = lower_normalized(source);
    TgaWorkflowContext context{source, prepared, cache_key};
    const bool result = a2fo_rs_stage9_rgb_prepare_workflow(
        &tga_workflow_step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return result;
}

bool ascii_equal(const std::string& left, const char* right) {
    if (!right) return false;
    return a2fo_rs_stage9_rgb_ascii_equal(
        reinterpret_cast<const std::uint8_t*>(left.data()), left.size(),
        reinterpret_cast<const std::uint8_t*>(right), std::strlen(right),
        &native_ascii_lower);
}

bool is_legacy_texture_directory(const std::string& name) {
    return a2fo_rs_stage9_rgb_legacy_directory(
        reinterpret_cast<const std::uint8_t*>(name.data()), name.size(),
        &native_ascii_lower);
}

bool tga_filename(const std::string& filename);
bool dds_filename(const std::string& filename);

struct ChildDirectoryContext {
    ChildDirectoryContext(const std::string& directory,
                          const char* requested)
        : parent(directory), wanted(requested) {}

    const std::string& parent;
    const char* wanted;
    std::string result;
    WIN32_FIND_DATAA data{};
    HANDLE search = INVALID_HANDLE_VALUE;
    std::exception_ptr failure;
};

std::uint32_t rgb_find_child_step(void* opaque, std::uint32_t operation,
                                  std::size_t) noexcept {
    auto& context = *static_cast<ChildDirectoryContext*>(opaque);
    try {
        if (operation == 0) {
            context.search = FindFirstFileA(
                join_path(context.parent, "*").c_str(), &context.data);
            return context.search != INVALID_HANDLE_VALUE;
        }
        if (operation == 1) {
            const std::string name = context.data.cFileName;
            if ((context.data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
                (context.data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 ||
                name == "." || name == ".." ||
                !ascii_equal(name, context.wanted)) return 0;
            context.result = join_path(context.parent, name);
            return 1;
        }
        if (operation == 2)
            return FindNextFileA(context.search, &context.data) != 0;
        if (operation == 3) {
            FindClose(context.search);
            return 1;
        }
        return 0;
    } catch (...) {
        context.failure = std::current_exception();
        return UINT32_MAX;
    }
}

std::string find_child_directory(const std::string& parent,
                                 const char* wanted) {
    ChildDirectoryContext context{parent, wanted};
    a2fo_rs_stage9_rgb_find_child(&rgb_find_child_step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return std::move(context.result);
}

struct TextureScanFrame {
    std::string directory;
    std::string relative;
    std::string physical;
    std::string child_relative;
    bool exclude_top_level_legacy = false;
    WIN32_FIND_DATAA data{};
    HANDLE search = INVALID_HANDLE_VALUE;
};

struct TextureScanContext {
    std::array<TextureScanFrame, 34> frames;
    std::unordered_map<std::string, std::string>& files;
    std::exception_ptr failure;
};

std::uint32_t rgb_scan_step(void* opaque, std::uint32_t operation,
                            std::size_t depth) noexcept {
    auto& context = *static_cast<TextureScanContext*>(opaque);
    auto& frame = context.frames[depth];
    try {
        if (operation == 0) {
            frame.search = FindFirstFileA(
                join_path(frame.directory, "*").c_str(), &frame.data);
            return frame.search != INVALID_HANDLE_VALUE;
        }
        if (operation == 1) {
            const std::string name = frame.data.cFileName;
            if (name == "." || name == ".." ||
                (frame.data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                return 0;
            frame.physical = join_path(frame.directory, name);
            frame.child_relative = frame.relative.empty()
                ? name : join_path(frame.relative, name);
            if ((frame.data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                if (frame.exclude_top_level_legacy && frame.relative.empty() &&
                    is_legacy_texture_directory(name)) return 0;
                return 1;
            }
            return 2;
        }
        if (operation == 2) {
            auto& child = context.frames[depth + 1];
            child.directory = frame.physical;
            child.relative = frame.child_relative;
            child.exclude_top_level_legacy = false;
            return 1;
        }
        if (operation == 3)
            return FindNextFileA(frame.search, &frame.data) != 0;
        if (operation == 4) {
            FindClose(frame.search);
            return 1;
        }
        if (operation == 5) {
            context.files[lower_normalized(frame.child_relative)] =
                frame.physical;
            return 1;
        }
        return 0;
    } catch (...) {
        context.failure = std::current_exception();
        return UINT32_MAX;
    }
}

void scan_texture_directory(const std::string& directory,
                            const std::string& relative,
                            std::uint32_t depth,
                            bool exclude_top_level_legacy,
                            std::unordered_map<std::string, std::string>& files) {
    if (depth > 32) return;
    TextureScanContext context{{}, files, {}};
    context.frames[0].directory = directory;
    context.frames[0].relative = relative;
    context.frames[0].exclude_top_level_legacy = exclude_top_level_legacy;
    a2fo_rs_stage9_rgb_scan_directory(&rgb_scan_step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
}

using LegacyTextureDiscovery = A2foRgbDiscovery;

struct RootScanState {
    std::string textures;
    std::unordered_map<std::string, std::string> root_files;
    std::unordered_map<std::string, std::string> rgb_files;
    std::unordered_map<std::string, std::string> index8_files;
    std::unordered_map<std::string, std::string> compressed_files;
    std::unordered_map<std::string, RootTgaCandidate> candidates;
    std::array<std::string, 3> folders;
};

struct RgbDiscoveryContext {
    std::optional<RootScanState> root;
    std::uint32_t root_index = 0;
    std::exception_ptr failure;
};

using RgbFileMap = std::unordered_map<std::string, std::string>;

struct RgbFileTraversal {
    RgbFileTraversal(RgbDiscoveryContext& discovery, RgbFileMap& source,
                     std::uint32_t mode, std::size_t route)
        : discovery(discovery), source(source), next(source.cbegin()),
          current(source.cend()), mode(mode), route(route) {}

    RgbDiscoveryContext& discovery;
    RgbFileMap& source;
    RgbFileMap::const_iterator next;
    RgbFileMap::const_iterator current;
    std::uint32_t mode;
    std::size_t route;
};

std::uint32_t rgb_file_traversal_step(void* opaque, std::uint32_t operation,
                                      std::size_t) noexcept {
    auto& traversal = *static_cast<RgbFileTraversal*>(opaque);
    try {
        if (operation == 0) {
            if (traversal.next == traversal.source.cend()) return 0;
            traversal.current = traversal.next++;
            return 1;
        }
        const auto& file = *traversal.current;
        if (operation == 1) {
            return traversal.mode == 3 ? dds_filename(file.first)
                 : traversal.mode == 6 ? tga_filename(file.first) : 0;
        }
        if (operation == 2) {
            if (traversal.mode == 3) {
                g_root_texture_files[file.first] = file.second;
            } else if (traversal.mode == 5) {
                auto* exact = traversal.route == 0 ? &g_compressed_files
                            : traversal.route == 1 ? &g_index8_files
                                                   : &g_rgb_files;
                (*exact)[file.first] = file.second;
            } else if (traversal.mode == 6) {
                const RootTgaRoute route = traversal.route == 0
                    ? RootTgaRoute::compressed
                    : traversal.route == 1 ? RootTgaRoute::index8
                    : traversal.route == 2 ? RootTgaRoute::rgb
                                           : RootTgaRoute::root;
                traversal.discovery.root->candidates[file.first] = {
                    file.second,
                    validated_manual_mip_base(traversal.source, file.first),
                    route,
                    traversal.discovery.root_index};
            }
            return 1;
        }
        if (operation == 3 && traversal.mode == 3) {
            A2foRgbSpan basename{};
            if (a2fo_rs_stage9_rgb_dds_alias(
                    reinterpret_cast<const std::uint8_t*>(file.first.data()),
                    file.first.size(), &basename)) {
                g_root_texture_files[file.first.substr(
                    basename.start, basename.length)] = file.second;
            }
            return 1;
        }
        return 0;
    } catch (...) {
        traversal.discovery.failure = std::current_exception();
        return UINT32_MAX;
    }
}

struct RgbCandidateTraversal {
    explicit RgbCandidateTraversal(RgbDiscoveryContext& discovery)
        : discovery(discovery), next(discovery.root->candidates.cbegin()),
          current(discovery.root->candidates.cend()) {}

    RgbDiscoveryContext& discovery;
    std::unordered_map<std::string, RootTgaCandidate>::const_iterator next;
    std::unordered_map<std::string, RootTgaCandidate>::const_iterator current;
};

std::uint32_t rgb_candidate_traversal_step(void* opaque,
                                           std::uint32_t operation,
                                           std::size_t) noexcept {
    auto& traversal = *static_cast<RgbCandidateTraversal*>(opaque);
    try {
        if (operation == 0) {
            if (traversal.next == traversal.discovery.root->candidates.cend())
                return 0;
            traversal.current = traversal.next++;
            return 1;
        }
        if (operation == 2) {
            const auto& candidate = *traversal.current;
            g_flattened_tga_candidates[candidate.first].push_back(
                candidate.second);
            return 1;
        }
        return 0;
    } catch (...) {
        traversal.discovery.failure = std::current_exception();
        return UINT32_MAX;
    }
}

std::uint32_t run_rgb_file_traversal(RgbDiscoveryContext& context,
                                     RgbFileMap& source,
                                     std::uint32_t mode,
                                     std::size_t route = 0) {
    RgbFileTraversal traversal(context, source, mode, route);
    return a2fo_rs_stage10_rgb_traverse(
        mode, &rgb_file_traversal_step, &traversal) ? 1 : UINT32_MAX;
}

std::uint32_t rgb_discovery_step(void* opaque, std::uint32_t operation,
                                 std::size_t index) noexcept {
    auto& context = *static_cast<RgbDiscoveryContext*>(opaque);
    try {
        if (operation == 0) {
            const std::uint32_t count = g_api->extension_root_count();
            return count > 4096 ? 4097 : count;
        }
        if (operation == 8) {
            log_line("Extension-root count is invalid");
            return 1;
        }
        if (operation == 1) {
            const char* path = g_api->extension_root(
                static_cast<std::uint32_t>(index));
            if (!path || !*path) return 0;
            const std::string textures = find_child_directory(path, "Textures");
            if (textures.empty()) return 0;
            context.root.emplace();
            context.root->textures = textures;
            context.root_index = static_cast<std::uint32_t>(index);
            return 1;
        }
        auto& root = *context.root;
        if (operation == 2) {
            scan_texture_directory(root.textures, {}, 0, true,
                                   root.root_files);
            return 1;
        }
        if (operation == 3) {
            return run_rgb_file_traversal(context, root.root_files, 3);
        }
        const char* names[] = {"Compressed", "Index8", "RGB"};
        if (operation == 4) {
            root.folders[index] = find_child_directory(
                root.textures, names[index]);
            return !root.folders[index].empty();
        }
        auto* files = index == 0 ? &root.compressed_files
                    : index == 1 ? &root.index8_files : &root.rgb_files;
        if (operation == 5) {
            scan_texture_directory(root.folders[index], {}, 0, false, *files);
            return run_rgb_file_traversal(context, *files, 5, index);
        }
        if (operation == 6) {
            auto* candidates = index == 0 ? &root.compressed_files
                             : index == 1 ? &root.index8_files
                             : index == 2 ? &root.rgb_files
                                          : &root.root_files;
            return run_rgb_file_traversal(context, *candidates, 6, index);
        }
        if (operation == 7) {
            RgbCandidateTraversal traversal(context);
            return a2fo_rs_stage10_rgb_traverse(
                7, &rgb_candidate_traversal_step, &traversal)
                ? 1 : UINT32_MAX;
        }
        return 0;
    } catch (...) {
        context.failure = std::current_exception();
        return UINT32_MAX;
    }
}

bool discover_legacy_texture_files(LegacyTextureDiscovery& discovery) {
    discovery = {};
    if (!g_api || !g_api->extension_root_count || !g_api->extension_root) {
        return false;
    }
    RgbDiscoveryContext context;
    const bool result = a2fo_rs_stage9_rgb_discover(
        &rgb_discovery_step, &context, &discovery);
    if (context.failure) std::rethrow_exception(context.failure);
    return result;
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
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        log_line("ArmadaL.exe has no valid PE header");
        return false;
    }
    const auto identity = a2fo::supported_armada::identify(g_armada);
    if (identity == a2fo::supported_armada::Identity::unsupported) {
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
    if (identity == a2fo::supported_armada::Identity::normalized) {
        log_line(
            "Accepted normalized ArmadaL.exe header from supported "
            "on-disk image fingerprint");
    }
    // Fleet Operations installs its own texture callbacks before native A2FO
    // modules load and may rewrite code in this chain. These bytes are useful
    // provenance diagnostics, not safe runtime gates. The supported image
    // identity, legacy literal, and fopen import ownership below are the hard
    // gates for the one pointer this module actually modifies.
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
    const std::array<const std::unordered_map<std::string, std::string>*, 3>
        files{{&g_rgb_files, &g_index8_files, &g_compressed_files}};
    const auto index = a2fo_rs_stage9_rgb_direct_index(
        static_cast<std::uint32_t>(request));
    return index < files.size() ? files[index] : nullptr;
}

const char* direct_legacy_folder_name(TextureRequest request) {
    constexpr std::array<const char*, 4> labels{{
        "RGB", "Index8", "Compressed", "legacy"}};
    return labels[a2fo_rs_stage9_rgb_direct_index(
        static_cast<std::uint32_t>(request))];
}

const char* root_tga_route_name(RootTgaRoute route) {
    constexpr std::array<const char*, 5> labels{{
        "root Textures", "RGB", "Index8 (prepared true-colour)",
        "Compressed (prepared true-colour)", "none"}};
    return labels[a2fo_rs_stage9_rgb_root_label(
        static_cast<std::uint32_t>(route))];
}

struct RootSelectionContext {
    RootSelectionContext(const std::string& requested,
                         std::string& output,
                         RootTgaRoute& selected,
                         const std::vector<RootTgaCandidate>* entries)
        : key(requested), physical(output), route(selected),
          candidates(entries) {}

    const std::string& key;
    std::string& physical;
    RootTgaRoute& route;
    const std::vector<RootTgaCandidate>* candidates;
    const std::vector<RootTgaCandidate>* bases = nullptr;
    const RootTgaCandidate* effective_base = nullptr;
    TgaHeaderSummary effective_base_header{};
    std::uint32_t mip_level = 0;
    std::uint16_t expected_width = 0;
    std::uint16_t expected_height = 0;
    std::exception_ptr failure;
};

std::uint32_t rgb_root_selection_step(void* opaque,
                                       std::uint32_t operation,
                                       std::size_t index) noexcept {
    auto& context = *static_cast<RootSelectionContext*>(opaque);
    try {
        if (operation == 0) {
            std::string stem;
            if (!parse_manual_mip_key(context.key, stem, context.mip_level))
                return 0;
            const auto found = g_flattened_tga_candidates.find(stem + ".tga");
            if (found == g_flattened_tga_candidates.end()) return 0;
            context.bases = &found->second;
            return static_cast<std::uint32_t>(context.bases->size());
        }
        if (operation == 1) {
            const auto& candidate = (*context.bases)[index];
            if (!read_tga_header_summary(candidate.physical,
                                         context.effective_base_header))
                return 0;
            context.effective_base = &candidate;
            return 1;
        }
        if (operation == 2)
            return static_cast<std::uint32_t>(context.candidates->size());
        const auto& candidate = (*context.candidates)[index];
        if (operation == 3) {
            if (candidate.legacy_mip_base.empty() || !context.effective_base)
                return 0;
            return a2fo_rs_stage9_rgb_synthesis_dimensions(
                context.effective_base->root_index, candidate.root_index,
                context.effective_base_header.width,
                context.effective_base_header.height, context.mip_level,
                &context.expected_width, &context.expected_height);
        }
        if (operation == 4) {
            TgaHeaderSummary inherited{};
            return a2fo_rs_stage9_rgb_inherited_is_large_enough(
                read_tga_header_summary(candidate.physical, inherited),
                inherited.width, inherited.height,
                context.expected_width, context.expected_height);
        }
        if (operation == 5) {
            if (!prepare_resized_legacy_tga(
                    context.effective_base->physical,
                    context.expected_width, context.expected_height,
                    context.physical)) return 0;
            context.route = context.effective_base->route;
            const LONG count = InterlockedIncrement(
                &g_cross_root_mip_synthesis_log_count);
            if (count <= 16) {
                log_line("Synthesized inherited manual mip from " +
                         context.effective_base->physical + " at " +
                         std::to_string(context.expected_width) + "x" +
                         std::to_string(context.expected_height) + " -> " +
                         context.physical);
            } else if (count == 17) {
                log_line("Further inherited manual-mip synthesis "
                         "logs suppressed");
            }
            return 1;
        }
        if (operation == 6) {
            if (!prepare_legacy_tga(candidate.physical, context.physical))
                return 0;
            if (!candidate.legacy_mip_base.empty()) {
                const LONG count = InterlockedIncrement(
                    &g_manual_mip_redirect_log_count);
                if (count <= 16) {
                    log_line("Legacy manual-mip root request preserved: " +
                             candidate.physical + " -> " + context.physical);
                } else if (count == 17) {
                    log_line("Further legacy manual-mip preservation logs "
                             "suppressed");
                }
            }
            context.route = candidate.route;
            return 1;
        }
        return 0;
    } catch (...) {
        context.failure = std::current_exception();
        return UINT32_MAX;
    }
}

bool select_root_tga(const std::string& key, std::string& physical,
                     RootTgaRoute& route) {
    physical.clear();
    route = RootTgaRoute::none;
    const auto found = g_flattened_tga_candidates.find(key);
    if (found == g_flattened_tga_candidates.end()) return false;
    // Rust walks the effective base and precedence chain. The native step
    // retains file reads, conversion cache, log counters, and source objects.
    RootSelectionContext context{key, physical, route, &found->second};
    const bool result = a2fo_rs_stage9_rgb_select_root(
        &rgb_root_selection_step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return result;
}

bool tga_filename(const std::string& filename) {
    return a2fo_rs_stage9_rgb_tga_filename(
        reinterpret_cast<const std::uint8_t*>(filename.data()),
        filename.size());
}

bool dds_filename(const std::string& filename) {
    return a2fo_rs_stage9_rgb_dds_filename(
        reinterpret_cast<const std::uint8_t*>(filename.data()),
        filename.size());
}

TextureRequest texture_request(const char* filename, const char* mode,
                               std::string& key) {
    if (!filename || !*filename || !mode || mode[0] != 'r') {
        return TextureRequest::none;
    }
    std::string normalized = lower_normalized(filename);
    A2foRgbSpan span{};
    const std::uint32_t request = a2fo_rs_stage9_rgb_request(
        reinterpret_cast<const std::uint8_t*>(normalized.data()),
        normalized.size(), &span);
    key = normalized.substr(span.start, span.length);
    return static_cast<TextureRequest>(request);
}

struct FopenRouteContext {
    FopenRouteContext(FopenFn native_open, const char* requested,
                     const char* requested_mode, const std::string& lookup_key,
                     TextureRequest lookup_route)
        : original(native_open), filename(requested), mode(requested_mode),
          key(lookup_key), request(lookup_route) {}

    FopenFn original;
    const char* filename;
    const char* mode;
    const std::string& key;
    TextureRequest request;
    std::string physical;
    const std::string* legacy = nullptr;
    RootTgaRoute route = RootTgaRoute::none;
    FILE* file = nullptr;
    std::exception_ptr failure;
};

std::uint32_t rgb_fopen_step(void* opaque, std::uint32_t operation,
                             std::size_t) noexcept {
    auto& context = *static_cast<FopenRouteContext*>(opaque);
    try {
        if (operation == 0) {
            const auto root = g_root_texture_files.find(context.key);
            if (root == g_root_texture_files.end()) return 0;
            context.file = context.original(root->second.c_str(), context.mode);
            if (!context.file) return 0;
            if (InterlockedCompareExchange(
                    &g_root_redirect_log_count, 1, 0) == 0) {
                log_line(std::string("First root texture redirect: ") +
                         context.filename + " -> " + root->second);
            }
            return 1;
        }
        if (operation == 1) {
            if (!select_root_tga(context.key, context.physical,
                                 context.route)) return 0;
            context.file = context.original(context.physical.c_str(),
                                            context.mode);
            if (!context.file) return 0;
            if (context.route == RootTgaRoute::root) {
                if (InterlockedCompareExchange(
                        &g_root_redirect_log_count, 1, 0) == 0) {
                    log_line(std::string("First root texture redirect: ") +
                             context.filename + " -> " + context.physical);
                }
            } else if (InterlockedCompareExchange(
                           &g_redirect_log_count, 1, 0) == 0) {
                log_line(std::string("First legacy texture fallback (") +
                         root_tga_route_name(context.route) + "): " +
                         context.filename + " -> " + context.physical);
            }
            return 1;
        }
        if (operation == 2) {
            const LONG missing = InterlockedIncrement(&g_missing_log_count);
            if (missing <= 16) {
                log_line(std::string("Legacy texture file not mapped: ") +
                         context.filename);
            } else if (missing == 17) {
                log_line("Further unmapped legacy texture filenames "
                         "suppressed");
            }
            return 1;
        }
        if (operation == 3) {
            const auto* files = direct_legacy_files(context.request);
            const auto found = files->find(context.key);
            if (found == files->end()) return 0;
            context.legacy = &found->second;
            context.physical = *context.legacy;
            return 1;
        }
        if (operation == 4) {
            if (context.request == TextureRequest::direct_rgb &&
                tga_filename(context.key)) {
                return prepare_legacy_tga(*context.legacy,
                                          context.physical);
            }
            return 1;
        }
        if (operation == 6) {
            if (InterlockedCompareExchange(
                    &g_redirect_log_count, 1, 0) == 0) {
                std::string route = "First legacy texture fallback (";
                route += direct_legacy_folder_name(context.request);
                route += ")";
                log_line(route + ": " + context.filename + " -> " +
                         context.physical);
            }
            context.file = context.original(context.physical.c_str(),
                                            context.mode);
            return 1;
        }
        return 0;
    } catch (...) {
        context.failure = std::current_exception();
        return UINT32_MAX;
    }
}

FILE* __cdecl legacy_texture_fopen(const char* filename,
                                   const char* mode) noexcept {
    FopenFn original = g_original_fopen;
    if (!original) return nullptr;
    try {
        std::string key;
        const TextureRequest request = texture_request(filename, mode, key);
        FopenRouteContext context{original, filename, mode, key, request};
        const bool fallback = a2fo_rs_stage9_rgb_fopen_route(
            static_cast<std::uint32_t>(request), &rgb_fopen_step, &context);
        if (!context.failure && request != TextureRequest::none) {
            if (fallback) return original(filename, mode);
            return context.file;
        }
    } catch (...) {
        // Never throw through msvcrt or the Armada caller.
    }
    return original(filename, mode);
}

struct MappedTextureContext {
    MappedTextureContext(const std::string& key_value,
                         TextureRequest request_value,
                         std::string& output)
        : key(key_value), request(request_value), physical(output),
          direct_files(direct_legacy_files(request_value)) {}

    const std::string& key;
    TextureRequest request;
    std::string& physical;
    const std::unordered_map<std::string, std::string>* direct_files;
    const std::string* legacy = nullptr;
    std::exception_ptr failure;
};

std::uint32_t rgb_mapped_step(void* opaque, std::uint32_t operation,
                              std::size_t) noexcept {
    auto& context = *static_cast<MappedTextureContext*>(opaque);
    try {
        if (operation == 0) {
            RootTgaRoute route = RootTgaRoute::none;
            return select_root_tga(context.key, context.physical, route);
        }
        if (operation == 1) {
            const auto found = context.direct_files->find(context.key);
            if (found == context.direct_files->end()) return 0;
            context.legacy = &found->second;
            return 1;
        }
        if (operation == 2) {
            // Armada probes RGB/TGA even when Legacy supplied only DDS.
            const std::string dds_key =
                context.key.substr(0, context.key.size() - 4u) + ".dds";
            const auto dds = g_root_texture_files.find(dds_key);
            if (dds == g_root_texture_files.end()) return 0;
            context.physical = dds->second;
            return 1;
        }
        if (operation == 3)
            return prepare_legacy_tga(*context.legacy, context.physical);
        if (operation == 4) {
            context.physical = *context.legacy;
            return 1;
        }
        return 0;
    } catch (...) {
        context.failure = std::current_exception();
        return UINT32_MAX;
    }
}

bool mapped_tga_path(const char* filename, std::string& physical) {
    physical.clear();
    std::string key;
    const TextureRequest request = texture_request(filename, "rb", key);
    MappedTextureContext context{key, request, physical};
    const bool result = a2fo_rs_stage9_rgb_mapped_route(
        static_cast<std::uint32_t>(request),
        request == TextureRequest::direct_rgb && tga_filename(key),
        &rgb_mapped_step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return result;
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
    struct Context {
        void* texture;
        std::string& name;
        std::string& key;
        std::string normalized;
        const char* source = nullptr;
        std::exception_ptr failure;
    } context{texture, name, key, {}, nullptr, {}};
    const auto step = +[](void* opaque, std::uint32_t operation,
                          std::size_t) noexcept -> std::uint32_t {
        auto& state = *static_cast<Context*>(opaque);
        try {
            switch (operation) {
                case 0: return state.texture != nullptr;
                case 1:
                    return readable_range(
                        static_cast<const std::uint8_t*>(state.texture) +
                        0x08, sizeof(const char*));
                case 2:
                    state.source = *reinterpret_cast<const char* const*>(
                        static_cast<const std::uint8_t*>(state.texture) +
                        0x08);
                    return bounded_string(state.source, state.name);
                case 3: {
                    state.normalized = lower_normalized(state.name);
                    A2foRgbSpan span{};
                    const std::uint32_t suffix =
                        a2fo_rs_stage9_rgb_name_key(
                            reinterpret_cast<const std::uint8_t*>(
                                state.normalized.data()),
                            state.normalized.size(), &span);
                    state.key = state.normalized.substr(
                        span.start, span.length);
                    return suffix;
                }
                case 4:
                    state.key.replace(state.key.size() - 4, 4, ".tga");
                    return 0;
                case 5: state.key += ".tga"; return 0;
                case 6: return !state.key.empty();
                default: return 0;
            }
        } catch (...) {
            state.failure = std::current_exception();
            return UINT32_MAX;
        }
    };
    const bool result = a2fo_rs_stage9_rgb_texture_name(step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return result;
}

bool legacy_texture_key_exists(const std::string& key) {
    struct Context {
        const std::string& key;
        std::exception_ptr failure;
    } context{key, {}};
    const auto step = +[](void* opaque, std::uint32_t,
                          std::size_t index) noexcept -> std::uint32_t {
        auto& state = *static_cast<Context*>(opaque);
        try {
            switch (index) {
                case 0:
                    return g_rgb_files.find(state.key) != g_rgb_files.end();
                case 1:
                    return g_index8_files.find(state.key) !=
                        g_index8_files.end();
                case 2:
                    return g_compressed_files.find(state.key) !=
                        g_compressed_files.end();
                default: return 0;
            }
        } catch (...) {
            state.failure = std::current_exception();
            return UINT32_MAX;
        }
    };
    const bool result = a2fo_rs_stage9_rgb_key_exists(step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
    return result;
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
    struct Context {
        std::vector<std::string>::iterator file;
        std::vector<std::string>::iterator end;
        std::exception_ptr failure;
    } context{};
    const auto step = +[](void* opaque, std::uint32_t operation,
                          std::size_t) noexcept -> std::uint32_t {
        auto& state = *static_cast<Context*>(opaque);
        try {
            switch (operation) {
                case 0: return g_conversion_lock_ready;
                case 1: EnterCriticalSection(&g_conversion_lock); return 0;
                case 2:
                    state.file = g_generated_texture_files.begin();
                    state.end = g_generated_texture_files.end();
                    return 0;
                case 3: return state.file != state.end;
                case 4: DeleteFileA(state.file->c_str()); return 0;
                case 5: ++state.file; return 0;
                case 6: g_generated_texture_files.clear(); return 0;
                case 7: g_prepared_texture_files.clear(); return 0;
                case 8: g_compatible_texture_files.clear(); return 0;
                case 9: g_unusable_texture_files.clear(); return 0;
                case 10: return !g_conversion_cache_directory.empty();
                case 11:
                    RemoveDirectoryA(g_conversion_cache_directory.c_str());
                    g_conversion_cache_directory.clear();
                    return 0;
                case 12: LeaveCriticalSection(&g_conversion_lock); return 0;
                case 13: DeleteCriticalSection(&g_conversion_lock); return 0;
                case 14: g_conversion_lock_ready = false; return 0;
                case 15: g_conversion_cache_directory.clear(); return 0;
                default: return 0;
            }
        } catch (...) {
            state.failure = std::current_exception();
            return UINT32_MAX;
        }
    };
    a2fo_rs_stage9_rgb_clear_conversion(step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
}

void clear_state() {
    struct Context { std::exception_ptr failure; } context{};
    const auto step = +[](void* opaque, std::uint32_t operation,
                          std::size_t) noexcept -> std::uint32_t {
        auto& state = *static_cast<Context*>(opaque);
        try {
            switch (operation) {
                case 0: restore_fopen_bridge(); return 0;
                case 1: clear_conversion_cache(); return 0;
                case 2: g_rgb_files.clear(); return 0;
                case 3: g_index8_files.clear(); return 0;
                case 4: g_compressed_files.clear(); return 0;
                case 5: g_root_texture_files.clear(); return 0;
                case 6: g_flattened_tga_candidates.clear(); return 0;
                case 7: g_root_directory.clear(); return 0;
                case 8: g_fopen_slot = nullptr; return 0;
                case 9: g_original_fopen = nullptr; return 0;
                case 10: g_armada = nullptr; return 0;
                case 11: InterlockedExchange(&g_redirect_log_count, 0); return 0;
                case 12: InterlockedExchange(&g_missing_log_count, 0); return 0;
                case 13: InterlockedExchange(&g_recovery_log_count, 0); return 0;
                case 14: InterlockedExchange(&g_blend_guard_log_count, 0); return 0;
                case 15: InterlockedExchange(&g_root_redirect_log_count, 0); return 0;
                case 16: InterlockedExchange(&g_scan_grid_guard_log_count, 0); return 0;
                case 17: InterlockedExchange(&g_conversion_log_count, 0); return 0;
                case 18: InterlockedExchange(&g_conversion_file_counter, 0); return 0;
                case 19: InterlockedExchange(&g_manual_mip_redirect_log_count, 0); return 0;
                case 20: InterlockedExchange(&g_cross_root_mip_synthesis_log_count, 0); return 0;
                case 21: g_api = nullptr; return 0;
                default: return 0;
            }
        } catch (...) {
            state.failure = std::current_exception();
            return UINT32_MAX;
        }
    };
    a2fo_rs_stage9_rgb_clear_state(step, &context);
    if (context.failure) std::rethrow_exception(context.failure);
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
