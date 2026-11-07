/*
 * File: modules/A2FOHybridBuild/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Bootstrap HybridBuild aliases, cocoon policy, and runtime hooks.
 */

#include "../A2FOFeaturePack/hybrid_bridge_api.hpp"
#include "hybrid_production_runtime.hpp"
#include "refit_ui_bridge_api.hpp"
#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/module_runtime_a2_ffi.h"
#include "../../sdk/include/a2fo_module_provider.hpp"

#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <exception>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kModuleName = "A2FOHybridBuild";

const A2FO_ModuleApi* g_api = nullptr;
constexpr std::size_t kMaximumOdfSize = 2 * 1024 * 1024;
std::map<std::string, std::string> g_odf_paths;
std::exception_ptr g_hybrid_error;
HMODULE g_feature_pack = nullptr;
A2FO_RegisterHybridBridgeFn g_register_bridge = nullptr;
HMODULE g_armada = nullptr, g_fleet_ops = nullptr;

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

// Rust owns overlay traversal and file ordering; Win32 enumeration remains native.
std::uint32_t hybrid_root_count(void*) noexcept {
    if (g_hybrid_error) return 0;
    try { return g_api->extension_root_count(); }
    catch (...) { g_hybrid_error=std::current_exception(); return 0; }
}
std::uint8_t hybrid_root(void*,std::uint32_t index,A2foRsHybridBytes* out) noexcept {
    if (g_hybrid_error || !out) return 0;
    try {
        const char* root=g_api->extension_root(index);
        if(!root || !*root)return 0;
        *out={reinterpret_cast<const std::uint8_t*>(root),std::strlen(root)};
        return 1;
    } catch (...) {g_hybrid_error=std::current_exception();return 0;}
}
void hybrid_list(void*,A2foRsHybridBytes directory,A2foRsHybridChildEmit emit,
                 void* emit_context) noexcept {
    if(g_hybrid_error || !emit)return;
    HANDLE search=INVALID_HANDLE_VALUE;
    try {
        std::string pattern(reinterpret_cast<const char*>(directory.data),directory.size);
        pattern += "\\*";
        WIN32_FIND_DATAA data{};
        search=FindFirstFileA(pattern.c_str(),&data);
        if(search==INVALID_HANDLE_VALUE)return;
        do {
            const std::size_t size=std::strlen(data.cFileName);
            emit(emit_context,{reinterpret_cast<const std::uint8_t*>(data.cFileName),size},
                (data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)!=0,
                (data.dwFileAttributes&FILE_ATTRIBUTE_REPARSE_POINT)!=0);
        } while(FindNextFileA(search,&data));
        FindClose(search);
    } catch (...) {
        if(search!=INVALID_HANDLE_VALUE)FindClose(search);
        g_hybrid_error=std::current_exception();
    }
}
void hybrid_store(void*,A2foRsHybridBytes key,A2foRsHybridBytes path) noexcept {
    if(g_hybrid_error)return;
    try {
        g_odf_paths[std::string(reinterpret_cast<const char*>(key.data),key.size)] =
            std::string(reinterpret_cast<const char*>(path.data),path.size);
    } catch (...) {g_hybrid_error=std::current_exception();}
}
std::uint8_t hybrid_find(void*,A2foRsHybridBytes key,A2foRsHybridBytes* out) noexcept {
    if(g_hybrid_error || !out)return 0;
    try {
        const auto found=g_odf_paths.find(std::string(reinterpret_cast<const char*>(key.data),key.size));
        if(found==g_odf_paths.end())return 0;
        *out={reinterpret_cast<const std::uint8_t*>(found->second.data()),found->second.size()};
        return 1;
    } catch (...) {g_hybrid_error=std::current_exception();return 0;}
}
bool read_small_text_file(const std::string& path, std::string* contents) {
    if (!contents) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(kMaximumOdfSize)) {
        return false;
    }
    input.seekg(0, std::ios::beg);
    std::ostringstream stream;
    stream << input.rdbuf();
    *contents = stream.str();
    return input.good() || input.eof();
}

