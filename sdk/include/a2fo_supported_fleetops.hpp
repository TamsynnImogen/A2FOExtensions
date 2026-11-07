#pragma once

#include "a2fo_supported_armada.hpp"

namespace a2fo::supported_fleetops {

// A reviewed data profile admits only this existing PE/ABI layout family.
// Profiles contain fingerprints, never addresses, patch bytes or ABI options.
inline bool approved_profile(HMODULE module) noexcept {
    using supported_armada::readable_header;
    if (!module || !readable_header(module, 0, sizeof(IMAGE_DOS_HEADER))) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(module);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        !readable_header(module, dos->e_lfanew, sizeof(IMAGE_NT_HEADERS32))) return false;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        reinterpret_cast<const unsigned char*>(module) + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.Characteristics != 0xa3ae ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->FileHeader.SizeOfOptionalHeader != sizeof(IMAGE_OPTIONAL_HEADER32) ||
        nt->OptionalHeader.ImageBase != 0x5a800000 ||
        nt->OptionalHeader.AddressOfEntryPoint != 0x201988 ||
        nt->OptionalHeader.SizeOfImage != 0x322000 ||
        nt->OptionalHeader.SectionAlignment != 0x1000 || nt->FileHeader.NumberOfSections != 8)
        return false;
    struct Section {
        const char* name;
        DWORD rva, virtual_size, raw, size, flags;
    };
    constexpr std::array<Section,8> layout{{
        {".text", 0x1000, 0x1fd054, 0x400, 0x1fd200, 0x60000020},
        {".itext", 0x1ff000, 0x2d84, 0x1fd600, 0x2e00, 0x60000020},
        {".data", 0x202000, 0x11288, 0x200400, 0x11400, 0xc0000040},
        {".bss", 0x214000, 0x36420, 0x211800, 0, 0xc0000000},
        {".idata", 0x24b000, 0x5000, 0x211800, 0x5000, 0xc0000040},
        {".edata", 0x250000, 0x53, 0x216800, 0x200, 0x40000040},
        {".reloc", 0x251000, 0x20f30, 0x216a00, 0x21000, 0x42000040},
        {".rsrc", 0x272000, 0xaf648, 0x237a00, 0xaf800, 0x40000040},
    }};
    const auto table = static_cast<std::size_t>(dos->e_lfanew) + 24 + nt->FileHeader.SizeOfOptionalHeader;
    if (!readable_header(module, table, sizeof(IMAGE_SECTION_HEADER) * layout.size())) return false;
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (std::size_t i = 0; i < layout.size(); ++i) {
        const auto& expected = layout[i];
        const auto& actual = sections[i];
        if (std::strncmp(reinterpret_cast<const char*>(actual.Name), expected.name, 8) ||
            actual.VirtualAddress != expected.rva || actual.Misc.VirtualSize != expected.virtual_size ||
            actual.PointerToRawData != expected.raw || actual.SizeOfRawData != expected.size ||
            actual.Characteristics != expected.flags) return false;
    }
    std::array<char,32768> path{};
    const DWORD length = GetModuleFileNameA(module,path.data(),path.size());
    if (!length || length >= path.size()) return false;
    WIN32_FILE_ATTRIBUTE_DATA info{};
    constexpr DWORD file_size = 3043840;
    if (!GetFileAttributesExA(path.data(),GetFileExInfoStandard,&info) ||
        info.nFileSizeHigh || info.nFileSizeLow != file_size) return false;
    char* slash = std::strrchr(path.data(),'\\');
    char* forward = std::strrchr(path.data(),'/');
    if (forward && (!slash || forward > slash)) slash = forward;
    if (!slash) return false;
    constexpr char filename[] = "A2FOFleetOpsProfiles.txt";
    const auto prefix = static_cast<std::size_t>(slash+1-path.data());
    if (prefix+sizeof(filename) > path.size()) return false;
    std::memcpy(path.data()+prefix,filename,sizeof(filename));
    HANDLE file = CreateFileA(path.data(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    std::array<char,8193> content{};
    DWORD count = 0;
    const bool read = GetFileSizeEx(file,&size) && size.QuadPart > 0 && size.QuadPart < 8193 &&
        ReadFile(file,content.data(),static_cast<DWORD>(size.QuadPart),&count,nullptr) &&
        count == size.QuadPart;
    CloseHandle(file);
    if (!read || std::memchr(content.data(),0,count)) return false;
    bool version = false, matched = false, fingerprinted = false;
    unsigned rows = 0;
    std::uint32_t crc = 0;
    char* cursor = content.data();
    while (*cursor) {
        char* line = cursor;
        while (*cursor && *cursor != '\n') ++cursor;
        if (*cursor) *cursor++ = 0;
        if (char* comment = std::strchr(line,'#')) *comment = 0;
        std::array<char*,6> tokens{};
        unsigned n = 0;
        while (*line) {
            while (*line == ' ' || *line == '\t' || *line == '\r') ++line;
            if (!*line) break;
            if (n == tokens.size()) return false;
            tokens[n++] = line;
            while (*line && *line != ' ' && *line != '\t' && *line != '\r') ++line;
            if (*line) *line++ = 0;
        }
        if (!n) continue;
        if (!version) {
            if (n != 1 || std::strcmp(tokens[0],"A2FO_FLEETOPS_PROFILES_V1")) return false;
            version = true;
            continue;
        }
        if (++rows > 32 || n != 5 || std::strcmp(tokens[0],"roots_fleetops")) return false;
        std::array<std::uint32_t,4> values{};
        for (unsigned i=0;i<values.size();++i) {
            if (std::strlen(tokens[i+1]) != 8) return false;
            for (unsigned j=0;j<8;++j) {
                const char c=tokens[i+1][j];
                const unsigned d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:
                    c>='A'&&c<='F'?c-'A'+10:16;
                if (d == 16) return false;
                values[i]=(values[i]<<4)|d;
            }
        }
        if (values[1] != 0x322000 || values[2] != file_size) return false;
        if (!fingerprinted) {
            if (!supported_armada::file_range_crc32(module,0,file_size,crc)) return false;
            fingerprinted=true;
        }
        matched = matched || (values[0] == nt->FileHeader.TimeDateStamp && values[3] == crc);
    }
    return version && matched;
}

} // namespace a2fo::supported_fleetops
