#include "../renderer/refit/startup.hpp"
#include "../core/extension_roots.hpp"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace a2fo::native11_capture {
unsigned exact_image_calls = 0;
bool exact_image_file(HMODULE, const unsigned char*) noexcept {
    ++exact_image_calls;
    return false;
}
}

namespace {
constexpr const char* CHILD = "Refit Contract Child";
std::vector<std::string> messages;

void need(bool yes, const char* why) {
    if (!yes) {
        std::fprintf(stderr, "FAIL: %s\n", why);
        std::exit(1);
    }
}
void log_message(const std::string& value) { messages.push_back(value); }
void directory(const std::string& path) {
    need(CreateDirectoryA(path.c_str(), nullptr) != FALSE, "create task-owned directory");
}
void file(const std::string& path, const std::string& contents) {
    HANDLE handle = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                                FILE_ATTRIBUTE_NORMAL, nullptr);
    need(handle != INVALID_HANDLE_VALUE, "create task-owned fixture file");
    DWORD written = 0;
    const BOOL okay = WriteFile(handle, contents.data(),
                                static_cast<DWORD>(contents.size()), &written, nullptr);
    need(CloseHandle(handle) != FALSE, "close task-owned fixture file");
    need(okay && written == contents.size(), "write full fixture file");
}
std::string task_root() {
    char prefix[32768]{};
    const DWORD n = GetEnvironmentVariableA("WINEPREFIX", prefix, sizeof(prefix));
    need(n > 0 && n < sizeof(prefix) &&
         std::strstr(prefix, "a2fo-refit-startup-policy-prefix-") != nullptr,
         "run only in a dedicated startup-policy Wine prefix");
    char temp[MAX_PATH + 1]{};
    const DWORD count = GetTempPathA(MAX_PATH, temp);
    need(count > 0 && count < MAX_PATH, "private prefix temp path");
    char unique[MAX_PATH + 1]{};
    need(GetTempFileNameA(temp, "rsp", 0, unique) != 0,
         "reserve unique task-owned temp root");
    need(DeleteFileA(unique) != FALSE, "remove reserved temp file");
    directory(unique);
    return unique;
}
std::string case_root(const std::string& task, const char* name,
                      const char* backend, const char* parent) {
    const std::string root = task + "\\" + name;
    directory(root);
    directory(root + "\\Mods");
    const std::string child = root + "\\Mods\\" + CHILD;
    directory(child);
    file(child + "\\info.ini", std::string("[mod]\r\nParentMod=") + parent + "\r\n");
    file(child + "\\A2FORenderer.ini",
         std::string("[D3D8Compat]\r\nBackend=") + backend + "\r\n");
    return root;
}
void unchanged(unsigned before, const char* why) {
    need(a2fo::native11_capture::exact_image_calls == before, why);
    need(GetModuleHandleA("A2FORefitDisplay.dll") == nullptr &&
         GetModuleHandleA("A2FORefitRenderer.dll") == nullptr &&
         GetModuleHandleA("UI.asi") == nullptr,
         "negative gateway loaded no Refit helper or UI");
}
void run_case(HMODULE image, const std::string& root, bool handled,
              const char* expected_log, const char* name) {
    messages.clear();
    const unsigned hashes = a2fo::native11_capture::exact_image_calls;
    const bool result = a2fo::refit::install_startup(image, root, log_message);
    if (result != handled) {
        std::fprintf(stderr, "FAIL: %s handled=%d expected=%d\n", name,
                     result, handled);
        std::exit(1);
    }
    if (expected_log) {
        need(messages.size() == 1 && messages[0].find(expected_log) != std::string::npos,
             "expected refusal diagnostic");
    } else {
        need(messages.empty(), "legacy request produced no Refit diagnostic");
    }
    unchanged(hashes, "negative gateway invoked exact SHA provider");
    std::printf("PASS: %s handled=%d log=%lu hashes=%u\n", name, result,
                static_cast<unsigned long>(messages.size()),
                a2fo::native11_capture::exact_image_calls);
}
}

int main(int argc, char** argv) {
    need(a2fo::active_mod_from_command_line(GetCommandLineA()) == CHILD,
         "launch fixture with /mod quoted child name");
    const std::string temp = task_root();
    const HMODULE exe = GetModuleHandleA(nullptr);
    need(exe != nullptr, "fixture executable module");
    bool d3d9 = false;
    for (int i = 1; i < argc; ++i)
        d3d9 = d3d9 || _stricmp(argv[i], "/d3d9") == 0 ||
                _stricmp(argv[i], "-d3d9") == 0;
    if (d3d9) {
        run_case(exe, case_root(temp, "d3d9-unrequested", "dxvk", ".."),
                 false, nullptr, "D3D9 without native request stays legacy");
        run_case(exe, case_root(temp, "d3d9-native", "refit-vulkan-test", ".."),
                 true, "FO D3D9 mode requires a separate device bridge; legacy renderer retained",
                 "D3D9 native request refused before parent and profile checks");
        need(a2fo::native11_capture::exact_image_calls == 0,
             "D3D9 refusals avoided exact SHA work");
        std::printf("PASS: startup policy D3D9 gate; task-owned fixture root=%s\n", temp.c_str());
        return 0;
    }
    run_case(exe, case_root(temp, "normal", "dxvk", "FO4"), false, nullptr,
             "unrequested backend stays legacy");
    run_case(exe, case_root(temp, "malformed", "refit-vulkan-test-extra", "FO4"),
             false, nullptr, "malformed backend stays legacy");
    run_case(exe, case_root(temp, "parent-traversal", "refit-vulkan-test", ".."),
             true, "requires an explicit child mod", "unsafe parent refused");
    run_case(exe, case_root(temp, "parent-self", "refit-d3d12-test", CHILD),
             true, "requires an explicit child mod", "self parent refused");
    run_case(exe, case_root(temp, "vulkan-unsupported", "refit-vulkan-test", "FO4"),
             true, "Unsupported executable/live mode profile",
             "Vulkan request on unsupported fixture EXE refused");
    run_case(exe, case_root(temp, "d3d12-unsupported", "refit-d3d12-test", "FO4"),
             true, "Unsupported executable/live mode profile",
             "D3D12 request on unsupported fixture EXE refused");
    run_case(reinterpret_cast<HMODULE>(static_cast<std::uintptr_t>(1)),
             case_root(temp, "invalid-image", "refit-vulkan-test", "FO4"),
             true, "Unsupported executable/live mode profile",
             "unmapped HMODULE rejected without crash");
    need(a2fo::native11_capture::exact_image_calls == 0,
         "all early refusals avoided exact SHA work");
    std::printf("PASS: startup policy negative gate; task-owned fixture root=%s\n", temp.c_str());
}
