// Full frozen/current resource workflow fixture; source is selected at build time.
#ifdef A2FO_RESOURCES_PRESENTATION
#ifndef A2FO_RESOURCES_PRESENTATION_FIXTURE
#error A2FO_RESOURCES_PRESENTATION_FIXTURE is required
#endif
#define main presentation_resources_main
#include A2FO_RESOURCES_PRESENTATION_FIXTURE
#undef main
int main() {
    std::puts("ready:resources-presentation");
    std::fflush(stdout);
    presentation_resources_main();
    std::puts("completed:resources-presentation");
    return 0;
}
#elif defined(A2FO_RESOURCES_INHERITED)
#define main inherited_resources_main
#include "rust_stage8_c_resources_mock.cpp"
#undef main
int main() {
    std::puts("ready:resources-inherited");
    std::fflush(stdout);
    const int result = inherited_resources_main();
    if (result == 0) std::puts("completed:resources-inherited");
    return result;
}
#else
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>
#include <windows.h>
#ifndef MODULE_SOURCE
#error MODULE_SOURCE is required
#endif
void fixture_enter(CRITICAL_SECTION* lock);
void fixture_leave(CRITICAL_SECTION* lock);
#define EnterCriticalSection fixture_enter
#define LeaveCriticalSection fixture_leave
#include MODULE_SOURCE
#undef EnterCriticalSection
#undef LeaveCriticalSection

namespace {
int setup_calls = 0;
bool unlimited = false;
int cost_calls = 0;
bool cost_success = true;
bool zero_costs = false;
int legacy_calls = 0;
int native_calls = 0;
bool legacy_result = true;
bool native_disable = false;
std::string render_order;
void* fixture_local_team = nullptr;
int lock_depth = 0;
int lock_enters = 0;
int lock_leaves = 0;
int injection_enter = 0;
bool inject_final_lock = false;
bool arm_final_lock = false;
void* observed_team = nullptr;
void* observed_class = nullptr;
std::array<std::uint8_t, 0x300> team{};
std::array<std::uint8_t, 0x100> producer{};
int object_class = 0;
void expect(bool value, const char* name) {
    if (!value) { std::fprintf(stderr, "FAIL:%s\n", name); std::exit(2); }
    std::printf("check:%s\n", name);
}
void print_amounts(const char* label) {
    const auto values = team_snapshot(team.data());
    std::printf("%s:%lld:%lld:%lld:%lld\n", label,
        static_cast<long long>(values[0]), static_cast<long long>(values[1]),
        static_cast<long long>(values[2]), static_cast<long long>(values[3]));
}
bool A2FO_CALL costs_callback(void* type, std::int32_t* values, std::uint32_t count) {
    ++cost_calls;
    expect(type == observed_class && count == 4, "cost-arguments");
    expect(lock_depth == 0, "cost-outside-lock");
    // Reentry observes the balance before payment, outside the payment lock.
    const auto seen = A2FOResources_Get(observed_team, 6);
    std::printf("cost-read:%d:%lld\n", cost_calls, static_cast<long long>(seen));
    if (cost_calls == 2) A2FOResources_Set(observed_team, 6, 9);
    if (inject_final_lock) {
        arm_final_lock = true;
        injection_enter = 0;
        inject_final_lock = false;
    }
    for (std::uint32_t index = 0; index < count; ++index) values[index] =
        zero_costs ? 0 : static_cast<std::int32_t>(index + 1);
    return cost_success;
}
bool __cdecl legacy_render(void*) {
    ++legacy_calls;
    render_order += 'L';
    std::printf("legacy:%d:%d\n", legacy_calls, legacy_result);
    return legacy_result;
}
void install_stub(void* module, std::uintptr_t rva,
                  const std::uint8_t* bytes, std::size_t size) {
    void* target = at(static_cast<HMODULE>(module), rva);
    DWORD previous = 0;
    expect(VirtualProtect(target, size, PAGE_EXECUTE_READWRITE, &previous) != 0,
           "stub-protect");
    std::memcpy(target, bytes, size);
    FlushInstructionCache(GetCurrentProcess(), target, size);
}
void put_address(std::uint8_t* bytes, std::size_t offset, const void* address) {
    const auto word = reinterpret_cast<std::uintptr_t>(address);
    static_assert(sizeof(word) == 4, "32-bit fixture only");
    std::memcpy(bytes + offset, &word, sizeof(word));
}
}

