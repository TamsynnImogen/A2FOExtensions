#include "fpq_paths.hpp"
#include "hook.hpp"
#include "odf_paths.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
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

// This file bridges two different binaries and ABIs:
//   * ArmadaL.exe owns the gameplay classes and SOD database (MSVC thiscall).
//   * FleetOpsHook.dll owns the replacement virtual filesystem (Delphi register
//     convention: EAX/EDX/ECX before any stack arguments).
// Every address below is an RVA from the supported module's load base, never a
// process-global absolute address. docs/addresses.md records their provenance.
constexpr std::uint32_t kArmadaTimestamp = 0x3c4c76bd;
constexpr std::uint32_t kArmadaImageSize = 0x00403999;
constexpr std::uint32_t kFleetOpsTimestamp = 0x51f6475c;
constexpr std::uint32_t kFleetOpsImageSize = 0x00322000;

constexpr std::uintptr_t kEvolverClassBuildClassRva = 0x0a85e0;
constexpr std::uintptr_t kEvolverClassDtorRva = 0x0a85d0;
constexpr std::uintptr_t kCocoonSelectorRva = 0x0b0534;
constexpr std::uintptr_t kParameterDbGetStringRva = 0x135350;
constexpr std::uintptr_t kLoadSodRva = 0x22cf10;
constexpr std::uintptr_t kSodDatabaseRva = 0x3ad508;
constexpr std::uintptr_t kDefaultCocoonRva = 0x33fccc;
constexpr std::uintptr_t kAlternativeCocoonRva = 0x33fd3c;

// FleetOpsHook.dll RVAs.
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
constexpr std::uintptr_t kFofsItemGetHashLookupCallRva = 0x105fec;
constexpr std::uintptr_t kGetFileFromHashTableRva = 0x109c14;

constexpr std::size_t kBuiltInVirtualDirectoryCount = 28;
constexpr std::size_t kMaximumVirtualDirectoryCount = 255;
constexpr std::size_t kMaximumPathLength = 32767;

// A timestamp alone is not enough protection for an injected hook. Each patch
// also verifies the exact instructions it is about to replace, and fails
// closed when another Fleet Ops/Armada build is detected.
const std::uint8_t kExpectedBuildClass[] = {0x55, 0x8b, 0xec, 0x6a, 0xff};
const std::uint8_t kExpectedDtor[] = {0xc7, 0x01, 0x44, 0x1b, 0x6b, 0x00};
const std::uint8_t kExpectedCocoonJump[] = {0xe9, 0xa7, 0x07, 0x35, 0x00};
const std::uint8_t kExpectedParameterDbGetString[] = {
    // Correct instruction bytes still needed.
};
const std::uint8_t kExpectedAddDisk[] = {0x55, 0x8b, 0xec, 0x81, 0xc4,
                                         0xa4, 0xfe, 0xff, 0xff};
const std::uint8_t kExpectedAddPack[] = {0x55, 0x8b, 0xec, 0x83, 0xc4, 0xcc};
const std::uint8_t kExpectedHashString[] = {0x55, 0x8b, 0xec, 0x83, 0xc4,
                                            0xf4, 0x53, 0x56, 0x57};
const std::uint8_t kExpectedAddFileToHash[] = {0x53, 0x56, 0x51, 0x8b,
                                               0xf1, 0x89, 0x14, 0x24};
const std::uint8_t kExpectedFofsItemGetHashLookupCall[] = {
    0xe8, 0x23, 0x3c, 0x00, 0x00};


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

// Reverse-engineered fields used from Fleet Ops' TFOFSFileEntry and mod-info
// objects. Keep these together: changing one for a different engine build
// requires revalidating the precedence and hash-chain code below.
constexpr std::size_t kFileEntryNextOffset = 0x08;
constexpr std::size_t kFileEntryBasenameOffset = 0x0c;
constexpr std::size_t kFileEntryPackedOffset = 0x18;
constexpr std::size_t kFileEntryModInfoOffset = 0x1c;
constexpr std::size_t kFileEntryPrimaryRootOffset = 0x20;
constexpr std::size_t kFileEntryOverriddenOffset = 0x21;
constexpr std::size_t kModInfoPriorityOffset = 0x3c;

// Implemented in delphi_bridge.S. Normal C++ calls cannot express all of the
// Delphi register ABI and Armada's 32-bit thiscall ABI reliably, so these
// wrappers translate ordinary C stack arguments into the required registers.
extern "C" void a2fo_lstr_from_pchar(void* function, void** output,
                                      const char* text);
extern "C" void a2fo_lstr_clear(void* function, void** value);
extern "C" void* a2fo_create_virtual_directory(void* function, void* class_ref,
                                                 void* delphi_string,
                                                 std::uint32_t flag,
                                                 std::uint32_t index);
extern "C" int a2fo_tlist_add(void* function, void* list, void* item);
extern "C" std::uint32_t a2fo_hash_string(void* function,
                                           void* delphi_string);
extern "C" void a2fo_add_file_to_hash(void* function, void* file_system,
                                       void* entry, std::uint32_t index);
extern "C" void a2fo_add_items_from_disk(void* function, void* file_system,
                                          void* root, void* mod_info,
                                          std::uint32_t* count, std::uint32_t flag);
extern "C" void a2fo_add_items_from_pack(void* function, void* file_system,
                                          void* delphi_path, void* mod_info,
                                          std::uint32_t* count);
