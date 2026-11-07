#pragma once
#include "distribution.hpp"
#include <windows.h>
#include <cwchar>
#include <string>
#include <vector>

namespace log_mailer {
// Serialize a menu Save with watcher reads across processes. A failed Save must
// not briefly expose consent/mode changes before its rollback is complete.
struct PreferencesLock {
    HANDLE mutex = nullptr;
    bool held = false;
    explicit PreferencesLock(bool needed = true) {
        if (!needed) { held = true; return; }
        mutex = CreateMutexW(nullptr, FALSE, L"Local\\A2FO.LogMailer.Preferences");
        if (mutex) {
            const DWORD result = WaitForSingleObject(mutex, 5000);
            held = result == WAIT_OBJECT_0 || result == WAIT_ABANDONED;
        }
    }
    ~PreferencesLock() {
        if (held && mutex) ReleaseMutex(mutex);
        if (mutex) CloseHandle(mutex);
    }
    PreferencesLock(const PreferencesLock&) = delete;
    PreferencesLock& operator=(const PreferencesLock&) = delete;
};
inline std::wstring from_ansi(const std::string& text) {
    const int size = MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, nullptr, 0);
    if (!size) return {};
    std::vector<wchar_t> result(size);
    if (!MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, result.data(), size)) return {};
    return result.data();
}
inline std::wstring join(const std::wstring& root, const std::wstring& name) {
    return root + (root.empty() || root.back() == L'\\' || root.back() == L'/' ?
        L"" : L"\\") + name;
}
inline std::wstring config_path(const std::wstring& root) {
    return join(root, L"A2FOLogMailer.ini");
}
inline std::wstring read_setting(const std::wstring& path, const wchar_t* key) {
    wchar_t buffer[2048]{};
    const DWORD length = GetPrivateProfileStringW(
        L"Reporting", key, L"", buffer, 2048, path.c_str());
    // A truncated override is invalid, rather than absent. In particular, two
    // oversized overrides must not silently switch to the bundled receiver.
    return length < 2047 ? std::wstring(buffer, length) : std::wstring(1, L'\n');
}
struct Config {
    bool enabled = false;
    std::wstring url;
    std::wstring token;
    bool all_sessions = false;
    bool ready() const {
        const std::wstring route = L"/v1/crash-reports";
        if (url.compare(0, 8, L"https://") != 0 ||
            url.size() <= 8 + route.size() ||
            url.compare(url.size() - route.size(), route.size(), route) != 0 ||
            token.empty())
            return false;
        const auto authority = url.substr(8, url.size() - 8 - route.size());
        for (wchar_t c : authority) {
            if (!((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') ||
                  (c >= L'0' && c <= L'9') || c == L'.' || c == L'-' ||
                  c == L':' || c == L'[' || c == L']')) return false;
        }
        const auto colon = authority.rfind(L':');
        std::wstring host = authority;
        // Colon inside a bracketed IPv6 address is not a port separator.
        if (colon != std::wstring::npos && authority.back() != L']') {
            const auto port_text = authority.substr(colon + 1);
            if (port_text.empty() || port_text.size() > 5) return false;
            unsigned port = 0;
            for (wchar_t c : port_text) {
                if (c < L'0' || c > L'9') return false;
                port = port * 10 + unsigned(c - L'0');
            }
            if (port == 0 || port > 65535) return false;
            host = authority.substr(0, colon);
        }
        if (host.empty()) return false;
        if (host.front() == L'[') {
            if (host.back() != L']' || host.size() < 4) return false;
            for (std::size_t i = 1; i + 1 < host.size(); ++i) {
                const wchar_t c = host[i];
                if (!((c >= L'0' && c <= L'9') || (c >= L'a' && c <= L'f') ||
                      (c >= L'A' && c <= L'F') || c == L':' || c == L'.')) return false;
            }
        } else if (host.find_first_of(L":[]") != std::wstring::npos) return false;
        // A distribution submission token is printable ASCII; never a mailbox password.
        for (wchar_t c : token) if (c < 33 || c > 126) return false;
        return true;
    }
};
inline Config load_config(const std::wstring& root) {
    PreferencesLock lock;
    if (!lock.held) return {};
    const auto path = config_path(root);
    Config config;
    config.enabled = GetPrivateProfileIntW(
        L"Reporting", L"Enabled", 0, path.c_str()) == 1;
    config.url = read_setting(path, L"RelayUrl");
    config.token = read_setting(path, L"SubmissionToken");
    // Players only choose consent and mode. Use the distributor's bundled
    // receiver when neither override is supplied, including old blank templates.
    // Never send its token to a partially configured alternative receiver.
    if (config.url.empty() && config.token.empty()) {
        config.url = distribution::relay_url;
        config.token = distribution::submission_token;
    }
    config.all_sessions = read_setting(path, L"Mode") == L"all_sessions";
    return config;
}
// Update consent and mode together, preserving distributor relay settings and
// unknown keys. A snapshot lets the menu roll back if saving module selection fails.
inline bool preferences_section(const std::wstring& root, std::wstring& section) {
    PreferencesLock lock;
    if (!lock.held) return false;
    std::vector<wchar_t> buffer(32768);
    const DWORD count = GetPrivateProfileSectionW(L"Reporting", buffer.data(),
        static_cast<DWORD>(buffer.size()), config_path(root).c_str());
    if (count >= buffer.size() - 2) return false;
    section.assign(buffer.data(), count + 2);
    return true;
}
inline bool write_section(const std::wstring& root, const std::wstring& section) {
    PreferencesLock lock;
    if (!lock.held) return false;
    return WritePrivateProfileSectionW(L"Reporting", section.c_str(),
        config_path(root).c_str()) != FALSE;
}
inline bool save_preferences(const std::wstring& root, bool enabled, bool all_sessions,
                             const std::wstring& previous) {
    std::wstring section;
    for (const wchar_t* item = previous.c_str(); *item; item += std::wcslen(item) + 1) {
        if (_wcsnicmp(item, L"Enabled=", 8) == 0 || _wcsnicmp(item, L"Mode=", 5) == 0)
            continue;
        section += item;
        section += L'\0';
    }
    section += enabled ? L"Enabled=1" : L"Enabled=0";
    section += L'\0';
    section += all_sessions ? L"Mode=all_sessions" : L"Mode=crash_only";
    section += L'\0';
    section += L'\0';
    return write_section(root, section);
}
inline std::wstring quote(const std::wstring& value) {
    std::wstring result = L"\"";
    unsigned slashes = 0;
    for (wchar_t c : value) {
        if (c == L'\\') { ++slashes; continue; }
        result.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
        slashes = 0;
        result += c;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}
} // namespace log_mailer
