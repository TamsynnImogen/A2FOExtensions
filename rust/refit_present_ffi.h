#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// All byte strings are borrowed for the duration of the call. Callbacks must
// catch C++ exceptions and return zero on failure.
typedef int (*A2FORefitTextSink)(void* context, int field, const uint8_t* bytes,
                                  size_t length);

int a2fo_refit_parse_objectives(const uint8_t* bytes, size_t length,
                                A2FORefitTextSink sink, void* context);
size_t a2fo_refit_sanitize_map(const uint8_t* bytes, size_t length,
                               uint8_t* output, size_t capacity);
int a2fo_refit_decode_bmp(const uint8_t* bytes, size_t length,
                          uint32_t* pixels, size_t capacity,
                          int32_t* width, int32_t* height);

typedef struct A2FORefitBytes {
    const uint8_t* data;
    size_t length;
} A2FORefitBytes;
// kind 0: objective text; 1: map image; 2: loading background image.
typedef int (*A2FORefitRead)(void* context, int kind, const uint8_t* path,
                              size_t length);
int a2fo_refit_load_content(const A2FORefitBytes* roots, size_t root_count,
                            const uint8_t* map, size_t map_length,
                            int windows_images, A2FORefitRead read, void* context,
                            int* show_preview);

typedef struct A2FORefitRect { int32_t x, y, width, height; } A2FORefitRect;
typedef struct A2FORefitMenuEntry {
    A2FORefitBytes caption;
    uint8_t visible, enabled;
} A2FORefitMenuEntry;
typedef struct A2FORefitMenuLayout {
    A2FORefitRect title, rows[7], footer;
    float scale;
} A2FORefitMenuLayout;
typedef struct A2FORefitArtwork {
    int32_t width, height;
    const uint32_t* pixels;
    size_t pixel_count;
} A2FORefitArtwork;

void a2fo_refit_menu_layout(int32_t width, int32_t height,
                            const A2FORefitMenuEntry* entries,
                            A2FORefitMenuLayout* output);
int32_t a2fo_refit_menu_hit(const A2FORefitMenuLayout* layout,
                            const A2FORefitMenuEntry* entries, int32_t x, int32_t y);
int32_t a2fo_refit_menu_next(const A2FORefitMenuEntry* entries,
                             int32_t current, int32_t direction);
// 0 fill, 1 text, 2 opaque DIB, 3 premultiplied ARGB AlphaBlend.
typedef int (*A2FORefitMenuDraw)(void* context, int op, A2FORefitRect rect,
                                  uint32_t color, int32_t size, int32_t flags,
                                  const uint8_t* text, size_t text_length,
                                  const uint32_t* pixels, size_t pixel_count);
int a2fo_refit_paint_menu(int32_t width, int32_t height,
                          const A2FORefitMenuEntry* entries,
                          A2FORefitBytes version, int32_t hot, int32_t pressed,
                          A2FORefitArtwork artwork, int32_t has_sse2,
                          A2FORefitMenuDraw draw,
                          void* context);

typedef struct A2FORefitLoadingInput {
    int32_t kind;
    int32_t show_preview;
    size_t player_count;
    const A2FORefitRect* protected_regions;
    size_t protected_count;
} A2FORefitLoadingInput;
typedef struct A2FORefitLoadingLayout {
    int32_t valid;
    float scale;
    A2FORefitRect kicker, title, briefing, objectives, hint;
    A2FORefitRect preview, players, status, percentage;
} A2FORefitLoadingLayout;
void a2fo_refit_loading_layout(int32_t width, int32_t height,
                               A2FORefitLoadingInput input,
                               A2FORefitLoadingLayout* output);

typedef struct A2FORefitSetupControl {
    uintptr_t key;
    int32_t kind;
    A2FORefitRect source;
    A2FORefitBytes text;
    int32_t message, visible, enabled, mode, open, down;
    int32_t item_height;
    A2FORefitRect pull;
    size_t item_count;
    const A2FORefitBytes* items;
    int32_t selected;
} A2FORefitSetupControl;
typedef struct A2FORefitPlacement {
    uintptr_t key;
    A2FORefitRect source, target;
    int32_t kind, item;
} A2FORefitPlacement;
typedef struct A2FORefitSetupLayout {
    int32_t valid;
    float scale;
    A2FORefitRect title, players, map, preview;
    size_t placement_count;
} A2FORefitSetupLayout;
int a2fo_refit_setup_arrange(int32_t width, int32_t height,
                             const A2FORefitSetupControl* controls,
                             size_t control_count, int32_t popup_scroll,
                             A2FORefitSetupLayout* output,
                             A2FORefitPlacement* placements, size_t capacity);
