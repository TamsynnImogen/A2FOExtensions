/*
 * File: modules/A2FOFeaturePack/odf_recursive.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Recursive ODF discovery, overlay publication, and winner lookup.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_supported_fleetops.hpp"
#include "../../core/fpq_paths.hpp"
#include "../../core/odf_paths.hpp"
#include "../../rust/stage8_c_odf_ffi.h"
#include "../../rust/stage10_feature_completion_ffi.h"
#include "buildyard_pseudo_technology.hpp"
#include "queue_enhancement.hpp"
#include "upgrade_pods.hpp"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <optional>
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
constexpr std::uintptr_t kGetFileFromHashTableRva = 0x109c14;

constexpr std::size_t kBuiltInVirtualDirectoryCount = 28;
constexpr std::size_t kMaximumVirtualDirectoryCount = 255;
constexpr std::size_t kMaximumPathLength = 32767;
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
struct RecursiveOdfWinner {
    void* entry = nullptr;
    std::string path;
};

// Built once while g_recursive_odf_state == 1, then published by the
// InterlockedExchange to state 2. Readers never mutate this table afterwards,
// so misses on the global FOFS hook can stay completely lock-free.
std::unordered_map<std::string, RecursiveOdfWinner> g_recursive_odf_winners;
std::set<std::string> g_logged_recursive_odf_lookups;
std::unordered_map<void*, std::uint32_t> g_project_ids_by_entry;
void** g_project_id_items = nullptr;
std::uint32_t g_project_id_scanned_count = 0;
std::uint32_t g_project_id_capacity = 0;
bool g_project_id_capacity_warning_logged = false;
std::atomic<void*> g_live_file_system{nullptr};

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
    struct Context {
        const IMAGE_NT_HEADERS32* nt = nullptr;
        std::exception_ptr error{};
    } context{};
    const auto step = +[](void* opaque, std::uint32_t operation,
                          std::size_t) noexcept -> std::int64_t {
        auto& c = *static_cast<Context*>(opaque);
        try { switch (operation) {
        case 0: return g_fleet_ops != nullptr;
        case 1: log_line("FleetOpsHook.dll is not loaded"); return 0;
        case 2: return reinterpret_cast<const IMAGE_DOS_HEADER*>(
            g_fleet_ops)->e_magic == IMAGE_DOS_SIGNATURE;
        case 3: log_line("FleetOpsHook.dll has no DOS header"); return 0;
        case 4: {
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(
                g_fleet_ops);
            c.nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
                reinterpret_cast<const std::uint8_t*>(g_fleet_ops) +
                dos->e_lfanew);
            return c.nt->Signature == IMAGE_NT_SIGNATURE;
        }
        case 5: return c.nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386;
        case 6: return c.nt->FileHeader.TimeDateStamp;
        case 7: return c.nt->OptionalHeader.SizeOfImage;
        case 8: return a2fo::supported_fleetops::approved_profile(g_fleet_ops);
        case 9: {
            char message[192]{};
            std::snprintf(message, sizeof(message),
                "FleetOpsHook.dll version mismatch "
                "(timestamp=%08lx, image=%08lx)",
                static_cast<unsigned long>(c.nt->FileHeader.TimeDateStamp),
                static_cast<unsigned long>(c.nt->OptionalHeader.SizeOfImage));
            log_line(message);
            return 0;
        }
        case 10: return std::memcmp(at(g_fleet_ops, kHashStringRva),
            kExpectedHashString, sizeof(kExpectedHashString)) == 0;
        case 11: return std::memcmp(at(g_fleet_ops, kAddFileToHashTableRva),
            kExpectedAddFileToHash, sizeof(kExpectedAddFileToHash)) == 0;
        case 12: return std::memcmp(at(g_fleet_ops, kAddItemsFromDiskRva),
            kExpectedAddDisk, sizeof(kExpectedAddDisk)) == 0;
        case 13: return std::memcmp(at(g_fleet_ops, kAddItemsFromPackRva),
            kExpectedAddPack, sizeof(kExpectedAddPack)) == 0;
        case 14: log_line("Fleet Operations filesystem scanner signature mismatch");
            return 0;
        default: return 0;
        }} catch (...) { c.error = std::current_exception(); return INT64_MIN; }
    };
    const bool result = a2fo_rs_c_odf_validate(&context, step);
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

std::string lower_ascii(std::string value) {
    const auto lower=+[](std::uint8_t ch) noexcept ->std::uint8_t {
        return static_cast<std::uint8_t>(std::tolower(ch));
    };
    a2fo_rs_c_odf_lower(reinterpret_cast<std::uint8_t*>(value.data()),
                        value.size(),lower);
    return value;
}

void replace_slashes(std::string& value, char slash = '\\') {
    a2fo_rs_c_odf_slashes(reinterpret_cast<std::uint8_t*>(value.data()),
                          value.size(),static_cast<std::uint8_t>(slash));
}

void trim_trailing_slashes(std::string& value) {
    value.resize(a2fo_rs_c_odf_trim_slashes(
        reinterpret_cast<const std::uint8_t*>(value.data()),value.size()));
}

std::string join_path(const std::string& left, const std::string& right) {
    switch(a2fo_rs_c_odf_join(
        reinterpret_cast<const std::uint8_t*>(left.data()),left.size(),
        reinterpret_cast<const std::uint8_t*>(right.data()),right.size())) {
    case 0:return right;
    case 1:return left;
    case 2:return left+right;
    default:return left+"\\"+right;
    }
}

bool is_absolute_path(const std::string& path) {
    const auto classify=+[](std::uint8_t ch) noexcept ->bool {
        return std::isalpha(ch)!=0;
    };
    return a2fo_rs_c_odf_absolute(
        reinterpret_cast<const std::uint8_t*>(path.data()),path.size(),classify);
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
    if (!list || !a2fo_rs_c_odf_list_count(list->count) || !list->items) {
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
    if (!a2fo_rs_c_odf_list_bounds_count(
            list->count, minimum_count, maximum_count) ||
        !a2fo_rs_c_odf_list_capacity(list->capacity, list->count)) {
        return false;
    }
    return list->count == 0 ||
           readable_range(list->items,
                          static_cast<std::size_t>(list->count) * sizeof(void*));
}

bool has_odf_extension(const std::string& name) {
    const std::string lower = lower_ascii(name);
    return a2fo_rs_c_odf_extension(
        reinterpret_cast<const std::uint8_t*>(lower.data()),lower.size());
}

void scan_loose_odfs(const std::string& absolute_directory,
                     const std::string& relative_directory,
                     std::uint32_t precedence,
                     std::map<std::string, OdfDirectoryCandidate>& directories,
                     unsigned depth = 0) {
    struct Context {
        const std::string& absolute;
        const std::string& relative;
        std::uint32_t precedence;
        std::map<std::string,OdfDirectoryCandidate>& directories;
        unsigned depth;
        WIN32_FIND_DATAA data{};
        HANDLE search=INVALID_HANDLE_VALUE;
        std::string name{};
        std::string normalized{};
        std::string key{};
        std::exception_ptr error{};
    } context{absolute_directory,relative_directory,precedence,directories,depth};
    const auto next=+[](void* opaque,A2foStage8OdfScanEntry* entry) noexcept
        ->std::int32_t {
        auto& c=*static_cast<Context*>(opaque);
        try {
            c.name=c.data.cFileName;
            entry->name=reinterpret_cast<const std::uint8_t*>(c.name.data());
            entry->size=c.name.size();
            entry->attributes=c.data.dwFileAttributes;
            return 1;
        }catch(...){c.error=std::current_exception();return -1;}
    };
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation) {
        case 0:c.search=FindFirstFileA(join_path(c.absolute,"*").c_str(),&c.data);
            return c.search!=INVALID_HANDLE_VALUE;
        case 2:return has_odf_extension(c.name);
        case 3:scan_loose_odfs(join_path(c.absolute,c.name),
            join_path(c.relative,c.name),c.precedence,c.directories,c.depth+1);
            return 0;
        case 4:return FindNextFileA(c.search,&c.data);
        case 5:FindClose(c.search);return 0;
        case 6:{c.normalized=c.relative;
            replace_slashes(c.normalized);
            c.key=lower_ascii(c.normalized);
            const auto found=c.directories.find(c.key);
            return found==c.directories.end()?-1:
                static_cast<std::int64_t>(found->second.precedence);}
        case 7:c.directories[c.key]=OdfDirectoryCandidate{
            c.normalized,c.precedence};return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_odf_scan(&context,depth,precedence,FILE_ATTRIBUTE_DIRECTORY,
                       FILE_ATTRIBUTE_REPARSE_POINT,next,step);
    if(context.error)std::rethrow_exception(context.error);
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
    const std::uint32_t count=g_api->odf_overlay_directory_count();
    struct Context {
        std::vector<OdfDirectoryCandidate>* result;
        char path[256]{};
        std::uint32_t precedence=A2FO_ODF_OVERLAY_NORMAL;
        std::exception_ptr error{};
    } context{&result};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t index) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation) {
        case 0:log_line("ODF overlay registry exceeded its supported limit");return 0;
        case 1:c.result->reserve(index);return 0;
        case 2:std::memset(c.path,0,sizeof(c.path));
            c.precedence=A2FO_ODF_OVERLAY_NORMAL;
            return g_api->get_odf_overlay_directory(
                static_cast<std::uint32_t>(index),c.path,sizeof(c.path),
                &c.precedence);
        case 3:return *c.path!=0;
        case 4:return c.precedence;
        case 5:log_line("Ignored invalid ODF overlay registry entry "+
            std::to_string(index));return 0;
        case 6:c.result->push_back(OdfDirectoryCandidate{
            c.path,c.precedence});return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_odf_overlays(&context,count,A2FO_ODF_OVERLAY_OVERRIDE,step);
    if(context.error)std::rethrow_exception(context.error);
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
        got != sizeof(header)) {
        CloseHandle(file);
        return false;
    }
    const std::uint64_t metadata_size=a2fo_rs_c_odf_fpq_size(
        header,sizeof(header));
    if (metadata_size == 0) {
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
    std::unordered_map<void*,void*> result;
    auto* directories=*reinterpret_cast<DelphiList**>(
        static_cast<std::uint8_t*>(file_system)+4);
    const auto directory_items=list_items(directories);
    struct Context {
        const std::vector<void*>& directories;
        std::unordered_map<void*,void*>& result;
        std::optional<std::vector<void*>> entries{};
        void* root=nullptr;
        void* mod_info=nullptr;
        std::exception_ptr error{};
    } context{directory_items,result};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t index) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation) {
        case 0:{auto* entries=*reinterpret_cast<DelphiList**>(
            static_cast<std::uint8_t*>(c.directories[index])+0x0c);
            c.entries.emplace(list_items(entries));
            return static_cast<std::int64_t>(c.entries->size());}
        case 1:{void* entry=(*c.entries)[index];
            c.root=*reinterpret_cast<void**>(static_cast<std::uint8_t*>(entry)+0x24);
            c.mod_info=*reinterpret_cast<void**>(
                static_cast<std::uint8_t*>(entry)+0x1c);
            return c.root!=nullptr;}
        case 2:return c.result.find(c.root)!=c.result.end();
        case 3:c.result.emplace(c.root,c.mod_info);return 0;
        case 4:c.entries.reset();return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_odf_root_mods(&context,directory_items.size(),step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

std::vector<RootInfo> collect_roots(void* file_system,
                                    const std::string& data_dir) {
    std::vector<RootInfo> result;
    const auto mod_info=map_roots_to_mod_info(file_system);
    auto* roots=*reinterpret_cast<DelphiList**>(
        static_cast<std::uint8_t*>(file_system)+0x18);
    const auto root_objects=list_items(roots);
    struct Context {
        const std::vector<void*>& objects;
        const std::unordered_map<void*,void*>& mod_info;
        const std::string& data_dir;
        std::vector<RootInfo>& result;
        RootInfo root{};
        std::string candidate{};
        std::exception_ptr error{};
    } context{root_objects,mod_info,data_dir,result};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t index) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation) {
        case 0:{c.root=RootInfo{};
            c.root.object=c.objects[index];
            const auto found=c.mod_info.find(c.root.object);
            c.root.mod_info=found==c.mod_info.end()?nullptr:found->second;
            c.root.runtime_path=delphi_string(c.root.object,4);
            replace_slashes(c.root.runtime_path);
            return 0;}
        case 1:return is_absolute_path(c.root.runtime_path);
        case 2:c.candidate=c.root.runtime_path;return 0;
        case 3:c.candidate=join_path(c.data_dir,c.root.runtime_path);return 0;
        case 4:c.root.absolute_path=full_path(c.candidate);
            c.root.primary=index!=0;
            c.result.push_back(std::move(c.root));return 0;
        case 5:return !c.result[index].mod_info;
        case 6:c.result[index].mod_info=c.result[index^1u].mod_info;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_odf_collect_roots(&context,root_objects.size(),step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

std::vector<ArchiveInfo> collect_archives(
    const std::vector<RootInfo>& roots,
    std::map<std::string,OdfDirectoryCandidate>& directories) {
    std::set<std::string> seen;
    std::vector<ArchiveInfo> result;
    struct Context {
        const std::vector<RootInfo>& roots;
        std::map<std::string,OdfDirectoryCandidate>& directories;
        std::set<std::string>& seen;
        std::vector<ArchiveInfo>& result;
        ArchiveInfo archive{};
        std::vector<std::uint8_t> metadata{};
        std::optional<a2fo::FpqPathResult> parsed{};
        std::exception_ptr error{};
    } context{roots,directories,seen,result};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t index) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation) {
        case 0:{const RootInfo& root=c.roots[index];
            c.archive=ArchiveInfo{};
            c.archive.mod_info=root.mod_info;
            c.archive.runtime_path=join_path(root.runtime_path,"odf.fpq");
            c.archive.absolute_path=full_path(
                join_path(root.absolute_path,"odf.fpq"));
            return GetFileAttributesA(c.archive.absolute_path.c_str())!=
                INVALID_FILE_ATTRIBUTES;}
        case 1:return c.seen.insert(lower_ascii(c.archive.absolute_path)).second;
        case 2:if(!read_fpq_metadata(c.archive.absolute_path,c.metadata)){
            log_line("Could not read FPQ metadata: "+c.archive.absolute_path);
            return 0;
        }return 1;
        case 3:c.parsed.emplace(a2fo::parse_fpq_odf_directories(c.metadata));
            if(!c.parsed->ok){
                log_line("Could not parse FPQ metadata: "+c.archive.absolute_path+
                    " ("+c.parsed->error+")");
                return 0;
            }return 1;
        case 4:return static_cast<std::int64_t>(c.parsed->odf_directories.size());
        case 5:{const std::string& path=c.parsed->odf_directories[index];
            c.directories.emplace(lower_ascii(path),
                OdfDirectoryCandidate{path,A2FO_ODF_OVERLAY_NORMAL});
            return 0;}
        case 6:c.result.push_back(std::move(c.archive));return 0;
        case 7:c.parsed.reset();
            std::vector<std::uint8_t>().swap(c.metadata);
            c.archive=ArchiveInfo{};return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    a2fo_rs_c_odf_collect_archives(&context,roots.size(),step);
    if(context.error)std::rethrow_exception(context.error);
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
    struct Context {
        void* file_system;
        std::uint32_t count=0;
        void** items=nullptr;
        std::uint32_t capacity=0;
        std::exception_ptr error{};
    } context{file_system};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t index) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation) {
        case 0:return readable_range(c.file_system,0x18);
        case 1:c.count=field<std::uint32_t>(c.file_system,0x10);
            c.items=field<void**>(c.file_system,0x14);return c.count;
        case 2:return c.items!=nullptr;
        case 3:return reinterpret_cast<std::uintptr_t>(c.items)>=
            sizeof(std::int32_t);
        case 4:return readable_range(c.items-1,sizeof(std::int32_t));
        case 5:{const std::int32_t signed_capacity=
            reinterpret_cast<const std::int32_t*>(c.items)[-1];
            c.capacity=static_cast<std::uint32_t>(signed_capacity);
            return signed_capacity;}
        case 6:return readable_range(c.items,
            static_cast<std::size_t>(c.count)*sizeof(void*));
        case 7:return c.items!=g_project_id_items;
        case 8:return c.count<g_project_id_scanned_count;
        case 9:g_project_ids_by_entry.clear();
            g_project_id_items=c.items;g_project_id_scanned_count=0;return 0;
        case 10:return g_project_id_scanned_count;
        case 11:return c.items[index]!=nullptr;
        case 12:g_project_ids_by_entry.emplace(c.items[index],
            static_cast<std::uint32_t>(index)+1);return 0;
        case 13:g_project_id_scanned_count=c.count;
            g_project_id_capacity=c.capacity;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_odf_refresh_ids(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

bool ensure_fleetops_project_id(void* file_system,void* entry,bool& added) {
    struct Context {
        void* file_system;
        void* entry;
        bool& added;
        std::uint32_t* project_id=nullptr;
        std::uint32_t existing=0;
        std::uint32_t count=0;
        std::exception_ptr error{};
    } context{file_system,entry,added};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation) {
        case 0:c.added=false;return 0;
        case 1:return c.entry!=nullptr;
        case 2:return writable_range(c.entry,
            kFileEntryProjectIdOffset+sizeof(std::uint32_t));
        case 3:return refresh_project_id_registry(c.file_system);
        case 4:c.project_id=&field<std::uint32_t>(
            c.entry,kFileEntryProjectIdOffset);return *c.project_id;
        case 5:return g_project_id_scanned_count;
        case 6:return g_project_id_items[*c.project_id-1]==c.entry;
        case 7:{const auto found=g_project_ids_by_entry.find(c.entry);
            if(found==g_project_ids_by_entry.end())return 0;
            c.existing=found->second;return 1;}
        case 8:*c.project_id=c.existing;return 0;
        case 9:c.count=g_project_id_scanned_count;return c.count;
        case 10:return g_project_id_capacity;
        case 11:return writable_range(g_project_id_items+c.count,sizeof(void*));
        case 12:return writable_range(
            static_cast<std::uint8_t*>(c.file_system)+0x10,
            sizeof(std::uint32_t));
        case 13:g_project_id_items[c.count]=c.entry;return 0;
        case 14:*c.project_id=c.count+1;return 0;
        case 15:field<std::uint32_t>(c.file_system,0x10)=c.count+1;return 0;
        case 16:g_project_ids_by_entry.emplace(c.entry,*c.project_id);return 0;
        case 17:g_project_id_scanned_count=c.count+1;c.added=true;return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_odf_ensure_id(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
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
    struct Context {
        void* left;
        void* right;
        const std::map<void*, CustomEntryInfo>& custom_entries;
        void* left_mod=nullptr;
        void* right_mod=nullptr;
        std::exception_ptr error{};
    } context{left,right,custom_entries};
    const auto step=+[](void* opaque,std::uint32_t operation,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try {switch(operation) {
        case 0:c.left_mod=field<void*>(c.left,kFileEntryModInfoOffset);
            return reinterpret_cast<std::uintptr_t>(c.left_mod);
        case 1:c.right_mod=field<void*>(c.right,kFileEntryModInfoOffset);
            return reinterpret_cast<std::uintptr_t>(c.right_mod);
        case 2:return field<std::int32_t>(c.left_mod,kModInfoPriorityOffset);
        case 3:return field<std::int32_t>(c.right_mod,kModInfoPriorityOffset);
        case 4:return overlay_precedence(c.left,c.custom_entries);
        case 5:return overlay_precedence(c.right,c.custom_entries);
        case 6:return field<std::uint8_t>(c.left,kFileEntryPrimaryRootOffset);
        case 7:return field<std::uint8_t>(c.right,kFileEntryPrimaryRootOffset);
        case 8:return field<std::uint8_t>(c.left,kFileEntryPackedOffset);
        case 9:return field<std::uint8_t>(c.right,kFileEntryPackedOffset);
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_odf_precedes(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

bool collect_hash_bucket(void* file_system, std::uint32_t index,
                         std::vector<void*>& entries) {
    struct Context {
        void* file_system;
        std::uint32_t index;
        std::vector<void*>& entries;
        std::uint32_t bucket_count = 0;
        std::uint32_t item_count = 0;
        void** buckets = nullptr;
        void* entry = nullptr;
        std::set<void*> visited{};
        std::exception_ptr error{};
    } context{file_system, index, entries};
    const auto step = +[](void* opaque, std::uint32_t operation,
                          std::size_t) noexcept -> std::int64_t {
        auto& c = *static_cast<Context*>(opaque);
        try { switch (operation) {
        case 0: c.entries.clear(); return 0;
        case 1: c.bucket_count = field<std::uint32_t>(c.file_system, 0x08);
            return c.bucket_count;
        case 2: c.buckets = field<void**>(c.file_system, 0x0c);
            return c.buckets != nullptr;
        case 3: c.item_count = field<std::uint32_t>(c.file_system, 0x20);
            return c.item_count;
        case 4: return readable_range(c.buckets,
            static_cast<std::size_t>(c.bucket_count) * sizeof(void*));
        case 5: c.entry = c.buckets[c.index];
            return reinterpret_cast<std::uintptr_t>(c.entry);
        case 6: return readable_range(c.entry, 0x2c);
        case 7: return c.visited.insert(c.entry).second;
        case 8: c.entries.clear(); return 0;
        case 9: c.entries.push_back(c.entry); return 0;
        case 10: c.entry = field<void*>(c.entry, kFileEntryNextOffset);
            return reinterpret_cast<std::uintptr_t>(c.entry);
        default: return 0;
        }} catch (...) { c.error = std::current_exception(); return INT64_MIN; }
    };
    const bool result = a2fo_rs_c_odf_hash_bucket(&context, index, step);
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

std::size_t add_custom_entries_to_hash(
    void* file_system, const std::vector<void*>& custom_directories) {
    const std::uint32_t bucket_count = field<std::uint32_t>(file_system, 0x08);
    void** buckets = field<void**>(file_system, 0x0c);
    if (!a2fo_rs_c_odf_bucket_admit(bucket_count, buckets != nullptr) ||
        !readable_range(buckets,
                        static_cast<std::size_t>(bucket_count) * sizeof(void*))) {
        log_line("Hash insertion: invalid Fleet Operations hash table");
        return 0;
    }
    struct Context {
        void* file_system;
        const std::vector<void*>& directories;
        std::uint32_t bucket_count;
        std::vector<void*> entries{};
        std::vector<void*> chain{};
        void* entry = nullptr;
        std::uint32_t index = 0;
        std::size_t added = 0;
        std::exception_ptr error{};
    } context{file_system, custom_directories, bucket_count};
    const auto step = +[](void* opaque, std::uint32_t operation,
                          std::size_t arg) noexcept -> std::int64_t {
        auto& c = *static_cast<Context*>(opaque);
        try { switch (operation) {
        case 0: c.entries = list_items(field<DelphiList*>(c.directories[arg], 0x0c));
            return c.entries.size();
        case 1: c.entry = c.entries[arg];
            { void* basename = field<void*>(c.entry, kFileEntryBasenameOffset);
              return basename && has_odf_extension(delphi_string_value(basename)); }
        case 2: {
            void* basename = field<void*>(c.entry, kFileEntryBasenameOffset);
            const std::uint32_t hash = a2fo_odf_hash_string(
                at(g_fleet_ops, kHashStringRva), basename);
            c.index = hash & (c.bucket_count - 1);
            return collect_hash_bucket(c.file_system, c.index, c.chain);
        }
        case 3: log_line("Hash insertion: malformed target bucket"); return 0;
        case 4: {
            const auto scan = +[](void* opaque, std::uint32_t scan_operation,
                                  std::size_t index) noexcept -> std::int64_t {
                auto& c = *static_cast<Context*>(opaque);
                try { switch (scan_operation) {
                case 0: return static_cast<std::int64_t>(c.chain.size());
                case 1: return c.chain[index] == c.entry;
                default: return 0;
                }} catch (...) {
                    c.error = std::current_exception();
                    return INT64_MIN;
                }
            };
            const bool found = a2fo_rs_stage10_feature_chain_contains(&c, scan);
            if (c.error) return INT64_MIN;
            return found;
        }
        case 5: a2fo_odf_add_file_to_hash(
                at(g_fleet_ops, kAddFileToHashTableRva), c.file_system,
                c.entry, c.index);
            ++c.added;
            return 0;
        default: return 0;
        }} catch (...) { c.error = std::current_exception(); return INT64_MIN; }
    };
    a2fo_rs_c_odf_hash_entries(&context, custom_directories.size(), step);
    if (context.error) std::rethrow_exception(context.error);
    log_line("Hash ready: " + std::to_string(context.added) + " entries added");
    return context.added;
}

std::size_t build_recursive_odf_winners(
    void* file_system, const std::vector<void*>& custom_directories,
    const std::map<void*, std::uint32_t>& directory_precedence) {
    const std::uint32_t bucket_count = field<std::uint32_t>(file_system, 0x08);
    void** buckets = field<void**>(file_system, 0x0c);
    if (!a2fo_rs_c_odf_bucket_admit(bucket_count, buckets != nullptr) ||
        !readable_range(buckets,
                        static_cast<std::size_t>(bucket_count) * sizeof(void*))) {
        log_line("Winner selection: invalid Fleet Operations hash table");
        return 0;
    }
    struct Context {
        void* file_system;
        const std::vector<void*>& directories;
        const std::map<void*, std::uint32_t>& precedence;
        std::uint32_t bucket_count;
        std::map<void*, CustomEntryInfo> custom_entries{};
        std::map<std::string, std::vector<void*>> custom_groups{};
        std::unordered_map<std::string, RecursiveOdfWinner> winners{};
        std::vector<void*> entries{};
        std::vector<void*> chain{};
        std::map<std::string, std::vector<void*>>::iterator group{};
        std::string basename{};
        void* directory = nullptr;
        void* entry = nullptr;
        void* candidate = nullptr;
        void* winner = nullptr;
        std::uint32_t overlay = A2FO_ODF_OVERLAY_NORMAL;
        std::size_t winner_count = 0;
        std::exception_ptr error{};
    } context{file_system, custom_directories, directory_precedence, bucket_count};
    const auto step = +[](void* opaque, std::uint32_t operation,
                          std::size_t arg) noexcept -> std::int64_t {
        auto& c = *static_cast<Context*>(opaque);
        try { switch (operation) {
        case 0: {
            c.directory = c.directories[arg];
            c.entries = list_items(field<DelphiList*>(c.directory, 0x0c));
            const auto found = c.precedence.find(c.directory);
            c.overlay = found == c.precedence.end()
                ? A2FO_ODF_OVERLAY_NORMAL : found->second;
            return c.entries.size();
        }
        case 1: c.entry = c.entries[arg];
            c.basename = lower_ascii(
                delphi_string(c.entry, kFileEntryBasenameOffset));
            return has_odf_extension(c.basename);
        case 2: {
            const std::string path = delphi_string(c.directory, 4);
            c.custom_entries.emplace(c.entry, CustomEntryInfo{path, c.overlay});
            c.custom_groups[c.basename].push_back(c.entry);
            return 0;
        }
        case 3: log_line("Precedence: " + std::to_string(c.custom_entries.size()) +
                         " custom entries in hash table");
            c.winners.reserve(c.custom_groups.size()); return 0;
        case 4: c.group = c.custom_groups.begin(); return c.custom_groups.size();
        case 5: {
            if (arg) std::advance(c.group, 1);
            void* basename = field<void*>(c.group->second.front(),
                                          kFileEntryBasenameOffset);
            const std::uint32_t hash = a2fo_odf_hash_string(
                at(g_fleet_ops, kHashStringRva), basename);
            const std::uint32_t bucket = hash & (c.bucket_count - 1);
            c.winner = nullptr;
            return collect_hash_bucket(c.file_system, bucket, c.chain);
        }
        case 6: log_line("Alias skipped malformed bucket: " + c.group->first);
            return 0;
        case 7: return c.chain.size();
        case 8: c.candidate = c.chain[arg];
            return lower_ascii(delphi_string(c.candidate,
                kFileEntryBasenameOffset)) == c.group->first;
        case 9: return file_entry_precedes(c.candidate, c.winner,
                                          c.custom_entries);
        case 10: c.winner = c.candidate; return 0;
        case 11: return file_entry_precedes(c.winner, c.candidate,
                                           c.custom_entries);
        case 12: return c.custom_entries.find(c.candidate) !=
                        c.custom_entries.end();
        case 13: return c.custom_entries.find(c.winner) !=
                        c.custom_entries.end();
        case 14: return c.custom_entries.find(c.winner) !=
                        c.custom_entries.end();
        case 15: return !delphi_string(c.winner,
                                      kFileEntryBasenameOffset).empty();
        case 16: {
            const auto selected = c.custom_entries.find(c.winner);
            const std::string basename = delphi_string(
                c.winner, kFileEntryBasenameOffset);
            const std::string target = join_path(
                selected->second.directory_path, basename);
            c.winners[c.group->first] = RecursiveOdfWinner{c.winner, target};
            return 0;
        }
        case 17: {
            StateLockGuard lock;
            g_recursive_odf_winners.swap(c.winners);
            g_logged_recursive_odf_lookups.clear();
            g_project_ids_by_entry.clear();
            g_project_id_items = nullptr;
            g_project_id_scanned_count = 0;
            g_project_id_capacity = 0;
            g_project_id_capacity_warning_logged = false;
            c.winner_count = g_recursive_odf_winners.size();
            return 0;
        }
        default: return 0;
        }} catch (...) { c.error = std::current_exception(); return INT64_MIN; }
    };
    a2fo_rs_c_odf_winners(&context, custom_directories.size(), step);
    if (context.error) std::rethrow_exception(context.error);
    log_line("Precedence ready: " + std::to_string(context.winner_count) +
             " basename winners published");
    log_line("Project IDs: recursive winners will be registered lazily");
    return context.custom_entries.size();
}

bool register_recursive_odfs(void* file_system) {
    struct Context {
        void* file_system;
        DWORD started_at = 0;
        DelphiList* directory_list = nullptr;
        DelphiList* root_list = nullptr;
        std::string data_dir{};
        std::vector<RootInfo> roots{};
        std::vector<OdfDirectoryCandidate> overlays{};
        std::map<std::string, OdfDirectoryCandidate> candidates{};
        std::vector<ArchiveInfo> archives{};
        std::vector<void*> existing_items{};
        std::map<std::string, void*> existing{};
        std::vector<void*> indexed_directories{};
        std::map<void*, std::uint32_t> directory_precedence{};
        std::vector<void*> created_directories{};
        std::map<std::string, OdfDirectoryCandidate>::iterator candidate{};
        std::size_t available = 0;
        std::size_t added = 0;
        std::size_t indexed_entries = 0;
        std::size_t ignored = 0;
        std::size_t scan_root = 0;
        std::exception_ptr error{};
    } context{file_system};
    const auto step = +[](void* opaque, std::uint32_t operation,
                          std::size_t arg) noexcept -> std::int64_t {
        auto& c = *static_cast<Context*>(opaque);
        try { switch (operation) {
        case 0: c.started_at = GetTickCount();
            if (readable_range(c.file_system, 0x24)) return 1;
            log_line("Fleet Operations filesystem is not ready; deferring");
            return 0;
        case 1: {
            c.directory_list = *reinterpret_cast<DelphiList**>(
                static_cast<std::uint8_t*>(c.file_system) + 4);
            c.root_list = *reinterpret_cast<DelphiList**>(
                static_cast<std::uint8_t*>(c.file_system) + 0x18);
            const std::uint32_t item_count = field<std::uint32_t>(
                c.file_system, 0x20);
            if (readable_list(c.directory_list,
                    static_cast<std::int32_t>(kBuiltInVirtualDirectoryCount),
                    1000000) && readable_list(c.root_list, 1, 1000000) &&
                item_count != 0) return 1;
            log_line("Fleet Operations filesystem is not ready; deferring");
            return 0;
        }
        case 2: c.data_dir = g_api && g_api->root_directory
                    ? g_api->root_directory() : "";
            c.roots = collect_roots(c.file_system, c.data_dir);
            log_line("Active filesystem roots: " +
                     std::to_string(c.roots.size()));
            return c.roots.size();
        case 3: {
            const RootInfo& root = c.roots[arg];
            log_line("  root: " + root.runtime_path + " -> " +
                     root.absolute_path);
            return 0;
        }
        case 4: c.overlays = registered_odf_overlays();
            return c.overlays.size();
        case 5: {
            const auto& overlay = c.overlays[arg];
            log_line("Active ODF overlay: " + overlay.path +
                     " (precedence=" + std::to_string(overlay.precedence) + ")");
            return 0;
        }
        case 6: c.scan_root = arg;
            scan_loose_odfs(join_path(c.roots[arg].absolute_path, "odf"),
                            "odf", A2FO_ODF_OVERLAY_NORMAL, c.candidates);
            return 0;
        case 7: {
            const auto& overlay = c.overlays[arg];
            scan_loose_odfs(join_path(c.roots[c.scan_root].absolute_path,
                                      overlay.path),
                            overlay.path, overlay.precedence, c.candidates);
            return 0;
        }
        case 8: log_line("Discovered ODF directories: " +
                         std::to_string(c.candidates.size()));
            c.archives = collect_archives(c.roots, c.candidates);
            log_line("Active ODF archives: " +
                     std::to_string(c.archives.size()));
            return c.archives.size();
        case 9: log_line("  archive: " + c.archives[arg].runtime_path);
            return 0;
        case 10: {
            c.existing_items = list_items(c.directory_list);
            c.candidate = c.candidates.begin();
            return c.existing_items.size();
        }
        case 11: {
            void* directory = c.existing_items[arg];
            c.existing.emplace(lower_ascii(delphi_string(directory, 4)),
                               directory);
            return 0;
        }
        case 12: return c.candidates.size();
        case 13: {
            const auto native = c.existing.find(c.candidate->first);
            if (native != c.existing.end()) {
                if (c.candidate->second.precedence >
                    A2FO_ODF_OVERLAY_NORMAL) {
                    c.indexed_directories.push_back(native->second);
                    c.directory_precedence.emplace(
                        native->second, c.candidate->second.precedence);
                    log_line("Using existing overlay directory: " +
                             c.candidate->second.path + " (precedence=" +
                             std::to_string(c.candidate->second.precedence) +
                             ")");
                }
                c.candidate = c.candidates.erase(c.candidate);
            } else { ++c.candidate; }
            return 0;
        }
        case 14: c.available = kMaximumVirtualDirectoryCount -
                std::min<std::size_t>(c.directory_list->count,
                                      kMaximumVirtualDirectoryCount);
            c.ignored = c.candidates.size() > c.available
                ? c.candidates.size() - c.available : 0;
            return c.available;
        case 15: return c.ignored;
        case 16: log_line("Directory limit reached; ignoring " +
                          std::to_string(c.ignored) + " directories");
            return 0;
        case 17: c.created_directories.reserve(
                std::min(c.candidates.size(), c.available));
            c.candidate = c.candidates.begin();
            return 0;
        case 18: return c.candidates.size();
        case 19: {
            const auto& entry = c.candidate->second;
            void* directory = create_virtual_directory(
                entry.path,
                static_cast<std::uint32_t>(c.directory_list->count));
            if (!directory) {
                log_line("Failed to create virtual directory: " + entry.path);
                ++c.candidate;
                return 0;
            }
            a2fo_odf_tlist_add(at(g_fleet_ops, kTListAddRva),
                               c.directory_list, directory);
            c.created_directories.push_back(directory);
            c.indexed_directories.push_back(directory);
            c.directory_precedence.emplace(directory, entry.precedence);
            ++c.added;
            log_line("Registered directory: " + entry.path +
                     " (precedence=" + std::to_string(entry.precedence) + ")");
            ++c.candidate;
            return 1;
        }
        case 20: return !c.indexed_directories.empty();
        case 21: log_line("Index: no recursive or overlay directories found");
            return 0;
        case 22: return !c.created_directories.empty();
        case 23: {
            auto* item_count = reinterpret_cast<std::uint32_t*>(
                static_cast<std::uint8_t*>(c.file_system) + 0x20);
            TemporaryDirectoryList temporary(c.directory_list,
                                             c.created_directories);
            struct ScanContext {
                Context& parent;
                std::uint32_t* item_count;
                std::exception_ptr error{};
            } scan{c, item_count};
            const auto scan_step = +[](void* value, std::uint32_t op,
                                       std::size_t index) noexcept -> std::int64_t {
                auto& state = *static_cast<ScanContext*>(value);
                auto& c = state.parent;
                try {
                    if (op == 0) {
                        const RootInfo& root = c.roots[index];
                        a2fo_odf_add_items_from_disk(
                            at(g_fleet_ops, kAddItemsFromDiskRva),
                            c.file_system, root.object, root.mod_info,
                            state.item_count, root.primary ? 1u : 0u);
                    } else {
                        const ArchiveInfo& archive = c.archives[index];
                        void* delphi_path = nullptr;
                        a2fo_odf_lstr_from_pchar(
                            at(g_fleet_ops, kLStrFromPCharRva),
                            &delphi_path, archive.runtime_path.c_str());
                        a2fo_odf_add_items_from_pack(
                            at(g_fleet_ops, kAddItemsFromPackRva),
                            c.file_system, delphi_path, archive.mod_info,
                            state.item_count);
                        a2fo_odf_lstr_clear(at(g_fleet_ops, kLStrClearRva),
                                             &delphi_path);
                    }
                    return 0;
                } catch (...) {
                    state.error = std::current_exception();
                    return INT64_MIN;
                }
            };
            a2fo_rs_c_odf_index_scan(&scan, c.roots.size(),
                                     c.archives.size(), scan_step);
            if (scan.error) std::rethrow_exception(scan.error);
            return 0;
        }
        case 24: return c.created_directories.size();
        case 25: a2fo_odf_renew_overrides(
                    at(g_fleet_ops, kRenewOverridesRva),
                    c.created_directories[arg]);
            return 0;
        case 26: add_custom_entries_to_hash(c.file_system,
                                            c.indexed_directories);
            return 0;
        case 27: c.indexed_entries = build_recursive_odf_winners(
                c.file_system, c.indexed_directories,
                c.directory_precedence);
            return 0;
        case 28: log_line("Index ready: " +
                          std::to_string(c.created_directories.size()) +
                          " created, " +
                          std::to_string(c.indexed_directories.size()) +
                          " indexed directories, " +
                          std::to_string(c.indexed_entries) +
                          " ODF entries in " +
                          std::to_string(GetTickCount() - c.started_at) +
                          " ms");
            return 0;
        default: return 0;
        }} catch (...) { c.error = std::current_exception(); return INT64_MIN; }
    };
    const bool result = a2fo_rs_c_odf_register(&context, step);
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

bool register_recursive_odfs_once(void* file_system) {
    struct Context {
        void* file_system;
        DWORD current_thread = 0;
        bool ok = false;
        std::exception_ptr error{};
    } context{file_system};
    const auto step = +[](void* opaque, std::uint32_t operation,
                          std::size_t) noexcept -> std::int64_t {
        auto& c = *static_cast<Context*>(opaque);
        try { switch (operation) {
        case 0: c.current_thread = GetCurrentThreadId();
            return c.current_thread;
        case 1: return InterlockedCompareExchange(
            &g_recursive_odf_state, 1, 0);
        case 2: g_recursive_odf_owner_thread = c.current_thread; return 0;
        case 3: return g_recursive_odf_owner_thread;
        case 4: log_line("Timed out waiting for registration"); return 0;
        case 5: Sleep(1); return 0;
        case 6: {
            c.ok = false;
            try { c.ok = register_recursive_odfs(c.file_system); }
            catch (...) {
                log_line("Registration aborted by an unexpected C++ exception");
            }
            return c.ok;
        }
        case 7: g_recursive_odf_owner_thread = 0;
            InterlockedExchange(&g_recursive_odf_state, c.ok ? 2 : 0);
            return 0;
        default: return 0;
        }} catch (...) { c.error = std::current_exception(); return INT64_MIN; }
    };
    const bool result = a2fo_rs_c_odf_register_once(&context, step);
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

bool A2FO_CALL lookup_handler(void* file_system, void* delphi_name,
                              std::uint32_t flags, void** result,
                              void* user_data) noexcept {
    try {
        (void)flags;
        (void)user_data;
        struct Context {
            void* file_system;
            void* delphi_name;
            void** result;
            std::string requested_name{};
            std::string key{};
            void* winner = nullptr;
            const std::string* winner_path = nullptr;
            bool first_log = false;
            bool project_id_failed = false;
            std::exception_ptr error{};
        } context{file_system, delphi_name, result};
        const auto step = +[](void* opaque, std::uint32_t operation,
                              std::size_t) noexcept -> std::int64_t {
            auto& c = *static_cast<Context*>(opaque);
            try { switch (operation) {
            case 0: return c.file_system != nullptr;
            case 1: g_live_file_system.store(c.file_system,
                                              std::memory_order_release);
                return 0;
            case 2: return c.result != nullptr;
            case 3: *c.result = nullptr; return 0;
            case 4: return InterlockedCompareExchange(
                &g_recursive_odf_state, 0, 0);
            case 5: log_line("First ODF item lookup reached"); return 0;
            case 6: register_recursive_odfs_once(c.file_system); return 0;
            case 7: c.requested_name = delphi_string_value(c.delphi_name);
                c.key = a2fo::recursive_odf_basename_key(c.requested_name);
                return !c.key.empty();
            case 8: {
                const auto selected = g_recursive_odf_winners.find(c.key);
                if (selected == g_recursive_odf_winners.end()) return 0;
                c.winner = selected->second.entry;
                c.winner_path = &selected->second.path;
                return 1;
            }
            case 9: {
                StateLockGuard lock;
                if (g_logged_recursive_odf_lookups.size() < kLookupLogLimit) {
                    c.first_log =
                        g_logged_recursive_odf_lookups.insert(c.key).second;
                }
                bool added = false;
                if (!ensure_fleetops_project_id(c.file_system,
                                                c.winner, added) &&
                    !g_project_id_capacity_warning_logged) {
                    g_project_id_capacity_warning_logged = true;
                    c.project_id_failed = true;
                }
                return 0;
            }
            case 10: return c.first_log;
            case 11: log_line("Lookup: " + c.requested_name + " -> " +
                              *c.winner_path); return 0;
            case 12: return c.project_id_failed;
            case 13: log_line("Project ID registry is full or unavailable; "
                              "recursive file lookup remains enabled");
                return 0;
            case 14: *c.result = c.winner; return 0;
            default: return 0;
            }} catch (...) { c.error = std::current_exception(); return INT64_MIN; }
        };
        const bool resolved = a2fo_rs_c_odf_lookup(&context, step);
        if (context.error) std::rethrow_exception(context.error);
        return resolved;
    } catch (...) {
        if (result) *result = nullptr;
        log_line("Lookup handler failed; using native Fleet Operations lookup");
        return false;
    }
}

}  // namespace

namespace a2fo {

std::uint32_t resolve_featurepack_odf_project_id(const char* raw_name) noexcept {
    try {
        struct Context {
            const char* raw_name;
            std::string requested{};
            std::string key{};
            std::string wanted{};
            void* file_system = nullptr;
            void* entry = nullptr;
            std::uint32_t bucket_count = 0;
            std::vector<void*> chain{};
            std::map<void*, CustomEntryInfo> no_custom_entries{};
            void* candidate = nullptr;
            void* best_active = nullptr;
            void* best_any = nullptr;
            std::size_t matching_entries = 0;
            std::uint32_t project_id = 0;
            bool added = false;
            std::exception_ptr error{};
        } context{raw_name};
        const auto step = +[](void* opaque, std::uint32_t operation,
                              std::size_t arg) noexcept -> std::int64_t {
            auto& c = *static_cast<Context*>(opaque);
            try { switch (operation) {
            case 0: return c.raw_name && *c.raw_name &&
                           g_state_lock_ready && g_fleet_ops;
            case 1: {
                c.requested = c.raw_name;
                A2foStage8OdfSpan span{};
                if (!a2fo_rs_c_odf_trim_name(
                        reinterpret_cast<const std::uint8_t*>(
                            c.requested.data()), c.requested.size(),
                        &span)) return 0;
                c.requested = c.requested.substr(span.first, span.length);
                replace_slashes(c.requested, '\\');
                const std::size_t basename =
                    a2fo_rs_c_odf_basename_start(
                        reinterpret_cast<const std::uint8_t*>(
                            c.requested.data()), c.requested.size());
                if (basename != 0) c.requested.erase(0, basename);
                if (c.requested.size() < 4 ||
                    _stricmp(c.requested.c_str() + c.requested.size() - 4,
                             ".odf") != 0) c.requested += ".odf";
                return 1;
            }
            case 2: c.file_system = g_live_file_system.load(
                        std::memory_order_acquire);
                if (!c.file_system || !readable_range(c.file_system, 0x24))
                    return 0;
                c.key = a2fo::recursive_odf_basename_key(c.requested);
                return 1;
            case 3: return InterlockedCompareExchange(
                &g_recursive_odf_state, 0, 0) == 2;
            case 4: {
                const auto winner = g_recursive_odf_winners.find(c.key);
                if (winner != g_recursive_odf_winners.end())
                    c.entry = winner->second.entry;
                return 0;
            }
            case 5: return c.entry != nullptr;
            case 6: {
                c.bucket_count = field<std::uint32_t>(c.file_system, 0x08);
                void** buckets = field<void**>(c.file_system, 0x0c);
                return c.bucket_count != 0 && c.bucket_count <= 0x100000 &&
                       buckets && readable_range(
                           buckets, static_cast<std::size_t>(c.bucket_count) *
                                        sizeof(void*));
            }
            case 7: {
                void* delphi_name = nullptr;
                a2fo_odf_lstr_from_pchar(
                    at(g_fleet_ops, kLStrFromPCharRva), &delphi_name,
                    c.requested.c_str());
                const std::uint32_t hash = a2fo_odf_hash_string(
                    at(g_fleet_ops, kHashStringRva), delphi_name);
                a2fo_odf_lstr_clear(at(g_fleet_ops, kLStrClearRva),
                                    &delphi_name);
                if (!collect_hash_bucket(
                        c.file_system, hash & (c.bucket_count - 1),
                        c.chain)) return 0;
                c.wanted = lower_ascii(c.requested);
                return 1;
            }
            case 8: return c.chain.size();
            case 9: {
                c.candidate = c.chain[arg];
                if (!c.candidate || !readable_range(c.candidate, 0x2c))
                    return 0;
                const std::string basename = lower_ascii(
                    delphi_string(c.candidate, kFileEntryBasenameOffset));
                if (basename != c.wanted) return 0;
                ++c.matching_entries;
                return 1;
            }
            case 10: return c.best_any != nullptr;
            case 11: return file_entry_precedes(
                c.candidate, c.best_any, c.no_custom_entries);
            case 12: c.best_any = c.candidate; return 0;
            case 13: return field<std::uint8_t>(
                c.candidate, kFileEntryOverriddenOffset) != 0;
            case 14: return c.best_active != nullptr;
            case 15: return file_entry_precedes(
                c.candidate, c.best_active, c.no_custom_entries);
            case 16: c.best_active = c.candidate; return 0;
            case 17: c.entry = c.best_active ? c.best_active : c.best_any;
                if (c.entry) {
                    log_line("BuildYard direct FOFS hash lookup resolved " +
                             c.requested + " (" +
                             std::to_string(c.matching_entries) +
                             " matching entr" +
                             (c.matching_entries == 1 ? "y)" : "ies)"));
                }
                return 0;
            case 18: {
                void* delphi_name = nullptr;
                a2fo_odf_lstr_from_pchar(
                    at(g_fleet_ops, kLStrFromPCharRva), &delphi_name,
                    c.requested.c_str());
                using GetFileFromHashFunction =
                    void* (__attribute__((regparm(3))) *)(
                        void*, void*, std::uintptr_t);
                const auto get_file = reinterpret_cast<GetFileFromHashFunction>(
                    at(g_fleet_ops, kGetFileFromHashTableRva));
                c.entry = get_file(c.file_system, delphi_name, 0);
                if (!c.entry) c.entry = get_file(c.file_system,
                                                delphi_name, 1);
                a2fo_odf_lstr_clear(at(g_fleet_ops, kLStrClearRva),
                                    &delphi_name);
                return 0;
            }
            case 19: log_line(
                "BuildYard technology entry '" + c.requested +
                "' is not present in the live Fleet Operations ODF registry; "
                "a backing ODF is required by the current native-project-ID "
                "technology path");
                return 0;
            case 20: {
                StateLockGuard lock;
                if (!ensure_fleetops_project_id(
                        c.file_system, c.entry, c.added)) return 0;
                c.project_id = field<std::uint32_t>(
                    c.entry, kFileEntryProjectIdOffset);
                return 1;
            }
            case 21: return c.added && c.project_id != 0;
            case 22: log_line("Assigned late project ID " +
                              std::to_string(c.project_id) + " to " +
                              c.requested +
                              " for BuildYard technology entry");
                return 0;
            case 23: return c.project_id;
            default: return 0;
            }} catch (...) { c.error = std::current_exception(); return INT64_MIN; }
        };
        const auto id = a2fo_rs_c_odf_resolve(&context, step);
        if (context.error) std::rethrow_exception(context.error);
        return id;
    } catch (...) { return 0; }
}

}  // namespace a2fo

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    struct Context {
        const A2FO_ModuleApi* api;
        std::exception_ptr error{};
    } context{api};
    const auto step = +[](void* opaque, std::uint32_t operation,
                          std::size_t) noexcept -> std::int64_t {
        auto& c = *static_cast<Context*>(opaque);
        try { switch (operation) {
        case 0: return c.api != nullptr;
        case 1: return c.api->struct_size >= A2FO_MODULE_API_V4_BASE_SIZE;
        case 2: return c.api->api_version == A2FO_MODULE_API_VERSION;
        case 3: return c.api->log != nullptr;
        case 4: return c.api->armada_module != nullptr;
        case 5: return c.api->fleetops_module != nullptr;
        case 6: return c.api->register_fofs_item_lookup_handler != nullptr;
        case 7: g_api = c.api; return 0;
        case 8: g_armada = static_cast<HMODULE>(c.api->armada_module());
            return 0;
        case 9: g_fleet_ops = static_cast<HMODULE>(c.api->fleetops_module());
            return 0;
        case 10: return g_armada != nullptr;
        case 11: return validate_fleetops();
        case 12: g_api = nullptr; g_armada = nullptr; g_fleet_ops = nullptr;
            return 0;
        case 13: InitializeCriticalSection(&g_state_lock);
            g_state_lock_ready = true; return 0;
        case 14: return c.api->register_fofs_item_lookup_handler(
            kModuleName, &lookup_handler, nullptr);
        case 15: DeleteCriticalSection(&g_state_lock);
            g_state_lock_ready = false;
            g_api = nullptr; g_armada = nullptr; g_fleet_ops = nullptr;
            return 0;
        case 16: log_line("Native feature pack initialized"); return 0;
        case 17: a2fo::initialize_queue_enhancements(
            c.api, g_armada, g_fleet_ops); return 0;
        case 18: a2fo::initialize_upgrade_pods(
            c.api, g_armada, g_fleet_ops); return 0;
        case 19: a2fo::initialize_buildyard_pseudo_technology(
            c.api, g_armada, g_fleet_ops); return 0;
        default: return 0;
        }} catch (...) { c.error = std::current_exception(); return INT64_MIN; }
    };
    const bool result = a2fo_rs_c_odf_init(&context, step);
    if (context.error) std::rethrow_exception(context.error);
    return result;
}
