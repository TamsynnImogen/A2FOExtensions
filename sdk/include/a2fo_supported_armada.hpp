#pragma once

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

namespace a2fo::supported_armada {

enum class Identity {
    unsupported,
    canonical,
    normalized,
};

constexpr std::uint32_t kCanonicalTimestamp = 0x3c4c76bd;
constexpr std::uint32_t kCanonicalImageSize = 0x00403999;
constexpr std::uint32_t kNormalizedImageSize = 0x00404000;
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

inline Identity identify(HMODULE module) noexcept {
    if (!module) return Identity::unsupported;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        dos->e_lfanew >= 0x1000) {
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

    constexpr std::array<SectionLayout, 7> expected_sections{{
        {".text", 0x00001000, 0x002ac6c4},
        {".rdata", 0x002ae000, 0x0003de90},
        {".data", 0x002ec000, 0x000ca1b8},
        {".idata", 0x003b7000, 0x000055fa},
        {".rsrc", 0x003bd000, 0x00006bb0},
        {".reloc", 0x003c4000, 0x00036000},
        {".test", 0x003fa000, 0x00009999},
    }};
    if (nt->OptionalHeader.SizeOfImage != kNormalizedImageSize ||
        nt->OptionalHeader.ImageBase != 0x00400000 ||
        nt->OptionalHeader.AddressOfEntryPoint != 0x002733c0 ||
        nt->OptionalHeader.SectionAlignment != 0x00001000 ||
        nt->FileHeader.NumberOfSections != expected_sections.size()) {
        return Identity::unsupported;
    }

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

    std::uint32_t rdata_crc32 = 0;
    if (!file_range_crc32(module, sections[1].PointerToRawData,
                          kRdataSize, rdata_crc32) ||
        rdata_crc32 != kRdataCrc32) {
        return Identity::unsupported;
    }
    return Identity::normalized;
}

}  // namespace a2fo::supported_armada
