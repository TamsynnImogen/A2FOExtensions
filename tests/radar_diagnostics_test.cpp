// Synthetic, console-only radar observer test. No game code or graphics provider runs.
#include "../modules/A2FORadarDiagnostics/module.cpp"
#include "../rust/native_hooks_ffi.h"
#ifndef A2FO_LEGACY_RADAR_PROBE
namespace radar_diagnostics {
// Exercise the current controller through the same operation adapter as Init.
bool install() { InitContext context{api}; return a2fo_rs_radar_install(&context,init_operation)!=0; }
}
#endif

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>

namespace {

using namespace radar_diagnostics;
constexpr std::size_t kImageSize = 4u * 1024u * 1024u;
const unsigned char kPrologue[]{0x55, 0x8b, 0xec, 0x83, 0xec, 0x44};
unsigned patch_count = 0;
unsigned refuse_patch = 0;
bool decommit_rollback = false;
unsigned clear_count = 0;
unsigned viewport_count = 0;
unsigned original_count = 0;
std::vector<std::uint8_t> installed_image;
HRESULT clear_result = D3DERR_DEVICELOST;
HRESULT last_clear_result = S_OK;
void* expected_self = nullptr;
void* expected_class = nullptr;
const float* expected_position = nullptr;
int expected_team = 0;
int expected_pass = 0;
IDirect3DDevice8* fake_device = nullptr;
const D3DRECT dot{21, 23, 25, 27};
constexpr DWORD kCount = 1;
constexpr DWORD kFlags = D3DCLEAR_TARGET;
constexpr D3DCOLOR kColor = 0x0012a4f0;
constexpr float kZ = 0.375f;
constexpr DWORD kStencil = 7;

void require(bool ok, const char* reason) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", reason); std::exit(1); }
}

void A2FO_CALL fake_log(const char*, const char*) {}
HMODULE reference_image=nullptr;
std::string reference_root;
void* A2FO_CALL reference_module() {return reference_image;}
const char* A2FO_CALL reference_directory() {return reference_root.c_str();}
void A2FO_CALL reference_log(const char*,const char* line) {std::printf("%s\n",line);}
bool A2FO_CALL fake_patch(void* target, void* replacement,
                          const std::uint8_t* expected, std::size_t size) {
    ++patch_count;
    if (patch_count == refuse_patch) {
        if(decommit_rollback) {
            // Fault injection: make the first patched page uncommitted so
            // rollback's VirtualProtect genuinely fails. Never execute it.
            const auto page=reinterpret_cast<std::uintptr_t>(image+clear_call)&~std::uintptr_t(4095);
            require(VirtualFree(reinterpret_cast<void*>(page),4096,MEM_DECOMMIT)!=0,
                    "decommit rollback fault page");
        }
        return false;
    }
    return a2fo_rs_windows_patch_call(target, replacement, expected, size);
}

