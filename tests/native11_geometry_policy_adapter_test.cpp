#include "../renderer/native11/material_snapshot.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>

using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){
    if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}
}
std::uint32_t bits(float value){
    std::uint32_t raw=0;std::memcpy(&raw,&value,sizeof(raw));return raw;
}
struct Source {std::shared_ptr<const ModelAsset> asset;};
std::shared_ptr<const ModelAsset> resolve(void* context,std::uint32_t database,
    std::uint64_t serial){
    const auto& source=*static_cast<Source*>(context);
    return source.asset&&source.asset->info.database==database&&
        source.asset->info.serial==serial?source.asset:nullptr;
}
std::shared_ptr<const ModelAsset> asset(std::uint64_t serial,float scalar,
    std::uint32_t load_control){
    auto value=std::make_shared<ModelAsset>();
    value->info.database=7;value->info.serial=serial;
    value->info.parser_scalar_bits=bits(scalar);
    value->info.parser_native_x87_control=load_control;
    std::strcpy(value->info.name,"geometry.sod");
    value->bytes={1,2,3};value->info.byte_count=3;
    return value;
}
std::shared_ptr<Frame> frame(std::uint64_t serial){
    auto value=std::make_shared<Frame>();value->object_count=1;
    auto& object=value->objects[0];object.entity=40;object.instance=50;
    object.database=7;object.model_serial=serial;
    object.model_identity=ModelIdentity::plain_database_name;
    object.placement_identity=PlacementIdentity::authored_placement;
    std::strcpy(object.model_name,"geometry.sod");
    return value;
}
std::shared_ptr<const BoundMaterialCapture> capture(Source& source,std::uint64_t serial,
    std::uint64_t epoch,std::uint32_t draw_control){
    auto input=frame(serial);
    auto observation=std::make_shared<MaterialObservation>();
    observation->instance=50;observation->database=7;
    observation->model_serial=serial;observation->model=source.asset;
    observation->native_x87_control=draw_control;
    MaterialBinding part{};part.node_state_valid=true;
    observation->parts.push_back(part);
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> values{{50,observation}};
    std::shared_ptr<const BoundMaterialCapture> result;std::string error;
    need(build_bound_material_capture(*input,epoch,resolve,&source,values,result,error)&&
         error.empty(),"build pinned material snapshot");
    return result;
}
struct Sink {
    unsigned calls=0;
    A2FO_N11_GeometryPolicy policy{};
    std::int32_t result=1;
};
std::int32_t A2FO_N11_CALL accept(void* context,const A2FO_N11_GeometryPolicy* policy){
    auto& sink=*static_cast<Sink*>(context);
    ++sink.calls;sink.policy=*policy;return sink.result;
}
std::int32_t query(const BoundMaterialCapture& bound,Sink& sink){
    const auto* id=reinterpret_cast<const std::uint8_t*>(bound.frame().objects[0].model_name);
    return BoundMaterialCapture::resolve_geometry_policy(
        const_cast<BoundMaterialCapture*>(&bound),id,
        static_cast<std::uint32_t>(std::strlen(reinterpret_cast<const char*>(id))),
        accept,&sink);
}
}
int main(){
    constexpr auto load=encode_native_x87(0x037f);
    constexpr auto draw=encode_native_x87(0x0b7f);
    Source source{asset(11,1.5f,load)};
    auto first=capture(source,11,1,draw);
    Sink sink;
    need(query(*first,sink)==1&&sink.calls==1&&
         sink.policy.struct_size==sizeof(A2FO_N11_GeometryPolicy)&&
         sink.policy.reserved==0&&sink.policy.parser_scalar_bits==bits(1.5f)&&
         sink.policy.native_x87_control==load&&
         first->native_x87_control(50)==draw,
         "resolver emits exactly one source LOAD policy, independent of draw control");
    std::weak_ptr<const ModelAsset> old_weak=source.asset;source.asset.reset();
    sink={};need(!old_weak.expired()&&query(*first,sink)==1&&sink.calls==1&&
        sink.policy.native_x87_control==load,
        "bound snapshot retains original immutable model source");
    auto retained=old_weak.lock();
    source.asset=asset(12,2.0f,encode_native_x87(0x077f));
    auto replacement=capture(source,12,2,draw);
    sink={};need(query(*replacement,sink)==1&&sink.policy.parser_scalar_bits==bits(2.0f)&&
        sink.policy.native_x87_control==encode_native_x87(0x077f),
        "new source serial uses its own captured load policy");
    Object old_object{};std::shared_ptr<const MaterialObservation> old_observation;
    std::string old_error;
    need(first->export_instance(50,old_object,old_observation,old_error)&&old_error.empty(),
         "old bound source remains exportable");
    sink={};need(query(*first,sink)==1&&sink.policy.parser_scalar_bits==bits(1.5f)&&
        old_observation->model==retained,
        "old source version remains pinned after replacement");

    auto reject_policy=[&](float scalar,std::uint32_t control,const char* why){
        Source bad_source{asset(20,scalar,control)};
        auto bad=capture(bad_source,20,3,draw);Sink called;
        need(query(*bad,called)==0&&called.calls==0,why);
    };
    reject_policy(1.0f,0,"unknown load arithmetic rejected without sink");
    reject_policy(1.0f,0x037f,"unmarked load control rejected without sink");
    reject_policy(1.0f,encode_native_x87(0x0100),
                  "invalid precision encoding rejected without sink");
    reject_policy(0.0f,load,"zero loader scalar rejected without sink");
    reject_policy(-1.0f,load,"negative loader scalar rejected without sink");
    reject_policy(NAN,load,"nonfinite loader scalar rejected without sink");
    reject_policy(INFINITY,load,"infinite loader scalar rejected without sink");

    Source missing{};auto missing_frame=frame(31);
    std::shared_ptr<const BoundMaterialCapture> unchanged=first;
    std::string missing_error;
    need(!build_bound_material_capture(*missing_frame,4,resolve,&missing,{},
         unchanged,missing_error)&&unchanged==first&&!missing_error.empty(),
         "missing model source cannot create a geometry policy view");

    const auto* id=reinterpret_cast<const std::uint8_t*>(first->frame().objects[0].model_name);
    const auto length=static_cast<std::uint32_t>(std::strlen(reinterpret_cast<const char*>(id)));
    void* callback_context=const_cast<BoundMaterialCapture*>(first.get());
    sink={};
    need(BoundMaterialCapture::resolve_geometry_policy(callback_context,id,0,accept,&sink)==0&&
         sink.calls==0,"empty opaque model ID rejected");
    need(BoundMaterialCapture::resolve_geometry_policy(callback_context,id,260,accept,&sink)==0&&
         sink.calls==0,"oversize opaque model ID rejected before read");
    need(BoundMaterialCapture::resolve_geometry_policy(callback_context,
         reinterpret_cast<const std::uint8_t*>("foreign"),7,accept,&sink)==0&&sink.calls==0,
         "foreign opaque model ID rejected without sink");
    need(BoundMaterialCapture::resolve_geometry_policy(nullptr,id,length,accept,&sink)==0&&
         BoundMaterialCapture::resolve_geometry_policy(callback_context,nullptr,length,accept,&sink)==0&&
         BoundMaterialCapture::resolve_geometry_policy(callback_context,id,length,nullptr,&sink)==0&&
         sink.calls==0,"missing adapter inputs rejected without sink");
    sink.result=0;need(query(*first,sink)==0&&sink.calls==1,
        "sink refusal propagates after exactly one policy record");
    std::puts("PASS native11 geometry policy adapter");
}
