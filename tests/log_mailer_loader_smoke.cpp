// Load the shipped DLL and helper in a private, headless Win32 fixture.
// The fixture parent stays alive until its helper has exited; no report is made.
#include "../sdk/include/a2fo_module_api.h"
#include <windows.h>
#include <tlhelp32.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
std::string root_ansi;
std::string messages;

void check(bool good, const char* reason) {
    if (!good) throw std::runtime_error(reason);
}
std::wstring join(const std::wstring& base, const wchar_t* name) {
    return base + L"\\" + name;
}
std::wstring full_path(const wchar_t* input) {
    const DWORD size = GetFullPathNameW(input, 0, nullptr, nullptr);
    check(size != 0, "resolve input path size");
    std::vector<wchar_t> buffer(size);
    const DWORD written = GetFullPathNameW(input, size, buffer.data(), nullptr);
    check(written != 0 && written < size, "resolve input path");
    return buffer.data();
}
void A2FO_CALL log_message(const char*, const char* message) {
    if (message) { messages += message; messages += '\n'; }
}
const char* A2FO_CALL root_directory() { return root_ansi.c_str(); }
template<class Function> Function export_symbol(HMODULE dll, const char* name) {
    const FARPROC address = GetProcAddress(dll, name);
    Function function = nullptr;
    static_assert(sizeof(function) == sizeof(address));
    std::memcpy(&function, &address, sizeof(function));
    return function;
}
void setting(const std::wstring& ini, const wchar_t* key, const wchar_t* value) {
    check(WritePrivateProfileStringW(L"Reporting", key, value, ini.c_str()) != 0,
          "write fixture configuration");
}
std::vector<HANDLE> own_helpers() {
    std::vector<HANDLE> children;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    check(snapshot != INVALID_HANDLE_VALUE, "snapshot processes");
    PROCESSENTRY32W item{};
    item.dwSize = sizeof(item);
    if (Process32FirstW(snapshot, &item)) {
        do {
            if (item.th32ParentProcessID != GetCurrentProcessId() ||
                _wcsicmp(item.szExeFile, L"A2FOLogMailerHelper.exe") != 0) continue;
            HANDLE child = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION |
                                       PROCESS_TERMINATE, FALSE, item.th32ProcessID);
            if (child) children.push_back(child);
        } while (Process32NextW(snapshot, &item));
    }
    CloseHandle(snapshot);
    return children;
}
void close_all(std::vector<HANDLE>& handles) {
    for (HANDLE handle : handles) CloseHandle(handle);
    handles.clear();
}
bool no_own_helper() {
    auto children = own_helpers();
    bool none = children.empty();
    close_all(children);
    return none;
}

struct Fixture {
    std::wstring root, modules, helper, ini;
    HMODULE dll = nullptr;
    A2FO_ModuleShutdownFn shutdown = nullptr;
    HANDLE child = nullptr;
    bool initialized = false;
    bool child_exited_cleanly = false;
    bool cleanup_verified = true;

    void stop_helper() {
        if (initialized && shutdown) shutdown();
        initialized = false;
        if (child) {
            DWORD result = WaitForSingleObject(child, 5000);
            if (result != WAIT_OBJECT_0) {
                std::fprintf(stderr, "helper did not cancel; terminating fixture child\n");
                TerminateProcess(child, 9);
                result = WaitForSingleObject(child, 5000);
            }
            DWORD code = 1;
            child_exited_cleanly = result == WAIT_OBJECT_0 &&
                GetExitCodeProcess(child, &code) && code == 0;
            if (result != WAIT_OBJECT_0) cleanup_verified = false;
            CloseHandle(child);
            child = nullptr;
        }
        // Also handle a failure after CreateProcess but before a child handle was captured.
        const ULONGLONG deadline = GetTickCount64() + 15000;
        while (GetTickCount64() < deadline) {
            std::vector<HANDLE> children;
            try {
                children = own_helpers();
            } catch (const std::exception&) {
                Sleep(100);
                continue;
            }
            if (children.empty()) return;
            for (HANDLE process : children) {
                DWORD result = WaitForSingleObject(process, 1000);
                if (result != WAIT_OBJECT_0) {
                    std::fprintf(stderr, "terminating uncaptured fixture child\n");
                    TerminateProcess(process, 9);
                    result = WaitForSingleObject(process, 1000);
                }
                if (result != WAIT_OBJECT_0) cleanup_verified = false;
            }
            close_all(children);
            Sleep(100);
        }
        cleanup_verified = false;
        std::fprintf(stderr, "could not confirm fixture helper termination\n");
    }
    ~Fixture() {
        stop_helper();
        if (dll) FreeLibrary(dll);
        if (!ini.empty()) DeleteFileW(ini.c_str());
        if (!helper.empty()) DeleteFileW(helper.c_str());
        if (!modules.empty()) RemoveDirectoryW(modules.c_str());
        if (!root.empty()) RemoveDirectoryW(root.c_str());
    }
};
} // namespace

