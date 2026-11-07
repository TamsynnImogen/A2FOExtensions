#include "../rust/stage8_b_loading_ffi.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>

struct Scenario {
    std::vector<std::uint32_t> ids;
    std::vector<std::uint32_t> args;
    std::uint32_t fail{};
    std::uint32_t branch{};
};

struct BarScenario {
    std::vector<std::uint32_t> ids;
    std::uint32_t disabled{};
    std::uint32_t presentation{1};
    std::uint32_t log_result{1};
};
std::uint64_t bar_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& s=*static_cast<BarScenario*>(opaque);
    s.ids.push_back(id);
    if(id==72)return !s.disabled;
    if(id==76)return s.presentation;
    if(id==77||id==78)return s.log_result;
    return 1;
}

struct SplashScenario {
    std::vector<std::uint32_t> ids;
    bool continuing{};
    bool armed{true};
};
std::uint64_t splash_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& s=*static_cast<SplashScenario*>(opaque);
    s.ids.push_back(id);
    if(id==86)return !s.continuing;
    if(id==87||id==96||id==100||id==103)return s.armed;
    if(id==93)return s.continuing;
    return 1;
}

struct PresentScenario {
    std::vector<std::uint32_t> ids;
    std::uint32_t width{800},height{600};
    bool restart{},layered{},paint{true};
};
std::uint64_t present_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& s=*static_cast<PresentScenario*>(opaque);
    s.ids.push_back(id);
    switch(id){
    case 112:case 113:case 119:case 146:case 158:case 163:case 164:return 1;
    case 114:return s.width;
    case 115:return s.height;
    case 118:return s.restart;
    case 120:case 121:case 122:case 135:case 143:case 149:case 160:return 0;
    case 152:return s.layered;
    case 154:return s.paint;
    case 155:case 156:case 157:return 0;
    case 161:return 1;
    default:return 0;
    }
}

struct RuntimeScenario {
    std::vector<std::uint32_t> ids,args;
    std::uint32_t fail{};
};
std::uint64_t runtime_operation(void* opaque,std::uint32_t id,std::uint32_t arg){
    auto& s=*static_cast<RuntimeScenario*>(opaque);
    s.ids.push_back(id);s.args.push_back(arg);
    if(id==s.fail)return 0;
    if(id==172)return 0; // idle deadline reached
    if(id==175||id==220)return 0x3f000000u; // finite 0.5f
    if(id==208)return arg!=0; // one active player
    if(id==215)return 9; // invalid team normalization path
    if(id==204||id==205||id==225||id==228)return 0;
    return 1;
}

struct BindingsScenario {
    std::vector<std::uint32_t> ids;
    bool direct{true},known{true},core{};
};
struct StatusScenario {std::vector<std::uint32_t> ids;bool enabled{true},frame{true};};
std::uint64_t status_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& s=*static_cast<StatusScenario*>(opaque);s.ids.push_back(id);
    if(id==249)return s.enabled;
    if(id==252)return s.frame;
    return 1;
}
std::uint64_t bindings_operation(void* opaque,std::uint32_t id,std::uint32_t){
    auto& s=*static_cast<BindingsScenario*>(opaque);s.ids.push_back(id);
    if(id==232)return s.direct;
    if(id==239)return s.known;
    if(id==240)return s.core;
    return 1;
}

std::uint64_t operation(void* opaque,std::uint32_t id,std::uint32_t arg){
    auto& s=*static_cast<Scenario*>(opaque);
    s.ids.push_back(id);s.args.push_back(arg);
    if(id==s.fail)return 0;
    if(id==30)return s.branch==1;
    if(id==35)return s.branch==2?0x00e9u:0xc368u;
    if(id==39)return s.branch!=3;
    if(id==59)return s.branch!=4;
    return 1;
}

bool check(bool valid,const char* name){
    if(!valid)std::fprintf(stderr,"loading policy failure: %s\n",name);
    return valid;
}

