#include "../rust/stage8_a1_lifecycle_ffi.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <vector>
struct Trace {std::vector<std::uint32_t> ids;std::uint32_t fail=UINT32_MAX;
    std::uint32_t error=UINT32_MAX;bool safe{};bool resource=true;
    bool ship=true;bool mutated_resource{};};
static std::uint64_t op(void* opaque,std::uint32_t id,std::uint64_t value){
    auto& t=*static_cast<Trace*>(opaque);t.ids.push_back(id);
    if(id==t.error)return UINT64_MAX;
    if(id==t.fail)return 0;
    if(id==68)return t.safe;
    if(id==94||id==95||id==96||id==97||id==98||id==99||id==100||id==101)
        return t.resource;
    if(id==104)return value?0:t.resource;
    if(id==145)return t.ship;
    if(id==147)return t.mutated_resource?0:t.resource;
    if(id==148)return value;
    return 1;
}
static bool check(bool okay,const char* label){
    if(!okay)std::fprintf(stderr,"A1 lifecycle policy failure: %s\n",label);
    return okay;
}
int main(){
    Trace invalid{};invalid.fail=1;
    if(!check(a2fo_rs_stage8_a1_lifecycle_init(&invalid,op)==0&&
        invalid.ids==std::vector<std::uint32_t>{0,1},
        "size admission before member reads"))return 1;
    Trace marker{};marker.fail=34;
    if(!check(a2fo_rs_stage8_a1_lifecycle_init(&marker,op)==0&&
        marker.ids[marker.ids.size()-3]==34&&marker.ids.back()==36,
        "marker failure logs then clears API"))return 1;
    Trace registration{};registration.fail=45;
    if(!check(a2fo_rs_stage8_a1_lifecycle_init(&registration,op)==0&&
        registration.ids[registration.ids.size()-3]==45&&
        registration.ids[registration.ids.size()-2]==46&&registration.ids.back()==36,
        "fatal registration order and clear"))return 1;
    Trace thrown{};thrown.error=45;
    if(!check(a2fo_rs_stage8_a1_lifecycle_init(&thrown,op)==0&&
        thrown.ids.back()==45,
        "native exception stops without failure log or API clear"))return 1;
    Trace essential{};essential.fail=70;
    if(!check(a2fo_rs_stage8_a1_lifecycle_init(&essential,op)==0&&
        essential.ids[essential.ids.size()-6]==71&&essential.ids.back()==76,
        "essential GUI failure cleanup order"))return 1;
    Trace safe{};safe.safe=true;
    if(!check(a2fo_rs_stage8_a1_lifecycle_init(&safe,op)==1&&
        std::find(safe.ids.begin(),safe.ids.end(),119u)==safe.ids.end()&&
        std::find(safe.ids.begin(),safe.ids.end(),144u)!=safe.ids.end()&&
        safe.ids[safe.ids.size()-4]==145&&safe.ids.back()==150,
        "safe mode skips producer but still installs ship last"))return 1;
    Trace resource{};resource.resource=false;
    if(!check(a2fo_rs_stage8_a1_lifecycle_init(&resource,op)==1&&
        std::find(resource.ids.begin(),resource.ids.end(),96u)==resource.ids.end()&&
        std::find(resource.ids.begin(),resource.ids.end(),149u)!=resource.ids.end(),
        "control/resource failure precedes signature checks"))return 1;
    Trace late{};late.mutated_resource=true;
    if(!check(a2fo_rs_stage8_a1_lifecycle_init(&late,op)==1&&
        late.ids.back()==150&&
        std::find(late.ids.begin(),late.ids.end(),149u)!=late.ids.end(),
        "late resource reread controls final status"))return 1;
    std::puts("A1 lifecycle policy: 8 branches PASS");
}