void fixture_enter(CRITICAL_SECTION* lock) {
    if (arm_final_lock && ++injection_enter == 2) {
        // Simulate another writer after the commit snapshot, before its lock.
        g_team_amounts[team.data()][0] = 0;
        arm_final_lock = false;
        std::puts("inject:before-final-lock");
    }
    EnterCriticalSection(lock);
    ++lock_depth;
    ++lock_enters;
}
void fixture_leave(CRITICAL_SECTION* lock) {
    --lock_depth;
    ++lock_leaves;
    LeaveCriticalSection(lock);
}

extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_0(void* function, void*) {
    if (function == at(g_armada, kGetGameSetupRva)) { ++setup_calls; return 1; }
    if (function == at(g_armada, kGameSetupInfiniteResourcesRva)) return unlimited;
    if (function == g_resource_panel_render_hook.gateway) {
        ++native_calls;
        render_order += 'N';
        if (native_disable) g_runtime_alive = false;
        std::printf("native:%d:%d\n", native_calls, native_disable);
        return 0;
    }
    std::abort();
}
extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_1(
    void*, void*, std::uintptr_t) { std::abort(); }
extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_3(
    void*, void*, std::uintptr_t, std::uintptr_t, std::uintptr_t) { std::abort(); }
extern "C" std::uintptr_t __cdecl a2fo_resources_call_thiscall_7(
    void*, void*, std::uintptr_t, std::uintptr_t, std::uintptr_t,
    std::uintptr_t, std::uintptr_t, std::uintptr_t, std::uintptr_t) { std::abort(); }
