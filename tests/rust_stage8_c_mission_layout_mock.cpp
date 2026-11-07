#include <windows.h>
#include <gdiplus.h>
#include <cstdint>
#include <cstdio>
#include <vector>

static int fixture_width=0,fixture_height=0;
static bool fixture_client_rect_ok=true;
static bool fixture_origin_ok=true;
struct FixtureMove {int id,x,y,width,height;};
static std::vector<FixtureMove> fixture_moves;
extern "C" BOOL WINAPI mission_mock_GetClientRect(HWND,LPRECT rect) {
    if(!fixture_client_rect_ok)return FALSE;
    *rect={0,0,fixture_width,fixture_height};
    return TRUE;
}
extern "C" BOOL WINAPI mission_mock_MoveWindow(HWND window,int x,int y,
    int width,int height,BOOL) {
    fixture_moves.push_back({static_cast<int>(reinterpret_cast<std::uintptr_t>(window)),
        x,y,width,height});
    return TRUE;
}
extern "C" BOOL WINAPI mission_mock_ClientToScreen(HWND,LPPOINT point) {
    if(!fixture_origin_ok)return FALSE;
    *point={10,20};
    return TRUE;
}
#define GetClientRect mission_mock_GetClientRect
#define MoveWindow mission_mock_MoveWindow
#define ClientToScreen mission_mock_ClientToScreen
#ifndef A2FO_STAGE8_MISSION_SOURCE
#error A2FO_STAGE8_MISSION_SOURCE is required
#endif
#include A2FO_STAGE8_MISSION_SOURCE
#undef GetClientRect
#undef MoveWindow
#undef ClientToScreen

extern "C" HWND A2FO_CALL a2fo_mission_selector_call_thiscall_0(void*,void*) {
    return nullptr;
}

int main() {
    SelectorContext context;
    context.dialog=reinterpret_cast<HWND>(1);
    context.campaign_list=reinterpret_cast<HWND>(2);
    context.mission_list=reinterpret_cast<HWND>(3);
    context.preview=reinterpret_cast<HWND>(4);
    context.details=reinterpret_cast<HWND>(5);
    context.overview=reinterpret_cast<HWND>(6);
    context.start=reinterpret_cast<HWND>(7);
    context.back=reinterpret_cast<HWND>(8);
    for(const auto dimensions:{std::pair{640,480},std::pair{1024,768},
        std::pair{200,140},std::pair{1920,1080},std::pair{0,0}}) {
        fixture_width=dimensions.first;fixture_height=dimensions.second;
        fixture_moves.clear();
        layout_controls(context);
        std::printf("layout:%d:%d:%zu\n",fixture_width,fixture_height,
            fixture_moves.size());
        for(const auto& move:fixture_moves)
            std::printf("move:%d:%d:%d:%d:%d\n",move.id,move.x,move.y,
                move.width,move.height);
    }
    fixture_client_rect_ok=false;
    fixture_moves.clear();
    layout_controls(context);
    std::printf("layout:client-fail:%zu\n",fixture_moves.size());
    RECT shell{11,22,33,44};
    std::printf("shell:client-fail:%d\n",shell_client_rectangle(context.dialog,&shell));
    std::printf("shell:unchanged:%ld:%ld:%ld:%ld\n",shell.left,shell.top,
        shell.right,shell.bottom);
    fixture_client_rect_ok=true;
    fixture_origin_ok=false;
    std::printf("shell:origin-fail:%d\n",shell_client_rectangle(context.dialog,&shell));
    std::printf("shell:unchanged:%ld:%ld:%ld:%ld\n",shell.left,shell.top,
        shell.right,shell.bottom);
    fixture_origin_ok=true;
    fixture_width=640;fixture_height=480;
    std::printf("shell:valid:%d\n",shell_client_rectangle(context.dialog,&shell));
    std::printf("shell:rect:%ld:%ld:%ld:%ld\n",shell.left,shell.top,
        shell.right,shell.bottom);
}
