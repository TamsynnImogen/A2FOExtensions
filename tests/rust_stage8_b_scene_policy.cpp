#include "../rust/stage8_b_scene_ffi.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

struct HandoverTrace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    bool accepted{},launch{},source{true},candidate{};
};
struct WorkerTrace {
    std::vector<std::uint32_t> ids;
    bool bridging{},stop{},changed{},departure{},active{true},pending{},rendered{true};
    std::uint32_t now{100},pause{33},cooldown{},completed{},failures{1},reports{};
    std::uint32_t timeout{};
};
struct PresentTrace {
    std::vector<std::uint32_t> ids;
    bool initial{},stop{},pending{true},image{true},drawn{true};
};
struct DestinationTrace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    bool previous{true},same{},token{true};
};
std::uint64_t destination_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& t=*static_cast<DestinationTrace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==409)return t.previous;
    if(id==410)return !t.same;
    if(id==413)return t.token;
    return 1;
}
struct EndTrace {
    std::vector<std::uint32_t> ids;
    std::vector<std::uint32_t> handles;
    bool parent{true},token{true},logger{true};
};
struct DrawTrace {
    std::vector<std::uint32_t> ids;
    std::vector<std::uint32_t> candidates;
    bool launch{},destination{true},source{true},parent{true},saved{true},drawn{true};
};
struct FrameTrace {
    std::vector<std::uint32_t> ids;
    bool image{true},same{true},accepted{true},resolver{true},window{true},registered{};
};
struct EraseTrace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    bool leased{true},property{},restored{true},drawn{true};
};
struct WindowTrace {
    std::vector<std::uint32_t> ids;
    bool timer{true},wanted{true},entry{true},match{true},presenting{true};
    bool mission{},visible{true},needs{true},closing{};
    std::uint32_t active_arg{UINT32_MAX};
};
struct LoadTrace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    bool cached{},clock{true};std::uint32_t log_arg{UINT32_MAX};
};
struct BackendTrace {
    std::vector<std::uint32_t> ids,args;
    std::uint32_t fail{UINT32_MAX};
    bool cached{},device{true},initialized{},departure{},pixels{true};
    std::uint32_t create_success{3},height{2};
};
struct PaintTrace {
    std::vector<std::uint32_t> ids,labels,chosen;
    bool wanted{true};
};
std::uint64_t paint_operation(void* opaque,std::uint32_t id,std::uint32_t arg){
    auto& t=*static_cast<PaintTrace*>(opaque);t.ids.push_back(id);
    if(id==1300)return t.wanted;
    if(id==1303)return 6;
    if(id==1304)return 0;
    if(id==1305)return 7;
    if(id==1306)return 3;
    if(id==1308)t.chosen.push_back(arg);
    if(id==1311)t.labels.push_back(arg);
    if(id==1312)return 1;
    return 1;
}
std::uint64_t backend_operation(void* opaque,std::uint32_t id,std::uint32_t arg){
    auto& t=*static_cast<BackendTrace*>(opaque);t.ids.push_back(id);
    if(id==1108||id==1229)t.args.push_back(arg);
    if(id==t.fail)return 0;
    if(id==1000)return t.cached;
    if(id==1101)return t.cached;
    if(id==1104)return 1;
    if(id==1108)return arg==t.create_success;
    if(id==1112)return t.initialized;
    if(id==1202)return t.device;
    if(id==1208||id==1214)return 0;
    if(id==1210)return t.departure;
    if(id==1226)return t.pixels;
    if(id==1232)return t.height;
    return 1;
}
std::uint64_t load_operation(void* opaque,std::uint32_t id,std::uint32_t arg){
    auto& t=*static_cast<LoadTrace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==900)return t.cached;
    if(id==905)return 1;
    if(id==911)return t.clock;
    if(id==912){t.log_arg=arg;return 0;}
    return 1;
}
std::uint64_t window_operation(void* opaque,std::uint32_t id,std::uint32_t arg){
    auto& t=*static_cast<WindowTrace*>(opaque);t.ids.push_back(id);
    if(id==800)return t.timer;
    if(id==802)return t.wanted;
    if(id==803||id==821)return t.entry;
    if(id==804)return t.match;
    if(id==805||id==808||id==817||id==825)return t.presenting;
    if(id==807)return !t.mission;
    if(id==809||id==810||id==811)return t.visible;
    if(id==812){t.active_arg=arg;return 0;}
    if(id==813)return t.needs;
    if(id==819)return t.closing;
    return 0;
}
std::uint64_t erase_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& t=*static_cast<EraseTrace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==703)return t.leased;
    if(id==704)return t.property;
    if(id==719)return t.restored;
    if(id==720)return t.drawn;
    return 1;
}
std::uint64_t frame_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& t=*static_cast<FrameTrace*>(opaque);t.ids.push_back(id);
    if(id==601||id==610||id==613||id==620)return t.image;
    if(id==602||id==614||id==622)return t.same;
    if(id==603||id==615)return t.accepted;
    if(id==624)return t.resolver;
    if(id==625)return t.window;
    if(id==626||id==628)return t.registered;
    if(id==627)t.registered=true;
    return 0;
}
std::uint64_t draw_operation(void* opaque,std::uint32_t id,std::uint32_t arg){
    auto& t=*static_cast<DrawTrace*>(opaque);t.ids.push_back(id);
    if(id==501)return t.launch;
    if(id==502){t.candidates.push_back(arg);
        return arg==0?t.destination:arg==1?t.source:t.parent;}
    if(id==511)return t.saved;
    if(id==513)return t.drawn;
    return 1;
}
std::uint64_t end_operation(void* opaque,std::uint32_t id,std::uint32_t arg){
    auto& t=*static_cast<EndTrace*>(opaque);t.ids.push_back(id);
    if(id==424){t.handles.push_back(arg);return 1;}
    if(id==421||id==428)return t.parent;
    if(id==425)return t.token;
    if(id==429)return t.logger;
    return 1;
}
std::uint64_t present_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& t=*static_cast<PresentTrace*>(opaque);t.ids.push_back(id);
    if(id==301)return t.stop;
    if(id==302)return t.initial;
    if(id==303)return t.pending;
    if(id==305)return t.image;
    if(id==309)return t.drawn;
    return 0;
}
std::uint64_t worker_operation(void* opaque,std::uint32_t id,std::uint32_t arg){
    auto& t=*static_cast<WorkerTrace*>(opaque);t.ids.push_back(id);
    switch(id){
    case 201:return t.bridging;
    case 204:return t.stop;
    case 205:return t.now;
    case 206:return t.changed;
    case 207:return t.departure;
    case 208:return 1;
    case 209:return t.active;
    case 210:return t.pause;
    case 211:return t.cooldown;
    case 212:return t.pending;
    case 213:return t.completed;
    case 215:return t.rendered;
    case 219:return t.failures;
    case 228:return t.reports;
    case 230:t.timeout=arg;return 0;
    default:return 0;
    }
}
std::uint64_t operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& t=*static_cast<HandoverTrace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==105)return t.accepted;
    if(id==106||id==120)return t.source;
    if(id==125||id==132)return t.launch;
    if(id==126)return t.candidate;
    return 1;
}
bool check(bool pass,const char* label){
    if(!pass)std::fprintf(stderr,"scene policy failure: %s\n",label);
    return pass;
}
int main(){
    HandoverTrace normal{};normal.candidate=true;
    if(!check(a2fo_rs_stage8_b_scene_handover(&normal,operation)==1&&
              normal.ids.front()==100&&normal.ids.back()==135&&
              std::find(normal.ids.begin(),normal.ids.end(),127u)!=normal.ids.end()&&
              std::find(normal.ids.begin(),normal.ids.end(),133u)==normal.ids.end(),
              "candidate handover and native publication order"))return 1;
    HandoverTrace absent_parent{};absent_parent.fail=103;
    if(!check(a2fo_rs_stage8_b_scene_handover(&absent_parent,operation)==0&&
              absent_parent.ids.back()==110&&
              std::find(absent_parent.ids.begin(),absent_parent.ids.end(),111u)==absent_parent.ids.end(),
              "window relation diagnostic precedes thread checks"))return 1;
    HandoverTrace accepted{};accepted.accepted=true;accepted.launch=true;accepted.source=false;
    if(!check(a2fo_rs_stage8_b_scene_handover(&accepted,operation)==1&&
              std::find(accepted.ids.begin(),accepted.ids.end(),112u)==accepted.ids.end()&&
              std::find(accepted.ids.begin(),accepted.ids.end(),133u)!=accepted.ids.end(),
              "accepted loading bypasses source and presents launch"))return 1;
    HandoverTrace prop_failure{};prop_failure.fail=121;
    if(!check(a2fo_rs_stage8_b_scene_handover(&prop_failure,operation)==0&&
              prop_failure.ids.back()==122,
              "source property failure rolls back parent"))return 1;
    HandoverTrace no_source{};no_source.source=false;
    if(!check(a2fo_rs_stage8_b_scene_handover(&no_source,operation)==0&&
              no_source.ids.back()==110,
              "ordinary handover requires source"))return 1;
    WorkerTrace stopped{};stopped.stop=true;
    if(!check(a2fo_rs_stage8_b_scene_worker_step(&stopped,worker_operation)==0&&
              stopped.ids==std::vector<std::uint32_t>{200,201,203,204},
              "worker stop before clock and device"))return 1;
    WorkerTrace rendered{};rendered.changed=true;
    if(!check(a2fo_rs_stage8_b_scene_worker_step(&rendered,worker_operation)==1&&
              rendered.ids.back()==229&&
              std::find(rendered.ids.begin(),rendered.ids.end(),227u)!=rendered.ids.end(),
              "worker publishes before timing report"))return 1;
    WorkerTrace retry{};retry.rendered=false;
    if(!check(a2fo_rs_stage8_b_scene_worker_step(&retry,worker_operation)==1&&
              retry.ids.back()==222,
              "worker resets and retries after render failure"))return 1;
    WorkerTrace unavailable{};unavailable.rendered=false;unavailable.failures=3;
    if(!check(a2fo_rs_stage8_b_scene_worker_step(&unavailable,worker_operation)==0&&
              unavailable.ids.back()==220,
              "worker stops after failure cap"))return 1;
    WorkerTrace sleeping{};sleeping.active=false;sleeping.completed=90;
    if(!check(a2fo_rs_stage8_b_scene_worker_step(&sleeping,worker_operation)==1&&
              sleeping.ids.back()==230&&sleeping.timeout==UINT32_MAX,
              "inactive worker waits without timeout"))return 1;
    WorkerTrace bridge_wait{};bridge_wait.bridging=true;bridge_wait.pending=true;
    if(!check(a2fo_rs_stage8_b_scene_worker_step(&bridge_wait,worker_operation)==1&&
              bridge_wait.timeout==16&&
              std::count(bridge_wait.ids.begin(),bridge_wait.ids.end(),202u)==1,
              "handover pending frame polls at 16 ms"))return 1;
    PresentTrace presented{};
    a2fo_rs_stage8_b_scene_present_handover(&presented,present_operation);
    if(!check(presented.ids.back()==310,
              "handover publishes after native draw"))return 1;
    PresentTrace no_image{};no_image.image=false;
    a2fo_rs_stage8_b_scene_present_handover(&no_image,present_operation);
    if(!check(no_image.ids.back()==305,
              "handover skips clock for missing image"))return 1;
    PresentTrace initial{};initial.initial=true;initial.pending=false;
    a2fo_rs_stage8_b_scene_present_handover(&initial,present_operation);
    if(!check(initial.ids.back()==310&&
              std::find(initial.ids.begin(),initial.ids.end(),303u)==initial.ids.end(),
              "initial handover bypasses pending gate"))return 1;
    PresentTrace not_drawn{};not_drawn.drawn=false;
    a2fo_rs_stage8_b_scene_present_handover(&not_drawn,present_operation);
    if(!check(not_drawn.ids.back()==309,
              "failed native draw preserves unpublished frame"))return 1;
    DestinationTrace destination{};
    a2fo_rs_stage8_b_scene_destination(&destination,destination_operation);
    if(!check(destination.ids.back()==415&&
              std::find(destination.ids.begin(),destination.ids.end(),414u)!=destination.ids.end(),
              "destination removes old property after publication"))return 1;
    DestinationTrace rejected_destination{};rejected_destination.fail=404;
    a2fo_rs_stage8_b_scene_destination(&rejected_destination,destination_operation);
    if(!check(rejected_destination.ids.back()==404,
              "stale destination leaves property untouched"))return 1;
    EndTrace ended{};
    a2fo_rs_stage8_b_scene_end_handover(&ended,end_operation);
    if(!check(ended.ids.front()==420&&ended.ids.back()==431&&
              ended.handles==std::vector<std::uint32_t>{0,1,2},
              "handover cleanup visits all three handles"))return 1;
    EndTrace no_parent{};no_parent.parent=false;no_parent.token=false;
    a2fo_rs_stage8_b_scene_end_handover(&no_parent,end_operation);
    if(!check(no_parent.ids.back()==431&&
              std::find(no_parent.ids.begin(),no_parent.ids.end(),423u)==no_parent.ids.end()&&
              std::find(no_parent.ids.begin(),no_parent.ids.end(),430u)==no_parent.ids.end(),
              "empty handover still wakes worker"))return 1;
    DrawTrace draw{};
    if(!check(a2fo_rs_stage8_b_scene_draw_handover(&draw,draw_operation)==1&&
              draw.candidates==std::vector<std::uint32_t>{0}&&draw.ids.back()==516,
              "destination draw flush restore release"))return 1;
    DrawTrace fallback{};fallback.destination=false;
    if(!check(a2fo_rs_stage8_b_scene_draw_handover(&fallback,draw_operation)==1&&
              fallback.candidates==std::vector<std::uint32_t>{0,1},
              "source fallback follows destination rejection"))return 1;
    DrawTrace launch{};launch.launch=true;
    if(!check(a2fo_rs_stage8_b_scene_draw_handover(&launch,draw_operation)==1&&
              launch.candidates==std::vector<std::uint32_t>{2},
              "launch draws only on parent"))return 1;
    DrawTrace save_failure{};save_failure.saved=false;
    if(!check(a2fo_rs_stage8_b_scene_draw_handover(&save_failure,draw_operation)==0&&
              save_failure.ids.back()==516&&
              std::find(save_failure.ids.begin(),save_failure.ids.end(),515u)==save_failure.ids.end(),
              "SaveDC failure still releases window DC"))return 1;
    DrawTrace draw_failure{};draw_failure.drawn=false;
    if(!check(a2fo_rs_stage8_b_scene_draw_handover(&draw_failure,draw_operation)==0&&
              draw_failure.ids.back()==516&&
              std::find(draw_failure.ids.begin(),draw_failure.ids.end(),514u)==draw_failure.ids.end()&&
              std::find(draw_failure.ids.begin(),draw_failure.ids.end(),515u)!=draw_failure.ids.end(),
              "draw failure restores DC without flush"))return 1;
    FrameTrace frame{};
    a2fo_rs_stage8_b_scene_frame_presented(&frame,frame_operation);
    if(!check(frame.ids==std::vector<std::uint32_t>{600,601,602,603,604,605},
              "frame publication after mailbox acceptance"))return 1;
    FrameTrace canvas{};
    a2fo_rs_stage8_b_scene_canvas_completed(&canvas,frame_operation);
    if(!check(canvas.ids.front()==610&&canvas.ids[1]==611&&
              canvas.ids.back()==617,
              "canvas pending erase before snapshot"))return 1;
    FrameTrace drawn_frame{};
    a2fo_rs_stage8_b_scene_frame_drawn(&drawn_frame,frame_operation);
    if(!check(drawn_frame.ids.back()==629&&drawn_frame.registered&&
              std::find(drawn_frame.ids.begin(),drawn_frame.ids.end(),630u)==drawn_frame.ids.end(),
              "shell completion registration before pending publication"))return 1;
    FrameTrace fallback_frame{};fallback_frame.window=false;
    a2fo_rs_stage8_b_scene_frame_drawn(&fallback_frame,frame_operation);
    if(!check(fallback_frame.ids.back()==630,
              "frame draw falls back to synchronous publication"))return 1;
    FrameTrace shutdown{};
    a2fo_rs_stage8_b_scene_shutdown(&shutdown,frame_operation);
    if(!check(shutdown.ids==std::vector<std::uint32_t>{640,641,642},
              "shutdown disables before stop and wake"))return 1;
    EraseTrace erased{};
    if(!check(a2fo_rs_stage8_b_scene_erase(&erased,erase_operation)==1&&
              erased.ids.back()==720,
              "active erase restores before draw success"))return 1;
    EraseTrace busy{};busy.leased=false;busy.property=true;
    if(!check(a2fo_rs_stage8_b_scene_erase(&busy,erase_operation)==1&&
              busy.ids.back()==704,
              "busy worker preserves property backed erase"))return 1;
    EraseTrace disabled{};disabled.fail=702;
    if(!check(a2fo_rs_stage8_b_scene_erase(&disabled,erase_operation)==0&&
              disabled.ids==std::vector<std::uint32_t>{700,701,702},
              "disabled erase leaves native fallback"))return 1;
    EraseTrace save_failed{};save_failed.fail=717;
    if(!check(a2fo_rs_stage8_b_scene_erase(&save_failed,erase_operation)==0&&
              save_failed.ids.back()==717,
              "failed SaveDC skips draw and restore"))return 1;
    EraseTrace restore_failed{};restore_failed.restored=false;
    if(!check(a2fo_rs_stage8_b_scene_erase(&restore_failed,erase_operation)==0&&
              restore_failed.ids.back()==719,
              "restore failure leaves native erase fallback"))return 1;
    WindowTrace timer{};
    if(!check(a2fo_rs_stage8_b_scene_window(&timer,window_operation)==1&&
              timer.ids.back()==814&&timer.active_arg==1,
              "timer redraw after visible mailbox frame"))return 1;
    WindowTrace hidden{};hidden.mission=true;
    if(!check(a2fo_rs_stage8_b_scene_window(&hidden,window_operation)==1&&
              hidden.active_arg==0&&hidden.ids.back()==812,
              "mission gate disables scene without redraw"))return 1;
    WindowTrace stale_timer{};stale_timer.presenting=false;
    if(!check(a2fo_rs_stage8_b_scene_window(&stale_timer,window_operation)==1&&
              stale_timer.ids.back()==805,
              "stale timer consumed before logging"))return 1;
    WindowTrace destroyed{};destroyed.timer=false;destroyed.closing=true;
    if(!check(a2fo_rs_stage8_b_scene_window(&destroyed,window_operation)==0&&
              destroyed.ids.back()==826,
              "window destruction clears presentation owner"))return 1;
    LoadTrace loaded{};
    if(!check(a2fo_rs_stage8_b_scene_backend_load(&loaded,load_operation)==1&&
              loaded.ids.back()==912&&loaded.log_arg==1,
              "provider pin precedes publication and clock diagnostic"))return 1;
    LoadTrace cached_provider{};cached_provider.cached=true;
    if(!check(a2fo_rs_stage8_b_scene_backend_load(&cached_provider,load_operation)==1&&
              cached_provider.ids==std::vector<std::uint32_t>{900},
              "cached provider skips symbol reads"))return 1;
    LoadTrace wrong_abi{};wrong_abi.fail=905;
    if(!check(a2fo_rs_stage8_b_scene_backend_load(&wrong_abi,load_operation)==0&&
              wrong_abi.ids.back()==905,
              "wrong provider ABI never pinned"))return 1;
    LoadTrace pin_failure{};pin_failure.fail=909;
    if(!check(a2fo_rs_stage8_b_scene_backend_load(&pin_failure,load_operation)==0&&
              pin_failure.ids.back()==909,
              "pin failure never publishes provider"))return 1;
    BackendTrace resized{};
    if(!check(a2fo_rs_stage8_b_scene_resize(&resized,backend_operation)==1&&
              resized.ids.back()==1009,
              "surface resize commits only after viewport"))return 1;
    BackendTrace partial_surface{};partial_surface.fail=1003;
    if(!check(a2fo_rs_stage8_b_scene_resize(&partial_surface,backend_operation)==0&&
              partial_surface.ids.back()==1003,
              "surface creation failure skips swap"))return 1;
    BackendTrace created{};
    if(!check(a2fo_rs_stage8_b_scene_backend_create(&created,backend_operation)==1&&
              created.args==std::vector<std::uint32_t>{0,1,2,3}&&
              created.ids.back()==1115,
              "device depth and vertex fallback order"))return 1;
    BackendTrace no_device{};no_device.fail=1106;
    if(!check(a2fo_rs_stage8_b_scene_backend_create(&no_device,backend_operation)==0&&
              no_device.ids.back()==1106,
              "missing D3D9 device skips attempts"))return 1;
    BackendTrace rendered_backend{};
    if(!check(a2fo_rs_stage8_b_scene_backend_render(&rendered_backend,backend_operation)==1&&
              rendered_backend.args==std::vector<std::uint32_t>{0,1}&&
              rendered_backend.ids.back()==1231,
              "readback copies rows before unlock and timing"))return 1;
    BackendTrace failed_draw{};failed_draw.fail=1218;
    if(!check(a2fo_rs_stage8_b_scene_backend_render(&failed_draw,backend_operation)==0&&
              failed_draw.ids.back()==1219,
              "failed provider draw still ends D3D scene"))return 1;
    BackendTrace invalid_pixels{};invalid_pixels.pixels=false;
    if(!check(a2fo_rs_stage8_b_scene_backend_render(&invalid_pixels,backend_operation)==0&&
              invalid_pixels.ids.back()==1231&&
              std::find(invalid_pixels.ids.begin(),invalid_pixels.ids.end(),1227u)==invalid_pixels.ids.end(),
              "invalid readback unlocks without pitch access"))return 1;
    A2foStage8SceneRect wide{},narrow{};
    a2fo_rs_stage8_b_scene_choice_rect(2,1200,900,100,200,5,&wide);
    a2fo_rs_stage8_b_scene_choice_rect(2,700,900,100,200,5,&narrow);
    if(!check(a2fo_rs_stage8_b_scene_page_count(6,5)==2&&
              a2fo_rs_stage8_b_scene_choice_count(6,0,5)==7&&
              wide.x==782&&wide.y==834&&wide.width==130&&wide.height==28&&
              narrow.x==578&&narrow.y==762&&narrow.width==110&&narrow.height==28,
              "choice pagination and wide/narrow geometry"))return 1;
    PaintTrace choices{};
    a2fo_rs_stage8_b_scene_paint_choices(&choices,paint_operation);
    if(!check(choices.ids.front()==1300&&choices.ids.back()==1314&&
              choices.labels==std::vector<std::uint32_t>{0,0,0,0,0,1,2}&&
              choices.chosen==std::vector<std::uint32_t>{0,0,0,1,0,0,0},
              "GDI choice loop selection and page arrows"))return 1;
    PaintTrace disabled_choices{};disabled_choices.wanted=false;
    a2fo_rs_stage8_b_scene_paint_choices(&disabled_choices,paint_operation);
    if(!check(disabled_choices.ids==std::vector<std::uint32_t>{1300},
              "disabled choice painter leaves HDC untouched"))return 1;
    std::puts("scene policy: 52 ordered branches PASS");
}
