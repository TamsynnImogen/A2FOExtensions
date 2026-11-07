// Isolated graphics integration fixture. Run ONLY inside a private headless
// compositor, never on the user's desktop. No engine executable or input is used.
// Include the exact worker implementation so its private completion handle can
// be waited on by the fixture; production shutdown deliberately never joins.
#include "../modules/A2FORefitMenus/menu_scene.cpp"
#include <stdexcept>

namespace {
std::string fixture_root, asset_root;
unsigned backend_creations{}, output_resizes{}, seen_views{};
DWORD longest_ui_call{};
HDC handover_dc{};
std::atomic<unsigned> handover_copies{};
std::atomic<HWND> handover_target{};
bool memory_handover(const a2fo::refit_menu::menu_scene::SceneHandover::State& state,
                     const a2fo::refit_menu::Artwork& image){
    const auto valid=[&](std::uintptr_t p){return a2fo::refit_menu::menu_scene::handover_window_matches(state,p);};
    const auto target=state.launch?state.parent:valid(state.destination)?state.destination:valid(state.source)?state.source:state.parent;
    if(!valid(target))return false;
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=image.width;info.bmiHeader.biHeight=-image.height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    const auto drawn=StretchDIBits(handover_dc,0,0,32,18,0,0,image.width,image.height,image.pixels.data(),
        &info,DIB_RGB_COLORS,SRCCOPY);
    if(!drawn||drawn==static_cast<int>(GDI_ERROR)||!GdiFlush())return false;
    handover_target=reinterpret_cast<HWND>(target);++handover_copies;return true;
}
const char* A2FO_CALL root() { return fixture_root.c_str(); }
unsigned A2FO_CALL root_count() { return 1; }
const char* A2FO_CALL asset(unsigned) { return asset_root.c_str(); }
void A2FO_CALL diagnostic(const char*, const char* message) {
    std::printf("%s\n", message);
    std::fflush(stdout);
    if (std::strstr(message, "Refit scene worker:")) ++backend_creations;
    if (std::strstr(message, "Refit output resized:")) ++output_resizes;
    for (unsigned i = 0; i < 5; ++i) {
        const std::string token = std::string("view=") + a2fo::refit_menu::menu_scene::views[i].id + " ";
        if (std::strstr(message, token.c_str())) seen_views |= 1u << i;
    }
}
void require(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
std::string absolute(const char* path) {
    char result[32768]{};
    const DWORD size = GetFullPathNameA(path, sizeof(result), result, nullptr);
    require(size && size < sizeof(result), "invalid fixture path");
    return result;
}
HWND menu(HWND parent=nullptr) {
    WNDCLASSEXA c{};
    c.cbSize = sizeof(c); c.hInstance = GetModuleHandleA(nullptr);
    c.lpfnWndProc = DefWindowProcA; c.lpszClassName = "TMainForm";
    require(RegisterClassExA(&c) || GetLastError() == ERROR_CLASS_ALREADY_EXISTS, "fixture class");
    // No WS_VISIBLE, ShowWindow, activation, focus or synthesized input.
    HWND window = CreateWindowExA(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        c.lpszClassName, "isolated scene fixture", WS_POPUP, 0, 0, 1920, 1080,
        parent, nullptr, c.hInstance, nullptr);
    require(window != nullptr && !IsWindowVisible(window), "hidden fixture window");
    return window;
}
const a2fo::refit_menu::Artwork* poll(HWND window,bool overlay=false) {
    MSG message{};
    while (PeekMessageA(&message, window, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message); DispatchMessageA(&message);
    }
    const DWORD begin = GetTickCount();
    auto image = a2fo::refit_menu::menu_scene::frame(window,overlay,false,overlay);
    a2fo::refit_menu::menu_scene::frame_presented(image);
    longest_ui_call = std::max(longest_ui_call, GetTickCount() - begin);
    require(longest_ui_call < 1000, "UI request waited on expensive render work");
    return image;
}
void capture(const char* name, const a2fo::refit_menu::Artwork& image) {
    BITMAPFILEHEADER f{}; BITMAPINFOHEADER h{};
    f.bfType = 0x4d42; f.bfOffBits = sizeof(f) + sizeof(h);
    f.bfSize = f.bfOffBits + static_cast<DWORD>(image.pixels.size() * 4);
    h.biSize = sizeof(h); h.biWidth = image.width; h.biHeight = -image.height;
    h.biPlanes = 1; h.biBitCount = 32; h.biCompression = BI_RGB;
    const auto path = fixture_root + "\\" + name;
    FILE* file = std::fopen(path.c_str(), "wb");
    require(file != nullptr, "capture file");
    const bool ok = std::fwrite(&f, sizeof(f), 1, file) == 1 &&
        std::fwrite(&h, sizeof(h), 1, file) == 1 &&
        std::fwrite(image.pixels.data(), 4, image.pixels.size(), file) == image.pixels.size();
    std::fclose(file); require(ok, "capture write");
}
const a2fo::refit_menu::Artwork* wait_for(HWND window, unsigned view) {
    const DWORD begin = GetTickCount();
    while (GetTickCount() - begin < 180000) {
        const auto image = poll(window);
        if (image && (seen_views & (1u << view))) return image;
        Sleep(20);
    }
    throw std::runtime_error("render deadline exceeded");
}
}

int main(int argc, char** argv) {
    using namespace a2fo::refit_menu::menu_scene;
    const bool startup_only=argc==4&&!std::strcmp(argv[3],"--startup-only");
    if (argc != 3&&!startup_only) { std::fprintf(stderr, "usage: fixture HOST_ROOT ASSET_ROOT [--startup-only]\n"); return 2; }
    HWND window{};
    try {
        fixture_root = absolute(argv[1]); asset_root = absolute(argv[2]);
        SetDllDirectoryA(fixture_root.c_str());
        // Private host recipes only: exercise IDs beyond stock campaigns and
        // beyond the old 32-byte provider string limit, with enough choices
        // to require more than one selector page.
        const auto recipe = fixture_root + "\\RefitScene\\Menu\\map.ini";
        FILE* extra = std::fopen(recipe.c_str(), "ab");
        require(extra != nullptr, "private recipe append");
        for (int i = 10; i < 18; ++i)
            std::fprintf(extra, "\n[View.custom_campaign_%d_with_a_long_named_camera]\nLabel=Custom %d\nGroup=fleet\nEye=125,25,285\nTarget=0,-60,-120\nFov=40\n", i, i);
        std::fclose(extra);
        A2FO_ModuleApi api{};
        api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
        api.root_directory = root; api.extension_root_count = root_count;
        api.extension_root = asset; api.log = diagnostic;
        window = menu();
        configure(&api, nullptr, false, 0);
        require(frame(window) == nullptr && !started && !GetModuleHandleA("A2FORefitScene.dll"),
            "LiveScene=0 started a renderer");
        std::puts("PASS: rendering off creates no worker/provider/device"); std::fflush(stdout);
        configure(&api, nullptr, true, 0);
        const auto first = wait_for(window, 0);
        require(first->width == 1920 && first->height == 1080, "native scene resolution");
        capture("main-native.bmp", *first);
        if(startup_only){
            shutdown();
            require(WaitForSingleObject(worker_thread.load(),60000)==WAIT_OBJECT_0,"startup profile worker cleanup");
            DestroyWindow(window);window=nullptr;
            std::puts("PASS: all-fleet startup profile");std::fflush(stdout);return 0;
        }
        const DWORD pending_begin=GetTickCount();
        while(!exchange.awaiting_presentation(exchange.request())){
            require(GetTickCount()-pending_begin<120000,"next menu frame deadline");Sleep(10);
        }
        const auto unpresented=exchange.snapshot();
        const auto before_failure=exchange.request();
        const auto before_revision=displayed_revision;
        require(frame(window)==unpresented.image.get()&&displayed_revision==before_revision,
                "requesting an image marked an unpresented revision as displayed");
        const auto logical=reinterpret_cast<HDC>(static_cast<std::uintptr_t>(1));
        pending_canvas[logical]=unpresented.image;
        canvas_completed(window,logical,FALSE,nullptr);
        require(pending_canvas.empty()&&exchange.request().frame==before_failure.frame&&
                exchange.request().animation_time==before_failure.animation_time&&displayed_revision==before_revision,
                "failed physical canvas present consumed animation ticket");
        pending_canvas[logical]=unpresented.image;
        canvas_completed(window,logical,TRUE,nullptr);
        require(pending_canvas.empty()&&exchange.request().frame==before_failure.frame+1&&
                displayed_revision==unpresented.image_revision,
                "successful physical canvas present did not release next frame");
        std::puts("PASS: failed canvas presentation retains choreography; successful completion acknowledges once");
        HWND device_owner = FindWindowA("A2FORefitScene.Offscreen.v1", nullptr);
        require(device_owner && !IsWindowVisible(device_owner), "private hidden device owner");
        require(backend_creations == 1, "initial backend creation count");
        // Native main-menu close precedes IA construction. Exercise that gap
        // without pumping the UI: the real worker draws into a private memory
        // DC, never into a visible fixture or desktop window.
        handover_dc=CreateCompatibleDC(nullptr);BITMAPINFO bridge_info{};
        bridge_info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bridge_info.bmiHeader.biWidth=32;
        bridge_info.bmiHeader.biHeight=-18;bridge_info.bmiHeader.biPlanes=1;bridge_info.bmiHeader.biBitCount=32;
        void* bridge_pixels{};const auto bridge_bitmap=CreateDIBSection(handover_dc,&bridge_info,DIB_RGB_COLORS,&bridge_pixels,nullptr,0);
        require(handover_dc&&bridge_bitmap&&bridge_pixels,"handover private canvas");
        const auto bridge_old=SelectObject(handover_dc,bridge_bitmap);
        handover_draw.store(&memory_handover);
        const auto source=menu(window);poll(source);
        require(begin_handover(source,window),"handover before native close");
        require(handover_erase_proc(window,WM_ERASEBKGND,reinterpret_cast<WPARAM>(handover_dc),0,0,0)==1,
                "parent erase did not retain the scene during native handover");
        const auto identity=handover.state();
        const auto unrelated=menu(window);
        require(handover_window_matches(identity,reinterpret_cast<std::uintptr_t>(source))&&
                handover_window_matches(identity,reinterpret_cast<std::uintptr_t>(window))&&
                !handover_window_matches(identity,reinterpret_cast<std::uintptr_t>(unrelated)),
                "handover accepted an unmarked native window");
        DestroyWindow(unrelated);
        const DWORD bridge_start=GetTickCount();
        while(!handover_ready()){
            require(GetTickCount()-bridge_start<120000,"handover first frame deadline");Sleep(10);
        }
        if(handover_target!=source||exchange.snapshot().view!="main.instantaction"){
            const auto state=handover.state();const auto snapshot=exchange.snapshot();
            std::printf("Source handover: target=%p source=%p parent=%p state=%s snapshot=%s copies=%u source-valid=%u parent-valid=%u\n",
                static_cast<void*>(handover_target.load()),static_cast<void*>(source),static_cast<void*>(window),
                state.view.c_str(),snapshot.view.c_str(),handover_copies.load(),
                handover_window_matches(state,reinterpret_cast<std::uintptr_t>(source)),
                handover_window_matches(state,reinterpret_cast<std::uintptr_t>(window)));
        }
        require(handover_target==source&&exchange.snapshot().view=="main.instantaction",
                "camera did not start on source menu before close");
        DestroyWindow(source);
        require(!handover_window_matches(identity,reinterpret_cast<std::uintptr_t>(source)),
                "destroyed native window retained handover identity");
        const auto time_before_gap=exchange.request().animation_time;
        const auto copies_before_gap=handover_copies.load();
        Sleep(700); // No frame(), native paint, UI pump or loading callback.
        require(!exchange.request().active&&handover_copies>=copies_before_gap+3&&
                handover_target==window&&exchange.request().animation_time>time_before_gap+.1,
                "scene did not animate on persistent parent during blocked IA construction");
        const auto earlier_destination=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,"#32770","private early IA",
            WS_POPUP,0,0,1920,1080,window,nullptr,GetModuleHandleA(nullptr),nullptr);
        require(earlier_destination&&!IsWindowVisible(earlier_destination),"hidden early IA handover destination");
        handover_destination(earlier_destination);
        const auto destination=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,"#32770","private IA",
            WS_POPUP,0,0,1920,1080,window,nullptr,GetModuleHandleA(nullptr),nullptr);
        require(destination&&!IsWindowVisible(destination),"hidden IA handover destination");
        handover_destination(destination);Sleep(300);
        require(handover_target==destination&&!GetPropA(earlier_destination,handover_property),
                "new IA root did not replace the previous handover destination");
        const DWORD finish_begin=GetTickCount();end_handover();
        require(GetTickCount()-finish_begin<500,"handover end waited on rendering");
        const auto stopped_copies=handover_copies.load();Sleep(150);
        require(handover_copies==stopped_copies&&!handover_active()&&!exchange.request().active,
                "handover painted over completed UI or kept rendering after source destruction");
        require(!GetPropA(window,handover_property)&&!GetPropA(destination,handover_property),
                "handover retained native window identity after completion");
        const auto* copied_pixels=static_cast<const std::uint32_t*>(bridge_pixels);
        require(std::any_of(copied_pixels,copied_pixels+32*18,
                    [](std::uint32_t pixel){return (pixel&0x00ffffffu)!=0;}),
                "handover sink received no actual pixels");
        const auto launch_source=menu(window);poll(launch_source);
        auto launch_ui=std::make_shared<a2fo::refit_menu::Artwork>();
        launch_ui->width=32;launch_ui->height=18;launch_ui->pixels.assign(32*18,0x00204080);
        const auto launch_copies=handover_copies.load();
        require(begin_launch_handover(launch_source,window,launch_ui)&&launch_handover_active()&&departing(),
                "Launch failed to begin background fade and departure together");
        const auto launch_epoch=exchange.request().epoch;
        DestroyWindow(launch_source);
        const DWORD launch_continuity_begin=GetTickCount();
        while(handover_copies<=launch_copies+3||handover_target!=window||!exchange.snapshot().departure){
            if(GetTickCount()-launch_continuity_begin>=2000){
                const auto snapshot=exchange.snapshot();const auto request=exchange.request();
                std::printf("Launch continuity deadline: copies=%u initial=%u target=%p parent=%p snapshot=%s departure=%u epoch=%llu request=%s departure=%u epoch=%llu active=%u ready=%u\n",
                    handover_copies.load(),launch_copies,static_cast<void*>(handover_target.load()),static_cast<void*>(window),
                    snapshot.view.c_str(),snapshot.departure,static_cast<unsigned long long>(snapshot.epoch),
                    request.view.c_str(),request.departure,static_cast<unsigned long long>(request.epoch),handover_active(),handover_ready());
                break;
            }
            Sleep(5);
        }
        std::printf("Launch continuity: first four retained/departure copies after native close in %lu ms\n",
            static_cast<unsigned long>(GetTickCount()-launch_continuity_begin));std::fflush(stdout);
        require(handover_copies>launch_copies+3&&handover_target==window&&exchange.snapshot().departure,
                "Launch scene stopped when native IA closed");
        const auto moving_copies=handover_copies.load();Sleep(400);
        require(handover_copies>=moving_copies+3&&handover_target==window&&exchange.snapshot().epoch==launch_epoch,
                "Launch departure stopped presenting after its first completed frame");
        loading_frame(1920,1080,false);
        require(exchange.request().epoch==launch_epoch,"first loading request restarted the Launch camera");
        end_handover();
        require(!launch_handover_active()&&departing(),"ending GDI handover also reset the departure");
        const auto launch_stopped=handover_copies.load();
        for(int i=0;i<10;++i){loading_presented(loading_frame(1920,1080,false));Sleep(20);}
        require(exchange.request().epoch==launch_epoch&&handover_copies==launch_stopped,
                "loading continuation reset the camera or repainted the obsolete IA window");
        end_departure();poll(window);
        std::puts("PASS: IA Launch fades on persistent parent, survives native close and continues same departure epoch into loading");
        require(begin_accepted_loading_handover(window,launch_ui)&&launch_handover_active()&&departing(),
                "accepted native loading could not begin from the surviving parent alone");
        const auto accepted_epoch=exchange.request().epoch;
        loading_frame(1920,1080,false);
        require(exchange.request().epoch==accepted_epoch,"accepted loading reset the departure epoch");
        end_handover();end_departure();poll(window);
        handover_draw.store(&draw_handover_pixels);
        DestroyWindow(destination);DestroyWindow(earlier_destination);SelectObject(handover_dc,bridge_old);
        DeleteObject(bridge_bitmap);DeleteDC(handover_dc);handover_dc=nullptr;
        poll(window);
        std::puts("PASS: camera starts before main close; parent animates during blocked UI; IA root takes over; no late painting");
        DestroyWindow(window); window = nullptr;
        Sleep(200);
        window = menu();
        require(poll(window) != nullptr, "menu return lost the CPU snapshot");
        require(FindWindowA("A2FORefitScene.Offscreen.v1", nullptr) == device_owner,
            "menu return recreated the device owner");
        for (int i = 0; i < 20; ++i) { poll(window); Sleep(20); }
        require(backend_creations == 1, "menu return recreated the GPU backend");
        std::puts("PASS: native-resolution frame and cache retained across menu destruction/return");
        std::fflush(stdout);
        HWND setup=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,"#32770","isolated setup",
            WS_POPUP,0,0,1920,1080,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
        require(setup&&!IsWindowVisible(setup),"hidden setup fixture window");
        frame_presented(frame(setup,true,false,false));
        require(exchange.request().view=="main","generic dialog incorrectly selected the IA camera");
        require(poll(setup,true)!=nullptr&&presentation_window==setup,"setup did not reuse current scene");
        require(exchange.request().view=="main.instantaction","setup did not request authored camera blend");
        const auto main_timer=windows.find(window);
        require(main_timer!=windows.end(),"main timer retained");
        window_proc(window,WM_TIMER,main_timer->second,0,0,0);
        require(exchange.request().active&&presentation_window==setup,"old main timer paused setup scene");
        for(int i=0;i<20;++i){poll(setup,true);Sleep(20);}
        require(backend_creations==1,"setup created another GPU backend");
        DestroyWindow(setup);
        require(poll(window)!=nullptr&&presentation_window==window&&backend_creations==1,
                "return from setup lost scene/cache");
        require(exchange.request().view=="main","return did not restore main camera");
        std::puts("PASS: setup overlay uses retained native-resolution scene; old main timer cannot pause it");
        std::fflush(stdout);
        for (const unsigned view : {3u, 4u}) {
            choose(static_cast<int>(view)); wait_for(window, view);
            const DWORD begin = GetTickCount();
            unsigned fresh_frames = 0;
            auto previous = displayed;
            // A cold draw can finish with the camera's initial blend pose.
            // Wait beyond the recipe's staggered Klingon decloak (about 5.5 s),
            // as well as for subsequent published frames after a cold draw.
            while (GetTickCount() - begin < 8000 || fresh_frames < 3) {
                require(GetTickCount() - begin < 120000, "stable faction frame deadline");
                poll(window);
                if (displayed != previous) { ++fresh_frames; previous = displayed; }
                Sleep(20);
            }
            capture(view == 3 ? "klingon-native.bmp" : "borg-native.bmp", *poll(window));
            require(backend_creations == 1, "faction switch recreated backend");
        }
        require(views.size() >= 13 && page_count() >= 3, "expanded view catalogue/page count");
        const int old_page = choice_page;
        choose(static_cast<int>(views.size()) + 1);
        require(choice_page == (old_page + 1) % page_count(), "next catalogue page");
        const char* custom = "custom_campaign_17_with_a_long_named_camera";
        require(select_view(custom), "arbitrary long named campaign view");
        require(!select_view("unknown-campaign-view"), "unknown campaign view rejected");
        const DWORD custom_begin = GetTickCount();
        while (exchange.snapshot().view != custom) {
            require(GetTickCount() - custom_begin < 120000, "custom view deadline");
            poll(window); Sleep(20);
        }
        const DWORD settled = GetTickCount();
        while(GetTickCount()-settled<2200){poll(window);Sleep(20);}
        const auto initial = exchange.snapshot().image;
        require(begin_departure(custom), "begin warp from selected arbitrary view");
        const auto epoch = exchange.request().epoch;
        const DWORD departure_begin = GetTickCount();
        unsigned warp_frames = 0;
        unsigned pan_frames=0,first_three_seconds=0;DWORD last_warp_frame{},longest_warp_gap{};
        auto prior = initial;
        while (GetTickCount() - departure_begin < 6500 || warp_frames < 3) {
            require(GetTickCount() - departure_begin < 120000, "warp frame deadline");
            const DWORD begin = GetTickCount();
            // The live game changes from the 1920x1080 shell to a 1280x800
            // loading target. This must replace surfaces, retaining the device
            // and uploaded assets throughout the camera/warp sequence.
            const auto drawn=loading_frame(1280,800);
            loading_presented(drawn);
            longest_ui_call = std::max(longest_ui_call, GetTickCount() - begin);
            require(longest_ui_call < 1000, "loading waited for graphics worker");
            const auto scene = exchange.snapshot();
            if(scene.departure && scene.epoch == epoch && scene.image != prior){
                prior = scene.image; ++warp_frames;
                const DWORD elapsed=GetTickCount()-departure_begin;
                if(elapsed<=1600)++pan_frames;
                if(elapsed<=3000)++first_three_seconds;
                if(last_warp_frame)longest_warp_gap=std::max(longest_warp_gap,GetTickCount()-last_warp_frame);
                last_warp_frame=GetTickCount();
                if(warp_frames == 1) {
                    require(prior->width==1280&&prior->height==800,"loading resize not published");
                    capture("departure-start.bmp", *prior);
                    std::printf("Loading resize plus first departure frame: %lu ms\n",
                        static_cast<unsigned long>(GetTickCount()-departure_begin));
                }
                if(warp_frames == 5) capture("departure-pan.bmp", *prior);
                if(warp_frames == 10) capture("departure-warp.bmp", *prior);
            }
            Sleep(20);
        }
        capture("warp-tunnel.bmp", *prior);
        std::printf("Departure cadence: %u completed frames in 1600 ms, %u in 3000 ms, %u total; longest sampled gap=%lu ms\n",
            pan_frames,first_three_seconds,warp_frames,static_cast<unsigned long>(longest_warp_gap));
        require(initial->pixels != prior->pixels, "departure/tunnel remained static");
        require(backend_creations == 1, "departure recreated retained backend");
        require(output_resizes==2,"loading resolution change rebuilt or repeatedly resized output");
        // A stalled load step must stop scheduling GPU work until a new native
        // callback renews the lease, and renewal must wake the idle worker.
        Sleep(3500);
        const auto asleep = exchange.snapshot().revision;
        const auto frozen_time=exchange.request().animation_time;
        Sleep(500);
        require(exchange.snapshot().revision == asleep, "expired loading lease kept rendering");
        require(exchange.request().animation_time==frozen_time,"unpresented frames advanced choreography");
        const DWORD resume_begin = GetTickCount();
        while(exchange.snapshot().revision == asleep){
            require(GetTickCount()-resume_begin < 120000, "lease renewal did not wake worker");
            loading_presented(loading_frame(1280,800)); Sleep(20);
        }
        require(exchange.request().animation_time-frozen_time<=0.050001,
                "loading stall skipped choreography instead of one bounded frame");
        end_departure();
        require(!departing(), "mission return did not end departure");
        const DWORD return_begin=GetTickCount();
        do {
            poll(window);Sleep(20);
            require(GetTickCount()-return_begin<120000,"menu resolution restoration deadline");
        } while(exchange.snapshot().image->width!=1920);
        require(backend_creations==1&&output_resizes==3,"menu return rebuilt device or missed resize");
        require(begin_departure(custom), "same-map departure restart");
        require(exchange.request().epoch > epoch, "same-map departure reused epoch");
        const auto next_epoch = exchange.request().epoch;
        const DWORD repeat_begin = GetTickCount();
        while(exchange.snapshot().epoch != next_epoch){
            require(GetTickCount()-repeat_begin < 120000, "same-map departure restart deadline");
            loading_presented(loading_frame(1280,800)); Sleep(20);
        }
        capture("departure-repeat.bmp", *exchange.snapshot().image);
        end_departure();
        require(backend_creations==1&&output_resizes==4,"repeat loading resized more than once");
        std::puts("PASS: 13 named views, long campaign ID, paged choices, animated departure/tunnel, lease renewal, same-map restart; one retained backend");
        std::fflush(stdout);
        const DWORD stop_begin = GetTickCount(); shutdown();
        require(GetTickCount() - stop_begin < 500, "shutdown blocked waiting for worker");
        require(WaitForSingleObject(worker_thread.load(), 60000) == WAIT_OBJECT_0, "worker cleanup deadline");
        require(!IsWindow(device_owner), "worker device window not destroyed on its thread");
        DestroyWindow(window); window = nullptr;
        std::printf("PASS: real D3D frames (main/Klingon/Borg), 1920x1080, retained cache, shutdown; max UI exchange=%lu ms\n",
            static_cast<unsigned long>(longest_ui_call));
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what()); shutdown();
        if (worker_thread.load()) WaitForSingleObject(worker_thread.load(), 60000);
        if (window) DestroyWindow(window);
        return 1;
    }
}