extern "C" void a2fo_renew_overrides(void* function, void* directory);
extern "C" void* a2fo_call_get_file_from_hash(void* function,
                                                void* file_system,
                                                void* delphi_name,
                                                std::uint32_t flags);
extern "C" void* a2fo_call_evolver_build_class(void* function, void* self,
                                                void* parameter_db);
extern "C" void* a2fo_call_evolver_dtor(void* function, void* self);
extern "C" bool a2fo_parameter_db_get_string(void* function, void* parameter_db,
                                               const char* key, char* output,
                                               std::uint32_t output_size,
                                               const char* default_value);
extern "C" void* a2fo_load_sod(void* function, void* database,
                                const char* name);
extern "C" void a2fo_cocoon_selector_hook();

HMODULE g_armada = nullptr;
HMODULE g_fleet_ops = nullptr;
HANDLE g_log = INVALID_HANDLE_VALUE;
CRITICAL_SECTION g_state_lock;
bool g_state_lock_ready = false;
bool g_evolver_hooks_ready = false;
bool g_fofs_item_get_lookup_hook_ready = false;
a2fo::InlineHook g_build_class_hook;
a2fo::InlineHook g_dtor_hook;
a2fo::InlineHook g_parameter_db_get_string_hook;
bool g_classlabel_alias_hook_ready = false;
volatile LONG g_recursive_odf_state = 0;
volatile DWORD g_recursive_odf_owner_thread = 0;
std::unordered_map<void*, std::string> g_class_cocoons;
std::unordered_map<std::string, void*> g_loaded_cocoons;
std::set<void*> g_logged_cocoon_classes;
std::map<std::string, std::string> g_recursive_odf_aliases;
std::map<std::string, void*> g_recursive_odf_winners;
std::set<std::string> g_logged_recursive_odf_lookups;

// g_recursive_odf_state values:
//   0 = not registered (or a previous attempt found the VFS unready)
//   1 = registration is in progress
//   2 = winner maps are complete and immutable until process exit

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
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
    while (value.size() > 3 && (value.back() == '\\' || value.back() == '/')) {
        value.pop_back();
    }
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    if (left.back() == '\\' || left.back() == '/') {
        return left + right;
    }
    return left + "\\" + right;
}

bool is_absolute_path(const std::string& path) {
    return path.size() >= 3 && std::isalpha(static_cast<unsigned char>(path[0])) &&
           path[1] == ':' && (path[2] == '\\' || path[2] == '/');
}

std::string full_path(const std::string& path) {
    std::vector<char> buffer(kMaximumPathLength);
    const DWORD length = GetFullPathNameA(path.c_str(),
                                          static_cast<DWORD>(buffer.size()),
                                          buffer.data(), nullptr);
    if (length == 0 || length >= buffer.size()) {
        return path;
    }
    std::string result(buffer.data(), length);
    replace_slashes(result);
    trim_trailing_slashes(result);
    return result;
}

std::string module_directory(HMODULE module) {
    std::vector<char> buffer(kMaximumPathLength);
    const DWORD length = GetModuleFileNameA(module, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
    if (length == 0 || length >= buffer.size()) {
        return {};
    }
    std::string path(buffer.data(), length);
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? std::string{} : path.substr(0, slash);
}

void open_log() {
    const std::string directory = module_directory(nullptr);
    const std::string path = join_path(directory, "A2FOExtensions.log");
    g_log = CreateFileA(path.c_str(), GENERIC_WRITE,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
}

void log_line(const std::string& text) {
    if (g_log == INVALID_HANDLE_VALUE) {
        return;
    }
    const std::string line = text + "\r\n";
    DWORD written = 0;
    WriteFile(g_log, line.data(), static_cast<DWORD>(line.size()), &written, nullptr);
    FlushFileBuffers(g_log);
}

bool validate_module(HMODULE module, std::uint32_t timestamp,
                     std::uint32_t image_size, const char* label) {
    if (!module) {
        log_line(std::string(label) + " is not loaded");
        return false;
    }
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        log_line(std::string(label) + " has no DOS header");
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const std::uint8_t*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp != timestamp ||
        nt->OptionalHeader.SizeOfImage != image_size) {
        char message[256]{};
        std::snprintf(message, sizeof(message),
                      "%s version mismatch (timestamp=%08lx, image=%08lx)",
                      label,
                      static_cast<unsigned long>(nt->FileHeader.TimeDateStamp),
                      static_cast<unsigned long>(nt->OptionalHeader.SizeOfImage));
        log_line(message);
        return false;
    }
    return true;
}

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) {
    return reinterpret_cast<T*>(reinterpret_cast<std::uint8_t*>(module) + rva);
}

std::string delphi_string(const void* owner, std::size_t offset) {
    if (!owner) {
        return {};
    }
    const auto* text = *reinterpret_cast<char* const*>(
        static_cast<const std::uint8_t*>(owner) + offset);
    if (!text || IsBadStringPtrA(text, kMaximumPathLength)) {
        return {};
    }
    return std::string(text);
}

std::string delphi_string_value(const void* value) {
    const auto* text = static_cast<const char*>(value);
    if (!text || IsBadStringPtrA(text, kMaximumPathLength)) {
        return {};
    }
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

bool readable_list(DelphiList* list, std::int32_t minimum_count,
                   std::int32_t maximum_count) {
    if (!readable_range(list, sizeof(*list))) {
        return false;
    }
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
    return lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".odf") == 0;
}

