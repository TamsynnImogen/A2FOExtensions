#pragma once
#include "../../sdk/include/a2fo_module_api.h"
#include "presentation.hpp"
#include <memory>
namespace a2fo::refit_menu::menu_scene {
void configure(const A2FO_ModuleApi* api, HMODULE fo, bool enabled, int render_width = 0);
bool requested();
const Artwork* frame(HWND owner, bool animate_overlay = false, bool repaint_children = false,
                     bool instant_action = false);
void frame_presented(const Artwork* frame);
void frame_drawn(const Artwork* frame, HDC destination);
// Begin before invoking the native IA business action. These functions never
// change window visibility, focus, activation, ownership or input.
bool begin_handover(HWND source, HWND game_parent);
bool begin_launch_handover(HWND source, HWND game_parent,std::shared_ptr<const Artwork> completed_ui);
// Only at the verified native loading entry, after IA acceptance. The IA HWND
// may already be gone; its owner is independently checked on this UI thread.
bool begin_accepted_loading_handover(HWND game_parent,std::shared_ptr<const Artwork> completed_ui);
bool launch_handover_active();
bool handover_active();
bool handover_active_for_erase(); // Never wait for the worker's native-window draw.
bool handover_ready();
void handover_destination(HWND window);
void end_handover();
// Immutable last completed scene, with no worker start, window or GPU calls.
std::shared_ptr<const Artwork> retained_frame();
// Named view IDs are independent of campaign count and future scene file type.
bool select_view(const char* id);
bool begin_departure(const char* id = nullptr);
void end_departure();
std::shared_ptr<const Artwork> loading_frame(int width, int height, bool restart = false);
// Acknowledge only a scene that reached the loading draw successfully.
void loading_presented(const std::shared_ptr<const Artwork>& frame);
bool departing();
std::uint64_t menu_epoch();
void shutdown();
int choice_at(int width, int height, int x, int y);
void choose(int index);
void paint_choices(HDC dc, int width, int height);
}
