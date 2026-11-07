#include "../renderer/native11/material_stream.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool value,const char* label){if(!value){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
struct Source {std::shared_ptr<const ModelAsset> model;MaterialFrameStream* stream=nullptr;
    Frame* frame=nullptr;std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>>* observations=nullptr;
    bool reenter=false;bool refused=false;};
std::shared_ptr<const ModelAsset> resolve(void* context,std::uint32_t db,std::uint64_t serial){
    auto& source=*static_cast<Source*>(context);
    if(source.reenter){std::string error;source.refused=!source.stream->clear() &&
        !source.stream->update(*source.frame,99,resolve,context,*source.observations,error) && !error.empty();}
    return source.model && source.model->info.database==db && source.model->info.serial==serial?source.model:nullptr;
}
std::shared_ptr<const ModelAsset> model(std::uint8_t byte){
    auto value=std::make_shared<ModelAsset>();value->info.database=7;value->info.serial=11;
    std::strcpy(value->info.name,"hull.sod");value->bytes={byte};value->info.byte_count=1;return value;
}
std::shared_ptr<const TextureAsset> texture(std::uint8_t byte){
    auto value=std::make_shared<TextureAsset>();value->serial=31;value->texture=201;
    value->logical_name="hull.tga";
    value->bytes=std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{byte});return value;
}
using Observations=std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>>;
void observe(Observations& observations,std::uint32_t instance,
    const std::shared_ptr<const ModelAsset>& asset,const std::shared_ptr<const TextureAsset>& pixels,
    float offset=0){
    auto value=std::make_shared<MaterialObservation>();value->instance=instance;
    value->database=7;value->model_serial=11;value->model=asset;
    MaterialBinding part;part.authored_texture="hull.tga";part.texture=pixels;
    part.node_state_valid=true;part.node_state.local_to_model[3]=offset;
    value->parts.push_back(part);observations[instance]=value;
}
void object(Object& value,std::uint32_t instance){
    value.instance=instance;value.entity=instance+1;value.database=7;value.model_serial=11;
    value.model_identity=ModelIdentity::plain_database_name;
    value.placement_identity=PlacementIdentity::authored_placement;
    std::strcpy(value.model_name,"hull.sod");
}
struct Sink {MaterialFrameStream* stream;Frame* frame;Source* source;Observations* observations;
    std::vector<std::uint8_t> bytes;bool refused=false;};
