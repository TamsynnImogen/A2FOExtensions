// Real DLL C ABI fixture for retained, opaque-ID static-preview assets.
#include "../renderer/native11/asset_snapshot.hpp"
#include "../renderer/native11/capture_service.hpp"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* label) { if(!yes){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);} }
void ok(std::int32_t hr,const char* label) { if(hr<0){std::fprintf(stderr,"FAIL %s hr=%08lx\n",label,static_cast<unsigned long>(hr));std::exit(1);} }
std::vector<std::uint8_t> model(const char* texture) {
    std::vector<std::uint8_t> b;
    auto word=[&](std::uint16_t v){b.push_back(v&255);b.push_back(v>>8);};
    auto dword=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)b.push_back((v>>(i*8))&255);};
    auto number=[&](float v){std::uint32_t bits;std::memcpy(&bits,&v,4);dword(bits);};
    auto text=[&](const char* p){word(static_cast<std::uint16_t>(std::strlen(p)));while(*p)b.push_back(*p++);};
    const char magic[]="Storm3D_SW";b.insert(b.end(),magic,magic+10);number(1.93f);
    word(1);text("mat");for(float v:{0.f,0.f,0.f,1.f,1.f,1.f,0.f,0.f,0.f,1.f})number(v);
    b.push_back(2);b.push_back(0);word(1);word(1);text("mesh");text("");
    for(float v:{1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f})number(v);
    text("default");dword(4);dword(1);text(texture);dword(0);text("");word(0);
    word(3);word(3);word(1);
    for(float v:{-.75f,-.75f,.2f,.75f,-.75f,.2f,0.f,.75f,.2f})number(v);
    for(unsigned j=0;j<6;++j)number(0);
    word(1);text("mat");for(unsigned j=0;j<3;++j){word(j);word(j);}
    b.push_back(0);word(0);word(0);word(0);return b;
}
std::shared_ptr<const std::vector<std::uint8_t>> tga(bool red) {
    std::vector<std::uint8_t> b(22);b[2]=2;b[12]=1;b[14]=1;b[16]=32;b[17]=0x28;
    b[red?20:19]=255;b[21]=255;
    return std::make_shared<const std::vector<std::uint8_t>>(std::move(b));
}
OpenedFile file(const char* name,std::vector<std::uint8_t> bytes) {
    OpenedFile out;out.name=name;out.bytes=std::move(bytes);return out;
}
std::shared_ptr<const ModelAsset> resolve(void* raw,std::uint32_t db,std::uint64_t serial) {
    return static_cast<ModelAssetStore*>(raw)->find(db,serial);
}
struct Host { const BoundCapture* active=nullptr;unsigned reads=0;bool block=false; };
std::int32_t A2FO_N11_CALL read(void* raw,std::uint32_t kind,const std::uint8_t* id,
    std::uint32_t length,std::uint32_t max_bytes,A2FO_N11_BytesSink sink,void* sink_context) {
    auto& host=*static_cast<Host*>(raw);need(host.active && !host.block,"callback bound snapshot");
    ++host.reads;
    return BoundCapture::read_asset(const_cast<BoundCapture*>(host.active),kind,id,length,max_bytes,sink,sink_context);
}
std::unique_ptr<Frame> frame(std::uint64_t left,std::uint64_t right) {
    auto out=std::make_unique<Frame>();out->camera=1;out->generation=1;out->object_count=2;
    for(unsigned i=0;i<4;++i)out->view.world_to_clip[i*5]=1;
    out->view.viewport[2]=out->view.viewport[3]=128;
    for(unsigned i=0;i<2;++i){auto& o=out->objects[i];o.database=i+1;o.model_serial=i?right:left;
        o.model_identity=ModelIdentity::plain_database_name;o.placement_identity=PlacementIdentity::authored_placement;
        std::strcpy(o.model_name,"same.sod");for(unsigned j=0;j<4;++j)o.world[j*5]=1;
        o.world[3]=i?.5f:-.5f;
    }
    return out;
}
std::vector<std::uint8_t> pixels(const A2FO_N11_API& api,A2FO_N11_Session session) {
    A2FO_N11_Readback out{};out.struct_size=sizeof(out);
    need(api.readback(session,&out)==S_FALSE&&out.width==128&&out.height==128&&out.required_bytes==128*128*4,"readback metadata");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(out.required_bytes));out.bgra=bytes.data();out.capacity=bytes.size();
    ok(api.readback(session,&out),"readback pixels");return bytes;
}
void colours(const std::vector<std::uint8_t>& image,bool left_red) {
    const auto* left=image.data()+(64*128+32)*4;
    const auto* right=image.data()+(64*128+96)*4;
    const auto red=[](const std::uint8_t* p){return p[2]>250&&p[1]<4&&p[0]<4;};
    const auto green=[](const std::uint8_t* p){return p[1]>250&&p[2]<4&&p[0]<4;};
    need(left_red?(red(left)&&green(right)):(green(left)&&red(right)),"distinct left and right model pixels");
}
}
int main(int argc,char** argv) {
    need(argc==2,"DLL path argument");
    HMODULE dll=LoadLibraryA(argv[1]);need(dll!=nullptr,"DLL load");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol!=nullptr,"versioned export");
    A2FO_N11_GetInterface get=nullptr;static_assert(sizeof(get)==sizeof(symbol));std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API2 api2{};ok(get(2,sizeof(api2),&api2),"version 2 API");
    need(api2.base.version==2&&api2.base.struct_size==sizeof(api2)&&api2.base.submit,"version 2 contract");
    ModelAssetStore store;
    need(store.publish(1,0x12345678,file("same.sod",model("red.tga")))&&
         store.publish(2,0x87654321,file("same.sod",model("green.tga"))),"two same-name model publishes");
    ModelAssetInfo left{},right{};need(store.info(1,left)&&store.info(2,right)&&left.serial!=right.serial,"distinct serials");
    auto source=frame(left.serial,right.serial);std::shared_ptr<const BoundCapture> bound;std::string error;
    std::vector<SnapshotTexture> textures{{"red.tga",tga(true)},{"green.tga",tga(false)}};
    need(build_bound_capture(*source,1,resolve,&store,textures,bound,error),"first bind");
    need(bound->models().size()==2&&bound->model_asset("n11:model:"+std::to_string(left.serial))&&
         bound->model_asset("n11:model:"+std::to_string(right.serial)),"distinct opaque model IDs");
    need(std::strcmp(source->objects[0].model_name,"same.sod")==0&&
         std::strcmp(bound->frame().objects[0].model_name,("n11:model:"+std::to_string(left.serial)).c_str())==0&&
         std::strcmp(bound->frame().objects[1].model_name,("n11:model:"+std::to_string(right.serial)).c_str())==0,
         "source names retained and exact serial-only IDs remapped");
    store.clear();source.reset();textures.clear();
    Host host{bound.get()};A2FO_N11_Host config{sizeof(config),0,128,128,&host,read,nullptr};
    A2FO_N11_Session session=0;ok(api2.base.create(&config,&session),"offscreen session");
    const float ambient[3]{1,1,1};
    ok(submit_captured(api2,session,bound->frame(),bound->source_epoch(),nullptr,0,ambient),"retained snapshot submit");
    colours(pixels(api2.base,session),true);
    A2FO_N11_Usage usage{};usage.struct_size=sizeof(usage);ok(api2.base.usage(session,&usage),"resource usage");
    need(usage.meshes==2&&usage.textures==3,"two meshes two textures plus white");
    const auto reads=host.reads;host.block=true;
    ok(submit_captured(api2,session,bound->frame(),bound->source_epoch(),nullptr,0,ambient),"hot cache resubmit");
    need(host.reads==reads,"hot cache no reads");host.block=false;
    need(store.publish(1,0x11111111,file("same.sod",model("green.tga")))&&
         store.publish(2,0x22222222,file("same.sod",model("red.tga"))),"replacement models");
    std::shared_ptr<const BoundCapture> replacement=bound;
    auto stale=frame(left.serial,right.serial);
    need(!build_bound_capture(*stale,2,resolve,&store,{},replacement,error)&&replacement==bound,"stale bind atomic");
    ModelAssetInfo new_left{},new_right{};need(store.info(1,new_left)&&store.info(2,new_right),"replacement serials");
    auto fresh=frame(new_left.serial,new_right.serial);
    std::vector<SnapshotTexture> changed{{"red.tga",tga(true)},{"green.tga",tga(false)}};
    need(build_bound_capture(*fresh,2,resolve,&store,changed,replacement,error),"replacement bind");
    store.clear();fresh.reset();changed.clear();host.active=replacement.get();
    ok(submit_captured(api2,session,replacement->frame(),replacement->source_epoch(),nullptr,0,ambient),"new epoch submit");
    colours(pixels(api2.base,session),false);
    Host probe{bound.get()};std::vector<std::uint8_t> original;
    auto collect=[](void* raw,const std::uint8_t* bytes,std::uint32_t size)->std::int32_t {
        auto& target=*static_cast<std::vector<std::uint8_t>*>(raw);target.assign(bytes,bytes+size);return 1;
    };
    const auto original_id=std::string(bound->frame().objects[0].model_name);
    need(BoundCapture::read_asset(const_cast<BoundCapture*>(probe.active),1,
         reinterpret_cast<const std::uint8_t*>(original_id.data()),static_cast<std::uint32_t>(original_id.size()),
         64u*1024*1024,collect,&original)==1&&original==model("red.tga"),"old retained snapshot remains original");
    ok(api2.base.destroy(session),"session destroy");need(FreeLibrary(dll)!=0,"DLL unload");
    std::puts("PASS native11 bound snapshot DLL C ABI");
}
