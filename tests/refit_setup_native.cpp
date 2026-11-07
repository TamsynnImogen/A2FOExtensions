// Headless, read-only widget decoder and constructor ABI fixture. The only
// windows are private hidden controls; no game entrypoint or input is invoked.
#include <windows.h>
BOOL WINAPI fixture_visible(HWND);
BOOL WINAPI fixture_position(HWND,HWND,int,int,int,int,UINT);
#define IsWindowVisible fixture_visible
#define SetWindowPos fixture_position
#include "../modules/A2FORefitMenus/setup_native.cpp"
#undef IsWindowVisible
#undef SetWindowPos

#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace native = a2fo::refit_menu::setup_native;
namespace setup = a2fo::refit_menu::setup;
namespace {
bool scene_requested{}, mission_active{};
unsigned scene_frame_calls{};
bool handover_running{},captured_during_handover{};
unsigned accepted_handover_calls{};
std::shared_ptr<const a2fo::refit_menu::Artwork> retained_scene;
std::shared_ptr<const a2fo::refit_menu::Artwork> completed_handover_scene;
}
namespace a2fo::refit_menu::loading_native {
void remember_setup_preview(const Artwork&) {}
}
namespace a2fo::refit_menu::menu_scene {
bool launch_handover_active(){return false;}
bool begin_launch_handover(HWND,HWND,std::shared_ptr<const Artwork>){return false;}
bool begin_accepted_loading_handover(HWND,std::shared_ptr<const Artwork> image){
    if(!image||image->pixels.empty())return false;
    ++accepted_handover_calls;return true;
}
bool handover_active(){return handover_running;}
bool handover_active_for_erase(){return handover_running;}
void end_departure(){}
void handover_destination(HWND){}
void end_handover(){if(handover_running){retained_scene=completed_handover_scene;handover_running=false;}}
const Artwork* frame(HWND, bool, bool, bool) {
    ++scene_frame_calls;captured_during_handover|=handover_running;
    return scene_requested?retained_scene.get():nullptr;
}
void frame_presented(const Artwork*) {}
void frame_drawn(const Artwork*,HDC) {}
bool requested() { return scene_requested; }
std::shared_ptr<const Artwork> retained_frame() { return retained_scene; }
}
namespace a2fo::refit_menu::mission_backdrop {
bool active() { return mission_active; }
}

