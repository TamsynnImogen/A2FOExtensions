#pragma once
#include <stdexcept>
#include "mmm_armada/ScriptInterface.h"

namespace mmm_test {
void event(const char*);
int current_interface_id();
extern bool throw_construct;
extern bool throw_start;
extern bool throw_resume;
}

namespace mmm {
struct Worker {
    Worker() {
        mmm_test::event("worker:create");
        if (mmm_test::throw_construct) throw std::runtime_error("construct");
    }
    ~Worker() {
        mmm_test::event(mmm_test::current_interface_id() == 2 ? "worker:destroy:interface2" :
                        mmm_test::current_interface_id() == 1 ? "worker:destroy:interface1" :
                        "worker:destroy:no-interface");
        if (getScriptInterface()) getScriptInterface()->MonitorInputControls(nullptr);
    }
    void Startup() {
        mmm_test::event("worker:startup");
        if (mmm_test::throw_start) throw std::runtime_error("startup");
    }
    void Resume() {
        mmm_test::event("worker:resume");
        if (mmm_test::throw_resume) throw std::runtime_error("resume");
    }
};
}
