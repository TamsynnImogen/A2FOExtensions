#include "../renderer/native11/craft_visibility.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
std::uint32_t bits(float value){std::uint32_t raw=0;std::memcpy(&raw,&value,4);return raw;}
unsigned short control(){unsigned short cw=0;asm volatile("fnstcw %0":"=m"(cw));return cw;}
void set_control(unsigned short cw){asm volatile("fldcw %0"::"m"(cw):"memory");}
float add_step(float value){constexpr float step=.2f;float out=0;
    asm volatile("flds %1; fadds %2; fstps %0":"=m"(out):"m"(value),"m"(step):"st","memory");
    return out;
}
CraftVisibilityInput tree(){
    CraftVisibilityInput input;input.root=0;input.hull_fraction=.7f;
    input.nodes.resize(4);
    input.nodes[0].first_child=1;
    input.nodes[1].parent=0;input.nodes[1].next_sibling=2;input.nodes[1].first_child=3;
    input.nodes[2].parent=0;
    input.nodes[3].parent=1;
    return input;
}
void reject(CraftVisibilityInput input,const char* why){
    CraftVisibilityResult out;out.flags={47};out.visible={true};out.damage_threshold=9;
    std::string error;
    need(!evaluate_craft_visibility(input,out,error),why);
    need(out.flags==std::vector<std::uint32_t>{47}&&out.visible.size()==1&&out.visible[0]&&
         out.damage_threshold==9&&!error.empty(),"failure preserves result and gives reason");
}
void cases(){
    auto input=tree();CraftVisibilityResult out;std::string error="stale";
    input.damage_child=1;
    need(evaluate_craft_visibility(input,out,error)&&error.empty(),"primary damage traversal");
    need(out.flags.size()==4&&!(out.flags[1]&1u)&&!(out.flags[2]&1u)&&
         (out.flags[3]&1u)&&out.visible[1]&&out.visible[2]&&!out.visible[3],
         "damage visits whole sibling subtree before first child");
    need(out.low_bit_writes==std::vector<std::uint8_t>({0,1,1,1}),
         "damage records own-hidden writes even when flags stay unchanged");
    float threshold=input.hull_fraction;
    for(unsigned i=0;i<3;++i)threshold=add_step(threshold);
    need(bits(out.damage_threshold)==bits(threshold),"native x87 threshold carries through all visits");

    input=tree();input.nodes[0].flags=2;input.nodes[1].flags=2;
    need(evaluate_craft_visibility(input,out,error)&&out.visible[0]&&!out.visible[1]&&
         !out.visible[2]&&!out.visible[3],"own prune bit retains root but ancestor prunes children");
    input=tree();input.nodes[1].flags=2;
    need(evaluate_craft_visibility(input,out,error)&&out.visible[1]&&out.visible[2]&&
         !out.visible[3],"own bit 2 prunes descendants but not self or sibling");
    input=tree();input.nodes[1].flags=1;
    need(evaluate_craft_visibility(input,out,error)&&!out.visible[1]&&out.visible[3],
         "own bit 1 hides node but not descendant");

    input=tree();input.nodes[1].flags=0x80;input.database_damage=1;
    input.system_groups={1,2,2,visibility_none,visibility_none};
    input.system_hidden={true,true,false,false,false};
    input.assimilation_group=1;input.assimilated=false;input.base_group=1;
    need(evaluate_craft_visibility(input,out,error)&&out.flags[1]==0x80&&
         !(out.flags[2]&2u),"native role order and system alias last write wins");
    need(out.low_bit_writes==std::vector<std::uint8_t>({0,3,2,0}),
         "aliased class roles retain write intent across overwritten values");
    input.instance_hidden=true;
    need(evaluate_craft_visibility(input,out,error)&&out.flags[1]==0x83,
         "base group final hidden write overrides assimilation");
    input.instance_hidden=false;input.context_enabled=false;
    need(evaluate_craft_visibility(input,out,error)&&out.flags[1]==0x83,
         "disabled context hides base group");
    input=tree();input.primary_database=false;input.database_damage=1;
    input.damage_child=1;input.system_groups[0]=2;input.system_hidden[0]=true;
    input.assimilation_group=3;input.assimilated=false;
    need(evaluate_craft_visibility(input,out,error)&&out.flags[1]==3&&out.flags[2]==0&&
         out.flags[3]==0&&bits(out.damage_threshold)==bits(input.hull_fraction),
         "alternate database skips health, system and assimilation mutations");
    need(out.low_bit_writes==std::vector<std::uint8_t>({0,3,0,0}),
         "alternate database records only executed writes");
    input.base_group=1;
    need(evaluate_craft_visibility(input,out,error)&&out.flags[1]==0,
         "alternate database still applies final base group priority");
    input=tree();input.system_groups[0]=1;input.system_hidden[0]=false;
    need(evaluate_craft_visibility(input,out,error)&&out.flags[1]==input.nodes[1].flags&&
         out.low_bit_writes[1]==2,"unchanged system flag store remains a write");

    input=tree();input.root=visibility_none;reject(input,"missing root with nodes");
    input=tree();input.nodes[0].next_sibling=1;reject(input,"root sibling refused");
    input=tree();input.nodes[3].first_child=1;reject(input,"cycle refused");
    input=tree();input.nodes[1].next_sibling=3;reject(input,"sibling parent mismatch refused");
    input=tree();input.nodes[1].next_sibling=visibility_none;
    reject(input,"disconnected sibling refused");
    input=tree();input.database_damage=99;reject(input,"bad role ID refused");
    input=tree();input.system_groups[4]=99;reject(input,"bad system group ID refused");
    input=tree();input.hull_fraction=std::numeric_limits<float>::quiet_NaN();
    reject(input,"nonfinite hull fraction refused");
    input=tree();input.hull_fraction=-.1f;reject(input,"negative hull fraction refused");
    input={};input.root=0;input.nodes.resize(257);
    for(unsigned i=0;i<257;++i){input.nodes[i].parent=i?i-1:visibility_none;
        input.nodes[i].first_child=i<256?i+1:visibility_none;}
    reject(input,"independent hierarchy depth above 256 refused");
    input.nodes.resize(256);input.nodes[255].first_child=visibility_none;
    need(evaluate_craft_visibility(input,out,error)&&out.visible.size()==256,
         "hierarchy depth 256 accepted");
    input={};need(evaluate_craft_visibility(input,out,error)&&out.flags.empty()&&
        out.visible.empty()&&out.damage_threshold==1,"empty owned tree with no roles accepted");
}
}
int main(){
    const auto original=control();
    for(unsigned mode=0;mode<4;++mode){
        const auto selected=static_cast<unsigned short>((original&~0x0c00u)|(mode<<10));
        set_control(selected);cases();need(control()==selected,"caller x87 control word preserved");
    }
    set_control(original);
    std::puts("PASS native11 craft visibility: ordered flags, damage traversal and topology rejection");
}
