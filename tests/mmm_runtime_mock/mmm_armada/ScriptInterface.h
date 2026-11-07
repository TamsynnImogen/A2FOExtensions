#pragma once

namespace mmm_test {
void event(const char*);
void capture_input(unsigned char (__stdcall*)());
}

namespace mmm {
struct hot_int;
struct ScriptInterface {
    void MonitorInputControls(unsigned char (__stdcall* callback)()) {
        mmm_test::event(callback ? "interface:monitor" : "interface:monitor:clear");
        mmm_test::capture_input(callback);
    }
    hot_int* ScriptCreate_hot_int(const char*) { return nullptr; }
};

inline ScriptInterface* mock_interface = nullptr;
inline ScriptInterface* getScriptInterface() { return mock_interface; }
inline void setScriptInterface(ScriptInterface* next) {
    mmm_test::event(next ? "interface:set" : "interface:clear");
    mock_interface = next;
}
}
