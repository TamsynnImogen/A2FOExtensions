#include "config.hpp"
#include "report.hpp"
#include <winhttp.h>
#include <algorithm>
#include <cwchar>
#include <vector>

namespace log_mailer {
struct Handle {
    HANDLE value = INVALID_HANDLE_VALUE;
    explicit Handle(HANDLE handle) : value(handle) {}
    ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
    Handle(const Handle&) = delete;
    Handle& operator=(const Handle&) = delete;
};
struct HttpHandle {
    HINTERNET value;
    explicit HttpHandle(HINTERNET handle) : value(handle) {}
    ~HttpHandle() { if (value) WinHttpCloseHandle(value); }
};
inline unsigned long long ticks(FILETIME time) {
    return (static_cast<unsigned long long>(time.dwHighDateTime) << 32) |
        time.dwLowDateTime;
}
bool fresh(const FILETIME& write, const FILETIME& start, const FILETIME& now) {
    return ticks(write) >= ticks(start) && ticks(write) <= ticks(now);
}
bool fresh_native(const std::wstring& root, FILETIME start, FILETIME now) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    return GetFileAttributesExW(join(root, L"bugreport.txt").c_str(),
        GetFileExInfoStandard, &data) &&
        !(data.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) &&
        (data.nFileSizeHigh || data.nFileSizeLow) && fresh(data.ftLastWriteTime, start, now);
}
struct Attachment { std::string name, bytes; };
std::vector<Attachment> collect(const std::wstring& root, FILETIME start, FILETIME now,
                              bool all_game_logs = false) {
    std::vector<std::wstring> names;
    WIN32_FIND_DATAW found{};
    const HANDLE search = FindFirstFileW(join(root, L"*").c_str(), &found);
    if (search == INVALID_HANDLE_VALUE) return {};
    do {
        if (found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
            continue;
        const std::wstring wide(found.cFileName);
        std::string name;
        bool ascii = true;
        for (wchar_t c : wide) { if (c > 127) { ascii = false; break; } name += char(c); }
        if (ascii && allowed_name(name, all_game_logs) && fresh(found.ftLastWriteTime, start, now))
            names.push_back(wide);
    } while (FindNextFileW(search, &found));
    FindClose(search);
    // Put the FO crash report first, then the principal extension log.
    auto priority = [](const std::wstring& name) {
        if (_wcsicmp(name.c_str(), L"bugreport.txt") == 0) return 0;
        if (_wcsicmp(name.c_str(), L"A2FOExtensions.log") == 0) return 1;
        return 2;
    };
    std::sort(names.begin(), names.end(), [&](const auto& a, const auto& b) {
        const auto pa = priority(a), pb = priority(b);
        return pa == pb ? a < b : pa < pb;
    });
    std::vector<Attachment> result;
    unsigned total = 0;
    for (const auto& name : names) {
        if (result.size() == max_files || total == max_total_bytes) break;
        Handle file(CreateFileW(join(root, name).c_str(), GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr));
        if (file.value == INVALID_HANDLE_VALUE) continue;
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(file.value, &info) ||
            (info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) ||
            !fresh(info.ftLastWriteTime, start, now)) continue;
        LARGE_INTEGER size{};
        if (!GetFileSizeEx(file.value, &size) || size.QuadPart <= 0) continue;
        const unsigned limit = std::min(max_file_bytes, max_total_bytes - total);
        const unsigned count = static_cast<unsigned>(
            std::min<LONGLONG>(size.QuadPart, limit));
        // Preserve the beginning of FO's stack report; retain the tail of long logs.
        if (_wcsicmp(name.c_str(), L"bugreport.txt") != 0 && size.QuadPart > count) {
            LARGE_INTEGER offset{};
            offset.QuadPart = size.QuadPart - count;
            if (!SetFilePointerEx(file.value, offset, nullptr, FILE_BEGIN)) continue;
        }
        std::string content(count, '\0');
        DWORD got = 0;
        if (!ReadFile(file.value, content.data(), count, &got, nullptr) || got == 0) continue;
        content.resize(got);
        result.push_back({std::string(name.begin(), name.end()), std::move(content)});
        total += got;
    }
    return result;
}
std::string payload(DWORD code, const std::vector<Attachment>& files) {
    std::string json = "{\"version\":1,\"exit_code\":" + std::to_string(code) + ",\"files\":[";
    bool first = true;
    for (const auto& file : files) {
        if (!first) json += ',';
        first = false;
        json += "{\"name\":\"" + file.name + "\",\"content_base64\":\"" +
            base64(file.bytes) + "\"}";
    }
    return json + "]}";
}
bool upload(const Config& config, const std::string& json) {
    if (!config.enabled || !config.ready()) return false;
    URL_COMPONENTS url{};
    url.dwStructSize = sizeof(url);
    url.dwSchemeLength = url.dwHostNameLength = url.dwUrlPathLength =
        url.dwExtraInfoLength = url.dwUserNameLength = url.dwPasswordLength = DWORD(-1);
    if (!WinHttpCrackUrl(config.url.c_str(), 0, 0, &url) ||
        url.nScheme != INTERNET_SCHEME_HTTPS || !url.dwHostNameLength ||
        url.dwUserNameLength || url.dwPasswordLength || url.dwExtraInfoLength ||
        std::wstring(url.lpszUrlPath, url.dwUrlPathLength) != L"/v1/crash-reports")
        return false;
    HttpHandle session(WinHttpOpen(L"A2FOLogMailer/1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session.value || !WinHttpSetTimeouts(session.value, 5000, 5000, 10000, 10000))
        return false;
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!WinHttpSetOption(session.value, WINHTTP_OPTION_REDIRECT_POLICY,
        &redirects, sizeof(redirects))) return false;
    const std::wstring host(url.lpszHostName, url.dwHostNameLength);
    HttpHandle connection(WinHttpConnect(session.value, host.c_str(), url.nPort, 0));
    if (!connection.value) return false;
    HttpHandle request(WinHttpOpenRequest(connection.value, L"POST", L"/v1/crash-reports",
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE));
    if (!request.value) return false;
    // Do not silently use the player's Windows login credentials for HTTP authentication.
    DWORD autologon = WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    if (!WinHttpSetOption(request.value, WINHTTP_OPTION_AUTOLOGON_POLICY,
        &autologon, sizeof(autologon))) return false;
    const std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer " +
        config.token + L"\r\n";
    if (!WinHttpSendRequest(request.value, headers.c_str(), DWORD(-1),
        const_cast<char*>(json.data()), static_cast<DWORD>(json.size()),
        static_cast<DWORD>(json.size()), 0) ||
        !WinHttpReceiveResponse(request.value, nullptr)) return false;
    DWORD status = 0, size = sizeof(status);
    return WinHttpQueryHeaders(request.value,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX) && status == 202;
}
bool write_file(const std::wstring& path, const std::string& bytes) {
    Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.value == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    return WriteFile(file.value, bytes.data(), static_cast<DWORD>(bytes.size()),
        &written, nullptr) && written == bytes.size();
}
using UploadFn = bool (*)(const Config&, const std::string&);
int watch(HANDLE parent, HANDLE cancel, const std::wstring& root, UploadFn send = &upload) {
    const auto original = load_config(root);
    if (!original.enabled || !original.ready()) return 0;
    FILETIME start{}, exit{}, kernel{}, user{};
    if (!GetProcessTimes(parent, &start, &exit, &kernel, &user)) return 2;
    HANDLE waits[]{cancel, parent};
    for (;;) {
        const DWORD result = WaitForMultipleObjects(2, waits, FALSE, 500);
        if (result == WAIT_OBJECT_0) return 0;
        if (result == WAIT_OBJECT_0 + 1) break;
        if (result != WAIT_TIMEOUT) return 2;
        // Revoking consent while the session runs stops the watcher promptly.
        if (!load_config(root).enabled) return 0;
    }
    DWORD code = 0;
    if (!GetExitCodeProcess(parent, &code)) return 2;
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    auto config = load_config(root);
    if (!config.enabled || !config.ready() || config.url != original.url ||
        config.token != original.token) return 0;
    if (!config.all_sessions && !is_crash(code, fresh_native(root, start, now))) return 0;
    const auto files = collect(root, start, now, config.all_sessions);
    const bool collected_all_sessions = config.all_sessions;
    if (files.empty()) return 0;
    const std::string json = payload(code, files);
    const auto directory = join(root, L"CrashReports");
    if (!CreateDirectoryW(directory.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 3;
    const DWORD attrs = GetFileAttributesW(directory.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || !(attrs & FILE_ATTRIBUTE_DIRECTORY) ||
        (attrs & FILE_ATTRIBUTE_REPARSE_POINT)) return 3;
    const auto stem = join(directory, L"report-" + std::to_wstring(ticks(start)) + L"-" +
        std::to_wstring(GetCurrentProcessId()));
    // Preserve the report before network I/O; a failed delivery never erases logs.
    if (!write_file(stem + L".json", json)) return 3;
    config = load_config(root);
    if (!config.enabled || config.url != original.url || config.token != original.token ||
        config.all_sessions != collected_all_sessions ||
        WaitForSingleObject(cancel, 0) == WAIT_OBJECT_0) {
        write_file(stem + L".status.txt", "Not sent: consent, mode or relay settings changed.\r\n");
        return 0;
    }
    const bool sent = send(config, json);
    write_file(stem + L".status.txt", sent ?
        "Relay accepted email delivery.\r\n" :
        "Delivery failed. Report retained locally; no automatic retries.\r\n");
    return sent ? 0 : 4;
}
} // namespace log_mailer

#ifndef LOG_MAILER_NO_MAIN
int wmain(int argc, wchar_t** argv) {
    if (argc != 5 || std::wcscmp(argv[1], L"--watch") != 0) return 2;
    wchar_t *end_parent = nullptr, *end_cancel = nullptr;
    const auto parent_value = std::wcstoull(argv[2], &end_parent, 10);
    const auto cancel_value = std::wcstoull(argv[3], &end_cancel, 10);
    if (!parent_value || !cancel_value || *end_parent || *end_cancel ||
        parent_value > UINTPTR_MAX || cancel_value > UINTPTR_MAX) return 2;
    log_mailer::Handle parent(reinterpret_cast<HANDLE>(std::uintptr_t(parent_value)));
    log_mailer::Handle cancel(reinterpret_cast<HANDLE>(std::uintptr_t(cancel_value)));
    return log_mailer::watch(parent.value, cancel.value, argv[4]);
}
#endif
