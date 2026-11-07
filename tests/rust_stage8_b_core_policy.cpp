#include "../rust/stage8_b_core_ffi.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Trace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    std::uint32_t fail_secondary{UINT32_MAX};
    bool existing{};
};
struct SaveTrace {
    std::vector<std::uint32_t> ids;
    std::vector<std::uint64_t> entry_indexes;
    std::uint32_t fail{UINT32_MAX};
    bool reporting{},changed{},save{true};
};
struct NotificationTrace {
    std::vector<std::uint32_t> ids;
    bool present{true},item_change{},initializing{},custom{},prepaint{};
    std::uint32_t state{2},color{UINT32_MAX};
};
struct DialogTrace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    unsigned loop_reads{};
};
struct ImportTrace {
    std::vector<std::uint32_t> ids;
    std::vector<std::uint64_t> indexes;
    bool module{true};
};
struct LaunchTrace {
    std::vector<std::uint32_t> ids;
    bool info{true},suffix_empty{},success{true},command_empty{true};
};
struct MessageTrace {
    std::vector<std::uint32_t> ids;
    std::uint32_t message{1005};
    bool context{true},save{true};
};
std::uint64_t message_operation(void* opaque,std::uint32_t id) {
    auto& t=*static_cast<MessageTrace*>(opaque);t.ids.push_back(id);
    if(id==1001)return 0;
    if(id==1003)return t.context;
    if(id==1004||id==1020)return 77;
    if(id==1005||id==1007||id==1013||id==1015||id==1018||
       id==1021||id==1022)return id==t.message;
    if(id==1008||id==1009)return 1;
    if(id==1010)return t.save;
    if(id==1014)return 32;
    if(id==1017)return 1;
    return 0;
}
std::uint64_t launch_operation(void* opaque,std::uint32_t id) {
    auto& t=*static_cast<LaunchTrace*>(opaque);t.ids.push_back(id);
    if(id==940)return t.info;
    if(id==901)return t.suffix_empty;
    if(id==911)return t.command_empty;
    if(id==912)return 1;
    if(id==920||id==934||id==948||id==955)return t.success;
    return 1;
}
std::uint64_t import_operation(void* opaque,std::uint32_t id,std::uint64_t index) {
    auto& t=*static_cast<ImportTrace*>(opaque);t.ids.push_back(id);
    if(id==800)return t.module;
    if(id==801||id==802)t.indexes.push_back(index);
    if(id==801)return index==1||index==6;
    return 0;
}
std::uint64_t dialog_operation(void* opaque,std::uint32_t id,std::uint64_t) {
    auto& t=*static_cast<DialogTrace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==707)return 2;
    if(id==708)return 1;
    if(id==723)return t.loop_reads++==0;
    if(id==724)return 1;
    if(id==725)return 0;
    return 1;
}
std::uint64_t notification_operation(void* opaque,std::uint32_t id,std::uint64_t arg) {
    auto& t=*static_cast<NotificationTrace*>(opaque);t.ids.push_back(id);
    if(id==400||id==401)return t.present;
    if(id==402)return t.item_change;
    if(id==403)return !t.initializing;
    if(id==404||id==414)return 0;
    if(id==405||id==415)return 1;
    if(id==406)return 0;
    if(id==407)return 0x2000;
    if(id==408||id==416)return t.state;
    if(id==409)return 1;
    if(id==410)return t.custom;
    if(id==411)return t.prepaint;
    if(id==412)return 32;
    if(id==413)return !t.prepaint;
    if(id==417){t.color=static_cast<std::uint32_t>(arg);return 0;}
    if(id==419)return 2;
    return 0;
}
std::uint8_t ascii_space(std::uint8_t value) {
    return value==' '||value=='\t'||value=='\n';
}
std::uint64_t save_operation(void* opaque,std::uint32_t id,std::uint64_t index) {
    auto& t=*static_cast<SaveTrace*>(opaque);t.ids.push_back(id);
    if(id>=208&&id<=211)t.entry_indexes.push_back(index);
    if(id==t.fail)return 0;
    if(id==201||id==203||id==204)return t.reporting;
    if(id==205)return 0;
    if(id==207)return 2;
    if(id==209)return index==1;
    if(id==222)return t.changed;
    if(id==225)return 0;
    if(id==232)return t.save;
    if(id==244)return 1;
    return 1;
}
std::uint64_t operation(void* opaque,std::uint32_t id) {
    auto& t=*static_cast<Trace*>(opaque);
    t.ids.push_back(id);
    if(id==t.fail||id==t.fail_secondary)return 0;
    if(id==43)return t.existing;
    if(id==66)return 1;
    if(id==37)return 440;
    if(id==38)return 288;
    if(id==39)return 100;
    if(id==40)return 20;
    return 1;
}
bool check(bool value,const char* label) {
    if(!value)std::fprintf(stderr,"core policy failure: %s\n",label);
    return value;
}
int main() {
    Trace absent{};absent.fail=0;
    if(!check(a2fo_rs_stage8_b_module_menu_install(&absent,operation)==0&&
              absent.ids==std::vector<std::uint32_t>{0},
              "missing image precedes publication"))return 1;
    Trace signature{};signature.fail=5;
    if(!check(a2fo_rs_stage8_b_module_menu_install(&signature,operation)==0&&
              signature.ids.back()==20&&signature.ids[1]==1,
              "signature gate after native publication"))return 1;
    Trace hook{};hook.fail=21;
    if(!check(a2fo_rs_stage8_b_module_menu_install(&hook,operation)==0&&
              hook.ids[hook.ids.size()-2]==23&&hook.ids.back()==24,
              "all three checked patches attempted"))return 1;
    Trace import{};import.fail=25;
    if(!check(a2fo_rs_stage8_b_module_menu_install(&import,operation)==1&&
              import.ids[import.ids.size()-2]==26&&import.ids.back()==27,
              "import failure logs and preserves primary install"))return 1;
    Trace button{};
    a2fo_rs_stage8_b_module_menu_button(&button,operation);
    if(!check(button.ids.back()==68&&
              button.ids[0]==30&&button.ids[1]==31&&
              button.ids[button.ids.size()-3]==64,
              "button construction precedes controls and diagnostic"))return 1;
    Trace reuse{};reuse.existing=true;
    a2fo_rs_stage8_b_module_menu_button(&reuse,operation);
    if(!check(reuse.ids.back()==68&&
              std::find(reuse.ids.begin(),reuse.ids.end(),50u)==reuse.ids.end(),
              "owned button reuse avoids constructor"))return 1;
    Trace no_window{};no_window.fail=32;
    a2fo_rs_stage8_b_module_menu_button(&no_window,operation);
    if(!check(no_window.ids==std::vector<std::uint32_t>{30,31,32,33},
              "window gate follows native field reads"))return 1;
    Trace unreadable{};unreadable.fail=35;
    a2fo_rs_stage8_b_module_menu_button(&unreadable,operation);
    if(!check(unreadable.ids.back()==36&&
              std::find(unreadable.ids.begin(),unreadable.ids.end(),37u)==unreadable.ids.end(),
              "launch unreadable gate protects bounds reads"))return 1;
    Trace invalid{};invalid.fail=39;
    a2fo_rs_stage8_b_module_menu_button(&invalid,operation);
    if(!check(invalid.ids.back()==41&&
              std::find(invalid.ids.begin(),invalid.ids.end(),42u)==invalid.ids.end(),
              "invalid bounds stop before allocation"))return 1;
    Trace construct{};construct.fail=51;
    a2fo_rs_stage8_b_module_menu_button(&construct,operation);
    if(!check(construct.ids.back()==53&&
              std::find(construct.ids.begin(),construct.ids.end(),54u)==construct.ids.end(),
              "constructor failure skips component insertion"))return 1;
    Trace launch_block{};launch_block.fail=80;
    if(!check(a2fo_rs_stage8_b_module_menu_launch_admit(&launch_block,operation)==0&&
              launch_block.ids==std::vector<std::uint32_t>{80,81,82},
              "launch block reads owner after policy result"))return 1;
    Trace launch_ok{};
    if(!check(a2fo_rs_stage8_b_module_menu_launch_admit(&launch_ok,operation)==1&&
              launch_ok.ids==std::vector<std::uint32_t>{80},
              "launch admit skips dialog"))return 1;
    Trace launch_flags{};launch_flags.fail=90;
    a2fo_rs_stage8_b_module_menu_launch_commit(&launch_flags,operation);
    if(!check(launch_flags.ids==std::vector<std::uint32_t>{90,91,92,93,94,95},
              "flag reads precede atomic publication and gateway"))return 1;
    Trace modules_click{};
    a2fo_rs_stage8_b_module_menu_click(&modules_click,operation);
    if(!check(modules_click.ids==std::vector<std::uint32_t>{100,101,102},
              "Modules button avoids toggle and shortcut checks"))return 1;
    Trace nointro_click{};nointro_click.fail=100;
    a2fo_rs_stage8_b_module_menu_click(&nointro_click,operation);
    if(!check(nointro_click.ids==std::vector<std::uint32_t>{100,103,104,105,106,107},
              "nointro click toggles and logs"))return 1;
    Trace shortcut{};shortcut.fail=100;
    // The remaining branches are exercised by a callback that identifies only
    // the shortcut button and keeps the native sequence observable.
    auto shortcut_op=[](void* opaque,std::uint32_t id)->std::uint64_t {
        auto& t=*static_cast<Trace*>(opaque);t.ids.push_back(id);
        if(id==100||id==103||id==108)return 0;
        if(id==122)return 1;
        return 1;
    };
    a2fo_rs_stage8_b_module_menu_click(&shortcut,shortcut_op);
    if(!check(shortcut.ids.back()==123&&
              std::find(shortcut.ids.begin(),shortcut.ids.end(),119u)!=shortcut.ids.end(),
              "shortcut native click precedes flag amendment"))return 1;
    if(!check(a2fo_rs_stage8_b_module_menu_button_left(100,90)==8&&
              a2fo_rs_stage8_b_module_menu_button_left(440,100)==322,
              "button left bounds policy"))return 1;
    SaveTrace saved{};
    if(!check(a2fo_rs_stage8_b_module_menu_save(&saved,save_operation)==1&&
              saved.ids.back()==245&&saved.entry_indexes==
                  std::vector<std::uint64_t>{0,0,0,0,1,1}&&
              std::find(saved.ids.begin(),saved.ids.end(),235u)!=saved.ids.end(),
              "dialog Save filters entries then releases lock before live apply"))return 1;
    SaveTrace busy{};busy.fail=223;
    if(!check(a2fo_rs_stage8_b_module_menu_save(&busy,save_operation)==0&&
              busy.ids[busy.ids.size()-2]==235&&busy.ids.back()==236,
              "busy LogMailer lock released before error dialog"))return 1;
    SaveTrace failed{};failed.changed=true;failed.reporting=true;failed.save=false;
    if(!check(a2fo_rs_stage8_b_module_menu_save(&failed,save_operation)==0&&
              std::find(failed.ids.begin(),failed.ids.end(),233u)!=failed.ids.end()&&
              failed.ids.back()==236,
              "failed module save rolls back changed reporting section"))return 1;
    SaveTrace preferences{};preferences.changed=true;preferences.reporting=true;
    preferences.fail=229;
    if(!check(a2fo_rs_stage8_b_module_menu_save(&preferences,save_operation)==0&&
              std::find(preferences.ids.begin(),preferences.ids.end(),232u)==preferences.ids.end()&&
              preferences.ids.back()==236,
              "failed preferences prevent module save"))return 1;
    Trace existing_flags{};
    a2fo_rs_stage8_b_module_menu_flags(&existing_flags,operation);
    if(!check(existing_flags.ids.back()==313&&
              std::find(existing_flags.ids.begin(),existing_flags.ids.end(),315u)==existing_flags.ids.end(),
              "owned native flags reuse controls"))return 1;
    Trace new_flags{};new_flags.fail=306;
    a2fo_rs_stage8_b_module_menu_flags(&new_flags,operation);
    if(!check(new_flags.ids.back()==322&&
              std::find(new_flags.ids.begin(),new_flags.ids.end(),319u)!=new_flags.ids.end(),
              "native flags allocation hides fallback controls"))return 1;
    Trace fallback_flags{};fallback_flags.fail=305;
    a2fo_rs_stage8_b_module_menu_flags(&fallback_flags,operation);
    if(!check(fallback_flags.ids.back()==332&&
              std::find(fallback_flags.ids.begin(),fallback_flags.ids.end(),315u)==fallback_flags.ids.end(),
              "missing native class uses Jet fallback"))return 1;
    int row_left=0,row_top=0;
    a2fo_rs_stage8_b_module_menu_flag_row(322,288,100,24,&row_left,&row_top);
    if(!check(row_left==172&&row_top==291,
              "flag row fits left of Modules"))return 1;
    a2fo_rs_stage8_b_module_menu_toggle_row(100,288,100,24,&row_left,&row_top);
    if(!check(row_left==100&&row_top==258,
              "fallback row moves above when left space is unavailable"))return 1;
    Trace jet_reuse{};
    if(!check(a2fo_rs_stage8_b_module_menu_jet_flags(&jet_reuse,operation)==1&&
              jet_reuse.ids.back()==353&&
              std::find(jet_reuse.ids.begin(),jet_reuse.ids.end(),355u)==jet_reuse.ids.end(),
              "Jet fallback reuse retains native component"))return 1;
    Trace jet_new{};jet_new.fail=343;
    if(!check(a2fo_rs_stage8_b_module_menu_jet_flags(&jet_new,operation)==1&&
              jet_new.ids.back()==367,
              "Jet fallback allocation destroys Win32 controls after success"))return 1;
    Trace jet_failure{};jet_failure.fail=343;jet_failure.fail_secondary=357;
    if(!check(a2fo_rs_stage8_b_module_menu_jet_flags(&jet_failure,operation)==0&&
              jet_failure.ids.back()==365&&
              std::find(jet_failure.ids.begin(),jet_failure.ids.end(),358u)==jet_failure.ids.end(),
              "first Jet allocation failure still hides partial control"))return 1;
    NotificationTrace missing_header{};missing_header.present=false;
    if(!check(a2fo_rs_stage8_b_module_menu_notification(&missing_header,notification_operation)==0&&
              missing_header.ids==std::vector<std::uint32_t>{400},
              "null notify header stops before HWND read"))return 1;
    NotificationTrace locked_change{};locked_change.item_change=true;
    if(!check(a2fo_rs_stage8_b_module_menu_notification(&locked_change,notification_operation)==1&&
              locked_change.ids.back()==409,
              "locked check state change vetoed"))return 1;
    NotificationTrace initialized{};initialized.item_change=true;initialized.initializing=true;
    if(!check(a2fo_rs_stage8_b_module_menu_notification(&initialized,notification_operation)==0&&
              std::find(initialized.ids.begin(),initialized.ids.end(),404u)==initialized.ids.end(),
              "initializing list skips state read"))return 1;
    NotificationTrace prepaint{};prepaint.custom=true;prepaint.prepaint=true;
    if(!check(a2fo_rs_stage8_b_module_menu_notification(&prepaint,notification_operation)==32&&
              prepaint.ids.back()==412,
              "prepaint requests item notification"))return 1;
    NotificationTrace conflict{};conflict.custom=true;conflict.state=4;
    if(!check(a2fo_rs_stage8_b_module_menu_notification(&conflict,notification_operation)==2&&
              conflict.color==0&&conflict.ids.back()==419,
              "conflict item receives warning color"))return 1;
    if(!check(a2fo_rs_stage8_b_module_menu_state_locked(5)==1&&
              a2fo_rs_stage8_b_module_menu_state_locked(1)==0&&
              a2fo_rs_stage8_b_module_menu_state_checked(2)==1&&
              a2fo_rs_stage8_b_module_menu_state_checked(3)==0,
              "policy state lock and check mapping"))return 1;
    const char command[]="game /nointro-extra -nointro /noai";
    const char switch_name[]="/nointro";
    if(!check(a2fo_rs_stage8_b_module_menu_has_switch(
                  reinterpret_cast<const std::uint8_t*>(command),sizeof(command)-1,
                  reinterpret_cast<const std::uint8_t*>(switch_name),sizeof(switch_name)-1,
                  ascii_space)==0,
              "command switch requires both token boundaries"))return 1;
    const char bounded[]="game /nointro /noai";
    if(!check(a2fo_rs_stage8_b_module_menu_has_switch(
                  reinterpret_cast<const std::uint8_t*>(bounded),sizeof(bounded)-1,
                  reinterpret_cast<const std::uint8_t*>(switch_name),sizeof(switch_name)-1,
                  ascii_space)==1,
              "command switch finds bounded token"))return 1;
    Trace append{};append.fail=501;
    a2fo_rs_stage8_b_module_menu_append_switches(&append,operation);
    if(!check(append.ids==std::vector<std::uint32_t>{500,501,502,503,504},
              "append nointro then rechecks updated command for noai"))return 1;
    Trace selected{};
    if(!check(a2fo_rs_stage8_b_module_menu_selected_mod(&selected,operation)==1&&
              selected.ids.back()==611,
              "selected grid row read follows VMT admission"))return 1;
    Trace no_grid{};no_grid.fail=602;
    if(!check(a2fo_rs_stage8_b_module_menu_selected_mod(&no_grid,operation)==0&&
              no_grid.ids.back()==602,
              "null grid skips VMT dereference"))return 1;
    Trace base_mod{};
    if(!check(a2fo_rs_stage8_b_module_menu_resolve_mod(&base_mod,operation)==1&&
              base_mod.ids==std::vector<std::uint32_t>{620,622,623,635},
              "base Data row uses global root"))return 1;
    Trace mod_failed{};mod_failed.fail=620;
    if(!check(a2fo_rs_stage8_b_module_menu_resolve_mod(&mod_failed,operation)==0&&
              mod_failed.ids==std::vector<std::uint32_t>{620,621},
              "missing selection sets error before path access"))return 1;
    auto custom_mod=[](void* opaque,std::uint32_t id)->std::uint64_t {
        auto& t=*static_cast<Trace*>(opaque);t.ids.push_back(id);
        if(id==622||id==626||id==632)return 0;
        return 1;
    };
    Trace custom{};
    if(!check(a2fo_rs_stage8_b_module_menu_resolve_mod(&custom,custom_mod)==1&&
              custom.ids.back()==635&&
              std::find(custom.ids.begin(),custom.ids.end(),630u)!=custom.ids.end(),
              "custom mod discovery precedes canonical root test"))return 1;
    DialogTrace no_selection{};no_selection.fail=700;
    a2fo_rs_stage8_b_module_menu_dialog(&no_selection,dialog_operation);
    if(!check(no_selection.ids==std::vector<std::uint32_t>{700,701},
              "dialog selection failure precedes module discovery"))return 1;
    DialogTrace no_class{};no_class.fail=711;
    a2fo_rs_stage8_b_module_menu_dialog(&no_class,dialog_operation);
    if(!check(no_class.ids.back()==712&&
              std::find(no_class.ids.begin(),no_class.ids.end(),713u)==no_class.ids.end(),
              "dialog class failure skips common-controls startup"))return 1;
    DialogTrace no_dialog_window{};no_dialog_window.fail=717;
    a2fo_rs_stage8_b_module_menu_dialog(&no_dialog_window,dialog_operation);
    if(!check(no_dialog_window.ids.back()==718&&
              std::find(no_dialog_window.ids.begin(),no_dialog_window.ids.end(),719u)==no_dialog_window.ids.end(),
              "window creation failure skips owner disable"))return 1;
    DialogTrace shown{};
    a2fo_rs_stage8_b_module_menu_dialog(&shown,dialog_operation);
    if(!check(shown.ids.back()==729&&shown.loop_reads==2&&
              std::find(shown.ids.begin(),shown.ids.end(),726u)!=shown.ids.end(),
              "dialog pumps native message then restores owner"))return 1;
    ImportTrace imports{};
    if(!check(a2fo_rs_stage8_b_module_menu_import_for(&imports,import_operation)==2&&
              imports.ids.back()==802&&imports.indexes.back()==6,
              "all seven process APIs attempted and successful imports logged"))return 1;
    ImportTrace missing_module{};missing_module.module=false;
    if(!check(a2fo_rs_stage8_b_module_menu_import_for(&missing_module,import_operation)==0&&
              missing_module.ids==std::vector<std::uint32_t>{800},
              "missing PE image skips import patch reads"))return 1;
    Trace all_imports{};
    if(!check(a2fo_rs_stage8_b_module_menu_import_all(&all_imports,operation)==1&&
              all_imports.ids==std::vector<std::uint32_t>{810,811,812,813,814,815},
              "Fleet Ops import scan precedes Armada scan and audit"))return 1;
    LaunchTrace direct{};direct.suffix_empty=true;
    a2fo_rs_stage8_b_module_menu_launch_intercept(&direct,launch_operation,0);
    if(!check(direct.ids==std::vector<std::uint32_t>{901,902},
              "no pending flags invoke unmodified CreateProcess"))return 1;
    LaunchTrace create_a{};
    a2fo_rs_stage8_b_module_menu_launch_intercept(&create_a,launch_operation,0);
    if(!check(create_a.ids.back()==922&&
              std::find(create_a.ids.begin(),create_a.ids.end(),913u)!=create_a.ids.end()&&
              std::find(create_a.ids.begin(),create_a.ids.end(),918u)!=create_a.ids.end(),
              "CreateProcessA quotes missing command and clears after success"))return 1;
    LaunchTrace create_w{};create_w.success=false;
    a2fo_rs_stage8_b_module_menu_launch_intercept(&create_w,launch_operation,1);
    if(!check(create_w.ids.back()==920&&
              std::find(create_w.ids.begin(),create_w.ids.end(),917u)==create_w.ids.end(),
              "failed CreateProcessW preserves pending flags"))return 1;
    LaunchTrace shell{};
    a2fo_rs_stage8_b_module_menu_launch_intercept(&shell,launch_operation,2);
    if(!check(shell.ids==std::vector<std::uint32_t>{901,930,931,932,933,934,935,922},
              "ShellExecuteA appends before native call and clears on success"))return 1;
    LaunchTrace null_ex{};null_ex.info=false;
    a2fo_rs_stage8_b_module_menu_launch_intercept(&null_ex,launch_operation,4);
    if(!check(null_ex.ids==std::vector<std::uint32_t>{940,902},
              "null ShellExecuteEx struct bypasses flag read"))return 1;
    LaunchTrace shell_ex{};
    a2fo_rs_stage8_b_module_menu_launch_intercept(&shell_ex,launch_operation,5);
    if(!check(shell_ex.ids.back()==922&&
              std::find(shell_ex.ids.begin(),shell_ex.ids.end(),947u)!=shell_ex.ids.end()&&
              std::find(shell_ex.ids.begin(),shell_ex.ids.end(),943u)==shell_ex.ids.end(),
              "ShellExecuteExW copies outputs before clearing flags"))return 1;
    LaunchTrace win_exec{};
    a2fo_rs_stage8_b_module_menu_launch_intercept(&win_exec,launch_operation,6);
    if(!check(win_exec.ids==std::vector<std::uint32_t>{901,950,951,952,953,954,955,956,922},
              "WinExec consumes flags only after successful native result"))return 1;
    MessageTrace missing_context{};missing_context.context=false;
    if(!check(a2fo_rs_stage8_b_module_menu_dialog_message(&missing_context,message_operation)==77&&
              missing_context.ids==std::vector<std::uint32_t>{1000,1001,1003,1004},
              "dialog unknown context dispatches native default"))return 1;
    MessageTrace create_msg{};
    if(!check(a2fo_rs_stage8_b_module_menu_dialog_message(&create_msg,message_operation)==0&&
              create_msg.ids.back()==1006,
              "WM_CREATE dispatches native control construction"))return 1;
    MessageTrace save_msg{};save_msg.message=1007;
    if(!check(a2fo_rs_stage8_b_module_menu_dialog_message(&save_msg,message_operation)==0&&
              save_msg.ids.back()==1011,
              "Save destroys dialog only after successful policy save"))return 1;
    MessageTrace failed_save{};failed_save.message=1007;failed_save.save=false;
    if(!check(a2fo_rs_stage8_b_module_menu_dialog_message(&failed_save,message_operation)==0&&
              failed_save.ids.back()==1010,
              "failed Save keeps dialog open"))return 1;
    MessageTrace notify{};notify.message=1013;
    if(!check(a2fo_rs_stage8_b_module_menu_dialog_message(&notify,message_operation)==32&&
              notify.ids.back()==1014,
              "notification result passes through unchanged"))return 1;
    MessageTrace paint{};paint.message=1015;
    if(!check(a2fo_rs_stage8_b_module_menu_dialog_message(&paint,message_operation)==1&&
              paint.ids.back()==1017,
              "owner draw keeps native TRUE result"))return 1;
    std::puts("core module menu policy: 63 branches PASS");
}
