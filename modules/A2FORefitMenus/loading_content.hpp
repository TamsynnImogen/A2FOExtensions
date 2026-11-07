#pragma once

#include "loading_presentation.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace a2fo::refit_menu {

// Objective text is retained as file bytes for the existing ANSI painter.
// A failed parse leaves the model untouched.
bool parse_loading_objectives(std::string_view text, LoadingModel& model);

// Returns an empty string for unsafe or unsupported map names.
std::string sanitize_loading_map_name(std::string_view input);

// Decode bounded, uncompressed BMP bytes. A failed decode leaves out untouched.
bool decode_loading_bmp(std::string_view bytes, Artwork& out);

// Roots are ordered low to high precedence. Only loose files are consulted.
// Returns true if any objective text or image was loaded.
bool load_loading_content(const std::vector<std::string>& roots, std::string_view map,
                          LoadingModel& model, Artwork& preview, Artwork& background);

} // namespace a2fo::refit_menu
