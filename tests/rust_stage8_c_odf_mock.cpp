// Full-source, no-game recursive ODF dependency mock. Both variants include
// their real odf_recursive.cpp and use the same fake FOFS/Win32 filesystem.
#include <windows.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <map>
#include <cstdint>

static DWORD fake_tick_count() { static DWORD tick = 100; return tick += 7; }
#define GetTickCount fake_tick_count
#include A2FO_STAGE8_ODF_SOURCE
#undef GetTickCount

namespace {
HMODULE fixture_fleetops = nullptr;
void* fixture_filesystem = nullptr;
A2FO_FofsItemLookupHandler fixture_lookup = nullptr;
std::vector<void*> fixture_created;
std::uint8_t fixture_entry[0x2c]{};
std::uint8_t fixture_root[16]{};
std::uint8_t fixture_directory[0x20]{};
DelphiList fixture_empty{};
DelphiList fixture_new_items{};
void* fixture_new_item_slots[8]{};
int failures = 0;
int hash_calls = 0;
int disk_calls = 0;
int queue_calls = 0;
int upgrade_calls = 0;
int buildyard_calls = 0;
bool registration_fails = false;

void expect(const char* name, bool good) {
    std::printf("case %s %s\n", name, good ? "ok" : "FAIL");
    if (!good) ++failures;
}
void A2FO_CALL logger(const char*, const char* message) {
    std::printf("log %s\n", message);
}
void* A2FO_CALL armada_module() { return reinterpret_cast<void*>(0x1234); }
void* A2FO_CALL fleetops_module() { return fixture_fleetops; }
const char* A2FO_CALL root_directory() { return "."; }
bool A2FO_CALL register_lookup(const char*, A2FO_FofsItemLookupHandler handler,
                               void*) {
    if (registration_fails) {
        std::puts("native register_lookup_failed");
        return false;
    }
    fixture_lookup = handler;
    std::puts("native register_lookup");
    return true;
}
void make_fleetops_image() {
    fixture_fleetops = static_cast<HMODULE>(VirtualAlloc(nullptr, 0x322000,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    auto* image = reinterpret_cast<std::uint8_t*>(fixture_fleetops);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    dos->e_magic = IMAGE_DOS_SIGNATURE;
    dos->e_lfanew = 0x100;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(image + 0x100);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.TimeDateStamp = 0x51f6475c;
    nt->OptionalHeader.SizeOfImage = 0x322000;
    std::memcpy(image + kHashStringRva, kExpectedHashString,
                sizeof(kExpectedHashString));
    std::memcpy(image + kAddFileToHashTableRva, kExpectedAddFileToHash,
                sizeof(kExpectedAddFileToHash));
    std::memcpy(image + kAddItemsFromDiskRva, kExpectedAddDisk,
                sizeof(kExpectedAddDisk));
    std::memcpy(image + kAddItemsFromPackRva, kExpectedAddPack,
                sizeof(kExpectedAddPack));
}
void assign_ptr(void* base, std::size_t offset, void* pointer) {
    std::memcpy(static_cast<std::uint8_t*>(base)+offset, &pointer,
                sizeof(pointer));
}
void assign_u32(void* base, std::size_t offset, std::uint32_t value) {
    std::memcpy(static_cast<std::uint8_t*>(base)+offset, &value,
                sizeof(value));
}
void make_file(const char* path) {
    HANDLE file = CreateFileA(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) { ++failures; return; }
    DWORD written = 0;
    WriteFile(file, "odf", 3, &written, nullptr);
    CloseHandle(file);
}
} // namespace

namespace a2fo {
std::string recursive_odf_basename_key(const std::string& name) {
    const auto slash = name.find_last_of("\\/");
    std::string key = slash == std::string::npos ? name : name.substr(slash + 1);
    for (char& c : key) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (key.size() < 4 || key.substr(key.size()-4) != ".odf") return {};
    return key;
}
FpqPathResult parse_fpq_odf_directories(const std::vector<std::uint8_t>&) {
    return {false,0,0,{},"mock unavailable"};
}
bool initialize_queue_enhancements(const A2FO_ModuleApi*, HMODULE, HMODULE) noexcept {
    ++queue_calls; std::puts("native init_queue"); return true;
}
bool initialize_upgrade_pods(const A2FO_ModuleApi*, HMODULE, HMODULE) {
    ++upgrade_calls; std::puts("native init_upgrade"); return true;
}
bool initialize_buildyard_pseudo_technology(const A2FO_ModuleApi*, HMODULE,
                                             HMODULE) noexcept {
    ++buildyard_calls; std::puts("native init_buildyard"); return true;
}
} // namespace a2fo

extern "C" {
void a2fo_odf_lstr_from_pchar(void*, void** output, const char* text) {
    *output = const_cast<char*>(text);
    std::puts("native lstr_from_pchar");
}
void a2fo_odf_lstr_clear(void*, void** value) {
    *value = nullptr; std::puts("native lstr_clear");
}
void* a2fo_odf_create_virtual_directory(void*, void*, void* value,
                                         std::uint32_t, std::uint32_t) {
    auto* directory = new std::uint8_t[0x20]{};
    assign_ptr(directory, 4, value);
    fixture_new_items.items = fixture_new_item_slots;
    fixture_new_items.capacity = 8;
    assign_ptr(directory, 0x0c, &fixture_new_items);
    fixture_created.push_back(directory);
    std::printf("native create_directory %s\n", static_cast<char*>(value));
    return directory;
}
int a2fo_odf_tlist_add(void*, void* list, void* item) {
    auto* entries = static_cast<DelphiList*>(list);
    entries->items[entries->count++] = item;
    std::puts("native tlist_add");
    return entries->count - 1;
}
std::uint32_t a2fo_odf_hash_string(void*, void* text) {
    const auto* name = static_cast<const unsigned char*>(text);
    std::uint32_t hash = 0;
    for (; *name; ++name) hash = hash * 33 + *name;
    ++hash_calls;
    std::printf("native hash %s\n", static_cast<const char*>(text));
    return hash;
}
void a2fo_odf_add_file_to_hash(void*, void* fs, void* entry,
                               std::uint32_t index) {
    auto** buckets = field<void**>(fs, 0x0c);
    assign_ptr(entry, kFileEntryNextOffset, buckets[index]);
    buckets[index] = entry;
    std::printf("native add_hash %u\n", index);
}
void a2fo_odf_add_items_from_disk(void*, void*, void*, void*,
                                  std::uint32_t* count, std::uint32_t primary) {
    ++disk_calls;
    ++*count;
    fixture_new_item_slots[fixture_new_items.count++] = fixture_entry;
    std::printf("native add_disk %u\n", primary);
}
void a2fo_odf_add_items_from_pack(void*, void*, void*, void*, std::uint32_t*) {
    std::puts("native add_pack");
}
void a2fo_odf_renew_overrides(void*, void*) { std::puts("native renew"); }
}

int main() {
    make_fleetops_image();
    expect("image", fixture_fleetops != nullptr);
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = logger;
    api.armada_module = armada_module;
    api.fleetops_module = fleetops_module;
    api.root_directory = root_directory;
    api.register_fofs_item_lookup_handler = register_lookup;
    expect("init_null", !A2FO_ModuleInit(nullptr));
    auto bad = api; bad.api_version = 0;
    expect("init_version", !A2FO_ModuleInit(&bad));
    auto* image = reinterpret_cast<std::uint8_t*>(fixture_fleetops);
    const auto magic = reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_magic;
    reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_magic = 0;
    expect("init_bad_dos", !A2FO_ModuleInit(&api));
    reinterpret_cast<IMAGE_DOS_HEADER*>(image)->e_magic = magic;
    const auto signature = image[kHashStringRva];
    image[kHashStringRva] = 0;
    expect("init_bad_signature", !A2FO_ModuleInit(&api));
    image[kHashStringRva] = signature;
    registration_fails = true;
    expect("init_register_failure", !A2FO_ModuleInit(&api));
    registration_fails = false;
    expect("init_success", A2FO_ModuleInit(&api));
    expect("init_calls", queue_calls==1 && upgrade_calls==1 &&
                         buildyard_calls==1 && fixture_lookup);

    expect("path_join", join_path("foo", "bar") == "foo\\bar");
    expect("path_absolute", is_absolute_path("C:/foo"));
    expect("odf_yes", has_odf_extension("Mixed.ODF"));
    expect("odf_no", !has_odf_extension("Mixed.ODF.bak"));
    std::string tail = "C:\\root\\\\";
    trim_trailing_slashes(tail);
    expect("trim_slash", tail == "C:\\root");
    std::vector<void*> bucket;
    std::uint8_t invalid_fs[0x30]{};
    expect("invalid_bucket", !collect_hash_bucket(invalid_fs, 0, bucket));
    expect("id_missing", a2fo::resolve_featurepack_odf_project_id("missing") == 0);
    std::vector<std::uint8_t> metadata;
    make_file("odf_invalid.fpq");
    expect("invalid_fpq", !read_fpq_metadata("odf_invalid.fpq", metadata));

    CreateDirectoryA("odf_fixture", nullptr);
    CreateDirectoryA("odf_fixture\\odf", nullptr);
    CreateDirectoryA("odf_fixture\\odf\\nested", nullptr);
    make_file("odf_fixture\\odf\\nested\\tester.odf");
    std::map<std::string,OdfDirectoryCandidate> candidates;
    scan_loose_odfs("odf_fixture\\odf", "odf", A2FO_ODF_OVERLAY_NORMAL,
                    candidates);
    expect("scan_nested", candidates.count("odf\\nested") == 1);
    scan_loose_odfs("odf_fixture\\odf", "odf", A2FO_ODF_OVERLAY_OVERRIDE,
                    candidates);
    expect("scan_overlay_precedence",
           candidates.at("odf\\nested").precedence ==
               A2FO_ODF_OVERLAY_OVERRIDE);
    std::uint8_t precedence_left[0x2c]{}, precedence_right[0x2c]{};
    std::map<void*,CustomEntryInfo> no_custom_entries;
    precedence_left[kFileEntryPrimaryRootOffset] = 1;
    expect("precedence_primary", file_entry_precedes(
        precedence_left, precedence_right, no_custom_entries));
    precedence_right[kFileEntryPrimaryRootOffset] = 1;
    precedence_right[kFileEntryPackedOffset] = 1;
    expect("precedence_loose", file_entry_precedes(
        precedence_left, precedence_right, no_custom_entries));
    std::uint8_t mod_info[0x40]{};
    assign_ptr(precedence_left, kFileEntryModInfoOffset, mod_info);
    expect("precedence_mod", file_entry_precedes(
        precedence_left, precedence_right, no_custom_entries));

    void* directory_slots[256]{};
    for (int i=0;i<28;++i) directory_slots[i] = fixture_directory;
    DelphiList directories{nullptr,directory_slots,28,256};
    assign_ptr(fixture_directory, 4, const_cast<char*>("built_in"));
    assign_ptr(fixture_directory, 0x0c, &fixture_empty);
    void* root_slots[] = {fixture_root};
    DelphiList roots{nullptr,root_slots,1,1};
    assign_ptr(fixture_root, 4, const_cast<char*>("odf_fixture"));
    void* buckets[8]{};
    struct Registry { std::int32_t capacity; void* items[8]; } registry{8,{}};
    std::uint8_t fs[0x30]{};
    assign_ptr(fs, 4, &directories);
    assign_u32(fs, 8, 8);
    assign_ptr(fs, 0x0c, buckets);
    assign_u32(fs, 0x10, 0);
    assign_ptr(fs, 0x14, registry.items);
    assign_ptr(fs, 0x18, &roots);
    assign_u32(fs, 0x20, 1);
    fixture_filesystem = fs;
    assign_ptr(fixture_entry, kFileEntryBasenameOffset,
               const_cast<char*>("tester.odf"));
    assign_ptr(fixture_entry, 0x24, fixture_root);
    expect("register", register_recursive_odfs_once(fs));
    expect("disk_scanned", disk_calls == 1);
    expect("hash_added", hash_calls >= 1);
    expect("winner_published", g_recursive_odf_winners.count("tester.odf") == 1);
    void* resolved = nullptr;
    expect("lookup", fixture_lookup(fs, const_cast<char*>("tester.odf"),
                                    0, &resolved, nullptr) &&
                     resolved == fixture_entry);
    expect("lookup_id", field<std::uint32_t>(fixture_entry,
                                              kFileEntryProjectIdOffset) == 1);
    expect("resolve_id", a2fo::resolve_featurepack_odf_project_id("tester") == 1);
    expect("resolve_trim", a2fo::resolve_featurepack_odf_project_id(
        "  nested/tester  ") == 1);
    expect("lookup_null_output", !fixture_lookup(fs,
        const_cast<char*>("tester.odf"), 0, nullptr, nullptr));
    expect("register_cached", register_recursive_odfs_once(fs));
    assign_ptr(fixture_entry, kFileEntryNextOffset, fixture_entry);
    bucket.clear();
    expect("hash_cycle", !collect_hash_bucket(fs,
        a2fo_odf_hash_string(nullptr,const_cast<char*>("tester.odf")) & 7,
        bucket) && bucket.empty());
    assign_ptr(fixture_entry, kFileEntryNextOffset, nullptr);
    std::uint8_t late_entry[0x2c]{};
    assign_u32(fs, 0x10, 8);
    bool added = true;
    expect("project_id_full", !ensure_fleetops_project_id(
        fs, late_entry, added) && !added);
    std::printf("counters %d %d %d %d\n", hash_calls, disk_calls,
                queue_calls, failures);
    return failures ? 3 : 0;
}