std::int32_t bytes_sink(void* context,const std::uint8_t* data,std::uint32_t size){
    auto& value=*static_cast<Sink*>(context);value.bytes.assign(data,data+size);
    std::string error;value.refused=!value.stream->clear() &&
        !value.stream->update(*value.frame,99,resolve,value.source,*value.observations,error) && !error.empty();
    return 1;
}
std::int32_t part_sink(void* context,const A2FO_N11_PartState* state){
    auto& value=*static_cast<Sink*>(context);value.bytes={static_cast<std::uint8_t>(state->visible)};
    std::string error;value.refused=!value.stream->clear() &&
        !value.stream->update(*value.frame,99,resolve,value.source,*value.observations,error) && !error.empty();
    return 1;
}
bool read(MaterialFrameStream& stream,const std::string& id,std::uint32_t kind,Sink& sink){
    return MaterialFrameStream::read_asset(&stream,kind,
        reinterpret_cast<const std::uint8_t*>(id.data()),id.size(),64,bytes_sink,&sink)==1;
}
}
int main(){
    auto frame=std::make_unique<Frame>();frame->object_count=2;
    object(frame->objects[0],100);object(frame->objects[1],101);
    auto first_model=model(1);auto first_texture=texture(9);
    Observations observations;observe(observations,100,first_model,first_texture);
    observe(observations,101,first_model,first_texture);
    MaterialFrameStream stream;Source source{first_model,&stream,frame.get(),&observations};
    std::string error;
    need(stream.update(*frame,10,resolve,&source,observations,error),"initial update");
    auto first=stream.current();need(first && first->source_epoch()==10,"initial epoch");
    const std::string view=first->frame().objects[0].model_name;
    Sink sink{&stream,frame.get(),&source,&observations,{},{}};
    need(read(stream,view,1,sink)&&sink.bytes==std::vector<std::uint8_t>{1}&&sink.refused,
        "model callback retains and refuses reentry");
    sink.refused=false;
    need(MaterialFrameStream::resolve_texture(&stream,
        reinterpret_cast<const std::uint8_t*>(view.data()),view.size(),0,
        reinterpret_cast<const std::uint8_t*>("hull.tga"),8,64,bytes_sink,&sink)==1 && sink.refused,
        "texture callback refuses reentry");
    const std::string texture_id(sink.bytes.begin(),sink.bytes.end());
    need(read(stream,texture_id,2,sink)&&sink.bytes==std::vector<std::uint8_t>{9},"texture bytes");
    sink.refused=false;
    need(MaterialFrameStream::resolve_part_state(&stream,
        reinterpret_cast<const std::uint8_t*>(view.data()),view.size(),0,nullptr,part_sink,&sink)==1 && sink.refused,
        "part callback refuses reentry");
    frame->camera=33;frame->generation=2;frame->objects[0].world[3]=12;
    observe(observations,100,first_model,first_texture,4);
    std::swap(frame->objects[0],frame->objects[1]);
    need(stream.update(*frame,11,resolve,&source,observations,error) &&
         stream.current()->source_epoch()==10 && stream.current()->frame().objects[0].instance==101,
         "camera pose and order reuse epoch");
    need(!stream.update(*frame,11,resolve,&source,observations,error) && !error.empty(),"repeated fresh epoch refused");
    auto old=stream.current();
    source.reenter=true;source.refused=false;
    need(stream.update(*frame,12,resolve,&source,observations,error)&&source.refused,"resolver reentry refused");
    source.reenter=false;
    auto replacement_texture=texture(8);
    observe(observations,100,first_model,replacement_texture);
    observe(observations,101,first_model,replacement_texture);
    need(stream.update(*frame,13,resolve,&source,observations,error)&&stream.current()->source_epoch()==13,
         "same serial new texture owner advances epoch");
    auto replacement_model=model(2);
    source.model=replacement_model;observe(observations,100,replacement_model,replacement_texture);
    observe(observations,101,replacement_model,replacement_texture);
    need(stream.update(*frame,14,resolve,&source,observations,error)&&stream.current()->source_epoch()==14,
         "same serial new model owner advances epoch");
    need(stream.current()->frame().objects[1].model_name!=view,"new view id");
    auto replacement=stream.current();
    observations.erase(100);
    need(!stream.update(*frame,15,resolve,&source,observations,error)&&stream.current()==replacement&&!error.empty(),
        "failed update retains current");
    observe(observations,100,replacement_model,replacement_texture);
    need(stream.update(*frame,15,resolve,&source,observations,error)&&stream.current()->source_epoch()==14,
        "failed fresh epoch may retry");
    old.reset();first.reset();first_model.reset();first_texture.reset();
    std::weak_ptr<const ModelAsset> weak_model=replacement_model;
    std::weak_ptr<const TextureAsset> weak_texture=replacement_texture;
    replacement.reset();replacement_model.reset();replacement_texture.reset();source.model.reset();observations.clear();
    need(!weak_model.expired()&&!weak_texture.expired(),"current owns frame resources");
    need(stream.clear()&&!stream.current()&&weak_model.expired()&&weak_texture.expired(),"clear drops owners");
    need(!stream.update(*frame,15,resolve,&source,observations,error),"clear retains monotonic guard");
    auto recovered_model=model(3);
    auto recovered_texture=texture(7);
    source.model=recovered_model;observe(observations,100,recovered_model,recovered_texture);
    observe(observations,101,recovered_model,recovered_texture);
    need(stream.update(*frame,16,resolve,&source,observations,error)&&stream.current()->source_epoch()==16,
        "clear recovery uses fresh epoch");
    std::puts("native11 material stream test passed");
}
