#include "../core/builtin_feature_pack_symbols.hpp"
#include "../modules/A2FOFeaturePack/hybrid_bridge_client.hpp"
#include "../modules/A2FOFeaturePack/hybrid_bridge_api.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>

// The actual HybridBuild implementation retains these unprefixed names.
#undef hybrid_production_has_queued_research_conflict
#undef hybrid_production_is_evolve_target
#undef hybrid_production_has_evolution_barrier
#undef hybrid_production_should_defer_construct_order
#undef finalize_hybrid_construct_order
#undef discard_hybrid_construct_placement
#undef clear_hybrid_construct_placements
#undef retain_hybrid_research_menu_after_order
#undef register_research_station_hybrid_lists
#undef publish_research_station_hybrid_items
#undef cleanup_hybrid_cocoon
#undef cleanup_hybrid_construction

extern "C" bool __cdecl A2FO_RegisterHybridBridge(const A2FO_HybridBridge*);

namespace {
int implementation_calls = 0;
int callback_calls[12]{};
int producer_token = 0;
int target_token = 0;
int station_token = 0;
int db_token = 0;
bool arguments_ok = true;

void check(bool condition) { arguments_ok = arguments_ok && condition; }

bool conflict(void* producer, void* target) noexcept {
    ++callback_calls[0]; check(producer == &producer_token && target == &target_token); return true;
}
bool evolve(void* producer, void* target) noexcept {
    ++callback_calls[1]; check(producer == &producer_token && target == &target_token); return false;
}
bool barrier(void* producer) noexcept {
    ++callback_calls[2]; check(producer == &producer_token); return true;
}
bool defer(void* producer, void* target) noexcept {
    ++callback_calls[3]; check(producer == &producer_token && target == &target_token); return false;
}
void finalize(void* producer, void* target, bool admitted) noexcept {
    ++callback_calls[4]; check(producer == &producer_token && target == &target_token && admitted);
}
void discard(void* producer, std::uint32_t queue_id) noexcept {
    ++callback_calls[5]; check(producer == &producer_token && queue_id == 47);
}
void clear(void* producer) noexcept {
    ++callback_calls[6]; check(producer == &producer_token);
}
void retain(void* producer, void* target) noexcept {
    ++callback_calls[7]; check(producer == &producer_token && target == &target_token);
}
bool register_lists(void* station, void* db) noexcept {
    ++callback_calls[8]; check(station == &station_token && db == &db_token); return true;
}
std::size_t publish(void* station) noexcept {
    ++callback_calls[9]; check(station == &station_token); return 37;
}
void cleanup_cocoon(void* station) noexcept {
    ++callback_calls[10]; check(station == &station_token);
}
void cleanup_construction(void* station) noexcept {
    ++callback_calls[11]; check(station == &station_token);
}

int total_callbacks() {
    int count = 0;
    for (int calls : callback_calls) count += calls;
    return count;
}

bool call_all(bool registered) {
    using namespace a2fo;
    const bool conflict_result = builtin_feature_pack_client_hybrid_production_has_queued_research_conflict(&producer_token, &target_token);
    const bool evolve_result = builtin_feature_pack_client_hybrid_production_is_evolve_target(&producer_token, &target_token);
    const bool barrier_result = builtin_feature_pack_client_hybrid_production_has_evolution_barrier(&producer_token);
    const bool defer_result = builtin_feature_pack_client_hybrid_production_should_defer_construct_order(&producer_token, &target_token);
    builtin_feature_pack_client_finalize_hybrid_construct_order(&producer_token, &target_token, true);
    builtin_feature_pack_client_discard_hybrid_construct_placement(&producer_token, 47);
    builtin_feature_pack_client_clear_hybrid_construct_placements(&producer_token);
    builtin_feature_pack_client_retain_hybrid_research_menu_after_order(&producer_token, &target_token);
    const bool lists_result = builtin_feature_pack_client_register_research_station_hybrid_lists(&station_token, &db_token);
    const std::size_t publish_result = builtin_feature_pack_client_publish_research_station_hybrid_items(&station_token);
    builtin_feature_pack_client_cleanup_hybrid_cocoon(&station_token);
    builtin_feature_pack_client_cleanup_hybrid_construction(&station_token);
    return conflict_result == registered && !evolve_result &&
        barrier_result == registered && !defer_result &&
        lists_result == registered && publish_result == (registered ? 37u : 0u);
}
}  // namespace

namespace a2fo {
bool hybrid_production_has_queued_research_conflict(void*, void*) noexcept { ++implementation_calls; return false; }
bool hybrid_production_is_evolve_target(void*, void*) noexcept { ++implementation_calls; return true; }
bool hybrid_production_has_evolution_barrier(void*) noexcept { ++implementation_calls; return false; }
bool hybrid_production_should_defer_construct_order(void*, void*) noexcept { ++implementation_calls; return true; }
void finalize_hybrid_construct_order(void*, void*, bool) noexcept { ++implementation_calls; }
void discard_hybrid_construct_placement(void*, std::uint32_t) noexcept { ++implementation_calls; }
void clear_hybrid_construct_placements(void*) noexcept { ++implementation_calls; }
void retain_hybrid_research_menu_after_order(void*, void*) noexcept { ++implementation_calls; }
bool register_research_station_hybrid_lists(void*, void*) noexcept { ++implementation_calls; return false; }
std::size_t publish_research_station_hybrid_items(void*) noexcept { ++implementation_calls; return 99; }
void cleanup_hybrid_cocoon(void*) noexcept { ++implementation_calls; }
void cleanup_hybrid_construction(void*) noexcept { ++implementation_calls; }
}  // namespace a2fo

int main() {
    static_assert(sizeof(A2FO_HybridBridge) == 56, "32-bit bridge layout changed");
    if (!call_all(false) || total_callbacks() != 0 || implementation_calls != 0) return 1;

    const A2FO_HybridBridge complete{
        sizeof(A2FO_HybridBridge), A2FO_HYBRID_BRIDGE_VERSION,
        conflict, evolve, barrier, defer, finalize, discard, clear, retain,
        register_lists, publish, cleanup_cocoon, cleanup_construction};
    A2FO_HybridBridge invalid = complete;
    invalid.version++;
    if (A2FO_RegisterHybridBridge(&invalid)) return 2;
    invalid = complete;
    invalid.struct_size--;
    if (A2FO_RegisterHybridBridge(&invalid)) return 3;
    invalid = complete;
    invalid.cleanup_construction = nullptr;
    if (A2FO_RegisterHybridBridge(&invalid)) return 4;
    if (!call_all(false) || total_callbacks() != 0 || implementation_calls != 0) return 5;

    if (!A2FO_RegisterHybridBridge(&complete)) return 6;
    if (A2FO_RegisterHybridBridge(&complete)) return 7;
    if (!call_all(true) || total_callbacks() != 12 || implementation_calls != 0 || !arguments_ok) return 8;
    for (int calls : callback_calls) if (calls != 1) return 9;
    std::puts("feature-pack bridge client: all 12 callbacks isolated and dispatched");
    return 0;
}
