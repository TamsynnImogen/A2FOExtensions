// Native callback ABI adapter; Rust owns validation, publication and dispatch.
#include "hybrid_bridge_client.hpp"
#include "hybrid_bridge_api.hpp"
#include "../../rust/queue_bridges_ffi.h"
#include <windows.h>
#include <array>
#include <cstring>
namespace {
template<class T> std::uintptr_t address(T callback) noexcept {
    static_assert(sizeof(callback) == sizeof(std::uintptr_t));
    std::uintptr_t out{}; std::memcpy(&out, &callback, sizeof(out)); return out;
}
template<class T> T function(std::uintptr_t value) noexcept {
    static_assert(sizeof(T) == sizeof(value));
    T out{}; std::memcpy(&out, &value, sizeof(out)); return out;
}
std::uintptr_t invoke(std::uintptr_t callback, std::uint32_t op, void* source,
    void* target, void* result, std::uint32_t queue, std::uint32_t value) noexcept {
    (void)source; (void)target; (void)result; (void)queue; (void)value;
    switch(op) {
    case 0: return function<decltype(A2FO_HybridBridge::has_queued_research_conflict)>(callback)(source, target);
    case 1: return function<decltype(A2FO_HybridBridge::is_evolve_target)>(callback)(source, target);
    case 2: return function<decltype(A2FO_HybridBridge::has_evolution_barrier)>(callback)(source);
    case 3: return function<decltype(A2FO_HybridBridge::should_defer_construct_order)>(callback)(source, target);
    case 4: function<decltype(A2FO_HybridBridge::finalize_construct_order)>(callback)(source, target, value != 0); return 0;
    case 5: function<decltype(A2FO_HybridBridge::discard_construct_placement)>(callback)(source, queue); return 0;
    case 6: function<decltype(A2FO_HybridBridge::clear_construct_placements)>(callback)(source); return 0;
    case 7: function<decltype(A2FO_HybridBridge::retain_research_menu_after_order)>(callback)(source, target); return 0;
    case 8: return function<decltype(A2FO_HybridBridge::register_research_station_lists)>(callback)(source, target);
    case 9: return function<decltype(A2FO_HybridBridge::publish_research_station_items)>(callback)(source);
    case 10: function<decltype(A2FO_HybridBridge::cleanup_cocoon)>(callback)(source); return 0;
    case 11: function<decltype(A2FO_HybridBridge::cleanup_construction)>(callback)(source); return 0;
    default: return 0;
    }
}
}
extern "C" __declspec(dllexport) bool __cdecl A2FO_RegisterHybridBridge(const A2FO_HybridBridge* value) {
    if (!value || !a2fo_rs_queue_bridge_valid_header(1, value->version, value->struct_size)) return false;
    const std::array<std::uintptr_t,12> callbacks{{
        address(value->has_queued_research_conflict),
        address(value->is_evolve_target),
        address(value->has_evolution_barrier),
        address(value->should_defer_construct_order),
        address(value->finalize_construct_order),
        address(value->discard_construct_placement),
        address(value->clear_construct_placements),
        address(value->retain_research_menu_after_order),
        address(value->register_research_station_lists),
        address(value->publish_research_station_items),
        address(value->cleanup_cocoon),
        address(value->cleanup_construction)
    }};
    return a2fo_rs_queue_bridge_register(1,value->version,value->struct_size,callbacks.data()) != 0;
}
namespace a2fo {
bool hybrid_production_has_queued_research_conflict(void* producer, void* target_class) noexcept {
    return a2fo_rs_queue_bridge_dispatch(1,0,&invoke,producer,target_class,nullptr,0,0) != 0;
}
bool hybrid_production_is_evolve_target(void* producer, void* target_class) noexcept {
    return a2fo_rs_queue_bridge_dispatch(1,1,&invoke,producer,target_class,nullptr,0,0) != 0;
}
bool hybrid_production_has_evolution_barrier(void* producer) noexcept {
    return a2fo_rs_queue_bridge_dispatch(1,2,&invoke,producer,nullptr,nullptr,0,0) != 0;
}
bool hybrid_production_should_defer_construct_order(void* producer, void* target_class) noexcept {
    return a2fo_rs_queue_bridge_dispatch(1,3,&invoke,producer,target_class,nullptr,0,0) != 0;
}
void finalize_hybrid_construct_order(void* producer, void* target_class, bool admitted) noexcept {
    a2fo_rs_queue_bridge_dispatch(1,4,&invoke,producer,target_class,nullptr,0,admitted ? 1u : 0u);
}
void discard_hybrid_construct_placement(void* producer, std::uint32_t queue_id) noexcept {
    a2fo_rs_queue_bridge_dispatch(1,5,&invoke,producer,nullptr,nullptr,queue_id,0);
}
void clear_hybrid_construct_placements(void* producer) noexcept {
    a2fo_rs_queue_bridge_dispatch(1,6,&invoke,producer,nullptr,nullptr,0,0);
}
void retain_hybrid_research_menu_after_order(void* producer, void* target_class) noexcept {
    a2fo_rs_queue_bridge_dispatch(1,7,&invoke,producer,target_class,nullptr,0,0);
}
bool register_research_station_hybrid_lists(void* station_class, void* parameter_db) noexcept {
    return a2fo_rs_queue_bridge_dispatch(1,8,&invoke,station_class,parameter_db,nullptr,0,0) != 0;
}
std::size_t publish_research_station_hybrid_items(void* station_class) noexcept {
    return a2fo_rs_queue_bridge_dispatch(1,9,&invoke,station_class,nullptr,nullptr,0,0);
}
void cleanup_hybrid_cocoon(void* station) noexcept {
    a2fo_rs_queue_bridge_dispatch(1,10,&invoke,station,nullptr,nullptr,0,0);
}
void cleanup_hybrid_construction(void* station) noexcept {
    a2fo_rs_queue_bridge_dispatch(1,11,&invoke,station,nullptr,nullptr,0,0);
}
}
