#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
struct A2foStage8MissionSpan {std::size_t begin,end;};
struct A2foStage8MissionTrim {std::size_t begin,end;bool quoted;};
using A2foStage8MissionByteClass = bool (*)(std::uint8_t,std::uint32_t);
A2foStage8MissionTrim a2fo_rs_c_mission_trim(const std::uint8_t*,std::size_t,
    A2foStage8MissionByteClass);
using A2foStage8MissionLower = std::uint8_t (*)(std::uint8_t);
void a2fo_rs_c_mission_lower(std::uint8_t*,std::size_t,A2foStage8MissionLower);
std::uint32_t a2fo_rs_c_mission_join_action(const std::uint8_t*,std::size_t,
    const std::uint8_t*,std::size_t);
using A2foStage8MissionEmit = bool (*)(void*,std::uint8_t);
bool a2fo_rs_c_mission_decode_escapes(const std::uint8_t*,std::size_t,
    void*,A2foStage8MissionEmit);
A2foStage8MissionSpan a2fo_rs_c_mission_file_stem(const std::uint8_t*,std::size_t);
bool a2fo_rs_c_mission_indexed_key(const std::uint8_t*,std::size_t,
    const std::uint8_t*,std::size_t,std::uint32_t,A2foStage8MissionByteClass,
    std::int32_t*,std::size_t*);
std::int32_t a2fo_rs_c_mission_bool_token(const std::uint8_t*,std::size_t);
bool a2fo_rs_c_mission_integer_admit(std::int64_t,std::int32_t,std::int32_t);
std::size_t a2fo_rs_c_mission_label_basename(std::uint8_t*,std::size_t);
bool a2fo_rs_c_mission_humanize(const std::uint8_t*,std::size_t,
    A2foStage8MissionByteClass,void*,A2foStage8MissionEmit);
struct A2foStage8MissionStock {std::uint32_t kind;std::size_t begin;};
A2foStage8MissionStock a2fo_rs_c_mission_humanize_stock(
    const std::uint8_t*,const std::uint8_t*,std::size_t);
using A2foStage8MissionStep = std::int64_t (*)(void*,std::uint32_t,std::size_t);
void a2fo_rs_c_mission_start(void*,A2foStage8MissionStep);
bool a2fo_rs_c_mission_init(void*,A2foStage8MissionStep);
bool a2fo_rs_c_mission_resolve_asset(void*,std::size_t,A2foStage8MissionStep);
bool a2fo_rs_c_mission_resolve_thumbnail(void*,A2foStage8MissionStep);
struct A2foStage8MissionLine {const std::uint8_t* data;std::size_t size;};
using A2foStage8MissionNext = std::int32_t (*)(void*,A2foStage8MissionLine*);
bool a2fo_rs_c_mission_parse(void*,std::uint32_t,A2foStage8MissionNext,
    A2foStage8MissionStep);
bool a2fo_rs_c_mission_collect_indices(void*,const std::uint8_t*,std::size_t,
    std::uint32_t,A2foStage8MissionByteClass,A2foStage8MissionNext,
    A2foStage8MissionStep);
struct A2foStage8MissionCatalogPlan {
    std::size_t configured_count;
    std::int32_t configured_max;
    std::uint32_t native_campaigns;
    std::uint32_t native_missions;
    std::size_t maximum_filename;
    bool request_ini;
    bool filenames_valid;
};
bool a2fo_rs_c_mission_catalog(void*,A2foStage8MissionCatalogPlan,
    A2foStage8MissionStep);
bool a2fo_rs_c_mission_index_valid(std::int32_t,std::int32_t);
std::uint32_t a2fo_rs_c_mission_menu_view(void*,A2foStage8MissionStep);
bool a2fo_rs_c_mission_roots(void*,A2foStage8MissionStep);
std::int64_t a2fo_rs_c_mission_show(void*,A2foStage8MissionStep);
std::int32_t a2fo_rs_c_mission_single_hook(void*,A2foStage8MissionStep);
std::int32_t a2fo_rs_c_mission_accept_hook(void*,A2foStage8MissionStep);
bool a2fo_rs_c_mission_populate_missions(void*,A2foStage8MissionStep);
bool a2fo_rs_c_mission_populate_campaigns(void*,A2foStage8MissionStep);
bool a2fo_rs_c_mission_details(void*,A2foStage8MissionStep);
bool a2fo_rs_c_mission_cache_roots(void*,A2foStage8MissionStep);
bool a2fo_rs_c_mission_safe_string(void*,std::size_t,A2foStage8MissionStep);
bool a2fo_rs_c_mission_unlocked(void*,std::int32_t,std::int32_t,
    std::int32_t,A2foStage8MissionStep);
void a2fo_rs_c_mission_restore(void*,std::size_t,A2foStage8MissionStep);
bool a2fo_rs_c_mission_retain(void*,std::size_t,std::size_t,std::size_t,
    A2foStage8MissionStep);
bool a2fo_rs_c_mission_background(void*,A2foStage8MissionStep);
bool a2fo_rs_c_mission_thumbnail(void*,A2foStage8MissionStep);
void a2fo_rs_c_mission_drawing_init(void*,A2foStage8MissionStep);
void a2fo_rs_c_mission_shutdown(void*,A2foStage8MissionStep);
struct A2foStage8MissionCommand {
    std::int32_t identifier,notification,campaign_list,mission_list,start,back,cancel;
    std::int32_t selection_changed,double_click,clicked,list_error;
};
std::int32_t a2fo_rs_c_mission_command(void*,A2foStage8MissionCommand,
    A2foStage8MissionStep);
void a2fo_rs_c_mission_exit(void*,A2foStage8MissionStep);
std::size_t a2fo_rs_c_mission_copy_length(std::size_t,std::size_t);
struct A2foStage8MissionShellRect {
    std::int32_t origin_x,origin_y,client_left,client_top,client_right,client_bottom;
    std::int32_t left,top,right,bottom;
    std::uint32_t filled;
};
bool a2fo_rs_c_mission_shell_rect(void*,A2foStage8MissionShellRect*,
    A2foStage8MissionStep);
struct A2foStage8MissionLayout {
    std::int32_t width,height,image_height,image_width;
    bool has_height_image,has_width_image;
    std::int32_t rectangles[7][4];
};
void a2fo_rs_c_mission_layout(A2foStage8MissionLayout*);
}
