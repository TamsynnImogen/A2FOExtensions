/*
 * Recursive edit-menu navigation for Fleet Operations.
 *
 * Armada's native EditMenu parser allocates a fixed 12 x 12 x 12 structure:
 * menuName -> buildItem -> item. This module retains that ABI and dynamically
 * refills the selected root slot when a buildItem target contains buildItem
 * commands of its own. The native renderer, hotkeys, placement code, and Back
 * command therefore remain in control at every visible level.
 */

#include "../../sdk/include/a2fo_module_api.h"
#include "edit_menu_odf.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" std::uintptr_t __cdecl a2fo_edit_menu_call_thiscall_0(
    void* function, void* self);

namespace {

constexpr char kModuleName[] = "A2FOEditMenu";
constexpr std::size_t kMaximumDepth = 32;
constexpr std::size_t kMaximumOdfSize = 2 * 1024 * 1024;
constexpr std::size_t kMaximumNativeString = 1024;

// ArmadaL.exe 1.1 / Fleet Operations Roots RVAs.
constexpr std::uintptr_t kEditMenuUpdateRva = 0x0011c610;
constexpr std::uintptr_t kEditMenuRootSlotsRva = 0x00365038;
constexpr std::uintptr_t kFindObjectClassByNameRva = 0x000cd370;
constexpr std::uintptr_t kEngineOperatorNewRva = 0x00252710;
constexpr std::uintptr_t kEngineOperatorDeleteRva = 0x002527d0;

constexpr std::size_t kObjectClassProjectIdOffset = 0x1cc;

constexpr std::size_t kEditMenuSelectedBuildItemOffset = 0x24;
constexpr std::size_t kEditMenuSelectedRootOffset = 0x28;
constexpr std::size_t kEditMenuLevelOffset = 0x2c;

constexpr std::array<std::uint8_t, 9> kExpectedEditMenuUpdate{{
    0x55, 0x8b, 0xec, 0x81, 0xec, 0xc4, 0x00, 0x00, 0x00}};

struct NativeBuildMenu {
    char* source_name = nullptr;
    char* title = nullptr;
    std::uint8_t force_to_neutral = 0;
    std::uint8_t padding[3]{};
    // Despite the original ODF command containing a string, ParameterDB's
    // GetProjectId writes a numeric cPrjID into this native array.
    std::array<std::uint32_t, a2fo::edit_menu::kEntryCount>
        item_project_ids{};
    std::array<void*, a2fo::edit_menu::kEntryCount> item_classes{};
};
static_assert(sizeof(NativeBuildMenu) == 0x6c,
              "Armada edit build-menu ABI must occupy 0x6c bytes");

struct NativeRootMenu {
    char* source_name = nullptr;
    char* title = nullptr;
    std::array<NativeBuildMenu, a2fo::edit_menu::kEntryCount> build_menus{};
};
static_assert(sizeof(NativeRootMenu) == 0x518,
              "Armada edit root-menu ABI must occupy 0x518 bytes");

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
bool g_runtime_ready = false;
bool g_processing_hook = false;
A2FO_InlineHook g_edit_menu_update_hook{};

std::map<std::string, std::string> g_odf_paths;
std::unordered_map<std::string,
                   std::unique_ptr<a2fo::edit_menu::MenuNode>> g_nodes;

void* g_active_edit_menu = nullptr;
std::int32_t g_active_root = -1;
NativeRootMenu g_saved_root{};
bool g_scratch_installed = false;
std::string g_current_node;
std::vector<std::string> g_breadcrumbs;
std::map<std::string, bool> g_logged_messages;

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

void log_once(const std::string& key, const std::string& message) noexcept {
    try {
        if (g_logged_messages.emplace(key, true).second) log_line(message);
    } catch (...) {
    }
}

void* at(HMODULE module, std::uintptr_t rva) noexcept {
    return module
        ? static_cast<void*>(reinterpret_cast<std::uint8_t*>(module) + rva)
        : nullptr;
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) == 0 ||
        information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
        return false;
    }
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    const auto base = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress);
    return start >= base && size <= information.RegionSize - (start - base);
}

bool writable_range(const void* address, std::size_t size) noexcept {
    if (!readable_range(address, size)) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) == 0) {
        return false;
    }
    const DWORD protection = information.Protect & 0xffu;
    return protection == PAGE_READWRITE || protection == PAGE_WRITECOPY ||
        protection == PAGE_EXECUTE_READWRITE ||
        protection == PAGE_EXECUTE_WRITECOPY;
}

