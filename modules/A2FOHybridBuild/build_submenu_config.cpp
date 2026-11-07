#include "build_submenu_config.hpp"
#include "../../rust/hybrid_ffi.h"
#include <algorithm>
#include <cstdint>
#include <exception>

namespace a2fo::build_submenu {
namespace {
struct ParseContext {
    Config* config;
    std::exception_ptr exception;
};
uint8_t accept_entry(void* raw, std::size_t parent, std::size_t child,
                     const uint8_t* value, std::size_t length) noexcept {
    auto& context = *static_cast<ParseContext*>(raw);
    if (context.exception) return 0;
    try {
        context.config->pages[parent].children[child].assign(
            reinterpret_cast<const char*>(value), length);
        return 1;
    } catch (...) {
        context.exception = std::current_exception();
        return 0;
    }
}
}
bool Page::empty() const noexcept {
    return std::all_of(children.begin(), children.end(),
                       [](const std::string& child) { return child.empty(); });
}
bool Config::empty() const noexcept {
    return std::all_of(pages.begin(), pages.end(),
                       [](const Page& page) { return page.empty(); });
}
bool parse_config(const std::string& contents, Config* output, std::string* error) {
    if (error) error->clear();
    if (!output) {
        if (error) *error = "missing output config";
        return false;
    }
    Config parsed{};
    ParseContext context{&parsed, {}};
    const auto result = a2fo_rs_hybrid_parse_submenu(
        reinterpret_cast<const uint8_t*>(contents.data()), contents.size(),
        &context, accept_entry);
    if (context.exception) std::rethrow_exception(context.exception);
    if (!result) return false;
    *output = std::move(parsed);
    return true;
}
std::string normalize_object_name(const std::string& value) {
    std::string output(value.size(), '\0');
    const std::size_t length = a2fo_rs_hybrid_normalize(
        reinterpret_cast<const uint8_t*>(value.data()), value.size(),
        reinterpret_cast<uint8_t*>(output.data()), output.size());
    output.resize(length);
    return output;
}
}
