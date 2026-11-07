#pragma once
#include <windows.h>
#include <stdint.h>

// Optional module-to-module presentation interface. All strings are copied;
// no native engine object, allocator, device or input ownership crosses it.
// Calls belong to the caller's existing UI/render thread, never a timer worker.
// Resolve exports from an already-loaded A2FORefitMenus.dll; absence is normal.
enum { A2FO_MENU_SCENE_DEPARTURE = 1 };
typedef BOOL (__cdecl *A2FO_MenuScenePresentV1)(HWND, HDC, int, int, const char*, unsigned);
typedef BOOL (__cdecl *A2FO_MenuSceneBeginDepartureV1)(const char*);
typedef void (__cdecl *A2FO_MenuSceneEndDepartureV1)(void);

typedef struct A2FO_MenuMissionV1 {
    uint32_t size;
    uint32_t version;
    char map[260];
    char title[256];
    char briefing[2048];
    char objectives[2048];
} A2FO_MenuMissionV1;
typedef BOOL (__cdecl *A2FO_MenuSceneMissionV1)(const A2FO_MenuMissionV1*);

// Exports: A2FORefitMenus_PresentSceneV1,
// A2FORefitMenus_BeginDepartureV1, A2FORefitMenus_EndDepartureV1,
// A2FORefitMenus_SetMissionV1. Present returns FALSE for disabled/unavailable
// scenes so the caller can retain its native/static backdrop. An empty view
// preserves the currently selected named view. No call waits for rendering.
