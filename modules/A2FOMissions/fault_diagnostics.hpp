#pragma once

#include <windows.h>
#include <cstdio>
#include <cstring>

// Diagnostic only: observe first-chance native faults without swallowing them.
// Use a pre-opened handle, bounded stack buffers and no game/extension callbacks.
namespace mmm::a2fo_missions::fault_diagnostics {
inline HANDLE file = INVALID_HANDLE_VALUE;
inline PVOID handler = nullptr;
inline volatile LONG faults = 0;

inline void write(const char* text) noexcept {
    if (file == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
}

inline LONG CALLBACK observe(EXCEPTION_POINTERS* info) noexcept {
    if (!info || !info->ExceptionRecord || !info->ContextRecord)
        return EXCEPTION_CONTINUE_SEARCH;
    const auto& exception = *info->ExceptionRecord;
    if (exception.ExceptionCode != EXCEPTION_ACCESS_VIOLATION &&
        exception.ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION &&
        exception.ExceptionCode != EXCEPTION_INT_DIVIDE_BY_ZERO)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&faults) > 8) return EXCEPTION_CONTINUE_SEARCH;

    const auto& context = *info->ContextRecord;
    char line[1024]{};
    char path[MAX_PATH]{};
    MEMORY_BASIC_INFORMATION region{};
    if (VirtualQuery(exception.ExceptionAddress, &region, sizeof(region))) {
        GetModuleFileNameA(static_cast<HMODULE>(region.AllocationBase), path, sizeof(path));
        path[sizeof(path) - 1] = 0;
    }
    std::snprintf(line, sizeof(line),
        "first-chance fault code=%08lx eip=%08lx module=%s base=%p offset=%08lx\r\n"
        "eax=%08lx ebx=%08lx ecx=%08lx edx=%08lx esi=%08lx edi=%08lx ebp=%08lx esp=%08lx\r\n",
        exception.ExceptionCode, context.Eip, path, region.AllocationBase,
        context.Eip - reinterpret_cast<DWORD>(region.AllocationBase),
        context.Eax, context.Ebx, context.Ecx, context.Edx,
        context.Esi, context.Edi, context.Ebp, context.Esp);
    write(line);
    if (exception.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && exception.NumberParameters >= 2) {
        std::snprintf(line, sizeof(line), "access=%lu address=%08lx\r\n",
                      exception.ExceptionInformation[0], exception.ExceptionInformation[1]);
        write(line);
    }
    DWORD stack[32]{};
    SIZE_T copied = 0;
    if (ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(context.Esp),
                          stack, sizeof(stack), &copied)) {
        for (unsigned i = 0; i < copied / sizeof(DWORD); ++i) {
            std::snprintf(line, sizeof(line), "stack+%02x=%08lx\r\n", i * 4, stack[i]);
            write(line);
        }
    }
    FlushFileBuffers(file);
    return EXCEPTION_CONTINUE_SEARCH;
}

inline void start() noexcept {
    if (handler) return;
    char path[MAX_PATH]{};
    const DWORD length = GetModuleFileNameA(nullptr, path, sizeof(path));
    if (!length || length >= sizeof(path)) return;
    char* slash = std::strrchr(path, '\\');
    if (!slash) slash = std::strrchr(path, '/');
    constexpr char name[] = "A2FOMissions-fault.log";
    if (!slash || static_cast<std::size_t>(slash + 1 - path) + sizeof(name) > sizeof(path)) return;
    std::memcpy(slash + 1, name, sizeof(name));
    file = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    faults = 0;
    write("A2FOMissions first-chance fault diagnostics; exceptions remain unhandled by this observer.\r\n");
    handler = AddVectoredExceptionHandler(1, observe);
    if (!handler) write("Unable to register exception observer.\r\n");
}

inline void stop() noexcept {
    if (handler) RemoveVectoredExceptionHandler(handler);
    handler = nullptr;
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    file = INVALID_HANDLE_VALUE;
}
}
