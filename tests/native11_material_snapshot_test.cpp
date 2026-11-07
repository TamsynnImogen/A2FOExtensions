#include "../renderer/native11/material_snapshot.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool good, const char* label) { if (!good) { std::fprintf(stderr, "FAIL %s\n",label); std::exit(1); } }
struct Store { std::shared_ptr<const ModelAsset> model; };
std::shared_ptr<const ModelAsset> resolve(void* context,std::uint32_t db,std::uint64_t serial) {
    auto model=static_cast<Store*>(context)->model;
    return model && model->info.database==db && model->info.serial==serial ? model : nullptr;
}
std::shared_ptr<const ModelAsset> model() {
    auto p=std::make_shared<ModelAsset>(); p->info.database=7;p->info.serial=11;
    std::strcpy(p->info.name,"hull.sod");p->bytes={1,2,3};p->info.byte_count=3;return p;
}
std::shared_ptr<const TextureAsset> texture(std::uint64_t serial,std::uint32_t ptr,std::uint8_t value) {
    auto p=std::make_shared<TextureAsset>();p->serial=serial;p->texture=ptr;
    p->logical_name="hull.tga";p->bytes=std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{value});return p;
}
struct Sink { std::vector<std::uint8_t> bytes; unsigned calls=0; };
std::int32_t sink(void* context,const std::uint8_t* bytes,std::uint32_t n) {
    auto& out=*static_cast<Sink*>(context);++out.calls;out.bytes.assign(bytes,bytes+n);return 1;
}
bool read(const BoundMaterialCapture& c,std::uint32_t kind,const std::string& id,Sink& out,std::uint32_t limit=64) {
    return BoundMaterialCapture::read_asset(const_cast<BoundMaterialCapture*>(&c),kind,
        reinterpret_cast<const std::uint8_t*>(id.data()),id.size(),limit,sink,&out)==1;
}
bool bind(const BoundMaterialCapture& c,const std::string& view,std::uint32_t part,
    const std::string& authored,Sink& out,std::uint32_t limit=1024) {
    return BoundMaterialCapture::resolve_texture(const_cast<BoundMaterialCapture*>(&c),
        reinterpret_cast<const std::uint8_t*>(view.data()),view.size(),part,
        reinterpret_cast<const std::uint8_t*>(authored.data()),authored.size(),limit,sink,&out)==1;
}
}
int main() {
    auto frame=std::make_unique<Frame>();frame->object_count=2;
    for(std::size_t i=0;i<2;++i){auto& o=frame->objects[i];o.instance=100+i;o.database=7;o.model_serial=11;
        o.model_identity=ModelIdentity::plain_database_name;o.placement_identity=PlacementIdentity::authored_placement;
        std::strcpy(o.model_name,"hull.sod");}
    Store store{model()};
    auto red=texture(31,201,9),green=texture(32,202,8);
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observed;
    for(unsigned i=0;i<2;++i){auto o=std::make_shared<MaterialObservation>();o->instance=100+i;
        o->database=7;o->model_serial=11;o->model=store.model;
        o->parts.push_back({"hull.tga",i?green:red,0});o->parts.push_back({"",nullptr,0});
        observed.emplace(o->instance,o);}
    std::shared_ptr<const BoundMaterialCapture> bound;std::string error;
    need(build_bound_material_capture(*frame,19,resolve,&store,observed,bound,error),"build");
    need(bound->source_epoch()==19 && bound->base()->models().size()==1,"retained base");
    const std::string left=bound->frame().objects[0].model_name,right=bound->frame().objects[1].model_name;
    need(left!=right && left.find("n11:view:")==0 && right.find("n11:view:")==0,"distinct opaque views");
    store.model.reset();observed.clear();red.reset();green.reset();
    Sink out;need(read(*bound,1,left,out)&&out.bytes==std::vector<std::uint8_t>({1,2,3}),"retained model");
    need(read(*bound,1,right,out)&&out.bytes==std::vector<std::uint8_t>({1,2,3}),"second view model");
    Sink l,r;need(bind(*bound,left,0,"hull.tga",l)&&bind(*bound,right,0,"hull.tga",r),"bindings");
    need(std::string(l.bytes.begin(),l.bytes.end())=="n11:texture:31" &&
         std::string(r.bytes.begin(),r.bytes.end())=="n11:texture:32","separate selections");
    Sink pixels;need(read(*bound,2,"n11:texture:31",pixels)&&pixels.bytes==std::vector<std::uint8_t>({9}),"left pixels");
    need(read(*bound,2,"n11:texture:32",pixels)&&pixels.bytes==std::vector<std::uint8_t>({8}),"right pixels");
    Sink absent;need(!bind(*bound,left,1,"hull.tga",absent)&&!bind(*bound,left,0,"other",absent)&&
        !bind(*bound,"n11:view:19:9",0,"hull.tga",absent)&&absent.calls==0,"exact resolver refusal");
    need(!read(*bound,1,"n11:model:11",absent)&&!read(*bound,2,"hull.tga",absent)&&absent.calls==0,"opaque reads");
    need(!bind(*bound,left,0,"hull.tga",absent,3)&&absent.calls==0,"id limit");
    auto old=bound;store.model=model();
    auto one=std::make_shared<MaterialObservation>();one->instance=100;one->database=7;one->model_serial=11;
    one->model=store.model;one->parts={{"hull.tga",texture(31,201,9),0},{"",nullptr,0}};
    auto two=std::make_shared<MaterialObservation>(*one);two->instance=101;
    observed={{100,one},{101,two}};
    auto rejected=[&](const char* label){need(!build_bound_material_capture(*frame,20,resolve,&store,observed,bound,error)&&bound==old&&!error.empty(),label);};
    observed.erase(101);rejected("missing observation");observed[101]=two;
    two->model=model();rejected("stale owner");two->model=store.model;
    two->parts[0].texture=texture(31,202,7);rejected("serial owner collision");two->parts[0].texture=one->parts[0].texture;
    two->parts[0].node_flags=1;rejected("hidden node");two->parts[0].node_flags=0;
    two->parts[0].node_flags=2;rejected("disabled node");two->parts[0].node_flags=0;
    frame->objects[1].instance=100;rejected("duplicate instance");frame->objects[1].instance=101;
    one->parts[0].texture.reset();rejected("missing texture");one->parts[0].texture=texture(31,201,9);
    one->parts[0].authored_texture="";rejected("white mismatch");one->parts[0].authored_texture="hull.tga";
    one->parts[0].texture=texture(31,201,9);two->parts[0].texture=one->parts[0].texture;
    one->parts.resize(8192,one->parts[0]);rejected("global part budget");one->parts.resize(2);
    one->parts[0].texture=texture(41,204,std::uint8_t(1));
    auto huge=std::make_shared<TextureAsset>(*one->parts[0].texture);
    huge->bytes=std::make_shared<const std::vector<std::uint8_t>>(64ull*1024*1024+1,1);
    one->parts[0].texture=huge;rejected("single texture budget");
    one->parts.clear();
    for(unsigned i=0;i<257;++i)
        one->parts.push_back({"hull.tga",texture(1000+i,3000+i,1),0});
    two->parts.clear();two->parts.push_back({"",nullptr,0});
    rejected("unique texture count budget");
    one->parts.clear();
    auto large_a=std::make_shared<TextureAsset>(*texture(5001,5001,1));
    auto large_b=std::make_shared<TextureAsset>(*texture(5002,5002,2));
    large_a->bytes=std::make_shared<const std::vector<std::uint8_t>>(64ull*1024*1024,1);
    large_b->bytes=std::make_shared<const std::vector<std::uint8_t>>(64ull*1024*1024,2);
    one->parts.push_back({"hull.tga",large_a,0});
    two->parts[0]={"hull.tga",large_b,0};
    rejected("combined byte budget");
    std::puts("native11 material snapshot test passed");
}
