#pragma once

#include <cstdint>
#include <string>

namespace log_mailer {
constexpr char recipient[] = "aclaydon1@googlemail.com";
constexpr unsigned max_files = 32;
constexpr unsigned max_file_bytes = 64 * 1024;
constexpr unsigned max_total_bytes = 1024 * 1024;

inline bool allowed_name(std::string name, bool all_game_logs = false) {
    for (char& c : name) {
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    }
    if (name == "bugreport.txt") return true;
    if (all_game_logs) {
        if (name.size() < 5 || name.size() > 255 ||
            name.substr(name.size() - 4) != ".log" ||
            !((name[0] >= 'a' && name[0] <= 'z') ||
              (name[0] >= '0' && name[0] <= '9'))) return false;
        for (char c : name) {
            if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '-' || c == '.')) return false;
        }
        return true;
    }
    if (name.size() < 8 || name.substr(0, 4) != "a2fo" ||
        name.substr(name.size() - 4) != ".log") return false;
    for (std::size_t i = 4; i < name.size() - 4; ++i) {
        const char c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-')) return false;
    }
    return true;
}

// FO can catch a fatal exception, write bugreport.txt and then exit with 0.
// A fresh native report therefore counts independently of the process code.
inline bool is_crash(std::uint32_t code, bool fresh_native_report) {
    return fresh_native_report ||
        ((code & 0xc0000000u) == 0xc0000000u && code != 0xc000013au);
}

inline std::string base64(const std::string& bytes) {
    constexpr char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((bytes.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < bytes.size(); i += 3) {
        const unsigned a = static_cast<unsigned char>(bytes[i]);
        const unsigned b = i + 1 < bytes.size() ?
            static_cast<unsigned char>(bytes[i + 1]) : 0;
        const unsigned c = i + 2 < bytes.size() ?
            static_cast<unsigned char>(bytes[i + 2]) : 0;
        result += alphabet[a >> 2];
        result += alphabet[((a & 3) << 4) | (b >> 4)];
        result += i + 1 < bytes.size() ? alphabet[((b & 15) << 2) | (c >> 6)] : '=';
        result += i + 2 < bytes.size() ? alphabet[c & 63] : '=';
    }
    return result;
}
} // namespace log_mailer
