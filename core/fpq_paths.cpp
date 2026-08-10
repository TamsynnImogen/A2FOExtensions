/*
 * File: core/fpq_paths.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: FPQ archive metadata and traversal helpers used to discover mod assets inside packed extension containers.
 */

#include "fpq_paths.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <set>

namespace a2fo {
namespace {

// FPQ metadata layout (all integer fields are little-endian):
// header -> fixed-size hash table -> folder records -> file records -> names.
// File payloads follow these tables and are outside this parser's scope.
constexpr std::size_t kHeaderSize = 0x1c;
constexpr std::size_t kHashRecordSize = 12;
constexpr std::size_t kFolderRecordSize = 12;
constexpr std::size_t kFileRecordSize = 25;
constexpr std::uint32_t kNoFolder = 0xffffffffu;

bool read_u32(const std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::uint32_t& value) {
    if (offset > bytes.size() || bytes.size() - offset < 4) {
        return false;
    }
    value = static_cast<std::uint32_t>(bytes[offset]) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
            (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
    return true;
}

bool checked_add_mul(std::size_t base, std::uint32_t count, std::size_t width,
                     std::size_t& result) {
    if (count > (std::numeric_limits<std::size_t>::max() - base) / width) {
        return false;
    }
    result = base + static_cast<std::size_t>(count) * width;
    return true;
}

std::string lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool ends_with_odf(const std::string& name) {
    const std::string lower = lower_ascii(name);
    return lower.size() >= 4 && lower.compare(lower.size() - 4, 4, ".odf") == 0;
}

bool valid_component(const std::string& value) {
    return !value.empty() && value != "." && value != ".." &&
           value.find('/') == std::string::npos &&
           value.find('\\') == std::string::npos &&
           value.find('\0') == std::string::npos;
}

}  // namespace

FpqPathResult parse_fpq_odf_directories(const std::vector<std::uint8_t>& bytes) {
    // Parse untrusted archive metadata defensively. Mods can supply FPQs, so
    // every offset/count is bounds-checked before iterator or pointer use.
    FpqPathResult result;
    if (bytes.size() < kHeaderSize || bytes[0] != 'F' || bytes[1] != 'P' ||
        bytes[2] != 'Q' || bytes[3] != 0) {
        result.error = "missing FPQ header";
        return result;
    }

    std::uint32_t folder_count = 0;
    std::uint32_t hash_capacity = 0;
    std::uint32_t name_table_size = 0;
    if (!read_u32(bytes, 0x04, result.version) ||
        !read_u32(bytes, 0x08, folder_count) ||
        !read_u32(bytes, 0x0c, result.file_count) ||
        !read_u32(bytes, 0x10, hash_capacity) ||
        !read_u32(bytes, 0x18, name_table_size)) {
        result.error = "truncated FPQ header";
        return result;
    }
    if (result.version != 1 && result.version != 2) {
        result.error = "unsupported FPQ version";
        return result;
    }
    if (hash_capacity == 0 || (hash_capacity & (hash_capacity - 1)) != 0 ||
        hash_capacity <= result.file_count) {
        result.error = "invalid FPQ hash capacity";
        return result;
    }
    if (folder_count > 1000000 || result.file_count > 1000000 ||
        name_table_size > 128u * 1024u * 1024u) {
        result.error = "unreasonable FPQ metadata size";
        return result;
    }

    std::size_t folder_table = 0;
    std::size_t file_table = 0;
    std::size_t name_table = 0;
    std::size_t metadata_end = 0;
    if (!checked_add_mul(kHeaderSize, hash_capacity, kHashRecordSize, folder_table) ||
        !checked_add_mul(folder_table, folder_count, kFolderRecordSize, file_table) ||
        !checked_add_mul(file_table, result.file_count, kFileRecordSize, name_table) ||
        name_table > std::numeric_limits<std::size_t>::max() - name_table_size) {
        result.error = "FPQ metadata layout overflow";
        return result;
    }
    metadata_end = name_table + name_table_size;
    if (metadata_end > bytes.size()) {
        result.error = "truncated FPQ metadata";
        return result;
    }

    auto read_name = [&](std::uint32_t offset, std::uint32_t length,
                         std::string& name) -> bool {
        if (offset > name_table_size || length > name_table_size - offset) {
            return false;
        }
        const auto first = bytes.begin() + static_cast<std::ptrdiff_t>(name_table + offset);
        name.assign(first, first + length);
        return valid_component(name);
    };

    std::vector<std::string> folders;
    folders.reserve(folder_count);
    for (std::uint32_t index = 0; index < folder_count; ++index) {
        // Folder records are parent-first. Reconstruct each complete virtual
        // path as it is encountered so children can reference earlier rows.
        const std::size_t record = folder_table + static_cast<std::size_t>(index) * kFolderRecordSize;
        std::uint32_t parent = 0;
        std::uint32_t name_offset = 0;
        std::uint32_t name_length = 0;
        std::string name;
        if (!read_u32(bytes, record, parent) ||
            !read_u32(bytes, record + 4, name_offset) ||
            !read_u32(bytes, record + 8, name_length) ||
            !read_name(name_offset, name_length, name)) {
            result.error = "invalid FPQ folder record";
            return result;
        }
        if (parent == kNoFolder) {
            folders.push_back(name);
        } else if (parent < folders.size()) {
            folders.push_back(folders[parent] + "\\" + name);
        } else {
            result.error = "FPQ folder references a missing parent";
            return result;
        }
    }

    std::set<std::string> unique_paths;
    for (std::uint32_t index = 0; index < result.file_count; ++index) {
        // Only directory names are needed by the runtime scanner. Ignore
        // non-ODF files and deduplicate paths case-insensitively.
        const std::size_t record = file_table + static_cast<std::size_t>(index) * kFileRecordSize;
        std::uint32_t name_offset = 0;
        std::uint32_t name_length = 0;
        std::uint32_t folder_index = 0;
        std::string name;
        if (!read_u32(bytes, record + 12, name_offset) ||
            !read_u32(bytes, record + 16, name_length) ||
            !read_u32(bytes, record + 21, folder_index) ||
            !read_name(name_offset, name_length, name)) {
            result.error = "invalid FPQ file record";
            return result;
        }
        if (!ends_with_odf(name) || folder_index == kNoFolder) {
            continue;
        }
        if (folder_index >= folders.size()) {
            result.error = "FPQ file references a missing folder";
            return result;
        }
        std::string key = lower_ascii(folders[folder_index]);
        if (key == "odf" || key.compare(0, 4, "odf\\") == 0) {
            unique_paths.insert(key);
        }
    }

    result.odf_directories.assign(unique_paths.begin(), unique_paths.end());
    result.ok = true;
    return result;
}

}  // namespace a2fo
