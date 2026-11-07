#include "../rust/stage8_b_object_editor_ffi.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Trace {
    std::vector<std::uint32_t> ids;
    std::uint32_t fail{UINT32_MAX};
    bool ready{},cleanup{},postload{true},entry{true};
    std::uint32_t prefix{};
};
std::uint64_t operation(void* opaque,std::uint32_t id) {
    auto& t=*static_cast<Trace*>(opaque);t.ids.push_back(id);
    if(id==t.fail)return 0;
    if(id==0||id==41||id==87||id==101||id==180||id==200||id==220||id==342||id==401)return t.ready;
    if(id==510||id==513||id==561||id==571||id==578||id==591)return 0;
    if(id==61)return t.prefix;
    if(id==83)return t.cleanup;
    if(id==88)return t.postload;
    if(id==89)return t.entry;
    return 1;
}
bool check(bool condition,const char* label) {
    if(!condition)std::fprintf(stderr,"object editor policy failure: %s\n",label);
    return condition;
}
int main() {
    Trace already{};already.ready=true;
    if(!check(a2fo_rs_stage8_b_object_editor_initialize(&already,operation)==1&&
              already.ids==std::vector<std::uint32_t>{0},
              "ready init avoids SDK field reads"))return 1;
    Trace missing{};missing.fail=8;
    if(!check(a2fo_rs_stage8_b_object_editor_initialize(&missing,operation)==0&&
              missing.ids.back()==14&&
              std::find(missing.ids.begin(),missing.ids.end(),9u)==missing.ids.end(),
              "short API struct stops before handler pointer"))return 1;
    Trace save_patch{};save_patch.fail=17;
    if(!check(a2fo_rs_stage8_b_object_editor_initialize(&save_patch,operation)==0&&
              save_patch.ids[save_patch.ids.size()-2]==19&&
              save_patch.ids.back()==20&&
              std::find(save_patch.ids.begin(),save_patch.ids.end(),18u)==save_patch.ids.end(),
              "save patch failure skips load patch yet publishes pass-through gate"))return 1;
    Trace legacy{};legacy.fail=22;
    if(!check(a2fo_rs_stage8_b_object_editor_initialize(&legacy,operation)==1&&
              legacy.ids[legacy.ids.size()-2]==28&&legacy.ids.back()==29,
              "optional legacy failure retains persistence and Fleet Ops bootstrap"))return 1;
    Trace installed{};
    if(!check(a2fo_rs_stage8_b_object_editor_initialize(&installed,operation)==1&&
              installed.ids[installed.ids.size()-2]==27&&installed.ids.back()==29,
              "legacy hook admission follows both persistence patches"))return 1;
    Trace inactive_read{};
    if(!check(a2fo_rs_stage8_b_object_editor_read_field(&inactive_read,operation)==1&&
              inactive_read.ids==std::vector<std::uint32_t>{40,41,42},
              "inactive load calls original before reader access"))return 1;
    Trace bad_reader{};bad_reader.ready=true;bad_reader.fail=45;
    if(!check(a2fo_rs_stage8_b_object_editor_read_field(&bad_reader,operation)==0&&
              bad_reader.ids.back()==45&&
              std::find(bad_reader.ids.begin(),bad_reader.ids.end(),49u)==bad_reader.ids.end(),
              "unreadable reader stops before entry erase"))return 1;
    Trace invalid_prefix{};invalid_prefix.ready=true;invalid_prefix.prefix=2;
    if(!check(a2fo_rs_stage8_b_object_editor_read_field(&invalid_prefix,operation)==0&&
              invalid_prefix.ids.back()==62,
              "invalid prefix refuses native first field"))return 1;
    Trace present{};present.ready=true;present.prefix=1;
    if(!check(a2fo_rs_stage8_b_object_editor_read_field(&present,operation)==1&&
              present.ids.back()==66&&
              std::find(present.ids.begin(),present.ids.end(),65u)!=present.ids.end(),
              "present prefix advances reader before native field"))return 1;
    Trace native_fail{};native_fail.ready=true;native_fail.fail=66;
    if(!check(a2fo_rs_stage8_b_object_editor_read_field(&native_fail,operation)==0&&
              native_fail.ids.back()==67,
              "failed native field removes loaded entry"))return 1;
    Trace no_event{};no_event.fail=80;
    a2fo_rs_stage8_b_object_editor_event(&no_event,operation);
    if(!check(no_event.ids==std::vector<std::uint32_t>{80},
              "null event precedes struct dereference"))return 1;
    Trace cleanup{};cleanup.cleanup=true;
    a2fo_rs_stage8_b_object_editor_event(&cleanup,operation);
    if(!check(cleanup.ids==std::vector<std::uint32_t>{80,81,82,83,84,85,86},
              "cleanup erases cached and accepted craft"))return 1;
    Trace dormant{};dormant.ready=false;
    a2fo_rs_stage8_b_object_editor_event(&dormant,operation);
    if(!check(dormant.ids.back()==87,
              "inactive POST_LOAD stops before kind and entry reads"))return 1;
    Trace postload{};postload.ready=true;
    a2fo_rs_stage8_b_object_editor_event(&postload,operation);
    if(!check(postload.ids.back()==91,
              "POST_LOAD refreshes handle before releasing loading gate"))return 1;
    Trace inactive_write{};
    if(!check(a2fo_rs_stage8_b_object_editor_write_field(&inactive_write,operation)==1&&
              inactive_write.ids==std::vector<std::uint32_t>{100,101,108},
              "inactive save passes directly to native float writer"))return 1;
    Trace failed_bytes{};failed_bytes.ready=true;failed_bytes.fail=107;
    if(!check(a2fo_rs_stage8_b_object_editor_write_field(&failed_bytes,operation)==0&&
              failed_bytes.ids.back()==107,
              "failed prefix write skips native float writer"))return 1;
    Trace no_state{};no_state.ready=true;no_state.fail=181;
    if(!check(a2fo_rs_stage8_b_object_editor_read_values(&no_state,operation)==0&&
              no_state.ids==std::vector<std::uint32_t>{180,181},
              "read_values checks state before directional or craft"))return 1;
    Trace no_shields{};no_shields.ready=true;no_shields.fail=185;
    if(!check(a2fo_rs_stage8_b_object_editor_read_values(&no_shields,operation)==1&&
              std::find(no_shields.ids.begin(),no_shields.ids.end(),186u)==no_shields.ids.end()&&
              no_shields.ids.back()==196,
              "read_values skips enabled query but publishes false directional"))return 1;
    Trace bad_apply{};bad_apply.ready=true;bad_apply.fail=202;
    if(!check(a2fo_rs_stage8_b_object_editor_apply_values(&bad_apply,operation)==0&&
              bad_apply.ids.back()==202,
              "invalid state stops before entry allocation"))return 1;
    Trace set_fail{};set_fail.ready=true;set_fail.fail=208;
    if(!check(a2fo_rs_stage8_b_object_editor_apply_values(&set_fail,operation)==0&&
              set_fail.ids.back()==208,
              "shield setter failure leaves entry unassigned"))return 1;
    Trace no_loaded{};no_loaded.ready=true;no_loaded.fail=221;
    if(!check(a2fo_rs_stage8_b_object_editor_take_loaded(&no_loaded,operation)==0&&
              no_loaded.ids==std::vector<std::uint32_t>{220,221},
              "missing output precedes struct-size read"))return 1;
    Trace take{};take.ready=true;
    if(!check(a2fo_rs_stage8_b_object_editor_take_loaded(&take,operation)==1&&
              take.ids[take.ids.size()-2]==226&&take.ids.back()==227,
              "loaded snapshot clears pending after output copy"))return 1;
    Trace bad_facing{};bad_facing.fail=246;
    if(!check(a2fo_rs_stage8_b_object_editor_read_shield_controls(&bad_facing,operation)==0&&
              bad_facing.ids==std::vector<std::uint32_t>{240,244,245,246},
              "facing parse stops before max parse and flag commit"))return 1;
    Trace facings{};
    if(!check(a2fo_rs_stage8_b_object_editor_read_shield_controls(&facings,operation)==1&&
              facings.ids[facings.ids.size()-2]==248&&facings.ids.back()==249,
              "four facings commit flags before final validation"))return 1;
    Trace empty_label{};empty_label.fail=301;
    if(!check(a2fo_rs_stage8_b_object_editor_accept_controls(&empty_label,operation)==0&&
              empty_label.ids==std::vector<std::uint32_t>{300,301,302},
              "empty native label warns before draft reads"))return 1;
    Trace no_entry{};no_entry.fail=317;
    if(!check(a2fo_rs_stage8_b_object_editor_accept_controls(&no_entry,operation)==0&&
              no_entry.ids.back()==318&&
              std::find(no_entry.ids.begin(),no_entry.ids.end(),319u)==no_entry.ids.end(),
              "allocation failure warns before shield snapshot"))return 1;
    Trace no_setter{};no_setter.fail=321;
    if(!check(a2fo_rs_stage8_b_object_editor_accept_controls(&no_setter,operation)==0&&
              no_setter.ids.back()==323&&
              std::find(no_setter.ids.begin(),no_setter.ids.end(),322u)==no_setter.ids.end(),
              "absent shield setter skips invocation and identity commit"))return 1;
    Trace no_gateway{};no_gateway.fail=341;no_gateway.ready=true;
    a2fo_rs_stage8_b_object_editor_dialog_proc(&no_gateway,operation);
    if(!check(no_gateway.ids==std::vector<std::uint32_t>{340,341},
              "dialog gate checks gateway before readiness"))return 1;
    Trace inactive_dialog{};
    a2fo_rs_stage8_b_object_editor_dialog_proc(&inactive_dialog,operation);
    if(!check(inactive_dialog.ids==std::vector<std::uint32_t>{340,341,342,344},
              "inactive dialog passes directly to native gateway"))return 1;
    Trace init_dialog{};init_dialog.ready=true;
    a2fo_rs_stage8_b_object_editor_dialog_proc(&init_dialog,operation);
    if(!check(init_dialog.ids==std::vector<std::uint32_t>{340,341,342,343,345,344,346},
              "dialog initialization calls native gateway before adding controls"))return 1;
    Trace no_craft{};no_craft.fail=503;
    if(!check(a2fo_rs_stage8_b_object_editor_add_controls(&no_craft,operation)==0&&
              no_craft.ids==std::vector<std::uint32_t>{500,501,502,503},
              "noncraft admission stops before dialog/window access"))return 1;
    Trace no_rect{};no_rect.fail=505;
    if(!check(a2fo_rs_stage8_b_object_editor_add_controls(&no_rect,operation)==0&&
              no_rect.ids.back()==514&&
              std::find(no_rect.ids.begin(),no_rect.ids.end(),506u)==no_rect.ids.end(),
              "window rect failure clears owner before other geometry reads"))return 1;
    Trace layout{};
    if(!check(a2fo_rs_stage8_b_object_editor_add_controls(&layout,operation)==1&&
              layout.ids[layout.ids.size()-2]==594&&layout.ids.back()==595&&
              std::count(layout.ids.begin(),layout.ids.end(),539u)==4&&
              std::count(layout.ids.begin(),layout.ids.end(),570u)==4,
              "layout creates each facing then publishes dialog active"))return 1;
    Trace missing_gateway{};missing_gateway.fail=400;
    a2fo_rs_stage8_b_object_editor_dialog_destructor(&missing_gateway,operation);
    if(!check(missing_gateway.ids==std::vector<std::uint32_t>{400},
              "missing destructor gateway skips all object reads"))return 1;
    Trace destroy{};destroy.ready=true;
    a2fo_rs_stage8_b_object_editor_dialog_destructor(&destroy,operation);
    if(!check(destroy.ids.back()==412&&
              std::find(destroy.ids.begin(),destroy.ids.end(),407u)!=destroy.ids.end(),
              "destructor snapshots admission before native call then clears accepted"))return 1;
    std::puts("object editor policy: 34 branches PASS");
}
