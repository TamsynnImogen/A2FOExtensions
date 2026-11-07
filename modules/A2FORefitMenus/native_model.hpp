#pragma once
#include "bindings.hpp"
#include "presentation.hpp"
#include <array>
#include <cstdint>

namespace a2fo::refit_menu::native {
struct Visual { std::uintptr_t control = 0; std::uint8_t visible = 0; };
template<class Read, class T>
bool field(Read& read, std::uintptr_t object, std::size_t offset, T& out) {
    return object && offset <= UINT32_MAX - object && read(object + offset, &out, sizeof(out));
}
template<class Read>
bool image_object(Read& read, std::uintptr_t object, std::uintptr_t image, std::uint32_t size) {
    std::uint32_t vmt = 0;
    return field(read, object, 0, vmt) && vmt >= image && vmt - image < size;
}
template<class Read>
bool model(Read& read, std::uintptr_t form, std::uintptr_t image, std::uint32_t size,
           Snapshot& result, std::array<Visual, 9>* visuals = nullptr) {
    std::uint32_t vmt = 0;
    if (!field(read, form, 0, vmt) || vmt != image + main_vmt_rva) return false;
    Snapshot next = default_snapshot();
    next.version = "FLEET OPERATIONS / A2FO EXTENSIONS";
    for (std::size_t i = 0; i < entry_count; ++i) {
        std::uint32_t action = 0, caption = 0;
        std::uint8_t visible = 0, enabled = 0;
        if (!field(read, form, action_fields[i], action) || !image_object(read, action, image, size) ||
            !field(read, action, action_visible_offset, visible) || visible > 1 ||
            !field(read, action, action_enabled_offset, enabled) || enabled > 1 ||
            !field(read, action, action_caption_offset, caption)) return false;
        next.entries[i].visible = visible != 0;
        next.entries[i].enabled = enabled != 0;
        if (caption) {
            std::int32_t count = 0;
            if (caption < 4 || !read(caption - 4, &count, sizeof(count)) || count < 0 || count > 512) return false;
            std::string text(static_cast<std::size_t>(count), '\0');
            if (count && !read(caption, text.data(), static_cast<std::size_t>(count))) return false;
            if (text.find('\0') != std::string::npos) return false;
            if (!text.empty() && text != "*") next.entries[i].caption = std::move(text);
        }
    }
    if (visuals) {
        std::array<Visual, 9> checked{};
        for (std::size_t i = 0; i < visual_fields.size(); ++i) {
            std::uint32_t control = 0, owner = 0, parent = 0;
            std::uint8_t visible = 0;
            if (!field(read, form, visual_fields[i], control) || !image_object(read, control, image, size) ||
                !field(read, control, 4, owner) || owner != form ||
                !field(read, control, 0x30, parent) || parent != form ||
                !field(read, control, visible_offset, visible) || visible > 1) return false;
            checked[i] = {control, visible};
        }
        *visuals = checked;
    }
    result = std::move(next);
    return true;
}
} // namespace a2fo::refit_menu::native
