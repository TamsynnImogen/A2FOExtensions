#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace a2fo::fleetops_profile {
// October 12 Roots input. Reviewed against the September profile: 30 changed
// bytes outside the menu/VCL/resource paths. Retain a complete SHA pin here.
inline bool october_roots_file(const char* path) noexcept {
    constexpr char expected[] = "3fd18eeee4201a9a1297ac26b3eb88f78c5f72e8c55a62c25179f1d4932e4c11";
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    HCRYPTPROV provider = 0; HCRYPTHASH hash = 0;
    bool okay = CryptAcquireContextA(&provider, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash);
    std::array<BYTE,16384> buffer{};
    while (okay) {
        DWORD count = 0;
        if (!ReadFile(file, buffer.data(), buffer.size(), &count, nullptr)) { okay = false; break; }
        if (!count) break;
        okay = CryptHashData(hash, buffer.data(), count, 0) != FALSE;
    }
    std::array<BYTE,32> digest{}; DWORD size = digest.size();
    if (okay) okay = CryptGetHashParam(hash, HP_HASHVAL, digest.data(), &size, 0) && size == digest.size();
    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider,0);
    CloseHandle(file);
    char hex[65]{};
    if (okay) for (std::size_t i=0;i<digest.size();++i) std::snprintf(hex+i*2,3,"%02x",digest[i]);
    return okay && std::strcmp(hex,expected)==0;
}
}
