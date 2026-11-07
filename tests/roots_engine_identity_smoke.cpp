// Headless PE identity regression for the Roots engine builds. Map images
// without calling their entry points; mutation fixtures are rejected before
// any engine code is executed.
#include "../sdk/include/a2fo_supported_armada.hpp"

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>

using a2fo::supported_armada::Identity;

namespace {
using Bytes = std::vector<char>;

bool read_file(const char* path, Bytes& bytes) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    bytes.assign(std::istreambuf_iterator<char>(input), {});
    return !bytes.empty();
}

bool pe_headers(const Bytes& bytes, const IMAGE_NT_HEADERS32*& nt) {
    if (bytes.size() < sizeof(IMAGE_DOS_HEADER)) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        static_cast<std::size_t>(dos->e_lfanew) + sizeof(IMAGE_NT_HEADERS32) > bytes.size())
        return false;
    nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(bytes.data() + dos->e_lfanew);
    return nt->Signature == IMAGE_NT_SIGNATURE &&
           nt->FileHeader.Machine == IMAGE_FILE_MACHINE_I386;
}

const IMAGE_SECTION_HEADER* find_section(const IMAGE_NT_HEADERS32* nt,
                                         const char* name) {
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (std::strncmp(reinterpret_cast<const char*>(sections[i].Name), name,
                         IMAGE_SIZEOF_SHORT_NAME) == 0)
            return sections + i;
    }
    return nullptr;
}

bool check_image(const char* path, Identity expected, const char* label) {
    HMODULE image = LoadLibraryExA(path, nullptr, DONT_RESOLVE_DLL_REFERENCES);
    if (!image) {
        std::fprintf(stderr, "%s: image mapping failed (%lu)\n", label,
                     static_cast<unsigned long>(GetLastError()));
        return false;
    }
    const Identity actual = a2fo::supported_armada::identify(image);
    FreeLibrary(image);
    if (actual != expected)
        std::fprintf(stderr, "%s: identity mismatch (got %u, expected %u)\n", label,
                     static_cast<unsigned>(actual), static_cast<unsigned>(expected));
    else
        std::printf("%s: PASS\n", label);
    return actual == expected;
}

bool check_rejected(Bytes bytes, std::size_t offset, const char* path,
                   const char* label, const char* temporary) {
    if (offset >= bytes.size()) {
        std::fprintf(stderr, "%s: mutation offset outside fixture\n", label);
        return false;
    }
    bytes[offset] ^= 1;
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!output) {
            std::fprintf(stderr, "%s: failed writing mutation of %s\n", label, path);
            return false;
        }
    }
    return check_image(temporary, Identity::unsupported, label);
}

bool section_bytes(const Bytes& bytes, const IMAGE_NT_HEADERS32* nt,
                   const char* name, std::size_t& offset, std::size_t& size) {
    const auto* section = find_section(nt, name);
    if (!section || section->PointerToRawData > bytes.size() ||
        section->SizeOfRawData > bytes.size() - section->PointerToRawData)
        return false;
    offset = section->PointerToRawData;
    size = section->SizeOfRawData;
    return size != 0;
}

bool mutate_profile(const char* source, const char* label, const char* temporary,
                    bool include_data) {
    Bytes original;
    if (!read_file(source, original)) {
        std::fprintf(stderr, "%s: cannot read %s\n", label, source);
        return false;
    }
    const IMAGE_NT_HEADERS32* nt = nullptr;
    if (!pe_headers(original, nt)) {
        std::fprintf(stderr, "%s: invalid PE fixture %s\n", label, source);
        return false;
    }
    const auto* text = find_section(nt, ".text");
    const auto* rdata = find_section(nt, ".rdata");
    const auto* test = find_section(nt, ".test");
    const auto* data = find_section(nt, ".data");
    if (!text || !rdata || !test || !data) {
        std::fprintf(stderr, "%s: required PE section missing\n", label);
        return false;
    }
    const auto* base = original.data();
    const std::size_t timestamp =
        reinterpret_cast<const char*>(&nt->FileHeader.TimeDateStamp) - base;
    const std::size_t image_size =
        reinterpret_cast<const char*>(&nt->OptionalHeader.SizeOfImage) - base;
    const std::size_t entry =
        reinterpret_cast<const char*>(&nt->OptionalHeader.AddressOfEntryPoint) - base;
    const std::size_t layout =
        reinterpret_cast<const char*>(&test->Misc.VirtualSize) - base;
    struct Mutation { std::size_t offset; const char* what; };
    const Mutation mutations[] = {
        {text->PointerToRawData, ".text fingerprint"},
        {rdata->PointerToRawData, ".rdata fingerprint"},
        {test->PointerToRawData, ".test fingerprint"},
        {image_size, "image size"},
        {layout, "section layout"},
        {entry, "entry point"},
        {timestamp, "timestamp"},
    };
    bool ok = true;
    char description[160];
    for (const auto& mutation : mutations) {
        std::snprintf(description, sizeof(description), "%s: reject changed %s",
                      label, mutation.what);
        ok = check_rejected(original, mutation.offset, source, description,
                            temporary) && ok;
    }
    if (include_data) {
        // Both September identities fingerprint initialized .data.
        std::snprintf(description, sizeof(description), "%s: reject changed initialized .data",
                      label);
        ok = check_rejected(original, data->PointerToRawData, source, description,
                            temporary) && ok;
    }
    return ok;
}

