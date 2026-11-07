// Appended to the project's full-source video fixture by the parity runner.
// All file, window and input observations are synthetic.
#include <stdexcept>
namespace {
std::vector<std::string> file_probes;
std::vector<std::string> roots{"Data", "Parent", "Selected"};
std::vector<std::string> existing;
std::uint32_t declared_count = 3;
std::uint32_t A2FO_CALL test_root_count() { return declared_count; }
const char* A2FO_CALL test_root(std::uint32_t index) { return roots.at(index).c_str(); }
const char* A2FO_CALL test_base_root() { return "Base"; }
}

DWORD WINAPI fake_GetFileAttributesA(LPCSTR path) {
    file_probes.emplace_back(path);
    return std::find(existing.begin(), existing.end(), path) == existing.end()
        ? INVALID_FILE_ATTRIBUTES : FILE_ATTRIBUTE_NORMAL;
}

int main() {
    g_fo = static_cast<HMODULE>(VirtualAlloc(nullptr, 0x250000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_armada = static_cast<HMODULE>(VirtualAlloc(nullptr, 0x3b0000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    assert(g_fo && g_armada);
    put(at(g_fo, 0x20e644), static_cast<void*>(settings.data()));
    put(at(g_fo, 0x24485c), static_cast<void*>(form.data()));
    put(form.data() + 0x1b4, main_window);
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.log = record;
    api.extension_root_count = test_root_count;
    api.extension_root = test_root;
    api.root_directory = test_base_root;
    g_api = &api;

    existing = {"Selected\\animations\\intro.mp4", "Parent\\intro.mp4"};
    assert(resolve_movie("intro.mp4") == "Selected\\animations\\intro.mp4");
    assert(file_probes.size() == 1);
    std::printf("path:selected:%s\n", file_probes[0].c_str());
    file_probes.clear();
    existing = {"Parent\\intro.mp4"};
    assert(resolve_movie("intro.mp4") == "Parent\\intro.mp4");
    for (const auto& path : file_probes) std::printf("path:probe:%s\n", path.c_str());
    file_probes.clear();
    existing = {"Base\\animations\\intro.webm"};
    assert(modern_replacement_for("intro.bik") == "Base\\animations\\intro.webm");
    for (const auto& path : file_probes) std::printf("replacement:probe:%s\n", path.c_str());
    file_probes.clear();
    existing = {"C:\\movies\\intro.mkv"};
    assert(modern_replacement_for("C:\\movies\\intro.mkv") == "C:\\movies\\intro.mkv");
    assert(file_probes.size() == 1);
    assert(modern_replacement_for("intro.avi").empty());
    assert(resolve_movie(nullptr).empty());
    std::printf("path:direct:%s\n", file_probes[0].c_str());
    file_probes.clear();
    existing = {"Base\\intro.mp4"};
    assert(resolve_movie("intro.mp4") == "Base\\intro.mp4");
    std::printf("path:base:%s\n", file_probes.back().c_str());
    declared_count = 4;
    try {
        (void)resolve_movie("missing.mp4");
        assert(false);
    } catch (const std::out_of_range&) {
        std::puts("path:root-exception:out_of_range");
    }
    declared_count = 3;

    existing.clear();
    skip_input_tests();
    std::puts("skip:all-modes-pass");
    event_tests();
    std::puts("events:all-modes-pass");
    reentry_tests();
    std::printf("hook:native=%u; nested=%u\n", native_play_calls, nested_play_result);
    g_native_play_movie = nullptr;
    g_enabled.store(true);
    assert(!play_movie_hook("missing.bik"));
    g_enabled.store(false);
    std::puts("hook:missing-native:false");
    clear_audio();
    {
        WaveOutput wave;
        std::atomic<bool> stop{false};
        const std::uint8_t sample[] = {1, 2, 3, 4};
        assert(wave.open() && wave.submit(sample, sizeof(sample), stop));
        assert(wave.drain(stop));
        assert(wave.submitted_bytes() == sizeof(sample));
    }
    assert(audio_writes == 1 && audio_resets == 1 && audio_closes == 1 && queued.empty());
    std::printf("wave:normal:%u,%u,%u\n", audio_writes, audio_resets, audio_closes);
    clear_audio(); audio_prepare_fails = true;
    {
        WaveOutput wave;
        std::atomic<bool> stop{false};
        const std::uint8_t sample[] = {1, 2, 3, 4};
        assert(wave.open() && !wave.submit(sample, sizeof(sample), stop));
    }
    assert(audio_writes == 0 && audio_resets == 1 && audio_closes == 1);
    std::printf("wave:prepare-fail:%u,%u,%u\n", audio_writes, audio_resets, audio_closes);
    clear_audio(); audio_write_fails = true;
    {
        WaveOutput wave;
        std::atomic<bool> stop{false};
        const std::uint8_t sample[] = {1, 2, 3, 4};
        assert(wave.open() && !wave.submit(sample, sizeof(sample), stop));
    }
    assert(audio_writes == 1 && audio_resets == 1 && audio_closes == 1);
    std::printf("wave:write-fail:%u,%u,%u\n", audio_writes, audio_resets, audio_closes);
#ifdef A2FO_CURRENT_VIDEO
    assert(!a2fo_rs_stage9_video_result(0, 0, 0));
    assert(a2fo_rs_stage9_video_result(1, 0, 1));
    assert(a2fo_rs_stage9_video_result(0, 1, 0));
    assert(!a2fo_rs_stage9_video_result(0, 1, 1));
#endif
    std::puts("result:all-modes-pass");
    VirtualFree(g_fo, 0, MEM_RELEASE);
    VirtualFree(g_armada, 0, MEM_RELEASE);
}
