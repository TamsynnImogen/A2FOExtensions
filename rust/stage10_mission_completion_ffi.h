#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
struct A2foStage10MissionFields {
    std::size_t key_begin, key_end, value_begin, value_end;
    bool valid;
};
struct A2foStage10MissionFlags {
    bool unlocked, replace_launch_filename;
};
bool a2fo_rs_stage10_mission_label_header(const std::uint8_t*, std::size_t,
    std::uint32_t);
A2foStage10MissionFields a2fo_rs_stage10_mission_label_fields(
    const std::uint8_t*, std::size_t, std::size_t);
bool a2fo_rs_stage10_mission_skip_label_key(const std::uint8_t*, std::size_t);
std::int32_t a2fo_rs_stage10_mission_campaign_label_index(std::int32_t);
bool a2fo_rs_stage10_mission_native_campaign(std::int32_t, std::int32_t);
bool a2fo_rs_stage10_mission_native_slot(bool, std::int32_t, std::int32_t);
A2foStage10MissionFlags a2fo_rs_stage10_mission_flags(
    bool, bool, bool, bool);
}
