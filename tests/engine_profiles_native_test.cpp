// Headless fixture test: map private copies as data only; never run game code.
#include "../sdk/include/a2fo_supported_armada.hpp"
#include "../sdk/include/a2fo_supported_fleetops.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<char>;
using Identity = a2fo::supported_armada::Identity;
unsigned checks = 0;

Bytes read(const char* path) {
    std::ifstream file(path, std::ios::binary);
    return Bytes(std::istreambuf_iterator<char>(file), {});
}
bool write(const std::string& path, const Bytes& bytes) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(bytes.data(), bytes.size());
    return bool(file);
}
bool write(const std::string& path, const std::string& value) {
    return write(path, Bytes(value.begin(), value.end()));
}
void set32(Bytes& bytes, std::size_t offset, std::uint32_t value) {
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
std::uint32_t crc(const Bytes& bytes, std::size_t offset, std::size_t size) {
    return a2fo::supported_armada::update_crc32(0xffffffffu,
        reinterpret_cast<const std::uint8_t*>(bytes.data() + offset), size) ^ 0xffffffffu;
}
void append_hex(std::string& text, std::uint32_t value) {
    char field[16]{};
    std::snprintf(field, sizeof(field), " %08x", value);
    text += field;
}
const IMAGE_NT_HEADERS32* nt(const Bytes& bytes) {
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    return reinterpret_cast<const IMAGE_NT_HEADERS32*>(bytes.data() + dos->e_lfanew);
}
std::size_t offset(const Bytes& bytes, const void* field) {
    return reinterpret_cast<const char*>(field) - bytes.data();
}
std::string armada_profile(const Bytes& bytes) {
    const auto* header = nt(bytes);
    const auto* sections = IMAGE_FIRST_SECTION(header);
    std::string text = "A2FO_ARMADA_PROFILES_V1\nroots_native_rotation";
    append_hex(text, header->FileHeader.TimeDateStamp);
    append_hex(text, header->OptionalHeader.SizeOfImage);
    append_hex(text, crc(bytes, 0, 0x1000));
    for (unsigned i = 0; i < 7; ++i)
        append_hex(text, crc(bytes, sections[i].PointerToRawData, sections[i].SizeOfRawData));
    return text + '\n';
}
std::string fleet_profile(const Bytes& bytes) {
    std::string text = "A2FO_FLEETOPS_PROFILES_V1\nroots_fleetops";
    append_hex(text, nt(bytes)->FileHeader.TimeDateStamp);
    append_hex(text, nt(bytes)->OptionalHeader.SizeOfImage);
    append_hex(text, static_cast<std::uint32_t>(bytes.size()));
    append_hex(text, crc(bytes, 0, bytes.size()));
    return text + '\n';
}
bool expect_armada(const std::string& image, Identity wanted, const char* label) {
    HMODULE mapped = LoadLibraryExA(image.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!mapped) {
        std::fprintf(stderr, "FAIL map Armada %s: %lu\n", label, GetLastError());
        return false;
    }
    const auto actual = a2fo::supported_armada::identify(mapped);
    FreeLibrary(mapped);
    ++checks;
    std::printf("%s Armada %s\n", actual == wanted ? "PASS" : "FAIL", label);
    return actual == wanted;
}
bool expect_fleet(const std::string& image, bool wanted, const char* label) {
    HMODULE mapped = LoadLibraryExA(image.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!mapped) {
        std::fprintf(stderr, "FAIL map Fleet Ops %s: %lu\n", label, GetLastError());
        return false;
    }
    const bool actual = a2fo::supported_fleetops::approved_profile(mapped);
    FreeLibrary(mapped);
    ++checks;
    std::printf("%s Fleet Ops %s\n", actual == wanted ? "PASS" : "FAIL", label);
    return actual == wanted;
}
} // namespace

