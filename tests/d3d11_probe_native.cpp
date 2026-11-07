// Console-only fixture. The HWND is never shown or activated; no input is sent.
#include "../renderer/d3d11/probe_device.hpp"

#include <cstdio>

namespace {

void log_line(const std::string& line) { std::printf("%s\n", line.c_str()); }

void require(bool condition, const char* check) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", check);
        ExitProcess(1);
    }
}

struct CrossThread {
    HWND window;
    HRESULT result = S_OK;
};

DWORD WINAPI test_other_thread(LPVOID parameter) {
    auto& test = *static_cast<CrossThread*>(parameter);
    a2fo::renderer::ProbeDevice probe;
    test.result = probe.initialize(test.window, 32, 32, false, log_line);
    return 0;
}

} // namespace

int main() {
    a2fo::renderer::ProbeDevice probe;
    require(!probe.ready(), "initial state");
    require(probe.present_test_colour() == E_UNEXPECTED, "present before init");
    require(probe.initialize(nullptr, 32, 32, false, log_line) == E_INVALIDARG,
            "null HWND rejected");
    require(!probe.ready(), "null HWND leaves empty state");

    WNDCLASSEXA cls{};
    cls.cbSize = sizeof(cls);
    cls.hInstance = GetModuleHandleA(nullptr);
    cls.lpfnWndProc = DefWindowProcA;
    cls.lpszClassName = "A2FOD3D11ProbeFixture";
    require(RegisterClassExA(&cls) != 0, "register private window class");
    HWND window = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        cls.lpszClassName, "D3D11 probe fixture", WS_POPUP,
        0, 0, 32, 32, nullptr, nullptr, cls.hInstance, nullptr);
    require(window && !IsWindowVisible(window), "private window remains hidden");

    require(probe.initialize(window, 0, 32, false, log_line) == E_INVALIDARG,
            "zero dimension rejected");
    require(probe.initialize(window, 16385, 32, false, log_line) == E_INVALIDARG,
            "oversized dimension rejected");
    require(!probe.ready(), "invalid dimensions leave empty state");

    CrossThread other{window};
    HANDLE thread = CreateThread(nullptr, 0, test_other_thread, &other, 0, nullptr);
    require(thread != nullptr, "start cross-thread ownership check");
    require(WaitForSingleObject(thread, 10000) == WAIT_OBJECT_0,
            "cross-thread ownership check completed");
    CloseHandle(thread);
    require(other.result == E_INVALIDARG, "foreign window thread rejected");

    HRESULT result = probe.initialize(window, 32, 32, true, log_line);
    if (FAILED(result)) {
        std::printf("SKIP: hardware FL11_0 D3D11 unavailable, HRESULT=0x%08lx\n",
                    static_cast<unsigned long>(result));
        require(!probe.ready(), "failed creation leaves empty state");
        DestroyWindow(window);
        UnregisterClassA(cls.lpszClassName, cls.hInstance);
        return 77;
    }
    require(probe.ready(), "device ready");
    result = probe.present_test_colour();
    require(SUCCEEDED(result), "diagnostic clear and present");
    result = probe.present_test_triangle({});
    require(SUCCEEDED(result), "SM5 transformed-FVF triangle in actual swap chain");
    probe.shutdown();
    require(!probe.ready(), "explicit shutdown");
    require(probe.present_test_colour() == E_UNEXPECTED, "present after shutdown");
    require(SUCCEEDED(probe.initialize(window, 32, 32, false, log_line)),
            "reinitialize same window");
    require(probe.ready(), "device ready after reinitialize");
    probe.shutdown();
    require(!probe.ready(), "final shutdown");
    require(DestroyWindow(window), "destroy private window");
    require(UnregisterClassA(cls.lpszClassName, cls.hInstance), "unregister class");
    std::puts("PASS: D3D11 probe lifetime and present");
    return 0;
}
