#pragma once

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdio>

namespace a2fo::supported_armada {

enum class Identity {
    unsupported,
    canonical,
    normalized,
    janb_20260905,
    janb_20260930,
    roots_profile,
};

inline bool uses_native_station_rotation(Identity identity) noexcept {
    return identity == Identity::janb_20260930 || identity == Identity::roots_profile;
}

constexpr std::uint32_t kCanonicalTimestamp = 0x3c4c76bd;
constexpr std::uint32_t kCanonicalImageSize = 0x00403999;
constexpr std::uint32_t kNormalizedImageSize = 0x00404000;
constexpr std::uint32_t kJanb20260905ImageSize = 0x00405000;
constexpr std::uint32_t kRdataRva = 0x002ae000;
constexpr std::uint32_t kRdataSize = 0x0003de90;
constexpr std::uint32_t kRdataFileOffset = 0x002ae000;
constexpr std::uint32_t kRdataCrc32 = 0x8511f68c;

struct SectionLayout {
    const char* name;
    std::uint32_t rva;
    std::uint32_t virtual_size;
};

inline std::uint32_t update_crc32(std::uint32_t crc,
                                  const std::uint8_t* bytes,
                                  std::size_t size) noexcept {
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= bytes[index];
        for (unsigned bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask =
                0u - static_cast<std::uint32_t>(crc & 1u);
            crc = (crc >> 1u) ^ (0xedb88320u & mask);
        }
    }
    return crc;
}

