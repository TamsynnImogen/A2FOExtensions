#ifndef A2FO_RUST_UI_FFI_H
#define A2FO_RUST_UI_FFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { const uint8_t* name; size_t len; int32_t adapter; uint8_t primary; } A2foRsMonitor;
typedef struct { int32_t index; uint8_t configured; uint8_t present; } A2foRsMonitorChoice;
typedef struct { int32_t left, top, right, bottom; } A2foRsBounds;
typedef struct { A2foRsBounds bounds; uint8_t valid, repaired; } A2foRsEffectiveBounds;
typedef struct { uint32_t role; uint8_t hot, pressed, disabled; } A2foRsUiStyle;
typedef struct { int32_t width, height; const uint32_t* pixels; size_t count; } A2foRsArtwork;
typedef struct { const uint8_t* key; size_t key_len; A2foRsArtwork image; } A2foRsSavedArtwork;
typedef struct { size_t id_offset, id_len, label_offset, label_len; } A2foRsSceneView;
typedef struct { uint32_t form, colors[11]; float corners[4]; size_t name_offset, name_len, race_offset, race_len; } A2foRsTheme;
A2foRsMonitorChoice a2fo_rs_ui_monitor(const A2foRsMonitor*, size_t, const uint8_t*, size_t, int32_t);
uint8_t a2fo_rs_ui_legacy_width(int32_t);
A2foRsEffectiveBounds a2fo_rs_ui_load_bounds(A2foRsBounds, A2foRsBounds);
uint8_t a2fo_rs_ui_contains(A2foRsBounds, int32_t, int32_t);
uint8_t a2fo_rs_ui_caption(const uint8_t*, size_t);
size_t a2fo_rs_ui_decode_details(const uint8_t*, size_t, uint8_t*, size_t);
/* Returns 0 on success. On error, existing outputs must be ignored. */
uint32_t a2fo_rs_ui_catalogue(const uint8_t*, size_t, A2foRsSceneView*, size_t, uint8_t*, size_t, size_t*, size_t*);
uint32_t a2fo_rs_ui_style(const uint8_t*, size_t);
uint8_t a2fo_rs_ui_apply(uint32_t*, size_t, int32_t, int32_t, A2foRsUiStyle);
uint8_t a2fo_rs_ui_valid_art(const A2foRsArtwork*);
int32_t a2fo_rs_ui_match(const A2foRsArtwork*, const A2foRsSavedArtwork*, size_t);
uint8_t a2fo_rs_ui_compose(const A2foRsArtwork*, const A2foRsArtwork*, const A2foRsArtwork*, const uint8_t*, size_t, uint32_t*, size_t);
typedef uint8_t (*A2foRsParseCorners)(const uint8_t*, size_t, float*);
uint32_t a2fo_rs_ui_theme(const uint8_t* const*, const size_t*, size_t, const uint8_t*, size_t, const uint8_t*, A2foRsParseCorners, A2foRsTheme*, uint8_t*, size_t, size_t*);
#ifdef __cplusplus
}
#endif
#endif
