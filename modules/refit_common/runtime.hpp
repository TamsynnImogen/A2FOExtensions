#pragma once

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "bindings.hpp"
#include "settings.hpp"
#include "status.hpp"
#include "../../rust/module_runtime_a3_ffi.h"
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdio>
#include <cstring>
#include <exception>
#include <locale>
#include <sstream>
#include <string>

static_assert(sizeof(void*) == 4, "Refit native adapters require Windows i386");
static_assert(sizeof(float) == 4, "Refit native adapters require binary32");

extern "C" {
void a2fo_refit_bridge();
alignas(4) volatile LONG a2fo_refit_value_bits =
#if defined(A2FO_REFIT_CAMERA)
    0x3f800000;
#else
    0;
#endif
alignas(4) volatile LONG a2fo_refit_parser_visits = 0;
std::uint32_t a2fo_refit_data_a = 0;
std::uint32_t a2fo_refit_data_b = 0;
}

namespace {
namespace rb = a2fo::refit::bindings;
#if defined(A2FO_REFIT_CAMERA)
constexpr char module_name[] = "A2FOCameraControls";
constexpr char section[] = "CameraControls";
constexpr char value_key[] = "PanSpeed";
constexpr char default_value[] = "2.5";
#elif defined(A2FO_REFIT_VIEW)
constexpr char module_name[] = "A2FOViewDistance";
constexpr char section[] = "ViewDistance";
constexpr char value_key[] = "MinimumDistance";
constexpr char default_value[] = "0";
#else
#error Select one native Refit module
#endif
const A2FO_ModuleApi* module_api = nullptr;
bool initialized = false;
bool installed = false;
bool configuration_passive = false;
float configured_value = 0;

void log_message(const char* message) noexcept {
    if (module_api && module_api->log) module_api->log(module_name, message);
}

bool read_exact(const void* address, void* destination, std::size_t size) noexcept {
    SIZE_T count = 0;
    return address && ReadProcessMemory(GetCurrentProcess(), address, destination, size, &count) && count == size;
}
template<std::size_t N>
bool matches(HMODULE image, std::uint32_t rva, const std::array<std::uint8_t, N>& expected) noexcept {
    std::array<std::uint8_t, N> actual{};
    return read_exact(reinterpret_cast<const std::uint8_t*>(image) + rva, actual.data(), N) && actual == expected;
}
bool writable_word(HMODULE image, std::uint32_t rva) noexcept {
    MEMORY_BASIC_INFORMATION memory{};
    const auto address = reinterpret_cast<std::uintptr_t>(image) + rva;
    if (!VirtualQuery(reinterpret_cast<void*>(address), &memory, sizeof(memory)) ||
        memory.AllocationBase != image || memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS)) ||
        !(memory.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return false;
    const auto end = reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize;
    return address <= end && 4 <= end - address;
}

bool clean_gog_file(HMODULE image) noexcept {
    // Main SDK currently identifies ArmadaL profiles, not GOG. Admit the clean
    // GOG image by its complete FILE hash, never by a similar instruction site.
    // No LAA/other variant is silently inferred from this one input.
    constexpr char expected[] = "8d3ba6ec826198daf3e0c7b3435e9ce8195293e56da7c87e0af2634eaa4693b3";
    char path[32768];
    const DWORD length = GetModuleFileNameA(image, path, sizeof(path));
    if (!length || length >= sizeof(path)) return false;
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    HCRYPTPROV provider = 0; HCRYPTHASH hash = 0;
    bool ok = CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
              CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash);
    std::array<BYTE, 16384> bytes{};
    while (ok) {
        DWORD n = 0;
        if (!ReadFile(file, bytes.data(), bytes.size(), &n, nullptr)) { ok = false; break; }
        if (!n) break;
        ok = CryptHashData(hash, bytes.data(), n, 0) != FALSE;
    }
    std::array<BYTE, 32> digest{}; DWORD count = digest.size();
    if (ok) ok = CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &count, 0) && count == digest.size();
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(file);
    if (!ok) return false;
    char hex[65]{};
    for (std::size_t i = 0; i < digest.size(); ++i) std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
    return std::strcmp(hex, expected) == 0;
}

