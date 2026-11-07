#include "../renderer/native11/asset_snapshot.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool ok, const char* label) { if (!ok) { std::fprintf(stderr,"FAIL %s\n",label); std::exit(1); } }
struct Source { std::vector<std::shared_ptr<const ModelAsset>> assets; unsigned calls=0; };
std::shared_ptr<const ModelAsset> resolve(void* p, std::uint32_t db, std::uint64_t serial) {
    auto& source=*static_cast<Source*>(p); ++source.calls;
    for (const auto& asset:source.assets) if (asset->info.database==db && asset->info.serial==serial) return asset;
    return {};
}
std::shared_ptr<const ModelAsset> asset(std::uint32_t db, std::uint64_t serial,
    const char* name, std::vector<std::uint8_t> bytes) {
    auto out=std::make_shared<ModelAsset>(); out->info.database=db; out->info.serial=serial;
    out->info.byte_count=static_cast<std::uint32_t>(bytes.size());
    out->info.parser_scalar_bits=0x12345678u; std::strcpy(out->info.name,name);
    out->bytes=std::move(bytes); return out;
}
void object(Frame& frame, std::size_t index, std::uint32_t db, std::uint64_t serial, const char* name) {
    auto& out=frame.objects[index]; out.database=db; out.model_serial=serial;
    out.model_identity=ModelIdentity::plain_database_name;
    out.placement_identity=PlacementIdentity::authored_placement;
    std::strcpy(out.model_name,name);
}
struct Sink { std::vector<std::uint8_t> bytes; unsigned calls=0; bool accept=true; };
std::int32_t sink(void* context,const std::uint8_t* data,std::uint32_t length) {
    auto& out=*static_cast<Sink*>(context);++out.calls;out.bytes.assign(data,data+length);
    return out.accept ? 1 : 0;
}
bool read(const BoundCapture& capture, std::uint32_t kind,const std::string& id, Sink& output,
    std::uint32_t max=100) {
    return BoundCapture::read_asset(const_cast<BoundCapture*>(&capture),kind,
        reinterpret_cast<const std::uint8_t*>(id.data()),static_cast<std::uint32_t>(id.size()),
        max,sink,&output)==1;
}
}
int main() {
    Frame frame{}; frame.object_count=3;
    object(frame,0,1,7,"same.sod"); object(frame,1,2,8,"same.sod"); object(frame,2,1,7,"same.sod");
    Source source{{asset(1,7,"same.sod",{1,2}),asset(2,8,"same.sod",{3,4,5})}};
    auto texture=std::make_shared<const std::vector<std::uint8_t>>(std::vector<std::uint8_t>{9,8});
    std::vector<SnapshotTexture> textures{{"diffuse.dds",texture}};
    std::shared_ptr<const BoundCapture> bound; std::string error;
    need(build_bound_capture(frame,11,resolve,&source,textures,bound,error),"build");
    need(source.calls==2 && bound->models().size()==2 && bound->source_epoch()==11,"dedup and epoch");
    need(std::strcmp(frame.objects[0].model_name,"same.sod")==0,"original frame unchanged");
    need(std::strcmp(bound->frame().objects[0].model_name,"n11:model:7")==0 &&
         std::strcmp(bound->frame().objects[1].model_name,"n11:model:8")==0,"opaque ids");
    need(bound->model_asset("n11:model:7")->info.parser_scalar_bits==0x12345678u &&
         std::strcmp(bound->model_asset("n11:model:7")->info.name,"same.sod")==0,"original metadata");
    source.assets.clear(); texture.reset(); textures.clear();
    Sink out; need(read(*bound,1,"n11:model:7",out) && out.bytes==std::vector<std::uint8_t>({1,2}),"retained first");
    need(read(*bound,1,"n11:model:8",out) && out.bytes==std::vector<std::uint8_t>({3,4,5}),"retained second");
    need(read(*bound,2,"diffuse.dds",out) && out.bytes==std::vector<std::uint8_t>({9,8}),"retained texture");
    auto old=bound;
    auto rejected=[&](const char* label) {
        need(!build_bound_capture(frame,12,resolve,&source,{},bound,error) && bound==old && !error.empty(),label);
    };
    rejected("missing asset atomic");
    source.assets={asset(1,7,"same.sod",{1}),asset(2,8,"same.sod",{2})};
    frame.objects[0].model_serial=99; rejected("serial mismatch"); frame.objects[0].model_serial=7;
    std::strcpy(frame.objects[0].model_name,"wrong.sod"); rejected("name mismatch");
    std::strcpy(frame.objects[0].model_name,"same.sod");
    frame.object_count=capacity+1; rejected("frame bound"); frame.object_count=3;
    frame.objects[1].model_serial=7; rejected("serial collision"); frame.objects[1].model_serial=8;
    frame.objects[0].placement_identity=PlacementIdentity::unresolved; rejected("placement");
    frame.objects[0].placement_identity=PlacementIdentity::authored_placement;
    auto bad=std::make_shared<ModelAsset>(*source.assets[0]); bad->info.byte_count=99;
    source.assets[0]=bad; rejected("byte count"); source.assets[0]=asset(1,7,"same.sod",{1});
    std::memset(frame.objects[0].model_name,'x',sizeof(frame.objects[0].model_name));
    rejected("unterminated capture name"); std::strcpy(frame.objects[0].model_name,"same.sod");
    source.assets[0]=asset(1,7,"same.sod",std::vector<std::uint8_t>(64ull*1024*1024+1,1));
    rejected("model byte cap");
    source.assets[0]=asset(1,7,"same.sod",std::vector<std::uint8_t>(64ull*1024*1024,1));
    frame.object_count=2; object(frame,1,1,7,"same.sod");
    std::shared_ptr<const BoundCapture> large;
    source.calls=0;
    need(build_bound_capture(frame,12,resolve,&source,{},large,error) && source.calls==1 &&
         large->models().size()==1,"duplicate model charged once");
    frame.object_count=3; object(frame,1,2,8,"same.sod");
    source.assets[0]=asset(1,7,"same.sod",{1});
    need(!build_bound_capture(frame,0,resolve,&source,{},bound,error) && bound==old,"zero epoch");
    auto check_texture=[&](std::vector<SnapshotTexture> t,const char* label) {
        need(!build_bound_capture(frame,12,resolve,&source,t,bound,error) && bound==old,label);
    };
    check_texture({{"missing",{}}},"missing texture");
    check_texture({{"bad\n",std::make_shared<const std::vector<std::uint8_t>>(1,1)}},"bad texture id");
    auto one=std::make_shared<const std::vector<std::uint8_t>>(1,1);
    check_texture({{"dup",one},{"dup",one}},"duplicate texture id");
    check_texture({{"big",std::make_shared<const std::vector<std::uint8_t>>(128ull*1024*1024,1)}},"texture budget");
    std::vector<SnapshotTexture> many(257,{"x",one}); check_texture(many,"texture count");
    Sink refused; refused.accept=false;
    need(!read(*old,1,"n11:model:7",refused) && refused.calls==1,"sink refusal once");
    Sink absent; need(!read(*old,1,"n11:model:99",absent) && absent.calls==0,"unknown model");
    need(!read(*old,2,"absent",absent) && absent.calls==0,"unknown texture");
    need(!read(*old,3,"n11:model:7",absent) && absent.calls==0,"unknown kind");
    need(!read(*old,1,"n11:model:7",absent,1) && absent.calls==0,"max bytes");
    need(BoundCapture::read_asset(const_cast<BoundCapture*>(old.get()),1,nullptr,1,100,sink,&absent)==0 &&
         BoundCapture::read_asset(const_cast<BoundCapture*>(old.get()),1,reinterpret_cast<const std::uint8_t*>("x"),1,100,nullptr,&absent)==0,
         "callback arguments");
    std::puts("native11 asset snapshot test passed");
}