bool check_mixed(const Bytes& newer, const IMAGE_NT_HEADERS32* newer_nt,
                 const Bytes& older, const IMAGE_NT_HEADERS32* older_nt,
                 const char* newer_path, const char* older_path,
                 const char* temporary, const char* newer_section,
                 const char* older_section, const char* label) {
    std::size_t newer_offset = 0, newer_size = 0, older_offset = 0, older_size = 0;
    if (!section_bytes(newer, newer_nt, newer_section, newer_offset, newer_size) ||
        !section_bytes(older, older_nt, older_section, older_offset, older_size)) {
        std::fprintf(stderr, "%s: cannot locate mixed sections (%s, %s)\n",
                     label, newer_path, older_path);
        return false;
    }
    Bytes hybrid = newer;
    const auto count = (std::min)(newer_size, older_size);
    std::copy_n(older.data() + older_offset, count, hybrid.data() + newer_offset);
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(hybrid.data(), static_cast<std::streamsize>(hybrid.size()));
        if (!output) return false;
    }
    return check_image(temporary, Identity::unsupported, label);
}

bool check_mixed_data_code(const Bytes& target, const IMAGE_NT_HEADERS32* target_nt,
                           const Bytes& source, const IMAGE_NT_HEADERS32* source_nt,
                           const char* target_path, const char* source_path,
                           const char* temporary, const char* label) {
    Bytes hybrid = target;
    std::size_t target_offset = 0, target_size = 0;
    std::size_t source_offset = 0, source_size = 0;
    if (!section_bytes(target, target_nt, ".data", target_offset, target_size) ||
        !section_bytes(source, source_nt, ".data", source_offset, source_size)) {
        std::fprintf(stderr, "%s: cannot locate .data in %s or %s\n",
                     label, target_path, source_path);
        return false;
    }
    // Preserve destination .text and .test so only initialized .data comes
    // from the other profile.
    std::copy_n(source.data() + source_offset,
                (std::min)(target_size, source_size),
                hybrid.data() + target_offset);
    {
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(hybrid.data(), static_cast<std::streamsize>(hybrid.size()));
        if (!output) return false;
    }
    return check_image(temporary, Identity::unsupported, label);
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 3 && argc != 4) {
        std::fprintf(stderr, "Usage: roots_engine_identity_smoke.exe NEW_SEP30_EXE OLD_SEP05_EXE [CANONICAL_EXE]\n");
        return 2;
    }
    bool ok = check_image(argv[1], Identity::janb_20260930, "Sept 30 engine") &&
        check_image(argv[2], Identity::janb_20260905, "Sept 5 engine");
    if (argc == 4) ok = check_image(argv[3], Identity::canonical, "canonical engine") && ok;
    else std::puts("Canonical engine fixture unavailable: SKIP");

    Bytes newer, older;
    const IMAGE_NT_HEADERS32 *newer_nt = nullptr, *older_nt = nullptr;
    if (!read_file(argv[1], newer) || !read_file(argv[2], older) ||
        !pe_headers(newer, newer_nt) || !pe_headers(older, older_nt)) {
        std::fprintf(stderr, "Cannot read new and old engine fixtures\n");
        return 3;
    }
    char directory[MAX_PATH]{}, temporary[MAX_PATH]{};
    if (!GetTempPathA(MAX_PATH, directory) ||
        !GetTempFileNameA(directory, "a2r", 0, temporary)) return 4;

    ok = mutate_profile(argv[1], "Sept 30", temporary, true) && ok;
    ok = mutate_profile(argv[2], "Sept 5", temporary, true) && ok;
    ok = check_mixed(newer, newer_nt, older, older_nt, argv[1], argv[2], temporary,
                     ".text", ".text", "Reject Sept 5 .text mixed into Sept 30") && ok;
    ok = check_mixed(newer, newer_nt, older, older_nt, argv[1], argv[2], temporary,
                     ".test", ".test", "Reject Sept 5 .test mixed into Sept 30") && ok;
    ok = check_mixed(older, older_nt, newer, newer_nt, argv[2], argv[1], temporary,
                     ".text", ".text", "Reject Sept 30 .text mixed into Sept 5") && ok;
    ok = check_mixed(older, older_nt, newer, newer_nt, argv[2], argv[1], temporary,
                     ".test", ".test", "Reject Sept 30 .test mixed into Sept 5") && ok;
    ok = check_mixed_data_code(older, older_nt, newer, newer_nt, argv[2], argv[1],
        temporary, "Reject Sept 30 .data mixed into Sept 5") && ok;
    ok = check_mixed_data_code(newer, newer_nt, older, older_nt, argv[1], argv[2],
        temporary, "Reject Sept 5 .data mixed into Sept 30") && ok;
    DeleteFileA(temporary);
    return ok ? 0 : 1;
}