void scan_loose_odfs(const std::string& absolute_directory,
                     const std::string& relative_directory,
                     std::map<std::string, std::string>& directories,
                     unsigned depth = 0) {
    // Reparse points are skipped to prevent junction loops. The depth limit is
    // a second guard for unusually deep or malformed mod directory trees.
    if (depth > 64) {
        return;
    }
    WIN32_FIND_DATAA data{};
    const std::string pattern = join_path(absolute_directory, "*");
    HANDLE search = FindFirstFileA(pattern.c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) {
        return;
    }
    bool contains_odf = false;
    do {
        const std::string name = data.cFileName;
        if (name == "." || name == "..") {
            continue;
        }
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
                scan_loose_odfs(join_path(absolute_directory, name),
                                join_path(relative_directory, name), directories,
                                depth + 1);
            }
        } else if (has_odf_extension(name)) {
            contains_odf = true;
        }
    } while (FindNextFileA(search, &data));
    FindClose(search);
    if (contains_odf) {
        std::string normalized = relative_directory;
        replace_slashes(normalized);
        directories.emplace(lower_ascii(normalized), normalized);
    }
}

bool read_fpq_metadata(const std::string& path, std::vector<std::uint8_t>& bytes) {
    // Only the FPQ header and metadata tables are needed to discover virtual
    // ODF directories; compressed file payloads are deliberately not read.
    HANDLE file = CreateFileA(path.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    std::uint8_t header[0x1c]{};
    DWORD got = 0;
    if (!ReadFile(file, header, sizeof(header), &got, nullptr) || got != sizeof(header) ||
        std::memcmp(header, "FPQ\0", 4) != 0) {
        CloseHandle(file);
        return false;
    }
    auto u32 = [&](std::size_t offset) {
        std::uint32_t value = 0;
        std::memcpy(&value, header + offset, sizeof(value));
        return value;
    };
    const std::uint64_t folders = u32(0x08);
    const std::uint64_t files = u32(0x0c);
    const std::uint64_t hashes = u32(0x10);
    const std::uint64_t names = u32(0x18);
    const std::uint64_t metadata_size = 0x1cull + hashes * 12ull +
                                        folders * 12ull + files * 25ull + names;
    if (metadata_size < sizeof(header) || metadata_size > 128ull * 1024ull * 1024ull) {
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
        if (!ReadFile(file, bytes.data() + total, wanted, &read, nullptr) || read == 0) {
            CloseHandle(file);
            return false;
        }
        total += read;
    }
    CloseHandle(file);
    return true;
}

std::unordered_map<void*, void*> map_roots_to_mod_info(void* file_system) {
    // Fleet Ops stores roots and mod metadata in separate object graphs. File
    // entries are the reliable link between them, so recover that association
    // before creating entries for newly discovered directories.
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

std::vector<RootInfo> collect_roots(void* file_system, const std::string& data_dir) {
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
                                          ? root.runtime_path
                                          : join_path(data_dir, root.runtime_path);
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
    std::map<std::string, std::string>& directories) {
    std::set<std::string> seen;
    std::vector<ArchiveInfo> result;
    for (const RootInfo& root : roots) {
        ArchiveInfo archive;
        archive.mod_info = root.mod_info;
        archive.runtime_path = join_path(root.runtime_path, "odf.fpq");
        archive.absolute_path = full_path(join_path(root.absolute_path, "odf.fpq"));
        if (GetFileAttributesA(archive.absolute_path.c_str()) == INVALID_FILE_ATTRIBUTES) {
            continue;
        }
        const std::string key = lower_ascii(archive.absolute_path);
        if (!seen.insert(key).second) {
            continue;
        }
        std::vector<std::uint8_t> metadata;
        if (!read_fpq_metadata(archive.absolute_path, metadata)) {
            log_line("Could not read FPQ metadata: " + archive.absolute_path);
            continue;
        }
        const a2fo::FpqPathResult parsed = a2fo::parse_fpq_odf_directories(metadata);
        if (!parsed.ok) {
            log_line("Could not parse FPQ metadata: " + archive.absolute_path +
                     " (" + parsed.error + ")");
            continue;
        }
        for (const std::string& path : parsed.odf_directories) {
            directories.emplace(lower_ascii(path), path);
        }
        result.push_back(std::move(archive));
    }
    return result;
}

// Fleet Ops' disk/archive scanners always walk the filesystem's directory
// list. Temporarily presenting only the new directories prevents rescanning
// and duplicating all 28 built-in directory entries. The original Delphi list
// storage is restored by the destructor even if C++ unwinds.
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

void* create_virtual_directory(const std::string& path, std::uint32_t index) {
    void* delphi_path = nullptr;
    a2fo_lstr_from_pchar(at(g_fleet_ops, kLStrFromPCharRva), &delphi_path,
                         path.c_str());
    void* class_ref = *at<void*>(g_fleet_ops, kVirtualDirectoryClassRefRva);
    void* result = a2fo_create_virtual_directory(
        at(g_fleet_ops, kVirtualDirectoryCtorRva), class_ref, delphi_path, 1, index);
    a2fo_lstr_clear(at(g_fleet_ops, kLStrClearRva), &delphi_path);
    return result;
}

template <typename T>
T& field(void* object, std::size_t offset) {
    return *reinterpret_cast<T*>(static_cast<std::uint8_t*>(object) + offset);
}

bool file_entry_precedes(void* left, void* right) {
    // Match Fleet Ops' effective override policy: active mod priority first,
    // then primary root, then loose files over packed FPQ files.
    void* left_mod = field<void*>(left, kFileEntryModInfoOffset);
    void* right_mod = field<void*>(right, kFileEntryModInfoOffset);
    if (left_mod != right_mod) {
        if (!left_mod) {
            return false;
        }
        if (!right_mod) {
            return true;
        }
        const std::int32_t left_priority =
            field<std::int32_t>(left_mod, kModInfoPriorityOffset);
        const std::int32_t right_priority =
            field<std::int32_t>(right_mod, kModInfoPriorityOffset);
        return left_priority < right_priority;
    }

    const std::uint8_t left_primary =
        field<std::uint8_t>(left, kFileEntryPrimaryRootOffset);
    const std::uint8_t right_primary =
        field<std::uint8_t>(right, kFileEntryPrimaryRootOffset);
    if (left_primary != right_primary) {
        return left_primary > right_primary;
    }

    const std::uint8_t left_packed =
        field<std::uint8_t>(left, kFileEntryPackedOffset);
    const std::uint8_t right_packed =
        field<std::uint8_t>(right, kFileEntryPackedOffset);
    if (left_packed != right_packed) {
        return left_packed < right_packed;
    }

    return false;
}

bool collect_hash_bucket(void* file_system, std::uint32_t index,
                         std::vector<void*>& entries) {
    // Snapshot one doubly-linked bucket without modifying it. The visited set
    // and item-count bound turn a damaged/cyclic chain into a safe failure.
    entries.clear();
    const std::uint32_t bucket_count =
        field<std::uint32_t>(file_system, 0x08);
    void** buckets = field<void**>(file_system, 0x0c);
    const std::uint32_t item_count =
        field<std::uint32_t>(file_system, 0x20);
    if (bucket_count == 0 || bucket_count > 0x100000 || index >= bucket_count ||
        !buckets ||
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
    void* file_system, const std::vector<void*>& custom_directory_list) {
    // Add only the entries produced by our temporary scan. We intentionally do
    // not reorder Fleet Ops' global buckets; the parser hook selects a custom
    // winner directly and leaves unrelated file resolution untouched.
    const std::uint32_t bucket_count =
        field<std::uint32_t>(file_system, 0x08);
    void** buckets = field<void**>(file_system, 0x0c);
    if (bucket_count == 0 || bucket_count > 0x100000 || !buckets ||
        !readable_range(buckets,
                        static_cast<std::size_t>(bucket_count) * sizeof(void*))) {
        log_line("Recursive ODF hash insertion: invalid Fleet Operations hash table");
        return 0;
    }

    std::size_t added = 0;
    for (void* directory : custom_directory_list) {
        auto* entries = field<DelphiList*>(directory, 0x0c);
        for (void* entry : list_items(entries)) {
            void* basename = field<void*>(entry, kFileEntryBasenameOffset);
            if (!basename) {
                continue;
            }
            const std::uint32_t hash =
                a2fo_hash_string(at(g_fleet_ops, kHashStringRva), basename);
            const std::uint32_t index = hash & (bucket_count - 1);
            std::vector<void*> chain;
            if (!collect_hash_bucket(file_system, index, chain)) {
                log_line("Recursive ODF hash insertion: malformed target bucket");
                continue;
            }
            if (std::find(chain.begin(), chain.end(), entry) != chain.end()) {
                continue;
            }
            a2fo_add_file_to_hash(at(g_fleet_ops, kAddFileToHashTableRva),
                                  file_system, entry, index);
            ++added;
        }
    }
    log_line("Recursive ODF hash ready: " + std::to_string(added) +
             " entries added");
    return added;
}

void build_recursive_odf_winners(
    void* file_system, const std::vector<void*>& custom_directory_list) {
    // Compute one custom winner per basename after all new entries exist. The
    // string map is for diagnostics; the pointer map is what the parser hook
    // returns to Fleet Ops' unmodified file-reading code.
    const std::uint32_t bucket_count =
        field<std::uint32_t>(file_system, 0x08);
    void** buckets = field<void**>(file_system, 0x0c);
    if (bucket_count == 0 || bucket_count > 0x100000 || !buckets ||
        !readable_range(buckets,
                        static_cast<std::size_t>(bucket_count) * sizeof(void*))) {
        log_line("Recursive ODF winners: invalid Fleet Operations hash table");
        return;
    }

    std::map<void*, std::string> custom_entries;
    std::map<std::string, std::vector<void*>> custom_groups;
    for (void* directory : custom_directory_list) {
        auto* entries = field<DelphiList*>(directory, 0x0c);
        for (void* entry : list_items(entries)) {
            custom_entries.emplace(entry, delphi_string(directory, 4));
            const std::string basename = lower_ascii(
                delphi_string(entry, kFileEntryBasenameOffset));
            if (!basename.empty()) {
                custom_groups[basename].push_back(entry);
            }
            log_line("Recursive ODF entry: " + basename +
                     " from " + delphi_string(directory, 4) +
                     " (primary=" +
                     std::to_string(field<std::uint8_t>(
                         entry, kFileEntryPrimaryRootOffset)) +
                     ", packed=" +
                     std::to_string(field<std::uint8_t>(
                         entry, kFileEntryPackedOffset)) +
                     ", overridden=" +
                     std::to_string(field<std::uint8_t>(
                         entry, kFileEntryOverriddenOffset)) + ")");
        }
    }
    log_line("Recursive ODF precedence: " +
             std::to_string(custom_entries.size()) +
             " custom entries in hash table");
    std::map<std::string, std::string> aliases;
    std::map<std::string, void*> winners;
    for (const auto& group : custom_groups) {
        void* basename_value = field<void*>(
            group.second.front(), kFileEntryBasenameOffset);
        const std::uint32_t hash =
            a2fo_hash_string(at(g_fleet_ops, kHashStringRva), basename_value);
        const std::uint32_t bucket = hash & (bucket_count - 1);
        std::vector<void*> chain;
        if (!collect_hash_bucket(file_system, bucket, chain)) {
            log_line("Recursive ODF alias skipped malformed bucket: " +
                     group.first);
            continue;
        }

        std::string contents;
        for (void* candidate : chain) {
            if (!contents.empty()) {
                contents += ", ";
            }
            contents += lower_ascii(delphi_string(
                candidate, kFileEntryBasenameOffset));
            if (custom_entries.find(candidate) != custom_entries.end()) {
                contents += " [recursive]";
            }
        }
        log_line("Recursive ODF hash bucket " + std::to_string(bucket) +
                 ": " + contents);

        void* winner = nullptr;
        for (void* candidate : chain) {
            const std::string basename =
                lower_ascii(delphi_string(candidate,
                                          kFileEntryBasenameOffset));
            if (basename != group.first ||
                field<std::uint8_t>(candidate,
                                    kFileEntryOverriddenOffset) != 0) {
                continue;
            }
            if (!winner || file_entry_precedes(candidate, winner)) {
                winner = candidate;
                continue;
            }
            if (!file_entry_precedes(winner, candidate) &&
                custom_entries.find(candidate) != custom_entries.end() &&
                custom_entries.find(winner) == custom_entries.end()) {
                winner = candidate;
            }
        }

        const auto custom_winner = custom_entries.find(winner);
        if (custom_winner != custom_entries.end()) {
            const std::string basename =
                delphi_string(winner, kFileEntryBasenameOffset);
            if (!basename.empty()) {
                const std::string target =
                    join_path(custom_winner->second, basename);
                aliases[group.first] = target;
                winners[group.first] = winner;
                log_line("Recursive ODF winner: " + group.first + " -> " +
                         target);
            }
        }
    }
    EnterCriticalSection(&g_state_lock);
    g_recursive_odf_aliases.swap(aliases);
    g_recursive_odf_winners.swap(winners);
    g_logged_recursive_odf_lookups.clear();
    const std::size_t winner_count = g_recursive_odf_winners.size();
    LeaveCriticalSection(&g_state_lock);
    log_line("Recursive ODF precedence ready: " +
             std::to_string(winner_count) + " basename winners published");
}

bool register_recursive_odfs(void* file_system) {
    // Registration pipeline:
    //   1. discover loose and FPQ ODF directories under active mod roots;
    //   2. create Fleet Ops virtual-directory objects for missing paths;
    //   3. ask Fleet Ops to populate file entries for just those directories;
    //   4. publish the entries into its basename hash and calculate winners.
    if (!readable_range(file_system, 0x24)) {
        log_line("Fleet Operations filesystem is not ready; deferring recursive ODF registration");
        return false;
    }
    auto* directory_list = *reinterpret_cast<DelphiList**>(
        static_cast<std::uint8_t*>(file_system) + 4);
    auto* root_list = *reinterpret_cast<DelphiList**>(
        static_cast<std::uint8_t*>(file_system) + 0x18);
    const std::uint32_t item_count_ready = field<std::uint32_t>(file_system, 0x20);
    if (!readable_list(directory_list,
                       static_cast<std::int32_t>(kBuiltInVirtualDirectoryCount),
                       1000000) ||
        !readable_list(root_list, 1, 1000000) || item_count_ready == 0) {
        log_line("Fleet Operations filesystem is not ready; deferring recursive ODF registration");
        return false;
    }

    const std::string data_dir = module_directory(nullptr);
    const std::vector<RootInfo> roots = collect_roots(file_system, data_dir);
    log_line("Active filesystem roots: " + std::to_string(roots.size()));
    for (const RootInfo& root : roots) {
        log_line("  root: " + root.runtime_path);
    }
    std::map<std::string, std::string> candidates;
    for (const RootInfo& root : roots) {
        scan_loose_odfs(join_path(root.absolute_path, "odf"), "odf", candidates);
    }
    std::vector<ArchiveInfo> archives = collect_archives(roots, candidates);
    log_line("Active ODF archives: " + std::to_string(archives.size()));
    for (const ArchiveInfo& archive : archives) {
        log_line("  archive: " + archive.runtime_path);
    }

    std::set<std::string> existing;
    for (void* directory : list_items(directory_list)) {
        existing.insert(lower_ascii(delphi_string(directory, 4)));
    }
    for (auto iterator = candidates.begin(); iterator != candidates.end();) {
        if (existing.find(iterator->first) != existing.end()) {
            iterator = candidates.erase(iterator);
        } else {
            ++iterator;
        }
    }

    const std::size_t available = kMaximumVirtualDirectoryCount -
                                  std::min<std::size_t>(directory_list->count,
                                                        kMaximumVirtualDirectoryCount);
    if (candidates.size() > available) {
        log_line("Recursive directory limit reached; ignoring " +
                 std::to_string(candidates.size() - available) + " directories");
    }

    std::vector<void*> custom_directories;
    custom_directories.reserve(std::min(candidates.size(), available));
    std::size_t added = 0;
    for (const auto& candidate : candidates) {
        if (added >= available) {
            break;
        }
        void* directory = create_virtual_directory(
            candidate.second,
            static_cast<std::uint32_t>(directory_list->count));
        if (!directory) {
            log_line("Failed to create virtual directory: " + candidate.second);
            continue;
        }
        a2fo_tlist_add(at(g_fleet_ops, kTListAddRva), directory_list, directory);
        custom_directories.push_back(directory);
        ++added;
        log_line("Registered ODF directory: " + candidate.second);
    }

    if (custom_directories.empty()) {
        log_line("Recursive ODF index: no custom directories found");
        return true;
    }

    auto* item_count = reinterpret_cast<std::uint32_t*>(
        static_cast<std::uint8_t*>(file_system) + 0x20);
    const std::uint32_t item_count_before = *item_count;
    {
        TemporaryDirectoryList temporary(directory_list, custom_directories);
        for (const RootInfo& root : roots) {
            a2fo_add_items_from_disk(at(g_fleet_ops, kAddItemsFromDiskRva),
                                     file_system, root.object, root.mod_info,
                                     item_count, root.primary ? 1u : 0u);
        }
        for (const ArchiveInfo& archive : archives) {
            void* delphi_path = nullptr;
            a2fo_lstr_from_pchar(at(g_fleet_ops, kLStrFromPCharRva), &delphi_path,
                                 archive.runtime_path.c_str());
            a2fo_add_items_from_pack(at(g_fleet_ops, kAddItemsFromPackRva),
                                     file_system, delphi_path, archive.mod_info,
                                     item_count);
            a2fo_lstr_clear(at(g_fleet_ops, kLStrClearRva), &delphi_path);
        }
    }
    for (void* directory : custom_directories) {
        a2fo_renew_overrides(at(g_fleet_ops, kRenewOverridesRva), directory);
    }
    add_custom_entries_to_hash(file_system, custom_directories);
    build_recursive_odf_winners(file_system, custom_directories);
    log_line("Recursive ODF index ready: " + std::to_string(custom_directories.size()) +
             " custom directories, " +
             std::to_string(*item_count - item_count_before) + " ODF entries");
    return true;
}

bool register_recursive_odfs_once(void* file_system) {
    // The first ParameterDB item lookup performs registration. Same-thread
    // recursion is allowed to fall through, while another thread receives a
    // bounded wait rather than observing half-built maps.
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
        if (previous == 2) {
            return true;
        }
        if (g_recursive_odf_owner_thread == current_thread) {
            return true;
        }
        if (waited >= kRegistrationTimeoutMilliseconds) {
            log_line("Timed out waiting for recursive ODF registration");
            return false;
        }
        Sleep(1);
        ++waited;
    }

    bool ok = false;
    try {
        ok = register_recursive_odfs(file_system);
    } catch (...) {
        log_line("Recursive ODF registration aborted by an unexpected C++ exception");
    }
    g_recursive_odf_owner_thread = 0;
    InterlockedExchange(&g_recursive_odf_state, ok ? 2 : 0);
    return ok;
}

void* __attribute__((regparm(3))) fofs_item_get_hash_lookup_hook(
    void* file_system, void* delphi_name, std::uintptr_t flags) {
    // Patched into the one GetFileFromHashTable call inside FOFS_ItemGet. At
    // this point EAX=file_system, EDX=Delphi filename, and CL=lookup flags.
    // Registering here is late enough that Fleet Ops has finished its native
    // VFS index, but early enough to affect ParameterDB's current ODF request.
    if (InterlockedCompareExchange(&g_recursive_odf_state, 0, 0) != 2) {
        log_line("Fleet Operations first ODF item lookup hook reached");
        register_recursive_odfs_once(file_system);
    }

    void* native = a2fo_call_get_file_from_hash(
        at(g_fleet_ops, kGetFileFromHashTableRva), file_system, delphi_name,
        static_cast<std::uint32_t>(flags & 0xffu));
    if (InterlockedCompareExchange(&g_recursive_odf_state, 0, 0) == 2) {
        try {
            const std::string requested_name =
                delphi_string_value(delphi_name);
            const std::string key =
                a2fo::recursive_odf_basename_key(requested_name);
            void* winner = nullptr;
            std::string winner_path;
            bool first_log = false;
            EnterCriticalSection(&g_state_lock);
            const auto selected = g_recursive_odf_winners.find(key);
            if (selected != g_recursive_odf_winners.end()) {
                winner = selected->second;
                const auto path = g_recursive_odf_aliases.find(key);
                if (path != g_recursive_odf_aliases.end()) {
                    winner_path = path->second;
                }
                first_log = g_logged_recursive_odf_lookups.insert(key).second;
            }
            LeaveCriticalSection(&g_state_lock);
            if (winner) {
                if (first_log) {
                    log_line("Recursive ODF lookup: " + requested_name + " -> " +
                             winner_path);
                }
                return winner;
            }
        } catch (...) {
            log_line("Recursive ODF winner lookup failed; using native lookup");
        }
    }
    return native;
}

std::string normalize_cocoon_name(const char* value) {
    // ODF authors may omit .sod and add surrounding whitespace. Empty values
    // deliberately mean "use the original Fleet Ops selection".
    if (!value) {
        return {};
    }
    std::string name(value);
    const auto first = name.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = name.find_last_not_of(" \t\r\n");
    name = name.substr(first, last - first + 1);
    replace_slashes(name);
    const std::string lower = lower_ascii(name);
    if (lower.size() < 4 || lower.compare(lower.size() - 4, 4, ".sod") != 0) {
        name += ".sod";
    }
    return name;
}

bool __attribute__((fastcall)) parameter_db_get_string_hook(...)

void* __attribute__((fastcall)) evolver_class_build_class_hook(
    void* self, void*, void* parameter_db) {
    // Let Armada construct the complete class first, then associate the final
    // class object returned in EAX with its optional cocoon setting.
    void* result = a2fo_call_evolver_build_class(g_build_class_hook.gateway,
                                                  self, parameter_db);
    char value[MAX_PATH]{};
    char basename[MAX_PATH]{};
    if (parameter_db) {
        a2fo_parameter_db_get_string(g_parameter_db_get_string_hook.gateway,
                                     parameter_db, "cocoon", value,
                                     sizeof(value), "");
        a2fo_parameter_db_get_string(g_parameter_db_get_string_hook.gateway,
                                     parameter_db, "basename", basename,
                                     sizeof(basename), "<unnamed>");
    }
    const std::string cocoon = normalize_cocoon_name(value);
    EnterCriticalSection(&g_state_lock);
    if (result) {
        if (cocoon.empty()) {
            g_class_cocoons.erase(result);
        } else {
            g_class_cocoons[result] = cocoon;
        }
    }
    LeaveCriticalSection(&g_state_lock);
    log_line("EvolverClass " + std::string(basename) + " cocoon: " +
             (cocoon.empty() ? "<Fleet Ops default>" : cocoon));
    return result;
}

void* __attribute__((fastcall)) evolver_class_dtor_hook(void* self, void*) {
    // Class pointers can be reused after destruction; erase all pointer-keyed
    // state so a later class cannot inherit an old cocoon choice.
    EnterCriticalSection(&g_state_lock);
    g_class_cocoons.erase(self);
    g_logged_cocoon_classes.erase(self);
    LeaveCriticalSection(&g_state_lock);
    return a2fo_call_evolver_dtor(g_dtor_hook.gateway, self);
}

bool install_classlabel_alias_hook()

bool install_evolver_hooks() {
    if (std::memcmp(at(g_armada, kEvolverClassBuildClassRva),
                    kExpectedBuildClass, sizeof(kExpectedBuildClass)) != 0 ||
        std::memcmp(at(g_armada, kEvolverClassDtorRva), kExpectedDtor,
                    sizeof(kExpectedDtor)) != 0 ||
        std::memcmp(at(g_armada, kCocoonSelectorRva), kExpectedCocoonJump,
                    sizeof(kExpectedCocoonJump)) != 0) {
        log_line("Evolver hook signature mismatch; no evolver hooks installed");
        return false;
    }
    if (!a2fo::install_inline_hook(
            at(g_armada, kEvolverClassBuildClassRva),
            reinterpret_cast<void*>(&evolver_class_build_class_hook),
            sizeof(kExpectedBuildClass), kExpectedBuildClass,
            g_build_class_hook)) {
        log_line("EvolverClass BuildClass hook signature mismatch");
        return false;
    }
    if (!a2fo::install_inline_hook(at(g_armada, kEvolverClassDtorRva),
                                   reinterpret_cast<void*>(&evolver_class_dtor_hook),
                                   sizeof(kExpectedDtor), kExpectedDtor, g_dtor_hook)) {
        log_line("EvolverClass destructor hook signature mismatch");
        return false;
    }
    if (!a2fo::patch_jump(at(g_armada, kCocoonSelectorRva),
                          reinterpret_cast<void*>(&a2fo_cocoon_selector_hook),
                          kExpectedCocoonJump, sizeof(kExpectedCocoonJump))) {
        log_line("Cocoon selector hook signature mismatch");
        return false;
    }
    log_line("Evolver cocoon ODF command enabled");
    return true;
}

bool install_fofs_item_get_lookup_hook() {
    if (!g_fleet_ops ||
        std::memcmp(at(g_fleet_ops, kFofsItemGetHashLookupCallRva),
                    kExpectedFofsItemGetHashLookupCall,
                    sizeof(kExpectedFofsItemGetHashLookupCall)) != 0) {
        log_line("Fleet Operations ODF item lookup call signature mismatch");
        return false;
    }
    if (!a2fo::patch_call(
            at(g_fleet_ops, kFofsItemGetHashLookupCallRva),
            reinterpret_cast<void*>(&fofs_item_get_hash_lookup_hook),
            kExpectedFofsItemGetHashLookupCall,
            sizeof(kExpectedFofsItemGetHashLookupCall))) {
        log_line("Could not install Fleet Operations ODF item lookup hook");
        return false;
    }
    log_line("Recursive ODF parser lookup enabled");
    return true;
}

DWORD WINAPI initialize(void*) {
    // DllMain installs immediately when possible. This worker is a fallback
    // for load orders where FleetOpsHook.dll was not visible at process attach;
    // it also avoids doing repeated module polling under the loader lock.
    g_armada = GetModuleHandleA(nullptr);
    for (unsigned attempt = 0; attempt < 100 && !g_fleet_ops; ++attempt) {
        g_fleet_ops = GetModuleHandleA("FleetOpsHook.dll");
        if (!g_fleet_ops) {
            Sleep(10);
        }
    }
    if (!validate_module(g_armada, kArmadaTimestamp, kArmadaImageSize, "ArmadaL.exe") ||
        !validate_module(g_fleet_ops, kFleetOpsTimestamp, kFleetOpsImageSize,
                         "FleetOpsHook.dll")) {
        log_line("No hooks installed");
        return 1;
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
        return 1;
    }

    try {
      try {
        if (g_classlabel_alias_hook_ready) {
          log_line("Classlabel alias hook enabled before class loading");
        } else {
          g_classlabel_alias_hook_ready =
            install_classlabel_alias_hook();
        }
       if (g_evolver_hooks_ready) {
            log_line("Evolver cocoon ODF command enabled before class loading");
        } else {
            g_evolver_hooks_ready = install_evolver_hooks();
        }
        if (g_fofs_item_get_lookup_hook_ready) {
            log_line("Recursive ODF parser lookup hook enabled early");
        } else {
            g_fofs_item_get_lookup_hook_ready =
                install_fofs_item_get_lookup_hook();
        }
    } catch (...) {
        log_line("Initialization aborted by an unexpected C++ exception");
        return 1;
    }
    log_line("A2FOExtensions initialization complete");
    return 0;
}

}  // namespace

extern "C" {
void* a2fo_cocoon_resume = nullptr;
}

extern "C" void* a2fo_select_cocoon(void* evolver) {
    // Called by the small assembly splice at Armada's cocoon-selection site.
    // Return a cached/custom geometry when possible, otherwise reproduce the
    // two original Fleet Ops defaults exactly.
    if (!evolver || !g_state_lock_ready) {
        return nullptr;
    }
    const auto* bytes = static_cast<std::uint8_t*>(evolver);
    void* class_object = *reinterpret_cast<void* const*>(bytes + 0x40);
    std::string name;
    bool cache_known = false;
    void* cached_geometry = nullptr;
    bool first_selection = false;
    EnterCriticalSection(&g_state_lock);
    const auto selected = g_class_cocoons.find(class_object);
    if (selected != g_class_cocoons.end()) {
        name = selected->second;
    }
    if (!name.empty()) {
        const auto cached = g_loaded_cocoons.find(lower_ascii(name));
        if (cached != g_loaded_cocoons.end()) {
            cache_known = true;
            cached_geometry = cached->second;
        }
    }
    first_selection = g_logged_cocoon_classes.insert(class_object).second;
    LeaveCriticalSection(&g_state_lock);

    if (first_selection) {
        log_line("Cocoon selector for EvolverClass: " +
                 (name.empty() ? std::string("<Fleet Ops default>") : name));
    }

    if (cache_known) {
        if (cached_geometry) {
            return cached_geometry;
        }
        name.clear();
    }

    if (!name.empty()) {
        void* database = *at<void*>(g_armada, kSodDatabaseRva);
        void* geometry = database ? a2fo_load_sod(at(g_armada, kLoadSodRva),
                                                  database, name.c_str())
                                  : nullptr;
        EnterCriticalSection(&g_state_lock);
        g_loaded_cocoons[lower_ascii(name)] = geometry;
        LeaveCriticalSection(&g_state_lock);
        if (geometry) {
            return geometry;
        }
        log_line("Could not load cocoon SOD; using Fleet Ops default: " + name);
    }

    const bool alternative =
        *reinterpret_cast<const std::uint32_t*>(bytes + 0x1e8) == 0;
    const std::uintptr_t pointer_rva = alternative ? kAlternativeCocoonRva
                                                   : kDefaultCocoonRva;
    return *at<void*>(g_armada, pointer_rva);
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        // The startup proxy loads us before Armada builds its class database.
        // Installing the evolver hook here prevents an early class from being
        // constructed before the new ODF command is visible.
        DisableThreadLibraryCalls(instance);
        open_log();
        log_line("A2FOExtensions initialization started");
        g_armada = GetModuleHandleA(nullptr);
        g_fleet_ops = GetModuleHandleA("FleetOpsHook.dll");
        a2fo_cocoon_resume = at(g_armada, kCocoonSelectorRva + 5);
        InitializeCriticalSection(&g_state_lock);
        g_state_lock_ready = true;
        if (validate_module(g_armada, kArmadaTimestamp, kArmadaImageSize,
                            "ArmadaL.exe")) {
            g_evolver_hooks_ready = install_evolver_hooks();
            if (validate_module(g_fleet_ops, kFleetOpsTimestamp,
                                kFleetOpsImageSize, "FleetOpsHook.dll")) {
                g_fofs_item_get_lookup_hook_ready =
                    install_fofs_item_get_lookup_hook();
            }
        }
        HANDLE thread = CreateThread(nullptr, 0, initialize, nullptr, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
