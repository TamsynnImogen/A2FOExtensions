#include "extension_roots.hpp"
#include "../rust/roots_ffi.h"
#include <cstdint>
#include <exception>
#include <fstream>
#include <sstream>
#if defined(_WIN32)
#include <windows.h>
#endif
namespace a2fo {
namespace {
A2foRsRootBytes bytes(const std::string &s) noexcept {
    return {reinterpret_cast<const std::uint8_t *>(s.data()), s.size()};
}
struct Text {
    std::string value;
    std::exception_ptr error;
};
void emit(void *p, std::uint32_t, const std::uint8_t *data, std::size_t n) noexcept {
    auto &out = *static_cast<Text *>(p);
    try {
        out.value.assign(reinterpret_cast<const char *>(data), n);
    } catch (...) {
        out.error = std::current_exception();
    }
}
std::string result(Text &out) {
    if (out.error)
        std::rethrow_exception(out.error);
    return std::move(out.value);
}
} // namespace
std::string active_mod_from_command_line(const std::string &v) {
    Text out;
    a2fo_rs_roots_active(bytes(v), emit, &out);
    return result(out);
}
std::string parent_mod_from_info(const std::string &v) {
    Text out;
    a2fo_rs_roots_parent(bytes(v), emit, &out);
    return result(out);
}
FleetOpsInfoDefaults fleet_ops_defaults_from_info(const std::string &v) {
    Text out;
    std::int32_t speed = 0;
    a2fo_rs_roots_defaults(bytes(v), &speed, emit, &out);
    FleetOpsInfoDefaults r;
    r.settings_directory = result(out);
    r.has_default_game_speed = speed != 0;
    r.default_game_speed = speed;
    return r;
}
std::string resolve_fleet_ops_settings_directory(const std::string &n, const std::string &v,
                                                 const std::string &f, const std::string &s) {
    Text out;
    a2fo_rs_roots_settings(bytes(n), bytes(v), bytes(f), bytes(s), emit, &out);
    return result(out);
}
bool safe_mod_directory_name(const std::string &v) { return a2fo_rs_roots_safe(bytes(v)) != 0; }
#if defined(_WIN32)
namespace {
struct Discover {
    ExtensionRootDiscovery value;
    std::exception_ptr error;
};
void discovered(void *p, std::uint32_t kind, const std::uint8_t *data, std::size_t n) noexcept {
    auto &out = *static_cast<Discover *>(p);
    try {
        std::string text(reinterpret_cast<const char *>(data), n);
        if (kind == 0)
            out.value.roots.push_back(std::move(text));
        else if (kind == 1)
            out.value.diagnostics.push_back(std::move(text));
        else
            out.value.active_mod = std::move(text);
    } catch (...) {
        out.error = std::current_exception();
    }
}
std::uint8_t directory(void *p, const std::uint8_t *data, std::size_t n) noexcept {
    try {
        std::string path(reinterpret_cast<const char *>(data), n);
        DWORD a = GetFileAttributesA(path.c_str());
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
    } catch (...) {
        static_cast<Discover *>(p)->error = std::current_exception();
        return 0;
    }
}
void read(void *p, const std::uint8_t *data, std::size_t n, A2foRsRootEmit cb, void *c) noexcept {
    try {
        std::string path(reinterpret_cast<const char *>(data), n);
        std::ifstream f(path, std::ios::binary);
        if (!f)
            return;
        std::ostringstream s;
        s << f.rdbuf();
        const auto text = s.str();
        cb(c, 0, reinterpret_cast<const std::uint8_t *>(text.data()), text.size());
    } catch (...) {
        static_cast<Discover *>(p)->error = std::current_exception();
    }
}
} // namespace
ExtensionRootDiscovery discover_extension_roots(const std::string &data, const std::string &cmd) {
    Discover out;
    a2fo_rs_roots_discover(bytes(data), bytes(cmd), directory, read, &out, discovered, &out);
    if (out.error)
        std::rethrow_exception(out.error);
    return std::move(out.value);
}
#endif
} // namespace a2fo
