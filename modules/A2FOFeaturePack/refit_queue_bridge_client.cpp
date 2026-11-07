// Native callback ABI adapter; Rust owns validation, publication and dispatch.
#include "refit_queue_bridge_client.hpp"
#include "refit_queue_bridge_api.hpp"
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
    case 0: return function<decltype(A2FO_RefitQueueBridge::consume_synchronized_command)>(callback)(source, target);
    case 1: return function<decltype(A2FO_RefitQueueBridge::cancel_synchronized_command)>(callback)(source);
    case 2: return function<decltype(A2FO_RefitQueueBridge::has_waiting_job)>(callback)(source);
    case 3: function<decltype(A2FO_RefitQueueBridge::job_finished)>(callback)(source, queue, target, result); return 0;
    case 4: function<decltype(A2FO_RefitQueueBridge::job_removed)>(callback)(source, queue, target, value); return 0;
    default: return 0;
    }
}
}
extern "C" __declspec(dllexport) bool __cdecl A2FO_RegisterRefitQueueBridge(const A2FO_RefitQueueBridge* value) {
    if (!value || !a2fo_rs_queue_bridge_valid_header(0, value->version, value->struct_size)) return false;
    const std::array<std::uintptr_t,5> callbacks{{
        address(value->consume_synchronized_command),
        address(value->cancel_synchronized_command),
        address(value->has_waiting_job),
        address(value->job_finished),
        address(value->job_removed)
    }};
    return a2fo_rs_queue_bridge_register(0,value->version,value->struct_size,callbacks.data()) != 0;
}
namespace a2fo {
bool consume_refit_synchronized_command(void* source, void* target_class) noexcept {
    return a2fo_rs_queue_bridge_dispatch(0,0,&invoke,source,target_class,nullptr,0,0) != 0;
}
bool cancel_refit_synchronized_command(void* source) noexcept {
    return a2fo_rs_queue_bridge_dispatch(0,1,&invoke,source,nullptr,nullptr,0,0) != 0;
}
bool refit_has_waiting_job(void* producer) noexcept {
    return a2fo_rs_queue_bridge_dispatch(0,2,&invoke,producer,nullptr,nullptr,0,0) != 0;
}
void notify_refit_job_finished(void* producer, std::uint32_t queue_id, void* target_class, void* result) noexcept {
    a2fo_rs_queue_bridge_dispatch(0,3,&invoke,producer,target_class,result,queue_id,0);
}
void notify_refit_job_removed(void* producer, std::uint32_t queue_id, void* target_class, std::uint32_t removal_kind) noexcept {
    a2fo_rs_queue_bridge_dispatch(0,4,&invoke,producer,target_class,nullptr,queue_id,removal_kind);
}
}
