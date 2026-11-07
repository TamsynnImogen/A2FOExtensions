#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
struct A2foStage8QueueGroup {
    std::uintptr_t target;
    std::uint32_t count;
    std::uint32_t physical_count;
    std::uint32_t pending_count;
    std::uint32_t first_queue_id;
    std::uint32_t last_queue_id;
    std::uint32_t first_pending_index;
    std::uint32_t last_pending_index;
    bool infinite;
};
struct A2foStage8QueueView {
    A2foStage8QueueGroup groups[10];
    std::uint32_t count;
};
struct A2foStage8QueueRect {
    std::int32_t left,top,right,bottom;
};
using A2foStage8QueueStep = std::uintptr_t (*)(void*,std::uint32_t,
    std::size_t);
bool a2fo_rs_c_queue_collect(void*,A2foStage8QueueStep,
    std::uint32_t,std::uint32_t,std::uint32_t,A2foStage8QueueView*);
bool a2fo_rs_c_queue_visual_slot(void*,A2foStage8QueueStep,
    std::uint32_t,std::uint32_t,std::uint32_t*);
std::uint32_t a2fo_rs_c_queue_units(void*,A2foStage8QueueStep);
bool a2fo_rs_c_queue_room(void*,std::uintptr_t,A2foStage8QueueStep);
bool a2fo_rs_c_queue_continuous_room(void*,std::uintptr_t,
    A2foStage8QueueStep);
std::uint32_t a2fo_rs_c_queue_fill(void*,std::uintptr_t,std::uintptr_t,
    A2foStage8QueueStep);
bool a2fo_rs_c_queue_push_once(void*,std::uintptr_t,std::uintptr_t,bool,bool,
    A2foStage8QueueStep);
bool a2fo_rs_c_queue_logical_one(void*,std::uintptr_t,std::uintptr_t,bool,
    A2foStage8QueueStep);
void a2fo_rs_c_queue_materialize(void*,std::uintptr_t,A2foStage8QueueStep);
void a2fo_rs_c_queue_refill(void*,std::uintptr_t,bool,A2foStage8QueueStep);
void a2fo_rs_c_queue_trim_group(void*,std::uintptr_t,std::uint32_t,
    A2foStage8QueueStep);
void a2fo_rs_c_queue_act_delete(void*,A2foStage8QueueStep);
bool a2fo_rs_c_queue_clear(void*,std::uintptr_t,A2foStage8QueueStep);
void a2fo_rs_c_queue_reconstruct_ids(void*,A2foStage8QueueStep);
std::uintptr_t a2fo_rs_c_queue_save(void*,std::uintptr_t,bool,
    A2foStage8QueueStep,const std::uint8_t*);
std::uintptr_t a2fo_rs_c_queue_load(void*,std::uintptr_t,bool,
    A2foStage8QueueStep,const std::uint8_t*);
bool a2fo_rs_c_queue_install_hooks(void*,A2foStage8QueueStep);
bool a2fo_rs_c_queue_initialize(void*,A2foStage8QueueStep);
bool a2fo_rs_c_queue_icon_context(void*,std::uintptr_t,std::uintptr_t,
    std::uintptr_t,std::uintptr_t,A2foStage8QueueStep);
std::uintptr_t a2fo_rs_c_queue_text_component(void*,std::uintptr_t,
    A2foStage8QueueStep);
bool a2fo_rs_c_queue_draw_count(void*,std::uint32_t,bool,
    A2foStage8QueueRect*,A2foStage8QueueStep);
bool a2fo_rs_c_queue_export_room(void*,std::uintptr_t,bool,
    A2foStage8QueueStep);
std::uint32_t a2fo_rs_c_queue_export_size(void*,std::uintptr_t,bool,
    A2foStage8QueueStep);
bool a2fo_rs_c_queue_export_can(void*,std::uintptr_t,std::uintptr_t,bool,
    A2foStage8QueueStep);
bool a2fo_rs_c_queue_export_overflow(void*,std::uintptr_t,std::uintptr_t,bool,
    A2foStage8QueueStep);
bool a2fo_rs_c_queue_export_cancel(void*,std::uintptr_t,std::uint32_t,bool,
    A2foStage8QueueStep);
std::uint32_t a2fo_rs_c_queue_push_refit(void*,std::uintptr_t,
    std::uintptr_t,bool,A2foStage8QueueStep,const std::uint8_t*);
std::uintptr_t a2fo_rs_c_queue_finish(void*,std::uintptr_t,
    A2foStage8QueueStep,const std::uint8_t*);
std::uintptr_t a2fo_rs_c_queue_cancel(void*,std::uintptr_t,
    A2foStage8QueueStep,const std::uint8_t*);
std::uintptr_t a2fo_rs_c_queue_dtor(void*,std::uintptr_t,
    A2foStage8QueueStep,const std::uint8_t*);
void a2fo_rs_c_queue_delete_native(void*,std::uintptr_t,std::uint32_t,
    A2foStage8QueueStep,const std::uint8_t*);
void a2fo_rs_c_queue_simulate(void*,std::uintptr_t,bool,
    A2foStage8QueueStep,const std::uint8_t*);
void a2fo_rs_c_queue_dequeue_command(void*,std::uintptr_t,std::uint32_t,
    std::uintptr_t,bool,A2foStage8QueueStep,const std::uint8_t*);
void a2fo_rs_c_queue_queue_command(void*,std::uint32_t,std::uintptr_t,bool,
    A2foStage8QueueStep,const std::uint8_t*);
void a2fo_rs_c_queue_producer_push(void*,std::uintptr_t,std::uintptr_t,bool,
    A2foStage8QueueStep,const std::uint8_t*);
bool a2fo_rs_c_queue_append_pending(void*,std::uintptr_t,std::uintptr_t,bool,
    A2foStage8QueueStep);
bool a2fo_rs_c_queue_erase_pending(void*,std::uintptr_t,std::uint32_t,bool,
    A2foStage8QueueStep,const std::uint8_t*);
std::uintptr_t a2fo_rs_c_queue_find_target(void*,std::uintptr_t,
    std::uint32_t,A2foStage8QueueStep);
void a2fo_rs_c_queue_snapshot_pending(void*,std::uintptr_t,bool,
    A2foStage8QueueStep,const std::uint8_t*);
void a2fo_rs_c_queue_icon_render(void*,A2foStage8QueueStep);
void a2fo_rs_c_queue_icon_simulate(void*,A2foStage8QueueStep);
void a2fo_rs_c_queue_refresh_buttons(void*,std::uintptr_t,
    A2foStage8QueueStep);
bool a2fo_rs_c_queue_install_ui(void*,A2foStage8QueueStep);
}
