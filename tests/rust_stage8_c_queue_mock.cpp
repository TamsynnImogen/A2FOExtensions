// Include the complete matching frozen/current translation unit.
#include A2FO_STAGE8_QUEUE_SOURCE

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <vector>

using namespace a2fo;

// Real game queue layouts are read by the included translation unit. These
// bridge stubs provide only deterministic native engine dependencies.
std::array<std::array<std::uint8_t,16>,128> g_native_items{};
std::uint32_t g_native_next_id=300;
std::size_t g_native_used=0;
bool g_reject_admit=false;
bool g_mutate_on_log=false;
void* g_mutation_producer=nullptr;

void native_push(void* producer,void* target){
    if(g_native_used>=g_native_items.size())std::abort();
    auto* node=g_native_items[g_native_used++].data();
    std::memset(node,0,16);
    std::memcpy(node,&target,sizeof(target));
    const auto id=g_native_next_id++;
    std::memcpy(node+kQueueItemIdOffset,&id,sizeof(id));
    auto* base=static_cast<std::uint8_t*>(producer);
    void* head=nullptr;
    std::memcpy(&head,base+kQueueHeadOffset,sizeof(head));
    if(!head)std::memcpy(base+kQueueHeadOffset,&node,sizeof(node));
    else{
        void* item=head;
        for(std::uint32_t visited=0;visited<128;++visited){
            void* next=nullptr;
            std::memcpy(&next,static_cast<std::uint8_t*>(item)+
                kQueueItemNextOffset,sizeof(next));
            if(!next){std::memcpy(static_cast<std::uint8_t*>(item)+
                kQueueItemNextOffset,&node,sizeof(node));break;}
            item=next;
        }
    }
    std::uint32_t count=0;
    std::memcpy(&count,base+kQueueCountOffset,sizeof(count));
    ++count;
    std::memcpy(base+kQueueCountOffset,&count,sizeof(count));
    std::printf("native_push id %lu count %lu\n",
        static_cast<unsigned long>(id),static_cast<unsigned long>(count));
}

extern "C" std::uintptr_t a2fo_call_thiscall_0(void* function,void*){
    if(function==g_fo_finish_hook.gateway){std::puts("native_finish");return 0x1234;}
    if(function==g_fo_cancel_hook.gateway){std::puts("native_cancel");return 0x5678;}
    if(function==g_producer_dtor_hook.gateway){std::puts("native_dtor");return 0x9abc;}
    if(function==g_fo_clear_hook.gateway){std::puts("native_clear");return 1;}
    std::abort();
}
extern "C" std::uintptr_t a2fo_call_thiscall_1(void* function,void* self,
    std::uintptr_t argument){
    if(function==g_fo_command_push_hook.gateway||
       function==at(g_fleet_ops,kFoProducerPushCheckedRva)){
        native_push(self,reinterpret_cast<void*>(argument));return 1;
    }
    if(function==g_producer_simulate_hook.gateway){
        std::printf("native_simulate %lu\n",static_cast<unsigned long>(argument));
        return 1;
    }
    if(function==g_fo_act_delete_hook.gateway){
        std::printf("native_delete %lu\n",static_cast<unsigned long>(argument));
        return 1;
    }
    if(function==g_producer_save_hook.gateway){
        auto* base=static_cast<std::uint8_t*>(self);
        std::uint32_t current=0,next=0;
        std::memcpy(&current,base+kCurrentQueueIdOffset,sizeof(current));
        std::memcpy(&next,base+kNextQueueIdOffset,sizeof(next));
        std::printf("native_save ids %lu %lu\n",
            static_cast<unsigned long>(current),
            static_cast<unsigned long>(next));
        return 0xbeef;
    }
    if(function==g_producer_load_hook.gateway){
        std::puts("native_load");return 1;
    }
    std::abort();
}
extern "C" std::uintptr_t a2fo_call_thiscall_2(void* function,void*,
    std::uintptr_t command,std::uintptr_t){
    if(function==g_game_object_queue_class_command_hook.gateway||
       function==g_game_object_dequeue_class_command_hook.gateway){
        std::printf("native_command %lu\n",static_cast<unsigned long>(command));
        return 1;
    }
    std::abort();
}
extern "C" std::uintptr_t a2fo_call_thiscall_7(void*,void*,std::uintptr_t,
    std::uintptr_t,std::uintptr_t,std::uintptr_t,std::uintptr_t,
    std::uintptr_t,std::uintptr_t){std::abort();}
