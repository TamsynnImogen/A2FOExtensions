#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
struct A2foStage8BuildyardBytes {
    const std::uint8_t* data;
    std::size_t size;
};
struct A2foStage8BuildyardEvent {
    std::uint32_t op;
    A2foStage8BuildyardBytes a;
    std::int32_t value;
};
using A2foStage8BuildyardClass = bool (*)(std::uint8_t,std::uint32_t);
using A2foStage8BuildyardEmit = bool (*)(void*,const A2foStage8BuildyardEvent*);
bool a2fo_rs_c_buildyard_parse(const std::uint8_t*,std::size_t,
    A2foStage8BuildyardClass,void*,A2foStage8BuildyardEmit);
struct A2foStage8BuildyardEvalRequest {
    std::uint32_t op;
    std::size_t requirement;
    std::size_t peer;
    std::size_t object;
    std::int32_t team;
    std::int32_t other;
    bool ally;
    A2foStage8BuildyardBytes text;
};
using A2foStage8BuildyardEvalStep = std::int64_t (*)(
    void*,A2foStage8BuildyardEvalRequest*);
using A2foStage8BuildyardEvalDone = bool (*)(
    void*,bool,const std::uint8_t*,std::size_t);
bool a2fo_rs_c_buildyard_evaluate(void*,std::int32_t,
    A2foStage8BuildyardClass,A2foStage8BuildyardEvalStep,
    A2foStage8BuildyardEvalDone);
std::uint8_t a2fo_rs_c_buildyard_merge(A2foStage8BuildyardBytes,
    A2foStage8BuildyardBytes,bool,bool);
using A2foStage8BuildyardStep = std::int64_t (*)(void*,std::uint32_t,std::size_t);
bool a2fo_rs_c_buildyard_relocated(void*,std::size_t,std::size_t,
    A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_validate_team(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_native_team_admit(bool,std::int32_t);
bool a2fo_rs_c_buildyard_race(void*,bool,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_peer(void*,bool,std::int32_t,std::int32_t,
    A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_copy_text(void*,std::size_t,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_capture(void*,std::int32_t,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_capture_publish(void*,std::size_t,std::int32_t,
    A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_begin(void*,std::int32_t,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_begin_locked(void*,std::int32_t,
    A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_install_tree(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_tree_text(void*,bool,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_tree_commit(void*,std::int32_t,
    A2foStage8BuildyardStep);
using A2foStage8BuildyardLower = std::uint8_t (*)(std::uint8_t);
struct A2foStage8BuildyardSpan {std::size_t first;std::size_t length;};
void a2fo_rs_c_buildyard_lower(std::uint8_t*,std::size_t,
    A2foStage8BuildyardLower);
bool a2fo_rs_c_buildyard_trim(A2foStage8BuildyardBytes,
    A2foStage8BuildyardClass,A2foStage8BuildyardSpan*);
bool a2fo_rs_c_buildyard_ends(A2foStage8BuildyardBytes,
    A2foStage8BuildyardBytes,A2foStage8BuildyardLower);
bool a2fo_rs_c_buildyard_normalize(A2foStage8BuildyardBytes,
    std::uint32_t,A2foStage8BuildyardClass,A2foStage8BuildyardLower,
    void*,A2foStage8BuildyardEmit);
bool a2fo_rs_c_buildyard_parse_command(void*,std::size_t,
    A2foStage8BuildyardStep);
void a2fo_rs_c_buildyard_copy_delphi(void*,std::size_t,
    A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_byte_truth(std::uintptr_t);
bool a2fo_rs_c_buildyard_tech_entry(void*,std::size_t,
    A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_tooltip_key(void*,std::size_t,
    A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_read_text(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_register_definition(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_parse_catalog(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_enumerate_xml(void*,A2foStage8BuildyardStep);
std::size_t a2fo_rs_c_buildyard_load_symbolic(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_has_bytes(A2foStage8BuildyardBytes);
bool a2fo_rs_c_buildyard_class_loaded(void*,std::uint32_t,
    A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_display_name(void*,A2foStage8BuildyardStep);
std::uint32_t a2fo_rs_c_buildyard_requirement_id(void*,A2foStage8BuildyardStep);
std::int32_t a2fo_rs_c_buildyard_count(void*,bool,bool,std::uint32_t,
    std::uintptr_t,std::uintptr_t,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_recursive_allow(void*,std::uint32_t,
    std::uintptr_t,std::uintptr_t,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_symbolic(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_symbolic_select(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_technology_dispatch(void*,A2foStage8BuildyardStep);
std::uint32_t a2fo_rs_c_buildyard_allocate_binding(void*,
    A2foStage8BuildyardStep);
std::size_t a2fo_rs_c_buildyard_localization_length(
    const std::uint8_t*,std::size_t);
bool a2fo_rs_c_buildyard_first_tooltip_line(void*,
    A2foStage8BuildyardClass,A2foStage8BuildyardStep,
    A2foStage8BuildyardEmit);
bool a2fo_rs_c_buildyard_format_missing(bool,bool,bool,bool,
    A2foStage8BuildyardBytes,A2foStage8BuildyardBytes,
    A2foStage8BuildyardLower,void*,A2foStage8BuildyardEmit);
bool a2fo_rs_c_buildyard_project_capture(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_initialize(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_two_hooks(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_resolve_target(std::uintptr_t,std::uintptr_t,
    void*,A2foStage8BuildyardStep);
std::size_t a2fo_rs_c_buildyard_patch_calls(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_button_binding(void*,std::uint32_t,
    A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_compose_generated(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_compose_component(void*,A2foStage8BuildyardStep);
using A2foStage8BuildyardFetch = bool (*)(void*,std::size_t,
    A2foStage8BuildyardBytes*);
using A2foStage8BuildyardDisplay = bool (*)(void*,
    A2foStage8BuildyardBytes,A2foStage8BuildyardBytes*);
using A2foStage8BuildyardAccept = bool (*)(void*,std::size_t,
    A2foStage8BuildyardBytes);
bool a2fo_rs_c_buildyard_match_tooltip(std::uint32_t,
    A2foStage8BuildyardBytes,A2foStage8BuildyardBytes,std::size_t,
    A2foStage8BuildyardClass,A2foStage8BuildyardLower,
    void*,A2foStage8BuildyardFetch,A2foStage8BuildyardDisplay,
    A2foStage8BuildyardAccept);
bool a2fo_rs_c_buildyard_provider(void*,std::int32_t,
    A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_bind_tooltip_api(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_install_provider(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_install_localization(void*,A2foStage8BuildyardStep);
bool a2fo_rs_c_buildyard_localization_lookup(void*,std::int32_t,
    A2foStage8BuildyardStep);
}
