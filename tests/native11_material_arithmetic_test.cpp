#include "../renderer/native11/material_snapshot.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Source {std::shared_ptr<const ModelAsset> model;};
std::shared_ptr<const ModelAsset> resolve(void* raw,std::uint32_t db,std::uint64_t serial){
    const auto& source=*static_cast<Source*>(raw);
    return source.model&&source.model->info.database==db&&source.model->info.serial==serial?
        source.model:nullptr;
}
std::shared_ptr<const ModelAsset> model(){
    auto value=std::make_shared<ModelAsset>();value->info.database=7;value->info.serial=11;
    std::strcpy(value->info.name,"scope.sod");value->bytes={1,2,3};value->info.byte_count=3;
    return value;
}
std::shared_ptr<Frame> frame(std::size_t count){
    auto value=std::make_shared<Frame>();value->object_count=count;
    for(std::size_t i=0;i<count;++i){auto& object=value->objects[i];
        object.entity=200+i;object.instance=100+i;object.database=7;object.model_serial=11;
        object.model_identity=ModelIdentity::plain_database_name;
        object.placement_identity=PlacementIdentity::authored_placement;
        std::strcpy(object.model_name,"scope.sod");}
    return value;
}
std::shared_ptr<MaterialObservation> observed(std::uint32_t instance,
    const std::shared_ptr<const ModelAsset>& asset,std::uint32_t encoding){
    auto value=std::make_shared<MaterialObservation>();value->instance=instance;
    value->database=7;value->model_serial=11;value->model=asset;
    value->native_x87_control=encoding;
    MaterialBinding binding{};binding.node_state_valid=true;
    value->parts.push_back(binding);return value;
}
using Observations=std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>>;
std::shared_ptr<const BoundMaterialCapture> build(const Frame& input,std::uint64_t epoch,
    Source& source,const Observations& values){
    std::shared_ptr<const BoundMaterialCapture> out;std::string error;
    need(build_bound_material_capture(input,epoch,resolve,&source,values,out,error)&&error.empty(),
         "build material capture");return out;
}
void cases(){
    static_assert(valid_native_x87_encoding(0),"legacy unknown");
    static_assert(valid_native_x87_encoding(encode_native_x87(0x0300)),"valid 64-bit precision");
    static_assert(!valid_native_x87_encoding(0x0100),"missing marker");
    static_assert(!valid_native_x87_encoding(encode_native_x87(0x0100)),"reserved precision");
    static_assert(!valid_native_x87_encoding(0x20000),"upper bits");
    Source source{model()};auto input=frame(2);
    const auto selected=encode_native_x87(0x037f);
    auto first=observed(100,source.model,selected);
    auto second=std::make_shared<MaterialObservation>(*first);second->instance=101;
    need(second->native_x87_control==selected,"shared observation copy retains source encoding");
    Observations uniform{{100,first},{101,second}};
    auto same=build(*input,1,source,uniform);
    need(same->native_x87_control(100)==selected&&same->native_x87_control(101)==selected&&
         same->native_x87_control(999)==0&&same->common_native_x87_control()==selected,
         "uniform and absent instance queries");
    Object raw{};std::shared_ptr<const MaterialObservation> exported;std::string error;
    need(same->export_instance(101,raw,exported,error)&&error.empty()&&
         raw.instance==101&&exported->native_x87_control==selected&&
         exported->model==source.model,"export carries pinned source encoding and model");
    auto retained_model=std::weak_ptr<const ModelAsset>(source.model);
    uniform.clear();first.reset();second.reset();source.model.reset();
    need(!retained_model.expired()&&exported->model==retained_model.lock()&&
         same->native_x87_control(101)==selected,
         "bound capture and export outlive source observations");
    source.model=retained_model.lock();
    auto changed=observed(100,source.model,encode_native_x87(0x0b7f));
    auto unchanged=observed(101,source.model,selected);
    Observations mixed{{100,changed},{101,unchanged}};
    auto different=build(*input,2,source,mixed);
    need(different->common_native_x87_control()==0&&
         different->native_x87_control(100)==changed->native_x87_control&&
         same->same_resources(*different)&&!same->same_instance_contents(100,*different)&&
         same->same_instance_contents(101,*different),
         "CW changes contents but retains GPU resource identity");
    changed->native_x87_control=0;
    auto unknown=build(*input,3,source,mixed);
    need(unknown->native_x87_control(100)==0&&unknown->common_native_x87_control()==0&&
         unknown->same_resources(*same)&&!unknown->same_instance_contents(100,*same),
         "unknown legacy source remains distinct contents");
    changed->native_x87_control=selected;
    auto restored=build(*input,4,source,mixed);
    need(restored->common_native_x87_control()==selected&&
         restored->same_instance_contents(100,*same),"matching source reuses instance contents");
    auto empty=frame(0);Observations none;
    need(build(*empty,5,source,none)->common_native_x87_control()==0,
         "empty capture has no common source");
    auto prior=restored;
    auto reject=[&](std::uint32_t encoding,const char* why){
        changed->native_x87_control=encoding;
        need(!build_bound_material_capture(*input,6,resolve,&source,mixed,restored,error)&&
             restored==prior&&!error.empty(),why);
    };
    reject(0x0100,"missing marker rejected atomically");
    reject(encode_native_x87(0x0100),"reserved precision rejected atomically");
    reject(0x20000,"unknown upper bits rejected atomically");
}
}
int main(){cases();std::puts("PASS native11 material arithmetic");}
