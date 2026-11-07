#include "../modules/A2FOFeaturePack/queue_enhancement.cpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <vector>

using namespace a2fo;

// This query must never call engine code. Fail immediately if the metric
// accidentally crosses a native boundary; the ABI bridges are tested elsewhere.
extern "C" std::uintptr_t a2fo_call_thiscall_0(void*,void*) { std::abort(); }
extern "C" std::uintptr_t a2fo_call_thiscall_1(void*,void*,std::uintptr_t) { std::abort(); }
extern "C" std::uintptr_t a2fo_call_thiscall_2(void*,void*,std::uintptr_t,std::uintptr_t) { std::abort(); }
extern "C" std::uintptr_t a2fo_call_thiscall_7(void*,void*,std::uintptr_t,std::uintptr_t,
    std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t) { std::abort(); }

namespace {

constexpr std::size_t kProducerSize = 0x300;
constexpr std::size_t kQueueItemSize = 0x10;

struct ProducerMemory {
    std::array<std::uint8_t, kProducerSize> bytes{};
};

struct QueueItemMemory {
    std::array<std::uint8_t, kQueueItemSize> bytes{};
};

void write_pointer(void* base, std::size_t offset, void* value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value,
                sizeof(value));
}

void write_u32(void* base, std::size_t offset, std::uint32_t value) {
    std::memcpy(static_cast<std::uint8_t*>(base) + offset, &value,
                sizeof(value));
}

void setup_producer(ProducerMemory& memory, std::uint32_t handle) {
    write_u32(memory.bytes.data(), kObjectHandleOffset, handle);
}

void setup_queue_item(QueueItemMemory& item, void* target_class,
                      std::uint32_t queue_id, void* next) {
    write_pointer(item.bytes.data(), 0, target_class);
    write_pointer(item.bytes.data(), kQueueItemNextOffset, next);
    write_u32(item.bytes.data(), kQueueItemIdOffset, queue_id);
}

void set_queue(ProducerMemory& producer, void* head,
               void* current_class, std::uint32_t current_id) {
    write_pointer(producer.bytes.data(), kQueueHeadOffset, head);
    write_pointer(producer.bytes.data(), kCurrentBuildClassOffset,
                  current_class);
    write_u32(producer.bytes.data(), kCurrentQueueIdOffset, current_id);
}

}  // namespace

int main() {
    InitializeCriticalSection(&g_queue_lock);
    g_queue_lock_ready = true;

    ProducerMemory first{};
    ProducerMemory second{};
    setup_producer(first, 0x1001);
    setup_producer(second, 0x1002);

    g_repeat_ready = false;
    assert(A2FO_ProducerLogicalQueueSize(first.bytes.data()) == UINT32_MAX);
    g_repeat_ready = true;
    assert(A2FO_ProducerLogicalQueueSize(nullptr) == UINT32_MAX);
    assert(A2FO_ProducerLogicalQueueSize(first.bytes.data()) == 0);

    // The active build remains represented by its physical FIFO node.
    int common_class = 0;
    std::array<QueueItemMemory, 3> physical{};
    for (std::size_t index = 0; index < physical.size(); ++index) {
        void* next = index + 1 < physical.size()
            ? physical[index + 1].bytes.data() : nullptr;
        setup_queue_item(physical[index], &common_class,
                         static_cast<std::uint32_t>(index + 10), next);
    }
    set_queue(first, physical[0].bytes.data(), &common_class, 10);
    assert(A2FO_ProducerLogicalQueueSize(first.bytes.data()) == 3);

    // A detached active node is inserted once ahead of the remaining FIFO.
    int other_class = 0;
    std::array<QueueItemMemory, 2> detached_fifo{};
    setup_queue_item(detached_fifo[0], &common_class, 21,
                     detached_fifo[1].bytes.data());
    setup_queue_item(detached_fifo[1], &other_class, 22, nullptr);
    set_queue(first, detached_fifo[0].bytes.data(), &common_class, 20);
    assert(A2FO_ProducerLogicalQueueSize(first.bytes.data()) == 3);

    // Ten native jobs plus fifteen sidecar jobs span three visible groups,
    // while the exported metric still reports all twenty-five units.
    std::array<QueueItemMemory, kQueueCapacity> ten_physical{};
    for (std::size_t index = 0; index < ten_physical.size(); ++index) {
        void* next = index + 1 < ten_physical.size()
            ? ten_physical[index + 1].bytes.data() : nullptr;
        setup_queue_item(ten_physical[index], &common_class,
                         static_cast<std::uint32_t>(30 + index), next);
    }
    set_queue(first, ten_physical[0].bytes.data(), &common_class, 30);
    g_pending_queue[0x1001] = std::vector<void*>(15, &common_class);

    const GroupedQueueView grouped = collect_grouped_queue(first.bytes.data());
    assert(grouped.count == 3);
    assert(grouped.groups[0].count == 10);
    assert(grouped.groups[1].count == 10);
    assert(grouped.groups[2].count == 5);
    assert(A2FO_ProducerLogicalQueueSize(first.bytes.data()) == 25);
    assert(A2FO_ProducerLogicalQueueSize(second.bytes.data()) == 0);

    g_pending_queue.clear();
    g_queue_lock_ready = false;
    DeleteCriticalSection(&g_queue_lock);
    std::puts("Queue workload export: unavailable, native, detached, deferred 25/3, and isolation PASS");
    return 0;
}
