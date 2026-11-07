#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
struct A2foStage8ResourceView {const std::uint8_t* data;std::size_t size;};
using A2foStage8ResourceRead = bool (*)(void*,std::uint32_t,std::uint32_t,A2foStage8ResourceView*);
using A2foStage8ResourceCompare = bool (*)(void*,const std::uint8_t*,const std::uint8_t*,std::size_t);
bool a2fo_rs_c_resources_find_field(void*,std::uint32_t,const std::uint8_t*,std::size_t,
    A2foStage8ResourceRead,A2foStage8ResourceCompare,A2foStage8ResourceView*);
using A2foStage8ResourceParse = std::int32_t (*)(void*,std::uint32_t);
bool a2fo_rs_c_resources_parse4(void*,A2foStage8ResourceParse);
using A2foStage8ResourcePresentation = bool (*)(void*,std::uint32_t);
void a2fo_rs_c_resources_presentation40(void*,A2foStage8ResourcePresentation);
std::int32_t a2fo_rs_c_resources_index(std::uint32_t);
bool a2fo_rs_c_resources_nonzero4(const std::int64_t*);
bool a2fo_rs_c_resources_nonzero_costs(const std::int32_t*);
using A2foStage8ResourceRectangleRead = bool (*)(void*,std::uint32_t,std::uint32_t);
std::uint32_t a2fo_rs_c_resources_load_rectangles(void*,bool,A2foStage8ResourceRectangleRead,
    const bool*);
struct A2foStage8ResourceRectangle {
    std::int32_t left,top,right,bottom;
};
struct A2foStage8ResourceContext {
    std::uintptr_t resource_display,text_component;
    A2foStage8ResourceRectangle rectangle;
};
using A2foStage8ResourceContextRead = bool (*)(void*,std::uint32_t,A2foStage8ResourceContext*);
bool a2fo_rs_c_resources_find_context(void*,const bool*,A2foStage8ResourceContextRead,
    std::uint32_t*,A2foStage8ResourceContext*);
bool a2fo_rs_c_resources_usable_rectangle(A2foStage8ResourceRectangle);
std::int32_t a2fo_rs_c_resources_hover(const A2foStage8ResourceRectangle*,
    std::int32_t,std::int32_t);
using A2foStage8ResourcePanelDraw = void (*)(void*,std::uint32_t);
void a2fo_rs_c_resources_draw_panel(void*,A2foStage8ResourcePanelDraw);
struct A2foStage8ResourceRawRectangle {std::int32_t x,y,width,height;};
using A2foStage8ResourceRawRead = void (*)(void*,std::uint32_t,std::uint32_t,
    A2foStage8ResourceRawRectangle*,A2foStage8ResourceRawRectangle*);
bool a2fo_rs_c_resources_panel_plan(void*,std::uint32_t,std::uint32_t,
    A2foStage8ResourceRectangle,const bool*,A2foStage8ResourceRawRead,
    A2foStage8ResourceRawRectangle*,A2foStage8ResourceRawRectangle*);
bool a2fo_rs_c_resources_amount_plan(std::uint32_t,A2foStage8ResourceRectangle);
bool a2fo_rs_c_resources_cache_fresh(std::uintptr_t,std::uintptr_t,
    std::uintptr_t,std::uintptr_t,std::uint32_t,std::uint32_t);
using A2foStage8ResourcePresentationFill = void (*)(void*,std::uint32_t,
    std::uint32_t,std::uint32_t);
void a2fo_rs_c_resources_fill_presentation(void*,A2foStage8ResourcePresentationFill);
using A2foStage8ResourceKeyMatch = bool (*)(void*,std::uint32_t);
std::int32_t a2fo_rs_c_resources_find_native_key(void*,A2foStage8ResourceKeyMatch);
using A2foStage8ResourceInstallStep = std::int32_t (*)(void*,std::uint32_t,std::uint32_t);
std::uint32_t a2fo_rs_c_resources_install_runtime(void*,A2foStage8ResourceInstallStep);
std::uint32_t a2fo_rs_c_resources_install_panel(void*,A2foStage8ResourceInstallStep);
struct A2foStage8ResourcePresentationInstall {
    bool tooltip,verbose,officer;
    std::uint32_t patched;
};
std::uint32_t a2fo_rs_c_resources_install_presentation(void*,
    A2foStage8ResourceInstallStep,A2foStage8ResourcePresentationInstall*);
struct A2foStage8ResourceStart {
    std::uint32_t setting;
    std::int64_t normal[4],lots[4];
};
using A2foStage8ResourceStartStep = bool (*)(void*,std::uint32_t,
    A2foStage8ResourceStart*);
bool a2fo_rs_c_resources_starting_amounts(void*,A2foStage8ResourceStartStep,
    std::int64_t*);
using A2foStage8ResourceSnapshotStep = std::int32_t (*)(void*,std::uint32_t);
bool a2fo_rs_c_resources_snapshot(void*,A2foStage8ResourceSnapshotStep);
using A2foStage8ResourceClassStep = std::int32_t (*)(void*,std::uint32_t);
bool a2fo_rs_c_resources_class_costs(void*,bool,A2foStage8ResourceClassStep);
using A2foStage8ResourcePresentationEmpty = std::int32_t (*)(void*,
    std::uint32_t,std::uint32_t);
using A2foStage8ResourcePresentationLog = bool (*)(void*,std::uint32_t);
bool a2fo_rs_c_resources_log_presentations(void*,
    A2foStage8ResourcePresentationEmpty,A2foStage8ResourcePresentationLog);
using A2foStage8ResourceRegistrationField = void (*)(void*,std::uint32_t,
    std::uint32_t);
void a2fo_rs_c_resources_registration_fields(void*,
    A2foStage8ResourceRegistrationField);
using A2foStage8ResourceInitStep = std::int32_t (*)(void*,std::uint32_t);
std::uint32_t a2fo_rs_c_resources_module_init(void*,A2foStage8ResourceInitStep);
bool a2fo_rs_c_resources_zero_costs(const std::int32_t*);
bool a2fo_rs_c_resources_equal_amounts(const std::int64_t*,const std::int64_t*);
bool a2fo_rs_c_resources_refund_kind(std::uint32_t);
bool a2fo_rs_c_resources_set_amount(std::int64_t);
std::int64_t a2fo_rs_c_resources_clamp_amount(std::int64_t);
bool a2fo_rs_c_resources_finite_colour(float,float,float);
bool a2fo_rs_c_resources_tooltip_fresh(std::uintptr_t,std::uintptr_t,
    std::uintptr_t,std::uintptr_t,std::int32_t,std::int32_t);
bool a2fo_rs_c_resources_cache_refresh(std::uintptr_t,std::uint32_t,
    std::uint32_t,bool);
bool a2fo_rs_c_resources_panel_config_stale(bool,std::uintptr_t,
    std::uintptr_t);
}
