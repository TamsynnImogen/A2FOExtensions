// Include after video_module_test.cpp's synthetic Win32 host definitions.
// The stage10 parity runner calls stage10_video_completion_native_tests().
#include <limits>

namespace {
bool stage10_timing_active = false;
bool stage10_timing_cancel_after_two = false;
LONGLONG stage10_first_counter = 0;
unsigned stage10_qpc_calls = 0;
std::vector<DWORD> stage10_sleep_args;
}

BOOL WINAPI stage10_fake_QueryPerformanceCounter(LARGE_INTEGER* out) {
    if (!stage10_timing_active) return stage10_real_qpc(out);
    ++stage10_qpc_calls;
    out->QuadPart = stage10_qpc_calls == 1
        ? stage10_first_counter : std::numeric_limits<LONGLONG>::max();
    if (stage10_timing_cancel_after_two && stage10_qpc_calls >= 2)
        window_alive = false;
    return TRUE;
}

VOID WINAPI stage10_fake_Sleep(DWORD milliseconds) {
    if (stage10_timing_active) stage10_sleep_args.push_back(milliseconds);
    fake_Sleep(milliseconds);
}

namespace {
unsigned short stage10_cw() {
    unsigned short value;
    __asm__ __volatile__("fnstcw %0" : "=m"(value));
    return value;
}
unsigned short stage10_sw() {
    unsigned short value;
    __asm__ __volatile__("fnstsw %0" : "=am"(value));
    return value;
}
void stage10_set_cw(unsigned short value) {
    __asm__ __volatile__("fldcw %0" : : "m"(value));
}
void stage10_timing_case(const char* label, unsigned precision, unsigned rounding,
                         LONGLONG start, LONGLONG frequency, LONGLONG first_counter,
                         double seconds, bool cancel_after_two) {
    reset_events();
    ticks = 0;
    sleep_calls = 0;
    stage10_sleep_args.clear();
    stage10_sleep_args.reserve(2);
    stage10_qpc_calls = 0;
    stage10_first_counter = first_counter;
    stage10_timing_cancel_after_two = cancel_after_two;
    VideoTiming timing{};
    timing.start.QuadPart = start;
    timing.frequency.QuadPart = frequency;
    MovieWindowEvents events(main_window);
    const unsigned short original = stage10_cw();
    const unsigned short selected = static_cast<unsigned short>(
        (original & ~0x0f00u) | precision | rounding);
    stage10_set_cw(selected);
    __asm__ __volatile__("fnclex");
    stage10_timing_active = true;
    const bool result = wait_until_seconds(timing, seconds, events);
    stage10_timing_active = false;
    const unsigned short after_cw = stage10_cw();
    const unsigned short after_sw = stage10_sw();
    stage10_set_cw(original);
    assert(result == !cancel_after_two);
    assert(stage10_qpc_calls <= 2 && stage10_sleep_args.size() <= 2);
    assert(stage10_sleep_args.size() == sleep_calls);
    if (std::strcmp(label, "negative") == 0)
        assert(stage10_qpc_calls == 0 && sleep_calls == 0);
    if (std::strcmp(label, "zero") == 0)
        assert(stage10_qpc_calls == 1 && sleep_calls == 0);
    if (std::strcmp(label, "sleep1") == 0)
        assert(stage10_qpc_calls == 2 && stage10_sleep_args == std::vector<DWORD>{1});
    if (cancel_after_two)
        assert(stage10_qpc_calls == 2 && sleep_calls == 2);
    std::printf("stage10:timing:%s:pc=%03x:rc=%03x:result=%u:qpc=%u:sleeps=%u:args=",
                label, precision, rounding, result, stage10_qpc_calls, sleep_calls);
    for (DWORD value : stage10_sleep_args) std::printf("%lu", static_cast<unsigned long>(value));
    std::printf(":ticks=%llu:cw=%04x:sw=%04x\n",
                static_cast<unsigned long long>(ticks), after_cw, after_sw);
}
void stage10_timing_matrix() {
    // Above double's exact-integer boundary, but far enough below INT64_MAX
    // that the second synthetic QPC always ends a finite wait in every mode.
    constexpr LONGLONG large = (LONGLONG{1} << 53) + 4096;
    for (const unsigned precision : {0x0000u, 0x0200u, 0x0300u}) {
        for (const unsigned rounding : {0x0000u, 0x0400u, 0x0800u, 0x0c00u}) {
            stage10_timing_case("negative", precision, rounding, 0, 1000, 0, -0.5, false);
            stage10_timing_case("zero", precision, rounding, 0, 1000, 0, 0.0, false);
            stage10_timing_case("sleep1", precision, rounding, 0, 1000, 0, 0.003, false);
            stage10_timing_case("sleep0", precision, rounding, 0, 1000, 0, 0.002, false);
            stage10_timing_case("large0", precision, rounding, large, 1000, large, 0.0, false);
            stage10_timing_case("large2", precision, rounding, large, 1000, large, 0.002, false);
            stage10_timing_case("nan", precision, rounding, 0, 1000, 0,
                                std::numeric_limits<double>::quiet_NaN(), true);
            stage10_timing_case("infinity", precision, rounding, 0, 1000, 0,
                                std::numeric_limits<double>::infinity(), true);
        }
    }
}
}

void stage10_video_completion_native_tests() {
    reset_events();
    MovieWindowEvents events(main_window);
    VideoTiming timing{};
    QueryPerformanceFrequency(&timing.frequency);
    QueryPerformanceCounter(&timing.start);
    assert(wait_until_seconds(timing, -1.0, events));
    enqueue(WM_CLOSE);
    assert(!wait_until_seconds(timing, -1.0, events));
    reset_events();
    MovieWindowEvents zero_wait(main_window);
    assert(wait_until_seconds(timing, 0.0, zero_wait));

    clear_audio();
    WaveOutput wave;
    assert(wave.open());
    std::atomic<bool> stop{false};
    SwrContext* swr = nullptr;
    assert(!decode_audio_frame(swr, nullptr, wave, stop));
    assert(swr == nullptr && wave.submitted_bytes() == 0);
    AVFrame frame{};
    frame.format = AV_SAMPLE_FMT_S16;
    frame.sample_rate = 48000;
    frame.nb_samples = 4;
    av_channel_layout_default(&frame.ch_layout, 2);
    std::int16_t samples[8]{100, -100, 200, -200, 300, -300, 400, -400};
    std::uint8_t* sample_data[] = {reinterpret_cast<std::uint8_t*>(samples)};
    frame.extended_data = sample_data;
    assert(decode_audio_frame(swr, &frame, wave, stop));
    assert(swr && wave.submitted_bytes() > 0);
    const std::uint64_t previous = wave.submitted_bytes();
    stop.store(true);
    assert(!decode_audio_frame(swr, &frame, wave, stop));
    assert(wave.submitted_bytes() == previous);
    av_channel_layout_uninit(&frame.ch_layout);
    swr_free(&swr);
    std::puts("stage10:wait-and-audio-frame-pass");
    stage10_timing_matrix();
}