bool supported_host(HMODULE image) noexcept {
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS32 nt{};
    if (!read_exact(image, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE ||
        dos.e_lfanew <= 0 || dos.e_lfanew >= 4096 ||
        !read_exact(reinterpret_cast<const std::uint8_t*>(image) + dos.e_lfanew, &nt, sizeof(nt)) ||
        nt.Signature != IMAGE_NT_SIGNATURE || nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt.OptionalHeader.ImageBase != rb::preferred_base || nt.OptionalHeader.AddressOfEntryPoint != 0x2733c0 ||
        nt.OptionalHeader.SizeOfImage > UINT32_MAX - reinterpret_cast<std::uintptr_t>(image)) return false;
    const auto identity = a2fo::supported_armada::identify(image);
    if (identity == a2fo::supported_armada::Identity::normalized ||
        identity == a2fo::supported_armada::Identity::janb_20260905 ||
        identity == a2fo::supported_armada::Identity::janb_20260930 ||
        identity == a2fo::supported_armada::Identity::roots_profile) return true;
    // The SDK's canonical shortcut is only a header check. Do not broaden
    // this module's new admission on that shortcut. GOG is fully hashed.
    return nt.OptionalHeader.SizeOfImage == 0x3fa000 && nt.FileHeader.NumberOfSections == 6 && clean_gog_file(image);
}

// Normalize only documented HIGHLOW operands. Never wildcard addresses or
// instruction bytes: each operand must have its own PE relocation record.
bool has_highlow(HMODULE image, std::uint32_t wanted) noexcept {
    IMAGE_DOS_HEADER dos{}; IMAGE_NT_HEADERS32 nt{};
    if (!read_exact(image, &dos, sizeof(dos)) ||
        !read_exact(reinterpret_cast<const std::uint8_t*>(image) + dos.e_lfanew, &nt, sizeof(nt)) ||
        nt.OptionalHeader.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_BASERELOC) return false;
    const auto dir = nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    const auto size = nt.OptionalHeader.SizeOfImage;
    if (!dir.VirtualAddress || dir.VirtualAddress >= size || dir.Size > size - dir.VirtualAddress) return false;
    std::uint32_t offset = 0;
    while (offset < dir.Size) {
        IMAGE_BASE_RELOCATION block{};
        if (sizeof(block) > dir.Size - offset ||
            !read_exact(reinterpret_cast<const std::uint8_t*>(image) + dir.VirtualAddress + offset, &block, sizeof(block)) ||
            block.SizeOfBlock < sizeof(block) || block.SizeOfBlock > dir.Size - offset ||
            (block.SizeOfBlock - sizeof(block)) % sizeof(WORD)) return false;
        for (std::uint32_t at = sizeof(block); at < block.SizeOfBlock; at += sizeof(WORD)) {
            WORD entry = 0;
            if (!read_exact(reinterpret_cast<const std::uint8_t*>(image) + dir.VirtualAddress + offset + at, &entry, sizeof(entry))) return false;
            if ((entry >> 12) == IMAGE_REL_BASED_HIGHLOW &&
                block.VirtualAddress <= wanted && wanted - block.VirtualAddress == (entry & 0xfffu)) return true;
        }
        offset += block.SizeOfBlock;
    }
    return false;
}
template<std::size_t N, std::size_t M>
bool relocated(HMODULE image, std::uint32_t rva, const std::array<std::uint8_t, N>& source,
               const std::array<std::size_t, M>& operands, std::array<std::uint8_t, N>& out) noexcept {
    out = source;
    const auto delta = static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image) - rb::preferred_base);
    for (const auto at : operands) {
        if (at > N || sizeof(std::uint32_t) > N - at || !has_highlow(image, rva + at)) return false;
        std::uint32_t value = 0;
        std::memcpy(&value, out.data() + at, sizeof(value));
        value += delta;
        std::memcpy(out.data() + at, &value, sizeof(value));
    }
    return true;
}

bool signatures_match(HMODULE image) noexcept {
#if defined(A2FO_REFIT_CAMERA)
    std::array<std::uint8_t, 25> context{};
    if (!relocated(image, rb::pan_context_rva, rb::pan_context, std::array<std::size_t, 2>{{1,19}}, context) ||
        !matches(image, rb::pan_context_rva, context) || !writable_word(image, rb::fastscroll_rva)) return false;
    for (const auto consumer : rb::pan_consumers) {
        std::array<std::uint8_t, 6> expected{};
        if (!relocated(image, consumer, rb::pan_consumer, std::array<std::size_t, 1>{{2}}, expected) ||
            !matches(image, consumer, expected)) return false;
    }
    return true;
#else
    std::array<std::uint8_t, 21> context{};
    return relocated(image, rb::view_context_rva, rb::view_context, std::array<std::size_t, 3>{{2,8,17}}, context) &&
        matches(image, rb::view_context_rva, context) &&
        writable_word(image, rb::far_plane_rva) && writable_word(image, rb::cull_rva) && writable_word(image, rb::restore_cull_rva);
#endif
}

