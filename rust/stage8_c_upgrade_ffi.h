#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
using A2foStage8UpgradeStep=std::uintptr_t (*)(void*,std::uint32_t,
    std::size_t);
struct A2foStage8UpgradeSpan{std::size_t start,length;};
std::uint32_t a2fo_rs_c_upgrade_join_mode(const std::uint8_t*,std::size_t,
    const std::uint8_t*,std::size_t);
bool a2fo_rs_c_upgrade_read_file(void*,A2foStage8UpgradeStep,
    const std::int64_t*,std::int64_t,const std::uint8_t*);
bool a2fo_rs_c_upgrade_trim_name(const std::uint8_t*,std::size_t,
    A2foStage8UpgradeSpan*);
void a2fo_rs_c_upgrade_load_config(void*,A2foStage8UpgradeStep,
    const std::uint8_t*);
std::uintptr_t a2fo_rs_c_upgrade_same_type(void*,std::uintptr_t,
    std::uintptr_t,A2foStage8UpgradeStep,const std::uint8_t*);
std::uintptr_t a2fo_rs_c_upgrade_can_build(void*,std::uintptr_t,
    std::uintptr_t,bool,A2foStage8UpgradeStep,const std::uint8_t*);
void a2fo_rs_c_upgrade_pod_class(void*,std::uintptr_t,
    A2foStage8UpgradeStep,const std::uint8_t*);
void a2fo_rs_c_upgrade_station_class(void*,A2foStage8UpgradeStep,
    const std::uint8_t*);
void a2fo_rs_c_upgrade_station_constructor(void*,A2foStage8UpgradeStep,
    const std::uint8_t*);
std::uintptr_t a2fo_rs_c_upgrade_station_destructor(void*,
    A2foStage8UpgradeStep,const std::uint8_t*);
void a2fo_rs_c_upgrade_pod_attach(void*,A2foStage8UpgradeStep,
    const std::uint8_t*);
void a2fo_rs_c_upgrade_tier_lists(void*,std::uintptr_t,std::uintptr_t,
    A2foStage8UpgradeStep,const std::uint8_t*);
void a2fo_rs_c_upgrade_refresh_station(void*,std::uintptr_t,bool,
    A2foStage8UpgradeStep,const std::uint8_t*);
void a2fo_rs_c_upgrade_recompute(void*,std::uintptr_t,std::uint32_t,bool,
    A2foStage8UpgradeStep,const std::uint8_t*);
void a2fo_rs_c_upgrade_attach_pod(void*,std::uintptr_t,bool,
    A2foStage8UpgradeStep,const std::uint8_t*);
bool a2fo_rs_c_upgrade_apply_items(void*,std::uintptr_t,bool,std::size_t,
    A2foStage8UpgradeStep,const std::uint8_t*);
void a2fo_rs_c_upgrade_init_station(void*,std::uintptr_t,std::uintptr_t,bool,
    A2foStage8UpgradeStep,const std::uint8_t*);
std::uintptr_t a2fo_rs_c_upgrade_detach_pod(void*,std::uintptr_t,bool,
    A2foStage8UpgradeStep,const std::uint8_t*);
bool a2fo_rs_c_upgrade_initialize(void*,A2foStage8UpgradeStep,
    const std::uint8_t*);
std::uintptr_t a2fo_rs_c_upgrade_class_dtor(void*,std::uintptr_t,bool,
    A2foStage8UpgradeStep,const std::uint8_t*);
}
