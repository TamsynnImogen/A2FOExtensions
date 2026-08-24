/*
 * File: tools/a2fo_renderer_helper.cpp
 * Purpose: Apply a saved renderer choice after Armada releases d3d9.dll.
 */

#include <windows.h>

#include "../core/build_identity.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace {

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

bool file_exists(const std::string& path) {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
           (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool ensure_directory(const std::string& path) {
    if (path.empty()) return false;
    const DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
    const std::size_t slash = path.find_last_of("\\/");
    if (slash != std::string::npos && slash > 0 &&
        !ensure_directory(path.substr(0, slash))) {
        return false;
    }
    return CreateDirectoryA(path.c_str(), nullptr) != FALSE ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

void log_line(const std::string& data_root, const std::string& text) {
    const std::string path = join_path(data_root, "A2FORenderer.log");
    HANDLE file = CreateFileA(path.c_str(), FILE_APPEND_DATA,
                              FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                              OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    SYSTEMTIME now{};
    GetLocalTime(&now);
    char prefix[64]{};
    std::snprintf(prefix, sizeof(prefix),
                  "%04u-%02u-%02u %02u:%02u:%02u ", now.wYear, now.wMonth,
                  now.wDay, now.wHour, now.wMinute, now.wSecond);
    const std::string output = std::string(prefix) + text + "\r\n";
    DWORD written = 0;
    WriteFile(file, output.data(), static_cast<DWORD>(output.size()),
              &written, nullptr);
    CloseHandle(file);
}

bool same_file_contents(const std::string& left, const std::string& right) {
    HANDLE a = CreateFileA(left.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (a == INVALID_HANDLE_VALUE) return false;
    HANDLE b = CreateFileA(right.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (b == INVALID_HANDLE_VALUE) {
        CloseHandle(a);
        return false;
    }

    LARGE_INTEGER a_size{};
    LARGE_INTEGER b_size{};
    bool equal = GetFileSizeEx(a, &a_size) && GetFileSizeEx(b, &b_size) &&
                 a_size.QuadPart == b_size.QuadPart;
    std::array<std::uint8_t, 64 * 1024> a_buffer{};
    std::array<std::uint8_t, 64 * 1024> b_buffer{};
    while (equal) {
        DWORD a_read = 0;
        DWORD b_read = 0;
        if (!ReadFile(a, a_buffer.data(), a_buffer.size(), &a_read, nullptr) ||
            !ReadFile(b, b_buffer.data(), b_buffer.size(), &b_read, nullptr) ||
            a_read != b_read ||
            std::memcmp(a_buffer.data(), b_buffer.data(), a_read) != 0) {
            equal = false;
            break;
        }
        if (a_read == 0) break;
    }
    CloseHandle(b);
    CloseHandle(a);
    return equal;
}

void write_result(const std::string& ini, const char* backend,
                  const std::string& error) {
    if (error.empty()) {
        WritePrivateProfileStringA("Renderer", "AppliedBackend", backend,
                                   ini.c_str());
        WritePrivateProfileStringA("Renderer", "LastError", "", ini.c_str());
    } else {
        WritePrivateProfileStringA("Renderer", "LastError", error.c_str(),
                                   ini.c_str());
    }
}

bool replace_active_from(const std::string& source,
                         const std::string& active,
                         std::string* error) {
    if (!error) return false;
    error->clear();
    if (!file_exists(active)) {
        if (CopyFileA(source.c_str(), active.c_str(), TRUE)) return true;
        *error = "Could not create Data\\d3d9.dll (Windows error " +
                 std::to_string(GetLastError()) + ")";
        return false;
    }

    const std::string staging = active + ".a2fo-new";
    if (file_exists(staging) && !DeleteFileA(staging.c_str())) {
        *error = "Could not clear the renderer staging file (Windows error " +
                 std::to_string(GetLastError()) + ")";
        return false;
    }
    if (!CopyFileA(source.c_str(), staging.c_str(), TRUE)) {
        *error = "Could not stage the renderer DLL (Windows error " +
                 std::to_string(GetLastError()) + ")";
        return false;
    }
    if (!ReplaceFileA(active.c_str(), staging.c_str(), nullptr,
                      REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)) {
        const DWORD replace_error = GetLastError();
        DeleteFileA(staging.c_str());
        *error = "Could not replace Data\\d3d9.dll (Windows error " +
                 std::to_string(replace_error) + ")";
        return false;
    }
    return true;
}

bool preserve_system_renderer(const std::string& active,
                              const std::string& backup,
                              std::string* error) {
    if (!error || !file_exists(active)) return true;
    if (file_exists(backup)) {
        if (same_file_contents(active, backup)) return true;
        *error = "The saved system d3d9.dll differs from the active wrapper";
        return false;
    }
    const std::size_t slash = backup.find_last_of("\\/");
    if (slash == std::string::npos ||
        !ensure_directory(backup.substr(0, slash))) {
        *error = "Could not create Data\\renderers\\system";
        return false;
    }
    if (!CopyFileA(active.c_str(), backup.c_str(), TRUE) ||
        !same_file_contents(active, backup)) {
        *error = "Could not preserve the existing system renderer (Windows error " +
                 std::to_string(GetLastError()) + ")";
        return false;
    }
    return true;
}

int apply_renderer(const std::string& data_root) {
    const std::string ini = join_path(data_root, "A2FORenderer.ini");
    const std::string active = join_path(data_root, "d3d9.dll");
    const std::string payload =
        join_path(data_root, "renderers\\dxvk\\d3d9.dll");
    const std::string system_backup =
        join_path(data_root, "renderers\\system\\d3d9.dll");
    char backend[32]{};
    GetPrivateProfileStringA("Renderer", "Backend", "system", backend,
                             sizeof(backend), ini.c_str());

    if (_stricmp(backend, "dxvk") == 0) {
        if (!file_exists(payload)) {
            const std::string error = "DXVK payload is missing";
            write_result(ini, "dxvk", error);
            log_line(data_root, error);
            return 2;
        }
        if (file_exists(active)) {
            if (!same_file_contents(active, payload)) {
                std::string error;
                if (!preserve_system_renderer(
                        active, system_backup, &error)) {
                    write_result(ini, "dxvk", error);
                    log_line(data_root, error);
                    return 3;
                }
                if (!replace_active_from(payload, active, &error)) {
                    write_result(ini, "dxvk", error);
                    log_line(data_root, error);
                    return 4;
                }
            }
        } else if (!CopyFileA(payload.c_str(), active.c_str(), TRUE)) {
            const std::string error = "Could not activate DXVK (Windows error " +
                                      std::to_string(GetLastError()) + ")";
            write_result(ini, "dxvk", error);
            log_line(data_root, error);
            return 5;
        }
        if (!same_file_contents(active, payload)) {
            const std::string error =
                "DXVK replacement verification failed";
            write_result(ini, "dxvk", error);
            log_line(data_root, error);
            return 6;
        }
        write_result(ini, "dxvk", "");
        log_line(data_root,
                 file_exists(system_backup)
                     ? "Applied DXVK (Vulkan); preserved the previous system wrapper"
                     : "Applied DXVK (Vulkan)");
        return 0;
    }

    if (file_exists(active)) {
        if (file_exists(system_backup) &&
            same_file_contents(active, system_backup)) {
            // The preserved system wrapper is already active.
        } else if (file_exists(payload) &&
                   same_file_contents(active, payload)) {
            if (file_exists(system_backup)) {
                std::string error;
                if (!replace_active_from(system_backup, active, &error)) {
                    write_result(ini, "system", error);
                    log_line(data_root, error);
                    return 7;
                }
            } else if (!DeleteFileA(active.c_str())) {
                const std::string error =
                    "Could not deactivate DXVK (Windows error " +
                    std::to_string(GetLastError()) + ")";
                write_result(ini, "system", error);
                log_line(data_root, error);
                return 8;
            }
        } else if (file_exists(system_backup)) {
            const std::string error =
                "Data\\d3d9.dll matches neither the managed DXVK nor the saved system wrapper";
            write_result(ini, "system", error);
            log_line(data_root, error);
            return 9;
        }
    } else if (file_exists(system_backup)) {
        if (!CopyFileA(system_backup.c_str(), active.c_str(), TRUE)) {
            const std::string error =
                "Could not restore the system renderer (Windows error " +
                std::to_string(GetLastError()) + ")";
            write_result(ini, "system", error);
            log_line(data_root, error);
            return 10;
        }
    }
    write_result(ini, "system", "");
    log_line(data_root,
             file_exists(system_backup)
                 ? "Restored System Direct3D 9 wrapper"
                 : "Applied System Direct3D 9 / WineD3D");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    DWORD wait_pid = 0;
    std::string data_root;
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--wait-pid") == 0 && index + 1 < argc) {
            wait_pid = static_cast<DWORD>(
                std::strtoul(argv[++index], nullptr, 10));
        } else if (std::strcmp(argv[index], "--data-root") == 0 &&
                   index + 1 < argc) {
            data_root = argv[++index];
        }
    }
    if (data_root.empty()) return 1;

    log_line(data_root,
             std::string("Renderer helper build: ") + A2FO_BUILD_ID);

    if (wait_pid != 0) {
        log_line(data_root, "Renderer helper started; waiting for Armada PID " +
                                std::to_string(wait_pid));
        HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, wait_pid);
        if (process) {
            WaitForSingleObject(process, INFINITE);
            CloseHandle(process);
        } else {
            // The parent may have finished between CreateProcess and
            // OpenProcess. A short delay also lets Wine release the DLL map.
            log_line(data_root,
                     "Armada PID was already unavailable; applying after a "
                     "short release delay");
            Sleep(500);
        }
    }
    return apply_renderer(data_root);
}
