// Compile this fixture twice, selecting the historical or migrated module.cpp.
// Every invocation runs one case in a fresh process, since module state is global.
// ReadProcessMemory is substituted only while including the module translation
// unit (also its fault observer header). The observer is not exercised here;
// all exported module calls and controller transitions use their real source.
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <vector>

namespace mmm { struct ScriptInterface; }
namespace mmm_test {
std::vector<std::string> events;
bool throw_construct = false;
bool throw_start = false;
bool throw_resume = false;
bool pending_finish = false;
unsigned char (__stdcall* input)() = nullptr;
mmm::ScriptInterface* first = nullptr;
mmm::ScriptInterface* second = nullptr;
bool edit_probe_enabled = false;
std::uint8_t edit_state[256]{};

void event(const char* line) { events.emplace_back(line); }
void capture_input(unsigned char (__stdcall* callback)()) { input = callback; }
int current_interface_id();
std::size_t printed_events = 0;
struct Finalizer { ~Finalizer(); } finalizer;
}

BOOL WINAPI MmmFixtureReadProcessMemory(HANDLE, LPCVOID source, LPVOID destination,
                                        SIZE_T size, SIZE_T* copied) {
    if (copied) *copied = 0;
    if (!mmm_test::edit_probe_enabled || !destination) return FALSE;
    if (source == reinterpret_cast<LPCVOID>(0x0076B5ACu) && size == sizeof(std::uintptr_t)) {
        const std::uintptr_t pointer = reinterpret_cast<std::uintptr_t>(mmm_test::edit_state);
        std::memcpy(destination, &pointer, size);
    } else if (source == mmm_test::edit_state + 0x84 && size == 1) {
        std::memcpy(destination, mmm_test::edit_state + 0x84, size);
    } else {
        return FALSE;
    }
    if (copied) *copied = size;
    return TRUE;
}

#ifndef A2FO_MMM_MODULE_SOURCE
#error Define A2FO_MMM_MODULE_SOURCE to the quoted absolute module.cpp path.
#endif
#define ReadProcessMemory MmmFixtureReadProcessMemory
#include A2FO_MMM_MODULE_SOURCE
#undef ReadProcessMemory

namespace mmm_test {
Finalizer::~Finalizer() {
    for (std::size_t i = printed_events; i < events.size(); ++i)
        std::printf("exit-event:%s\n", events[i].c_str());
    std::printf("EXIT:%zu\n", events.size() - printed_events);
}
int current_interface_id() {
    return mmm::getScriptInterface() == first ? 1 :
           mmm::getScriptInterface() == second ? 2 : 0;
}
void logger(const char*, const char* message) { events.emplace_back(std::string("log:") + message); }
void result(const char* operation, int value) {
    events.emplace_back(std::string(operation) + ":" + std::to_string(value));
}
template<class F> void call(const char* operation, F action) {
    try { result(operation, action()); }
    catch (const std::exception& error) {
        events.emplace_back(std::string(operation) + ":throw:" + error.what());
    }
}
void extend(mmm::ScriptInterface* value) {
    call("extend", [=] { return A2FOMissions_ScriptExtendInterface(value); });
}
void init() { call("init", [] { return A2FOMissions_ScriptInit(nullptr); }); }
void tick() {
    if (!input) { event("input:missing"); return; }
    call("input", [] { return static_cast<int>(input()); });
}
void direct_input() {
    event("synthetic-direct-input");
    call("input", [] { return static_cast<int>(mmm::a2fo_missions::InputHandler()); });
}
}

#if defined(A2FO_MMM_FIXTURE_DLL)
extern "C" __declspec(dllexport) int __cdecl MmmFixtureRunScenario(const char* scenario) {
    int argc = 2;
    char* argv[] = {nullptr, const_cast<char*>(scenario)};
#else
int main(int argc, char** argv) {
#endif
    if (argc != 2) return 2;
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.api_version = A2FO_MODULE_API_VERSION;
    api.log = mmm_test::logger;
    if (!A2FO_ModuleInit(&api)) return 3;
    // Remain live through the module CRT finalizers in this synthetic process.
    auto& one = *new mmm::ScriptInterface;
    auto& two = *new mmm::ScriptInterface;
    mmm_test::first = &one;
    mmm_test::second = &two;
    const char* name = argv[1];

    using namespace mmm_test;
    if (std::strcmp(name, "before-extend") == 0) {
        init();
    } else if (std::strcmp(name, "null-extend") == 0) {
        extend(nullptr);
    } else if (std::strcmp(name, "new-game") == 0) {
        extend(&one); init(); tick(); tick();
    } else if (std::strcmp(name, "deferred-finish") == 0) {
        extend(&one); init(); pending_finish = true; tick(); tick();
    } else if (std::strcmp(name, "detach-null-extend") == 0) {
        extend(&one); init(); tick();
        A2FOMissions_MarkMissionUnloaded();
        extend(nullptr); extend(&two); init(); tick();
    } else if (std::strcmp(name, "resume") == 0) {
        extend(&one); init(); tick();
        A2FOMissions_MarkMissionUnloaded(); extend(&two);
        // Deliberately no ScriptInit: this is the legacy resume route.
        tick(); direct_input();
    } else if (std::strcmp(name, "repeated-extend") == 0) {
        extend(&one); init(); tick(); extend(&two); init(); tick();
    } else if (std::strcmp(name, "construct-retry") == 0) {
        extend(&one); init(); throw_construct = true; tick();
        throw_construct = false; tick();
    } else if (std::strcmp(name, "start-retry") == 0) {
        extend(&one); init(); throw_start = true; tick();
        throw_start = false; tick();
    } else if (std::strcmp(name, "resume-retry") == 0) {
        extend(&one); init(); tick();
        A2FOMissions_MarkMissionUnloaded(); extend(&two);
        tick(); throw_resume = true; direct_input(); throw_resume = false; direct_input();
    } else if (std::strcmp(name, "shutdown") == 0) {
        extend(&one); init(); tick(); A2FO_ModuleShutdown();
    } else if (std::strcmp(name, "edit-mode") == 0) {
        edit_state[0x84] = 1;
        edit_probe_enabled = true;
        const int extend_result = A2FOMissions_ScriptExtendInterface(&one);
        const int init_result = A2FOMissions_ScriptInit(nullptr);
        edit_probe_enabled = false;
        result("extend", extend_result);
        result("init", init_result);
        bool valid = extend_result == 51 && init_result == 51 && input == nullptr;
        for (const auto& item : events) {
            if (item.rfind("worker:", 0) == 0 || item.rfind("interface:monitor", 0) == 0)
                valid = false;
        }
        if (!valid) return 6;
    } else return 4;

    for (const auto& item : events) std::puts(item.c_str());
    std::printf("SUMMARY:%s:%zu\n", name, events.size());
    printed_events = events.size();
    return 0;
}