namespace a2fo {
bool hybrid_production_has_queued_research_conflict(void*,void*) noexcept {return false;}
bool hybrid_production_is_evolve_target(void*,void*) noexcept {return false;}
bool hybrid_production_has_evolution_barrier(void*) noexcept {return false;}
bool hybrid_production_should_defer_construct_order(void*,void*) noexcept {return false;}
void finalize_hybrid_construct_order(void*,void*,bool) noexcept {}
void discard_hybrid_construct_placement(void*,std::uint32_t) noexcept {}
void clear_hybrid_construct_placements(void*) noexcept {}
void retain_hybrid_research_menu_after_order(void*,void*) noexcept {}
bool consume_refit_synchronized_command(void*,void*) noexcept {return false;}
bool cancel_refit_synchronized_command(void*) noexcept {return false;}
bool refit_has_waiting_job(void*) noexcept {return false;}
void notify_refit_job_finished(void*,std::uint32_t id,void*,void*) noexcept {
    std::printf("refit_finished %lu\n",static_cast<unsigned long>(id));
}
void notify_refit_job_removed(void*,std::uint32_t id,void*,
    std::uint32_t reason) noexcept {
    std::printf("refit_removed %lu %lu\n",static_cast<unsigned long>(id),
        static_cast<unsigned long>(reason));
}
}

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

void A2FO_CALL fixture_log(const char*,const char* message){
    std::printf("log %s\n",message);
    if(g_mutate_on_log&&g_mutation_producer&&
       std::strcmp(message,
           "First synchronized Producer build command reached")==0){
        write_u32(g_mutation_producer,kQueueCountOffset,10);
        g_mutate_on_log=false;
    }
}

bool A2FO_CALL fixture_event(const A2FO_ProducerEvent* event){
    std::printf("event %lu\n",static_cast<unsigned long>(event->kind));
    return !(g_reject_admit&&event->kind==A2FO_PRODUCER_EVENT_ADMIT);
}

}  // namespace

