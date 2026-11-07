// Full source allocation and native lock unwind observations. One process per case.
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <windows.h>

#ifndef MODULE_SOURCE
#error MODULE_SOURCE is required
#endif
void fault_enter(CRITICAL_SECTION*);
void fault_leave(CRITICAL_SECTION*);
#define EnterCriticalSection fault_enter
#define LeaveCriticalSection fault_leave
#include MODULE_SOURCE
#undef EnterCriticalSection
#undef LeaveCriticalSection

namespace {
int depth = 0, enters = 0, leaves = 0;
int arm_on_enter = -1, allocation_budget = -1, failures = 0;
int leave_uncaught = -1, leave_during_unwind = 0;
std::array<std::uint8_t, 0x300> team{}, other_team{};
std::array<std::uint8_t, 0x100> producer{};
int klass = 0, race = 0;
void check(bool condition, const char* label) {
    if (!condition) { std::fprintf(stderr, "FAIL:%s\n", label); std::exit(2); }
}
void arm_at(int entry) noexcept {
    arm_on_enter = entry; allocation_budget = -1;
}
bool A2FO_CALL costs(void*, std::int32_t* output, std::uint32_t count) {
    check(count == 4, "cost-count");
    for (std::uint32_t i = 0; i < count; ++i) output[i] = 1;
    return true;
}
void setup() {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    void* image = VirtualAlloc(nullptr, 0x400000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    check(image != nullptr, "image");
    g_armada = static_cast<HMODULE>(image);
    void* shell = reinterpret_cast<void*>(0x1000);
    std::memcpy(static_cast<std::uint8_t*>(image) + kCurrentSetupShellRva, &shell, sizeof(shell));
    InitializeCriticalSection(&g_lock);
    g_lock_ready = g_runtime_alive = g_production_integration_ready = true;
    void* race_pointer = &race;
    std::memcpy(team.data() + kTeamRaceOffset, &race_pointer, sizeof(race_pointer));
    void* team_pointer = team.data();
    std::memcpy(producer.data() + kProducerTeamOffset, &team_pointer, sizeof(team_pointer));
    g_race_starting_resources[&race] = RaceStartingResources{};
    g_team_amounts[other_team.data()] = Amounts{{9, 8, 7, 6}};
    g_squad_costs = &costs;
    g_class_costs[&klass] = Costs{{1, 1, 1, 1}};
}
void verify_lock() {
    check(depth == 0 && enters == leaves, "lock-balanced");
    check(leave_during_unwind >= 1 && leave_uncaught > 0, "unwind-before-leave");
    check(failures == 1 && allocation_budget == -1, "one-targeted-failure");
}
void report(const char* name) {
    std::printf("case:%s:bad_alloc=1:failure=%d:depth=%d:enters=%d:leaves=%d:leave_uncaught=%d:unwind_leaves=%d\n",
                name, failures, depth, enters, leaves, leave_uncaught, leave_during_unwind);
}
void terminated() noexcept {
    std::printf("case:commit:terminate=1:failure=%d:depth=%d:enters=%d:leaves=%d:leave_uncaught=%d:unwind_leaves=%d:uncaught=%d\n",
                failures, depth, enters, leaves, leave_uncaught, leave_during_unwind,
                std::uncaught_exceptions());
    std::fflush(stdout);
    std::_Exit(77);
}
}

void fault_enter(CRITICAL_SECTION* lock) {
    EnterCriticalSection(lock);
    ++depth; ++enters;
    if (enters == arm_on_enter) { allocation_budget = 0; arm_on_enter = -1; }
}
void fault_leave(CRITICAL_SECTION* lock) {
    leave_uncaught = std::uncaught_exceptions();
    if (leave_uncaught > 0) ++leave_during_unwind;
    --depth; ++leaves;
    LeaveCriticalSection(lock);
}
__attribute__((noinline)) void* operator new(std::size_t size) {
    if (allocation_budget == 0) {
        allocation_budget = -1; ++failures; throw std::bad_alloc();
    }
    if (allocation_budget > 0) --allocation_budget;
    void* pointer = std::malloc(size ? size : 1);
    if (!pointer) throw std::bad_alloc();
    return pointer;
}
void* operator new[](std::size_t size) { return ::operator new(size); }
__attribute__((noinline)) void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { ::operator delete(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { ::operator delete(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { ::operator delete(pointer); }

extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_0(void* function, void*) {
    if (function == at(g_armada, kGetGameSetupRva)) return 1;
    if (function == at(g_armada, kGameSetupInfiniteResourcesRva)) return 0;
    if (function == at(g_armada, kGameSetupGetStartingResourcesIndexRva)) return 0;
    std::abort();
}
extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_1(void*, void*, std::uintptr_t) { std::abort(); }
extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_3(void*, void*, std::uintptr_t, std::uintptr_t, std::uintptr_t) { std::abort(); }
extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_7(void*, void*, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t) { std::abort(); }
extern "C" void a2fo_resources_deduct_hook() { std::abort(); }

int main(int argc, char** argv) {
    check(argc == 2, "case-argument");
    setup();
    const char* mode = argv[1];
    bool threw = false;
    if (std::strcmp(mode, "set") == 0) {
        arm_at(enters + 3);
        try { (void)A2FOResources_Set(team.data(), 6, 5); }
        catch (const std::bad_alloc&) { threw = true; }
        check(threw && g_team_amounts.count(team.data()) == 0, "set-state");
        check(g_team_amounts.at(other_team.data()) == Amounts{{9,8,7,6}}, "set-other");
    } else if (std::strcmp(mode, "refund") == 0) {
        arm_at(enters + 3);
        A2FO_ProducerEvent event{sizeof(A2FO_ProducerEvent), A2FO_PRODUCER_EVENT_CANCELLED,
                                 producer.data(), &klass};
        try { (void)producer_event_handler(&event, nullptr); }
        catch (const std::bad_alloc&) { threw = true; }
        check(threw && g_team_amounts.count(team.data()) == 0, "refund-state");
        check(g_team_amounts.at(other_team.data()) == Amounts{{9,8,7,6}}, "refund-other");
    } else if (std::strcmp(mode, "class") == 0) {
        A2FO_GameObjectClassLoadedEvent event{};
        event.struct_size = sizeof(event); int other_class = 0; event.object_class = &other_class;
        arm_at(enters + 1);
        try { class_loaded_handler(&event, nullptr); }
        catch (const std::bad_alloc&) { threw = true; }
        check(threw && g_class_costs.count(&other_class) == 0, "class-state");
        check(g_class_costs.at(&klass) == Costs{{1,1,1,1}}, "class-other");
    } else if (std::strcmp(mode, "race") == 0) {
        g_race_starting_resources[&race].normal[0] = 3;
        g_presentation_generation = 41;
        g_presentation_cache.team = team.data(); g_presentation_cache.generation = 41;
        g_panel_text_cache.team = team.data(); g_panel_text_cache.generation = 41;
        g_panel_tooltip_component = team.data(); g_panel_tooltip_team = team.data();
        g_panel_tooltip_resource = 6;
        const A2FO_OdfFieldView fields[] = {{{"normalTritanium", 15}, {"12", 2}}};
        A2FO_RaceLoadedEvent event{}; event.struct_size = sizeof(event);
        event.race = &race; event.odf_fields = fields; event.odf_field_count = 1;
        arm_at(enters + 1);
        try { race_loaded_handler(&event, nullptr); }
        catch (const std::bad_alloc&) { threw = true; }
        check(threw && g_race_starting_resources.at(&race).normal[0] == 12,
              "race-first-map-written");
        check(g_race_presentations.count(&race) == 0 && g_presentation_generation == 41,
              "race-second-map-unchanged");
        check(g_presentation_cache.team == team.data() && g_presentation_cache.generation == 41 &&
              g_panel_text_cache.team == team.data() && g_panel_text_cache.generation == 41 &&
              g_panel_tooltip_component == team.data() && g_panel_tooltip_team == team.data() &&
              g_panel_tooltip_resource == 6, "race-cache-preserved");
    } else if (std::strcmp(mode, "commit") == 0) {
        std::set_terminate(terminated);
        arm_at(enters + 3);
        a2fo_resources_commit_payment(producer.data(), &klass);
        check(false, "commit-must-terminate");
    } else check(false, "unknown-case");
    verify_lock(); report(mode);
    return 0;
}