inline bool file_range_crc32(HMODULE module, std::uint32_t file_offset,
                             std::uint32_t size,
                             std::uint32_t& result) noexcept {
    std::array<char, 32768> path{};
    const DWORD path_length = GetModuleFileNameA(
        module, path.data(), static_cast<DWORD>(path.size()));
    if (path_length == 0 ||
        path_length >= static_cast<DWORD>(path.size())) {
        return false;
    }

    HANDLE file = CreateFileA(
        path.data(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER offset{};
    offset.QuadPart = file_offset;
    if (!SetFilePointerEx(file, offset, nullptr, FILE_BEGIN)) {
        CloseHandle(file);
        return false;
    }

    std::array<std::uint8_t, 4096> buffer{};
    std::uint32_t crc = 0xffffffffu;
    std::uint32_t remaining = size;
    while (remaining != 0) {
        const DWORD requested = static_cast<DWORD>(
            (std::min)(static_cast<std::size_t>(remaining), buffer.size()));
        DWORD read = 0;
        if (!ReadFile(file, buffer.data(), requested, &read, nullptr) ||
            read != requested) {
            CloseHandle(file);
            return false;
        }
        crc = update_crc32(crc, buffer.data(), read);
        remaining -= read;
    }
    CloseHandle(file);
    result = crc ^ 0xffffffffu;
    return true;
}

// A compatibility file is an explicit, reviewed approval of exact file bytes,
// never permission to scan for a similar prologue or ignore changed code.
// Its capabilities are bounded to the existing Roots native-rotation family;
// addresses, ABI layouts and hook signatures cannot be supplied by this file.
inline bool approved_roots_profile(HMODULE module,
                                   const IMAGE_NT_HEADERS32* nt) noexcept {
    struct RawLayout { std::uint32_t offset, size, flags; };
    constexpr std::array<RawLayout, 7> layout{{
        {0x1000, 0x2ad000, 0x60000020},
        {0x2ae000, 0x3e000, 0x40000040},
        {0x2ec000, 0x49000, 0xc0000040},
        {0x335000, 0x6000, 0xc0000040},
        {0x33b000, 0x7000, 0x40000040},
        {0x342000, 0x36000, 0x42000040},
        {0x378000, 0xa999, 0x60000020},
    }};
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (std::size_t i = 0; i < layout.size(); ++i) {
        if (sections[i].PointerToRawData != layout[i].offset ||
            sections[i].SizeOfRawData != layout[i].size ||
            sections[i].Characteristics != layout[i].flags) return false;
    }
    std::array<char, 32768> path{};
    const DWORD length = GetModuleFileNameA(module, path.data(), path.size());
    if (!length || length >= path.size()) return false;
    char* slash = std::strrchr(path.data(), '\\');
    char* forward = std::strrchr(path.data(), '/');
    if (forward && (!slash || forward > slash)) slash = forward;
    if (!slash) return false;
    constexpr char filename[] = "A2FOArmadaProfiles.txt";
    const auto prefix = static_cast<std::size_t>(slash + 1 - path.data());
    if (prefix + sizeof(filename) > path.size()) return false;
    std::memcpy(path.data() + prefix, filename, sizeof(filename));
    HANDLE file = CreateFileA(path.data(), GENERIC_READ, FILE_SHARE_READ,
                             nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    std::array<char, 8193> content{};
    DWORD read = 0;
    const bool read_ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 &&
        size.QuadPart < static_cast<LONGLONG>(content.size()) &&
        ReadFile(file, content.data(), static_cast<DWORD>(size.QuadPart), &read, nullptr) &&
        read == size.QuadPart;
    CloseHandle(file);
    if (!read_ok || std::memchr(content.data(), 0, read)) return false;
    bool version = false, matched = false, fingerprinted = false;
    unsigned profiles = 0;
    std::array<std::uint32_t, 8> actual{};
    char* cursor = content.data();
    while (*cursor) {
        char* line = cursor;
        while (*cursor && *cursor != '\n') ++cursor;
        if (*cursor) *cursor++ = 0;
        if (char* comment = std::strchr(line, '#')) *comment = 0;
        std::array<char*, 12> tokens{};
        unsigned count = 0;
        while (*line) {
            while (*line == ' ' || *line == '\t' || *line == '\r') ++line;
            if (!*line) break;
            if (count == tokens.size()) return false;
            tokens[count++] = line;
            while (*line && *line != ' ' && *line != '\t' && *line != '\r') ++line;
            if (*line) *line++ = 0;
        }
        if (!count) continue;
        if (!version) {
            if (count != 1 || std::strcmp(tokens[0], "A2FO_ARMADA_PROFILES_V1")) return false;
            version = true;
            continue;
        }
        if (++profiles > 32 || count != 11 ||
            std::strcmp(tokens[0], "roots_native_rotation")) return false;
        std::array<std::uint32_t, 10> values{};
        for (unsigned i = 0; i < values.size(); ++i) {
            if (std::strlen(tokens[i + 1]) != 8) return false;
            for (unsigned j = 0; j < 8; ++j) {
                const char c = tokens[i + 1][j];
                const unsigned digit = c >= '0' && c <= '9' ? c - '0' :
                    c >= 'a' && c <= 'f' ? c - 'a' + 10 :
                    c >= 'A' && c <= 'F' ? c - 'A' + 10 : 16;
                if (digit == 16) return false;
                values[i] = (values[i] << 4) | digit;
            }
        }
        if (values[1] != kJanb20260905ImageSize) return false;
        if (!fingerprinted) {
            if (!file_range_crc32(module, 0, 0x1000, actual[0])) return false;
            for (unsigned i = 0; i < layout.size(); ++i)
                if (!file_range_crc32(module, layout[i].offset, layout[i].size,
                                      actual[i + 1])) return false;
            fingerprinted = true;
        }
        bool row_matches = values[0] == nt->FileHeader.TimeDateStamp;
        for (unsigned i = 0; i < actual.size(); ++i)
            row_matches = row_matches && values[i + 2] == actual[i];
        matched = matched || row_matches;
    }
    return version && matched;
}

inline bool readable_header(HMODULE module, std::size_t offset,
                            std::size_t size) noexcept {
    const auto start = reinterpret_cast<std::uintptr_t>(module);
    if (offset > 0x1000 || size > 0x1000 - offset) return false;
    auto cursor = start + offset;
    const auto end = cursor + size;
    while (cursor < end) {
        MEMORY_BASIC_INFORMATION memory{};
        if (!VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) ||
            memory.State != MEM_COMMIT || memory.AllocationBase != module ||
            (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
            !(memory.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                               PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)))
            return false;
        const auto next = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
        if (next <= cursor) return false;
        cursor = next;
    }
    return true;
}

inline Identity identify(HMODULE module) noexcept {
    if (!module || !readable_header(module, 0, sizeof(IMAGE_DOS_HEADER))) return Identity::unsupported;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        dos->e_lfanew >= 0x1000 ||
        !readable_header(module, dos->e_lfanew, sizeof(IMAGE_NT_HEADERS32))) {
        return Identity::unsupported;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const std::uint8_t*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        return Identity::unsupported;
    }
    if (nt->FileHeader.TimeDateStamp == kCanonicalTimestamp &&
        nt->OptionalHeader.SizeOfImage == kCanonicalImageSize) {
        return Identity::canonical;
    }

    // Jan_B's September builds extend .test by one page. Their section RVAs
    // remain fixed, but the code fingerprints differ between engine updates.
    // Never accept an arbitrary larger image just because its timestamp agrees.
    const bool janb_september_layout =
        nt->OptionalHeader.SizeOfImage == kJanb20260905ImageSize;
    const std::array<SectionLayout, 7> expected_sections{{
        {".text", 0x00001000, 0x002ac6c4},
        {".rdata", 0x002ae000, 0x0003de90},
        {".data", 0x002ec000, 0x000ca1b8},
        {".idata", 0x003b7000, 0x000055fa},
        {".rsrc", 0x003bd000, 0x00006bb0},
        {".reloc", 0x003c4000, 0x00036000},
        {".test", 0x003fa000, janb_september_layout ? 0x0000a999u : 0x00009999u},
    }};
    if ((!janb_september_layout &&
         nt->OptionalHeader.SizeOfImage != kNormalizedImageSize) ||
        nt->OptionalHeader.ImageBase != 0x00400000 ||
        nt->OptionalHeader.AddressOfEntryPoint != 0x002733c0 ||
        nt->OptionalHeader.SectionAlignment != 0x00001000 ||
        nt->FileHeader.NumberOfSections != expected_sections.size()) {
        return Identity::unsupported;
    }

    const auto section_offset = static_cast<std::size_t>(dos->e_lfanew) + 24 +
        nt->FileHeader.SizeOfOptionalHeader;
    if (nt->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER32) ||
        !readable_header(module, section_offset,
                         expected_sections.size() * sizeof(IMAGE_SECTION_HEADER)))
        return Identity::unsupported;

    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (std::size_t index = 0; index < expected_sections.size(); ++index) {
        if (std::strncmp(
                reinterpret_cast<const char*>(sections[index].Name),
                expected_sections[index].name,
                IMAGE_SIZEOF_SHORT_NAME) != 0 ||
            sections[index].VirtualAddress != expected_sections[index].rva ||
            sections[index].Misc.VirtualSize !=
                expected_sections[index].virtual_size) {
            return Identity::unsupported;
        }
    }
    if (sections[1].PointerToRawData != kRdataFileOffset ||
        sections[1].SizeOfRawData < kRdataSize) {
        return Identity::unsupported;
    }

    // External profiles bind the exact file header and every complete raw
    // section. Reviewed constant/resource/header changes within this fixed
    // layout family therefore do not require another DLL rebuild. Legacy
    // built-in identities still require their original fingerprints below.
    std::uint32_t rdata_crc32 = 0;
    if (!file_range_crc32(module, sections[1].PointerToRawData,
                          kRdataSize, rdata_crc32)) {
        return Identity::unsupported;
    }
    if (rdata_crc32 != (janb_september_layout ? 0xcd47b19cu : kRdataCrc32))
        return janb_september_layout && approved_roots_profile(module, nt)
            ? Identity::roots_profile : Identity::unsupported;
    if (janb_september_layout) {
        if (nt->FileHeader.TimeDateStamp != kCanonicalTimestamp)
            return approved_roots_profile(module, nt)
                ? Identity::roots_profile : Identity::unsupported;
        // Read the file, not live memory: FleetOpsHook has already detoured
        // Armada before most callers reach this shared identity check.
        std::uint32_t text_crc32 = 0;
        std::uint32_t patch_crc32 = 0;
        if (sections[0].PointerToRawData != 0x00001000 ||
            sections[0].SizeOfRawData < 0x002ac6c4 ||
            sections[6].PointerToRawData != 0x00378000 ||
            sections[6].SizeOfRawData < 0x0000a999 ||
            !file_range_crc32(module, sections[0].PointerToRawData,
                              0x002ac6c4, text_crc32) ||
            !file_range_crc32(module, sections[6].PointerToRawData,
                              0x0000a999, patch_crc32)) {
            return Identity::unsupported;
        }
        // Match complete profiles: mixing code from different updates is not
        // supported. The September 30 update also changes initialized data
        // (including native rotation configuration and geometry constants).
        std::uint32_t data_crc32 = 0;
        if (sections[2].PointerToRawData != 0x002ec000 ||
            sections[2].SizeOfRawData != 0x00049000 ||
            !file_range_crc32(module, sections[2].PointerToRawData,
                              sections[2].SizeOfRawData, data_crc32)) {
            return Identity::unsupported;
        }
        if (text_crc32 == 0x5458da70 && patch_crc32 == 0x785c09e4 &&
            data_crc32 == 0x830d0160) {
            return Identity::janb_20260905;
        }
        if (text_crc32 == 0x98ed8dbd && patch_crc32 == 0x6592de0f &&
            data_crc32 == 0x3049ef39) {
            return Identity::janb_20260930;
        }
        if (approved_roots_profile(module, nt)) return Identity::roots_profile;
        return Identity::unsupported;
    }
    return Identity::normalized;
}

}  // namespace a2fo::supported_armada