struct RefitContext {
    const A2FO_ModuleApi* api = nullptr;
    HMODULE image = nullptr;
    std::string root;
    std::string path;
    char enabled[32]{}, value[64]{};
    std::uint32_t site_rva = 0;
    const std::uint8_t* expected = nullptr;
#if defined(A2FO_REFIT_CAMERA)
    std::array<std::uint8_t,25> expected_context{};
#else
    std::array<std::uint8_t,21> expected_context{};
#endif
    std::exception_ptr error;
};
std::uint8_t refit_read(void* opaque,A2foRsRefitConfig* out) noexcept {
    auto& context=*static_cast<RefitContext*>(opaque);
    if(context.error || !out)return 0;
    try {
        const auto n=GetPrivateProfileStringA(section,"Enabled","0",context.enabled,
            sizeof(context.enabled),context.path.c_str());
        const auto m=GetPrivateProfileStringA(section,value_key,default_value,context.value,
            sizeof(context.value),context.path.c_str());
        *out={{reinterpret_cast<const std::uint8_t*>(context.enabled),n},
              {reinterpret_cast<const std::uint8_t*>(context.value),m},n,m};
        return 1;
    } catch (...) {context.error=std::current_exception();return 0;}
}
std::uint8_t refit_number(void* opaque,A2foRsRefitBytes text,float* out) noexcept {
    auto& context=*static_cast<RefitContext*>(opaque);
    if(context.error || !out)return 0;
    try {
        std::istringstream stream(std::string(
            reinterpret_cast<const char*>(text.data),text.size));
        stream.imbue(std::locale::classic());stream>>std::noskipws;
        float value=0.0f;stream>>value;
        if(!stream || stream.peek()!=std::char_traits<char>::eof())return 0;
        *out=value;return 1;
    } catch (...) {context.error=std::current_exception();return 0;}
}
std::uint8_t refit_step(void* opaque,std::uint32_t operation,
                        std::uint32_t value_bits) noexcept {
    auto& context=*static_cast<RefitContext*>(opaque);
    if(context.error)return 0;
    try {
        switch(operation){
        case 0: initialized=true;module_api=context.api;return 1;
        case 1:{
            const char* root=context.api->root_directory();
            if(!root || !*root)return 0;
            context.root=root;
            context.path=context.root+"\\"+module_name+".ini";
            return 1;
        }
        case 2:std::memcpy(&configured_value,&value_bits,sizeof(value_bits));return 1;
        case 3:configuration_passive=true;return 1;
        case 4:
            context.image=static_cast<HMODULE>(context.api->armada_module());
            return supported_host(context.image);
        case 5:return signatures_match(context.image);
        case 6:{
            HMODULE resident=nullptr;
            return GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|
                GET_MODULE_HANDLE_EX_FLAG_PIN,
                reinterpret_cast<const char*>(&a2fo_refit_bridge),&resident)!=FALSE;
        }
        case 7:{
#if defined(A2FO_REFIT_CAMERA)
            a2fo_refit_data_a=reinterpret_cast<std::uintptr_t>(context.image)+rb::fastscroll_rva;
            context.site_rva=rb::pan_context_rva+rb::pan_patch_offset;
            if(!relocated(context.image,rb::pan_context_rva,rb::pan_context,
                std::array<std::size_t,2>{{1,19}},context.expected_context))return 0;
            context.expected=context.expected_context.data()+rb::pan_patch_offset;
#else
            a2fo_refit_data_a=reinterpret_cast<std::uintptr_t>(context.image)+rb::far_plane_rva;
            a2fo_refit_data_b=reinterpret_cast<std::uintptr_t>(context.image)+rb::cull_rva;
            context.site_rva=rb::view_context_rva;
            if(!relocated(context.image,rb::view_context_rva,rb::view_context,
                std::array<std::size_t,3>{{2,8,17}},context.expected_context))return 0;
            context.expected=context.expected_context.data();
#endif
            return 1;
        }
        case 8:
            InterlockedExchange(&a2fo_refit_value_bits,static_cast<LONG>(value_bits));return 1;
        case 9:return context.api->patch_call(
            reinterpret_cast<std::uint8_t*>(context.image)+context.site_rva,
            reinterpret_cast<void*>(&a2fo_refit_bridge),context.expected,6);
        case 10:InterlockedExchange(&a2fo_refit_value_bits,
            static_cast<LONG>(value_bits));return 1;
        case 11:installed=true;return 1;
        default:return 0;
        }
    } catch (...) {context.error=std::current_exception();return 0;}
}
void refit_log(void* opaque,std::uint32_t code) noexcept {
    auto& context=*static_cast<RefitContext*>(opaque);
    if(context.error)return;
    switch(code){
    case 0:log_message("No extension root; passive");break;
    case 1:log_message("Invalid or truncated configuration; passive");break;
    case 2:log_message("Invalid configuration; passive");break;
    case 3:
#if defined(A2FO_REFIT_CAMERA)
        log_message("Disabled or PanSpeed=1; no hooks installed");
#else
        log_message("Disabled or MinimumDistance=0; no hooks installed");
#endif
        break;
    case 4:log_message("Unsupported executable identity/base; passive");break;
    case 5:log_message("Native signatures/data permissions differ or another hook owns a site; passive");break;
    case 6:log_message("Could not pin native bridge lifetime; passive");break;
    case 7:log_message("Checked patch failed; passive");break;
    case 8:{
        char message[224];
        std::snprintf(message,sizeof(message),
#if defined(A2FO_REFIT_BUILTIN)
            "Prepared %s=%.9g at RVA 0x%08lx; module policy gates future native configuration parses (restart required)",
#else
            "Armed %s=%.9g at RVA 0x%08lx; applies when native configuration parses (restart required)",
#endif
            value_key,static_cast<double>(configured_value),
            static_cast<unsigned long>(context.site_rva));
        log_message(message);break;
    }
    }
}
bool initialize(const A2FO_ModuleApi* api) {
    const bool valid=api && api->api_version==A2FO_MODULE_API_VERSION &&
        A2FO_MODULE_API_HAS(api,patch_call) && api->log && api->root_directory &&
        api->armada_module && api->patch_call;
    if(!valid)return false;
    RefitContext context;context.api=api;
    const bool result=a2fo_rs_runtime_refit_init(valid,initialized,
#if defined(A2FO_REFIT_CAMERA)
        1,
#else
        0,
#endif
#if defined(A2FO_REFIT_BUILTIN)
        1,
#else
        0,
#endif
        &refit_read,&refit_number,&refit_step,&refit_log,&context)!=0;
    if(context.error)std::rethrow_exception(context.error);
    return result;
}
void refit_shutdown_step(void*,std::uint32_t operation) noexcept {
    switch(operation){
    case 0:InterlockedExchange(&a2fo_refit_value_bits,
#if defined(A2FO_REFIT_CAMERA)
        0x3f800000
#else
        0
#endif
        );break;
    case 1:{
        char message[192];
        std::snprintf(message,sizeof(message),
            "Shutdown: parser visits=%ld; future adjustments disabled; parsed engine values require restart to restore",
            a2fo_refit_parser_visits);
        log_message(message);break;
    }
    case 2:module_api=nullptr;break;
    }
}
} // namespace