template <typename Value>
Value read_at(const void* object, std::size_t offset,
              Value fallback = Value{}) noexcept {
    const auto* address = object
        ? static_cast<const std::uint8_t*>(object) + offset : nullptr;
    if (!readable_range(address, sizeof(Value))) return fallback;
    Value value{};
    std::memcpy(&value, address, sizeof(value));
    return value;
}

template <typename Value>
bool write_at(void* object, std::size_t offset, const Value& value) noexcept {
    auto* address = object
        ? static_cast<std::uint8_t*>(object) + offset : nullptr;
    if (!writable_range(address, sizeof(Value))) return false;
    std::memcpy(address, &value, sizeof(value));
    return true;
}

template <std::size_t Size>
bool signature_matches(std::uintptr_t rva,
                       const std::array<std::uint8_t, Size>& expected) {
    const void* address = at(g_armada, rva);
    return readable_range(address, Size) &&
        std::memcmp(address, expected.data(), Size) == 0;
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    return value;
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

bool has_odf_extension(const std::string& name) {
    const std::string lowered = lower_ascii(name);
    return lowered.size() >= 4 &&
        lowered.compare(lowered.size() - 4, 4, ".odf") == 0;
}

void index_odf_directory(const std::string& directory,
                         std::size_t depth) {
    if (depth > 64) return;
    WIN32_FIND_DATAA data{};
    const std::string pattern = join_path(directory, "*");
    HANDLE search = FindFirstFileA(pattern.c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) return;

    std::vector<std::pair<std::string, bool>> children;
    do {
        const std::string name = data.cFileName;
        if (name == "." || name == "..") continue;
        const bool directory_entry =
            (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        if (directory_entry &&
            (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            continue;
        }
        if (directory_entry || has_odf_extension(name)) {
            children.emplace_back(name, directory_entry);
        }
    } while (FindNextFileA(search, &data));
    FindClose(search);

    std::sort(children.begin(), children.end(),
              [](const auto& left, const auto& right) {
                  return lower_ascii(left.first) < lower_ascii(right.first);
              });
    for (const auto& child : children) {
        const std::string path = join_path(directory, child.first);
        if (child.second) {
            index_odf_directory(path, depth + 1);
        } else {
            g_odf_paths[lower_ascii(child.first)] = path;
        }
    }
}

void index_odf_files() {
    g_odf_paths.clear();
    if (!g_api || !g_api->extension_root_count ||
        !g_api->extension_root) {
        return;
    }
    const std::uint32_t root_count = g_api->extension_root_count();
    for (std::uint32_t index = 0; index < root_count; ++index) {
        const char* root = g_api->extension_root(index);
        if (!root || !*root) continue;
        index_odf_directory(join_path(root, "odf"), 0);
    }
}

bool read_small_text_file(const std::string& path, std::string* contents) {
    if (!contents) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(kMaximumOdfSize)) {
        log_once("oversized:" + path,
                 "Ignored oversized edit-menu ODF: " + path);
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::ostringstream stream;
    stream << input.rdbuf();
    *contents = stream.str();
    return input.good() || input.eof();
}

const a2fo::edit_menu::MenuNode* load_node(
    const std::string& requested_name) {
    const std::string key =
        a2fo::edit_menu::normalize_odf_name(requested_name);
    if (key.empty()) return nullptr;
    const auto cached = g_nodes.find(key);
    if (cached != g_nodes.end()) return cached->second.get();

    const auto selected = g_odf_paths.find(key);
    if (selected == g_odf_paths.end()) {
        log_once("missing:" + key,
                 "Could not resolve recursive edit-menu ODF '" + key + "'");
        return nullptr;
    }
    std::string contents;
    if (!read_small_text_file(selected->second, &contents)) {
        log_once("unreadable:" + key,
                 "Could not read recursive edit-menu ODF '" +
                     selected->second + "'");
        return nullptr;
    }
    try {
        auto node = std::make_unique<a2fo::edit_menu::MenuNode>();
        std::string error;
        if (!a2fo::edit_menu::parse_menu_node(
                contents, key, node.get(), &error)) {
            log_once("parse:" + key,
                     "Could not parse recursive edit-menu ODF '" + key +
                         "': " + error);
            return nullptr;
        }
        const auto inserted = g_nodes.emplace(key, std::move(node));
        return inserted.first->second.get();
    } catch (...) {
        log_once("cache:" + key,
                 "Could not cache recursive edit-menu ODF '" + key + "'");
        return nullptr;
    }
}

std::string safe_native_string(const char* value) {
    if (!value) return {};
    std::size_t length = 0;
    while (length < kMaximumNativeString) {
        if (!readable_range(value + length, 1)) return {};
        if (value[length] == '\0') return std::string(value, length);
        ++length;
    }
    return {};
}

std::string fallback_title(const a2fo::edit_menu::MenuNode& node) {
    if (!node.title.empty()) return node.title;
    std::string title = node.source_name;
    if (title.size() >= 4 &&
        title.compare(title.size() - 4, 4, ".odf") == 0) {
        title.resize(title.size() - 4);
    }
    return title;
}

char* duplicate_engine_string(const std::string& value) noexcept {
    if (value.empty() || !g_armada) return nullptr;
    using AllocateFn = void* (__cdecl*)(std::size_t);
    const auto allocate = reinterpret_cast<AllocateFn>(
        at(g_armada, kEngineOperatorNewRva));
    if (!allocate) return nullptr;
    void* memory = allocate(value.size() + 1);
    if (!memory) return nullptr;
    std::memcpy(memory, value.c_str(), value.size() + 1);
    return static_cast<char*>(memory);
}

void free_engine_string(char* value) noexcept {
    if (!value || !g_armada) return;
    using FreeFn = void (__cdecl*)(void*);
    const auto release = reinterpret_cast<FreeFn>(
        at(g_armada, kEngineOperatorDeleteRva));
    if (release) release(value);
}

void release_scratch_strings(NativeRootMenu* root) noexcept {
    if (!root) return;
    free_engine_string(root->source_name);
    free_engine_string(root->title);
    for (NativeBuildMenu& build_menu : root->build_menus) {
        free_engine_string(build_menu.source_name);
        free_engine_string(build_menu.title);
    }
    *root = NativeRootMenu{};
}

std::string object_class_name(std::string value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = value.find_last_not_of(" \t\r\n");
    value = value.substr(first, last - first + 1);
    std::replace(value.begin(), value.end(), '/', '\\');
    const std::size_t slash = value.find_last_of('\\');
    if (slash != std::string::npos) value.erase(0, slash + 1);
    if (value.size() > 4 &&
        lower_ascii(value.substr(value.size() - 4)) == ".odf") {
        value.resize(value.size() - 4);
    }
    return value;
}

void* resolve_object_class_by_name(const std::string& configured_name) {
    if (!g_armada) return nullptr;
    const std::string name = object_class_name(configured_name);
    if (name.empty()) return nullptr;
    using ResolveFn = void* (__cdecl*)(const char*);
    const auto resolve = reinterpret_cast<ResolveFn>(
        at(g_armada, kFindObjectClassByNameRva));
    return resolve ? resolve(name.c_str()) : nullptr;
}

std::uint32_t object_class_project_id(void* object_class) noexcept {
    void* project_id = read_at<void*>(
        object_class, kObjectClassProjectIdOffset, nullptr);
    return read_at<std::uint32_t>(project_id, 0, 0);
}

bool populate_scratch(const a2fo::edit_menu::MenuNode& node,
                      NativeRootMenu* output) {
    if (!output) return false;
    NativeRootMenu candidate{};
    candidate.source_name = duplicate_engine_string(node.source_name);
    candidate.title = duplicate_engine_string(fallback_title(node));
    if (!candidate.source_name || !candidate.title) {
        release_scratch_strings(&candidate);
        return false;
    }

    for (std::size_t index = 0;
         index < a2fo::edit_menu::kEntryCount; ++index) {
        if (node.build_items[index].empty()) continue;
        NativeBuildMenu& native = candidate.build_menus[index];
        native.source_name = duplicate_engine_string(node.build_items[index]);
        const a2fo::edit_menu::MenuNode* child =
            load_node(node.build_items[index]);
        const std::string title = child
            ? fallback_title(*child) : node.build_items[index];
        native.title = duplicate_engine_string(title);
        if (!native.source_name || !native.title) {
            release_scratch_strings(&candidate);
            return false;
        }
        if (!child) continue;
        native.force_to_neutral = child->force_to_neutral ? 1 : 0;
        if (child->is_submenu()) continue;

        for (std::size_t item = 0;
             item < a2fo::edit_menu::kEntryCount; ++item) {
            if (child->items[item].empty()) continue;
            void* item_class = resolve_object_class_by_name(
                child->items[item]);
            const std::uint32_t project_id =
                object_class_project_id(item_class);
            if (!item_class || project_id == 0) {
                log_once(
                    "unresolved-item:" + child->source_name + ":" +
                        child->items[item],
                    "Skipped unresolved edit-menu item '" +
                        child->items[item] + "' in '" +
                        child->source_name + "'");
                continue;
            }
            native.item_project_ids[item] = project_id;
            native.item_classes[item] = item_class;
        }
    }

    *output = candidate;
    return true;
}

NativeRootMenu* root_slot(std::int32_t index) noexcept {
    if (index < 0 ||
        index >= static_cast<std::int32_t>(
            a2fo::edit_menu::kEntryCount)) {
        return nullptr;
    }
    auto* roots = static_cast<NativeRootMenu*>(
        at(g_armada, kEditMenuRootSlotsRva));
    NativeRootMenu* slot = roots ? roots + index : nullptr;
    return writable_range(slot, sizeof(*slot)) ? slot : nullptr;
}

void reset_navigation_state() noexcept {
    g_active_edit_menu = nullptr;
    g_active_root = -1;
    g_scratch_installed = false;
    g_current_node.clear();
    g_breadcrumbs.clear();
    g_saved_root = NativeRootMenu{};
}

void restore_original_root() noexcept {
    if (!g_scratch_installed) {
        reset_navigation_state();
        return;
    }
    NativeRootMenu* slot = root_slot(g_active_root);
    if (slot) {
        release_scratch_strings(slot);
        std::memcpy(slot, &g_saved_root, sizeof(*slot));
    }
    reset_navigation_state();
}

bool install_node(const a2fo::edit_menu::MenuNode& node,
                  void* edit_menu, std::int32_t root_index) {
    NativeRootMenu candidate{};
    if (!populate_scratch(node, &candidate)) return false;
    NativeRootMenu* slot = root_slot(root_index);
    if (!slot) {
        release_scratch_strings(&candidate);
        return false;
    }

    if (!g_scratch_installed) {
        std::memcpy(&g_saved_root, slot, sizeof(g_saved_root));
        g_scratch_installed = true;
        g_active_edit_menu = edit_menu;
        g_active_root = root_index;
    } else {
        release_scratch_strings(slot);
    }
    std::memcpy(slot, &candidate, sizeof(*slot));
    g_current_node = node.source_name;
    return true;
}

bool ancestor_contains(const std::string& node) {
    return node == g_current_node ||
        std::find(g_breadcrumbs.begin(), g_breadcrumbs.end(), node) !=
            g_breadcrumbs.end();
}

void enter_selected_submenu(void* edit_menu) {
    const std::int32_t root_index = read_at<std::int32_t>(
        edit_menu, kEditMenuSelectedRootOffset, -1);
    const std::int32_t build_index = read_at<std::int32_t>(
        edit_menu, kEditMenuSelectedBuildItemOffset, -1);
    NativeRootMenu* slot = root_slot(root_index);
    if (!slot || build_index < 0 ||
        build_index >= static_cast<std::int32_t>(
            a2fo::edit_menu::kEntryCount)) {
        return;
    }
    if (g_scratch_installed &&
        (g_active_edit_menu != edit_menu || g_active_root != root_index)) {
        restore_original_root();
        slot = root_slot(root_index);
        if (!slot) return;
    }

    const std::string target_name = safe_native_string(
        slot->build_menus[static_cast<std::size_t>(build_index)].source_name);
    const a2fo::edit_menu::MenuNode* target = load_node(target_name);
    if (!target || !target->is_submenu()) return;

    const std::string target_key = target->source_name;
    std::string parent = g_current_node;
    if (!g_scratch_installed) {
        parent = a2fo::edit_menu::normalize_odf_name(
            safe_native_string(slot->source_name));
    }
    if (target_key == parent || ancestor_contains(target_key)) {
        log_once("cycle:" + target_key,
                 "Stopped recursive edit-menu cycle at '" +
                     target_key + "'");
        write_at(edit_menu, kEditMenuLevelOffset, std::int32_t{1});
        return;
    }
    if (g_breadcrumbs.size() + 1 >= kMaximumDepth) {
        log_once("depth:" + target_key,
                 "Stopped recursive edit menu at its 32-level safety limit");
        write_at(edit_menu, kEditMenuLevelOffset, std::int32_t{1});
        return;
    }

    g_breadcrumbs.push_back(parent);
    if (!install_node(*target, edit_menu, root_index)) {
        g_breadcrumbs.pop_back();
        log_once("install:" + target_key,
                 "Could not install recursive edit-menu node '" +
                     target_key + "'");
        write_at(edit_menu, kEditMenuLevelOffset, std::int32_t{1});
        return;
    }
    write_at(edit_menu, kEditMenuLevelOffset, std::int32_t{1});
    log_once("enter:" + target_key,
             "Recursive edit-menu navigation entered '" + target_key + "'");
}

void leave_recursive_submenu(void* edit_menu) {
    if (!g_scratch_installed || g_breadcrumbs.empty()) {
        restore_original_root();
        return;
    }
    if (g_breadcrumbs.size() == 1) {
        restore_original_root();
        write_at(edit_menu, kEditMenuLevelOffset, std::int32_t{1});
        return;
    }

    const std::string parent_key = g_breadcrumbs.back();
    g_breadcrumbs.pop_back();
    const a2fo::edit_menu::MenuNode* parent = load_node(parent_key);
    if (!parent || !install_node(*parent, edit_menu, g_active_root)) {
        restore_original_root();
        return;
    }
    write_at(edit_menu, kEditMenuLevelOffset, std::int32_t{1});
}

void process_level_transition(void* edit_menu,
                              std::int32_t before,
                              std::int32_t after) {
    if (!edit_menu) return;
    if (g_scratch_installed && g_active_edit_menu != edit_menu) {
        restore_original_root();
    }
    if (before == 1 && after == 2) {
        enter_selected_submenu(edit_menu);
        return;
    }
    if (g_scratch_installed && before == 1 && after == 0) {
        leave_recursive_submenu(edit_menu);
        return;
    }
    if (g_scratch_installed && (after < 0 || after > 2)) {
        restore_original_root();
    }
}

void __attribute__((fastcall)) edit_menu_update_hook(
    void* edit_menu, void*) noexcept {
    const std::int32_t before = read_at<std::int32_t>(
        edit_menu, kEditMenuLevelOffset, -1);
    a2fo_edit_menu_call_thiscall_0(
        g_edit_menu_update_hook.gateway, edit_menu);
    if (!g_runtime_ready || !edit_menu || g_processing_hook) return;
    const std::int32_t after = read_at<std::int32_t>(
        edit_menu, kEditMenuLevelOffset, -1);

    g_processing_hook = true;
    try {
        process_level_transition(edit_menu, before, after);
    } catch (...) {
        restore_original_root();
        log_once("runtime-exception",
                 "Recursive edit-menu navigation was reset after an exception");
    }
    g_processing_hook = false;
}

bool preflight_signatures() {
    // We overwrite only EditMenu::Update, so that entry must retain its exact
    // supported prologue. Fleet Operations is allowed to detour the public
    // class-name resolver and allocator entries: calling those entries
    // deliberately follows its active object database and memory manager.
    if (!signature_matches(kEditMenuUpdateRva, kExpectedEditMenuUpdate)) {
        log_line("Armada EditMenu::Update signature was not found");
        return false;
    }
    if (!readable_range(at(g_armada, kFindObjectClassByNameRva), 5) ||
        !readable_range(at(g_armada, kEngineOperatorNewRva), 5) ||
        !readable_range(at(g_armada, kEngineOperatorDeleteRva), 5)) {
        log_line("An edit-menu resolver or allocator entry is unreadable");
        return false;
    }
    return true;
}

bool install_runtime_hook() {
    if (!g_api || !g_api->install_inline_hook || !g_armada) return false;
    if (!preflight_signatures()) {
        log_line("Supported ArmadaL edit-menu signatures were not found; "
                 "runtime disabled");
        return false;
    }
    return g_api->install_inline_hook(
        at(g_armada, kEditMenuUpdateRva),
        reinterpret_cast<void*>(&edit_menu_update_hook),
        kExpectedEditMenuUpdate.size(), kExpectedEditMenuUpdate.data(),
        &g_edit_menu_update_hook);
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->install_inline_hook ||
        !api->extension_root_count || !api->extension_root) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    if (!g_armada) return false;

    try {
        index_odf_files();
    } catch (...) {
        log_line("Could not index edit-menu ODF files");
    }
    g_runtime_ready = install_runtime_hook();
    log_line(std::string("Recursive edit-menu module") +
             (g_runtime_ready
                  ? " initialized; buildItem targets may contain buildItem "
                    "commands recursively"
                  : " loaded with runtime disabled") +
             "; indexed " + std::to_string(g_odf_paths.size()) +
             " loose ODF basename(s)");
    // Inline hooks are process-lifetime patches. Even a failed setup keeps
    // this DLL resident so an installed hook can remain a safe pass-through.
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    if (g_scratch_installed) restore_original_root();
}