int wmain(int argc, wchar_t** argv) {
    try {
        check(argc == 3, "usage: log_mailer_loader_smoke DLL HELPER");
        const std::wstring dll_path = full_path(argv[1]);
        const std::wstring helper_path = full_path(argv[2]);
        Fixture fixture;
        wchar_t temporary[MAX_PATH]{}, name[MAX_PATH]{};
        const DWORD temp_length = GetTempPathW(MAX_PATH, temporary);
        check(temp_length != 0 && temp_length < MAX_PATH, "get temp path");
        check(GetTempFileNameW(temporary, L"lml", 0, name) != 0, "reserve fixture path");
        check(DeleteFileW(name) && CreateDirectoryW(name, nullptr), "create fixture root");
        fixture.root = name;
        fixture.modules = join(fixture.root, L"modules");
        fixture.helper = join(fixture.modules, L"A2FOLogMailerHelper.exe");
        fixture.ini = join(fixture.root, L"A2FOLogMailer.ini");
        check(CreateDirectoryW(fixture.modules.c_str(), nullptr) != 0, "create modules directory");
        check(CopyFileW(helper_path.c_str(), fixture.helper.c_str(), TRUE) != 0,
              "copy helper into fixture");
        const int bytes = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS,
            fixture.root.c_str(), -1, nullptr, 0, nullptr, nullptr);
        check(bytes > 0, "size fixture ACP path");
        root_ansi.resize(bytes);
        BOOL substituted = FALSE;
        check(WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, fixture.root.c_str(),
              -1, root_ansi.data(), bytes, nullptr, &substituted) != 0 && !substituted,
              "encode fixture ACP path exactly");
        fixture.dll = LoadLibraryW(dll_path.c_str());
        check(fixture.dll != nullptr, "LoadLibrary of real A2FOLogMailer DLL");
        const auto init = export_symbol<A2FO_ModuleInitFn>(fixture.dll, "A2FO_ModuleInit");
        fixture.shutdown = export_symbol<A2FO_ModuleShutdownFn>(fixture.dll, "A2FO_ModuleShutdown");
        check(init && fixture.shutdown, "resolve DLL lifecycle exports");
        A2FO_ModuleApi api{};
        api.struct_size = sizeof(api);
        api.api_version = A2FO_MODULE_API_VERSION;
        api.log = log_message;
        api.root_directory = root_directory;

        check(init(&api), "initialize DLL without configuration");
        fixture.initialized = true;
        check(messages.find("Disabled:") != std::string::npos,
              "disabled branch logged");
        check(no_own_helper(), "disabled branch started no helper");
        fixture.stop_helper();

        setting(fixture.ini, L"Enabled", L"1");
        setting(fixture.ini, L"Mode", L"crash_only");
        setting(fixture.ini, L"RelayUrl", L"https://127.0.0.1:1/v1/crash-reports");
        setting(fixture.ini, L"SubmissionToken", L"FixtureOnlyDummySubmissionToken00000001");
        messages.clear();
        check(init(&api), "initialize real DLL with enabled fixture");
        fixture.initialized = true;
        check(messages.find("crash-only") != std::string::npos,
              "enabled crash-only branch logged");
        for (int attempt = 0; attempt < 100 && !fixture.child; ++attempt) {
            auto children = own_helpers();
            if (!children.empty()) {
                fixture.child = children.front();
                children.erase(children.begin());
            }
            close_all(children);
            if (!fixture.child) Sleep(50);
        }
        check(fixture.child != nullptr, "observe fixture helper child process");
        check(WaitForSingleObject(fixture.child, 100) == WAIT_TIMEOUT,
              "helper remains running while fixture parent is alive");
        fixture.stop_helper();
        check(fixture.child_exited_cleanly, "helper exited with status zero after shutdown");
        check(fixture.cleanup_verified, "fixture child cleanup confirmed");
        check(no_own_helper(), "helper gone after ModuleShutdown");
        check(GetFileAttributesW(join(fixture.root, L"CrashReports").c_str()) ==
              INVALID_FILE_ATTRIBUTES, "no crash report created");
        std::puts("log_mailer_loader_smoke: PASS");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "log_mailer_loader_smoke: FAIL: %s\n", error.what());
        return 1;
    }
}
