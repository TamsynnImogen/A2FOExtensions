// Headless PE identity regression: map fixtures without executing game code.
#include "../sdk/include/a2fo_supported_armada.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

using a2fo::supported_armada::Identity;

bool check_file(const char* path, Identity expected, const char* label) {
    HMODULE image = LoadLibraryExA(path, nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!image) {
        std::fprintf(stderr, "%s: PE mapping failed (%lu)\n", label,
                     static_cast<unsigned long>(GetLastError()));
        return false;
    }
    const Identity actual = a2fo::supported_armada::identify(image);
    FreeLibrary(image);
    std::printf("%s: %s\n", label, actual == expected ? "PASS" : "FAIL");
    return actual == expected;
}

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "Usage: supported_armada_smoke.exe NEW_EXE OLD_EXE\n");
        return 2;
    }
    bool ok = check_file(argv[1], Identity::janb_20260905, "September build") &&
        check_file(argv[2], Identity::canonical, "Previous canonical build");
    std::ifstream input(argv[1], std::ios::binary);
    const std::vector<char> original{
        std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    if (original.size() < 0x382999) return 3;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(original.data());
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(
        original.data() + dos->e_lfanew);
    const std::size_t image_size_offset =
        reinterpret_cast<const char*>(&nt->OptionalHeader.SizeOfImage) - original.data();
    const std::size_t patch_size_offset =
        reinterpret_cast<const char*>(&IMAGE_FIRST_SECTION(nt)[6].Misc.VirtualSize) -
        original.data();
    const std::size_t entry_offset =
        reinterpret_cast<const char*>(&nt->OptionalHeader.AddressOfEntryPoint) -
        original.data();
    const std::size_t timestamp_offset =
        reinterpret_cast<const char*>(&nt->FileHeader.TimeDateStamp) - original.data();
    struct Mutation { std::size_t offset; const char* label; };
    const Mutation mutations[] = {
        {0x1000, "Reject changed engine code"},
        {0x2ae020, "Reject changed read-only data"},
        {0x378000, "Reject changed patch code"},
        {image_size_offset + 1, "Reject unknown image size"},
        {patch_size_offset, "Reject changed patch-section layout"},
        {entry_offset, "Reject changed entry point"},
        {timestamp_offset, "Reject unknown September timestamp"},
    };
    char directory[MAX_PATH]{};
    char temporary[MAX_PATH]{};
    if (!GetTempPathA(MAX_PATH, directory) ||
        !GetTempFileNameA(directory, "a2i", 0, temporary)) return 4;
    for (const auto& mutation : mutations) {
        auto bytes = original;
        bytes[mutation.offset] ^= 1;
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            if (!output) { DeleteFileA(temporary); return 5; }
        }
        ok = check_file(temporary, Identity::unsupported, mutation.label) && ok;
    }
    DeleteFileA(temporary);
    return ok ? 0 : 1;
}
