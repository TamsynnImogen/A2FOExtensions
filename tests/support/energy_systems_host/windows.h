#pragma once

// Test-only declarations for compiling the production energy callback on a
// POSIX host. No Win32 function is defined here: the link must discard every
// unused native hook/UI path, and fail if the exercised callback needs one.
#include <cstddef>
#include <cstdint>
#include <strings.h>

#define __cdecl
#define __stdcall
#define __fastcall
#define __declspec(kind)
#define _strnicmp strncasecmp

using HMODULE = void*;
using FARPROC = void (*)();

struct MEMORY_BASIC_INFORMATION {
    void* BaseAddress;
    std::size_t RegionSize;
    std::uint32_t State;
    std::uint32_t Protect;
};

constexpr std::uint32_t MEM_COMMIT = 0x1000;
constexpr std::uint32_t PAGE_NOACCESS = 0x01;
constexpr std::uint32_t PAGE_GUARD = 0x100;

extern "C" std::size_t VirtualQuery(
    const void* address, MEMORY_BASIC_INFORMATION* information,
    std::size_t length);
extern "C" HMODULE GetModuleHandleA(const char* name);
extern "C" FARPROC GetProcAddress(HMODULE module, const char* name);
