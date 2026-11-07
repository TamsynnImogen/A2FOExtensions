#include "../rust/stage8_b_canvas_ffi.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Trace {std::vector<std::uint32_t> ids;std::uint32_t fail{UINT32_MAX};
    bool ready{true};std::uint64_t count{1};};
std::uint64_t operation(void* opaque,std::uint32_t id) {
    auto& t=*static_cast<Trace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==13||id==21||id==44||id==340)return t.count;
    if(id==740)return 0;
    if(id==1200||id==1201)return 0;
    if(id==41||id==42||id==73||id==110||id==202||id==303)return t.ready;
    return 1;
}
bool check(bool ok,const char* label) {
    if(!ok)std::fprintf(stderr,"canvas policy failure: %s\n",label);
    return ok;
}
int main() {
    Trace no_api{};no_api.fail=1;
    if(!check(a2fo_rs_stage8_b_canvas_initialize(&no_api,operation)==0&&
              no_api.ids==std::vector<std::uint32_t>{0,1},
              "module pointers publish before API pointer admission"))return 1;
    Trace signature{};signature.fail=7;
    if(!check(a2fo_rs_stage8_b_canvas_initialize(&signature,operation)==0&&
              signature.ids.back()==7&&
              std::find(signature.ids.begin(),signature.ids.end(),9u)==signature.ids.end(),
              "signature failure stops before user32 import lookup"))return 1;
    Trace patch{};patch.fail=22;
    if(!check(a2fo_rs_stage8_b_canvas_initialize(&patch,operation)==0&&
              patch.ids.back()==22&&
              std::find(patch.ids.begin(),patch.ids.end(),23u)==patch.ids.end(),
              "failed import patch leaves ready unpublished"))return 1;
    Trace installed{};
    if(!check(a2fo_rs_stage8_b_canvas_initialize(&installed,operation)==1&&
              installed.ids[installed.ids.size()-2]==23&&installed.ids.back()==24,
              "checked imports precede live gate and log"))return 1;
    Trace inactive_mode{};inactive_mode.ready=false;
    a2fo_rs_stage8_b_canvas_active_mode(&inactive_mode,operation);
    if(!check(inactive_mode.ids==std::vector<std::uint32_t>{40,41},
              "mode copy before inactive gate"))return 1;
    Trace roots{};
    a2fo_rs_stage8_b_canvas_active_mode(&roots,operation);
    if(!check(roots.ids.back()==50&&
              std::find(roots.ids.begin(),roots.ids.end(),0x30000u)!=roots.ids.end(),
              "roots snapshot before ordered scale/move/invalidate"))return 1;
    Trace missing_output{};missing_output.fail=70;
    if(!check(a2fo_rs_stage8_b_canvas_copy(&missing_output,operation)==0&&
              missing_output.ids==std::vector<std::uint32_t>{70},
              "missing width stops before height or output write"))return 1;
    Trace no_record{};no_record.ready=false;
    if(!check(a2fo_rs_stage8_b_canvas_copy(&no_record,operation)==0&&
              no_record.ids==std::vector<std::uint32_t>{70,71,72,73},
              "copy zeros outputs before owner lookup"))return 1;
    Trace no_dc{};no_dc.fail=79;
    if(!check(a2fo_rs_stage8_b_canvas_copy(&no_dc,operation)==1&&
              no_dc.ids.back()==79,
              "metadata-only copy succeeds before destination inspection"))return 1;
    Trace bad_bitmap{};bad_bitmap.fail=85;
    if(!check(a2fo_rs_stage8_b_canvas_copy(&bad_bitmap,operation)==0&&
              bad_bitmap.ids.back()==85,
              "missing destination bitmap prevents GDI save"))return 1;
    Trace presenter_off{};presenter_off.ready=false;
    if(!check(a2fo_rs_stage8_b_canvas_presenter(&presenter_off,operation)==0&&
              presenter_off.ids==std::vector<std::uint32_t>{110},
              "presenter owner gate"))return 1;
    Trace presenter_existing{};
    if(!check(a2fo_rs_stage8_b_canvas_presenter(&presenter_existing,operation)==1&&
              presenter_existing.ids.back()==117,
              "existing presenter matches both callback and context"))return 1;
    Trace scroll_off{};scroll_off.ready=false;
    a2fo_rs_stage8_b_canvas_scroll(&scroll_off,operation);
    if(!check(scroll_off.ids==std::vector<std::uint32_t>{200,201,202,207},
              "untracked scroll calls original before alias snapshots"))return 1;
    Trace scroll_region{};scroll_region.fail=211;
    a2fo_rs_stage8_b_canvas_scroll(&scroll_region,operation);
    if(!check(scroll_region.ids[scroll_region.ids.size()-2]==212&&
              scroll_region.ids.back()==213,
              "region allocation failure invalidates and reports memory error"))return 1;
    Trace acquire_off{};acquire_off.ready=false;
    if(!check(a2fo_rs_stage8_b_canvas_acquire(&acquire_off,operation)==0&&
              acquire_off.ids==std::vector<std::uint32_t>{300,301,302,303},
              "untracked DC returns physical before resource allocation"))return 1;
    Trace finish_none{};finish_none.fail=341;
    if(!check(a2fo_rs_stage8_b_canvas_finish(&finish_none,operation)==0&&
              finish_none.ids==std::vector<std::uint32_t>{340,0x40000,341},
              "lease lookup misses without GDI restoration"))return 1;
    Trace finish_one{};
    if(!check(a2fo_rs_stage8_b_canvas_finish(&finish_one,operation)==1&&
              finish_one.ids.back()==356,
              "lease completion copies outputs before owner destruction"))return 1;
    Trace no_dialog{};no_dialog.fail=421;
    a2fo_rs_stage8_b_canvas_dialog_dispatch(&no_dialog,operation);
    if(!check(no_dialog.ids==std::vector<std::uint32_t>{420,421},
              "missing dialog skips callback and TLS dispatch"))return 1;
    Trace no_window{};no_window.fail=441;
    a2fo_rs_stage8_b_canvas_window_dispatch(&no_window,operation);
    if(!check(no_window.ids==std::vector<std::uint32_t>{440,441,442},
              "missing window forwards to native DefWindowProc"))return 1;
    if(!check(a2fo_rs_stage8_b_canvas_mouse_message(0x020a)==0&&
              a2fo_rs_stage8_b_canvas_mouse_message(0x020d)==1,
              "mouse wheel excluded; X button retained"))return 1;
    Trace child_off{};child_off.fail=781;
    a2fo_rs_stage8_b_canvas_adopt_child(&child_off,operation);
    if(!check(child_off.ids==std::vector<std::uint32_t>{780,781},
              "child root gate precedes window geometry"))return 1;
    Trace child_fit{};
    a2fo_rs_stage8_b_canvas_fit_children(&child_fit,operation);
    if(!check(child_fit.ids==std::vector<std::uint32_t>{760,761,762,0x50000,763,764,765,766},
              "children snapshot precedes positioning and restore"))return 1;
    Trace ensure_off{};ensure_off.fail=741;
    if(!check(a2fo_rs_stage8_b_canvas_ensure_child(&ensure_off,operation)==0&&
              ensure_off.ids==std::vector<std::uint32_t>{740,741},
              "ensure child tests existing record before readiness"))return 1;
    Trace bridge_off{};bridge_off.fail=800;
    a2fo_rs_stage8_b_canvas_bridge_dialog(&bridge_off,operation);
    if(!check(bridge_off.ids==std::vector<std::uint32_t>{800},
              "dialog class gate precedes callback owner read"))return 1;
    Trace font_cached{};
    a2fo_rs_stage8_b_canvas_update_font(&font_cached,operation);
    if(!check(font_cached.ids.back()==825&&
              std::find(font_cached.ids.begin(),font_cached.ids.end(),827u)==font_cached.ids.end(),
              "current scaled font skips GetObject"))return 1;
    Trace seed_off{};seed_off.fail=923;
    if(!check(a2fo_rs_stage8_b_canvas_present_seed(&seed_off,operation)==0&&
              seed_off.ids.back()==923,
              "missing presenter prevents backing allocation"))return 1;
    Trace seed_refresh{};seed_refresh.fail=902;
    a2fo_rs_stage8_b_canvas_refresh_seed(&seed_refresh,operation);
    if(!check(seed_refresh.ids==std::vector<std::uint32_t>{900,901,902},
              "invalid seed skips staging surface"))return 1;
    Trace replay_off{};replay_off.fail=950;
    a2fo_rs_stage8_b_canvas_represent_seed(&replay_off,operation);
    if(!check(replay_off.ids==std::vector<std::uint32_t>{950},
              "reentrant replay stops before window query"))return 1;
    Trace fit_off{};fit_off.fail=1005;
    if(!check(a2fo_rs_stage8_b_canvas_normalize_fit(&fit_off,operation)==0&&
              fit_off.ids.back()==1005,
              "invalid scale stops before native dialog inspection"))return 1;
    Trace nested_off{};nested_off.fail=1100;
    if(!check(a2fo_rs_stage8_b_canvas_nested_parent(&nested_off,operation)==0&&
              nested_off.ids==std::vector<std::uint32_t>{1100},
              "missing scoped parent stops before style read"))return 1;
    char known[64]="TSingleForm";
    Trace quiet_known{};
    a2fo_rs_stage8_b_canvas_quiet_secondary(&quiet_known,operation,
        reinterpret_cast<const std::uint8_t*>(known));
    if(!check(std::find(quiet_known.ids.begin(),quiet_known.ids.end(),1205u)!=quiet_known.ids.end(),
              "known secondary class reaches extended style update"))return 1;
    Trace move_off{};move_off.fail=1301;
    a2fo_rs_stage8_b_canvas_move_window(&move_off,operation);
    if(!check(move_off.ids==std::vector<std::uint32_t>{1300,1301,1306},
              "untracked MoveWindow still forwards"))return 1;
    Trace position_off{};position_off.fail=1321;
    a2fo_rs_stage8_b_canvas_position_window(&position_off,operation);
    if(!check(position_off.ids==std::vector<std::uint32_t>{1320,1321,1331},
              "untracked SetWindowPos still forwards"))return 1;
    std::puts("canvas policy: 33 branches PASS");
}
