#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

#include "../core/build_identity.hpp"

namespace {

std::string join_path(const std::string& root, const char* tail) {
    if (root.empty()) return tail ? std::string(tail) : std::string();
    if (!tail || !*tail) return root;
    const char last = root.back();
    if (last == '\\' || last == '/') return root + tail;
    return root + "\\" + tail;
}

bool file_exists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool same_file_contents(const std::string& left, const std::string& right) {
    HANDLE left_file = CreateFileA(
        left.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (left_file == INVALID_HANDLE_VALUE) return false;

    HANDLE right_file = CreateFileA(
        right.c_str(), GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (right_file == INVALID_HANDLE_VALUE) {
        CloseHandle(left_file);
        return false;
    }

    LARGE_INTEGER left_size{};
    LARGE_INTEGER right_size{};
    bool equal = GetFileSizeEx(left_file, &left_size) &&
                 GetFileSizeEx(right_file, &right_size) &&
                 left_size.QuadPart == right_size.QuadPart;

    std::array<std::uint8_t, 16 * 1024> left_buffer{};
    std::array<std::uint8_t, 16 * 1024> right_buffer{};
    while (equal) {
        DWORD left_read = 0;
        DWORD right_read = 0;
        if (!ReadFile(left_file, left_buffer.data(),
                      static_cast<DWORD>(left_buffer.size()), &left_read,
                      nullptr) ||
            !ReadFile(right_file, right_buffer.data(),
                      static_cast<DWORD>(right_buffer.size()), &right_read,
                      nullptr) ||
            left_read != right_read ||
            std::memcmp(left_buffer.data(), right_buffer.data(), left_read) != 0) {
            equal = false;
            break;
        }
        if (left_read == 0) break;
    }

    CloseHandle(right_file);
    CloseHandle(left_file);
    return equal;
}

std::string win32_error_text(DWORD error) {
    char buffer[256]{};
    const DWORD count = FormatMessageA(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr,
        error, 0, buffer, static_cast<DWORD>(sizeof(buffer)), nullptr);
    if (count == 0) return "Win32 error " + std::to_string(error);
    while (*buffer && (buffer[std::strlen(buffer) - 1] == '\r' ||
                       buffer[std::strlen(buffer) - 1] == '\n')) {
        buffer[std::strlen(buffer) - 1] = '\0';
    }
    return std::string(buffer) + " (" + std::to_string(error) + ")";
}

void append_log(const std::string& data_root, const std::string& message) {
    std::ofstream log(join_path(data_root, "A2FORendererHelper.log"),
                      std::ios::out | std::ios::app);
    if (!log) return;

    SYSTEMTIME now{};
    GetLocalTime(&now);
    char stamp[64]{};
    std::snprintf(stamp, sizeof(stamp),
                  "%04u-%02u-%02u %02u:%02u:%02u.%03u",
                  static_cast<unsigned>(now.wYear),
                  static_cast<unsigned>(now.wMonth),
                  static_cast<unsigned>(now.wDay),
                  static_cast<unsigned>(now.wHour),
                  static_cast<unsigned>(now.wMinute),
                  static_cast<unsigned>(now.wSecond),
                  static_cast<unsigned>(now.wMilliseconds));
    log << '[' << stamp << "] " << message << '\n';
}

bool write_ini_value(const std::string& ini, const char* key,
                     const std::string& value) {
    return WritePrivateProfileStringA("Renderer", key, value.c_str(),
                                      ini.c_str()) != FALSE;
}

void write_result(const std::string& data_root, const std::string& ini,
                  const char* applied_backend, const std::string& error) {
    if (applied_backend) {
        if (!write_ini_value(ini, "AppliedBackend", applied_backend)) {
            append_log(data_root,
                       "Could not write Renderer/AppliedBackend to " + ini);
        }
    }
    if (!write_ini_value(ini, "LastError", error)) {
        append_log(data_root, "Could not write Renderer/LastError to " + ini);
    }
}

bool wait_for_process(DWORD pid, const std::string& data_root) {
    if (pid == 0) return true;

    HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid);
    if (!process) {
        const DWORD error = GetLastError();
        // ERROR_INVALID_PARAMETER normally means the process exited before the
        // helper managed to open it. In that case it is safe to continue.
        if (error == ERROR_INVALID_PARAMETER) {
            append_log(data_root,
                       "Parent process had already exited; applying renderer now");
            return true;
        }
        append_log(data_root,
                   "OpenProcess failed for PID " + std::to_string(pid) + ": " +
                       win32_error_text(error));
        return false;
    }

    append_log(data_root,
               "Waiting for Armada process " + std::to_string(pid) + " to exit");
    const DWORD wait = WaitForSingleObject(process, INFINITE);
    CloseHandle(process);
    if (wait != WAIT_OBJECT_0) {
        append_log(data_root,
                   "WaitForSingleObject failed: " +
                       win32_error_text(GetLastError()));
        return false;
    }
    return true;
}

int apply_renderer(const std::string& data_root) {
    const std::string ini = join_path(data_root, "A2FORenderer.ini");
    const std::string active = join_path(data_root, "d3d9.dll");
    const std::string payload =
        join_path(data_root, "renderers\\dxvk\\d3d9.dll");

    char requested[32]{};
    GetPrivateProfileStringA("Renderer", "Backend", "system", requested,
                             static_cast<DWORD>(sizeof(requested)), ini.c_str());
    const bool want_dxvk = _stricmp(requested, "dxvk") == 0;

    append_log(data_root,
               std::string("Applying requested backend: ") +
                   (want_dxvk ? "dxvk" : "system"));

    if (want_dxvk) {
        if (!file_exists(payload)) {
            const std::string error =
                "DXVK payload is missing: renderers\\dxvk\\d3d9.dll";
            write_result(data_root, ini, nullptr, error);
            append_log(data_root, error);
            return 2;
        }

        if (file_exists(active)) {
            if (!same_file_contents(active, payload)) {
                const std::string error =
                    "Data\\d3d9.dll already exists and is not the managed DXVK "
                    "payload; refusing to overwrite it";
                write_result(data_root, ini, nullptr, error);
                append_log(data_root, error);
                return 3;
            }
        } else if (!CopyFileA(payload.c_str(), active.c_str(), TRUE)) {
            const DWORD code = GetLastError();
            const std::string error =
                "Could not install managed DXVK d3d9.dll: " +
                win32_error_text(code);
            write_result(data_root, ini, nullptr, error);
            append_log(data_root, error);
            return 4;
        }

        write_result(data_root, ini, "dxvk", "");
        append_log(data_root, "Managed DXVK renderer applied successfully");
        return 0;
    }

    // "system" only owns d3d9.dll when it is byte-identical to our packaged
    // DXVK payload. Never remove an unrelated wrapper such as ReShade,
    // dgVoodoo, or dxwrapper.
    if (file_exists(active) && file_exists(payload) &&
        same_file_contents(active, payload)) {
        if (!DeleteFileA(active.c_str())) {
            const DWORD code = GetLastError();
            const std::string error =
                "Could not remove managed DXVK d3d9.dll: " +
                win32_error_text(code);
            write_result(data_root, ini, nullptr, error);
            append_log(data_root, error);
            return 5;
        }
    }

    write_result(data_root, ini, "system", "");
    append_log(data_root, "System/unmanaged renderer selected successfully");
    return 0;
}

void usage() {
    std::fputs(
        "A2FORendererHelper --wait-pid <pid> --data-root <Data directory>\n",
        stderr);
}

}  // namespace

int main(int argc, char** argv) {
    DWORD wait_pid = 0;
    std::string data_root;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--wait-pid") == 0 && i + 1 < argc) {
            char* end = nullptr;
            const unsigned long value = std::strtoul(argv[++i], &end, 10);
            if (!end || *end != '\0') {
                usage();
                return 64;
            }
            wait_pid = static_cast<DWORD>(value);
        } else if (std::strcmp(argv[i], "--data-root") == 0 && i + 1 < argc) {
            data_root = argv[++i];
        } else {
            usage();
            return 64;
        }
    }

    if (data_root.empty()) {
        usage();
        return 64;
    }

    append_log(data_root,
               std::string("A2FORendererHelper starting; build=") +
                   A2FO_BUILD_ID);

    if (!wait_for_process(wait_pid, data_root)) {
        const std::string ini = join_path(data_root, "A2FORenderer.ini");
        const std::string error =
            "Renderer helper could not wait for Armada to exit safely";
        write_result(data_root, ini, nullptr, error);
        return 1;
    }

    return apply_renderer(data_root);
}
