/* Host-testable parser for recursive map-editor menus. */
#include "edit_menu_odf.hpp"
#include "../../rust/config_ffi.h"
#include <exception>
namespace a2fo::edit_menu {
namespace {
A2foRsBytes span(const std::string &s) {
    return {reinterpret_cast<const uint8_t *>(s.data()), s.size()};
}
struct Output {
    MenuNode *node;
    std::string *error;
    std::exception_ptr exception;
};
void receive(void *p, uint32_t kind, size_t index, const uint8_t *data, size_t size) noexcept {
    auto &out = *static_cast<Output *>(p);
    if (out.exception)
        return;
    try {
        std::string value(reinterpret_cast<const char *>(data), size);
        switch (kind) {
        case 0:
            out.node->source_name = std::move(value);
            break;
        case 1:
            out.node->title = std::move(value);
            break;
        case 2:
            out.node->build_items[index] = std::move(value);
            break;
        case 3:
            out.node->items[index] = std::move(value);
            break;
        case 4:
            out.node->force_to_neutral = size && data[0];
            break;
        case 5:
            if (out.error)
                *out.error = std::move(value);
            break;
        }
    } catch (...) {
        out.exception = std::current_exception();
    }
}
} // namespace
bool MenuNode::is_submenu() const noexcept {
    std::array<A2foRsBytes, kEntryCount> items;
    for (std::size_t i = 0; i < items.size(); ++i)
        items[i] = span(build_items[i]);
    return a2fo_rs_config_edit_submenu(items.data(), items.size()) != 0;
}
std::string normalize_odf_name(const std::string &value) {
    MenuNode node;
    Output out{&node, nullptr, {}};
    a2fo_rs_config_edit_normalize(span(value), receive, &out);
    if (out.exception)
        std::rethrow_exception(out.exception);
    return node.source_name;
}
bool parse_menu_node(const std::string &contents, const std::string &source_name, MenuNode *output,
                     std::string *error) {
    if (error)
        error->clear();
    if (!output) {
        if (error)
            *error = "missing output node";
        return false;
    }
    MenuNode parsed;
    Output out{&parsed, error, {}};
    const bool ok =
        a2fo_rs_config_edit_parse(span(contents), span(source_name), receive, &out) != 0;
    if (out.exception)
        std::rethrow_exception(out.exception);
    if (ok)
        *output = std::move(parsed);
    return ok;
}
} // namespace a2fo::edit_menu
