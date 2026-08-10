/*
 * File: modules/A2FOFeaturePack/odf_recursive.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Recursive ODF discovery, overlay publication, and winner lookup.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../core/fpq_paths.hpp"
#include "../../core/odf_paths.hpp"
#include "bink_video.hpp"
#include "queue_enhancement.hpp"
#include "upgrade_pods.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr const char* kModuleName = "A2FOFeaturePack";
constexpr std::uint32_t kFleetOpsTimestamp = 0x51f6475c;
constexpr std::uint32_t kFleetOpsImageSize = 0x00322000;

constexpr std::uintptr_t kLStrClearRva = 0x0056b8;
constexpr std::uintptr_t kLStrFromPCharRva = 0x0058b0;
constexpr std::uintptr_t kTListAddRva = 0x080824;
constexpr std::uintptr_t kHashStringRva = 0x0fa83c;
constexpr std::uintptr_t kVirtualDirectoryClassRefRva = 0x10870c;
constexpr std::uintptr_t kVirtualDirectoryCtorRva = 0x108b6c;
constexpr std::uintptr_t kRenewOverridesRva = 0x108c14;
constexpr std::uintptr_t kAddFileToHashTableRva = 0x1092d0;
constexpr std::uintptr_t kAddItemsFromDiskRva = 0x109488;
constexpr std::uintptr_t kAddItemsFromPackRva = 0x109650;

constexpr std::size_t kBuiltInVirtualDirectoryCount = 28;
constexpr std::size_t kMaximumVirtualDirectoryCount = 255;
constexpr std::size_t kMaximumPathLength = 32767;
constexpr std::size_t kDetailedIndexLogLimit = 24;
constexpr std::size_t kLookupLogLimit = 64;

const std::uint8_t kExpectedAddDisk[] = {
    0x55, 0x8b, 0xec, 0x81, 0xc4, 0xa4, 0xfe, 0xff, 0xff};
const std::uint8_t kExpectedAddPack[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xcc};
const std::uint8_t kExpectedHashString[] = {
    0x55, 0x8b, 0xec, 0x83, 0xc4, 0xf4, 0x53, 0x56, 0x57};
const std::uint8_t kExpectedAddFileToHash[] = {
    0x53, 0x56, 0x51, 0x8b, 0xf1, 0x89, 0x14, 0x24};
struct DelphiList {
    void* vtable;
    void** items;
    std::int32_t count;
    std::int32_t capacity;
};

struct RootInfo {
    void* object = nullptr;
    void* mod_info = nullptr;
    std::string runtime_path;
    std::string absolute_path;
    bool primary = false;
};

struct ArchiveInfo {
    void* mod_info = nullptr;
    std::string runtime_path;
    std::string absolute_path;
};

struct OdfDirectoryCandidate {
    std::string path;
    std::uint32_t precedence = A2FO_ODF_OVERLAY_NORMAL;
};

struct CustomEntryInfo {
    std::string directory_path;
    std::uint32_t overlay_precedence = A2FO_ODF_OVERLAY_NORMAL;
};

constexpr std::size_t kFileEntryNextOffset = 0x08;
constexpr std::size_t kFileEntryBasenameOffset = 0x0c;
constexpr std::size_t kFileEntryProjectIdOffset = 0x14;
constexpr std::size_t kFileEntryPackedOffset = 0x18;
constexpr std::size_t kFileEntryModInfoOffset = 0x1c;
constexpr std::size_t kFileEntryPrimaryRootOffset = 0x20;
constexpr std::size_t kFileEntryOverriddenOffset = 0x21;
constexpr std::size_t kModInfoPriorityOffset = 0x3c;

extern "C" void a2fo_odf_lstr_from_pchar(void* function, void** output,
                                           const char* text);
extern "C" void a2fo_odf_lstr_clear(void* function, void** value);
extern "C" void* a2fo_odf_create_virtual_directory(
    void* function, void* class_ref, void* delphi_string,
    std::uint32_t flag, std::uint32_t index);
extern "C" int a2fo_odf_tlist_add(void* function, void* list, void* item);
extern "C" std::uint32_t a2fo_odf_hash_string(void* function,
                                                void* delphi_string);
extern "C" void a2fo_odf_add_file_to_hash(
    void* function, void* file_system, void* entry, std::uint32_t index);
extern "C" void a2fo_odf_add_items_from_disk(
    void* function, void* file_system, void* root, void* mod_info,
    std::uint32_t* count, std::uint32_t flag);
extern "C" void a2fo_odf_add_items_from_pack(
    void* function, void* file_system, void* delphi_path, void* mod_info,
    std::uint32_t* count);
extern "C" void a2fo_odf_renew_overrides(void* function, void* directory);
const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
CRITICAL_SECTION g_state_lock;
bool g_state_lock_ready = false;
volatile LONG g_recursive_odf_state = 0;
volatile DWORD g_recursive_odf_owner_thread = 0;
std::map<std::string, std::string> g_recursive_odf_aliases;
std::map<std::string, void*> g_recursive_odf_winners;
std::set<std::string> g_logged_recursive_odf_lookups;
std::unordered_map<void*, std::uint32_t> g_project_ids_by_entry;
void** g_project_id_items = nullptr;
std::uint32_t g_project_id_scanned_count = 0;
std::uint32_t g_project_id_capacity = 0;
bool g_project_id_capacity_warning_logged = false;

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) {
    return reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(module) + rva);
}

void log_line(const std::string& text) {
    if (g_api && g_api->log) {
        g_api->log(kModuleName, text.c_str());
    }
}

bool validate_fleetops() {
    if (!g_fleet_ops) {
        log_line("FleetOpsHook.dll is not loaded");
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_fleet_ops);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        log_line("FleetOpsHook.dll has no DOS header");
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const std::uint8_t*>(g_fleet_ops) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp != kFleetOpsTimestamp ||
        nt->OptionalHeader.SizeOfImage != kFleetOpsImageSize) {
        char message[192]{};
        std::snprintf(message, sizeof(message),
                      "FleetOpsHook.dll version mismatch "
                      "(timestamp=%08lx, image=%08lx)",
                      static_cast<unsigned long>(nt->FileHeader.TimeDateStamp),
                      static_cast<unsigned long>(nt->OptionalHeader.SizeOfImage));
        log_line(message);
        return false;
    }
    if (std::memcmp(at(g_fleet_ops, kHashStringRva), kExpectedHashString,
                    sizeof(kExpectedHashString)) != 0 ||
        std::memcmp(at(g_fleet_ops, kAddFileToHashTableRva),
                    kExpectedAddFileToHash,
                    sizeof(kExpectedAddFileToHash)) != 0 ||
        std::memcmp(at(g_fleet_ops, kAddItemsFromDiskRva), kExpectedAddDisk,
                    sizeof(kExpectedAddDisk)) != 0 ||
        std::memcmp(at(g_fleet_ops, kAddItemsFromPackRva), kExpectedAddPack,
                    sizeof(kExpectedAddPack)) != 0) {
        log_line("Fleet Operations filesystem scanner signature mismatch");
        return false;
    }
    return true;
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char ch) {
                       return static_cast<char>(std::tolower(ch));
                   });
    return value;
}

void replace_slashes(std::string& value, char slash = '\\') {
    for (char& ch : value) {
        if (ch == '/' || ch == '\\') {
            ch = slash;
        }
    }
}

void trim_trailing_slashes(std::string& value) {
    while (value.size() > 3 &&
           (value.back() == '\\' || value.back() == '/')) {
        value.pop_back();
    }
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

bool is_absolute_path(const std::string& path) {
    return path.size() >= 3 &&
           std::isalpha(static_cast<unsigned char>(path[0])) &&
           path[1] == ':' && (path[2] == '\\' || path[2] == '/');
}

std::string full_path(const std::string& path) {
    std::vector<char> buffer(kMaximumPathLength);
    const DWORD length = GetFullPathNameA(
        path.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    if (length == 0 || length >= buffer.size()) {
        return path;
    }
    std::string result(buffer.data(), length);
    replace_slashes(result);
    trim_trailing_slashes(result);
    return result;
}

std::string delphi_string(const void* owner, std::size_t offset) {
    if (!owner) return {};
    const auto* text = *reinterpret_cast<char* const*>(
        static_cast<const std::uint8_t*>(owner) + offset);
    if (!text || IsBadStringPtrA(text, kMaximumPathLength)) return {};
    return std::string(text);
}

std::string delphi_string_value(const void* value) {
    const auto* text = static_cast<const char*>(value);
    if (!text || IsBadStringPtrA(text, kMaximumPathLength)) return {};
    return std::string(text);
}

std::vector<void*> list_items(DelphiList* list) {
    std::vector<void*> result;
    if (!list || list->count <= 0 || list->count > 1000000 || !list->items) {
        return result;
    }
    result.assign(list->items, list->items + list->count);
    return result;
}

bool readable_range(const void* pointer, std::size_t size) {
    return pointer && size != 0 && !IsBadReadPtr(pointer, size);
}

bool writable_range(void* pointer, std::size_t size) {
    return pointer && size != 0 && !IsBadWritePtr(pointer, size);
}

bool readable_list(DelphiList* list, std::int32_t minimum_count,
                   std::int32_t maximum_count) {
    if (!readable_range(list, sizeof(*list))) return false;
    if (list->count < minimum_count || list->count > maximum_count ||
        list->capacity < list->count) {
        return false;
    }
    return list->count == 0 ||
           readable_range(list->items,
                          static_cast<std::size_t>(list->count) * sizeof(void*));
}

bool has_odf_extension(const std::string& name) {
    const std::string lower = lower_ascii(name);
    return lower.size() >= 4 &&
           lower.compare(lower.size() - 4, 4, ".odf") == 0;
}

void scan_loose_odfs(const std::string& absolute_directory,
                     const std::string& relative_directory,
                     std::uint32_t precedence,
                     std::map<std::string, OdfDirectoryCandidate>& directories,
                     unsigned depth = 0) {
    if (depth > 64) return;
    WIN32_FIND_DATAA data{};
    HANDLE search = FindFirstFileA(
        join_path(absolute_directory, "*").c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) return;
    bool contains_odf = false;
    do {
        const std::string name = data.cFileName;
        if (name == "." || name == "..") continue;
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
                scan_loose_odfs(join_path(absolute_directory, name),
                                join_path(relative_directory, name),
                                precedence, directories, depth + 1);
            }
        } else if (has_odf_extension(name)) {
            contains_odf = true;
        }
    } while (FindNextFileA(search, &data));
    FindClose(search);
    if (contains_odf) {
        std::string normalized = relative_directory;
        replace_slashes(normalized);
        const std::string key = lower_ascii(normalized);
        const auto existing = directories.find(key);
        if (existing == directories.end() ||
            precedence > existing->second.precedence) {
            directories[key] = OdfDirectoryCandidate{
                normalized, precedence};
        }
    }
}

std::vector<OdfDirectoryCandidate> registered_odf_overlays() {
    std::vector<OdfDirectoryCandidate> result;
    if (!g_api ||
        !A2FO_MODULE_API_HAS(g_api, get_odf_overlay_directory) ||
        (g_api->capabilities & A2FO_CAP_ODF_OVERLAY_DIRECTORIES) == 0 ||
        !g_api->odf_overlay_directory_count ||
        !g_api->get_odf_overlay_directory) {
        return result;
    }
    const std::uint32_t count = g_api->odf_overlay_directory_count();
    if (count > 64) {
        log_line("ODF overlay registry exceeded its supported limit");
        return result;
    }
    result.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        char path[256]{};
        std::uint32_t precedence = A2FO_ODF_OVERLAY_NORMAL;
        if (!g_api->get_odf_overlay_directory(
                index, path, sizeof(path), &precedence) || !*path ||
            precedence > A2FO_ODF_OVERLAY_OVERRIDE) {
            log_line("Ignored invalid ODF overlay registry entry " +
                     std::to_string(index));
            continue;
        }
        result.push_back(OdfDirectoryCandidate{path, precedence});
    }
    return result;
}

bool read_fpq_metadata(const std::string& path,
                       std::vector<std::uint8_t>& bytes) {
    HANDLE file = CreateFileA(
        path.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    std::uint8_t header[0x1c]{};
    DWORD got = 0;
    if (!ReadFile(file, header, sizeof(header), &got, nullptr) ||
        got != sizeof(header) || std::memcmp(header, "FPQ\0", 4) != 0) {
        CloseHandle(file);
        return false;
    }
    auto u32 = [&](std::size_t offset) {
        std::uint32_t value = 0;
        std::memcpy(&value, header + offset, sizeof(value));
        return value;
    };
    const std::uint64_t metadata_size =
        0x1cull + static_cast<std::uint64_t>(u32(0x10)) * 12ull +
        static_cast<std::uint64_t>(u32(0x08)) * 12ull +
        static_cast<std::uint64_t>(u32(0x0c)) * 25ull + u32(0x18);
    if (metadata_size < sizeof(header) ||
        metadata_size > 128ull * 1024ull * 1024ull) {
        CloseHandle(file);
        return false;
    }
    bytes.resize(static_cast<std::size_t>(metadata_size));
    LARGE_INTEGER zero{};
    SetFilePointerEx(file, zero, nullptr, FILE_BEGIN);
    std::size_t total = 0;
    while (total < bytes.size()) {
        const DWORD wanted = static_cast<DWORD>(
            std::min<std::size_t>(bytes.size() - total, 1024 * 1024));
        DWORD read = 0;
        if (!ReadFile(file, bytes.data() + total, wanted, &read, nullptr) ||
            read == 0) {
            CloseHandle(file);
            return false;
        }
        total += read;
    }
    CloseHandle(file);
    return true;
}

std::unordered_map<void*, void*> map_roots_to_mod_info(void* file_system) {
    std::unordered_map<void*, void*> result;
    auto* directories = *reinterpret_cast<DelphiList**>(
        static_cast<std::uint8_t*>(file_system) + 4);
    for (void* directory : list_items(directories)) {
        auto* entries = *reinterpret_cast<DelphiList**>(
            static_cast<std::uint8_t*>(directory) + 0x0c);
        for (void* entry : list_items(entries)) {
            void* root = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(entry) + 0x24);
            void* mod_info = *reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(entry) + 0x1c);
            if (root && result.find(root) == result.end()) {
                result.emplace(root, mod_info);
            }
        }
    }
    return result;
}

std::vector<RootInfo> collect_roots(void* file_system,
                                    const std::string& data_dir) {
    std::vector<RootInfo> result;
    const auto mod_info = map_roots_to_mod_info(file_system);
    auto* roots = *reinterpret_cast<DelphiList**>(
        static_cast<std::uint8_t*>(file_system) + 0x18);
    const auto root_objects = list_items(roots);
    for (std::size_t index = 0; index < root_objects.size(); ++index) {
        RootInfo root;
        root.object = root_objects[index];
        const auto found = mod_info.find(root.object);
        root.mod_info = found == mod_info.end() ? nullptr : found->second;
        root.runtime_path = delphi_string(root.object, 4);
        replace_slashes(root.runtime_path);
        const std::string candidate = is_absolute_path(root.runtime_path)
            ? root.runtime_path : join_path(data_dir, root.runtime_path);
        root.absolute_path = full_path(candidate);
        root.primary = (index % 2) == 0;
        result.push_back(std::move(root));
    }
    for (std::size_t index = 0; index < result.size(); ++index) {
        if (!result[index].mod_info && (index ^ 1u) < result.size()) {
            result[index].mod_info = result[index ^ 1u].mod_info;
        }
    }
    return result;
}

std::vector<ArchiveInfo> collect_archives(
    const std::vector<RootInfo>& roots,
    std::map<std::string, OdfDirectoryCandidate>& directories) {
    std::set<std::string> seen;
    std::vector<ArchiveInfo> result;
    for (const RootInfo& root : roots) {
        ArchiveInfo archive;
        archive.mod_info = root.mod_info;
        archive.runtime_path = join_path(root.runtime_path, "odf.fpq");
        archive.absolute_path = full_path(
            join_path(root.absolute_path, "odf.fpq"));
        if (GetFileAttributesA(archive.absolute_path.c_str()) ==
            INVALID_FILE_ATTRIBUTES) {
            continue;
        }
        if (!seen.insert(lower_ascii(archive.absolute_path)).second) continue;
        std::vector<std::uint8_t> metadata;
        if (!read_fpq_metadata(archive.absolute_path, metadata)) {
            log_line("Could not read FPQ metadata: " + archive.absolute_path);
            continue;
        }
        const a2fo::FpqPathResult parsed =
            a2fo::parse_fpq_odf_directories(metadata);
        if (!parsed.ok) {
            log_line("Could not parse FPQ metadata: " + archive.absolute_path +
                     " (" + parsed.error + ")");
            continue;
        }
        for (const std::string& path : parsed.odf_directories) {
            directories.emplace(
                lower_ascii(path),
                OdfDirectoryCandidate{path, A2FO_ODF_OVERLAY_NORMAL});
        }
        result.push_back(std::move(archive));
    }
    return result;
}

class TemporaryDirectoryList {
public:
    TemporaryDirectoryList(DelphiList* list, std::vector<void*>& replacement)
        : list_(list), items_(list->items), count_(list->count),
          capacity_(list->capacity) {
        list_->items = replacement.empty() ? nullptr : replacement.data();
        list_->count = static_cast<std::int32_t>(replacement.size());
        list_->capacity = list_->count;
    }

    ~TemporaryDirectoryList() {
        list_->items = items_;
        list_->count = count_;
        list_->capacity = capacity_;
    }

    TemporaryDirectoryList(const TemporaryDirectoryList&) = delete;
    TemporaryDirectoryList& operator=(const TemporaryDirectoryList&) = delete;

private:
    DelphiList* list_;
    void** items_;
    std::int32_t count_;
    std::int32_t capacity_;
};

class StateLockGuard {
public:
    StateLockGuard() { EnterCriticalSection(&g_state_lock); }
    ~StateLockGuard() { LeaveCriticalSection(&g_state_lock); }

    StateLockGuard(const StateLockGuard&) = delete;
    StateLockGuard& operator=(const StateLockGuard&) = delete;
};

void* create_virtual_directory(const std::string& path, std::uint32_t index) {
    void* delphi_path = nullptr;
    a2fo_odf_lstr_from_pchar(at(g_fleet_ops, kLStrFromPCharRva),
                             &delphi_path, path.c_str());
    void* class_ref = *at<void*>(g_fleet_ops, kVirtualDirectoryClassRefRva);
    void* result = a2fo_odf_create_virtual_directory(
        at(g_fleet_ops, kVirtualDirectoryCtorRva), class_ref,
        delphi_path, 1, index);
    a2fo_odf_lstr_clear(at(g_fleet_ops, kLStrClearRva), &delphi_path);
    return result;
}

template <typename T>
T& field(void* object, std::size_t offset) {
    return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(object) + offset);
}

bool refresh_project_id_registry(void* file_system) {
    if (!readable_range(file_system, 0x18)) return false;

    const std::uint32_t count = field<std::uint32_t>(file_system, 0x10);
    void** items = field<void**>(file_system, 0x14);
    if (count > 1000000 || !items ||
        reinterpret_cast<std::uintptr_t>(items) < sizeof(std::int32_t) ||
        !readable_range(items - 1, sizeof(std::int32_t))) {
        return false;
    }

    // Fleet Ops stores the Delphi dynamic-array length immediately before the
    // first item. FOFS_ItemInitCache normally reserves 0x4000 entries.
    const std::int32_t signed_capacity =
        reinterpret_cast<const std::int32_t*>(items)[-1];
    if (signed_capacity <= 0 || signed_capacity > 1000000) return false;
    const std::uint32_t capacity =
        static_cast<std::uint32_t>(signed_capacity);
    if (count > capacity ||
        (count != 0 &&
         !readable_range(items,
                         static_cast<std::size_t>(count) * sizeof(void*)))) {
        return false;
    }

    // Rebuild only if Fleet Ops moved or shortened its dynamic array. In the
    // common case this scans the native registry once and then only consumes
    // entries appended since the preceding recursive lookup.
    if (items != g_project_id_items || count < g_project_id_scanned_count) {
        g_project_ids_by_entry.clear();
        g_project_id_items = items;
        g_project_id_scanned_count = 0;
    }
    for (std::uint32_t index = g_project_id_scanned_count;
         index < count; ++index) {
        if (items[index]) {
            g_project_ids_by_entry.emplace(items[index], index + 1);
        }
    }
    g_project_id_scanned_count = count;
    g_project_id_capacity = capacity;
    return true;
}

bool ensure_fleetops_project_id(void* file_system, void* entry,
                                bool& added) {
    added = false;
    if (!entry ||
        !writable_range(entry,
                        kFileEntryProjectIdOffset + sizeof(std::uint32_t)) ||
        !refresh_project_id_registry(file_system)) {
        return false;
    }

    std::uint32_t& project_id =
        field<std::uint32_t>(entry, kFileEntryProjectIdOffset);
    if (project_id != 0) {
        return project_id <= g_project_id_scanned_count &&
               g_project_id_items[project_id - 1] == entry;
    }

    const auto existing = g_project_ids_by_entry.find(entry);
    if (existing != g_project_ids_by_entry.end()) {
        project_id = existing->second;
        return true;
    }

    const std::uint32_t count = g_project_id_scanned_count;
    if (count >= g_project_id_capacity ||
        !writable_range(g_project_id_items + count, sizeof(void*)) ||
        !writable_range(static_cast<std::uint8_t*>(file_system) + 0x10,
                        sizeof(std::uint32_t))) {
        return false;
    }

    // Append only. Existing cPrjID values are one-based array positions, so
    // sorting or rebuilding the array after class loading has begun would
    // invalidate every ID already held by Armada.
    g_project_id_items[count] = entry;
    project_id = count + 1;
    field<std::uint32_t>(file_system, 0x10) = count + 1;
    g_project_ids_by_entry.emplace(entry, project_id);
    g_project_id_scanned_count = count + 1;
    added = true;
    return true;
}

std::uint32_t overlay_precedence(
    void* entry, const std::map<void*, CustomEntryInfo>& custom_entries) {
    const auto found = custom_entries.find(entry);
    return found == custom_entries.end()
        ? A2FO_ODF_OVERLAY_NORMAL
        : found->second.overlay_precedence;
}

bool file_entry_precedes(
    void* left, void* right,
    const std::map<void*, CustomEntryInfo>& custom_entries) {
    void* left_mod = field<void*>(left, kFileEntryModInfoOffset);
    void* right_mod = field<void*>(right, kFileEntryModInfoOffset);
    if (left_mod != right_mod) {
        if (!left_mod) return false;
        if (!right_mod) return true;
        const std::int32_t left_priority =
            field<std::int32_t>(left_mod, kModInfoPriorityOffset);
        const std::int32_t right_priority =
            field<std::int32_t>(right_mod, kModInfoPriorityOffset);
        return left_priority < right_priority;
    }

    const std::uint32_t left_overlay =
        overlay_precedence(left, custom_entries);
    const std::uint32_t right_overlay =
        overlay_precedence(right, custom_entries);
    if (left_overlay != right_overlay) return left_overlay > right_overlay;

    const std::uint8_t left_primary =
        field<std::uint8_t>(left, kFileEntryPrimaryRootOffset);
    const std::uint8_t right_primary =
        field<std::uint8_t>(right, kFileEntryPrimaryRootOffset);
    if (left_primary != right_primary) return left_primary > right_primary;

    const std::uint8_t left_packed =
        field<std::uint8_t>(left, kFileEntryPackedOffset);
    const std::uint8_t right_packed =
        field<std::uint8_t>(right, kFileEntryPackedOffset);
    if (left_packed != right_packed) return left_packed < right_packed;
    return false;
}

bool collect_hash_bucket(void* file_system, std::uint32_t index,
                         std::vector<void*>& entries) {
    entries.clear();
    const std::uint32_t bucket_count =
        field<std::uint32_t>(file_system, 0x08);
    void** buckets = field<void**>(file_system, 0x0c);
    const std::uint32_t item_count =
        field<std::uint32_t>(file_system, 0x20);
    if (bucket_count == 0 || bucket_count > 0x100000 ||
        index >= bucket_count || !buckets ||
        !readable_range(buckets,
                        static_cast<std::size_t>(bucket_count) * sizeof(void*))) {
        return false;
    }

    std::set<void*> visited;
    void* entry = buckets[index];
    while (entry) {
        if (entries.size() >= item_count || !readable_range(entry, 0x2c) ||
            !visited.insert(entry).second) {
            entries.clear();
            return false;
        }
        entries.push_back(entry);
        entry = field<void*>(entry, kFileEntryNextOffset);
    }
    return true;
}

std::size_t add_custom_entries_to_hash(
    void* file_system, const std::vector<void*>& custom_directories) {
    const std::uint32_t bucket_count =
        field<std::uint32_t>(file_system, 0x08);
    void** buckets = field<void**>(file_system, 0x0c);
    if (bucket_count == 0 || bucket_count > 0x100000 || !buckets ||
        !readable_range(buckets,
                        static_cast<std::size_t>(bucket_count) * sizeof(void*))) {
        log_line("Hash insertion: invalid Fleet Operations hash table");
        return 0;
    }

    std::size_t added = 0;
    for (void* directory : custom_directories) {
        auto* entries = field<DelphiList*>(directory, 0x0c);
        for (void* entry : list_items(entries)) {
            void* basename = field<void*>(entry, kFileEntryBasenameOffset);
            if (!basename ||
                !has_odf_extension(delphi_string_value(basename))) {
                continue;
            }
            const std::uint32_t hash = a2fo_odf_hash_string(
                at(g_fleet_ops, kHashStringRva), basename);
            const std::uint32_t index = hash & (bucket_count - 1);
            std::vector<void*> chain;
            if (!collect_hash_bucket(file_system, index, chain)) {
                log_line("Hash insertion: malformed target bucket");
                continue;
            }
            if (std::find(chain.begin(), chain.end(), entry) != chain.end()) {
                continue;
            }
            a2fo_odf_add_file_to_hash(
                at(g_fleet_ops, kAddFileToHashTableRva),
                file_system, entry, index);
            ++added;
        }
    }
    log_line("Hash ready: " + std::to_string(added) + " entries added");
    return added;
}

std::size_t build_recursive_odf_winners(
    void* file_system, const std::vector<void*>& custom_directories,
    const std::map<void*, std::uint32_t>& directory_precedence) {
    const std::uint32_t bucket_count =
        field<std::uint32_t>(file_system, 0x08);
    void** buckets = field<void**>(file_system, 0x0c);
    if (bucket_count == 0 || bucket_count > 0x100000 || !buckets ||
        !readable_range(buckets,
                        static_cast<std::size_t>(bucket_count) * sizeof(void*))) {
        log_line("Winner selection: invalid Fleet Operations hash table");
        return 0;
    }

    std::map<void*, CustomEntryInfo> custom_entries;
    std::map<std::string, std::vector<void*>> custom_groups;
    std::size_t logged_entries = 0;
    for (void* directory : custom_directories) {
        auto* entries = field<DelphiList*>(directory, 0x0c);
        const auto precedence = directory_precedence.find(directory);
        const std::uint32_t overlay =
            precedence == directory_precedence.end()
                ? A2FO_ODF_OVERLAY_NORMAL
                : precedence->second;
        for (void* entry : list_items(entries)) {
            const std::string basename = lower_ascii(
                delphi_string(entry, kFileEntryBasenameOffset));
            if (!has_odf_extension(basename)) continue;
            const std::string directory_path = delphi_string(directory, 4);
            custom_entries.emplace(
                entry, CustomEntryInfo{directory_path, overlay});
            custom_groups[basename].push_back(entry);
            if (logged_entries < kDetailedIndexLogLimit) {
                log_line("Entry: " + basename + " from " + directory_path +
                         " (primary=" +
                         std::to_string(field<std::uint8_t>(
                             entry, kFileEntryPrimaryRootOffset)) +
                         ", packed=" +
                         std::to_string(field<std::uint8_t>(
                             entry, kFileEntryPackedOffset)) +
                         ", overridden=" +
                         std::to_string(field<std::uint8_t>(
                             entry, kFileEntryOverriddenOffset)) + ")");
                ++logged_entries;
            }
        }
    }
    log_line("Precedence: " + std::to_string(custom_entries.size()) +
             " custom entries in hash table");
    if (custom_entries.size() > logged_entries) {
        log_line("Entry diagnostics: " +
                 std::to_string(custom_entries.size() - logged_entries) +
                 " additional entries suppressed");
    }

    std::map<std::string, std::string> aliases;
    std::map<std::string, void*> winners;
    std::size_t logged_winners = 0;
    for (const auto& group : custom_groups) {
        void* basename_value = field<void*>(
            group.second.front(), kFileEntryBasenameOffset);
        const std::uint32_t hash = a2fo_odf_hash_string(
            at(g_fleet_ops, kHashStringRva), basename_value);
        const std::uint32_t bucket = hash & (bucket_count - 1);
        std::vector<void*> chain;
        if (!collect_hash_bucket(file_system, bucket, chain)) {
            log_line("Alias skipped malformed bucket: " + group.first);
            continue;
        }

        if (logged_winners < kDetailedIndexLogLimit) {
            std::string contents;
            for (void* candidate : chain) {
                if (!contents.empty()) contents += ", ";
                contents += lower_ascii(delphi_string(
                    candidate, kFileEntryBasenameOffset));
                if (custom_entries.find(candidate) != custom_entries.end()) {
                    contents += " [recursive]";
                }
            }
            log_line("Hash bucket " + std::to_string(bucket) + ": " +
                     contents);
        }

        void* winner = nullptr;
        for (void* candidate : chain) {
            const std::string basename = lower_ascii(
                delphi_string(candidate, kFileEntryBasenameOffset));
            if (basename != group.first) continue;

            // RenewOverrides can mark an inherited parent's recursive loose
            // entry as overridden merely because that parent also contains a
            // packed copy. That flag is therefore not sufficient once child
            // mods add more filesystem roots. Calculate the winner from the
            // explicit mod priority, primary-root, and loose-before-packed
            // rules below. A real active-mod entry still wins by mod priority.
            if (!winner ||
                file_entry_precedes(candidate, winner, custom_entries)) {
                winner = candidate;
                continue;
            }
            if (!file_entry_precedes(winner, candidate, custom_entries) &&
                custom_entries.find(candidate) != custom_entries.end() &&
                custom_entries.find(winner) == custom_entries.end()) {
                winner = candidate;
            }
        }

        const auto selected = custom_entries.find(winner);
        if (selected != custom_entries.end()) {
            const std::string basename =
                delphi_string(winner, kFileEntryBasenameOffset);
            if (!basename.empty()) {
                const std::string target = join_path(
                    selected->second.directory_path, basename);
                aliases[group.first] = target;
                winners[group.first] = winner;
                const bool corrected_override =
                    field<std::uint8_t>(winner,
                                        kFileEntryOverriddenOffset) != 0;
                if (logged_winners < kDetailedIndexLogLimit) {
                    log_line("Winner: " + group.first + " -> " + target +
                             (corrected_override
                                  ? " (recursive precedence corrected override flag)"
                                  : ""));
                    ++logged_winners;
                }
            }
        }
    }
    if (winners.size() > logged_winners) {
        log_line("Winner diagnostics: " +
                 std::to_string(winners.size() - logged_winners) +
                 " additional winners suppressed");
    }

    std::size_t winner_count = 0;
    {
        StateLockGuard lock;
        g_recursive_odf_aliases.swap(aliases);
        g_recursive_odf_winners.swap(winners);
        g_logged_recursive_odf_lookups.clear();
        g_project_ids_by_entry.clear();
        g_project_id_items = nullptr;
        g_project_id_scanned_count = 0;
        g_project_id_capacity = 0;
        g_project_id_capacity_warning_logged = false;
        winner_count = g_recursive_odf_winners.size();
    }
    log_line("Precedence ready: " + std::to_string(winner_count) +
             " basename winners published");
    log_line("Project IDs: recursive winners will be registered lazily");
    return custom_entries.size();
}

bool register_recursive_odfs(void* file_system) {
    const DWORD started_at = GetTickCount();
    if (!readable_range(file_system, 0x24)) {
        log_line("Fleet Operations filesystem is not ready; deferring");
        return false;
    }
    auto* directory_list = *reinterpret_cast<DelphiList**>(
        static_cast<std::uint8_t*>(file_system) + 4);
    auto* root_list = *reinterpret_cast<DelphiList**>(
        static_cast<std::uint8_t*>(file_system) + 0x18);
    const std::uint32_t item_count_ready =
        field<std::uint32_t>(file_system, 0x20);
    if (!readable_list(directory_list,
                       static_cast<std::int32_t>(
                           kBuiltInVirtualDirectoryCount),
                       1000000) ||
        !readable_list(root_list, 1, 1000000) || item_count_ready == 0) {
        log_line("Fleet Operations filesystem is not ready; deferring");
        return false;
    }

    const std::string data_dir =
        g_api && g_api->root_directory ? g_api->root_directory() : "";
    const std::vector<RootInfo> roots =
        collect_roots(file_system, data_dir);
    log_line("Active filesystem roots: " + std::to_string(roots.size()));
    for (const RootInfo& root : roots) {
        log_line("  root: " + root.runtime_path + " -> " +
                 root.absolute_path);
    }

    std::map<std::string, OdfDirectoryCandidate> candidates;
    const std::vector<OdfDirectoryCandidate> overlays =
        registered_odf_overlays();
    for (const OdfDirectoryCandidate& overlay : overlays) {
        log_line("Active ODF overlay: " + overlay.path +
                 " (precedence=" +
                 std::to_string(overlay.precedence) + ")");
    }
    for (const RootInfo& root : roots) {
        scan_loose_odfs(join_path(root.absolute_path, "odf"),
                        "odf", A2FO_ODF_OVERLAY_NORMAL, candidates);
        for (const OdfDirectoryCandidate& overlay : overlays) {
            scan_loose_odfs(join_path(root.absolute_path, overlay.path),
                            overlay.path, overlay.precedence, candidates);
        }
    }
    log_line("Discovered ODF directories: " +
             std::to_string(candidates.size()));
    std::vector<ArchiveInfo> archives = collect_archives(roots, candidates);
    log_line("Active ODF archives: " + std::to_string(archives.size()));
    for (const ArchiveInfo& archive : archives) {
        log_line("  archive: " + archive.runtime_path);
    }

    std::map<std::string, void*> existing;
    for (void* directory : list_items(directory_list)) {
        existing.emplace(
            lower_ascii(delphi_string(directory, 4)), directory);
    }
    std::vector<void*> indexed_directories;
    std::map<void*, std::uint32_t> directory_precedence;
    for (auto iterator = candidates.begin(); iterator != candidates.end();) {
        const auto native = existing.find(iterator->first);
        if (native != existing.end()) {
            if (iterator->second.precedence > A2FO_ODF_OVERLAY_NORMAL) {
                indexed_directories.push_back(native->second);
                directory_precedence.emplace(
                    native->second, iterator->second.precedence);
                log_line("Using existing overlay directory: " +
                         iterator->second.path + " (precedence=" +
                         std::to_string(iterator->second.precedence) + ")");
            }
            iterator = candidates.erase(iterator);
        } else {
            ++iterator;
        }
    }

    const std::size_t available = kMaximumVirtualDirectoryCount -
        std::min<std::size_t>(directory_list->count,
                              kMaximumVirtualDirectoryCount);
    if (candidates.size() > available) {
        log_line("Directory limit reached; ignoring " +
                 std::to_string(candidates.size() - available) +
                 " directories");
    }

    std::vector<void*> created_directories;
    created_directories.reserve(std::min(candidates.size(), available));
    std::size_t added = 0;
    for (const auto& candidate : candidates) {
        if (added >= available) break;
        void* directory = create_virtual_directory(
            candidate.second.path,
            static_cast<std::uint32_t>(directory_list->count));
        if (!directory) {
            log_line("Failed to create virtual directory: " +
                     candidate.second.path);
            continue;
        }
        a2fo_odf_tlist_add(at(g_fleet_ops, kTListAddRva),
                           directory_list, directory);
        created_directories.push_back(directory);
        indexed_directories.push_back(directory);
        directory_precedence.emplace(
            directory, candidate.second.precedence);
        ++added;
        log_line("Registered directory: " + candidate.second.path +
                 " (precedence=" +
                 std::to_string(candidate.second.precedence) + ")");
    }

    if (indexed_directories.empty()) {
        log_line("Index: no recursive or overlay directories found");
        return true;
    }

    auto* item_count = reinterpret_cast<std::uint32_t*>(
        static_cast<std::uint8_t*>(file_system) + 0x20);
    if (!created_directories.empty()) {
        TemporaryDirectoryList temporary(directory_list, created_directories);
        for (const RootInfo& root : roots) {
            a2fo_odf_add_items_from_disk(
                at(g_fleet_ops, kAddItemsFromDiskRva),
                file_system, root.object, root.mod_info, item_count,
                root.primary ? 1u : 0u);
        }
        for (const ArchiveInfo& archive : archives) {
            void* delphi_path = nullptr;
            a2fo_odf_lstr_from_pchar(at(g_fleet_ops, kLStrFromPCharRva),
                                     &delphi_path,
                                     archive.runtime_path.c_str());
            a2fo_odf_add_items_from_pack(
                at(g_fleet_ops, kAddItemsFromPackRva),
                file_system, delphi_path, archive.mod_info, item_count);
            a2fo_odf_lstr_clear(at(g_fleet_ops, kLStrClearRva), &delphi_path);
        }
    }
    for (void* directory : created_directories) {
        a2fo_odf_renew_overrides(at(g_fleet_ops, kRenewOverridesRva), directory);
    }
    add_custom_entries_to_hash(file_system, indexed_directories);
    const std::size_t indexed_entries =
        build_recursive_odf_winners(
            file_system, indexed_directories, directory_precedence);
    log_line("Index ready: " + std::to_string(created_directories.size()) +
             " created, " +
             std::to_string(indexed_directories.size()) +
             " indexed directories, " +
             std::to_string(indexed_entries) + " ODF entries in " +
             std::to_string(GetTickCount() - started_at) + " ms");
    return true;
}

bool register_recursive_odfs_once(void* file_system) {
    constexpr DWORD kRegistrationTimeoutMilliseconds = 30000;
    const DWORD current_thread = GetCurrentThreadId();
    DWORD waited = 0;
    for (;;) {
        const LONG previous =
            InterlockedCompareExchange(&g_recursive_odf_state, 1, 0);
        if (previous == 0) {
            g_recursive_odf_owner_thread = current_thread;
            break;
        }
        if (previous == 2) return true;
        if (g_recursive_odf_owner_thread == current_thread) return true;
        if (waited >= kRegistrationTimeoutMilliseconds) {
            log_line("Timed out waiting for registration");
            return false;
        }
        Sleep(1);
        ++waited;
    }

    bool ok = false;
    try {
        ok = register_recursive_odfs(file_system);
    } catch (...) {
        log_line("Registration aborted by an unexpected C++ exception");
    }
    g_recursive_odf_owner_thread = 0;
    InterlockedExchange(&g_recursive_odf_state, ok ? 2 : 0);
    return ok;
}

bool A2FO_CALL lookup_handler(void* file_system, void* delphi_name,
                              std::uint32_t flags, void** result,
                              void* user_data) noexcept {
    try {
        (void)flags;
        (void)user_data;
        if (!result) return false;
        *result = nullptr;
        if (InterlockedCompareExchange(&g_recursive_odf_state, 0, 0) != 2) {
            log_line("First ODF item lookup reached");
            register_recursive_odfs_once(file_system);
        }
        if (InterlockedCompareExchange(&g_recursive_odf_state, 0, 0) != 2) {
            return false;
        }

        const std::string requested_name = delphi_string_value(delphi_name);
        const std::string key =
            a2fo::recursive_odf_basename_key(requested_name);
        if (key.empty()) return false;

        void* winner = nullptr;
        std::string winner_path;
        bool first_log = false;
        bool project_id_failed = false;
        {
            StateLockGuard lock;
            const auto selected = g_recursive_odf_winners.find(key);
            if (selected != g_recursive_odf_winners.end()) {
                winner = selected->second;
                const auto path = g_recursive_odf_aliases.find(key);
                if (path != g_recursive_odf_aliases.end()) {
                    winner_path = path->second;
                }
                if (g_logged_recursive_odf_lookups.size() < kLookupLogLimit) {
                    first_log =
                        g_logged_recursive_odf_lookups.insert(key).second;
                }
                bool added = false;
                if (!ensure_fleetops_project_id(file_system, winner, added) &&
                    !g_project_id_capacity_warning_logged) {
                    g_project_id_capacity_warning_logged = true;
                    project_id_failed = true;
                }
            }
        }

        if (!winner) return false;
        if (first_log) {
            log_line("Lookup: " + requested_name + " -> " + winner_path);
        }
        if (project_id_failed) {
            log_line("Project ID registry is full or unavailable; recursive "
                     "file lookup remains enabled");
        }
        *result = winner;
        return true;
    } catch (...) {
        if (result) *result = nullptr;
        log_line("Lookup handler failed; using native Fleet Operations lookup");
        return false;
    }
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module ||
        !api->fleetops_module ||
        !api->register_fofs_item_lookup_handler) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    g_fleet_ops = static_cast<HMODULE>(api->fleetops_module());
    if (!g_armada || !validate_fleetops()) {
        g_api = nullptr;
        g_armada = nullptr;
        g_fleet_ops = nullptr;
        return false;
    }
    InitializeCriticalSection(&g_state_lock);
    g_state_lock_ready = true;
    if (!api->register_fofs_item_lookup_handler(
            kModuleName, &lookup_handler, nullptr)) {
        DeleteCriticalSection(&g_state_lock);
        g_state_lock_ready = false;
        g_api = nullptr;
        g_armada = nullptr;
        g_fleet_ops = nullptr;
        return false;
    }
    log_line("Native feature pack initialized");
    a2fo::initialize_queue_enhancements(api, g_armada, g_fleet_ops);
    a2fo::initialize_upgrade_pods(api, g_armada, g_fleet_ops);
    a2fo::initialize_bink_video_scaling(api, g_armada);
    return true;
}
