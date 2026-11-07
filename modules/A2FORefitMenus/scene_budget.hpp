#pragma once
#include <algorithm>
#include <cstdint>
namespace a2fo::refit_menu::menu_scene {
// Leave the UI thread idle between GPU readbacks, even on a slow Wine driver.
// The old limiter measured from render start, so an expensive frame rendered
// again immediately on the next paint, starving native menu interaction.
inline constexpr unsigned max_render_failures = 3;
inline std::uint32_t retry_pause(unsigned failures, std::uint32_t failed_at, std::uint32_t now) {
    if (!failures) return 0;
    const std::uint32_t elapsed = now - failed_at;
    return elapsed < 1000 ? 1000 - elapsed : 0;
}
inline std::uint32_t frame_pause(std::uint32_t render_ms) {
    return static_cast<std::uint32_t>(std::clamp<std::uint64_t>(
        static_cast<std::uint64_t>(render_ms) * 3, 67, 1000));
}
inline bool frame_due(bool present, std::uint32_t completed, std::uint32_t now, std::uint32_t render_ms) {
    return !present || static_cast<std::uint32_t>(now - completed) >= frame_pause(render_ms);
}
// Loading choreography targets 30 Hz. Unlike the idle-menu duty limiter, a
// 67 ms minimum pause alone cannot provide even 15 completed frames/second.
// Slow frames still yield at least 4 ms; there is never a catch-up spin loop.
inline std::uint32_t animation_pause(std::uint32_t render_ms) {
    return render_ms < 29 ? 33-render_ms : 4;
}
}
