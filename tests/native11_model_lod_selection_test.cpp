#include "../renderer/native11/model_lod_selection.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){
    if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}
}
void selected(const ModelLodSelectionInput& input,std::uint32_t expected,const char* why){
    std::uint32_t index=12345;std::string error="old error";
    need(select_model_lod(input,index,error)&&index==expected&&error.empty(),why);
}
void rejected(const ModelLodSelectionInput& input,const char* why){
    std::uint32_t index=12345;std::string error;
    need(!select_model_lod(input,index,error)&&index==12345&&!error.empty(),why);
}
ModelLodSelectionInput automatic(float distance){
    ModelLodSelectionInput input;
    input.translation[0]=distance;
    input.thresholds={30,20,10,8};
    input.level_count=5;
    return input;
}
}
int main(){
    ModelLodSelectionInput input;
    selected(input,UINT32_MAX,"empty control is a native no-op");
    input.level_count=3;input.force_highest=true;input.requested=-8;
    selected(input,2,"force highest selects list tail before override");
    input.force_highest=false;input.requested=-2;
    selected(input,0,"explicit negative other than minus one clamps to first");
    input.requested=0;selected(input,0,"explicit zero selects first");
    input.requested=2;selected(input,2,"explicit ordinal retained");
    input.requested=3;selected(input,2,"explicit ordinal clamps to list tail");
    input.requested=std::numeric_limits<std::int32_t>::max();
    selected(input,2,"large explicit ordinal clamps to list tail");

    selected(automatic(7),4,"below first threshold selects ordinal four");
    selected(automatic(std::nextafter(8.0f,0.0f)),4,
             "immediately below first threshold selects ordinal four");
    selected(automatic(8),3,"first threshold equality descends to ordinal three");
    selected(automatic(std::nextafter(8.0f,9.0f)),3,
             "immediately above first threshold selects ordinal three");
    selected(automatic(10),2,"second threshold equality descends to ordinal two");
    selected(automatic(20),1,"third threshold equality descends to ordinal one");
    selected(automatic(30),0,"fourth threshold equality falls through to zero");
    selected(automatic(100),0,"past all thresholds falls through to zero");
    input=automatic(7);input.level_count=2;
    selected(input,1,"automatic ordinal clamps to available list tail");
    input=automatic(7);input.quality_cap=-5;
    selected(input,0,"signed negative quality cap clamps to first");
    input.quality_cap=0;selected(input,0,"zero quality cap clamps to first");
    input.quality_cap=2;selected(input,2,"quality cap limits automatic ordinal");
    input.quality_cap=100;selected(input,4,"high quality cap retains automatic ordinal");
    input=automatic(10);input.thresholds={5,25,10,8};
    selected(input,2,"nonmonotonic threshold order is used without sorting");

    input=automatic(7);input.radius=std::numeric_limits<float>::quiet_NaN();
    rejected(input,"NaN radius rejected atomically");
    input.radius=-1;rejected(input,"negative radius rejected atomically");
    input.radius=std::numeric_limits<float>::infinity();
    rejected(input,"infinite radius rejected atomically");
    input=automatic(7);input.translation[1]=std::numeric_limits<float>::infinity();
    rejected(input,"nonfinite translation rejected atomically");
    input=automatic(7);input.centre[2]=std::numeric_limits<float>::quiet_NaN();
    rejected(input,"nonfinite centre rejected atomically");
    input=automatic(7);input.thresholds[0]=std::numeric_limits<float>::infinity();
    rejected(input,"nonfinite threshold rejected atomically");
    input=automatic(7);input.level_count=4097;
    rejected(input,"level count limit rejected atomically");
    std::puts("PASS native11 model LOD selection policy");
}