namespace {
HWND visible_fixture_child{},snapshot_parent{},present_fixture_parent{};
native::Session* visibility_session{};
unsigned hide_calls{},show_calls{};
bool benchmark_setup{};
bool fail_hide{},fail_show{},restore_during_hide{},ownership_missing{};
}
BOOL WINAPI fixture_visible(HWND window){return window==visible_fixture_child||window==present_fixture_parent?TRUE:IsWindowVisible(window);}
BOOL WINAPI fixture_position(HWND window,HWND after,int x,int y,int w,int h,UINT flags){
    // Intercept visibility operations only. Never reveal any fixture HWND.
    if(flags&(SWP_SHOWWINDOW|SWP_HIDEWINDOW)){
        if(!visibility_session||window!=visible_fixture_child)return FALSE;
        if(flags&SWP_SHOWWINDOW){++show_calls;return fail_show?FALSE:TRUE;}
        ++hide_calls;
        ownership_missing|=visibility_session->logs.empty()||!visibility_session->logs[0].hidden_by_us;
        if(restore_during_hide)native::restore_logs(*visibility_session);
        return fail_hide?FALSE:TRUE;
    }
    return SetWindowPos(window,after,x,y,w,h,flags);
}
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class T, std::size_t N> void put(std::array<BYTE,N>& bytes, std::size_t offset, T value) {
    require(offset + sizeof(value) <= bytes.size(), "fixture field bounds");
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}
template<class T, std::size_t N> T get(const std::array<BYTE,N>& bytes, std::size_t offset) {
    require(offset + sizeof(T) <= bytes.size(), "fixture field bounds");
    T value{};
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}
void rectangle(std::array<BYTE,0x500>& bytes, std::size_t offset,
               int left, int top, int right, int bottom) {
    put(bytes, offset, left); put(bytes, offset+4, top);
    put(bytes, offset+8, right); put(bytes, offset+12, bottom);
}
HWND hidden(const char* name, DWORD style, HWND parent = nullptr,int id=0) {
    HWND window = CreateWindowExA(WS_EX_NOACTIVATE, name, "", style,
        0, 0, 160, 120, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), GetModuleHandleA(nullptr), nullptr);
    require(window != nullptr, "hidden fixture control created");
    require(!IsWindowVisible(window), "fixture control remains hidden");
    return window;
}
void check_early_seed(HWND dialog) {
    HWND wrong_class=hidden("BUTTON",WS_POPUP);
    HDC dc=CreateCompatibleDC(nullptr);
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=8;info.bmiHeader.biHeight=-6;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    info.bmiHeader.biCompression=BI_RGB;
    void* pixels{};
    HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    require(dc&&bitmap&&pixels,"private seed destination allocated");
    HGDIOBJ previous=SelectObject(dc,bitmap);
    require(previous&&previous!=HGDI_ERROR,"private seed bitmap selected");
    auto artwork=std::make_shared<a2fo::refit_menu::Artwork>();
    artwork->width=2;artwork->height=2;
    artwork->pixels={0x00ff0000u,0x0000ff00u,0x000000ffu,0x00ffffffu};
    retained_scene=artwork;
    scene_requested=true;native::wanted.store(true);
    require(!IsWindowVisible(dialog)&&!IsWindowVisible(wrong_class),
            "seed fixture windows remain hidden");
    require(PatBlt(dc,0,0,8,6,BLACKNESS)!=FALSE,"seed destination cleared");
    require(native::seed_canvas(dialog,dc,8,6,nullptr)!=FALSE,
            "admitted dialog receives retained scene");
    require(GetPixel(dc,1,1)==RGB(255,0,0)&&GetPixel(dc,6,1)==RGB(0,255,0)&&
            GetPixel(dc,1,4)==RGB(0,0,255)&&GetPixel(dc,6,4)==RGB(255,255,255),
            "retained scene pixels scale to logical canvas");
    require(scene_frame_calls==0,"early seed does not request live scene frame");
    POINT original{},after{};
    require(SetViewportOrgEx(dc,2,1,&original)!=FALSE,"fixture viewport shifted");
    const int original_mode=SetStretchBltMode(dc,HALFTONE);
    require(original_mode!=0&&native::seed_canvas(dialog,dc,8,6,nullptr)!=FALSE,
            "seed succeeds with caller DC state");
    require(GetViewportOrgEx(dc,&after)!=FALSE&&after.x==2&&after.y==1&&
            GetStretchBltMode(dc)==HALFTONE,"seed restores viewport and stretch mode");
    require(SetViewportOrgEx(dc,original.x,original.y,nullptr)!=FALSE&&
            SetStretchBltMode(dc,original_mode)!=0,"fixture DC state restored");
    for(int rejected=0;rejected<4;++rejected) {
        require(PatBlt(dc,0,0,8,6,BLACKNESS)!=FALSE,"fallback destination cleared");
        HWND candidate=dialog;
        if(rejected==0)candidate=wrong_class;
        if(rejected==1)native::wanted.store(false);
        if(rejected==2)retained_scene.reset();
        if(rejected==3)mission_active=true;
        require(!native::seed_canvas(candidate,dc,8,6,nullptr)&&
                GetPixel(dc,4,3)==RGB(0,0,0),"nonadmitted seed keeps native pixels");
        native::wanted.store(true);retained_scene=artwork;mission_active=false;
    }
    require(scene_frame_calls==0,"seed fallback never calls live frame");
    scene_requested=false;retained_scene.reset();native::wanted.store(false);
    SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);
    DestroyWindow(wrong_class);
}
void check_combo(HWND parent, HWND combo) {
    std::array<BYTE,0x500> bytes{};
    put(bytes, 0, reinterpret_cast<std::uintptr_t>(native::at(0x2bad2c)));
    put(bytes, 0x20, parent);
    put(bytes, 0x198, combo);
    put(bytes, 0xe4, std::uint32_t{1});
    put(bytes, 0xe8, std::uint32_t{1});
    put(bytes, 0xc0, std::uint32_t{2});
    put(bytes, 0x190, BYTE{1});
    put(bytes, 0x1b4, BYTE{1});
    put(bytes, 0x17c, 18);
    rectangle(bytes, 0xc4, 21, 33, 221, 65);
    rectangle(bytes, 0x180, 21, 65, 221, 145);
    const native::Widget widget{bytes.data(), parent};
    setup::Control control;
    require(native::control(widget, control), "real combo VMT decoded");
    require(control.kind == setup::Kind::combo && control.source.x == 21 &&
        control.source.y == 33 && control.source.width == 200 && control.source.height == 32,
        "combo source rectangle");
    require(control.visible && control.enabled && control.mode == 2 && control.open &&
        control.down && control.item_height == 18, "combo flags and item height");
    require(control.pull.x == 21 && control.pull.y == 65 &&
        control.pull.width == 200 && control.pull.height == 80, "combo pull rectangle");
    require(control.selected == 1 && control.text == "Borg" &&
        control.items.size() == 2 && control.items[0] == "Federation" &&
        control.items[1] == "Borg", "combo selected text and items");

    put(bytes, 0x190, BYTE{0});
    control = {};
    require(native::control(widget, control) && !control.open &&
        control.items.empty() && control.text == "Borg", "closed combo retains selected text");
    put(bytes, 0x190, BYTE{1});
    put(bytes, 0x198, reinterpret_cast<HWND>(0x1234));
    require(!native::control(widget, control), "invalid combo HWND rejected");
    put(bytes, 0x198, combo);
    HWND wrong_class = hidden("BUTTON", WS_CHILD, parent);
    put(bytes, 0x198, wrong_class);
    require(!native::control(widget, control), "non-combo child rejected");
    DestroyWindow(wrong_class);
    put(bytes, 0x198, combo);
    rectangle(bytes, 0xc4, 30, 33, 30, 65);
    require(!native::control(widget, control), "empty source rectangle rejected");
    rectangle(bytes, 0xc4, 21, 33, 221, 65);
    put(bytes, 0, reinterpret_cast<std::uintptr_t>(native::at(0x2bb2d4)));
    require(!native::control(widget, control), "base widget VMT rejected");
}
void check_label(HWND parent) {
    std::array<BYTE,0x500> bytes{};
    put(bytes, 0, reinterpret_cast<std::uintptr_t>(native::at(0x2bbaa4)));
    put(bytes, 4, parent);
    put(bytes, 0x45c, std::uint32_t{1});
    rectangle(bytes, 0x408, 10, 20, 110, 40);
    std::memcpy(bytes.data()+8, "Map Size", 9);
    const native::Widget widget{bytes.data(), parent};
    setup::Control control;
    require(native::control(widget, control) && control.kind == setup::Kind::label &&
        control.visible && control.text == "Map Size" && control.source.width == 100,
        "real label VMT decoded");
    std::memcpy(bytes.data()+8, "NULL", 5);
    require(native::control(widget, control) && control.text.empty(), "NULL label safe");
    put(bytes, 4, reinterpret_cast<HWND>(0x1234));
    require(!native::control(widget, control), "label HWND mismatch rejected");
}
void check_ctor(HWND first, HWND second) {
    std::array<BYTE,21> actual{};
    require(native::read(native::at(native::ctor_rva), actual.data(), actual.size()) &&
        actual == native::ctor_bytes, "full pinned constructor bytes");
    native::owner_thread = GetCurrentThreadId();
    native::wanted.store(true);
    native::next_ctor.store(native::at(native::ctor_rva));
    std::array<BYTE,0x500> bytes{};
    auto* self = bytes.data();
    require(native::widget_ctor(self, nullptr, first) == self, "native ctor return self");
    require(get<std::uintptr_t>(bytes, 0) ==
        std::uintptr_t{0x6bb2d4} &&
        get<HWND>(bytes, 4) == first, "native ctor VMT and HWND fields");
    require(native::widgets.size() == 1 && native::widgets[0].object == self &&
        native::widgets[0].window == first, "observer remembers constructed widget");
    require(native::widget_ctor(self, nullptr, second) == self &&
        native::widgets.size() == 1 && native::widgets[0].window == second &&
        get<HWND>(bytes, 4) == second,
        "duplicate address replaces observer record");
    native::widgets.clear();
    native::wanted.store(false);
}
BOOL __cdecl snapshot_bridge(HWND window){return window==snapshot_parent;}
BOOL __cdecl snapshot_canvas(HWND window,HDC destination,int width,int height,int* w,int* h){
    if(window!=snapshot_parent||!w||!h)return FALSE;
    *w=1200;*h=900;
    return !destination||(width>=1200&&height>=900&&PatBlt(destination,0,0,1200,900,BLACKNESS));
}
void check_complete_snapshot(HWND parent){
    snapshot_parent=parent;
    require(SetWindowPos(parent,nullptr,0,0,1920,1080,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE,
            "hidden parent physical size");
    std::vector<std::array<BYTE,0x500>> objects(26);
    native::widgets.clear();
    for(int i=0;i<24;++i){
        auto& bytes=objects[i];const int row=i/3,col=i%3;
        HWND combo=hidden("COMBOBOX",WS_CHILD|CBS_DROPDOWNLIST,parent);
        require(SendMessageA(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>("Selected"))==0&&
                SendMessageA(combo,CB_SETCURSEL,0,0)==0,"full model combo properties");
        put(bytes,0,reinterpret_cast<std::uintptr_t>(native::at(0x2bad2c)));
        put(bytes,0x20,parent);put(bytes,0x198,combo);
        put(bytes,0xe4,std::uint32_t{1});put(bytes,0xe8,std::uint32_t{1});
        rectangle(bytes,0xc4,50+125*col,80+22*row,150+125*col,100+22*row);
        native::widgets.push_back({bytes.data(),parent});
    }
    for(int i=24;i<26;++i){
        auto& bytes=objects[i];put(bytes,0,reinterpret_cast<std::uintptr_t>(native::at(0x2bb0e4)));
        put(bytes,0x20,parent);put(bytes,0xe4,std::uint32_t{1});put(bytes,0xe8,std::uint32_t{1});
        std::memcpy(bytes.data()+0x3c,i==24?"Previous Menu":"Launch",i==24?14:7);
        rectangle(bytes,0x178,14+(i-24)*500,570,150+(i-24)*500,595);
        native::widgets.push_back({bytes.data(),parent});
    }
    HWND status=hidden("EDIT",WS_CHILD|ES_READONLY|ES_MULTILINE|ES_AUTOVSCROLL,parent,2120);
    require(SetWindowTextA(status,"Status from native right pane")!=FALSE,"status text property");
    require(SetWindowPos(status,nullptr,1637,613,240,252,SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE,"right status physical rectangle");
    visible_fixture_child=status; // Metadata stub only; real HWND remains hidden.
    require(!IsWindowVisible(parent)&&!IsWindowVisible(status),"full snapshot windows remain hidden");
    native::Session session;session.window=parent;
    native::bridge_ready=snapshot_bridge;native::copy_canvas=snapshot_canvas;
    require(native::snapshot(session),"full setup snapshot accepts native right status pane");
    require(session.geometry.valid&&session.widget_width==800&&session.canvas_width==1200,
            "full snapshot retains independent widget and canvas coordinates");
    require(session.logs.size()==1,"read-only status pane retained");
    const auto message=std::find_if(session.model.controls.begin(),session.model.controls.end(),
                                  [](const auto& c){return c.text=="Status from native right pane";});
    require(message!=session.model.controls.end()&&message->message,"full snapshot carries native status text as message");
    const auto placement=std::find_if(session.geometry.controls.begin(),session.geometry.controls.end(),
                                    [&](const auto& c){return c.key==message->key;});
    require(placement!=session.geometry.controls.end()&&
            placement->target.y>=session.geometry.players.y+session.geometry.players.height,
            "native right status pane is laid out below Players");
    visibility_session=&session;
    native::restore_logs(session);
    require(show_calls==0,"cached native pane is not revealed without ownership");
    require(native::hide_logs(session)&&session.logs[0].hidden_by_us&&hide_calls==1&&!ownership_missing,
            "ownership published before native hide");
    native::restore_logs(session);
    require(show_calls==1&&!session.logs[0].hidden_by_us,"owned pane restored exactly once");
    native::restore_logs(session);
    require(show_calls==1,"repeated restoration does not reveal unowned pane");
    fail_hide=true;
    require(!native::hide_logs(session)&&!session.logs[0].hidden_by_us&&show_calls==2,
            "failed hide rolls back ownership and restores native pane");
    fail_hide=false;restore_during_hide=true;
    require(!native::hide_logs(session)&&!session.logs[0].hidden_by_us&&show_calls==4&&!ownership_missing,
            "synchronous restoration during hide falls back and restores again after outer call");
    restore_during_hide=false;
    require(native::hide_logs(session),"hide before restoration failure");
    fail_show=true;native::restore_logs(session);
    require(show_calls==5&&session.logs[0].hidden_by_us,"failed restoration retains ownership for retry");
    fail_show=false;native::restore_logs(session);
    require(show_calls==6&&!session.logs[0].hidden_by_us,"later fallback retries failed restoration");
    // Simulate a worker advancing the camera while native geometry was built.
    // The completed layout must capture the final handover image, not an older
    // snapshot taken before background presentation has stopped.
    require(session.output.get(1920,1080)&&a2fo::refit_menu::paint(session.output.dc,1920,1080,
        a2fo::refit_menu::default_snapshot(),-1,-1),"embedded presentation fonts initialized");
    retained_scene=std::make_shared<a2fo::refit_menu::Artwork>(a2fo::refit_menu::Artwork{2,2,{0xff0000,0xff0000,0xff0000,0xff0000}});
    completed_handover_scene=std::make_shared<a2fo::refit_menu::Artwork>(a2fo::refit_menu::Artwork{2,2,{0x00ff00,0x00ff00,0x00ff00,0x00ff00}});
    scene_requested=true;native::wanted.store(true);handover_running=true;present_fixture_parent=parent;
    require(native::present(session)&&!captured_during_handover&&!handover_running,
            "completed IA layout captured before stopping background handover");
    require(GdiFlush()!=FALSE,"completed IA layout pixels flushed");
    const auto final_pixel=GetPixel(session.output.dc,0,0);
    // The normal translucent blue tint contributes a little red/blue too.
    require(final_pixel!=CLR_INVALID&&GetGValue(final_pixel)>GetRValue(final_pixel)+32&&
            GetGValue(final_pixel)>GetBValue(final_pixel)+32,
            "completed IA layout replayed the old red camera snapshot");
    const auto previous_presents=session.presents,previous_scene_calls=scene_frame_calls;
    auto shared_session=std::make_shared<native::Session>();
    shared_session->window=parent;shared_session->active=true;shared_session->coalescing_hover=true;
    native::sessions[parent]=shared_session;
    require(native::presenter(parent,nullptr,nullptr)&&scene_frame_calls==previous_scene_calls&&
            shared_session->presents==0&&session.presents==previous_presents,
            "native hover canvas leases defer composition without acquiring another scene");
    native::sessions.erase(parent);
    native::Dib erased;require(erased.get(1920,1080),"private erase destination");
    require(native::erase_retained(session,erased.dc)&&GdiFlush()&&
            GetPixel(erased.dc,0,0)==GetPixel(session.output.dc,0,0),
            "active IA erase restores completed UI instead of a blank background");
    if(benchmark_setup){
        LARGE_INTEGER frequency{},begin{},captured{},painted{};
        require(QueryPerformanceFrequency(&frequency)!=FALSE,"benchmark clock available");
        for(bool direct:{false,true}){
        std::vector<double> snapshots,paints;
        for(int i=0;i<32;++i){
            QueryPerformanceCounter(&begin);
            require(native::snapshot(session),"benchmark native snapshot");
            QueryPerformanceCounter(&captured);
            a2fo::refit_menu::setup_skin::DirectTargetScope target{
                session.output.dc,session.output.width,session.output.height,direct?session.output.bits:nullptr};
            require(setup::paint(session.output.dc,1920,1080,session.model,session.geometry,
                retained_scene.get(),&session.native,-1,-1)&&GdiFlush(),"benchmark hidden IA composition");
            QueryPerformanceCounter(&painted);
            snapshots.push_back(1000.0*(captured.QuadPart-begin.QuadPart)/frequency.QuadPart);
            paints.push_back(1000.0*(painted.QuadPart-captured.QuadPart)/frequency.QuadPart);
        }
        std::sort(snapshots.begin(),snapshots.end());std::sort(paints.begin(),paints.end());
        std::printf("Hidden 1920x1080 IA fixture %s median: snapshot %.3f ms, paint+flush %.3f ms (32 frames)\n",
            direct?"direct DIB":"GDI",snapshots[16],paints[16]);
        }
    }
    present_fixture_parent=nullptr;scene_requested=false;native::wanted.store(false);
    retained_scene.reset();completed_handover_scene.reset();native::restore_logs(session);
    visibility_session=nullptr;
    require(!IsWindowVisible(parent)&&!IsWindowVisible(status),"visibility fixture never reveals real windows");
    // Unknown/interactive children still keep the original interface.
    require(SetWindowLongPtrA(status,GWL_STYLE,WS_CHILD|ES_MULTILINE)!=0,"make fixture Edit interactive");
    require(!native::snapshot(session),"editable status child retains native interface");
    require(SetWindowLongPtrA(status,GWL_STYLE,WS_CHILD|ES_MULTILINE|ES_READONLY)!=0,"restore read-only fixture Edit");
    require(SetWindowLongPtrA(status,GWLP_ID,9999)==2120,"set unknown status ID");
    require(!native::snapshot(session),"unknown read-only Edit retains native interface");
    require(SetWindowLongPtrA(status,GWLP_ID,2120)==9999,"restore known status ID");
    require(SetWindowPos(status,nullptr,1900,613,240,252,SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE,"place status outside canvas");
    require(!native::snapshot(session),"cached pane bounds rechecked outside canvas");
    require(SetWindowPos(status,nullptr,1637,613,240,252,SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE,"restore status bounds");
    require(native::snapshot(session),"native snapshot recovers after corrected child metadata");
    visible_fixture_child=nullptr;native::widgets.clear();snapshot_parent=nullptr;
    native::bridge_ready=nullptr;native::copy_canvas=nullptr;
}
unsigned cleared_seed{},cleared_refresh{};
BOOL __cdecl clear_seed(A2FO_ShellCanvasSeedV1 callback,void* context){
    require(!callback&&!context,"shutdown clears seed callback/context");++cleared_seed;return TRUE;
}
BOOL __cdecl clear_refresh(A2FO_ShellCanvasRefreshV1 callback,void* context){
    require(!callback&&!context,"shutdown clears refresh callback/context");++cleared_refresh;return TRUE;
}
void check_callback_shutdown(){
    native::owner_thread=GetCurrentThreadId();native::set_seed=&clear_seed;native::set_refresh=&clear_refresh;
    native::shutdown();
    require(cleared_seed==1&&cleared_refresh==1&&!native::set_seed&&!native::set_refresh,
            "shutdown retained a registered canvas callback");
    native::shutdown();
    require(cleared_seed==1&&cleared_refresh==1,"shutdown unregisters callbacks more than once");
}
void check_launch_dispatch(){
    native::Session session;
    const auto image=std::make_shared<a2fo::refit_menu::Artwork>();
    session.pending_launch_ui=image;
    require(!native::take_launch_overlay(session,true),"unrelated hide consumed an unarmed Launch snapshot");
    {
        native::LaunchDispatch dispatch(session,true);
        {native::LaunchDispatch nested(session,true);}
        require(session.launch_dispatch&&!native::take_launch_overlay(session,false),"release scope absent or consumed without native close");
        require(native::take_launch_overlay(session,true)==image&&!session.pending_launch_ui,
                "native close in actual release did not consume its snapshot once");
    }
    require(!session.launch_dispatch,"release dispatch scope survived native forwarding");
    session.pending_launch_ui=image;
    {native::LaunchDispatch rejected(session,true);}
    require(!session.pending_launch_ui&&!native::take_launch_overlay(session,true),
            "rejected release left a snapshot for a later unrelated hide");
}
void check_accepted_loading(HWND parent){
    const DWORD previous_owner=native::owner_thread;native::owner_thread=GetCurrentThreadId();native::wanted=true;
    auto candidate=std::make_shared<native::Session>();candidate->window=hidden("#32770",WS_POPUP,parent);
    candidate->parent=parent;candidate->identity=0x12345;
    require(candidate->output.get(8,6),"accepted loading private UI");
    std::fill_n(candidate->output.bits,48,0x00204080u);
    const auto arm=[&]{
        require(SetPropA(parent,native::retained_property,reinterpret_cast<HANDLE>(candidate->identity)),"accepted parent identity");
        if(IsWindow(candidate->window))require(SetPropA(candidate->window,native::retained_property,
            reinterpret_cast<HANDLE>(candidate->identity)),"accepted source identity");
        native::retained_setup=candidate;
    };
    const auto before=accepted_handover_calls;arm();
    require(native::begin_loading_handover()&&accepted_handover_calls==before+1&&!native::retained_setup,
            "accepted loading did not consume a live IA snapshot once");
    require(!native::begin_loading_handover(),"accepted snapshot consumed twice");
    candidate->closed_at=GetTickCount64();DestroyWindow(candidate->window);arm();
    require(native::begin_loading_handover()&&accepted_handover_calls==before+2,
            "accepted loading lost the IA snapshot after asynchronous native destruction");
    arm();SetPropA(parent,native::retained_property,reinterpret_cast<HANDLE>(candidate->identity+1));
    require(!native::begin_loading_handover()&&accepted_handover_calls==before+2,
            "different parent identity admitted a stale IA snapshot");
    arm();candidate->closed_at=GetTickCount64()-3001;
    require(!native::begin_loading_handover(),"expired closed IA snapshot admitted");
    arm();HWND newer=hidden("#32770",WS_POPUP,parent);
    native::observe(newer,{});
    require(!native::retained_setup&&!native::begin_loading_handover(),
            "new IA candidate that fails admission retained a prior dialog image");
    DestroyWindow(newer);
    native::wanted=false;native::owner_thread=previous_owner;
}
}

int main(int argc, char** argv) {
    try {
        benchmark_setup=argc==3&&!std::strcmp(argv[2],"--benchmark");
        require(argc == 2||benchmark_setup, "usage: refit_setup_native.exe ArmadaL.exe [--benchmark]");
        HMODULE mapped = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
        require(mapped != nullptr,"pinned image read-only mapping");
        require(a2fo::supported_armada::identify(mapped) !=
            a2fo::supported_armada::Identity::unsupported, "supported ArmadaL image identity");
        // Wine maps LoadLibrary(EXE) as data away from its preferred base. Copy
        // PE sections into a private fixture allocation. Rebase only the three
        // fixture vtable snapshots below; the constructor bytes stay unchanged.
        // No imports, loader callbacks, TLS, or game entrypoint are invoked.
        auto* source=reinterpret_cast<const BYTE*>(mapped);
        const auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(source);
        const auto* nt=reinterpret_cast<const IMAGE_NT_HEADERS32*>(source+dos->e_lfanew);
        auto* image=static_cast<BYTE*>(VirtualAlloc(nullptr,
            nt->OptionalHeader.SizeOfImage,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
        require(image!=nullptr,"private fixture image allocation");
        std::memcpy(image,source,nt->OptionalHeader.SizeOfHeaders);
        const auto* sections=IMAGE_FIRST_SECTION(nt);
        for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i)
            std::memcpy(image+sections[i].VirtualAddress,source+sections[i].VirtualAddress,
                        sections[i].Misc.VirtualSize);
        const auto image_size=nt->OptionalHeader.SizeOfImage;
        FreeLibrary(mapped);mapped=reinterpret_cast<HMODULE>(image);
        DWORD previous{};
        require(VirtualProtect(image+native::ctor_rva,4096,PAGE_EXECUTE_READ,&previous)!=FALSE,
            "only verified constructor page executable");
        native::engine = mapped;
        // LoadLibrary(EXE) data mappings do not relocate engine code pointers.
        // Validate these original entries before rebasing fixture-only tables.
        auto* combo_table=reinterpret_cast<std::uintptr_t*>(native::at(0x2bad2c));
        auto* button_table=reinterpret_cast<std::uintptr_t*>(native::at(0x2bb0e4));
        require(button_table[8]==0x5a4af0,"original button update anchor");
        auto* label_table=reinterpret_cast<std::uintptr_t*>(native::at(0x2bbaa4));
        require(combo_table[8]==0x59d6e0 && label_table[1]==0x5a7590,
                "original combo and label vtable anchors");
        for(auto* table:{combo_table,label_table,button_table})for(int i=0;i<9;++i)
            if(table[i]>=0x400000&&table[i]<0x400000+image_size)
                table[i]=reinterpret_cast<std::uintptr_t>(mapped)+(table[i]-0x400000);
        HWND parent = hidden("#32770", WS_POPUP);
        HWND second = hidden("#32770", WS_POPUP);
        check_early_seed(parent);
        HWND combo = hidden("COMBOBOX", WS_CHILD | CBS_DROPDOWNLIST, parent);
        require(SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Federation")) == 0 &&
            SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Borg")) == 1 &&
            SendMessageA(combo, CB_SETCURSEL, 1, 0) == 1, "combo data properties initialized");
        check_combo(parent, combo);
        check_label(parent);
        check_ctor(parent, second);
        check_complete_snapshot(parent);
        check_accepted_loading(parent);
        check_callback_shutdown();
        check_launch_dispatch();
        DestroyWindow(second);
        DestroyWindow(parent);
        VirtualFree(mapped,0,MEM_RELEASE);
        std::puts("refit setup native widget decode and constructor passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
