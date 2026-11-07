#pragma once
#include <windows.h>
namespace a2fo::native11_borg_material::source {
// Read-only signatures for eight named original operations, including the
// pinned file's selection code cave. Caller supplies a live, exact-file image.
// This neither pins the image nor qualifies CraftClass type/readiness, a
// render callback route, extension overlays, or a complete effective material.
// No normalization of unknown detours and no native calls or writes.
bool matches(HMODULE image) noexcept;
}
