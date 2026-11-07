#pragma once

#include "policy.hpp"
#include <windows.h>
#include <atomic>
#include <cstdint>

namespace a2fo::shell_display::presentation {

struct State {
    int width = 0;
    int height = 0;
    int depth = 0;
    int client_width = 0;
    int client_height = 0;
    bool borderless = false;
    HWND shell_window = nullptr;
    unsigned generation = 0;
};

namespace detail {
inline std::atomic<unsigned> sequence{0};
inline std::atomic<int> width{0};
inline std::atomic<int> height{0};
inline std::atomic<int> depth{0};
inline std::atomic<int> client_width{0};
inline std::atomic<int> client_height{0};
inline std::atomic<unsigned> borderless{0};
inline std::atomic<std::uintptr_t> shell_window{0};
inline std::atomic<unsigned> valid{0};
}

inline void publish(const Mode& mode, bool is_borderless, HWND window,
                    int actual_client_width, int actual_client_height) noexcept {
    detail::sequence.fetch_add(1, std::memory_order_acq_rel); // odd: writer active
    detail::width.store(mode.width, std::memory_order_relaxed);
    detail::height.store(mode.height, std::memory_order_relaxed);
    detail::depth.store(mode.depth, std::memory_order_relaxed);
    detail::client_width.store(actual_client_width, std::memory_order_relaxed);
    detail::client_height.store(actual_client_height, std::memory_order_relaxed);
    detail::borderless.store(is_borderless ? 1u : 0u, std::memory_order_relaxed);
    detail::shell_window.store(reinterpret_cast<std::uintptr_t>(window),
                               std::memory_order_relaxed);
    detail::valid.store(valid_mode(mode) ? 1u : 0u, std::memory_order_relaxed);
    detail::sequence.fetch_add(1, std::memory_order_release); // even: stable
}

inline void clear() noexcept {
    detail::sequence.fetch_add(1, std::memory_order_acq_rel);
    detail::valid.store(0, std::memory_order_relaxed);
    detail::shell_window.store(0, std::memory_order_relaxed);
    detail::client_width.store(0, std::memory_order_relaxed);
    detail::client_height.store(0, std::memory_order_relaxed);
    detail::sequence.fetch_add(1, std::memory_order_release);
}

inline bool snapshot(State& out) noexcept {
    for (unsigned attempt = 0; attempt < 8; ++attempt) {
        const unsigned before = detail::sequence.load(std::memory_order_acquire);
        if (before & 1u) continue;
        if (!detail::valid.load(std::memory_order_relaxed)) return false;

        State copy;
        copy.width = detail::width.load(std::memory_order_relaxed);
        copy.height = detail::height.load(std::memory_order_relaxed);
        copy.depth = detail::depth.load(std::memory_order_relaxed);
        copy.client_width = detail::client_width.load(std::memory_order_relaxed);
        copy.client_height = detail::client_height.load(std::memory_order_relaxed);
        copy.borderless = detail::borderless.load(std::memory_order_relaxed) != 0;
        copy.shell_window = reinterpret_cast<HWND>(
            detail::shell_window.load(std::memory_order_relaxed));

        const unsigned after = detail::sequence.load(std::memory_order_acquire);
        if (before == after && !(after & 1u)) {
            copy.generation = after;
            out = copy;
            return true;
        }
    }
    return false;
}

} // namespace a2fo::shell_display::presentation