extern "C" void a2fo_resources_deduct_hook() { std::abort(); }
int main() {
    std::puts("ready:resources-workflow"); std::fflush(stdout);
    void* memory = VirtualAlloc(nullptr, 0x400000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    expect(memory != nullptr, "armada-allocation");
    g_armada = static_cast<HMODULE>(memory);
    void* shell = reinterpret_cast<void*>(0x1000);
    std::memcpy(static_cast<std::uint8_t*>(memory) + kCurrentSetupShellRva, &shell, sizeof(shell));
    InitializeCriticalSection(&g_lock); g_lock_ready = true; g_runtime_alive = true;
    g_production_integration_ready = true;
    observed_team = team.data(); observed_class = &object_class;
    void* team_address = team.data();
    std::memcpy(producer.data() + kProducerTeamOffset, &team_address, sizeof(team_address));
    g_team_amounts[team.data()] = Amounts{{10, 20, 30, 40}};
    g_class_costs[&object_class] = Costs{{4, 5, 6, 7}};
    g_squad_costs = &costs_callback;
    print_amounts("initial");
    expect(a2fo_resources_can_pay(producer.data(), &object_class), "can-pay");
    expect(cost_calls == 1, "can-pay-cost-once");
    a2fo_resources_commit_payment(producer.data(), &object_class);
    expect(cost_calls == 2, "payment-cost-once");
    print_amounts("after-payment");
    expect(A2FOResources_Get(team.data(), 6) == 8, "debit-reentry-value");
    A2FO_ProducerEvent event{sizeof(A2FO_ProducerEvent), A2FO_PRODUCER_EVENT_CANCELLED,
                              producer.data(), &object_class};
    expect(producer_event_handler(&event, nullptr), "refund-result");
    print_amounts("after-refund");
    expect(A2FOResources_Get(team.data(), 6) == 9, "refund-value");
    cost_success = false;
    expect(A2FOResources_GetCost(&object_class, 6) == 4, "class-cost-fallback");
    expect(a2fo_resources_can_pay(producer.data(), &object_class),
           "fallback-can-pay");
    cost_success = true;
    zero_costs = true;
    expect(a2fo_resources_can_pay(reinterpret_cast<void*>(1), &object_class),
           "zero-cost-skips-team-read");
    zero_costs = false;
    const int before_infinite = cost_calls;
    unlimited = true;
    expect(a2fo_resources_can_pay(producer.data(), &object_class), "infinite-can-pay");
    a2fo_resources_commit_payment(producer.data(), &object_class);
    producer_event_handler(&event, nullptr);
    expect(cost_calls == before_infinite, "infinite-skips-costs");
    print_amounts("infinite-unchanged");
    unlimited = false;
    event.kind = A2FO_PRODUCER_EVENT_ADMIT;
    producer_event_handler(&event, nullptr);
    expect(cost_calls == before_infinite, "non-refund-skips-costs");
    event.kind = static_cast<A2FO_ProducerEventKind>(UINT32_MAX);
    expect(producer_event_handler(&event, nullptr), "malformed-kind-result");
    expect(cost_calls == before_infinite, "malformed-kind-skips-costs");
    g_team_amounts[team.data()] = Amounts{{10, 20, 30, 40}};
    const int before_race_enters = lock_enters;
    const int before_race_leaves = lock_leaves;
    inject_final_lock = true;
    a2fo_resources_commit_payment(producer.data(), &object_class);
    expect(!arm_final_lock && injection_enter == 2,
           "final-lock-injection-reached");
    expect(lock_depth == 0 && lock_enters == before_race_enters + 3 &&
           lock_leaves == before_race_leaves + 3, "commit-lock-order");
    expect(g_team_amounts[team.data()] == Amounts{{0, 20, 30, 40}},
           "final-lock-recheck-blocked-debit");
    print_amounts("raced-payment");
    expect(!A2FOResources_Set(team.data(), 6, -1), "negative-set-rejected");
    expect(A2FOResources_Set(team.data(), 6, INT64_MAX), "maximum-set");
    expect(A2FOResources_Add(team.data(), 6, 1), "saturating-add");
    expect(A2FOResources_Get(team.data(), 6) == INT64_MAX, "add-at-maximum");
    event.kind = A2FO_PRODUCER_EVENT_CANCELLED;
    producer_event_handler(&event, nullptr);
    expect(A2FOResources_Get(team.data(), 6) == INT64_MAX, "credit-at-maximum");
    const auto after_maximum = team_snapshot(team.data());
    std::printf("saturation:%lld:%lld:%lld:%lld\n",
        static_cast<long long>(after_maximum[0]),
        static_cast<long long>(after_maximum[1]),
        static_cast<long long>(after_maximum[2]),
        static_cast<long long>(after_maximum[3]));
    unsigned short saved_control = 0;
    __asm__ volatile("fnstcw %0" : "=m"(saved_control));
    g_panel_configuration.native_rectangle_found[1] = true;
    g_panel_configuration.native_rectangles[1] = {0, 0, 100, 100};
    g_panel_configuration.rectangle_found[0] = true;
    g_panel_configuration.rectangles[0] = {-25, 10, 60, 20};
    g_panel_configuration.rectangle_found[1] = true;
    g_panel_configuration.rectangles[1] = {40, -15, 50, 30};
    const NativeRectangle viewport{10, 20, 111, 79};
    for (unsigned precision : {0u, 2u, 3u}) {
        for (unsigned rounding : {0u, 1u, 2u, 3u}) {
            const unsigned short control = static_cast<unsigned short>(
                (saved_control & ~0x0f00u) | (precision << 8) | (rounding << 10));
            __asm__ volatile("fldcw %0" : : "m"(control));
            __asm__ volatile("fnclex");
            unsigned short status_before = 0;
            __asm__ volatile("fnstsw %0" : "=a"(status_before));
            expect(A2FOResources_Add(team.data(), 6, 1), "cw-add");
            for (std::size_t index = 0; index < 2; ++index) {
                const auto projected = resource_panel_rectangle(index, 1, viewport);
                const auto amount = panel_amount_rectangle(index, projected);
                expect(projected.right > projected.left &&
                       projected.bottom > projected.top &&
                       amount.right > amount.left, "cw-projection-valid");
                unsigned short projection_control = 0;
                unsigned short projection_status = 0;
                __asm__ volatile("fnstcw %0" : "=m"(projection_control));
                __asm__ volatile("fnstsw %0" : "=a"(projection_status));
                expect(projection_control == control &&
                       (projection_status & 0x3800u) ==
                       (status_before & 0x3800u), "cw-projection-preserved");
                std::printf("rectcw:%u:%u:%lu:%d:%d:%d:%d:%d:%d:%d:%d:%04x:%02x:%04x\n",
                    precision, rounding, static_cast<unsigned long>(index),
                    projected.left, projected.top, projected.right,
                    projected.bottom, amount.left, amount.top, amount.right,
                    amount.bottom, projection_control,
                    static_cast<unsigned>(projection_status & 0x023fu),
                    static_cast<unsigned>(projection_status & 0x3800u));
            }
            unsigned short actual = 0;
            __asm__ volatile("fnstcw %0" : "=m"(actual));
            expect(actual == control, "cw-preserved");
            std::printf("cw:%u:%u:%04x\n", precision, rounding, actual);
        }
    }
    __asm__ volatile("fldcw %0" : : "m"(saved_control));
    g_squad_costs = nullptr;
    g_a1_legacy_resource_panel_render = &legacy_render;
    g_resource_panel_render_hook.gateway = reinterpret_cast<void*>(0x1);
    resource_panel_render_hook(team.data(), nullptr);
    expect(legacy_calls == 1 && native_calls == 1 && render_order == "LN",
           "a1-legacy-route");
    fixture_local_team = team.data();
    std::uint8_t local_stub[] = {0x31, 0xc0, 0xc3};
    install_stub(memory, kLocalTeamRva, local_stub, sizeof(local_stub));
    std::uint8_t team_stub[] = {0xa1, 0, 0, 0, 0, 0xc3};
    put_address(team_stub, 1, &fixture_local_team);
    install_stub(memory, kTeamForTeamRva, team_stub, sizeof(team_stub));
    std::array<std::uint8_t, 0x120> panel{};
    legacy_result = false;
    g_presentation_cache = {};
    g_panel_text_cache = {};
    g_panel_configuration = {};
    resource_panel_render_hook(panel.data(), nullptr);
    expect(legacy_calls == 2 && native_calls == 2 && render_order == "LNLN",
           "nonlegacy-gateway-order");
    expect(g_presentation_cache.team == team.data() &&
           g_panel_text_cache.team == team.data() &&
           g_panel_configuration.loaded, "nonlegacy-cache-and-config");
    expect(g_panel_text_cache.amounts == g_team_amounts[team.data()] &&
           g_panel_text_cache.lines[0] == "9223372036854775807" &&
           g_panel_text_cache.lines[3] == "44", "nonlegacy-amount-lines");
    std::printf("panel-live:%s:%s:%d\n", g_panel_text_cache.lines[0].c_str(),
                g_panel_text_cache.lines[3].c_str(), g_panel_configuration.loaded);
    const char* first_line = g_panel_text_cache.lines[0].c_str();
    resource_panel_render_hook(panel.data(), nullptr);
    expect(legacy_calls == 3 && native_calls == 3 && render_order == "LNLNLN",
           "nonlegacy-repeat-order");
    expect(g_panel_text_cache.lines[0].c_str() == first_line,
           "nonlegacy-cache-fresh-reuse");
    g_presentation_cache = {};
    g_panel_text_cache = {};
    native_disable = true;
    resource_panel_render_hook(panel.data(), nullptr);
    expect(legacy_calls == 4 && native_calls == 4 && render_order == "LNLNLNLN",
           "gateway-disable-order");
    expect(!g_runtime_alive && !g_presentation_cache.team &&
           !g_panel_text_cache.team, "gateway-disable-skips-cache");
    g_runtime_alive = false; g_lock_ready = false; DeleteCriticalSection(&g_lock);
    VirtualFree(memory, 0, MEM_RELEASE);
    std::puts("completed:resources-workflow");
    return 0;
}
#endif
