// Direct one-shot native allocation-failure checks around the frozen fixture.
#define main fire_arcs_existing_fixture_main
#include "rust_stage10_fire_arcs_native.cpp"
#undef main

#include <new>
#include <malloc.h>

namespace {
bool fail_next_allocation = false;
unsigned failures_consumed = 0;
void arm_failure() {
    fail_next_allocation = true;
}
void require_consumed(const char* label) {
    require(!fail_next_allocation && failures_consumed != 0, label);
    failures_consumed = 0;
}
}

void* operator new(std::size_t size) {
    if (fail_next_allocation) {
        fail_next_allocation = false;
        ++failures_consumed;
        throw std::bad_alloc();
    }
    if (void* pointer = std::malloc(size ? size : 1)) return pointer;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) {
    return ::operator new(size);
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                 SEM_NOOPENFILEERRORBOX);
    try {
        mode = "oom";
        trace.reserve(64);
        fields.reserve(4);
        seed_image();
        g_armada = reinterpret_cast<HMODULE>(image);
        A2FO_ModuleApi api = make_api();
        g_api = &api;
        fixture_roots[0] = "root0000";

        std::string trim_value(100, 'A');
        trim_value.front() = ' ';
        trim_value.back() = ' ';
        const std::string original = trim_value;
        arm_failure();
        bool trim_threw = false;
        try { trim_string(&trim_value); }
        catch (const std::bad_alloc&) { trim_threw = true; }
        require_consumed("trim allocation not consumed");
        require(trim_threw && trim_value == original,
                "trim must throw and preserve input");
        std::puts("trim:bad_alloc:unchanged");

        std::string output;
        bool present = true;
        arm_failure();
        const bool invalid = query_parameter_string(
            nullptr, "test", &output, &present);
        require(!invalid && !present && output.empty() &&
                fail_next_allocation && failures_consumed == 0,
                "invalid query consumed allocation or state wrong");
        fail_next_allocation = false;
        std::puts("query-invalid:false:absent:empty:no-allocation");

        fields["test"] = std::string(100, 'q');
        present = false;
        arm_failure();
        const bool assignment = query_parameter_string(
            parameter_db, "test", &output, &present);
        require_consumed("query assignment allocation not consumed");
        require(!assignment && present && output.empty(),
                "query assignment failure state");
        std::puts("query-assign:false:present:empty");

        output.reserve(260);
        fields["test"] = " " + std::string(100, 'q') + " ";
        present = false;
        arm_failure();
        const bool substring = query_parameter_string(
            parameter_db, "test", &output, &present);
        require_consumed("query substring allocation not consumed");
        require(!substring && present && output.empty(),
                "query substring failure state");
        std::puts("query-trim:false:present:empty");

        mode = "root-missing";
        arm_failure();
        bool root_threw = false;
        try {
#ifdef FIRE_ARCS_CURRENT
            (void)process_firearc_root(0, true);
#else
            (void)load_firearc_enabled();
#endif
        }
        catch (const std::bad_alloc&) { root_threw = true; }
        require_consumed("root allocation not consumed");
        require(root_threw, "root join must throw");
        std::puts("root:bad_alloc");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "oom fixture: %s\n", error.what());
        return 1;
    }
}
