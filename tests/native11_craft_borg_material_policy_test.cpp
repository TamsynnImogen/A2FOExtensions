#include "../renderer/native11/craft_borg_material_policy.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
bool same(const CraftMaterialSlotWrite& row,std::uint32_t node,std::uint32_t slot,
    std::uint32_t texture){return row.node_index==node&&row.slot==slot&&row.texture==texture;}
void rejects(const std::vector<CraftBorgMaterialRow>& rows,const char* why){
    std::vector<CraftMaterialSlotWrite> out{{77,7,0xdeadbeef}};
    std::string error="old";
    need(!craft_borg_material_writes(rows,0,out,error)&&!error.empty()&&error!="old"&&
         out.size()==1&&same(out[0],77,7,0xdeadbeef),why);
}
}
int main(){
    const std::vector<CraftBorgMaterialRow> rows{{9,0x11,0x21,4},{2,0,0x22,8},{7,0x13,0x23,5}};
    std::vector<CraftMaterialSlotWrite> out{{77,7,0xdeadbeef}};
    std::string error="old";
    need(craft_borg_material_writes(rows,0,out,error)&&error.empty()&&out.size()==6&&
         same(out[0],9,3,0x11)&&same(out[1],9,0,0x11)&&
         same(out[2],2,3,0)&&same(out[3],2,0,0)&&
         same(out[4],7,3,0x13)&&same(out[5],7,0,0x13),
         "base emits auxiliary slot before diffuse for each row in source order, including null original");
    for(const auto& write:out)need(write.slot==3||write.slot==0,
                                   "base does not claim writes to preserved slots");
    for(const auto flag:std::array<std::uint8_t,4>{{1,2,128,255}}){
        need(craft_borg_material_writes(rows,flag,out,error)&&error.empty()&&out.size()==3&&
             same(out[0],9,0,0x21)&&same(out[1],2,0,0x22)&&same(out[2],7,0,0x23),
             "every nonzero Borg byte writes only diffuse slot in row order");
    }
    need(craft_borg_material_writes({},0,out,error)&&out.empty()&&error.empty(),
         "empty base input is an explicit empty write list");
    out.push_back({77,7,0xdeadbeef});
    need(craft_borg_material_writes({},255,out,error)&&out.empty()&&error.empty(),
         "empty Borg input replaces a prior result with an explicit empty write list");
    rejects({{1,1,2,4},{1,3,4,8}},"duplicate node index rejected atomically");
    rejects({{4096,1,2,4}},"out-of-range node index rejected atomically");
    rejects({{1,1,2,3}},"slot vector below native domain rejected atomically");
    rejects({{1,1,2,9}},"slot vector above native domain rejected atomically");
    rejects({{1,1,0,4}},"zero alternate rejected even on base path");
    rejects({{0,1,2,4},{4095,3,4,8},{4096,5,6,4}},
            "late invalid row preserves prior output");
    std::vector<CraftBorgMaterialRow> maximum;
    maximum.reserve(4096);
    for(std::uint32_t k=0;k<4096;++k)maximum.push_back({k,k,1,4});
    need(craft_borg_material_writes(maximum,0,out,error)&&out.size()==8192&&
         same(out.front(),0,3,0)&&same(out.back(),4095,0,4095),
         "4096 distinct rows at the bounded maximum preserve exact ordering");
    maximum.push_back({0,1,2,4});
    rejects(maximum,"more than 4096 rows rejected before producing writes");
    std::puts("PASS native11 Craft Borg material write policy");
}