#if defined(A2FO_REFIT_BUILTIN)
extern "C" bool A2FORefit_BuiltinReady() noexcept {
    return installed || configuration_passive;
}

// Publish only the bridge's existing aligned atomic value word. This keeps
// the native assembly path free of C++ calls and preserves its register/FP
// contract. Menu changes affect future parses, never previously parsed data.
extern "C" void A2FORefit_SetEnabled(bool enabled) noexcept {
    InterlockedExchange(&a2fo_refit_value_bits,
        static_cast<LONG>(a2fo_rs_runtime_refit_value_bits(
#if defined(A2FO_REFIT_CAMERA)
            1,
#else
            0,
#endif
            installed,enabled,a2fo::refit::float_to_bits(configured_value))));
}
#endif

extern "C" __declspec(dllexport) bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    try { return initialize(api); }
    catch (...) { log_message("Configuration/initialization exception contained; passive"); return true; }
}
extern "C" __declspec(dllexport) bool A2FO_CALL A2FORefit_QueryStatus(a2fo::refit::RefitStatus* out) {
    if (!out || out->struct_size != sizeof(*out)) return false;
    const auto bits = static_cast<std::uint32_t>(InterlockedCompareExchange(&a2fo_refit_value_bits, 0, 0));
    *out = {sizeof(*out), 1, installed ? 1u : 0u,
        a2fo_rs_runtime_refit_armed(
#if defined(A2FO_REFIT_CAMERA)
            1,
#else
            0,
#endif
            installed,bits),
        static_cast<std::uint32_t>(InterlockedCompareExchange(&a2fo_refit_parser_visits, 0, 0)), configured_value};
    return true;
}
extern "C" __declspec(dllexport) void A2FO_CALL A2FO_ModuleShutdown() {
    a2fo_rs_runtime_refit_shutdown(&refit_shutdown_step,nullptr);
}
