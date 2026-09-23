// Use the same SharedButtons art as the native shell. resolve_asset searches
// the active mod before its parents and Data, just like other selector assets.
void load_standard_buttons(SelectorContext& context) {
    if (!g_gdiplus_token) return;
    constexpr std::array<const char*, 4> assets{{
        "SharedButtons\\Button1.png",
        "SharedButtons\\Button1_hoover.png",
        "SharedButtons\\Button1_down.png",
        "SharedButtons\\Button1_disabled.png"
    }};
    for (std::size_t i = 0; i < assets.size(); ++i) {
        const std::string path = resolve_asset(assets[i]);
        if (path.empty()) continue;
        const std::wstring wide_path = widen(path);
        if (wide_path.empty()) continue;
        std::unique_ptr<Gdiplus::Image> image(
            Gdiplus::Image::FromFile(wide_path.c_str(), FALSE));
        if (!image || image->GetLastStatus() != Gdiplus::Ok ||
            image->GetWidth() == 0 || image->GetHeight() == 0 ||
            image->GetWidth() > 1024 || image->GetHeight() > 256) continue;
        context.button_images[i] = std::move(image);
        if (i == 0) log_line("Using standard shell button artwork: " + path);
    }
}

LRESULT CALLBACK standard_button_subclass(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam,
    UINT_PTR subclass, DWORD_PTR reference) {
    auto* context = reinterpret_cast<SelectorContext*>(reference);
    if (context) {
        if (message == WM_MOUSEMOVE && context->hovered_button != window) {
            HWND previous = context->hovered_button;
            context->hovered_button = window;
            if (previous) InvalidateRect(previous, nullptr, FALSE);
            InvalidateRect(window, nullptr, FALSE);
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, window, 0};
            TrackMouseEvent(&tracking);
        } else if (message == WM_MOUSELEAVE &&
                   context->hovered_button == window) {
            context->hovered_button = nullptr;
            InvalidateRect(window, nullptr, FALSE);
        } else if (message == WM_ENABLE || message == WM_SETFOCUS ||
                   message == WM_KILLFOCUS) {
            InvalidateRect(window, nullptr, FALSE);
        } else if (message == WM_NCDESTROY) {
            if (context->hovered_button == window)
                context->hovered_button = nullptr;
            RemoveWindowSubclass(window, &standard_button_subclass, subclass);
        }
    }
    return DefSubclassProc(window, message, wparam, lparam);
}

void draw_button(SelectorContext& context,
                 const DRAWITEMSTRUCT* item) noexcept {
    if (!item) return;
    const bool disabled = (item->itemState & ODS_DISABLED) != 0;
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const bool focused = (item->itemState & ODS_FOCUS) != 0;
    const bool highlighted = context.hovered_button == item->hwndItem || focused;
    const std::size_t index = disabled ? 3 : pressed ? 2 : highlighted ? 1 : 0;
    Gdiplus::Image* image = context.button_images[index].get();
    if (!image) image = context.button_images[0].get();

    POINT origin{};
    MapWindowPoints(item->hwndItem, context.dialog, &origin, 1);
    draw_dialog_background_region(
        context, item->hDC, item->rcItem, origin.x, origin.y);
    if (image) {
        Gdiplus::Graphics graphics(item->hDC);
        graphics.SetInterpolationMode(Gdiplus::InterpolationModeNearestNeighbor);
        graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);
        graphics.DrawImage(image,
            static_cast<INT>(item->rcItem.left),
            static_cast<INT>(item->rcItem.top),
            static_cast<INT>(item->rcItem.right - item->rcItem.left),
            static_cast<INT>(item->rcItem.bottom - item->rcItem.top));
    } else {
        RECT frame = item->rcItem;
        DrawFrameControl(item->hDC, &frame, DFC_BUTTON,
            DFCS_BUTTONPUSH | (disabled ? DFCS_INACTIVE : 0) |
            (pressed ? DFCS_PUSHED : 0));
    }

    char text[96]{};
    GetWindowTextA(item->hwndItem, text, sizeof(text));
    const int previous_mode = SetBkMode(item->hDC, TRANSPARENT);
    const COLORREF previous_colour = SetTextColor(item->hDC,
        disabled ? RGB(120, 120, 120)
        : image ? RGB(240, 245, 250) : GetSysColor(COLOR_BTNTEXT));
    HGDIOBJ previous_font = SelectObject(item->hDC, g_regular_font);
    RECT rectangle = item->rcItem;
    if (pressed && !disabled) OffsetRect(&rectangle, 1, 1);
    DrawTextA(item->hDC, text, -1, &rectangle,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if (focused && (item->itemState & ODS_NOFOCUSRECT) == 0) {
        RECT focus = item->rcItem;
        InflateRect(&focus, -4, -3);
        DrawFocusRect(item->hDC, &focus);
    }
    SelectObject(item->hDC, previous_font);
    SetTextColor(item->hDC, previous_colour);
    SetBkMode(item->hDC, previous_mode);
}
