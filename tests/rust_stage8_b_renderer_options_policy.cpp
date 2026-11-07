#include "../rust/stage8_b_renderer_options_ffi.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Trace {
    std::vector<std::uint32_t> ids;
    std::uint32_t previous{},applied{},requested{};
    bool active{},payload{},same{},last_error_empty{},preserve{},write{true};
    bool scheduled{},helper{true},created{true};
};
struct InstallTrace {
    std::vector<std::uint32_t> ids,signatures;
    std::uint32_t fail{UINT32_MAX};
    bool installed{},same_module{true},same_root{true},pending{},error_empty{true};
};
struct MonitorTrace {
    std::vector<std::uint32_t> ids;
    bool syncing{},window{true},timer_active{};
    std::int32_t selected{1},observed{-1};
};
std::uint64_t monitor_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& t=*static_cast<MonitorTrace*>(opaque);t.ids.push_back(id);
    if(id==70)return !t.syncing;
    if(id==71||id==72||id==81||id==82||id==83)return t.window;
    if(id==73)return static_cast<std::uint32_t>(t.selected);
    if(id==74)return 2;
    if(id==75)return static_cast<std::uint32_t>(t.observed);
    if(id==80)return t.timer_active;
    return 0;
}
struct WindowTrace {
    std::vector<std::uint32_t> ids;
    bool renderer{true},monitor{},same_request{true},destroy{};
    std::int32_t selected{1};
};
struct EffectTrace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    bool form{true},bump{true},reuse{};
};
struct MonitorFlowTrace {
    std::vector<std::uint32_t> ids,indexes;
    std::uint32_t fail{UINT32_MAX};
    std::uint32_t count{2};bool reusable{},configured{},present{true};
};
struct ControlTrace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    bool reuse{},font{true};
};
struct RendererControlTrace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    bool reusable{},font{},last_error_empty{true};
};
struct HelperTrace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    bool empty{},cached{},loaded{true},copy{true},payload_exists{};
};
std::uint64_t helper_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& t=*static_cast<HelperTrace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==702)return t.cached?1:static_cast<std::uint32_t>(-1);
    if(id==703)return t.empty;
    if(id==704)return t.loaded;
    if(id==706)return t.loaded;
    if(id==740)return t.payload_exists;
    if(id==744)return t.copy;
    if(id==745)return 0;
    if(id==750)return 12;
    if(id==751)return 18;
    if(id==752)return 16;
    return 1;
}
std::uint64_t renderer_control_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& t=*static_cast<RendererControlTrace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==509)return t.reusable;
    if(id==518||id==575)return 1;
    if(id==557||id==561)return t.font;
    if(id==593)return t.last_error_empty;
    if(id==595)return 0;
    return 1;
}
std::uint64_t control_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& t=*static_cast<ControlTrace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==403)return t.reuse;
    if(id==433||id==437)return t.font;
    return 1;
}
std::uint64_t monitor_flow_operation(void* opaque,std::uint32_t id,std::uint32_t arg){
    auto& t=*static_cast<MonitorFlowTrace*>(opaque);t.ids.push_back(id);
    if(id==277)t.indexes.push_back(arg);
    if(id==t.fail)return 0;
    if(id==252||id==253)return t.reusable;
    if(id==254||id==259)return 0;
    if(id==276||id==300)return t.count;
    if(id==278)return t.count==0;
    if(id==292)return t.configured;
    if(id==293)return t.present;
    return 1;
}
std::uint64_t effect_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& t=*static_cast<EffectTrace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==200)return t.form;
    if(id==201)return t.bump;
    if(id==203)return t.reuse;
    return 1;
}
std::uint64_t window_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& t=*static_cast<WindowTrace*>(opaque);t.ids.push_back(id);
    if(id==100||id==101||id==102||id==103)return t.renderer;
    if(id==104||id==105||id==106||id==107)return t.monitor;
    if(id==111||id==112||id==120||id==121)return 1;
    if(id==113||id==122)return static_cast<std::uint32_t>(t.selected);
    if(id==114)return t.same_request?0:1;
    if(id==115)return 0;
    if(id==123)return 2;
    if(id==124)return 0;
    if(id==133||id==134)return t.destroy;
    return 0;
}
std::uint64_t install_operation(void* opaque,std::uint32_t id,std::uint32_t arg){
    auto& t=*static_cast<InstallTrace*>(opaque);t.ids.push_back(id);
    if(id==49)t.signatures.push_back(arg);
    if(id==t.fail)return 0;
    if(id==41)return 0;
    if(id==42)return t.installed;
    if(id==43)return t.same_module;
    if(id==44)return t.same_root;
    if(id==55)return t.pending;
    if(id==56)return 0;
    if(id==60)return t.error_empty;
    return 1;
}
std::uint64_t operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& t=*static_cast<Trace*>(opaque);
    t.ids.push_back(id);
    switch(id){
    case 0:return t.active;
    case 1:case 21:return t.payload;
    case 2:return t.same;
    case 4:case 18:case 22:return t.applied;
    case 5:case 26:return t.write;
    case 8:return t.scheduled;
    case 10:return t.helper;
    case 13:return t.created;
    case 17:return t.previous;
    case 19:return t.last_error_empty;
    case 23:return t.preserve;
    default:return 1;
    }
}
bool check(bool valid,const char* label){
    if(!valid)std::fprintf(stderr,"renderer policy failure: %s\n",label);
    return valid;
}
int main(){
    Trace unmanaged{};unmanaged.active=true;
    if(!check(a2fo_rs_stage8_b_renderer_reconcile(&unmanaged,operation)==1&&
              unmanaged.ids==std::vector<std::uint32_t>{0,1,3},
              "unmanaged active file"))return 1;
    Trace matched{};matched.active=true;matched.payload=true;matched.same=true;
    matched.applied=1;
    if(!check(a2fo_rs_stage8_b_renderer_reconcile(&matched,operation)==1&&
              matched.ids==std::vector<std::uint32_t>{0,1,2,4},
              "matching DXVK"))return 1;
    Trace reconciliation{};reconciliation.active=false;reconciliation.applied=1;
    if(!check(a2fo_rs_stage8_b_renderer_reconcile(&reconciliation,operation)==1&&
              reconciliation.ids==std::vector<std::uint32_t>{0,4,5,7},
              "reconcile system"))return 1;
    Trace missing{};missing.helper=false;
    if(!check(a2fo_rs_stage8_b_renderer_schedule(&missing,operation)==0&&
              missing.ids==std::vector<std::uint32_t>{8,9,10,11},
              "missing helper"))return 1;
    Trace started{};
    if(!check(a2fo_rs_stage8_b_renderer_schedule(&started,operation)==1&&
              started.ids==std::vector<std::uint32_t>{8,9,10,12,13,15,16},
              "helper launch"))return 1;
    Trace unchanged{};unchanged.previous=1;unchanged.applied=1;unchanged.last_error_empty=true;
    if(!check(a2fo_rs_stage8_b_renderer_select(&unchanged,operation,1)==1&&
              unchanged.ids==std::vector<std::uint32_t>{17,18,19,20},
              "already selected"))return 1;
    Trace absent{};absent.previous=0;absent.applied=0;
    if(!check(a2fo_rs_stage8_b_renderer_select(&absent,operation,1)==1&&
              absent.ids==std::vector<std::uint32_t>{17,18,21,22,24},
              "missing DXVK payload"))return 1;
    Trace preserve{};preserve.previous=1;preserve.applied=1;
    if(!check(a2fo_rs_stage8_b_renderer_select(&preserve,operation,0)==1&&
              preserve.ids==std::vector<std::uint32_t>{17,18,23,25},
              "cannot preserve DXVK"))return 1;
    Trace save{};save.write=false;
    if(!check(a2fo_rs_stage8_b_renderer_select(&save,operation,0)==1&&
              save.ids==std::vector<std::uint32_t>{17,18,19,26,27},
              "setting write failure"))return 1;
    Trace selected{};selected.helper=true;
    if(!check(a2fo_rs_stage8_b_renderer_select(&selected,operation,0)==1&&
              selected.ids==std::vector<std::uint32_t>{17,18,19,26,28,29,31,32},
              "selection success"))return 1;
    InstallTrace installed{};installed.installed=true;
    if(!check(a2fo_rs_stage8_b_renderer_install(&installed,install_operation)==1&&
              installed.ids==std::vector<std::uint32_t>{40,41,42,43,44},
              "matching installed hook returns before state mutation"))return 1;
    InstallTrace signature_failure{};signature_failure.fail=49;
    if(!check(a2fo_rs_stage8_b_renderer_install(&signature_failure,install_operation)==0&&
              signature_failure.ids.back()==50&&
              signature_failure.signatures==std::vector<std::uint32_t>{0},
              "first bad signature prevents both hooks"))return 1;
    InstallTrace hook_failure{};hook_failure.fail=51;
    if(!check(a2fo_rs_stage8_b_renderer_install(&hook_failure,install_operation)==0&&
              hook_failure.ids.back()==53&&hook_failure.signatures.size()==24,
              "signature sequence completes before checked hooks"))return 1;
    InstallTrace pending_request{};pending_request.pending=true;
    if(!check(a2fo_rs_stage8_b_renderer_install(&pending_request,install_operation)==1&&
              pending_request.ids.back()==62,
              "pending renderer helper scheduled after audit"))return 1;
    InstallTrace settled{};
    if(!check(a2fo_rs_stage8_b_renderer_install(&settled,install_operation)==1&&
              settled.ids.back()==64,
              "matching applied state skips helper"))return 1;
    MonitorTrace changed{};
    a2fo_rs_stage8_b_renderer_observe_monitor(&changed,monitor_operation);
    if(!check(changed.ids.back()==78&&
              std::find(changed.ids.begin(),changed.ids.end(),76u)!=changed.ids.end(),
              "monitor observation precedes saved setting"))return 1;
    MonitorTrace busy{};busy.syncing=true;
    a2fo_rs_stage8_b_renderer_observe_monitor(&busy,monitor_operation);
    if(!check(busy.ids==std::vector<std::uint32_t>{70},
              "syncing monitor skips HWND read"))return 1;
    MonitorTrace timer{};
    a2fo_rs_stage8_b_renderer_monitor_timer(&timer,monitor_operation);
    if(!check(timer.ids.back()==86,
              "timer marks active before diagnostic"))return 1;
    WindowTrace renderer_window{};
    a2fo_rs_stage8_b_renderer_window(&renderer_window,window_operation);
    if(!check(std::find(renderer_window.ids.begin(),renderer_window.ids.end(),110u)!=renderer_window.ids.end()&&
              std::find(renderer_window.ids.begin(),renderer_window.ids.end(),117u)!=renderer_window.ids.end()&&
              renderer_window.ids.back()==133,
              "renderer fallback runs after native forwarding"))return 1;
    WindowTrace monitor_window{};monitor_window.renderer=false;monitor_window.monitor=true;
    a2fo_rs_stage8_b_renderer_window(&monitor_window,window_operation);
    if(!check(std::find(monitor_window.ids.begin(),monitor_window.ids.end(),128u)!=monitor_window.ids.end()&&
              std::find(monitor_window.ids.begin(),monitor_window.ids.end(),113u)==monitor_window.ids.end(),
              "monitor fallback avoids renderer combo"))return 1;
    WindowTrace destroyed{};destroyed.renderer=false;destroyed.destroy=true;
    a2fo_rs_stage8_b_renderer_window(&destroyed,window_operation);
    if(!check(destroyed.ids.back()==137,
              "window destroy clears handles after native forwarding"))return 1;
    EffectTrace reused{};reused.reuse=true;
    a2fo_rs_stage8_b_renderer_effect_controls(&reused,effect_operation);
    if(!check(reused.ids.back()==215&&
              std::find(reused.ids.begin(),reused.ids.end(),217u)==reused.ids.end(),
              "reused effect controls refresh without allocation"))return 1;
    EffectTrace constructed{};
    a2fo_rs_stage8_b_renderer_effect_controls(&constructed,effect_operation);
    if(!check(constructed.ids.back()==248&&
              std::find(constructed.ids.begin(),constructed.ids.end(),235u)!=constructed.ids.end(),
              "parent precedes image assignment"))return 1;
    EffectTrace second_failed{};second_failed.fail=220;
    a2fo_rs_stage8_b_renderer_effect_controls(&second_failed,effect_operation);
    if(!check(second_failed.ids[second_failed.ids.size()-2]==221&&
              second_failed.ids.back()==222,
              "constructor failure clears both controls"))return 1;
    EffectTrace unparented{};unparented.fail=232;
    a2fo_rs_stage8_b_renderer_effect_controls(&unparented,effect_operation);
    if(!check(unparented.ids.back()==234&&
              std::find(unparented.ids.begin(),unparented.ids.end(),235u)==unparented.ids.end(),
              "parent mismatch skips shared images"))return 1;
    MonitorFlowTrace subclass{};
    if(!check(a2fo_rs_stage8_b_renderer_subclass(&subclass,monitor_flow_operation)==1&&
              subclass.ids.back()==262,
              "subclass publishes native previous proc after install"))return 1;
    MonitorFlowTrace reused_subclass{};reused_subclass.reusable=true;
    if(!check(a2fo_rs_stage8_b_renderer_subclass(&reused_subclass,monitor_flow_operation)==1&&
              reused_subclass.ids==std::vector<std::uint32_t>{250,251,252,253},
              "existing form skips WNDPROC replacement"))return 1;
    MonitorFlowTrace refreshed{};refreshed.configured=true;refreshed.present=false;
    a2fo_rs_stage8_b_renderer_refresh_monitor(&refreshed,monitor_flow_operation);
    if(!check(refreshed.indexes==std::vector<std::uint32_t>{0,1}&&
              refreshed.ids.back()==294,
              "monitor enumeration before disconnected fallback"))return 1;
    MonitorFlowTrace empty_monitors{};empty_monitors.count=0;
    a2fo_rs_stage8_b_renderer_refresh_monitor(&empty_monitors,monitor_flow_operation);
    if(!check(empty_monitors.ids.back()==284&&
              std::find(empty_monitors.ids.begin(),empty_monitors.ids.end(),285u)==empty_monitors.ids.end(),
              "empty monitor list releases sync guard"))return 1;
    MonitorFlowTrace saved{};
    a2fo_rs_stage8_b_renderer_save_monitor(&saved,monitor_flow_operation,1);
    if(!check(saved.ids==std::vector<std::uint32_t>{300,301,302,306,307},
              "monitor save updates observed before persistence"))return 1;
    MonitorFlowTrace failed_save{};failed_save.fail=302;
    a2fo_rs_stage8_b_renderer_save_monitor(&failed_save,monitor_flow_operation,1);
    if(!check(failed_save.ids.back()==305&&
              std::find(failed_save.ids.begin(),failed_save.ids.end(),303u)!=failed_save.ids.end(),
              "failed monitor save refreshes selection"))return 1;
    ControlTrace reused_monitor{};reused_monitor.reuse=true;
    a2fo_rs_stage8_b_renderer_monitor_controls(&reused_monitor,control_operation);
    if(!check(reused_monitor.ids.back()==415&&
              std::find(reused_monitor.ids.begin(),reused_monitor.ids.end(),416u)==reused_monitor.ids.end(),
              "reusable monitor controls reacquire handle"))return 1;
    ControlTrace new_monitor{};
    a2fo_rs_stage8_b_renderer_monitor_controls(&new_monitor,control_operation);
    if(!check(new_monitor.ids.back()==453&&
              std::find(new_monitor.ids.begin(),new_monitor.ids.end(),430u)!=new_monitor.ids.end()&&
              std::find(new_monitor.ids.begin(),new_monitor.ids.end(),431u)!=new_monitor.ids.end(),
              "monitor combo style before parent before handle"))return 1;
    ControlTrace no_font{};no_font.font=false;
    a2fo_rs_stage8_b_renderer_monitor_controls(&no_font,control_operation);
    if(!check(no_font.ids.back()==453&&
              std::find(no_font.ids.begin(),no_font.ids.end(),435u)==no_font.ids.end(),
              "missing native font still creates selector"))return 1;
    ControlTrace failed_handle{};failed_handle.fail=441;
    a2fo_rs_stage8_b_renderer_monitor_controls(&failed_handle,control_operation);
    if(!check(failed_handle.ids.back()==443,
              "missing handle stops before population"))return 1;
    RendererControlTrace controls{};
    a2fo_rs_stage8_b_renderer_controls(&controls,renderer_control_operation);
    if(!check(controls.ids.back()==601&&
              std::find(controls.ids.begin(),controls.ids.end(),551u)!=controls.ids.end()&&
              std::find(controls.ids.begin(),controls.ids.end(),554u)!=controls.ids.end()&&
              std::find(controls.ids.begin(),controls.ids.end(),568u)!=controls.ids.end(),
              "renderer combo style and parent precede native handle"))return 1;
    RendererControlTrace reused_controls{};reused_controls.reusable=true;
    a2fo_rs_stage8_b_renderer_controls(&reused_controls,renderer_control_operation);
    if(!check(reused_controls.ids.back()==526&&
              std::find(reused_controls.ids.begin(),reused_controls.ids.end(),528u)==reused_controls.ids.end(),
              "reopened form reuses owned VCL controls"))return 1;
    RendererControlTrace failed_bounds{};failed_bounds.fail=547;
    a2fo_rs_stage8_b_renderer_controls(&failed_bounds,renderer_control_operation);
    if(!check(failed_bounds.ids.back()==549&&
              std::find(failed_bounds.ids.begin(),failed_bounds.ids.end(),551u)==failed_bounds.ids.end(),
              "unreadable bounds stop before style and parent"))return 1;
    RendererControlTrace failed_handler{};failed_handler.fail=577;
    a2fo_rs_stage8_b_renderer_controls(&failed_handler,renderer_control_operation);
    if(!check(failed_handler.ids.back()==578&&
              std::find(failed_handler.ids.begin(),failed_handler.ids.end(),580u)==failed_handler.ids.end(),
              "change-handler failure stops visibility publication"))return 1;
    RendererControlTrace prior_error{};prior_error.last_error_empty=false;
    a2fo_rs_stage8_b_renderer_controls(&prior_error,renderer_control_operation);
    if(!check(prior_error.ids.back()==601&&
              std::find(prior_error.ids.begin(),prior_error.ids.end(),594u)!=prior_error.ids.end()&&
              std::find(prior_error.ids.begin(),prior_error.ids.end(),596u)==prior_error.ids.end(),
              "prior helper error suppresses reschedule"))return 1;
    HelperTrace cached{};cached.cached=true;
    if(!check(a2fo_rs_stage8_b_renderer_read_effect(&cached,helper_operation)==1&&
              cached.ids==std::vector<std::uint32_t>{700,701,702},
              "cached effect avoids INI read"))return 1;
    HelperTrace load{};load.loaded=false;
    if(!check(a2fo_rs_stage8_b_renderer_read_effect(&load,helper_operation)==0&&
              load.ids==std::vector<std::uint32_t>{700,701,702,703,704,705,706},
              "effect loading samples atomic after CAS"))return 1;
    HelperTrace save_error{};save_error.fail=726;
    a2fo_rs_stage8_b_renderer_save_effect(&save_error,helper_operation);
    if(!check(save_error.ids.back()==728&&
              std::find(save_error.ids.begin(),save_error.ids.end(),730u)==save_error.ids.end(),
              "effect save failure rolls back before log"))return 1;
    HelperTrace save_ok{};
    a2fo_rs_stage8_b_renderer_save_effect(&save_ok,helper_operation);
    if(!check(save_ok.ids.back()==731&&
              std::find(save_ok.ids.begin(),save_ok.ids.end(),727u)==save_ok.ids.end(),
              "effect save success publishes cache"))return 1;
    HelperTrace payload{};payload.payload_exists=true;
    if(!check(a2fo_rs_stage8_b_renderer_preserve_payload(&payload,helper_operation)==1&&
              payload.ids==std::vector<std::uint32_t>{740},
              "existing DXVK payload short circuits"))return 1;
    HelperTrace no_source{};no_source.fail=741;
    if(!check(a2fo_rs_stage8_b_renderer_preserve_payload(&no_source,helper_operation)==0&&
              no_source.ids==std::vector<std::uint32_t>{740,741},
              "missing active renderer skips directory creation"))return 1;
    HelperTrace positioned{};
    a2fo_rs_stage8_b_renderer_position_effect(&positioned,helper_operation);
    if(!check(positioned.ids==std::vector<std::uint32_t>{750,751,752,754,755,756},
              "effect positions both controls before diagnostic"))return 1;
    HelperTrace invalid{};invalid.fail=752;
    a2fo_rs_stage8_b_renderer_position_effect(&invalid,helper_operation);
    if(!check(invalid.ids.back()==753,
              "invalid native bounds skip VCL setters"))return 1;
    HelperTrace owner_failed{};owner_failed.fail=771;
    if(!check(a2fo_rs_stage8_b_renderer_create_control(&owner_failed,helper_operation)==0&&
              owner_failed.ids.back()==772,
              "constructor insertion precedes owner validation"))return 1;
    HelperTrace control_ok{};
    if(!check(a2fo_rs_stage8_b_renderer_create_control(&control_ok,helper_operation)==1&&
              control_ok.ids.back()==771,
              "constructed control accepted"))return 1;
    if(!check(a2fo_rs_stage8_b_renderer_valid_bump_bounds(400,320,20)==1&&
              a2fo_rs_stage8_b_renderer_valid_bump_bounds(0,320,20)==0&&
              a2fo_rs_stage8_b_renderer_valid_bump_bounds(400,8193,20)==0&&
              a2fo_rs_stage8_b_renderer_scaled_coordinate(288,400,392)==294,
              "graphics layout native anchor limits and rounding"))return 1;
    std::puts("renderer options policy: 49 branches PASS");
}
