/*
 * Host-testable parser for live Producer build-submenu commands.
 */

#pragma once

#include <array>
#include <cstddef>
#include <string>

namespace a2fo::build_submenu {

constexpr std::size_t kBuildItemCount = 57;

struct Page {
    std::array<std::string, kBuildItemCount> children{};

    bool empty() const noexcept;
};

struct Config {
    std::array<Page, kBuildItemCount> pages{};

    bool empty() const noexcept;
};

// Reads only buildItem<X>Refit<Y>. Ordinary buildItem rows and unrelated ODF
// commands are deliberately ignored. Indices are zero-based, matching the
// native Producer list.
bool parse_config(const std::string& contents, Config* output,
                  std::string* error = nullptr);

// Returns a bare object-class basename suitable for GameObjectClass::Find.
std::string normalize_object_name(const std::string& value);

}  // namespace a2fo::build_submenu
