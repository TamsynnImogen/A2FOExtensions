// Exercise FeaturePack's actual cancellation export with a grouped queue.
#include "../modules/A2FOFeaturePack/queue_enhancement.cpp"
#include <cassert>
#include <iostream>

namespace {
std::uint32_t deleted_id = 0;
unsigned refunds = 0;
template<class T> void put(void* object, std::size_t offset, T value) {
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
}
void __attribute__((fastcall)) native_delete(void* producer, void*, std::uint32_t id) {
    deleted_id = id;
    // The exact first node is removed, leaving the identical queued peer.
    auto* head = *reinterpret_cast<std::uint8_t**>(static_cast<std::uint8_t*>(producer) + 0x270);
    assert(*reinterpret_cast<std::uint32_t*>(head + 0x0c) == id);
    put(producer, 0x270, *reinterpret_cast<void**>(head + 8));
    put<std::uint32_t>(producer, 0x274, 1);
}
bool A2FO_CALL refund_event(const A2FO_ProducerEvent* event) {
    assert(event->kind == A2FO_PRODUCER_EVENT_DELETED);
    ++refunds;
    return true;
}
}

int main() {
    using namespace a2fo;
    std::array<std::uint8_t, 0x300> producer{};
    std::array<std::uint8_t, 0x500> yard_class{};
    std::array<std::uint8_t, 0x100> config{};
    std::array<std::uint8_t, 0x20> first{}, second{};
    int target_class;
    put(producer.data(), kObjectClassOffset, yard_class.data());
    put(yard_class.data(), kProducerConfigOffset, config.data());
    put<std::uint8_t>(config.data(), kChargeAtQueueOffset, 1);
    put(producer.data(), kQueueHeadOffset, first.data());
    put<std::uint32_t>(producer.data(), kQueueCountOffset, 2);
    put(first.data(), 0, &target_class);
    put(second.data(), 0, &target_class);
    put(first.data(), 8, second.data());
    put<std::uint32_t>(first.data(), 0x0c, 77);
    put<std::uint32_t>(second.data(), 0x0c, 78);
    InitializeCriticalSection(&g_queue_lock);
    g_queue_lock_ready = g_repeat_ready = g_grouped_queue_ui_ready = true;
    g_grouped_queue_ui_context = {true, producer.data(), 0};
    g_fo_act_delete_hook.gateway = reinterpret_cast<void*>(&native_delete);
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api);
    api.capabilities = A2FO_CAP_PRODUCER_EVENTS;
    api.dispatch_producer_event = &refund_event;
    g_api = &api;
    assert(A2FO_ProducerCancelQueuedJob(producer.data(), 77));
    assert(deleted_id == 77 && refunds == 1);
    assert(queued_target_class(producer.data(), 78) == &target_class);
    assert(!A2FO_ProducerCancelQueuedJob(producer.data(), 77));
    assert(!A2FO_ProducerCancelQueuedJob(producer.data(), 0));
    assert(!A2FO_ProducerCancelQueuedJob(nullptr, 78));
    g_repeat_ready = false;
    assert(!A2FO_ProducerCancelQueuedJob(producer.data(), 78));
    assert(refunds == 1 && deleted_id == 77);
    DeleteCriticalSection(&g_queue_lock);
    std::cout << "PASS exact paid job cancellation bypasses grouped slot mapping and emits one refund; stale IDs rejected\n";
}
