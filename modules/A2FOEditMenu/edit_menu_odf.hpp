/*
 * Parsed recursive edit-menu model shared by the runtime and unit tests.
 */

#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace a2fo::edit_menu {

constexpr std::size_t kEntryCount = 12;

// One native menu page. Armada exposes twelve visible slots per page, so the
// parsed representation intentionally has the same fixed capacity.
struct MenuNode {
    std::string source_name;
    std::string title;
    std::array<std::string, kEntryCount> build_items{};
    std::array<std::string, kEntryCount> items{};
    bool force_to_neutral = false;

    // A page containing buildItemX entries opens another level. Otherwise its
    // itemX entries are resolved as placeable object classes.
    bool is_submenu() const noexcept;
};

// Returns a lowercase basename with a .odf suffix. Directory components are
// deliberately discarded because Armada's edit-menu references are basename
// lookups through the virtual ODF filesystem.
std::string normalize_odf_name(const std::string& value);

// Parses the edit-menu commands needed by the recursive runtime. Unknown ODF
// commands are ignored, preserving compatibility with ordinary game ODFs.
bool parse_menu_node(const std::string& contents,
                     const std::string& source_name,
                     MenuNode* output,
                     std::string* error = nullptr);

}  // namespace a2fo::edit_menu