int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    if (argc != 3) {
        std::fprintf(stderr, "Usage: engine_profiles_native_test.exe OCT23_ARMADAL_EXE OCT23_FLEETOPSHOOK_DLL\n");
        return 2;
    }
    const Bytes armada = read(argv[1]), fleet = read(argv[2]);
    if (armada.size() != 4816251 || fleet.size() != 3043840 ||
        nt(armada)->FileHeader.NumberOfSections != 7 ||
        nt(fleet)->FileHeader.NumberOfSections != 8) {
        std::fprintf(stderr, "Expected the reviewed October 23 binary pair\n");
        return 3;
    }
    char temp[MAX_PATH]{}, dir[MAX_PATH]{};
    if (!GetTempPathA(MAX_PATH, temp) || !GetTempFileNameA(temp, "epn", 0, dir)) return 4;
    DeleteFileA(dir);
    if (!CreateDirectoryA(dir, nullptr)) return 4;
    const std::string base = dir;
    const std::string exe = base + "\\ArmadaL.exe", dll = base + "\\FleetOpsHook.dll";
    const std::string armada_manifest = base + "\\A2FOArmadaProfiles.txt";
    const std::string fleet_manifest = base + "\\A2FOFleetOpsProfiles.txt";
    const std::string armada_row = armada_profile(armada), fleet_row = fleet_profile(fleet);
    bool ok = write(exe, armada) && write(dll, fleet);
    ok = expect_armada(exe, Identity::unsupported, "absent approval") && ok;
    ok = expect_fleet(dll, false, "absent approval") && ok;
    ok = write(armada_manifest, armada_row) && write(fleet_manifest, fleet_row) && ok;
    ok = expect_armada(exe, Identity::roots_profile, "exact reviewed row") && ok;
    ok = expect_fleet(dll, true, "exact reviewed row") && ok;

    // A stale row cannot bless changed content, including complete raw tails.
    Bytes changed = armada;
    const auto* armada_sections = IMAGE_FIRST_SECTION(nt(armada));
    changed[armada_sections[0].PointerToRawData + 0x100] ^= 1;
    ok = write(exe, changed) && expect_armada(exe, Identity::unsupported, "changed code with stale row") && ok;
    for (unsigned i = 0; i < 7; ++i) {
        changed = armada;
        changed[armada_sections[i].PointerToRawData + armada_sections[i].SizeOfRawData - 1] ^= 1;
        ok = write(exe, changed) && expect_armada(exe, Identity::unsupported, "changed raw section tail") && ok;
    }
    changed = fleet;
    changed[IMAGE_FIRST_SECTION(nt(fleet))[0].PointerToRawData + 0x100] ^= 1;
    ok = write(dll, changed) && expect_fleet(dll, false, "changed code with stale row") && ok;
    changed = fleet;
    const auto& last = IMAGE_FIRST_SECTION(nt(fleet))[7];
    changed[last.PointerToRawData + last.SizeOfRawData - 1] ^= 1;
    ok = write(dll, changed) && expect_fleet(dll, false, "changed resource tail with stale row") && ok;

    // Refresh every digest after each mutation: geometry must remain compiled in.
    auto armada_bad_geometry = [&](Bytes mutant, const char* label) {
        return write(exe, mutant) && write(armada_manifest, armada_profile(mutant)) &&
            expect_armada(exe, Identity::unsupported, label);
    };
    auto fleet_bad_geometry = [&](Bytes mutant, const char* label) {
        return write(dll, mutant) && write(fleet_manifest, fleet_profile(mutant)) &&
            expect_fleet(dll, false, label);
    };
    changed = armada;
    set32(changed, offset(armada, &nt(armada)->OptionalHeader.AddressOfEntryPoint), 0x2733c1);
    ok = armada_bad_geometry(changed, "entry RVA despite refreshed row") && ok;
    changed = armada;
    set32(changed, offset(armada, &nt(armada)->OptionalHeader.ImageBase), 0x00500000);
    ok = armada_bad_geometry(changed, "image base despite refreshed row") && ok;
    changed = armada;
    set32(changed, offset(armada, &armada_sections[0].Characteristics), 0x40000020);
    ok = armada_bad_geometry(changed, "section flags despite refreshed row") && ok;
    changed = armada;
    set32(changed, offset(armada, &armada_sections[0].Misc.VirtualSize),
          armada_sections[0].Misc.VirtualSize + 1);
    ok = armada_bad_geometry(changed, "section geometry despite refreshed row") && ok;
    changed = fleet;
    set32(changed, offset(fleet, &nt(fleet)->OptionalHeader.AddressOfEntryPoint), 0x201989);
    ok = fleet_bad_geometry(changed, "entry RVA despite refreshed row") && ok;
    changed = fleet;
    set32(changed, offset(fleet, &nt(fleet)->OptionalHeader.ImageBase), 0x5a900000);
    ok = fleet_bad_geometry(changed, "image base despite refreshed row") && ok;
    changed = fleet;
    set32(changed, offset(fleet, &IMAGE_FIRST_SECTION(nt(fleet))[0].Characteristics), 0x40000020);
    ok = fleet_bad_geometry(changed, "section flags despite refreshed row") && ok;
    changed = fleet;
    set32(changed, offset(fleet, &IMAGE_FIRST_SECTION(nt(fleet))[0].Misc.VirtualSize),
          IMAGE_FIRST_SECTION(nt(fleet))[0].Misc.VirtualSize + 1);
    ok = fleet_bad_geometry(changed, "section geometry despite refreshed row") && ok;

    ok = write(exe, armada) && write(dll, fleet) && ok;
    const std::vector<std::string> bad_armada{
        "", "A2FO_ARMADA_PROFILES_V2\n" + armada_row,
        "A2FO_ARMADA_PROFILES_V1\n", armada_row + "bad row\n",
        armada_row + std::string("\0hidden", 7), armada_row + std::string(8200, '#'),
        "A2FO_ARMADA_PROFILES_V1\nunknown_family" + armada_row.substr(armada_row.find(' ')),
        armada_row.substr(0, armada_row.size() - 3) + "xx\n",
    };
    const std::vector<std::string> bad_fleet{
        "", "A2FO_FLEETOPS_PROFILES_V2\n" + fleet_row,
        "A2FO_FLEETOPS_PROFILES_V1\n", fleet_row + "bad row\n",
        fleet_row + std::string("\0hidden", 7), fleet_row + std::string(8200, '#'),
        "A2FO_FLEETOPS_PROFILES_V1\nunknown_family" + fleet_row.substr(fleet_row.find(' ')),
        fleet_row.substr(0, fleet_row.size() - 3) + "xx\n",
    };
    for (const auto& bad : bad_armada)
        ok = write(armada_manifest, bad) && expect_armada(exe, Identity::unsupported, "malformed manifest") && ok;
    for (const auto& bad : bad_fleet)
        ok = write(fleet_manifest, bad) && expect_fleet(dll, false, "malformed manifest") && ok;
    std::string excessive = "A2FO_ARMADA_PROFILES_V1\n";
    const auto armada_data_row = armada_row.substr(armada_row.find("roots_native_rotation"));
    for (unsigned i = 0; i < 33; ++i) excessive += armada_data_row;
    ok = write(armada_manifest, excessive) && expect_armada(exe, Identity::unsupported, "33 rows") && ok;
    excessive = "A2FO_FLEETOPS_PROFILES_V1\n";
    const auto fleet_data_row = fleet_row.substr(fleet_row.find("roots_fleetops"));
    for (unsigned i = 0; i < 33; ++i) excessive += fleet_data_row;
    ok = write(fleet_manifest, excessive) && expect_fleet(dll, false, "33 rows") && ok;

    // One future timestamp and resource-only update, accepted by new exact rows
    // in this same compiled test process without changing either checker.
    changed = armada;
    set32(changed, offset(armada, &nt(armada)->FileHeader.TimeDateStamp),
          nt(armada)->FileHeader.TimeDateStamp + 1);
    changed[armada_sections[4].PointerToRawData + 0x100] ^= 1;
    ok = write(exe, changed) && write(armada_manifest, armada_profile(changed)) &&
         expect_armada(exe, Identity::roots_profile, "future timestamp and resource update") && ok;
    const Bytes future_armada = changed;
    const std::string future_armada_row = armada_profile(future_armada).substr(armada_row.find("roots_native_rotation"));
    changed = fleet;
    set32(changed, offset(fleet, &nt(fleet)->FileHeader.TimeDateStamp),
          nt(fleet)->FileHeader.TimeDateStamp + 1);
    changed[IMAGE_FIRST_SECTION(nt(fleet))[7].PointerToRawData + 0x100] ^= 1;
    ok = write(dll, changed) && write(fleet_manifest, fleet_profile(changed)) &&
         expect_fleet(dll, true, "future timestamp and resource update") && ok;
    const Bytes future_fleet = changed;
    const std::string future_fleet_row = fleet_profile(future_fleet).substr(fleet_row.find("roots_fleetops"));

    // Either row order must approve both exact files, including a different timestamp.
    for (unsigned order = 0; order < 2; ++order) {
        const bool future_first = order != 0;
        const std::string armada_rows = "A2FO_ARMADA_PROFILES_V1\n" +
            (future_first ? future_armada_row + armada_data_row : armada_data_row + future_armada_row);
        const std::string fleet_rows = "A2FO_FLEETOPS_PROFILES_V1\n" +
            (future_first ? future_fleet_row + fleet_data_row : fleet_data_row + future_fleet_row);
        ok = write(armada_manifest, armada_rows) && write(fleet_manifest, fleet_rows) && ok;
        ok = write(exe, armada) && expect_armada(exe, Identity::roots_profile,
            future_first ? "future row first, original file" : "original row first, original file") && ok;
        ok = write(exe, future_armada) && expect_armada(exe, Identity::roots_profile,
            future_first ? "future row first, future file" : "original row first, future file") && ok;
        ok = write(dll, fleet) && expect_fleet(dll, true,
            future_first ? "future row first, original file" : "original row first, original file") && ok;
        ok = write(dll, future_fleet) && expect_fleet(dll, true,
            future_first ? "future row first, future file" : "original row first, future file") && ok;
    }

    DeleteFileA(armada_manifest.c_str()); DeleteFileA(fleet_manifest.c_str());
    DeleteFileA(exe.c_str()); DeleteFileA(dll.c_str()); RemoveDirectoryA(dir);
    std::printf("%u mapped-image checks: %s\n", checks, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}