int main() {
    std::printf("grouped_log %d\n",g_logged_grouped_queue_ui);
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
    std::printf("grouped %lu\n",static_cast<unsigned long>(grouped.count));
    for(std::uint32_t index=0;index<grouped.count;++index){
        const auto& group=grouped.groups[index];
        std::printf("group %lu count %lu physical %lu pending %lu first %lu last %lu pending_range %lu %lu infinite %u\n",
            static_cast<unsigned long>(index),
            static_cast<unsigned long>(group.count),
            static_cast<unsigned long>(group.physical_count),
            static_cast<unsigned long>(group.pending_count),
            static_cast<unsigned long>(group.first_queue_id),
            static_cast<unsigned long>(group.last_queue_id),
            static_cast<unsigned long>(group.first_pending_index),
            static_cast<unsigned long>(group.last_pending_index),
            group.infinite);
    }
    assert(grouped.count == 3);
    assert(grouped.groups[0].count == 10);
    assert(grouped.groups[1].count == 10);
    assert(grouped.groups[2].count == 5);
    assert(A2FO_ProducerLogicalQueueSize(first.bytes.data()) == 25);
    assert(A2FO_ProducerLogicalQueueSize(second.bytes.data()) == 0);

    A2FO_ModuleApi api{};
    api.struct_size=sizeof(api);
    api.capabilities=A2FO_CAP_PRODUCER_EVENTS;
    api.log=&fixture_log;
    api.dispatch_producer_event=&fixture_event;
    g_api=&api;
    g_fleet_ops=reinterpret_cast<HMODULE>(0x100000);
    g_fo_command_push_hook.gateway=reinterpret_cast<void*>(0x101);
    g_fo_finish_hook.gateway=reinterpret_cast<void*>(0x102);
    g_fo_cancel_hook.gateway=reinterpret_cast<void*>(0x103);
    g_fo_clear_hook.gateway=reinterpret_cast<void*>(0x104);
    g_fo_act_delete_hook.gateway=reinterpret_cast<void*>(0x105);
    g_producer_dtor_hook.gateway=reinterpret_cast<void*>(0x106);
    g_producer_simulate_hook.gateway=reinterpret_cast<void*>(0x107);
    g_producer_save_hook.gateway=reinterpret_cast<void*>(0x10a);
    g_producer_load_hook.gateway=reinterpret_cast<void*>(0x10b);
    g_game_object_queue_class_command_hook.gateway=
        reinterpret_cast<void*>(0x108);
    g_game_object_dequeue_class_command_hook.gateway=
        reinterpret_cast<void*>(0x109);

    g_pending_queue.clear();
    set_queue(first,nullptr,nullptr,0);
    write_u32(first.bytes.data(),kQueueCountOffset,0);
    set_queue(second,nullptr,nullptr,0);
    write_u32(second.bytes.data(),kQueueCountOffset,0);
    std::puts("case refit push");
    assert(A2FO_ProducerPushRefit(first.bytes.data(),&common_class)==300);
    g_reject_admit=true;
    assert(A2FO_ProducerPushRefit(first.bytes.data(),&common_class)==0);
    g_reject_admit=false;
    std::printf("refit count %lu\n",
        static_cast<unsigned long>(native_queue_count(first.bytes.data())));

    std::puts("case synchronized push");
    g_mutation_producer=first.bytes.data();
    g_mutate_on_log=true;
    producer_command_push_hook(first.bytes.data(),nullptr,&other_class);
    assert(!g_mutate_on_log);
    std::printf("push count %lu\n",
        static_cast<unsigned long>(native_queue_count(first.bytes.data())));
    g_reject_admit=true;
    producer_command_push_hook(first.bytes.data(),nullptr,&other_class);
    g_reject_admit=false;
    std::printf("rejected count %lu\n",
        static_cast<unsigned long>(native_queue_count(first.bytes.data())));
    std::puts("case inactive continuous entry");
    g_continuous[0x1001]={false,&common_class,0,7};
    producer_command_push_hook(first.bytes.data(),nullptr,&other_class);
    const auto inactive_found=g_continuous.find(0x1001);
    std::printf("inactive entry retained %u ticks %lu\n",
        inactive_found!=g_continuous.end(),
        static_cast<unsigned long>(inactive_found==g_continuous.end()?0:
            inactive_found->second.paused_retry_ticks));

    std::puts("case completion and cancellation");
    assert(producer_finish_hook(first.bytes.data(),nullptr)==0x1234);
    assert(producer_cancel_hook(first.bytes.data(),nullptr)==0x5678);

    std::puts("case marker receiver");
    game_object_dequeue_class_command_hook(first.bytes.data(),nullptr,
        A2FO_REFIT_CLASS_COMMAND,&common_class);
    game_object_dequeue_class_command_hook(first.bytes.data(),nullptr,
        A2FO_REFIT_CANCEL_CLASS_COMMAND,&common_class);
    game_object_dequeue_class_command_hook(first.bytes.data(),nullptr,
        0x55,&common_class);
    game_object_dequeue_class_command_hook(first.bytes.data(),nullptr,
        kExtendedQueueMarkerCommand,&other_class);
    game_object_dequeue_class_command_hook(first.bytes.data(),nullptr,
        kContinuousMarkerCommand,&other_class);
    game_object_dequeue_class_command_hook(first.bytes.data(),nullptr,
        kExtendedQueueMarkerCommand,&other_class);

    std::puts("case sender control marker");
    std::vector<std::uint8_t> fake_executable(0x370000);
    int pressed=1;
    write_pointer(fake_executable.data(),kCommandControlPointerRva,&pressed);
    write_pointer(fake_executable.data(),kCommandAltPointerRva,&pressed);
    g_armada=reinterpret_cast<HMODULE>(fake_executable.data());
    game_object_queue_class_command_hook(first.bytes.data(),nullptr,
        kBuildCommand,&other_class);

    std::puts("case save load and clear");
    write_u32(first.bytes.data(),kCurrentQueueIdOffset,44);
    write_u32(first.bytes.data(),kNextQueueIdOffset,45);
    g_continuous[0x1001]={true,&other_class,73,0};
    assert(producer_save_hook(first.bytes.data(),nullptr,nullptr)==0xbeef);
    std::printf("restored ids %lu %lu\n",
        static_cast<unsigned long>(current_queue_id(first.bytes.data())),
        static_cast<unsigned long>(*reinterpret_cast<std::uint32_t*>(
            first.bytes.data()+kNextQueueIdOffset)));
    assert(producer_load_hook(first.bytes.data(),nullptr,nullptr)==1);
    producer_clear_hook(first.bytes.data(),nullptr);

    std::puts("case simulate refill");
    set_queue(second,nullptr,nullptr,0);
    write_u32(second.bytes.data(),kQueueCountOffset,0);
    g_continuous[0x1002]={true,&other_class,0,29};
    producer_simulate_hook(second.bytes.data(),nullptr,5);
    std::printf("simulate count %lu\n",
        static_cast<unsigned long>(native_queue_count(second.bytes.data())));
    assert(producer_dtor_hook(second.bytes.data(),nullptr)==0x9abc);

    g_api=nullptr;
    g_armada=nullptr;
    g_fleet_ops=nullptr;

    g_pending_queue.clear();
    g_continuous.clear();
    g_queue_lock_ready = false;
    DeleteCriticalSection(&g_queue_lock);
    std::puts("case initialization signature failure");
    api.install_inline_hook=+[](void*,void*,std::size_t,
        const std::uint8_t*,A2FO_InlineHook*)->bool{
        std::abort();
    };
    g_api=&api;
    g_armada=reinterpret_cast<HMODULE>(fake_executable.data());
    g_fleet_ops=reinterpret_cast<HMODULE>(0x100000);
    assert(!initialize_queue_enhancements(&api,g_armada,g_fleet_ops));
    g_queue_lock_ready=false;
    DeleteCriticalSection(&g_queue_lock);
    g_api=nullptr;
    g_armada=nullptr;
    g_fleet_ops=nullptr;
    std::puts("Queue workload export: unavailable, native, detached, deferred 25/3, and isolation PASS");
    return 0;
}
