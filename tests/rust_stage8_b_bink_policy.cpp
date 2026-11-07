#include "../rust/stage8_b_bink_video_ffi.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Trace {std::vector<std::uint32_t> ids;std::uint32_t fail{UINT32_MAX};
    bool ready{true},refresh{},needs_mode{},changed{},cached{},shell{},
        shell_bypass{};};
std::uint64_t operation(void* opaque,std::uint32_t id) {
    auto& t=*static_cast<Trace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==51||id==60||id==80||id==200||id==300)return t.ready;
    if(id==101)return !t.refresh;
    if(id==110)return t.needs_mode;
    if(id==111||id==52||id==62||id==93)return 0;
    if(id==124)return !t.changed;
    if(id==209||id==304)return t.shell_bypass;
    if(id==522)return t.cached;
    if(id==524)return t.shell;
    return 1;
}
bool check(bool ok,const char* label) {
    if(!ok)std::fprintf(stderr,"bink policy failure: %s\n",label);
    return ok;
}
int main() {
    Trace no_api{};no_api.fail=1;
    if(!check(a2fo_rs_stage8_b_bink_initialize(&no_api,operation)==0&&
              no_api.ids==std::vector<std::uint32_t>{0,1,7},
              "initial gate clear precedes API read and log"))return 1;
    Trace no_patch{};no_patch.fail=29;
    if(!check(a2fo_rs_stage8_b_bink_initialize(&no_patch,operation)==0&&
              no_patch.ids.back()==33&&
              std::find(no_patch.ids.begin(),no_patch.ids.end(),30u)==no_patch.ids.end(),
              "checked patch failure stops later writes"))return 1;
    Trace no_bytes{};no_bytes.fail=35;
    if(!check(a2fo_rs_stage8_b_bink_initialize(&no_bytes,operation)==0&&
              no_bytes.ids.back()==36&&
              std::find(no_bytes.ids.begin(),no_bytes.ids.end(),37u)==no_bytes.ids.end(),
              "byte patch failure retains inactive gate"))return 1;
    Trace installed{};
    if(!check(a2fo_rs_stage8_b_bink_initialize(&installed,operation)==1&&
              installed.ids[installed.ids.size()-2]==37&&installed.ids.back()==38,
              "live publication follows atomic byte patch"))return 1;
    Trace off_window{};off_window.ready=false;
    a2fo_rs_stage8_b_bink_window(&off_window,operation);
    if(!check(off_window.ids==std::vector<std::uint32_t>{50,51},
              "native window getter precedes disabled gate"))return 1;
    Trace off_play{};off_play.ready=false;
    if(!check(a2fo_rs_stage8_b_bink_play(&off_play,operation)==1&&
              off_play.ids==std::vector<std::uint32_t>{60,68,69},
              "inactive play still scopes native handle and calls original"))return 1;
    Trace active_play{};
    if(!check(a2fo_rs_stage8_b_bink_play(&active_play,operation)==1&&
              active_play.ids.back()==69&&
              std::find(active_play.ids.begin(),active_play.ids.end(),67u)!=active_play.ids.end(),
              "active play refreshes window before native scope"))return 1;
    Trace off_render{};off_render.ready=false;
    a2fo_rs_stage8_b_bink_render(&off_render,operation);
    if(!check(off_render.ids==std::vector<std::uint32_t>{80,82},
              "inactive render calls original without viewport"))return 1;
    Trace no_armada{};no_armada.fail=90;
    if(!check(a2fo_rs_stage8_b_bink_preserve_shell(&no_armada,operation)==0&&
              no_armada.ids==std::vector<std::uint32_t>{90},
              "shell gate prevents raw state read"))return 1;
    Trace snapshot{};
    if(!check(a2fo_rs_stage8_b_bink_preserve_shell(&snapshot,operation)==1&&
              snapshot.ids==std::vector<std::uint32_t>{90,91,92,93,95},
              "shell output precedes atomic publication"))return 1;
    Trace existing{};
    if(!check(a2fo_rs_stage8_b_bink_seed_presentation(&existing,operation)==1&&
              existing.ids==std::vector<std::uint32_t>{100,101},
              "existing saved selection exits before FO memory"))return 1;
    Trace changed{};changed.refresh=true;changed.changed=true;changed.needs_mode=true;
    if(!check(a2fo_rs_stage8_b_bink_seed_presentation(&changed,operation)==1&&
              changed.ids.back()==129&&
              std::find(changed.ids.begin(),changed.ids.end(),125u)!=changed.ids.end(),
              "refresh resolves desktop mode and publishes changed selection"))return 1;
    Trace no_window{};no_window.fail=500;
    if(!check(a2fo_rs_stage8_b_bink_apply_window(&no_window,operation)==0&&
              no_window.ids==std::vector<std::uint32_t>{500},
              "null intro window prevents snapshot"))return 1;
    Trace cached{};cached.cached=true;
    if(!check(a2fo_rs_stage8_b_bink_apply_window(&cached,operation)==1&&
              cached.ids.back()==523,
              "matching cached geometry returns before style writes"))return 1;
    Trace resize{};
    if(!check(a2fo_rs_stage8_b_bink_apply_window(&resize,operation)==1&&
              resize.ids.back()==563&&
              std::find(resize.ids.begin(),resize.ids.end(),552u)!=resize.ids.end(),
              "unmatched intro window positions then republishes geometry"))return 1;
    Trace no_intro{};no_intro.fail=400;
    a2fo_rs_stage8_b_bink_prepare_viewport(&no_intro,operation);
    if(!check(no_intro.ids==std::vector<std::uint32_t>{400},
              "missing intro skips COM reads"))return 1;
    Trace no_target{};no_target.fail=411;
    a2fo_rs_stage8_b_bink_prepare_viewport(&no_target,operation);
    if(!check(no_target.ids.back()==411&&
              std::find(no_target.ids.begin(),no_target.ids.end(),410u)!=no_target.ids.end(),
              "swap chain released before missing target exit"))return 1;
    Trace gdi_off{};gdi_off.ready=false;
    a2fo_rs_stage8_b_bink_gdi(&gdi_off,operation);
    if(!check(gdi_off.ids==std::vector<std::uint32_t>{200,208},
              "inactive GDI preserves native SetDIBits path"))return 1;
    Trace gdi_scale{};
    a2fo_rs_stage8_b_bink_gdi(&gdi_scale,operation);
    if(!check(gdi_scale.ids.back()==225&&
              std::find(gdi_scale.ids.begin(),gdi_scale.ids.end(),219u)!=gdi_scale.ids.end(),
              "admitted GDI fits target before StretchDIBits"))return 1;
    Trace texture_off{};texture_off.ready=false;
    a2fo_rs_stage8_b_bink_texture(&texture_off,operation);
    if(!check(texture_off.ids==std::vector<std::uint32_t>{300,301},
              "inactive texture calls original before pointer admission"))return 1;
    Trace embedded{};
    a2fo_rs_stage8_b_bink_texture(&embedded,operation);
    if(!check(embedded.ids.back()==317&&
              std::find(embedded.ids.begin(),embedded.ids.end(),312u)!=embedded.ids.end()&&
              std::find(embedded.ids.begin(),embedded.ids.end(),313u)==embedded.ids.end(),
              "embedded movie bypasses fitted rectangle"))return 1;
    unsigned fw{},fh{};
    a2fo_rs_stage8_b_bink_fit_dimensions(640,480,1920,1080,&fw,&fh);
    if(!check(fw==1440&&fh==1080,"integer movie fit preserves aspect"))return 1;
    std::puts("bink policy: 22 branches PASS");
}