struct HybridHandlerContext { a2fo::build_submenu::Config config; };
std::uint8_t hybrid_read(void*,A2foRsHybridBytes path,A2foRsHybridTextEmit emit,
                         void* emit_context) noexcept {
    if(g_hybrid_error || !emit)return 0;
    try {
        std::string contents;
        if(!read_small_text_file(std::string(reinterpret_cast<const char*>(path.data),path.size),
                                 &contents))return 0;
        emit(emit_context,{reinterpret_cast<const std::uint8_t*>(contents.data()),contents.size()});
        return 1;
    } catch (...) {g_hybrid_error=std::current_exception();return 0;}
}
std::uint8_t hybrid_parse(void* context,A2foRsHybridBytes contents) noexcept {
    if(g_hybrid_error)return 0;
    try {
        auto& output=*static_cast<HybridHandlerContext*>(context);
        std::string error;
        return a2fo::build_submenu::parse_config(
            std::string(reinterpret_cast<const char*>(contents.data),contents.size),
            &output.config,&error) && !output.config.empty();
    } catch (...) {g_hybrid_error=std::current_exception();return 0;}
}
std::uint8_t hybrid_register(void* context,std::uintptr_t object_class,
                             A2foRsHybridBytes source) noexcept {
    if(g_hybrid_error)return 0;
    try {
        auto& output=*static_cast<HybridHandlerContext*>(context);
        return a2fo::register_live_build_submenus(reinterpret_cast<void*>(object_class),
            output.config,std::string(reinterpret_cast<const char*>(source.data),source.size));
    } catch (...) {g_hybrid_error=std::current_exception();return 0;}
}
void hybrid_log(void*,std::uint32_t code,A2foRsHybridBytes source,
                std::uint32_t count) noexcept {
    if(g_hybrid_error)return;
    try {
        switch(code){
        case 0: log_line("HybridBuild semantic policy registration failed");break;
        case 1: log_line("A2FOFeaturePack is required for shared queue and ResearchStation hook ownership");break;
        case 2: log_line("A2FOFeaturePack does not expose the HybridBuild bridge");break;
        case 3: log_line("FeaturePack Producer event dispatch unavailable");break;
        case 4: log_line("A2FOFeaturePack rejected the HybridBuild bridge");break;
        case 5: log_line("Shared queue and ResearchStation bridge registered");break;
        case 6: log_line("Could not index loose ODF files for live build submenus");break;
        case 7: log_line("Live build submenus in "+
            std::string(reinterpret_cast<const char*>(source.data),source.size)+
            " were rejected; native build list retained");break;
        case 8: log_line("GameObjectClass-loaded API unavailable; live build submenus disabled");break;
        case 9: log_line("HybridBuild module initialized; live build submenus enabled ("+
            std::to_string(count)+" loose ODFs indexed)");break;
        case 10: log_line("HybridBuild module initialized; live build submenus disabled ("+
            std::to_string(count)+" loose ODFs indexed)");break;
        case 11: log_line("HybridBuild module loaded with runtime adapter disabled");break;
        }
    } catch (...) {g_hybrid_error=std::current_exception();}
}
void A2FO_CALL class_loaded_handler(
    const A2FO_GameObjectClassLoadedEvent* event,void*) {
    g_hybrid_error=nullptr;
    HybridHandlerContext context;
    const bool valid=event && event->struct_size>=sizeof(*event);
    const A2foRsHybridBytes source{
        reinterpret_cast<const std::uint8_t*>(valid ? event->source_odf.data : nullptr),
        valid ? event->source_odf.size : 0};
    a2fo_rs_runtime_hybrid_handler(valid,
        reinterpret_cast<std::uintptr_t>(valid ? event->object_class : nullptr),
        source,&hybrid_find,&hybrid_read,&hybrid_parse,&hybrid_register,&hybrid_log,&context);
    if(g_hybrid_error)std::rethrow_exception(g_hybrid_error);
}
std::uint32_t hybrid_step(void*,std::uint32_t operation) noexcept {
    if(g_hybrid_error && operation!=8)return 0;
    try {
        switch(operation){
        case 0:
            g_armada=static_cast<HMODULE>(g_api->armada_module());
            g_fleet_ops=static_cast<HMODULE>(g_api->fleetops_module());
            return g_armada && g_fleet_ops;
        case 1:return g_api->register_classlabel_alias(kModuleName,"hybridbuild","research");
        case 2:return g_api->register_evolver_cocoon_command(kModuleName,"cocoon");
        case 3:
            g_feature_pack=a2fo::startup_module_provider("A2FOFeaturePack.dll",
                {"A2FO_RegisterHybridBridge","A2FO_ProducerEventsReady"});
            return g_feature_pack!=nullptr;
        case 4:{
            FARPROC address=GetProcAddress(g_feature_pack,"A2FO_RegisterHybridBridge");
            static_assert(sizeof(g_register_bridge)==sizeof(address));
            std::memcpy(&g_register_bridge,&address,sizeof(g_register_bridge));
            return g_register_bridge!=nullptr;
        }
        case 5:{
            using Ready=bool (A2FO_CALL*)() noexcept;
            FARPROC address=GetProcAddress(g_feature_pack,"A2FO_ProducerEventsReady");
            Ready ready=nullptr;
            static_assert(sizeof(ready)==sizeof(address));
            std::memcpy(&ready,&address,sizeof(ready));
            return ready && ready();
        }
        case 6:{
            A2FO_HybridBridge bridge{};
            bridge.struct_size=sizeof(bridge);bridge.version=A2FO_HYBRID_BRIDGE_VERSION;
            bridge.has_queued_research_conflict=&a2fo::hybrid_production_has_queued_research_conflict;
            bridge.is_evolve_target=&a2fo::hybrid_production_is_evolve_target;
            bridge.has_evolution_barrier=&a2fo::hybrid_production_has_evolution_barrier;
            bridge.should_defer_construct_order=&a2fo::hybrid_production_should_defer_construct_order;
            bridge.finalize_construct_order=&a2fo::finalize_hybrid_construct_order;
            bridge.discard_construct_placement=&a2fo::discard_hybrid_construct_placement;
            bridge.clear_construct_placements=&a2fo::clear_hybrid_construct_placements;
            bridge.retain_research_menu_after_order=&a2fo::retain_hybrid_research_menu_after_order;
            bridge.register_research_station_lists=&a2fo::register_research_station_hybrid_lists;
            bridge.publish_research_station_items=&a2fo::publish_research_station_hybrid_items;
            bridge.cleanup_cocoon=&a2fo::cleanup_hybrid_cocoon;
            bridge.cleanup_construction=&a2fo::cleanup_hybrid_construction;
            return g_register_bridge(&bridge);
        }
        case 7:g_odf_paths.clear();return 1;
        case 8:{ const bool failed=g_hybrid_error!=nullptr; g_hybrid_error=nullptr; return failed; }
        case 9:return a2fo::initialize_hybrid_production_registry(g_api,g_armada,g_fleet_ops);
        case 10:return A2FO_MODULE_API_HAS(g_api,register_game_object_class_loaded_handler) &&
            (g_api->capabilities&A2FO_CAP_GAME_OBJECT_CLASS_LOADED)!=0 &&
            g_api->register_game_object_class_loaded_handler;
        case 11:return g_api->register_game_object_class_loaded_handler(
            kModuleName,nullptr,0,&class_loaded_handler,nullptr);
        case 12:return static_cast<std::uint32_t>(g_odf_paths.size());
        default:return 0;
        }
    } catch (...) {g_hybrid_error=std::current_exception();return 0;}
}
}  // namespace

extern "C" __declspec(dllexport)
bool __cdecl A2FO_RegisterRefitUiBridge(
    const A2FO_RefitUiBridge* bridge) {
    return a2fo::register_refit_ui_bridge(bridge);
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    const bool valid=api && api->struct_size>=A2FO_MODULE_API_V4_BASE_SIZE &&
        api->api_version==A2FO_MODULE_API_VERSION && api->log &&
        api->armada_module && api->fleetops_module &&
        api->extension_root_count && api->extension_root &&
        api->register_classlabel_alias && api->register_evolver_cocoon_command;
    if(!valid)return false;
    g_api=api;
    g_hybrid_error=nullptr;
    const bool result=a2fo_rs_runtime_hybrid_init(valid,&hybrid_step,
        &hybrid_root_count,&hybrid_root,&hybrid_list,&hybrid_store,&hybrid_log,nullptr)!=0;
    if(g_hybrid_error)std::rethrow_exception(g_hybrid_error);
    return result;
}
