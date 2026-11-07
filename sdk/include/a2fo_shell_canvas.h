#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

// Call on the shell window's owning UI thread. A null destination queries the
// existing logical canvas size. Copy requires a memory DC with a selected
// bitmap at least capacity_width by capacity_height; pixels are copied at 0,0.
typedef BOOL (__cdecl *A2FO_CopyShellCanvasV1Fn)(HWND window, HDC destination,
    int capacity_width, int capacity_height, int* width, int* height);

BOOL __cdecl A2FO_CopyShellCanvasV1(HWND window, HDC destination,
    int capacity_width, int capacity_height, int* width, int* height);

// Resolve only a current paint/DC lease on this UI thread. Memory DCs do not
// support WindowFromDC; never infer their owner from the active desktop window.
// Returns NULL after release, for foreign DCs, or while the canvas is disabled.
typedef HWND (__cdecl *A2FO_ShellCanvasWindowV1Fn)(HDC logical);
HWND __cdecl A2FO_ShellCanvasWindowV1(HDC logical);

// Register on the owning UI thread before a root's first canvas lease. Called
// once for each newly allocated or resized full-root backing canvas, before
// its first partial paint. Return FALSE to retain the native black canvas.
// The callback and context must stay valid until cleared on that thread.
typedef BOOL (__cdecl *A2FO_ShellCanvasSeedV1)(HWND window, HDC logical,
    int width, int height, void* context);
typedef BOOL (__cdecl *A2FO_SetShellCanvasSeedV1Fn)(A2FO_ShellCanvasSeedV1 seed,
    void* context);
BOOL __cdecl A2FO_SetShellCanvasSeedV1(A2FO_ShellCanvasSeedV1 seed,
    void* context);

// Optional update for an existing seeded root canvas, on its owning UI thread
// before early erase or visible show/resize replays it. Paint the full logical
// area and return TRUE to replace the cached scene. FALSE or a thrown callback
// keeps the previous valid scene. A null callback clears its thread's context.
// SeedV1 retains its once-per-allocation behavior.
typedef BOOL (__cdecl *A2FO_ShellCanvasRefreshV1)(HWND window, HDC logical,
    int width, int height, void* context);
typedef BOOL (__cdecl *A2FO_SetShellCanvasRefreshV1Fn)(
    A2FO_ShellCanvasRefreshV1 refresh, void* context);
BOOL __cdecl A2FO_SetShellCanvasRefreshV1(A2FO_ShellCanvasRefreshV1 refresh,
    void* context);

// Register on the owning UI thread while ShellDisplay is active. Called once
// after each matched paint or DC lease attempts physical presentation; FALSE
// means both the presenter and native canvas presentation declined or failed.
// The logical DC is valid only during this callback. A null callback clears
// the current thread's registration and context, as with the seed callback.
typedef void (__cdecl *A2FO_ShellCanvasCompletionV1)(HWND window, HDC logical,
    BOOL presented, void* context);
typedef BOOL (__cdecl *A2FO_SetShellCanvasCompletionV1Fn)(
    A2FO_ShellCanvasCompletionV1 completion, void* context);
BOOL __cdecl A2FO_SetShellCanvasCompletionV1(
    A2FO_ShellCanvasCompletionV1 completion, void* context);

// The caller keeps the callback and context valid until it unregisters on the
// owning UI thread. Return TRUE only after fully painting the physical client
// (the supplied DC may carry a partial BeginPaint clip);
// FALSE lets ShellDisplay present its native canvas. Do not modify that canvas.
typedef BOOL (__cdecl *A2FO_ShellCanvasPresenterV1)(HWND window, HDC physical,
    void* context);

// Query on the owning UI thread before re-laying out a native game dialog.
// Fails closed when the checked dialog coordinate bridge was not installed or
// its callback has subsequently changed.
typedef BOOL (__cdecl *A2FO_ShellDialogBridgeReadyV1Fn)(HWND window);
BOOL __cdecl A2FO_ShellDialogBridgeReadyV1(HWND window);

// One presenter per shell window. Registering the same pair is idempotent;
// a null callback unregisters only when context matches the registered one.
BOOL __cdecl A2FO_SetShellCanvasPresenterV1(HWND window,
    A2FO_ShellCanvasPresenterV1 presenter, void* context);

#ifdef __cplusplus
}
#endif
