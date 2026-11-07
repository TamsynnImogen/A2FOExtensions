// Synthetic checks for the native A2FOVideo facade. No decoder, window,
// input device, audio device, or game binary is used by these tests.
#include <windows.h>
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

DWORD WINAPI fake_GetFileAttributesA(LPCSTR path);
#define GetFileAttributesA fake_GetFileAttributesA
#define A2FO_BUILTIN_STARTUP 1
#define A2FO_ModuleInit A2FOBuiltinVideo_Init
#define A2FO_ModuleShutdown A2FOBuiltinVideo_Shutdown
#include "../modules/A2FOVideo/module.cpp"
#undef GetFileAttributesA

namespace {
std::vector<std::string> files;
std::string logs;
std::string last_native_request;
std::u16string last_media_request;
unsigned native_calls = 0;
unsigned media_opens = 0;
unsigned media_closes = 0;
unsigned media_shutdowns = 0;
unsigned media_startups = 0;
unsigned media_infos = 0;
bool nested_result = true;
bool request_nested_play = false;
bool shutdown_during_open = false;
bool shutdown_on_intercept = false;
std::uint32_t media_session = 0;

void A2FO_CALL record(const char*, const char* message) {
    logs += message;
    logs += '\n';
    if (shutdown_on_intercept &&
        std::strncmp(message, "Movie request intercepted:", 26) == 0) {
        shutdown_on_intercept = false;
        A2FOBuiltinVideo_Shutdown();
    }
}
const char* A2FO_CALL root_directory() { return "C:\\Base"; }
std::uint32_t A2FO_CALL extension_root_count() { return 2; }
const char* A2FO_CALL extension_root(std::uint32_t index) {
    return index == 0 ? "C:\\Data" : index == 1 ? "C:\\Mod" : nullptr;
}

bool WINAPI native_play(const char* filename) {
    ++native_calls;
    last_native_request = filename ? filename : "<null>";
    if (request_nested_play) {
        request_nested_play = false;
        nested_result = play_movie_hook("nested.bik");
    }
    return true;
}

std::uint32_t media_open(const A2FO_MediaOpen* request) {
    ++media_opens;
    assert(request && request->size == sizeof(*request));
    assert(request->version == A2FO_MEDIA_VERSION);
    assert(request->threads == 4);
    last_media_request.assign(reinterpret_cast<const char16_t*>(request->path),
                              request->path_length);
    if (shutdown_during_open) {
        shutdown_during_open = false;
        A2FOBuiltinVideo_Shutdown();
        return 0;
    }
    return media_session;
}
std::uint32_t media_info(std::uint32_t session, A2FO_MediaInfo* info) {
    ++media_infos;
    assert(session == media_session && info && info->size == sizeof(*info));
    return 0; // Stop before the facade queries or touches a window.
}
std::uint32_t media_play(std::uint32_t, const A2FO_MediaPlay*) {
    assert(false && "synthetic media_info must prevent GUI playback");
    return 0;
}
std::uint32_t media_seek(std::uint32_t, std::int64_t) { return 0; }
void media_cancel(std::uint32_t) {}
void media_close(std::uint32_t session) {
    assert(session == media_session);
    ++media_closes;
}
std::uint32_t media_shutdown() { ++media_shutdowns; return 1; }
std::uint32_t media_startup() { ++media_startups; return 1; }
A2FO_MediaApiV1 fakeMediaApi{
    sizeof(A2FO_MediaApiV1), A2FO_MEDIA_VERSION,
    media_open, media_info, media_play, media_seek, media_cancel,
    media_close, media_shutdown, media_startup
};

void reset() {
    files.clear(); logs.clear(); last_native_request.clear(); last_media_request.clear();
    native_calls = media_opens = media_closes = media_shutdowns = media_startups = media_infos = 0;
    nested_result = true;
    request_nested_play = false;
    shutdown_during_open = shutdown_on_intercept = false;
    media_session = 0;
    static A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = record;
    api.root_directory = root_directory;
    api.extension_root_count = extension_root_count;
    api.extension_root = extension_root;
    g_api.store(&api, std::memory_order_release);
    g_active_hooks.store(0, std::memory_order_seq_cst);
    g_hook_depth = 0;
    g_playing = false;
    g_native_play_movie = native_play;
    g_media = &fakeMediaApi;
    g_enabled.store(true, std::memory_order_seq_cst);
}

void resolution_tests() {
    reset();
    files = {"C:\\Data\\animations\\clip.mp4", "C:\\Mod\\clip.mp4",
             "C:\\Mod\\animations\\clip.mp4"};
    assert(resolve_movie("clip.mp4") == "C:\\Mod\\animations\\clip.mp4");
    files.erase(std::remove(files.begin(), files.end(), "C:\\Mod\\animations\\clip.mp4"), files.end());
    assert(resolve_movie("clip.mp4") == "C:\\Mod\\clip.mp4");
    files = {"C:\\Base\\animations\\clip.mp4"};
    assert(resolve_movie("clip.mp4") == "C:\\Base\\animations\\clip.mp4");
    files = {"C:\\Base\\clip.mp4"};
    assert(resolve_movie("clip.mp4") == "C:\\Base\\clip.mp4");
    files = {"C:\\Direct\\clip.mp4", "C:\\Mod\\animations\\clip.mp4"};
    assert(resolve_movie("C:\\Direct\\clip.mp4") == "C:\\Direct\\clip.mp4");
    assert(resolve_movie("missing.mp4").empty());

    files = {"C:\\Mod\\animations\\clip.mp4", "C:\\Mod\\animations\\clip.webm"};
    assert(modern_replacement_for("clip.BIK") == "C:\\Mod\\animations\\clip.mp4");
    files.erase(files.begin());
    assert(modern_replacement_for("clip.bik") == "C:\\Mod\\animations\\clip.webm");
    files = {"C:\\Data\\animations\\clip.mkv"};
    assert(modern_replacement_for("clip.bik") == "C:\\Data\\animations\\clip.mkv");
    assert(modern_replacement_for("clip.avi").empty());
    assert(modern_replacement_for(nullptr).empty());
}

void routing_tests() {
    reset();
    files = {"C:\\Mod\\animations\\clip.mp4"};
    media_session = 7;
    assert(play_movie_hook("clip.bik"));
    assert(media_opens == 1 && media_infos == 1 && media_closes == 1);
    assert(last_media_request == u"C:\\Mod\\animations\\clip.mp4");
    assert(native_calls == 1 && last_native_request == "clip.bik");
    assert(logs.find("Modern movie playback failed") != std::string::npos);

    files.clear();
    assert(play_movie_hook("clip.avi"));
    assert(native_calls == 2 && media_opens == 1 && last_native_request == "clip.avi");

    g_playing = true;
    assert(!play_movie_hook("clip.bik"));
    assert(native_calls == 2 && media_opens == 1);
    g_playing = false;
    assert(logs.find("Reentrant movie request declined") != std::string::npos);

    request_nested_play = true;
    assert(play_movie_hook("outer.fixture"));
    assert(!nested_result && native_calls == 3);
}

void lifecycle_tests() {
    reset();
    assert(load_media_backend()); // Cached fake API: no DLL loading.
    assert(media_startups == 1);

    A2FOBuiltinVideo_Shutdown();
    assert(!g_enabled.load(std::memory_order_seq_cst));
    assert(media_shutdowns == 1);
    assert(g_api.load(std::memory_order_acquire) == nullptr);
    assert(play_movie_hook("native.bik"));
    assert(native_calls == 1 && media_opens == 0);

    reset();
    g_playing = true;
    g_hook_depth = 1;
    g_active_hooks.store(1, std::memory_order_seq_cst);
    A2FOBuiltinVideo_Shutdown(); // Reentrant shutdown must return before the active hook exits.
    assert(media_shutdowns == 1);
    assert(g_api.load(std::memory_order_acquire) != nullptr);
    g_playing = false;
    g_hook_depth = 0;
    g_active_hooks.store(0, std::memory_order_seq_cst);
    assert(play_movie_hook("native.bik"));
    assert(g_api.load(std::memory_order_acquire) == nullptr);
    assert(native_calls == 1 && media_opens == 0);
}

void shutdown_overlap_tests() {
    reset();
    files = {"C:\\Mod\\animations\\clip.mp4"};
    shutdown_during_open = true;
    assert(play_movie_hook("clip.bik"));
    assert(media_opens == 1 && media_infos == 0 && media_closes == 0);
    assert(media_shutdowns == 1 && native_calls == 0);
    assert(!g_enabled.load(std::memory_order_seq_cst));
    assert(g_api.load(std::memory_order_acquire) == nullptr);
    assert(g_active_hooks.load(std::memory_order_seq_cst) == 0 && g_hook_depth == 0);

    reset();
    files = {"C:\\Mod\\animations\\clip.mp4"};
    shutdown_on_intercept = true;
    assert(play_movie_hook("clip.bik"));
    assert(media_shutdowns == 1 && native_calls == 0);
    assert(media_infos == 0 && media_closes == 0);
    assert(!g_enabled.load(std::memory_order_seq_cst));
    assert(g_api.load(std::memory_order_acquire) == nullptr);
    assert(g_active_hooks.load(std::memory_order_seq_cst) == 0 && g_hook_depth == 0);
}
} // namespace