struct Image {
    std::uint8_t* bytes = static_cast<std::uint8_t*>(VirtualAlloc(
        nullptr, kImageSize, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
    Image() { require(bytes != nullptr, "allocate synthetic 4 MiB image"); }
    ~Image() { VirtualFree(bytes, 0, MEM_RELEASE); }
    void seed() {
        std::memset(bytes, 0xcc, kImageSize);
        std::memcpy(bytes + craft_call, expected_call, sizeof(expected_call));
        std::memcpy(bytes + map_object, kPrologue, sizeof(kPrologue));
        std::memcpy(bytes + clear_call, expected_clear, sizeof(expected_clear));
    }
};

std::uint8_t* target(const std::uint8_t* site) {
    require(site[0] == 0xe8, "patched call opcode");
    std::int32_t offset = 0;
    std::memcpy(&offset, site + 1, sizeof(offset));
    return const_cast<std::uint8_t*>(site) + 5 + offset;
}

HRESULT WINAPI device_clear(IDirect3DDevice8* self, DWORD count,
    const D3DRECT* rects, DWORD flags, D3DCOLOR color, float z, DWORD stencil) {
    ++clear_count;
    require(self == fake_device && count == kCount && rects == &dot &&
            flags == kFlags && color == kColor && z == kZ &&
            stencil == kStencil, "Clear preserves self and every argument");
    return clear_result;
}

HRESULT WINAPI device_viewport(IDirect3DDevice8* self, D3DVIEWPORT8* viewport) {
    ++viewport_count;
    require(self == fake_device && viewport != nullptr, "GetViewport self/output");
    *viewport = D3DVIEWPORT8{16, 16, 32, 32, 0.0f, 1.0f};
    return S_OK;
}

void __attribute__((fastcall)) original_stub(void* self, void* dummy,
    void* object_class, const float* position, int team, int pass) {
    ++original_count;
    require(self == expected_self && dummy == nullptr &&
            object_class == expected_class && position == expected_position &&
            team == expected_team && pass == expected_pass,
            "original receives native self/class/position/team/pass");
    last_clear_result = clear_hook(fake_device, kCount, &dot, kFlags,
                                   kColor, kZ, kStencil);
}

void patch_cases(Image& fixture, A2FO_ModuleApi& sdk) {
    api = &sdk;
    image = fixture.bytes;
    for (const auto site : std::array<std::uintptr_t, 3>{
             craft_call, map_object, clear_call}) {
        fixture.seed();
        fixture.bytes[site] ^= 0x01;
        const std::vector<std::uint8_t> before(fixture.bytes,
                                                fixture.bytes + kImageSize);
        patch_count = refuse_patch = 0;
        installed = false;
        require(!install() && patch_count == 0 && !installed,
                "changed site rejected before either patch");
        require(std::memcmp(fixture.bytes, before.data(), kImageSize) == 0,
                "changed-site rejection leaves whole image intact");
    }

    fixture.seed();
    const std::vector<std::uint8_t> before(fixture.bytes,
                                            fixture.bytes + kImageSize);
    patch_count = 0;
    refuse_patch = 2;
    require(!install() && patch_count == 2 && !installed,
            "second patch failure rejects installation");
    require(std::memcmp(fixture.bytes, before.data(), kImageSize) == 0,
            "second patch failure restores first site and all image bytes");

    patch_count=0;
    decommit_rollback=true;
    require(install() && installed && !capturing.load(),
            "failed rollback retains callback owner with capture disabled");
    const auto page=reinterpret_cast<std::uintptr_t>(image+clear_call)&~std::uintptr_t(4095);
    require(VirtualAlloc(reinterpret_cast<void*>(page),4096,MEM_COMMIT,PAGE_EXECUTE_READWRITE)!=nullptr,
            "recommit fault page for subsequent tests");
    installed=false;
    capturing.store(true);
    decommit_rollback=false;

    fixture.seed();
    patch_count = refuse_patch = 0;
    require(install() && patch_count == 2 && installed,
            "both checked patches installed");
    require(target(fixture.bytes + clear_call) ==
                reinterpret_cast<std::uint8_t*>(&clear_hook) &&
            fixture.bytes[clear_call + 5] == 0x90 &&
            target(fixture.bytes + craft_call) ==
                reinterpret_cast<std::uint8_t*>(&object_hook),
            "both patch targets and Clear padding exact");
    installed_image.assign(fixture.bytes,fixture.bytes+kImageSize);
}

void forwarding_cases() {
    // The install address was validated above. Never execute synthetic game bytes.
    original_object = &original_stub;
    std::array<void*, 97> vtable{};
    vtable[36] = reinterpret_cast<void*>(&device_clear);
    vtable[41] = reinterpret_cast<void*>(&device_viewport);
    struct Device { void** table; } device{vtable.data()};
    fake_device = reinterpret_cast<IDirect3DDevice8*>(&device);
    std::array<std::uint8_t, 0x220> object_class{};
    float position[3]{1.25f, -2.5f, 3.75f};
    int self_storage = 0;
    expected_self = &self_storage;
    expected_class = object_class.data();
    expected_position = position;
    expected_team = 3;
    expected_pass = 0;

    require(clear_hook(fake_device, kCount, &dot, kFlags, kColor, kZ,
                       kStencil) == clear_result &&
            clear_count == 1 && viewport_count == 0 && dots.load() == 0,
            "out-of-scope Clear forwards without viewport query or dot count");

    for (unsigned n = 0; n < 25; ++n) {
        expected_pass = static_cast<int>(n & 1u);
        object_hook(expected_self, nullptr, expected_class,
                    expected_position, expected_team, expected_pass);
        require(last_clear_result == clear_result,
                "scoped Clear HRESULT preserved through original");
    }
    require(original_count == 25 && clear_count == 26 && dots.load() == 25 &&
            viewport_count == 24 && samples == 25 &&
            objects[0].load() == 13 && objects[1].load() == 12 &&
            !craft_scope && craft_dots == 0,
            "object forwarding, bounded viewport samples and scope restoration");
    A2FO_ModuleShutdown();
    require(installed && api && !capturing.load(),"shutdown retains callback storage and disables capture");
    object_hook(expected_self,nullptr,expected_class,expected_position,expected_team,expected_pass);
    require(original_count==26 && clear_count==27 && viewport_count==24 && dots.load()==25 &&
            objects[0].load()==13 && objects[1].load()==12 && last_clear_result==clear_result,
            "after shutdown calls still forward without capture or extra queries");
}

} // namespace

int main(int argc,char** argv) {
    Image fixture;
    A2FO_ModuleApi sdk{};
    sdk.struct_size = sizeof(sdk);
    sdk.api_version = A2FO_MODULE_API_VERSION;
    sdk.log = &fake_log;
    sdk.patch_call = &fake_patch;
    patch_cases(fixture, sdk);
    forwarding_cases();
    require(std::memcmp(fixture.bytes, installed_image.data(), kImageSize) == 0,
            "shutdown leaves executable instructions intact for in-flight callbacks");
    if(argc==4) {
        // Map the real executable as inert data. Never call its entry point,
        // imports, renderer functions or either patched instruction.
        reference_image=LoadLibraryExA(argv[2],nullptr,DONT_RESOLVE_DLL_REFERENCES);
        require(reference_image!=nullptr,"map reference EXE as inert data");
        char current[32768]{};
        require(GetCurrentDirectoryA(sizeof(current),current)!=0,"private fixture directory");
        reference_root=current;
        const std::string ini=reference_root+"\\A2FORadarDiagnostics.ini";
        require(WritePrivateProfileStringA("Diagnostics","Enabled","1",ini.c_str())!=0,"enable private fixture diagnostic");
        HMODULE diagnostic=LoadLibraryA(argv[3]);
        require(diagnostic!=nullptr,"load actual diagnostic DLL");
        using Init=bool (A2FO_CALL*)(const A2FO_ModuleApi*);
        Init init=nullptr; const auto address=GetProcAddress(diagnostic,"A2FO_ModuleInit");
        std::memcpy(&init,&address,sizeof(init));
        require(init!=nullptr,"diagnostic init export");
        sdk.armada_module=&reference_module;
        sdk.root_directory=&reference_directory;
        sdk.log=&reference_log;
        patch_count=refuse_patch=0;
        require(init(&sdk) && patch_count==2,"actual DLL accepts exact reference identity and both real call sites");
        const auto* bytes=reinterpret_cast<const std::uint8_t*>(reference_image);
        require(bytes[craft_call]==0xe8 && bytes[clear_call]==0xe8 && bytes[clear_call+5]==0x90,
                "actual diagnostic DLL patches private reference mapping");
        std::puts("PASS: data-only reference image identity and actual DLL initialization");
        // The private reference mapping and DLL remain until fixture exit,
        // matching their declared process lifetime; no game code executes.
    }
    std::puts("PASS: radar diagnostics checked patches, rollback, forwarding and lifetime");
    return 0;
}
