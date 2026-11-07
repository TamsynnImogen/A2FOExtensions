#include "../rust/stage8_b_setup_ffi.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Trace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    bool existing{},retained{true},current{true},flushed{true},accepted{true};
};
std::uint64_t operation(void* opaque,std::uint32_t id){
    auto& t=*static_cast<Trace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==101)return t.existing;
    if(id==132)return t.retained;
    if(id==135)return t.current;
    if(id==140)return t.flushed;
    if(id==142)return t.accepted;
    return 1;
}
struct PresentTrace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    bool busy{},handover{},blit{true};
};
std::uint64_t present_operation(void* opaque,std::uint32_t id){
    auto& t=*static_cast<PresentTrace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==160)return t.busy;
    if(id==161)return t.handover;
    if(id==182)return t.blit;
    if(id==191)return 0;
    return 1;
}
struct WindowTrace {
    std::vector<std::uint32_t> ids;
    bool handover{},paint{},wheel{},pointer{},release{};
};
std::uint64_t window_operation(void* opaque,std::uint32_t id){
    auto& t=*static_cast<WindowTrace*>(opaque);t.ids.push_back(id);
    switch(id){
    case 200:case 202:case 204:case 220:case 230:case 232:return 0;
    case 206:return t.handover;
    case 207:return t.paint;
    case 209:case 210:case 212:case 214:case 216:case 223:case 224:case 229:return 1;
    case 213:return t.wheel;
    case 217:return t.pointer;
    case 222:return t.release;
    default:return 1;
    }
}
struct IndexedTrace {
    std::vector<std::uint32_t> ids;
    std::vector<int> hidden{1,0};
    bool visible{true},show_success{true},hide_success{true},nested_restore{};
};
std::uint64_t indexed_operation(void* opaque,std::uint32_t id,std::uintptr_t index){
    auto& t=*static_cast<IndexedTrace*>(opaque);t.ids.push_back(id);
    switch(id){
    case 300:case 310:return t.hidden.size();
    case 301:case 314:return t.hidden[index];
    case 302:t.hidden[index]=0;return 0;
    case 303:case 304:case 306:case 307:return 1;
    case 305:return t.show_success;
    case 308:t.hidden[index]=1;return 0;
    case 311:return t.visible;
    case 312:case 315:t.hidden[index]=1;return 0;
    case 313:if(t.nested_restore)t.hidden[index]=0;return t.hide_success;
    default:return 1;
    }
}
struct SnapshotTrace {
    std::vector<std::uint32_t> ids;
    bool admitted{true},editable{},valid{true};
    std::uint64_t key{7},opened{};
};
struct CanvasTrace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    bool bridge{},active{true},requested{true},restored{true},drawn{true};
};
struct AdmissionTrace {
    std::vector<std::uint32_t> ids;
    std::uint64_t size{},present{},seen{};
};
std::uint64_t admission_operation(void* opaque,std::uint32_t id){
    auto& t=*static_cast<AdmissionTrace*>(opaque);t.ids.push_back(id);
    if(id==600)return t.size;
    if(id==601)return t.present;
    if(id==602)return t.seen;
    return 0;
}
struct WidgetTrace {
    std::vector<std::uint32_t> ids;
    bool owner{true},wanted{true};std::uint64_t size{};
};
struct ControlTrace {
    std::vector<std::uint32_t> ids;
    bool label{},combo{},open{};
    std::int32_t count{2},selected{-1},height{18};
    std::uint32_t fail{UINT32_MAX};
};
std::uint64_t control_operation(void* opaque,std::uint32_t id){
    auto& t=*static_cast<ControlTrace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==801)return 100;
    if(id==802)return 10;
    if(id==803)return 200;
    if(id==804)return 300;
    if(id==806)return t.combo;
    if(id==807)return 1;
    if(id==808)return t.label;
    if(id==814)return 0;
    if(id==824)return static_cast<std::uint32_t>(t.count);
    if(id==825)return static_cast<std::uint32_t>(t.selected);
    if(id==828)return t.open;
    if(id==829)return static_cast<std::uint32_t>(t.height);
    return 1;
}
std::uint64_t widget_operation(void* opaque,std::uint32_t id){
    auto& t=*static_cast<WidgetTrace*>(opaque);t.ids.push_back(id);
    if(id==700)return t.owner;
    if(id==701)return t.wanted;
    if(id==703)return t.size;
    return 0;
}
std::uint64_t canvas_operation(void* opaque,std::uint32_t id){
    auto& t=*static_cast<CanvasTrace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==521)return 0;
    if(id==523)return t.bridge;
    if(id==524)return t.active;
    if(id==528)return t.requested;
    if(id==516||id==533)return t.restored;
    if(id==517||id==534)return t.drawn;
    return 1;
}
std::uint64_t snapshot_operation(void* opaque,std::uint32_t id){
    auto& t=*static_cast<SnapshotTrace*>(opaque);t.ids.push_back(id);
    if(id==400)return t.admitted;
    if(id==408)return t.editable;
    if(id==410)return t.key;
    if(id==411)return t.opened;
    if(id==415)return t.valid;
    return 1;
}
bool has(const Trace& t,std::uint32_t id){
    return std::find(t.ids.begin(),t.ids.end(),id)!=t.ids.end();
}
bool check(bool okay,const char* label){
    if(!okay)std::fprintf(stderr,"setup policy failure: %s\n",label);
    return okay;
}
int main(){
    Trace admitted{};
    a2fo_rs_stage8_b_setup_observe(&admitted,operation);
    if(!check(admitted.ids.front()==100&&admitted.ids.back()==126&&
              has(admitted,106)&&has(admitted,118)&&has(admitted,124),
              "admission publication"))return 1;
    Trace duplicate{};duplicate.existing=true;
    a2fo_rs_stage8_b_setup_observe(&duplicate,operation);
    if(!check(duplicate.ids==std::vector<std::uint32_t>{100,101},
              "duplicate skips callbacks"))return 1;
    Trace foreign{};foreign.fail=102;
    a2fo_rs_stage8_b_setup_observe(&foreign,operation);
    if(!check(foreign.ids.back()==103&&!has(foreign,106),
              "foreign thread keeps prior snapshot"))return 1;
    Trace presenter{};presenter.fail=119;
    a2fo_rs_stage8_b_setup_observe(&presenter,operation);
    if(!check(presenter.ids.back()==120&&has(presenter,118)&&!has(presenter,124),
              "failed presenter rollback"))return 1;
    Trace empty{};empty.retained=false;
    if(!check(a2fo_rs_stage8_b_setup_handover(&empty,operation)==0&&
              empty.ids.back()==133,"missing snapshot"))return 1;
    Trace stale{};stale.current=false;
    if(!check(a2fo_rs_stage8_b_setup_handover(&stale,operation)==0&&
              stale.ids.back()==138&&!has(stale,139),"stale snapshot cleanup"))return 1;
    Trace no_flush{};no_flush.flushed=false;
    if(!check(a2fo_rs_stage8_b_setup_handover(&no_flush,operation)==0&&
              no_flush.ids.back()==140&&!has(no_flush,143),
              "flush failure retains snapshot"))return 1;
    Trace accepted{};
    if(!check(a2fo_rs_stage8_b_setup_handover(&accepted,operation)==1&&
              accepted.ids[accepted.ids.size()-2]==143&&accepted.ids.back()==144,
              "accepted handover"))return 1;
    Trace rejected{};rejected.accepted=false;
    if(!check(a2fo_rs_stage8_b_setup_handover(&rejected,operation)==0&&
              rejected.ids.back()==143&&!has(rejected,144),
              "scene rejection clears snapshot"))return 1;
    Trace shutdown{};
    a2fo_rs_stage8_b_setup_shutdown(&shutdown,operation);
    if(!check(shutdown.ids==std::vector<std::uint32_t>{150,151,152,153,154,155,
              156,157,158,159},"shutdown owner cleanup"))return 1;
    PresentTrace painted{};
    if(!check(a2fo_rs_stage8_b_setup_present(&painted,present_operation)==1&&
              painted.ids.back()==195&&
              std::find(painted.ids.begin(),painted.ids.end(),183u)!=painted.ids.end(),
              "present complete"))return 1;
    PresentTrace handover{};handover.handover=true;
    if(!check(a2fo_rs_stage8_b_setup_present(&handover,present_operation)==1&&
              handover.ids==std::vector<std::uint32_t>{160,161},
              "launch handover"))return 1;
    PresentTrace snapshot{};snapshot.fail=169;
    if(!check(a2fo_rs_stage8_b_setup_present(&snapshot,present_operation)==0&&
              snapshot.ids.back()==170,"snapshot fallback"))return 1;
    PresentTrace hide{};hide.fail=179;
    if(!check(a2fo_rs_stage8_b_setup_present(&hide,present_operation)==0&&
              hide.ids.back()==180,"hide failure"))return 1;
    PresentTrace blit{};blit.blit=false;
    if(!check(a2fo_rs_stage8_b_setup_present(&blit,present_operation)==0&&
              std::find(blit.ids.begin(),blit.ids.end(),188u)!=blit.ids.end()&&
              std::find(blit.ids.begin(),blit.ids.end(),192u)==blit.ids.end(),
              "blit fallback"))return 1;
    WindowTrace forwarded{};
    if(!check(a2fo_rs_stage8_b_setup_window_active(&forwarded,window_operation)==0&&
              std::find(forwarded.ids.begin(),forwarded.ids.end(),228u)!=forwarded.ids.end(),
              "native event forwarded"))return 1;
    WindowTrace paint_event{};paint_event.handover=true;paint_event.paint=true;
    if(!check(a2fo_rs_stage8_b_setup_window_active(&paint_event,window_operation)==1&&
              paint_event.ids.back()==208,"handover paint consumed"))return 1;
    WindowTrace wheel{};wheel.wheel=true;
    if(!check(a2fo_rs_stage8_b_setup_window_active(&wheel,window_operation)==1&&
              wheel.ids.back()==215,"wheel redraw"))return 1;
    WindowTrace release{};release.pointer=true;release.release=true;
    if(!check(a2fo_rs_stage8_b_setup_window_active(&release,window_operation)==0&&
              std::find(release.ids.begin(),release.ids.end(),225u)!=release.ids.end()&&
              std::find(release.ids.begin(),release.ids.end(),226u)!=release.ids.end()&&
              std::find(release.ids.begin(),release.ids.end(),228u)!=release.ids.end(),
              "release snapshot before native action"))return 1;
    IndexedTrace restored{};restored.show_success=false;
    a2fo_rs_stage8_b_setup_restore_logs(&restored,indexed_operation);
    if(!check(restored.hidden[0]&&
              std::find(restored.ids.begin(),restored.ids.end(),308u)!=restored.ids.end(),
              "failed show keeps ownership"))return 1;
    IndexedTrace nested{};nested.hidden={0};nested.nested_restore=true;
    if(!check(a2fo_rs_stage8_b_setup_hide_logs(&nested,indexed_operation)==0&&
              nested.ids[nested.ids.size()-2]==315&&nested.ids.back()==316,
              "reentrant hide fallback"))return 1;
    IndexedTrace invisible{};invisible.hidden={0};invisible.visible=false;
    if(!check(a2fo_rs_stage8_b_setup_hide_logs(&invisible,indexed_operation)==1&&
              std::find(invisible.ids.begin(),invisible.ids.end(),313u)==invisible.ids.end(),
              "invisible child stays native"))return 1;
    SnapshotTrace complete{};
    if(!check(a2fo_rs_stage8_b_setup_snapshot(&complete,snapshot_operation)==1&&
              complete.ids.back()==416&&
              std::find(complete.ids.begin(),complete.ids.end(),412u)!=complete.ids.end(),
              "snapshot preview publication"))return 1;
    SnapshotTrace interactive{};interactive.editable=true;
    if(!check(a2fo_rs_stage8_b_setup_snapshot(&interactive,snapshot_operation)==0&&
              interactive.ids.back()==409,"interactive child rejection"))return 1;
    SnapshotTrace invalid_geometry{};invalid_geometry.valid=false;
    if(!check(a2fo_rs_stage8_b_setup_snapshot(&invalid_geometry,snapshot_operation)==0&&
              invalid_geometry.ids.back()==418,"incomplete model diagnostic"))return 1;
    SnapshotTrace absent_bridge{};absent_bridge.admitted=false;
    if(!check(a2fo_rs_stage8_b_setup_snapshot(&absent_bridge,snapshot_operation)==0&&
              absent_bridge.ids==std::vector<std::uint32_t>{400},
              "bridge gate before canvas read"))return 1;
    CanvasTrace seeded{};
    if(!check(a2fo_rs_stage8_b_setup_seed(&seeded,canvas_operation)==1&&
              seeded.ids.front()==500&&seeded.ids.back()==517,
              "seed performs ordered draw and restore"))return 1;
    CanvasTrace seed_gate{};seed_gate.fail=501;
    if(!check(a2fo_rs_stage8_b_setup_seed(&seed_gate,canvas_operation)==0&&
              seed_gate.ids==std::vector<std::uint32_t>{500,501},
              "seed skips inactive canvas"))return 1;
    CanvasTrace seed_save{};seed_save.fail=514;
    if(!check(a2fo_rs_stage8_b_setup_seed(&seed_save,canvas_operation)==0&&
              seed_save.ids.back()==514,
              "seed failed SaveDC has no draw"))return 1;
    CanvasTrace seed_restore{};seed_restore.restored=false;
    if(!check(a2fo_rs_stage8_b_setup_seed(&seed_restore,canvas_operation)==0&&
              seed_restore.ids.back()==517,
              "seed checks draw after restore failure"))return 1;
    CanvasTrace erased{};erased.bridge=true;
    if(!check(a2fo_rs_stage8_b_setup_erase(&erased,canvas_operation)==1&&
              erased.ids.back()==534&&
              std::find(erased.ids.begin(),erased.ids.end(),527u)==erased.ids.end(),
              "erase uses retained frame during bridge"))return 1;
    CanvasTrace erased_direct{};
    if(!check(a2fo_rs_stage8_b_setup_erase(&erased_direct,canvas_operation)==1&&
              erased_direct.ids.back()==527,
              "erase uses direct native blit"))return 1;
    CanvasTrace erase_gate{};erase_gate.fail=522;
    if(!check(a2fo_rs_stage8_b_setup_erase(&erase_gate,canvas_operation)==0&&
              erase_gate.ids.back()==522,
              "erase missing client rect skips state"))return 1;
    CanvasTrace missing_request{};missing_request.active=false;missing_request.requested=false;
    if(!check(a2fo_rs_stage8_b_setup_erase(&missing_request,canvas_operation)==0&&
              missing_request.ids.back()==528,
              "erase requested gate precedes retained frame"))return 1;
    AdmissionTrace capped{};capped.size=64;
    a2fo_rs_stage8_b_setup_admission(&capped,admission_operation,4);
    if(!check(capped.ids==std::vector<std::uint32_t>{600,601},
              "admission cap precedes insertion"))return 1;
    AdmissionTrace repeat{};repeat.seen=4;
    a2fo_rs_stage8_b_setup_admission(&repeat,admission_operation,4);
    if(!check(repeat.ids==std::vector<std::uint32_t>{600,602},
              "admission repeated reason quiet"))return 1;
    AdmissionTrace admitted_reason{};
    a2fo_rs_stage8_b_setup_admission(&admitted_reason,admission_operation,4);
    if(!check(admitted_reason.ids==std::vector<std::uint32_t>{600,602,603,604},
              "admission records before logging"))return 1;
    WidgetTrace foreign_widget{};foreign_widget.owner=false;
    a2fo_rs_stage8_b_setup_widget(&foreign_widget,widget_operation);
    if(!check(foreign_widget.ids==std::vector<std::uint32_t>{700},
              "foreign widget skips state"))return 1;
    WidgetTrace capped_widget{};capped_widget.size=4096;
    a2fo_rs_stage8_b_setup_widget(&capped_widget,widget_operation);
    if(!check(capped_widget.ids==std::vector<std::uint32_t>{700,701,702,703},
              "widget trims before cap check"))return 1;
    WidgetTrace captured_widget{};
    a2fo_rs_stage8_b_setup_widget(&captured_widget,widget_operation);
    if(!check(captured_widget.ids==std::vector<std::uint32_t>{700,701,702,703,704},
              "widget registers after original thunk"))return 1;
    ControlTrace label_control{};label_control.label=true;
    if(!check(a2fo_rs_stage8_b_setup_control(&label_control,control_operation)==1&&
              label_control.ids.back()==814&&
              std::find(label_control.ids.begin(),label_control.ids.end(),816u)==label_control.ids.end(),
              "label reads full native bytes only"))return 1;
    ControlTrace button_control{};
    if(!check(a2fo_rs_stage8_b_setup_control(&button_control,control_operation)==1&&
              button_control.ids.back()==818,
              "button skips combo window reads"))return 1;
    ControlTrace combo_control{};combo_control.combo=true;combo_control.open=true;
    if(!check(a2fo_rs_stage8_b_setup_control(&combo_control,control_operation)==1&&
              std::count(combo_control.ids.begin(),combo_control.ids.end(),831u)==2&&
              std::find(combo_control.ids.begin(),combo_control.ids.end(),807u)==combo_control.ids.end(),
              "open combo traverses items"))return 1;
    ControlTrace invalid_count{};invalid_count.combo=true;invalid_count.count=-1;
    if(!check(a2fo_rs_stage8_b_setup_control(&invalid_count,control_operation)==0&&
              invalid_count.ids.back()==825,
              "combo reads selection before count validation"))return 1;
    ControlTrace no_bounds{};no_bounds.combo=true;no_bounds.open=true;no_bounds.fail=830;
    if(!check(a2fo_rs_stage8_b_setup_control(&no_bounds,control_operation)==0&&
              no_bounds.ids.back()==830,
              "combo invalid pull bounds stops item traversal"))return 1;
    std::puts("setup policy: 44 ordered branches PASS");
}