int main(){
    Scenario direct{};direct.branch=1;
    if(!check(a2fo_rs_stage8_b_loading_release(&direct,operation)==1&&
              direct.ids==std::vector<std::uint32_t>{29,30,31,32},"direct release"))return 1;
    Scenario chained{};
    auto transfer_arg=[](const Scenario& s){
        const auto it=std::find(s.ids.begin(),s.ids.end(),36u);
        return it==s.ids.end()?0u:s.args[static_cast<std::size_t>(it-s.ids.begin())];
    };
    if(!check(a2fo_rs_stage8_b_loading_release(&chained,operation)==1&&
              chained.ids.back()==48&&transfer_arg(chained)==1,"chained release"))return 1;
    Scenario jump{};jump.branch=2;
    if(!check(a2fo_rs_stage8_b_loading_release(&jump,operation)==1&&
              transfer_arg(jump)==2,"jump release"))return 1;
    Scenario checked{};checked.branch=3;
    if(!check(a2fo_rs_stage8_b_loading_release(&checked,operation)==1&&
              checked.ids.back()==48,"core observer check"))return 1;
    Scenario rollback{};rollback.fail=46;
    if(!check(a2fo_rs_stage8_b_loading_release(&rollback,operation)==0&&
              rollback.ids.back()==47,"release rollback"))return 1;
    Scenario animation{};
    if(!check(a2fo_rs_stage8_b_loading_animation(&animation,operation)==1&&
              animation.ids.back()==71&&animation.args.back()==4,
              "splash and inner sites"))return 1;
    Scenario transient{};transient.branch=4;
    if(!check(a2fo_rs_stage8_b_loading_animation(&transient,operation)==1&&
              std::find(transient.ids.begin(),transient.ids.end(),62u)!=transient.ids.end()&&
              transient.ids.back()==71,
              "transient texture fallback"))return 1;
    Scenario partial{};partial.fail=57;
    if(!check(a2fo_rs_stage8_b_loading_animation(&partial,operation)==0&&
              partial.ids.back()==57,"partial splash hook"))return 1;
    BarScenario active{};
    if(!check(a2fo_rs_stage8_b_loading_bar(&active,bar_operation)==1&&
              active.ids==std::vector<std::uint32_t>{72,73,74,75,76,77,80,81,82,83,84,85},
              "active bar order"))return 1;
    BarScenario failed{};failed.presentation=2;
    if(!check(a2fo_rs_stage8_b_loading_bar(&failed,bar_operation)==1&&
              std::find(failed.ids.begin(),failed.ids.end(),79u)!=failed.ids.end()&&
              std::find(failed.ids.begin(),failed.ids.end(),77u)==failed.ids.end(),
              "caught presentation"))return 1;
    BarScenario log_failed{};log_failed.log_result=2;
    if(!check(a2fo_rs_stage8_b_loading_bar(&log_failed,bar_operation)==1&&
              std::find(log_failed.ids.begin(),log_failed.ids.end(),79u)!=log_failed.ids.end(),
              "caught report"))return 1;
    BarScenario disabled{};disabled.disabled=1;
    if(!check(a2fo_rs_stage8_b_loading_bar(&disabled,bar_operation)==1&&
              disabled.ids==std::vector<std::uint32_t>{72,81,82,83,84,85},
              "disabled forwards"))return 1;
    SplashScenario first{};
    if(!check(a2fo_rs_stage8_b_loading_splash_begin(&first,splash_operation)==1&&
              std::find(first.ids.begin(),first.ids.end(),92u)!=first.ids.end()&&
              first.ids.back()==102,"new splash handover"))return 1;
    SplashScenario continuation{};continuation.continuing=true;
    if(!check(a2fo_rs_stage8_b_loading_splash_begin(&continuation,splash_operation)==1&&
              std::find(continuation.ids.begin(),continuation.ids.end(),92u)==continuation.ids.end()&&
              continuation.ids.back()==101,"continuing splash"))return 1;
    SplashScenario ended{};
    if(!check(a2fo_rs_stage8_b_loading_splash_end(&ended,splash_operation)==1&&
              ended.ids==std::vector<std::uint32_t>{103,104,105,106,107,108,109,110},
              "splash teardown"))return 1;
    SplashScenario disarmed{};disarmed.armed=false;
    if(!check(a2fo_rs_stage8_b_loading_splash_end(&disarmed,splash_operation)==1&&
              disarmed.ids==std::vector<std::uint32_t>{103,110},
              "disarmed teardown forwards"))return 1;
    PresentScenario ordinary{};
    if(!check(a2fo_rs_stage8_b_loading_present(&ordinary,present_operation,.4f)==1&&
              ordinary.ids.back()==164&&
              std::count(ordinary.ids.begin(),ordinary.ids.end(),119u)==2,
              "ordinary frame read order"))return 1;
    PresentScenario layered{};layered.layered=true;
    if(!check(a2fo_rs_stage8_b_loading_present(&layered,present_operation,.4f)==1&&
              layered.ids[layered.ids.size()-2]==161&&layered.ids.back()==162,
              "layered frame"))return 1;
    PresentScenario oversized{};oversized.width=4000;
    if(!check(a2fo_rs_stage8_b_loading_present(&oversized,present_operation,.4f)==0&&
              oversized.ids.back()==115,"dimension admission"))return 1;
    PresentScenario invalid{};
    if(!check(a2fo_rs_stage8_b_loading_present(&invalid,present_operation,
              __builtin_nanf(""))==0&&invalid.ids.empty(),"nonfinite admission"))return 1;
    RuntimeScenario idle{};
    if(!check(a2fo_rs_stage8_b_loading_extra_frame(&idle,runtime_operation)==1&&
              idle.ids[idle.ids.size()-3]==189&&idle.ids.back()==192,
              "extra frame present and restore"))return 1;
    RuntimeScenario rejected{};rejected.fail=182;
    if(!check(a2fo_rs_stage8_b_loading_extra_frame(&rejected,runtime_operation)==0&&
              rejected.ids.back()==182,"nested scene rejected"))return 1;
    RuntimeScenario players{};
    if(!check(a2fo_rs_stage8_b_loading_players(&players,runtime_operation)==1&&
              std::find(players.ids.begin(),players.ids.end(),231u)!=players.ids.end()&&
              std::count(players.ids.begin(),players.ids.end(),209u)==1&&
              players.ids.back()==230,"one player and team normalization"))return 1;
    RuntimeScenario unavailable{};unavailable.fail=201;
    if(!check(a2fo_rs_stage8_b_loading_players(&unavailable,runtime_operation)==0&&
              unavailable.ids==std::vector<std::uint32_t>{200,201},
              "player binding absent"))return 1;
    BindingsScenario direct_bindings{};
    if(!check(a2fo_rs_stage8_b_loading_player_bindings(&direct_bindings,bindings_operation)==1&&
              direct_bindings.ids==std::vector<std::uint32_t>{232,243,244,245,246,247,248},
              "direct slot signatures"))return 1;
    BindingsScenario detour{};detour.direct=false;
    if(!check(a2fo_rs_stage8_b_loading_player_bindings(&detour,bindings_operation)==1&&
              std::find(detour.ids.begin(),detour.ids.end(),242u)!=detour.ids.end()&&
              detour.ids.back()==248,"known race detour"))return 1;
    BindingsScenario core{};core.direct=false;core.known=false;core.core=true;
    if(!check(a2fo_rs_stage8_b_loading_player_bindings(&core,bindings_operation)==1&&
              std::find(core.ids.begin(),core.ids.end(),241u)!=core.ids.end()&&
              core.ids.back()==248,"exact core race detour"))return 1;
    StatusScenario status{};
    if(!check(a2fo_rs_stage8_b_loading_status(&status,status_operation)==1&&
              status.ids==std::vector<std::uint32_t>{249,250,251,252,253,254,255,256,257},
              "status callback order"))return 1;
    StatusScenario inactive{};inactive.enabled=false;inactive.frame=false;
    if(!check(a2fo_rs_stage8_b_loading_status(&inactive,status_operation)==1&&
              inactive.ids==std::vector<std::uint32_t>{249,251,252,257},
              "inactive status still forwards"))return 1;
    std::puts("loading policy: 29 ordered branches PASS");
}
