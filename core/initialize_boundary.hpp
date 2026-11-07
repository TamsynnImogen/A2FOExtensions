#pragma once

// Compatibility bridge while initialization still has native C++ consumers.
// Recovery must itself be noexcept: a failure cannot cross a Rust C ABI frame.
namespace a2fo {
template<class Initialize, class Recover>
bool initialize_without_unwind(Initialize initialize, Recover recover) noexcept {
    try { return initialize(); }
    catch (...) { recover(); return false; }
}
}
