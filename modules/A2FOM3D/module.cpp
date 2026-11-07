/*
 * Star Trek: Legacy M3D runtime compatibility for Armada II / Fleet Ops.
 *
 * M3D is kept as the authored asset.  We detour Armada's normal SOD geometry
 * resolver and, when the winning extension-root asset is an M3D, parse it on
 * demand.  The Legacy data is translated only in memory into the closest
 * Storm3D 1.93 stream and passed directly to Armada's own Storm3D parser using
 * its native memory-backed input-stream layout.  No .sod cache is written.
 *
 * This is intentionally the first native-loading stage: geometry, hierarchy,
 * diffuse materials and SOD-compatible animation data enter the stock
 * runtime model database immediately.  Legacy-only specular/glow/detail
 * channels remain available to be mapped into A2FO's renderer in the next
 * stage instead of being baked into persistent SOD files.
 */

#include "../../sdk/include/a2fo_module_api.h"
#ifdef A2FO_BUILTIN_FEATURE
#include "../../core/builtin_modules.hpp"
#endif
#include "m3d_conversion.hpp"
#include "../../rust/stage8_a_m3d_ffi.h"
#include "../../rust/stage10_m3d_completion_ffi.h"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

extern "C" void* a2fo_m3d_call_load_sod(
    void* function, void* self, const char* name, float scale);
extern "C" void* a2fo_m3d_parse_sod_stream(
    void* function, void* stream, float scale, const char* database_name);
extern "C" std::uintptr_t a2fo_m3d_call_thiscall_0(void* function, void* self);
extern "C" std::uintptr_t a2fo_m3d_call_thiscall_1(
    void* function, void* self, std::uintptr_t argument1);
extern "C" std::uintptr_t a2fo_m3d_call_thiscall_2(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2);
extern "C" std::uintptr_t a2fo_m3d_call_thiscall_4(
    void* function, void* self, std::uintptr_t argument1,
    std::uintptr_t argument2, std::uintptr_t argument3,
    std::uintptr_t argument4);

namespace {

constexpr char kModuleName[] = "A2FOM3D";
constexpr char kOldMarkerSuffix[] = ".a2fo-m3d";

// ArmadaL.exe / supported Fleet Ops build.
constexpr std::uintptr_t kParseSodStreamRva = 0x0023fa60;
constexpr std::uintptr_t kInputStreamVtableRva = 0x002bc874;
// Storm3D database lifetime helpers.  The parser allocates/registers a
// database before reading nodes, but on a node-parse failure it returns null
// without destroying that partial object.  Leaving it registered poisons
// later model/database lookups and can stall match loading.
constexpr std::uintptr_t kStormDatabaseDestructorRva = 0x00220690;
constexpr std::uintptr_t kEngineOperatorDeleteRva = 0x002527d0;

// Live Storm3D model/node layout already used by A2FONebulaRenderer.  These
// diagnostics let the M3D loader verify that a successful binary parse also
// produced geometry reachable from model+0x3c.
constexpr std::uintptr_t kMeshUpdateRva = 0x00231c10;
constexpr std::uintptr_t kMeshGetTextureRva = 0x00231380;
constexpr std::uintptr_t kMeshSetTextureRva = 0x002313b0;
constexpr std::uintptr_t kTextureFindRva = 0x00242870;
constexpr std::uintptr_t kTextureLockSurfaceRva = 0x00242780;
constexpr std::uintptr_t kTextureUnlockSurfaceRva = 0x002437b0;
constexpr std::uintptr_t kGraphicsEnginePointerRva = 0x003ad508;
constexpr std::size_t kTextureDatabaseOffset = 0x0004;
constexpr std::size_t kTextureNameOffset = 0x0008;
constexpr std::size_t kTextureFlagsOffset = 0x0018;
constexpr std::uint8_t kExpectedMeshGetTexture[]{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0x8b};
constexpr std::uint8_t kExpectedMeshUpdate[]{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x0c, 0x53};
constexpr std::uint8_t kExpectedMeshSetTexture[]{
    0x55, 0x8b, 0xec, 0x83, 0xec, 0x10, 0x53};
constexpr std::uint8_t kExpectedTextureFind[]{
    0x55, 0x8b, 0xec, 0x64, 0xa1, 0x00};
constexpr std::uint8_t kExpectedTextureLockSurface[]{
    0x55, 0x8b, 0xec, 0x56, 0x57};
constexpr std::uint8_t kExpectedTextureUnlockSurface[]{
    0x55, 0x8b, 0xec, 0x56, 0x57};
constexpr std::size_t kGeometryRootNodeOffset = 0x003c;
constexpr std::size_t kNodeNameOffset = 0x0008;
// ArmadaL SHA-256 63bd645d...: constructor 0x6386d3..0x6386dc
// writes node+0x18=parent, node+0x20=parent->firstChild, parent+0x1c=node.
// Recursive collection at 0x639db9/0x639dd9 reads child then next sibling.
constexpr std::size_t kNodeChildOffset = 0x001c;
constexpr std::size_t kNodeSiblingOffset = 0x0020;
constexpr std::size_t kMeshRendererOffset = 0x0128;
constexpr std::size_t kMeshRenderFlagsOffset = 0x012c;
constexpr std::size_t kMeshLocalBoundsMinimumOffset = 0x00e4;
constexpr std::size_t kMeshLocalBoundsMaximumOffset = 0x00f0;
constexpr std::size_t kMaximumNodeCount = 4096;

// FleetOpsHook.dll replaces Armada's public loader (Armada RVA 0x22cf10)
// before A2FO modules are initialized.  Hook the Fleet Ops replacement
// rather than fighting the already-installed detour.  This preserves FO's
// virtual filesystem, database cache and fallback behavior for all non-M3D
// requests while allowing M3D winners to be handled first.
constexpr std::uintptr_t kFoLoadSodHandlerRva = 0x001d49a4;
constexpr std::uintptr_t kFoLoadSodNoFallbackHandlerRva = 0x001d48f4;
constexpr std::uint8_t kExpectedFoLoadSodHandler[]{
    0x55, 0x8b, 0xec, 0x6a, 0x00, 0x6a, 0x00, 0x6a, 0x00};
constexpr std::uint8_t kExpectedFoLoadSodNoFallbackHandler[]{
    0x55, 0x8b, 0xec, 0x6a, 0x00, 0x6a, 0x00, 0x6a, 0x00};

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
A2FO_InlineHook g_load_hook{};
A2FO_InlineHook g_load_no_fallback_hook{};
bool g_ready = false;

bool runtime_enabled() noexcept {
#ifdef A2FO_BUILTIN_FEATURE
    return a2fo::builtin_module_enabled(A2FO_BUILTIN_FEATURE);
#else
    return g_ready;
#endif
}
using RegisterM3DMeshFunction = void (__cdecl*)(void*);
using ClearM3DMeshesFunction = void (__cdecl*)();
RegisterM3DMeshFunction g_register_m3d_mesh = nullptr;
ClearM3DMeshesFunction g_clear_m3d_meshes = nullptr;

struct M3DSource {
    std::string path;
    std::uint32_t root_index = 0;
};

// Only winning M3D assets are kept here.  A SOD in the same or a later
// extension root erases an earlier M3D winner.  A later M3D therefore correctly
// overrides a parent-mod SOD.
std::unordered_map<std::string, M3DSource> g_sources;
std::unordered_map<std::string, void*> g_loaded;
std::unordered_set<std::string> g_loading;
std::unordered_set<std::string> g_failed_runtime;

// Layout used by Armada's native file input object.  When +0x14 is non-null,
// the stock virtual read method copies from +0x1c and advances it, so no FILE*
// is required.  ST3D's parser stores the decoded format version at +0x08.
struct NativeMemoryInput {
    void* vtable = nullptr;              // +0x00
    std::uint32_t database_04 = 0;       // +0x04 parser-created Storm3D database
    float format_version = 0.0f;         // +0x08
    const char* filename = nullptr;      // +0x0c
    void* file = nullptr;                // +0x10
    std::uint8_t* begin = nullptr;       // +0x14
    std::uint8_t* end = nullptr;         // +0x18
    std::uint8_t* cursor = nullptr;      // +0x1c
};
static_assert(sizeof(NativeMemoryInput) == 0x20,
              "Storm3D memory input layout must remain 32-bit");

// Keep Armada's native input-object ABI for every virtual method except the
// raw memory read.  The stock RVA 0x240260 blindly memcpy()s without checking
// +0x18, so one still-unknown format field can turn a parser rejection into an
// access violation.  The guarded reader clamps at end-of-stream, zero-fills
// the requested tail and records the first overrun.  We then reject/clean the
// partial database even if Armada happens to return a non-null object.
struct GuardedMemoryInput {
    NativeMemoryInput native{};
    bool overflow = false;
    std::uint32_t overflow_offset = 0;
    std::uint32_t overflow_request = 0;
};

void* g_guarded_stream_vtable[8]{};

#pragma GCC diagnostic push
// This is the established i386 vtable ABI. Keep it unchanged when the
// shared fixture build enables -Werror for the original module source.
#pragma GCC diagnostic ignored "-Wattributes"
void* __attribute__((thiscall)) guarded_raw_read(
    void* self_pointer, void* destination, std::uint32_t count) noexcept {
    auto* guarded = static_cast<GuardedMemoryInput*>(self_pointer);
    NativeMemoryInput& stream = guarded->native;
    const auto begin = reinterpret_cast<std::uintptr_t>(stream.begin);
    const auto end = reinterpret_cast<std::uintptr_t>(stream.end);
    const auto cursor = reinterpret_cast<std::uintptr_t>(stream.cursor);

    std::size_t remaining = 0;
    if (begin <= cursor && cursor <= end) {
        remaining = static_cast<std::size_t>(end - cursor);
    }

    if (destination && static_cast<std::size_t>(count) <= remaining) {
        if (count) std::memcpy(destination, stream.cursor, count);
        stream.cursor += count;
        return self_pointer;
    }

    if (!guarded->overflow) {
        guarded->overflow = true;
        guarded->overflow_offset =
            begin <= cursor && cursor >= begin
                ? static_cast<std::uint32_t>(
                      std::min<std::uintptr_t>(cursor - begin, 0xffffffffu))
                : 0xffffffffu;
        guarded->overflow_request = count;
    }

    // Preserve native call semantics as far as possible after an overrun:
    // return deterministic zero data rather than touching unmapped memory.
    // Counts in this format are at most 16-bit, so this remains bounded.
    if (destination && count) {
        const std::size_t copy = std::min<std::size_t>(remaining, count);
        if (copy) std::memcpy(destination, stream.cursor, copy);
        std::memset(static_cast<std::uint8_t*>(destination) + copy, 0,
                    static_cast<std::size_t>(count) - copy);
    }
    stream.cursor = stream.end;
    return self_pointer;
}
#pragma GCC diagnostic pop

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

void* at(std::uintptr_t rva) noexcept {
    return reinterpret_cast<void*>(
        reinterpret_cast<std::uintptr_t>(g_armada) + rva);
}

bool readable_range(const void* pointer, std::size_t size) noexcept {
    if (!pointer || !size) return false;
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(pointer, &region, sizeof(region)) ||
        region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0 ||
        (region.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                           PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                           PAGE_EXECUTE_WRITECOPY)) == 0) {
        return false;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto base = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    return address >= base && address - base <= region.RegionSize &&
           size <= region.RegionSize - (address - base);
}

