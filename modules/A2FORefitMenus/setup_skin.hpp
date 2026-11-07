#pragma once

#include "presentation.hpp"

#ifdef _WIN32
namespace a2fo::refit_menu::setup_skin {
// Order is shared with assets/refit-menus/ui/atlas.json.
enum class Part : unsigned {
    panel, header, button_normal, button_hot, button_pressed,
    button_disabled, popup, popup_selected, count
};
// The creator of a private, top-down 32-bit DIB may opt in while painting it.
// A selected bitmap alone cannot reliably reveal row order through GetObject.
class DirectTargetScope {
public:
    DirectTargetScope(HDC target,int width,int height,void* bits);
    ~DirectTargetScope();
    DirectTargetScope(const DirectTargetScope&)=delete;
    DirectTargetScope& operator=(const DirectTargetScope&)=delete;
private:
    HDC previous_target_{};
    int previous_width_{},previous_height_{};
    void* previous_bits_{};
};
bool available();
bool paint(HDC target, const Rect& rectangle, Part part, int corner);
// Blend a solid RGB colour with a constant alpha into an owned DIB.
// False leaves the destination untouched so callers can use their GDI path.
bool tint_dib(HDC target,const Rect& rectangle,unsigned rgb,BYTE alpha);
} // namespace a2fo::refit_menu::setup_skin
#endif
