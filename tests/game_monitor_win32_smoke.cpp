#include "game_monitor.hpp"

#include <cstdio>

int main() {
    const auto monitors = a2fo::enumerate_game_monitors();
    if (monitors.empty()) {
        std::fprintf(stderr, "no attached Win32 monitors detected\n");
        return 1;
    }
    bool primary = false;
    const a2fo::GameMonitorDescriptor* primary_monitor = nullptr;
    for (const auto& monitor : monitors) {
        if (monitor.device_name.empty() || monitor.adapter_index < 0 ||
            monitor.width <= 0 || monitor.height <= 0) {
            std::fprintf(stderr, "invalid monitor descriptor\n");
            return 2;
        }
        primary = primary || monitor.primary;
        if (monitor.primary) primary_monitor = &monitor;
        std::printf("%s [%s]\n",
                    a2fo::game_monitor_label(monitor).c_str(),
                    monitor.device_name.c_str());
    }
    if (!primary) {
        std::fprintf(stderr, "no primary Win32 monitor detected\n");
        return 3;
    }

    HWND window = CreateWindowExA(
        0, "STATIC", "A2FO monitor placement smoke",
        WS_OVERLAPPEDWINDOW, 32, 32, 640, 480,
        nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (!window || !primary_monitor) {
        std::fprintf(stderr, "could not create placement smoke window\n");
        return 4;
    }
    RECT before{};
    RECT after{};
    if (!GetWindowRect(window, &before) ||
        !a2fo::position_window_on_game_monitor(window, *primary_monitor) ||
        !GetWindowRect(window, &after)) {
        DestroyWindow(window);
        std::fprintf(stderr, "position-only placement failed\n");
        return 5;
    }
    if (before.right - before.left != after.right - after.left ||
        before.bottom - before.top != after.bottom - after.top) {
        DestroyWindow(window);
        std::fprintf(stderr, "position-only placement changed window size\n");
        return 6;
    }
    if (!a2fo::is_window_on_game_monitor(window, *primary_monitor)) {
        DestroyWindow(window);
        std::fprintf(stderr, "placement did not reach the selected monitor\n");
        return 7;
    }
    DestroyWindow(window);
    return 0;
}