DWORD WINAPI fake_GetFileAttributesA(LPCSTR path) {
    if (!path) return INVALID_FILE_ATTRIBUTES;
    return std::find(files.begin(), files.end(), path) != files.end()
        ? FILE_ATTRIBUTE_NORMAL : INVALID_FILE_ATTRIBUTES;
}

int main(int argc,char** argv) {
    if(argc==3&&std::strcmp(argv[1],"--backend")==0){
        // This executable is copied beside the real sidecar. Loading/pinning
        // and container opens execute production code; no window is queried.
        g_api.store(nullptr);g_media=nullptr;g_media_library=nullptr;
        assert(load_media_backend());assert(g_media&&g_media_library);assert(load_media_backend());
        const int n=MultiByteToWideChar(CP_UTF8,0,argv[2],-1,nullptr,0);assert(n>1);std::vector<wchar_t> path(n);
        assert(MultiByteToWideChar(CP_UTF8,0,argv[2],-1,path.data(),n));
        A2FO_MediaOpen request{sizeof(request),1,reinterpret_cast<const std::uint16_t*>(path.data()),static_cast<std::uint32_t>(n-1),4,nullptr,nullptr};
        const auto id=g_media->open(&request);assert(id);A2FO_MediaInfo info{};info.size=sizeof(info);assert(g_media->info(id,&info)&&info.width&&info.height&&info.has_audio);
        g_media->close(id);assert(!g_media->info(id,&info));A2FOBuiltinVideo_Shutdown();
        std::puts("actual Rust sidecar loader/startup/open/close/shutdown passed");return 0;
    }

    resolution_tests();
    routing_tests();
    lifecycle_tests();
    shutdown_overlap_tests();
    std::puts("video facade tests passed");
}