template <std::size_t Size>
bool signature_matches(std::uintptr_t rva,
                       const std::uint8_t (&expected)[Size]) noexcept {
    return readable_range(at(rva), Size) &&
           std::memcmp(at(rva), expected, Size) == 0;
}

struct M3DJoinContext {
    const std::string* left;
    const std::string* right;
    std::string output;
    std::exception_ptr exception{};
};

std::uintptr_t m3d_join_step(void* opaque, std::uint32_t op,
                             std::uintptr_t) noexcept {
    auto& context = *static_cast<M3DJoinContext*>(opaque);
    try {
        switch (op) {
        case 0: return context.left->size();
        case 1: return context.right->size();
        case 2: return static_cast<unsigned char>(context.left->back());
        case 3: context.output = *context.right; return 1;
        case 4: context.output = *context.left; return 1;
        case 5: context.output = *context.left + *context.right; return 1;
        case 6:
            context.output = *context.left + "\\" + *context.right;
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

std::string join_path(const std::string& left, const std::string& right) {
    M3DJoinContext context{&left, &right, {}, {}};
    a2fo_rs_stage8_m3d_join(&m3d_join_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
    return std::move(context.output);
}

bool exists_file(const std::string& path) noexcept {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool exists_directory(const std::string& path) noexcept {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

struct M3DNameView {
    const std::uint8_t* data;
    std::size_t size;
};

struct M3DNameContext {
    std::string input;
    std::string output;
    std::exception_ptr exception{};
};

std::uintptr_t m3d_name_step(void* opaque, std::uint32_t op,
                             std::uintptr_t value) noexcept {
    auto& context = *static_cast<M3DNameContext*>(opaque);
    try {
        switch (op) {
        case 0: return context.input.size();
        case 1:
            return static_cast<unsigned char>(context.input[value]);
        case 2:
            return static_cast<unsigned char>(std::tolower(
                static_cast<unsigned char>(value)));
        case 3: {
            const auto& view =
                *reinterpret_cast<const M3DNameView*>(value);
            context.output.assign(
                reinterpret_cast<const char*>(view.data), view.size);
            return 1;
        }
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

std::string m3d_name(std::string input, std::uint32_t kind) {
    M3DNameContext context{std::move(input), {}, {}};
    a2fo_rs_stage8_m3d_name(&m3d_name_step, &context, kind);
    if (context.exception) std::rethrow_exception(context.exception);
    return std::move(context.output);
}

std::string lower_ascii(std::string value) {
    return m3d_name(std::move(value), 0);
}

std::string model_key(std::string name) {
    return m3d_name(std::move(name), 1);
}

std::string basename_without_extension(const char* filename) {
    if (!filename) return {};
    return m3d_name(std::string(filename), 2);
}

std::vector<std::string> enumerate_files(const std::string& directory,
                                         const char* pattern) {
    std::vector<std::string> files;
    if (!exists_directory(directory)) return files;
    WIN32_FIND_DATAA data{};
    HANDLE find = FindFirstFileA(join_path(directory, pattern).c_str(), &data);
    if (find == INVALID_HANDLE_VALUE) return files;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            files.emplace_back(data.cFileName);
        }
    } while (FindNextFileA(find, &data));
    FindClose(find);
    return files;
}

#ifndef A2FO_BUILTIN_FEATURE
struct M3DCacheContext {
    const std::string* directory;
    std::vector<std::string> markers;
    std::string marker;
    std::string sod;
    std::exception_ptr exception{};
};

std::uintptr_t m3d_cache_step(void* opaque, std::uint32_t op,
                              std::uintptr_t index) noexcept {
    auto& context = *static_cast<M3DCacheContext*>(opaque);
    try {
        switch (op) {
        case 0:
            context.markers = enumerate_files(
                *context.directory, "*.sod.a2fo-m3d");
            return 1;
        case 1: return context.markers.size();
        case 2: return context.markers[index].size();
        case 3: return std::strlen(kOldMarkerSuffix);
        case 4:
            context.marker = join_path(
                *context.directory, context.markers[index]);
            return 1;
        case 5: return exists_file(context.sod);
        case 6: return DeleteFileA(context.sod.c_str()) != 0;
        case 7: return DeleteFileA(context.marker.c_str()) != 0;
        case 8: return !exists_file(context.marker);
        case 9: {
            const auto& name = context.markers[index];
            context.sod = join_path(
                *context.directory,
                name.substr(0, name.size() - std::strlen(kOldMarkerSuffix)));
            return 1;
        }
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

unsigned remove_old_disk_caches(const std::string& directory) {
    M3DCacheContext context{&directory, {}, {}, {}, {}};
    const unsigned removed = a2fo_rs_stage8_m3d_remove_caches(
        &m3d_cache_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
    return removed;
}
#endif

struct M3DIndexTotals {
    std::uint32_t m3d;
    std::uint32_t sod;
    std::uint32_t removed;
};

struct M3DIndexContext {
    std::string root;
    std::string directory;
    std::string key;
    std::vector<std::string> sod_files;
    std::vector<std::string> m3d_files;
    std::unordered_set<std::string> native_sods;
    std::unordered_set<std::string>::iterator current{};
    std::uint32_t root_index = 0;
    std::exception_ptr exception{};
};

std::uintptr_t m3d_index_step(void* opaque, std::uint32_t op,
                              std::uintptr_t value) noexcept {
    auto& context = *static_cast<M3DIndexContext*>(opaque);
    try {
        switch (op) {
        case 0:
            g_sources.clear();
            return g_api && g_api->extension_root_count &&
                g_api->extension_root;
        case 1: return g_api->extension_root_count();
        case 2: {
            const char* root = g_api->extension_root(
                static_cast<std::uint32_t>(value));
            if (!root || !*root) return 0;
            context.root = root;
            context.root_index = static_cast<std::uint32_t>(value);
            context.directory = join_path(context.root, "sod");
            return 1;
        }
        case 3:
#ifndef A2FO_BUILTIN_FEATURE
            return remove_old_disk_caches(context.directory);
#else
            return 0;
#endif
        case 4: return exists_directory(context.directory);
        case 5:
            context.native_sods.clear();
            context.sod_files = enumerate_files(
                context.directory, "*.sod");
            return 1;
        case 6: return context.sod_files.size();
        case 7:
#ifdef A2FO_BUILTIN_FEATURE
            return exists_file(join_path(
                context.directory, context.sod_files[value]) +
                kOldMarkerSuffix);
#else
            return 0;
#endif
        case 8:
            context.native_sods.insert(model_key(
                context.sod_files[value]));
            return 1;
        case 9:
            context.m3d_files = enumerate_files(
                context.directory, "*.m3d");
            return 1;
        case 10: return context.m3d_files.size();
        case 11:
            context.key = model_key(context.m3d_files[value]);
            return 1;
        case 12:
            return !context.key.empty() &&
                context.native_sods.find(context.key) ==
                    context.native_sods.end();
        case 13:
            g_sources[context.key] = M3DSource{
                join_path(context.directory,
                          context.m3d_files[value]), context.root_index};
            return 1;
        case 14:
            context.current = context.native_sods.begin();
            return 1;
        case 15:
            return context.current != context.native_sods.end();
        case 16:
            g_sources.erase(*context.current);
            ++context.current;
            return 1;
        case 17: {
            const auto& totals =
                *reinterpret_cast<const M3DIndexTotals*>(value);
            char message[512]{};
            std::snprintf(message, sizeof(message),
                          "Indexed %u M3D file(s) and %u native SOD file(s); %zu M3D basename winner(s); removed %u obsolete generated SOD cache(s)",
                          totals.m3d, totals.sod, g_sources.size(),
                          totals.removed);
            log_line(message);
            return 1;
        }
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

bool build_source_index() {
    M3DIndexContext context{};
    const bool result = a2fo_rs_stage8_m3d_index(
        &m3d_index_step, &context) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return result;
}

struct M3DFileLength {
    std::int64_t value = 0;
};

struct M3DReadContext {
    std::ifstream input;
    std::vector<std::uint8_t>* bytes;
    std::string* error;
    M3DFileLength length{};
    std::exception_ptr exception{};

    M3DReadContext(const std::string& path,
                   std::vector<std::uint8_t>* destination,
                   std::string* failure)
        : input(path, std::ios::binary), bytes(destination), error(failure) {}
};

std::uintptr_t m3d_read_step(void* opaque, std::uint32_t op,
                             std::uintptr_t value) noexcept {
    auto& context = *static_cast<M3DReadContext*>(opaque);
    try {
        switch (op) {
        case 0: return static_cast<bool>(context.input);
        case 1:
            *context.error = "could not open M3D source";
            return 1;
        case 2:
            context.input.seekg(0, std::ios::end);
            context.length.value =
                static_cast<std::int64_t>(context.input.tellg());
            return 1;
        case 3:
            return reinterpret_cast<std::uintptr_t>(&context.length);
        case 4:
            *context.error = "invalid or unreasonably large M3D source";
            return 1;
        case 5:
            context.input.seekg(0, std::ios::beg);
            return 1;
        case 6:
            context.bytes->resize(value);
            return 1;
        case 7:
            return static_cast<bool>(context.input.read(
                reinterpret_cast<char*>(context.bytes->data()),
                static_cast<std::streamsize>(value)));
        case 8:
            *context.error = "could not read complete M3D source";
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

bool read_file(const std::string& path, std::vector<std::uint8_t>& bytes,
               std::string& error) {
    M3DReadContext context(path, &bytes, &error);
    const bool result = a2fo_rs_stage8_m3d_read_file(
        &m3d_read_step, &context) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return result;
}

std::string runtime_cache_key(const std::string& key, float scale) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(scale), "float size mismatch");
    std::memcpy(&bits, &scale, sizeof(bits));
    char suffix[16]{};
    std::snprintf(suffix, sizeof(suffix), "#%08lx",
                  static_cast<unsigned long>(bits));
    return key + suffix;
}

struct M3DReportContext {
    const std::string* source;
    const a2fo::m3d::ConversionStats* stats;
    std::exception_ptr exception{};
};

std::uintptr_t m3d_report_step(void* opaque, std::uint32_t op,
                               std::uintptr_t index) noexcept {
    auto& context = *static_cast<M3DReportContext*>(opaque);
    const auto& source = *context.source;
    const auto& stats = *context.stats;
    try {
        switch (op) {
        case 0: {
            char message[640]{};
            std::snprintf(message, sizeof(message),
                          "Runtime M3D parse '%s': v%.3f, %zu mesh(es), %zu group(s), %zu triangle(s), %zu Storm3D node(s), %zu source root(s), %zu node position(s) reordered%s",
                          source.c_str(), stats.source_version, stats.meshes,
                          stats.groups, stats.triangles, stats.output_nodes,
                          stats.source_roots, stats.reordered_nodes,
                          stats.synthesized_root
                            ? " (synthetic root inserted)" : "");
            log_line(message);
            return 1;
        }
        case 1: return stats.warnings.size();
        case 2:
            log_line("M3D compatibility note: " + stats.warnings[index]);
            return 1;
        case 3:
            log_line("M3D compatibility note: " +
                     std::to_string(index) +
                     " additional note(s) suppressed");
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

void report_conversion(const std::string& source,
                       const a2fo::m3d::ConversionStats& stats) {
    M3DReportContext context{&source, &stats, {}};
    a2fo_rs_stage8_m3d_report(&m3d_report_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
}

const char* parser_stage(std::size_t consumed,
                         const a2fo::m3d::ConversionStats& stats) {
    static constexpr const char* kStages[] = {
        "header/version", "materials", "nodes/hierarchy/mesh",
        "animations", "texture-animation references",
        "end/trailing data"};
    const std::uint32_t selected = a2fo_rs_stage8_m3d_parser_stage(
        consumed, stats.sod_materials_end, stats.sod_nodes_end,
        stats.sod_animations_end, stats.sod_references_end);
    return kStages[selected];
}

struct RuntimeTreeStats {
    std::size_t nodes = 0;
    std::size_t meshes = 0;
    std::size_t mesh_renderers = 0;
    std::size_t rebuilt_renderers = 0;
    std::size_t missing_renderers = 0;
    bool truncated = false;
};

std::string bounded_node_name(void* node) noexcept {
    if (!node || !readable_range(static_cast<std::uint8_t*>(node) +
                                 kNodeNameOffset, sizeof(const char*))) {
        return {};
    }
    const char* name = *reinterpret_cast<const char**>(
        static_cast<std::uint8_t*>(node) + kNodeNameOffset);
    if (!name) return {};
    try {
        std::string result;
        result.reserve(64);
        struct Context {
            const char* name;
            std::string* result;
            std::exception_ptr error;
        } context{name, &result, {}};
        const auto step = [](void* raw, std::size_t index,
                             std::uint32_t phase,
                             std::uint8_t* byte) -> std::uint8_t {
            auto& context = *static_cast<Context*>(raw);
            try {
                if (phase == 0) {
                    if (!readable_range(context.name + index, 1)) return 0;
                    *byte = static_cast<std::uint8_t>(context.name[index]);
                } else if (phase == 1) {
                    context.result->push_back(context.name[index]);
                } else {
                    return 0;
                }
                return 1;
            } catch (...) {
                context.error = std::current_exception();
                return 0;
            }
        };
        const bool terminated = a2fo_rs_stage10_m3d_bounded_name(step, &context) != 0;
        if (context.error) std::rethrow_exception(context.error);
        if (terminated) return result;
    } catch (...) {}
    return {};
}

std::uintptr_t runtime_node_type(void* node) noexcept {
    if (!node || !readable_range(node, sizeof(void*))) return ~std::uintptr_t{0};
    void* vtable = *reinterpret_cast<void**>(node);
    if (!readable_range(vtable, sizeof(void*) * 2u)) return ~std::uintptr_t{0};
    void* get_type = *(reinterpret_cast<void**>(vtable) + 1u);
    if (!readable_range(get_type, 1)) return ~std::uintptr_t{0};
    return a2fo_m3d_call_thiscall_0(get_type, node);
}

std::string legacy_texture_filename(std::string value) {
    return m3d_name(std::move(value), 3);
}

[[maybe_unused]] std::vector<std::string> texture_lookup_candidates(const std::string& authored) {
    std::vector<std::string> result;
    std::string base = legacy_texture_filename(authored);
    struct Context {
        const std::string* base;
        std::vector<std::string>* result;
        std::exception_ptr error;
    } context{&base, &result, {}};
    const auto step = [](void* raw, std::uint32_t op,
                         std::uint8_t* value) -> std::uint8_t {
        auto& context = *static_cast<Context*>(raw);
        try {
            switch (op) {
            case 0: *value = context.base->empty(); break;
            case 1: context.result->push_back(*context.base); break;
            case 2: *value = context.base->find_last_of('.') == std::string::npos; break;
            case 3: context.result->push_back(*context.base + ".dds"); break;
            case 4: context.result->push_back(*context.base + ".tga"); break;
            default: return 0;
            }
            return 1;
        } catch (...) {
            context.error = std::current_exception();
            return 0;
        }
    };
    a2fo_rs_stage10_m3d_texture_candidates(step, &context);
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

using TextureFindFunction = void* (__cdecl*)(
    void* database, const char* texture_name, std::uint32_t flags);

int current_texture_device() noexcept {
    if (!g_armada) return -1;
    auto** renderer_slot = reinterpret_cast<void**>(at(kGraphicsEnginePointerRva));
    if (!readable_range(renderer_slot, sizeof(*renderer_slot)) || !*renderer_slot)
        return -1;
    const auto* device = static_cast<const std::uint8_t*>(*renderer_slot) + 0xc0;
    if (!readable_range(device, sizeof(std::int32_t))) return -1;
    const std::int32_t value = *reinterpret_cast<const std::int32_t*>(device);
    return value >= 0 && value < 32 ? value : -1;
}

struct M3DMaterializeContext {
    void* texture;
    const std::string* logical_name;
    int device = -1;
    std::uint32_t pitch = 0;
};

std::uintptr_t m3d_materialize_step(void* opaque, std::uint32_t op,
                                    std::uintptr_t) noexcept {
    auto& context = *static_cast<M3DMaterializeContext*>(opaque);
    switch (op) {
    case 0: return context.texture != nullptr;
    case 1: return signature_matches(
        kTextureLockSurfaceRva, kExpectedTextureLockSurface);
    case 2: return signature_matches(
        kTextureUnlockSurfaceRva, kExpectedTextureUnlockSurface);
    case 3:
        context.device = current_texture_device();
        return static_cast<std::uintptr_t>(context.device);
    case 4:
        return a2fo_m3d_call_thiscall_4(
            at(kTextureLockSurfaceRva), context.texture,
            static_cast<std::uintptr_t>(context.device),
            reinterpret_cast<std::uintptr_t>(&context.pitch), 0u, 0u);
    case 5:
        a2fo_m3d_call_thiscall_1(
            at(kTextureUnlockSurfaceRva), context.texture,
            static_cast<std::uintptr_t>(context.device));
        return 1;
    case 6: {
        char line[384]{};
        std::snprintf(line, sizeof(line),
            "Materialized Legacy texture '%s' on device %d (pitch=%u)",
            context.logical_name->c_str(), context.device,
            static_cast<unsigned>(context.pitch));
        log_line(line);
        return 1;
    }
    case 7:
        log_line("Legacy texture '" + *context.logical_name +
                 "' remained pixel-unavailable after LockSurface");
        return 1;
    case 8: return context.pitch != 0;
    default: return 0;
    }
}

bool materialize_texture_pixels(void* texture,
                                const std::string& logical_name) noexcept {
    M3DMaterializeContext context{texture, &logical_name};
    return a2fo_rs_stage8_m3d_materialize(
        &m3d_materialize_step, &context) != 0;
}

struct M3DFindTextureContext {
    void* reference_texture;
    const std::string* authored;
    std::uint32_t extra_flags;
    std::string* selected;
    M3DFindTextureContext(void* reference, const std::string* name,
                          std::uint32_t flags, std::string* output)
        : reference_texture(reference), authored(name),
          extra_flags(flags), selected(output) {}
    void* database = nullptr;
    std::uint32_t flags = 0;
    std::string logical;
    void* texture = nullptr;
};

std::uintptr_t m3d_find_texture_step(void* opaque, std::uint32_t op,
                                     std::uintptr_t) noexcept {
    auto& context = *static_cast<M3DFindTextureContext*>(opaque);
    switch (op) {
    case 0: return context.reference_texture != nullptr;
    case 1:
        context.flags = context.extra_flags;
        if (readable_range(
                static_cast<std::uint8_t*>(context.reference_texture) +
                    kTextureDatabaseOffset, sizeof(void*))) {
            context.database = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(context.reference_texture) +
                kTextureDatabaseOffset);
        }
        if (readable_range(
                static_cast<std::uint8_t*>(context.reference_texture) +
                    kTextureFlagsOffset, sizeof(std::uint32_t))) {
            context.flags |= *reinterpret_cast<std::uint32_t*>(
                static_cast<std::uint8_t*>(context.reference_texture) +
                kTextureFlagsOffset);
        }
        return context.database != nullptr;
    case 2:
        context.logical = m3d_name(*context.authored, 4);
        return 1;
    case 3: return !context.logical.empty();
    case 4: {
        const auto find = reinterpret_cast<TextureFindFunction>(
            at(kTextureFindRva));
        context.texture = find(
            context.database, context.logical.c_str(), context.flags);
        return reinterpret_cast<std::uintptr_t>(context.texture);
    }
    case 5:
        if (context.selected) *context.selected = context.logical;
        return 1;
    default: return 0;
    }
}

void* find_runtime_texture(void* reference_texture,
                           const std::string& authored,
                           std::uint32_t extra_flags,
                           std::string* selected) noexcept {
    M3DFindTextureContext context{
        reference_texture, &authored, extra_flags, selected};
    return reinterpret_cast<void*>(a2fo_rs_stage8_m3d_find_texture(
        &m3d_find_texture_step, &context));
}

struct M3DBindContext {
    void* geometry;
    const a2fo::m3d::ConversionStats* conversion;
    const std::string* database_name;
    M3DBindContext(void* model, const a2fo::m3d::ConversionStats* stats,
                   const std::string* name)
        : geometry(model), conversion(stats), database_name(name) {}
    std::vector<void*> pending;
    std::unordered_set<void*> visited;
    std::unordered_map<std::string, void*> meshes;
    void* node = nullptr;
    void* mesh = nullptr;
    const a2fo::m3d::RuntimeTextureBinding* binding = nullptr;
    void* native_diffuse = nullptr;
    void* diffuse = nullptr;
    std::string selected;
    std::size_t bound = 0;
    std::size_t misses = 0;
    std::uint32_t flags_before = 0;
    std::uint32_t flags_after = 0;
};

std::uintptr_t m3d_bind_step(void* opaque, std::uint32_t op,
                             std::uintptr_t index) noexcept {
    auto& context = *static_cast<M3DBindContext*>(opaque);
    switch (op) {
    case 0:
        return context.geometry &&
            !context.conversion->texture_bindings.empty();
    case 1: return signature_matches(
        kTextureFindRva, kExpectedTextureFind);
    case 2: return signature_matches(
        kMeshGetTextureRva, kExpectedMeshGetTexture);
    case 3: return signature_matches(
        kMeshSetTextureRva, kExpectedMeshSetTexture);
    case 4: return signature_matches(
        kMeshUpdateRva, kExpectedMeshUpdate);
    case 5:
        log_line("Runtime M3D texture binding unavailable: Storm3D texture/mesh signatures do not match");
        return 1;
    case 6: {
        void* root = nullptr;
        if (readable_range(
                static_cast<std::uint8_t*>(context.geometry) +
                    kGeometryRootNodeOffset, sizeof(void*))) {
            root = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(context.geometry) +
                kGeometryRootNodeOffset);
        }
        if (root) context.pending.push_back(root);
        return root != nullptr;
    }
    case 7: return context.pending.size();
    case 8: return context.visited.size();
    case 9:
        context.node = context.pending.back();
        context.pending.pop_back();
        return 1;
    case 10:
        return context.node &&
            context.visited.insert(context.node).second;
    case 11:
        if (readable_range(
                static_cast<std::uint8_t*>(context.node) +
                    kNodeSiblingOffset, sizeof(void*))) {
            if (void* sibling = *reinterpret_cast<void**>(
                    static_cast<std::uint8_t*>(context.node) +
                    kNodeSiblingOffset)) {
                context.pending.push_back(sibling);
            }
        }
        return 1;
    case 12:
        if (readable_range(
                static_cast<std::uint8_t*>(context.node) +
                    kNodeChildOffset, sizeof(void*))) {
            if (void* child = *reinterpret_cast<void**>(
                    static_cast<std::uint8_t*>(context.node) +
                    kNodeChildOffset)) {
                context.pending.push_back(child);
            }
        }
        return 1;
    case 13: return runtime_node_type(context.node) == 1u;
    case 14: {
        const std::string name = bounded_node_name(context.node);
        if (!name.empty()) context.meshes[lower_ascii(name)] = context.node;
        return 1;
    }
    case 15: return context.conversion->texture_bindings.size();
    case 16: {
        context.binding = &context.conversion->texture_bindings[index];
        const auto found = context.meshes.find(
            lower_ascii(context.binding->mesh_name));
        if (found == context.meshes.end()) return 0;
        context.mesh = found->second;
        return 1;
    }
    case 17:
        return context.binding->has_local_bounds &&
            readable_range(
                static_cast<std::uint8_t*>(context.mesh) +
                    kMeshLocalBoundsMinimumOffset, sizeof(float) * 3u) &&
            readable_range(
                static_cast<std::uint8_t*>(context.mesh) +
                    kMeshLocalBoundsMaximumOffset, sizeof(float) * 3u);
    case 18: {
        std::memcpy(
            static_cast<std::uint8_t*>(context.mesh) +
                kMeshLocalBoundsMinimumOffset,
            context.binding->local_bounds_min.data(), sizeof(float) * 3u);
        std::memcpy(
            static_cast<std::uint8_t*>(context.mesh) +
                kMeshLocalBoundsMaximumOffset,
            context.binding->local_bounds_max.data(), sizeof(float) * 3u);
        char bounds_line[512]{};
        std::snprintf(bounds_line, sizeof(bounds_line),
            "Applied Legacy local bounds to mesh '%s': min=(%.3f, %.3f, %.3f) max=(%.3f, %.3f, %.3f)",
            context.binding->mesh_name.c_str(),
            context.binding->local_bounds_min[0],
            context.binding->local_bounds_min[1],
            context.binding->local_bounds_min[2],
            context.binding->local_bounds_max[0],
            context.binding->local_bounds_max[1],
            context.binding->local_bounds_max[2]);
        log_line(bounds_line);
        return 1;
    }
    case 19:
        context.native_diffuse = reinterpret_cast<void*>(
            a2fo_m3d_call_thiscall_1(
                at(kMeshGetTextureRva), context.mesh, 0u));
        return reinterpret_cast<std::uintptr_t>(context.native_diffuse);
    case 20:
        log_line("Runtime mesh '" + context.binding->mesh_name +
                 "' has no native slot-0 texture context; Legacy texture binding skipped");
        return 1;
    case 21: return !context.binding->diffuse_texture.empty();
    case 22:
        context.selected.clear();
        context.diffuse = find_runtime_texture(
            context.native_diffuse, context.binding->diffuse_texture,
            0u, &context.selected);
        return reinterpret_cast<std::uintptr_t>(context.diffuse);
    case 23:
        materialize_texture_pixels(context.diffuse, context.selected);
        return 1;
    case 24: {
        a2fo_m3d_call_thiscall_2(
            at(kMeshSetTextureRva), context.mesh,
            reinterpret_cast<std::uintptr_t>(context.diffuse), 0u);
        ++context.bound;
        char line[512]{};
        std::snprintf(line, sizeof(line),
            "Bound Legacy diffuse '%s' to runtime mesh '%s' via Storm3D key '%s'",
            context.binding->diffuse_texture.c_str(),
            context.binding->mesh_name.c_str(), context.selected.c_str());
        log_line(line);
        return 1;
    }
    case 25:
        ++context.misses;
        log_line("Could not resolve Legacy diffuse '" +
                 context.binding->diffuse_texture +
                 "' for runtime mesh '" +
                 context.binding->mesh_name + "'");
        return 1;
    case 26: return !context.binding->bump_texture.empty();
    case 27:
        log_line("Legacy bump '" + context.binding->bump_texture +
                 "' retained but runtime bump binding is disabled for mesh '" +
                 context.binding->mesh_name + "'");
        return 1;
    case 28:
        context.flags_before = 0;
        context.flags_after = 0;
        if (readable_range(
                static_cast<std::uint8_t*>(context.mesh) +
                    kMeshRenderFlagsOffset,
                sizeof(context.flags_before))) {
            auto* flags = reinterpret_cast<std::uint32_t*>(
                static_cast<std::uint8_t*>(context.mesh) +
                kMeshRenderFlagsOffset);
            context.flags_before = *flags;
            *flags = context.flags_before & ~0x2u;
            context.flags_after = *flags;
        }
        return 1;
    case 29:
        a2fo_m3d_call_thiscall_2(
            at(kMeshSetTextureRva), context.mesh, 0u, 1u);
        return 1;
    case 30: {
        a2fo_m3d_call_thiscall_1(
            at(kMeshUpdateRva), context.mesh, 0u);
        char line[384]{};
        std::snprintf(line, sizeof(line),
            "M3D bump-state neutralized for mesh '%s': stage1=null flags 0x%08lx -> 0x%08lx",
            context.binding->mesh_name.c_str(),
            static_cast<unsigned long>(context.flags_before),
            static_cast<unsigned long>(context.flags_after));
        log_line(line);
        return 1;
    }
    case 31: {
        char summary[384]{};
        std::snprintf(summary, sizeof(summary),
            "Runtime M3D texture binding for '%s': %zu texture slot(s) bound, %zu diffuse miss(es)",
            context.database_name->c_str(), context.bound, context.misses);
        log_line(summary);
        return 1;
    }
    case 32: return context.bound;
    default: return 0;
    }
}

std::size_t bind_runtime_m3d_textures(
    void* geometry, const a2fo::m3d::ConversionStats& conversion,
    const std::string& database_name) noexcept {
    M3DBindContext context{geometry, &conversion, &database_name};
    return a2fo_rs_stage8_m3d_bind_textures(
        &m3d_bind_step, &context);
}

struct RuntimeHierarchyAuditStats {
    std::size_t discovered = 0;
    std::size_t expected = 0;
    std::size_t mismatched = 0;
    std::size_t missing = 0;
};

bool writable_range(void* pointer, std::size_t size) noexcept {
    if (!pointer || !size) return false;
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(pointer, &region, sizeof(region)) ||
        region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0 ||
        (region.Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
                           PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) == 0) {
        return false;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    const auto base = reinterpret_cast<std::uintptr_t>(region.BaseAddress);
    return address >= base && address - base <= region.RegionSize &&
           size <= region.RegionSize - (address - base);
}

struct M3DAuditPending {
    void* node;
    void* parent;
};

struct M3DAuditContext {
    void* geometry;
    const a2fo::m3d::ConversionStats* conversion;
    const std::string* database_name;
    RuntimeHierarchyAuditStats result{};
    std::vector<M3DAuditPending> pending;
    std::unordered_set<void*> visited;
    std::unordered_map<std::string, void*> by_name;
    std::unordered_map<std::string, std::string> actual_parent;
    std::vector<void*> runtime_nodes;
    std::vector<std::string> mismatch_notes;
    M3DAuditPending current{};
    std::string current_name;
    const a2fo::m3d::RuntimeHierarchyBinding* binding = nullptr;
    std::string actual_name;

    M3DAuditContext(void* model, const a2fo::m3d::ConversionStats* stats,
                    const std::string* name)
        : geometry(model), conversion(stats), database_name(name) {}
};

std::uintptr_t m3d_audit_step(void* opaque, std::uint32_t op,
                              std::uintptr_t index) noexcept {
    auto& context = *static_cast<M3DAuditContext*>(opaque);
    switch (op) {
    case 0:
        context.result.expected =
            context.conversion->hierarchy_bindings.size();
        return context.geometry &&
            !context.conversion->hierarchy_bindings.empty() &&
            readable_range(
                static_cast<std::uint8_t*>(context.geometry) +
                    kGeometryRootNodeOffset, sizeof(void*));
    case 1: {
        void* root = *reinterpret_cast<void**>(
            static_cast<std::uint8_t*>(context.geometry) +
            kGeometryRootNodeOffset);
        if (root) context.pending.push_back({root, nullptr});
        return root != nullptr;
    }
    case 2: return context.pending.size();
    case 3: return context.visited.size();
    case 4:
        context.current = context.pending.back();
        context.pending.pop_back();
        return 1;
    case 5:
        return context.current.node &&
            context.visited.insert(context.current.node).second;
    case 6:
        context.runtime_nodes.push_back(context.current.node);
        return 1;
    case 7:
        context.current_name = bounded_node_name(
            context.current.node);
        return !context.current_name.empty();
    case 8:
        context.by_name[context.current_name] = context.current.node;
        context.actual_parent[context.current_name] =
            context.current.parent
                ? bounded_node_name(context.current.parent)
                : std::string{};
        return 1;
    case 9:
        if (readable_range(
                static_cast<std::uint8_t*>(context.current.node) +
                    kNodeSiblingOffset, sizeof(void*))) {
            void* sibling = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(context.current.node) +
                kNodeSiblingOffset);
            if (sibling) context.pending.push_back(
                {sibling, context.current.parent});
        }
        return 1;
    case 10:
        if (readable_range(
                static_cast<std::uint8_t*>(context.current.node) +
                    kNodeChildOffset, sizeof(void*))) {
            void* child = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(context.current.node) +
                kNodeChildOffset);
            if (child) context.pending.push_back(
                {child, context.current.node});
        }
        return 1;
    case 11:
        context.result.discovered = context.runtime_nodes.size();
        return 1;
    case 12: return context.conversion->hierarchy_bindings.size();
    case 13:
        context.binding =
            &context.conversion->hierarchy_bindings[index];
        return context.by_name.find(context.binding->node_name) !=
            context.by_name.end();
    case 14: ++context.result.missing; return 1;
    case 15: return context.mismatch_notes.size();
    case 16:
        context.mismatch_notes.push_back(
            "missing runtime node '" + context.binding->node_name + "'");
        return 1;
    case 17: {
        const auto actual = context.actual_parent.find(
            context.binding->node_name);
        context.actual_name = actual == context.actual_parent.end()
            ? std::string{} : actual->second;
        return context.actual_name != context.binding->parent_name;
    }
    case 18: ++context.result.mismatched; return 1;
    case 19:
        context.mismatch_notes.push_back(
            "'" + context.binding->node_name + "': expected parent '" +
            (context.binding->parent_name.empty()
                ? std::string("<root>") : context.binding->parent_name) +
            "', runtime parent '" +
            (context.actual_name.empty()
                ? std::string("<root>") : context.actual_name) + "'");
        return 1;
    case 20: {
        char summary[512]{};
        std::snprintf(summary, sizeof(summary),
            "Runtime M3D hierarchy audit for '%s': discovered=%zu expected=%zu mismatched=%zu missing=%zu; native links preserved (child=+0x1c sibling=+0x20)",
            context.database_name->c_str(), context.result.discovered,
            context.result.expected, context.result.mismatched,
            context.result.missing);
        log_line(summary);
        return 1;
    }
    case 21:
        log_line("Hierarchy mismatch: " +
                 context.mismatch_notes[index]);
        return 1;
    default: return 0;
    }
}

RuntimeHierarchyAuditStats audit_runtime_hierarchy(
    void* geometry, const a2fo::m3d::ConversionStats& conversion,
    const std::string& database_name) noexcept {
    M3DAuditContext context{geometry, &conversion, &database_name};
    a2fo_rs_stage8_m3d_hierarchy_audit(&m3d_audit_step, &context);
    return context.result;
}

struct M3DRename {
    char* storage;
    std::string from;
    std::string to;
};

struct M3DRestoreContext {
    void* geometry;
    const a2fo::m3d::ConversionStats* conversion;
    const std::string* database_name;
    std::unordered_map<std::string, std::string> authored;
    std::vector<M3DRename> renames;
    std::vector<void*> pending;
    std::unordered_set<void*> visited;
    void* node = nullptr;
    std::string current_name;
    std::unordered_map<std::string, std::string>::iterator wanted{};
    char* storage = nullptr;

    M3DRestoreContext(void* model, const a2fo::m3d::ConversionStats* stats,
                      const std::string* name)
        : geometry(model), conversion(stats), database_name(name) {}
};

std::uintptr_t m3d_restore_step(void* opaque, std::uint32_t op,
                                std::uintptr_t index) noexcept {
    auto& context = *static_cast<M3DRestoreContext*>(opaque);
    switch (op) {
    case 0:
        return context.geometry &&
            !context.conversion->hierarchy_bindings.empty() &&
            readable_range(
                static_cast<std::uint8_t*>(context.geometry) +
                    kGeometryRootNodeOffset, sizeof(void*));
    case 1: {
        void* root = *reinterpret_cast<void**>(
            static_cast<std::uint8_t*>(context.geometry) +
            kGeometryRootNodeOffset);
        if (root) context.pending.push_back(root);
        return root != nullptr;
    }
    case 2: return context.conversion->hierarchy_bindings.size();
    case 3: {
        const auto& binding =
            context.conversion->hierarchy_bindings[index];
        return !binding.authored_name.empty() &&
            binding.authored_name != binding.node_name;
    }
    case 4: {
        const auto& binding =
            context.conversion->hierarchy_bindings[index];
        context.authored[binding.node_name] = binding.authored_name;
        return 1;
    }
    case 5: return !context.authored.empty();
    case 6: return context.pending.size();
    case 7: return context.visited.size();
    case 8:
        context.node = context.pending.back();
        context.pending.pop_back();
        return 1;
    case 9:
        return context.node &&
            context.visited.insert(context.node).second;
    case 10:
        context.current_name = bounded_node_name(context.node);
        context.wanted = context.authored.find(
            context.current_name);
        return context.wanted != context.authored.end();
    case 11:
        return readable_range(
            static_cast<std::uint8_t*>(context.node) +
                kNodeNameOffset, sizeof(char*));
    case 12:
        context.storage = *reinterpret_cast<char**>(
            static_cast<std::uint8_t*>(context.node) +
            kNodeNameOffset);
        return context.storage != nullptr;
    case 13: return context.wanted->second.size();
    case 14: return context.current_name.size();
    case 15:
        return writable_range(
            context.storage, context.current_name.size() + 1u);
    case 16:
        context.renames.push_back({
            context.storage, context.current_name,
            context.wanted->second});
        return 1;
    case 17:
        if (readable_range(
                static_cast<std::uint8_t*>(context.node) +
                    kNodeSiblingOffset, sizeof(void*))) {
            void* sibling = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(context.node) +
                kNodeSiblingOffset);
            if (sibling) context.pending.push_back(sibling);
        }
        return 1;
    case 18:
        if (readable_range(
                static_cast<std::uint8_t*>(context.node) +
                    kNodeChildOffset, sizeof(void*))) {
            void* child = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(context.node) +
                kNodeChildOffset);
            if (child) context.pending.push_back(child);
        }
        return 1;
    case 19: return context.renames.size();
    case 20: {
        auto& rename = context.renames[index];
        std::memcpy(rename.storage, rename.to.c_str(),
                    rename.to.size() + 1u);
        return 1;
    }
    case 21: {
        char summary[384]{};
        std::snprintf(summary, sizeof(summary),
            "Runtime M3D hierarchy for '%s': restored %zu temporary disambiguated node name(s) to their authored Legacy names after hierarchy audit",
            context.database_name->c_str(), context.renames.size());
        log_line(summary);
        return 1;
    }
    default: return 0;
    }
}

std::size_t restore_runtime_authored_names(
    void* geometry, const a2fo::m3d::ConversionStats& conversion,
    const std::string& database_name) noexcept {
    M3DRestoreContext context{geometry, &conversion, &database_name};
    return a2fo_rs_stage8_m3d_restore_names(
        &m3d_restore_step, &context);
}

struct M3DInspectContext {
    void* geometry;
    const std::string* database_name;
    RuntimeTreeStats stats{};
    void* root = nullptr;
    bool can_rebuild = false;
    std::vector<void*> pending;
    std::unordered_set<void*> visited;
    std::vector<std::string> mesh_notes;
    void* node = nullptr;
    void* renderer = nullptr;
    std::uint32_t flags = 0;

    M3DInspectContext(void* model, const std::string* name)
        : geometry(model), database_name(name) {}
};

std::uintptr_t m3d_inspect_step(void* opaque, std::uint32_t op,
                                std::uintptr_t value) noexcept {
    auto& context = *static_cast<M3DInspectContext*>(opaque);
    switch (op) {
    case 0:
        return context.geometry && readable_range(
            static_cast<std::uint8_t*>(context.geometry) +
                kGeometryRootNodeOffset, sizeof(void*));
    case 1:
        log_line("Runtime Storm3D tree for '" +
                 *context.database_name +
                 "' has no readable model/root slot");
        return 1;
    case 2:
        context.root = *reinterpret_cast<void**>(
            static_cast<std::uint8_t*>(context.geometry) +
            kGeometryRootNodeOffset);
        if (context.root) context.pending.push_back(context.root);
        return context.root != nullptr;
    case 3:
        log_line("Runtime Storm3D tree for '" +
                 *context.database_name + "' has a null root node");
        return 1;
    case 4:
        context.can_rebuild = signature_matches(
            kMeshUpdateRva, kExpectedMeshUpdate);
        return context.can_rebuild;
    case 5: return context.pending.size();
    case 6: return context.visited.size();
    case 7:
        context.node = context.pending.back();
        context.pending.pop_back();
        return 1;
    case 8:
        return context.node &&
            context.visited.insert(context.node).second;
    case 9: ++context.stats.nodes; return 1;
    case 10:
        if (readable_range(
                static_cast<std::uint8_t*>(context.node) +
                    kNodeSiblingOffset, sizeof(void*))) {
            void* sibling = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(context.node) +
                kNodeSiblingOffset);
            if (sibling) context.pending.push_back(sibling);
        }
        return 1;
    case 11:
        if (readable_range(
                static_cast<std::uint8_t*>(context.node) +
                    kNodeChildOffset, sizeof(void*))) {
            void* child = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(context.node) +
                kNodeChildOffset);
            if (child) context.pending.push_back(child);
        }
        return 1;
    case 12: return runtime_node_type(context.node) == 1u;
    case 13: ++context.stats.meshes; return 1;
    case 14:
        if (g_register_m3d_mesh) g_register_m3d_mesh(context.node);
        return 1;
    case 15:
        context.renderer = nullptr;
        context.flags = 0;
        if (readable_range(
                static_cast<std::uint8_t*>(context.node) +
                    kMeshRendererOffset, sizeof(void*))) {
            context.renderer = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(context.node) +
                kMeshRendererOffset);
        }
        if (readable_range(
                static_cast<std::uint8_t*>(context.node) +
                    kMeshRenderFlagsOffset, sizeof(context.flags))) {
            context.flags = *reinterpret_cast<std::uint32_t*>(
                static_cast<std::uint8_t*>(context.node) +
                kMeshRenderFlagsOffset);
        }
        return context.renderer != nullptr;
    case 16:
        a2fo_m3d_call_thiscall_1(
            at(kMeshUpdateRva), context.node, 0);
        return 1;
    case 17:
        if (readable_range(
                static_cast<std::uint8_t*>(context.node) +
                    kMeshRendererOffset, sizeof(void*))) {
            context.renderer = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(context.node) +
                kMeshRendererOffset);
        }
        return context.renderer != nullptr;
    case 18: ++context.stats.rebuilt_renderers; return 1;
    case 19:
        if (value != 0) ++context.stats.mesh_renderers;
        else ++context.stats.missing_renderers;
        return 1;
    case 20: return context.mesh_notes.size();
    case 21: {
        char line[384]{};
        const std::string name = bounded_node_name(context.node);
        std::snprintf(line, sizeof(line),
                      "Runtime mesh '%s': node=%p renderer=%p flags=0x%08lx%s",
                      name.empty() ? "<unnamed>" : name.c_str(),
                      context.node, context.renderer,
                      static_cast<unsigned long>(context.flags),
                      value != 0 ? " (MeshUpdate rebuild failed)" : "");
        context.mesh_notes.emplace_back(line);
        return 1;
    }
    case 22:
        context.stats.truncated = !context.pending.empty();
        return 1;
    case 23: {
        char summary[512]{};
        std::snprintf(summary, sizeof(summary),
                      "Runtime Storm3D tree for '%s': root=%p nodes=%zu meshes=%zu renderers=%zu rebuilt=%zu missing-renderers=%zu%s",
                      context.database_name->c_str(), context.root,
                      context.stats.nodes, context.stats.meshes,
                      context.stats.mesh_renderers,
                      context.stats.rebuilt_renderers,
                      context.stats.missing_renderers,
                      context.stats.truncated
                          ? " (traversal capped)" : "");
        log_line(summary);
        return 1;
    }
    case 24:
        log_line(context.mesh_notes[value]);
        return 1;
    default: return 0;
    }
}

RuntimeTreeStats inspect_runtime_tree(
    void* geometry, const std::string& database_name) noexcept {
    M3DInspectContext context{geometry, &database_name};
    a2fo_rs_stage8_m3d_inspect_tree(&m3d_inspect_step, &context);
    return context.stats;
}

struct M3DCleanupContext {
    NativeMemoryInput* stream;
    void* database = nullptr;
};

std::uintptr_t m3d_cleanup_step(void* opaque, std::uint32_t op,
                                std::uintptr_t) noexcept {
    auto& context = *static_cast<M3DCleanupContext*>(opaque);
    switch (op) {
    case 0:
        if (!context.stream->database_04) return 0;
        context.database = reinterpret_cast<void*>(
            static_cast<std::uintptr_t>(
                context.stream->database_04));
        return 1;
    case 1:
        return readable_range(context.database, sizeof(void*));
    case 2: {
        char message[256]{};
        std::snprintf(message, sizeof(message),
                      "Storm3D left an unreadable partial database %p after parse failure; cleanup skipped",
                      context.database);
        log_line(message);
        return 1;
    }
    case 3: context.stream->database_04 = 0; return 1;
    case 4:
        a2fo_m3d_call_thiscall_0(
            at(kStormDatabaseDestructorRva), context.database);
        return 1;
    case 5: {
        using EngineDelete = void (__cdecl*)(void*);
        reinterpret_cast<EngineDelete>(
            at(kEngineOperatorDeleteRva))(context.database);
        return 1;
    }
    case 6: {
        char message[256]{};
        std::snprintf(message, sizeof(message),
                      "Cleaned partial Storm3D database %p after rejected M3D stream",
                      context.database);
        log_line(message);
        return 1;
    }
    default: return 0;
    }
}

void cleanup_partial_database(NativeMemoryInput& stream) noexcept {
    M3DCleanupContext context{&stream};
    a2fo_rs_stage8_m3d_cleanup_database(
        &m3d_cleanup_step, &context);
}

struct M3DLoadContext {
    const std::string* source_path;
    const char* requested_name;
    float scale;
    M3DLoadContext(const std::string* source, const char* name, float factor)
        : source_path(source), requested_name(name), scale(factor) {}
    std::vector<std::uint8_t> m3d_bytes;
    std::vector<std::uint8_t> storm_bytes;
    std::string error;
    a2fo::m3d::ConversionStats stats{};
    GuardedMemoryInput guarded{};
    std::string database_name;
    void* geometry = nullptr;
    RuntimeTreeStats runtime{};
    std::exception_ptr exception{};
};

std::uintptr_t m3d_load_step(void* opaque, std::uint32_t op,
                             std::uintptr_t) noexcept {
    auto& context = *static_cast<M3DLoadContext*>(opaque);
    const auto& source_path = *context.source_path;
    NativeMemoryInput& stream = context.guarded.native;
    try {
        switch (op) {
        case 0:
            return read_file(source_path, context.m3d_bytes, context.error);
        case 1:
            log_line("Could not read '" + source_path + "': " + context.error);
            return 1;
        case 2:
            return a2fo::m3d::convert_bytes_to_sod(
                context.m3d_bytes, context.storm_bytes,
                context.stats, context.error);
        case 3:
            log_line("Could not parse runtime M3D '" + source_path +
                     "': " + context.error);
            return 1;
        case 4: return context.storm_bytes.size() >= 14u;
        case 5:
            log_line("Runtime M3D adapter produced an invalid Storm3D stream for '" +
                     source_path + "'");
            return 1;
        case 6:
            stream.vtable = g_guarded_stream_vtable;
            stream.filename = source_path.c_str();
            stream.begin = context.storm_bytes.data();
            stream.end = context.storm_bytes.data() +
                         context.storm_bytes.size();
            stream.cursor = stream.begin;
            context.database_name =
                context.requested_name && *context.requested_name
                    ? basename_without_extension(context.requested_name)
                    : basename_without_extension(source_path.c_str());
            return 1;
        case 7:
            context.geometry = a2fo_m3d_parse_sod_stream(
                at(kParseSodStreamRva), &stream, context.scale,
                context.database_name.c_str());
            return reinterpret_cast<std::uintptr_t>(context.geometry);
        case 8: return context.guarded.overflow;
        case 9: {
            char message[768]{};
            std::snprintf(
                message, sizeof(message),
                "Armada attempted to read beyond the M3D compatibility stream for '%s' at offset %lu/%zu (requested %lu bytes); rejecting safely instead of allowing native memcpy to crash",
                source_path.c_str(),
                static_cast<unsigned long>(context.guarded.overflow_offset),
                context.storm_bytes.size(),
                static_cast<unsigned long>(context.guarded.overflow_request));
            log_line(message);
            return 1;
        }
        case 10: cleanup_partial_database(stream); return 1;
        case 11: {
            std::size_t consumed = 0;
            if (stream.cursor >= stream.begin &&
                stream.cursor <= stream.end) {
                consumed = static_cast<std::size_t>(
                    stream.cursor - stream.begin);
            }
            char message[768]{};
            std::snprintf(message, sizeof(message),
                          "Armada's Storm3D parser rejected '%s' after %zu/%zu bytes (stage=%s, parsed-version=%.6f, partial-db=%p)",
                          source_path.c_str(), consumed,
                          context.storm_bytes.size(),
                          parser_stage(consumed, context.stats),
                          stream.format_version,
                          stream.database_04
                            ? reinterpret_cast<void*>(
                                static_cast<std::uintptr_t>(
                                    stream.database_04))
                            : nullptr);
            log_line(message);
            return 1;
        }
        case 12:
            audit_runtime_hierarchy(
                context.geometry, context.stats, context.database_name);
            return 1;
        case 13:
            bind_runtime_m3d_textures(
                context.geometry, context.stats, context.database_name);
            return 1;
        case 14:
            context.runtime = inspect_runtime_tree(
                context.geometry, context.database_name);
            return 1;
        case 15:
            restore_runtime_authored_names(
                context.geometry, context.stats, context.database_name);
            return 1;
        case 16:
            return context.runtime.nodes == 0u ||
                context.runtime.meshes == 0u;
        case 17:
            log_line("Storm3D accepted M3D '" + source_path +
                     "' but its active root reaches no renderable mesh nodes; hierarchy/root adaptation is required");
            return 1;
        case 18:
            log_line("Storm3D accepted M3D '" + source_path +
                     "' and created mesh nodes, but none has a live renderer after MeshUpdate(0)");
            return 1;
        case 19: report_conversion(source_path, context.stats); return 1;
        case 20: {
            char message[512]{};
            std::snprintf(message, sizeof(message),
                          "Loaded M3D '%s' directly into Storm3D runtime as database '%s' (%p), scale=%.6f; no SOD file created",
                          source_path.c_str(), context.database_name.c_str(),
                          context.geometry,
                          static_cast<double>(context.scale));
            log_line(message);
            return 1;
        }
        case 21: return context.runtime.mesh_renderers == 0u;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

void* load_m3d_runtime(const std::string& source_path,
                       const char* requested_name, float scale) noexcept {
    M3DLoadContext context{&source_path, requested_name, scale};
    const auto result = a2fo_rs_stage8_m3d_load(
        &m3d_load_step, &context);
    if (context.exception) {
        log_line("Unexpected exception while loading M3D '" + source_path + "'");
        return nullptr;
    }
    return reinterpret_cast<void*>(result);
}

struct M3DFinalizeContext {
    void* geometry;
    bool no_fallback;
    void* resource = nullptr;
};

std::uintptr_t m3d_finalize_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t) noexcept {
    auto& context = *static_cast<M3DFinalizeContext*>(opaque);
    switch (op) {
    case 0: return readable_range(context.geometry, 0x44u);
    case 1:
        log_line("Parsed M3D returned an unreadable Fleet Ops model object; refusing to return an unfinalized database");
        return 1;
    case 2:
        context.resource = *reinterpret_cast<void**>(
            static_cast<std::uint8_t*>(context.geometry) + 0x40u);
        return readable_range(context.resource, 0x15u);
    case 3: {
        char message[320]{};
        std::snprintf(message, sizeof(message),
                      "Parsed M3D model %p has no readable Fleet Ops resource at +0x40 (%p); loader return contract mismatch",
                      context.geometry, context.resource);
        log_line(message);
        return 1;
    }
    case 4: {
        auto* bytes = static_cast<std::uint8_t*>(context.resource);
        auto* references =
            reinterpret_cast<std::uint32_t*>(bytes + 0x10u);
        ++(*references);
        bytes[0x14u] = context.no_fallback ? 1u : 0u;
        return 1;
    }
    case 5: {
        auto* bytes = static_cast<std::uint8_t*>(context.resource);
        const auto* references =
            reinterpret_cast<const std::uint32_t*>(bytes + 0x10u);
        char message[384]{};
        std::snprintf(message, sizeof(message),
                      "Finalized M3D model %p through Fleet Ops resource %p: refCount=%lu mode=%u",
                      context.geometry, context.resource,
                      static_cast<unsigned long>(*references),
                      static_cast<unsigned>(bytes[0x14u]));
        log_line(message);
        return 1;
    }
    default: return 0;
    }
}

bool finalize_fleetops_model(void* geometry, bool no_fallback) noexcept {
    M3DFinalizeContext context{geometry, no_fallback};
    return a2fo_rs_stage8_m3d_finalize(
        &m3d_finalize_step, &context) != 0;
}

void* call_original(const A2FO_InlineHook& hook, void* self,
                    const char* name, float scale) noexcept {
    if (!hook.gateway) return nullptr;
    return a2fo_m3d_call_load_sod(hook.gateway, self, name, scale);
}

struct M3DResolveContext {
    const A2FO_InlineHook* hook;
    void* self;
    const char* name;
    float scale;
    bool no_fallback;
    std::string key;
    std::string cache_key;
    std::unordered_map<std::string, M3DSource>::iterator source{};
    std::unordered_map<std::string, void*>::iterator cached{};
    void* geometry = nullptr;
};

std::uintptr_t m3d_resolve_step(void* opaque, std::uint32_t op,
                                std::uintptr_t) noexcept {
    auto& context = *static_cast<M3DResolveContext*>(opaque);
    switch (op) {
    case 0: return runtime_enabled() && context.name && *context.name;
    case 1: context.key = model_key(context.name); return 1;
    case 2:
        context.source = g_sources.find(context.key);
        return context.source != g_sources.end();
    case 3:
        context.cache_key = runtime_cache_key(context.key, context.scale);
        return 1;
    case 4: return g_failed_runtime.find(context.cache_key) !=
                   g_failed_runtime.end();
    case 5:
        log_line("Fleet Ops requested model '" + std::string(context.name) +
                 "'; resolving M3D winner '" + context.source->second.path +
                 "'");
        return 1;
    case 6:
        context.cached = g_loaded.find(context.cache_key);
        return context.cached != g_loaded.end() && context.cached->second;
    case 7:
        return finalize_fleetops_model(
            context.cached->second, context.no_fallback);
    case 8:
        return reinterpret_cast<std::uintptr_t>(context.cached->second);
    case 9: return g_loading.insert(context.cache_key).second;
    case 10:
        context.geometry = load_m3d_runtime(
            context.source->second.path, context.name, context.scale);
        return reinterpret_cast<std::uintptr_t>(context.geometry);
    case 11: g_loading.erase(context.cache_key); return 1;
    case 12:
        return finalize_fleetops_model(
            context.geometry, context.no_fallback);
    case 13: g_failed_runtime.insert(context.cache_key); return 1;
    case 14:
        log_line("M3D parsed successfully but Fleet Ops model finalization failed for basename '" +
                 context.key + "'; falling back to native SOD/default path");
        return 1;
    case 15:
        g_loaded[context.cache_key] = context.geometry;
        return 1;
    case 17:
        log_line("M3D runtime load failed for basename '" + context.key +
                 "'; suppressing further M3D parse attempts for this model/scale and falling back to Fleet Ops' native SOD/default path");
        return 1;
    case 18:
        return reinterpret_cast<std::uintptr_t>(call_original(
            *context.hook, context.self, context.name, context.scale));
    case 19:
        g_loaded.erase(context.cached);
        return 1;
    default: return 0;
    }
}

void* resolve_m3d_or_original(const A2FO_InlineHook& hook, void* self,
                              const char* name, float scale,
                              bool no_fallback) noexcept {
    M3DResolveContext context{&hook, self, name, scale, no_fallback, {}, {}};
    return reinterpret_cast<void*>(a2fo_rs_stage8_m3d_resolve(
        &m3d_resolve_step, &context));
}

void* __attribute__((fastcall)) load_sod_hook(
    void* self, void*, const char* name, float scale) noexcept {
    return resolve_m3d_or_original(g_load_hook, self, name, scale, false);
}

void* __attribute__((fastcall)) load_sod_no_fallback_hook(
    void* self, void*, const char* name, float scale) noexcept {
    return resolve_m3d_or_original(g_load_no_fallback_hook, self, name, scale, true);
}

struct M3DInitContext {
    const A2FO_ModuleApi* api;
    std::exception_ptr exception{};
};

std::uintptr_t m3d_init_step(void* opaque, std::uint32_t op,
                             std::uintptr_t index) noexcept {
    auto& context = *static_cast<M3DInitContext*>(opaque);
    const auto* api = context.api;
    try {
        switch (op) {
        case 0:
            return api && api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE &&
                api->api_version == A2FO_MODULE_API_VERSION && api->log &&
                api->armada_module && api->fleetops_module &&
                api->install_inline_hook && api->extension_root_count &&
                api->extension_root;
        case 1:
            g_api = api;
            g_armada = static_cast<HMODULE>(api->armada_module());
            return 1;
        case 2:
            g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
            return 1;
        case 3:
            if (HMODULE core = GetModuleHandleA("A2FOExtensions.dll")) {
                const auto register_mesh = GetProcAddress(
                    core, "A2FO_NebulaRegisterM3DMesh");
                const auto clear_meshes = GetProcAddress(
                    core, "A2FO_NebulaClearM3DMeshes");
                static_assert(sizeof(g_register_m3d_mesh) ==
                              sizeof(register_mesh));
                static_assert(sizeof(g_clear_m3d_meshes) ==
                              sizeof(clear_meshes));
                std::memcpy(&g_register_m3d_mesh, &register_mesh,
                            sizeof(register_mesh));
                std::memcpy(&g_clear_m3d_meshes, &clear_meshes,
                            sizeof(clear_meshes));
            }
            return 1;
        case 4: return g_armada && g_fleet_ops;
        case 5: return readable_range(at(kParseSodStreamRva), 8);
        case 6: return readable_range(
            at(kInputStreamVtableRva), sizeof(g_guarded_stream_vtable));
        case 7: return readable_range(at(kStormDatabaseDestructorRva), 8);
        case 8: return readable_range(at(kEngineOperatorDeleteRva), 8);
        case 9: return readable_range(
            at(kMeshUpdateRva), sizeof(kExpectedMeshUpdate));
        case 10:
            log_line("Native M3D loading unavailable: Armada Storm3D parser/layout does not match the supported build");
            return 1;
        case 11: {
            const auto* bytes =
                reinterpret_cast<const std::uint8_t*>(g_fleet_ops) +
                (index == 0 ? kFoLoadSodHandlerRva :
                              kFoLoadSodNoFallbackHandlerRva);
            if (index == 0) {
                return readable_range(
                    bytes, sizeof(kExpectedFoLoadSodHandler)) &&
                    std::memcmp(bytes, kExpectedFoLoadSodHandler,
                                sizeof(kExpectedFoLoadSodHandler)) == 0;
            }
            return readable_range(
                bytes, sizeof(kExpectedFoLoadSodNoFallbackHandler)) &&
                std::memcmp(bytes, kExpectedFoLoadSodNoFallbackHandler,
                            sizeof(kExpectedFoLoadSodNoFallbackHandler)) == 0;
        }
        case 12:
            log_line("Native M3D loading unavailable: Fleet Ops SOD resolver signatures do not match the supported build");
            return 1;
        case 13:
            std::memcpy(g_guarded_stream_vtable, at(kInputStreamVtableRva),
                        sizeof(g_guarded_stream_vtable));
            g_guarded_stream_vtable[4] =
                reinterpret_cast<void*>(&guarded_raw_read);
            return 1;
        case 14: return build_source_index();
        case 15:
            log_line("Native M3D loading unavailable: extension roots could not be indexed");
            return 1;
        case 16:
            if (index == 0) {
                return api->install_inline_hook(
                    reinterpret_cast<std::uint8_t*>(g_fleet_ops) +
                        kFoLoadSodHandlerRva,
                    reinterpret_cast<void*>(&load_sod_hook),
                    sizeof(kExpectedFoLoadSodHandler),
                    kExpectedFoLoadSodHandler, &g_load_hook);
            }
            return api->install_inline_hook(
                reinterpret_cast<std::uint8_t*>(g_fleet_ops) +
                    kFoLoadSodNoFallbackHandlerRva,
                reinterpret_cast<void*>(&load_sod_no_fallback_hook),
                sizeof(kExpectedFoLoadSodNoFallbackHandler),
                kExpectedFoLoadSodNoFallbackHandler,
                &g_load_no_fallback_hook);
        case 17:
            log_line("Native M3D loading unavailable: Fleet Ops SOD resolver hooks could not be installed");
            return 1;
        case 18: g_ready = true; return 1;
        case 19:
            log_line("M3D compatibility build 2026-09-29-damage-vents: child=+0x1c sibling=+0x20; break vents mapped to native damage selectors; authored in_* meshes retained");
            return 1;
        case 20:
            log_line("Native M3D resolver chained after Fleet Ops model/VFS resolver; M3D assets are parsed on demand in memory and no SOD caches are written; Fleet Ops 1.93 multi-texture + 1.92 Borg-texture mesh layout, guarded stream reads, failed-parser database cleanup, hierarchy normalization, runtime mesh-tree diagnostics and M3D-only diffuse draw tagging enabled");
            return 1;
        default: return 0;
        }
    } catch (...) {
        context.exception = std::current_exception();
        return UINTPTR_MAX;
    }
}

struct M3DShutdownContext {
    std::exception_ptr exception{};
};

std::uintptr_t m3d_shutdown_step(void* opaque, std::uint32_t op,
                                 std::uintptr_t) noexcept {
    auto& context = *static_cast<M3DShutdownContext*>(opaque);
    try {
        switch (op) {
        case 0:
            if (g_clear_m3d_meshes) g_clear_m3d_meshes();
            return 1;
        case 1: g_ready = false; return 1;
        case 2: g_loading.clear(); return 1;
        case 3: g_loaded.clear(); return 1;
        case 4: g_failed_runtime.clear(); return 1;
        case 5: g_sources.clear(); return 1;
        case 6: g_fleet_ops = nullptr; return 1;
        case 7: g_armada = nullptr; return 1;
        case 8: g_register_m3d_mesh = nullptr; return 1;
        case 9: g_clear_m3d_meshes = nullptr; return 1;
        case 10: g_api = nullptr; return 1;
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
    M3DInitContext context{api, {}};
    const bool ready = a2fo_rs_stage8_m3d_init(
        &m3d_init_step, &context) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return ready;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    M3DShutdownContext context{};
    a2fo_rs_stage8_m3d_shutdown(&m3d_shutdown_step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
}

#ifndef A2FO_BUILTIN_FEATURE
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) {
    return TRUE;
}
#endif
