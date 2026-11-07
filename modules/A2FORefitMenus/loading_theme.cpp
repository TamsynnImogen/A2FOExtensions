#include "loading_theme.hpp"
// Copyright (c) 2026 Cedric Haase (original MIT theme design).
// Palette and signed-corner semantics adapted from armada2-refit
// ui/src/widgets/theme.rs; license retained in third_party/armada2-refit/LICENSE.txt.
#include "../../rust/ui_ffi.h"
#include <cctype>
#include <cmath>
#include <exception>
#include <fstream>
#include <sstream>
namespace a2fo::refit_menu {
namespace {
thread_local std::exception_ptr corner_error;
std::uint8_t parse_corners(const std::uint8_t *text, std::size_t size, float *out) noexcept {
    try {
        std::istringstream in(std::string(reinterpret_cast<const char *>(text), size));
        for (int i = 0; i < 4; ++i) {
            if (!(in >> out[i]) || !std::isfinite(out[i]) || std::abs(out[i]) > 100.0f)
                return 0;
            if (i < 3) {
                in >> std::ws;
                if (in.get() != ',')
                    return 0;
            }
        }
        in >> std::ws;
        return in.eof();
    } catch (...) {
        corner_error = std::current_exception();
        return 0;
    }
}
} // namespace
LoadingTheme loading_theme(const std::vector<std::string> &roots, std::string_view race) {
    std::vector<std::string> texts;
    texts.reserve(roots.size());
    for (const auto &root : roots) {
        if (root.empty())
            continue;
        std::string path = root;
        const auto last = path.back();
        if (last != '/' && last != '\\')
            path += '/';
        path += "RefitScene/Loading/themes.ini";
        std::ifstream input(path);
        if (!input)
            continue;
        std::string line, contents;
        std::size_t total = 0;
        while (std::getline(input, line) && total < 65536) {
            total += line.size();
            contents += line;
            contents += '\n';
        }
        texts.push_back(std::move(contents));
    }
    std::vector<const std::uint8_t *> pointers;
    std::vector<std::size_t> lengths;
    for (const auto &t : texts) {
        pointers.push_back(reinterpret_cast<const std::uint8_t *>(t.data()));
        lengths.push_back(t.size());
    }
    A2foRsTheme parsed{};
    std::vector<std::uint8_t> storage(65536 + race.size() + 1);
    std::size_t used = 0;
    std::uint8_t lower_map[256];
    for (unsigned i = 0; i < 256; ++i)
        lower_map[i] = static_cast<std::uint8_t>(std::tolower(i));
    corner_error = nullptr;
    const auto status =
        a2fo_rs_ui_theme(pointers.data(), lengths.data(), pointers.size(),
                         reinterpret_cast<const std::uint8_t *>(race.data()), race.size(),
                         lower_map, parse_corners, &parsed, storage.data(), storage.size(), &used);
    if (corner_error)
        std::rethrow_exception(corner_error);
    LoadingTheme out;
    if (status)
        return out;
    out.name.assign(reinterpret_cast<const char *>(storage.data() + parsed.name_offset),
                    parsed.name_len);
    out.race.assign(reinterpret_cast<const char *>(storage.data() + parsed.race_offset),
                    parsed.race_len);
    out.form = static_cast<LoadingForm>(parsed.form);
    out.panel_top = parsed.colors[0];
    out.panel = parsed.colors[1];
    out.panel_edge = parsed.colors[2];
    out.panel_hi = parsed.colors[3];
    out.text = parsed.colors[4];
    out.text_dim = parsed.colors[5];
    out.title = parsed.colors[6];
    out.accent = parsed.colors[7];
    out.accent2 = parsed.colors[8];
    out.fill = parsed.colors[9];
    out.good = parsed.colors[10];
    for (int i = 0; i < 4; ++i)
        out.corners[i] = parsed.corners[i];
    return out;
}
} // namespace a2fo::refit_menu
