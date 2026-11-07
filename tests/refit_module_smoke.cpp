// Real DLLs with a data-only mapped reference PE and private instruction
// fixtures. Never calls the PE entry point or launches a game/GUI/input path.
#include "../sdk/include/a2fo_module_api.h"
#include "../rust/native_hooks_ffi.h"
#include "../modules/refit_common/bindings.hpp"
#include "../modules/refit_common/settings.hpp"
#include "../modules/refit_common/status.hpp"
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

struct alignas(16) Capture {
    std::uint32_t registers[9];
    std::uint8_t padding[12];
    std::uint8_t before[512];
    std::uint8_t after[512];
};
static_assert(offsetof(Capture, before) == 48 && offsetof(Capture, after) == 560);
extern "C" {
void refit_run_fixture();
void* refit_target = nullptr;
Capture* refit_capture = nullptr;
std::uint32_t refit_parsed_value = 0;
int refit_pan_fixture = 0;
std::uint16_t refit_control_word = 0x37f;
void* refit_saved_sp = nullptr;
void* refit_saved_fx = nullptr;
}
namespace {
namespace rb = a2fo::refit::bindings;
HMODULE engine = nullptr;
std::string root;
std::string diagnostic;
void* bridge = nullptr;
unsigned patches = 0;
bool refuse_patch = false;
template<typename Function>
Function symbol(HMODULE module, const char* name) {
    FARPROC address = GetProcAddress(module, name);
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(address));
    std::memcpy(&function, &address, sizeof(function));
    return function;
}
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void A2FO_CALL logger(const char*, const char* text) { if (text) diagnostic += std::string(text) + '\n'; }
void* A2FO_CALL image() { return engine; }
const char* A2FO_CALL directory() { return root.c_str(); }
bool A2FO_CALL patch(void* target, void* replacement, const std::uint8_t* expected, std::size_t size) {
    ++patches;
    if (refuse_patch) return false;
    bridge = replacement;
    return a2fo_rs_windows_patch_call(target, replacement, expected, size);
}
std::uint32_t word(std::uint32_t rva) {
    std::uint32_t value = 0;
    std::memcpy(&value, reinterpret_cast<const std::uint8_t*>(engine) + rva, sizeof(value));
    return value;
}
void set_word(std::uint32_t rva, std::uint32_t value) {
    std::memcpy(reinterpret_cast<std::uint8_t*>(engine) + rva, &value, sizeof(value));
}
void mutate(std::uint32_t rva, std::uint8_t mask = 1) {
    auto* site = reinterpret_cast<std::uint8_t*>(engine) + rva;
    DWORD old = 0, ignored = 0;
    require(VirtualProtect(site, 1, PAGE_EXECUTE_READWRITE, &old) != 0, "mutation protection");
    *site ^= mask;
    require(VirtualProtect(site, 1, old, &ignored) != 0, "restore mutation protection");
}
void relocate_fixture() {
    // LoadLibrary's EXE data mapping does not perform loader relocations.
    // Emulate HIGHLOW fixups using the reference PE's table, never its entry.
    auto* base = reinterpret_cast<std::uint8_t*>(engine);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    const auto delta = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(engine) - nt->OptionalHeader.ImageBase);
    if (!delta) return;
    const auto dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    require(dir.VirtualAddress && dir.VirtualAddress < nt->OptionalHeader.SizeOfImage &&
        dir.Size <= nt->OptionalHeader.SizeOfImage - dir.VirtualAddress, "fixture relocation directory");
    // If a platform has already applied fixups, don't apply them twice.
    if (word(rb::pan_context_rva + 19) == reinterpret_cast<std::uintptr_t>(engine) + rb::fastscroll_rva) return;
    require(word(rb::pan_context_rva + 19) == rb::preferred_base + rb::fastscroll_rva, "fixture initial relocation state");
    for (std::uint32_t offset = 0; offset < dir.Size;) {
        require(sizeof(IMAGE_BASE_RELOCATION) <= dir.Size - offset, "fixture block header");
        const auto* block = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(base + dir.VirtualAddress + offset);
        if (!block->VirtualAddress && !block->SizeOfBlock) {
            for (auto at = offset; at < dir.Size; ++at)
                require(base[dir.VirtualAddress + at] == 0, "fixture zero relocation padding");
            break;
        }
        require(block->SizeOfBlock >= sizeof(*block) && block->SizeOfBlock <= dir.Size - offset && !(block->SizeOfBlock % 2), "fixture block size");
        const auto* entries = reinterpret_cast<const WORD*>(block + 1);
        for (unsigned n = 0; n < (block->SizeOfBlock - sizeof(*block)) / sizeof(WORD); ++n) {
            const auto type = entries[n] >> 12;
            if (!type) continue;
            require(type == IMAGE_REL_BASED_HIGHLOW, "fixture supported relocation type");
            const auto rva = block->VirtualAddress + (entries[n] & 0xfffu);
            require(rva <= nt->OptionalHeader.SizeOfImage - 4, "fixture operand bounds");
            DWORD old = 0, ignored = 0;
            require(VirtualProtect(base + rva, 4, PAGE_EXECUTE_READWRITE, &old) != 0, "fixture fixup protection");
            set_word(rva, word(rva) + delta);
            require(VirtualProtect(base + rva, 4, old, &ignored) != 0, "fixture fixup restore");
        }
        offset += block->SizeOfBlock;
    }
    FlushInstructionCache(GetCurrentProcess(), base, nt->OptionalHeader.SizeOfImage);
}
void remove_relocation(std::uint32_t wanted) {
    const auto* base = reinterpret_cast<const std::uint8_t*>(engine);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
    const auto dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    for (std::uint32_t offset = 0; offset < dir.Size;) {
        const auto* block = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(base + dir.VirtualAddress + offset);
        const auto* entries = reinterpret_cast<const WORD*>(block + 1);
        for (unsigned n = 0; n < (block->SizeOfBlock - sizeof(*block)) / sizeof(WORD); ++n) {
            if ((entries[n] >> 12) == IMAGE_REL_BASED_HIGHLOW && block->VirtualAddress + (entries[n] & 0xfffu) == wanted) {
                // Flip HIGHLOW to LOW: signature bytes remain valid, but
                // this operand is no longer authorized for normalization.
                mutate(static_cast<std::uint32_t>(reinterpret_cast<const std::uint8_t*>(entries + n) - base) + 1, 0x10);
                return;
            }
        }
        offset += block->SizeOfBlock;
    }
    require(false, "find negative relocation fixture");
}
void* fixture(bool pan) {
    auto* bytes = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    require(bytes != nullptr, "fixture allocation");
    bytes[0] = 0xe8;
    const auto delta = reinterpret_cast<std::intptr_t>(bridge) - reinterpret_cast<std::intptr_t>(bytes + 5);
    const auto rel = static_cast<std::int32_t>(delta);
    std::memcpy(bytes + 1, &rel, 4); bytes[5] = 0x90;
    std::size_t n = 6;
    if (!pan) {
        // The original continuation stores ECX in the script restoration
        // baseline; isolate that store without running the surrounding game.
        bytes[n++] = 0x89; bytes[n++] = 0x0d;
        const auto address = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(engine) + rb::restore_cull_rva);
        std::memcpy(bytes + n, &address, 4); n += 4;
    }
    bytes[n++] = 0xc3;
    DWORD old = 0;
    require(VirtualProtect(bytes, 32, PAGE_EXECUTE_READ, &old) != 0, "fixture execute protection");
    FlushInstructionCache(GetCurrentProcess(), bytes, n);
    return bytes;
}
void check_registers(const Capture& out, bool pan, std::uint32_t ecx) {
    require(out.registers[0] == 0x778899aa && out.registers[1] == 0x66778899 &&
        out.registers[2] == 0x55667788 && out.registers[4] == 0x44556677 &&
        out.registers[5] == 0x33445566 && out.registers[7] == 0x11223344, "integer register preservation");
    require(out.registers[6] == (pan ? 0x22334455u : ecx), "original ECX result");
    require((out.registers[8] & 0xcd5u) == (0x646u & 0xcd5u), "EFLAGS/DF preservation");
    require(std::memcmp(out.before, out.after, 2) == 0 &&
        std::memcmp(out.before + 24, out.after + 24, 8) == 0, "FP control/MXCSR preservation");
    if (pan) {
        const unsigned before_top = (out.before[3] >> 3) & 7;
        const unsigned after_top = (out.after[3] >> 3) & 7;
        require(after_top == ((before_top + 1) & 7), "FSTP consumes exactly one x87 entry");
        require(out.after[4] == (out.before[4] & ~(1u << before_top)), "x87 tag stack");
        // FXSAVE stores logical ST0 first, now the untouched 1.0 guard.
        require(std::memcmp(out.after + 32, out.before + 48, 10) == 0, "remaining x87 value");
    } else {
        require(std::memcmp(out.before, out.after, 416) == 0, "view bridge must preserve entire architectural FP state");
    }
}
void run(Capture& out, bool pan, std::uint32_t ecx) {
    out = {}; refit_capture = &out; refit_run_fixture(); check_registers(out, pan, ecx);
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 4) { std::fprintf(stderr, "Usage: refit_module_smoke.exe MODULE EXE CASE\n"); return 2; }
    const bool pan = std::string(argv[1]).find("CameraControls") != std::string::npos;
    const std::string mode = argv[3];
    char temporary[MAX_PATH]{}, folder[MAX_PATH]{};
    try {
        require(GetTempPathA(sizeof(temporary), temporary) != 0, "temporary directory");
        require(GetTempFileNameA(temporary, "rft", 0, folder) != 0, "temporary name");
        require(DeleteFileA(folder) && CreateDirectoryA(folder, nullptr), "temporary fixture directory");
        root = folder;
        const std::string ini = root + (pan ? "\\A2FOCameraControls.ini" : "\\A2FOViewDistance.ini");
        const char* section = pan ? "CameraControls" : "ViewDistance";
        if (mode != "missing") {
            require(WritePrivateProfileStringA(section, "Enabled", mode == "disabled" ? "0" : "1", ini.c_str()) != 0, "write fixture enable");
            const char* value = mode == "invalid" ? "NaN" : mode == "noop" ? (pan ? "1" : "0") : pan ? "2.5" : "10000";
            require(WritePrivateProfileStringA(section, pan ? "PanSpeed" : "MinimumDistance", value, ini.c_str()) != 0, "write fixture setting");
        }
        engine = LoadLibraryExA(argv[2], nullptr, DONT_RESOLVE_DLL_REFERENCES);
        require(engine != nullptr, "map input without entry point");
        relocate_fixture();
        if (mode == "relocation") remove_relocation(pan ? rb::pan_context_rva + 19 : rb::view_context_rva + 2);
        if (mode == "foreign") mutate(pan ? rb::pan_context_rva + rb::pan_patch_offset : rb::view_context_rva);
        if (mode == "context") mutate(pan ? rb::pan_context_rva : rb::view_context_rva + 20);
        if (mode == "consumer" && pan) mutate(rb::pan_consumers[1]);
        if (mode == "identity") mutate(0);
        refuse_patch = mode == "patch-fail";
        HMODULE module = LoadLibraryA(argv[1]);
        require(module != nullptr, "load module");
        auto init = symbol<A2FO_ModuleInitFn>(module, "A2FO_ModuleInit");
        auto shutdown = symbol<A2FO_ModuleShutdownFn>(module, "A2FO_ModuleShutdown");
        using Query = bool (A2FO_CALL*)(a2fo::refit::RefitStatus*);
        auto query = symbol<Query>(module, "A2FORefit_QueryStatus");
        require(init && shutdown && query, "required exports");
        A2FO_ModuleApi api{};
        api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
        api.log = logger; api.armada_module = image; api.root_directory = directory; api.patch_call = patch;
        require(!init(nullptr), "null API rejected");
        if (mode == "api") {
            api.struct_size = offsetof(A2FO_ModuleApi, patch_call);
            require(!init(&api) && patches == 0, "short API rejected");
        } else {
            require(init(&api), "module initialization");
            a2fo::refit::RefitStatus status{sizeof(status), 0, 0, 0, 0, 0};
            require(query(&status), "status query");
            const bool success = mode == "success";
            if (success && !status.installed) {
                std::fprintf(stderr, "mapped=%p pan operands=%08lx/%08lx view=%08lx\n", static_cast<void*>(engine),
                    static_cast<unsigned long>(word(rb::pan_context_rva+1)),
                    static_cast<unsigned long>(word(rb::pan_context_rva+19)),
                    static_cast<unsigned long>(word(rb::view_context_rva+2)));
            }
            require(status.installed == static_cast<unsigned>(success), "installation boundary");
            require(patches == (success || refuse_patch ? 1u : 0u), "patch count");
            require(init(&api) && patches == (success || refuse_patch ? 1u : 0u), "idempotent initialization");
            if (success) {
                refit_target = fixture(pan); refit_pan_fixture = pan; refit_parsed_value = a2fo::refit::float_to_bits(0.005f);
                Capture out{};
                if (pan) {
                    for (const std::uint16_t precision : {0u, 0x200u, 0x300u}) {
                        for (const std::uint16_t rounding : {0u, 0x400u, 0x800u, 0xc00u}) {
                            refit_control_word = static_cast<std::uint16_t>(0x7f | precision | rounding);
                            run(out, true, 0);
                            float actual = 0; const auto actual_bits = word(rb::fastscroll_rva); std::memcpy(&actual, &actual_bits, 4);
                            require(actual > 0.01249f && actual < 0.01251f, "scaled parsed pan value");
                        }
                    }
                    // A second parse consumes fresh native input, not the already-scaled global.
                    run(out, true, 0);
                } else {
                    const auto requested = a2fo::refit::float_to_bits(10000.0f);
                    const std::array<std::uint32_t, 10> values{{0,0x80000000u,0x7f800000u,0xff800000u,0x7fc12345u,0x7f812345u,
                        a2fo::refit::float_to_bits(2800.0f),a2fo::refit::float_to_bits(20000.0f),requested,0x3f800000u}};
                    for (const auto far_bits : values) for (const auto cull : values) {
                        set_word(rb::far_plane_rva, far_bits); set_word(rb::cull_rva, cull); set_word(rb::restore_cull_rva, 123);
                        const auto expected_cull = a2fo::refit::raise_positive_finite_bits(cull, requested);
                        run(out, false, expected_cull);
                        require(word(rb::far_plane_rva) == a2fo::refit::raise_positive_finite_bits(far_bits, requested), "far plane maximum");
                        require(word(rb::cull_rva) == expected_cull && word(rb::restore_cull_rva) == expected_cull, "culling/script restoration maximum");
                    }
                }
                shutdown(); require(query(&status) && !status.armed && status.parser_visits > 0, "shutdown disarms bridge");
                if (pan) { run(out, true, 0); require(word(rb::fastscroll_rva) == refit_parsed_value, "shutdown native FSTP passthrough"); }
                else {
                    set_word(rb::far_plane_rva, a2fo::refit::float_to_bits(5000.0f));
                    const auto before = a2fo::refit::float_to_bits(2800.0f); set_word(rb::cull_rva, before);
                    run(out, false, before);
                    require(word(rb::far_plane_rva) == a2fo::refit::float_to_bits(5000.0f) && word(rb::cull_rva) == before, "shutdown native MOV passthrough");
                }
                require(VirtualFree(refit_target, 0, MEM_RELEASE) != 0, "release private fixture");
            } else shutdown();
        }
        require(DeleteFileA(ini.c_str()) || mode == "missing", "remove temporary INI");
        require(RemoveDirectoryA(root.c_str()) != 0, "remove fixture directory");
        std::printf("%s %s: PASS (reference image=%p)\n", pan ? "camera" : "view", mode.c_str(), static_cast<void*>(engine));
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL %s %s: %s (Win32=%lu)\n%s", pan ? "camera" : "view", mode.c_str(), error.what(), GetLastError(), diagnostic.c_str());
        return 1;
    }
}