int a2fo_refit_setup_map_point(const A2FORefitSetupLayout* layout,
                                const A2FORefitPlacement* placements,
                                int32_t x, int32_t y, int32_t* sx, int32_t* sy);
uintptr_t a2fo_refit_setup_launch_at(const A2FORefitSetupControl* controls,
                                     size_t control_count,
                                     const A2FORefitSetupLayout* layout,
                                     const A2FORefitPlacement* placements,
                                     int32_t x, int32_t y);
int a2fo_refit_setup_preview(A2FORefitArtwork artwork,
                             const A2FORefitSetupLayout* layout,
                             const A2FORefitPlacement* placements,
                             uint32_t* output, size_t capacity,
                             int32_t* width, int32_t* height);
int32_t a2fo_refit_setup_native_width(int32_t width, int32_t height,
                                      int32_t canvas_width, int32_t canvas_height,
                                      const A2FORefitSetupControl* controls,
                                      size_t control_count, int32_t popup_scroll);
int a2fo_refit_setup_normalize(int32_t native_width,
                               A2FORefitSetupControl* controls, size_t count);
// Setup draw operations: 0 fill, 1 skin, 2 fallback polygon, 3 single-line
// text, 4 artwork, 5 wrapped message text.
typedef int (*A2FORefitSetupDraw)(void* context, int32_t op,
                                   A2FORefitRect rect, A2FORefitRect source,
                                   uint32_t color, uint32_t edge, int32_t size,
                                   int32_t flags, A2FORefitBytes text,
                                   A2FORefitArtwork artwork);
int a2fo_refit_paint_setup(int32_t width, int32_t height,
                           const A2FORefitSetupControl* controls, size_t control_count,
                           const A2FORefitSetupLayout* layout,
                           const A2FORefitPlacement* placements,
                           A2FORefitArtwork background, A2FORefitArtwork native_canvas,
                           int32_t background_present, int32_t native_present,
                           int32_t hot_x, int32_t hot_y,
                           A2FORefitSetupDraw draw, void* context);

typedef struct A2FORefitLoadingTheme {
    int32_t form;
    uint32_t panel_top, panel, panel_edge, panel_hi;
    uint32_t text, text_dim, title, accent, accent2, fill, good;
    float corners[4];
} A2FORefitLoadingTheme;
typedef struct A2FORefitPlayer {
    A2FORefitBytes name, detail;
    int32_t team, local;
    float fraction;
} A2FORefitPlayer;
typedef struct A2FORefitLoadingPaintInput {
    int32_t kind;
    int32_t backdrop_present, preview_present;
    A2FORefitBytes title, mode, status, hint;
    const A2FORefitBytes* briefing;
    size_t briefing_count;
    const A2FORefitBytes* objectives;
    size_t objective_count;
    const A2FORefitPlayer* players;
    size_t player_count;
    float progress;
    double time_seconds;
    int32_t departure_shown;
    A2FORefitRect bar_track;
    const A2FORefitRect* protected_regions;
    size_t protected_count;
    A2FORefitLoadingTheme theme;
} A2FORefitLoadingPaintInput;
typedef struct A2FORefitPoint { int32_t x, y; } A2FORefitPoint;
// 0 fill, 1 text, 2 pixels, 3 black, 4 save, 5 restore, 6 exclude,
// 7 intersect, 8 clipped gradient panel plus outline.
typedef int (*A2FORefitLoadingDraw)(void* context, int32_t op,
                                     A2FORefitRect rect, uint32_t color,
                                     int32_t size, int32_t flags,
                                     A2FORefitBytes text,
                                     const uint32_t* pixels, size_t pixel_count,
                                     const A2FORefitPoint* points, size_t point_count,
                                     int32_t* used);
int a2fo_refit_paint_loading(int32_t width, int32_t height,
                             A2FORefitLoadingPaintInput input,
                             A2FORefitArtwork backdrop, A2FORefitArtwork preview,
                             int32_t transparent_overlay,
                             A2FORefitLoadingDraw draw, void* context);

#ifdef __cplusplus
}
#endif
