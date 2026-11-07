// Real DLL client for retained material frames and stable stream callbacks.
#include "../renderer/native11/material_stream.hpp"
#include "../renderer/native11/capture_service.hpp"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <tlhelp32.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
extern "C" int32_t native11_c_query4(A2FO_N11_GetInterface,A2FO_N11_API4*);
namespace {
void need(bool v,const char* why){if(!v){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void ok(int32_t hr,const char* why){if(hr<0){std::fprintf(stderr,"FAIL %s hr=%08lx\n",why,static_cast<unsigned long>(hr));std::exit(1);}}
std::vector<uint8_t> tga(uint8_t b,uint8_t g,uint8_t r){std::vector<uint8_t> v(22);v[2]=2;v[12]=1;v[14]=1;v[16]=24;v[17]=0x20;v[18]=b;v[19]=g;v[20]=r;return v;}
std::shared_ptr<const ModelAsset> model(uint64_t serial){auto p=std::make_shared<ModelAsset>();p->info.database=7;p->info.serial=serial;
    std::strcpy(p->info.name,"hull.sod");p->bytes=native11_fixture::model("hull.tga");p->info.byte_count=p->bytes.size();return p;}
std::shared_ptr<const TextureAsset> texture(uint64_t serial,uint32_t address,uint8_t b,uint8_t g,uint8_t r){
    auto p=std::make_shared<TextureAsset>();p->serial=serial;p->texture=address;p->logical_name="hull.tga";
    p->bytes=std::make_shared<const std::vector<uint8_t>>(tga(b,g,r));return p;}
struct Source {std::shared_ptr<const ModelAsset> owner;};
std::shared_ptr<const ModelAsset> resolve(void* raw,uint32_t db,uint64_t serial){auto p=static_cast<Source*>(raw)->owner;
    return p&&p->info.database==db&&p->info.serial==serial?p:nullptr;}
using Observations=std::map<uint32_t,std::shared_ptr<const MaterialObservation>>;
std::shared_ptr<const MaterialObservation> observe(uint32_t instance,const std::shared_ptr<const ModelAsset>& m,
    const std::shared_ptr<const TextureAsset>& t,bool visible=true,float x=0){
    auto p=std::make_shared<MaterialObservation>();p->instance=instance;p->database=7;p->model_serial=m->info.serial;p->model=m;
    MaterialBinding part;part.authored_texture="hull.tga";part.texture=t;part.node_state_valid=true;
    part.node_state.visible=visible;part.node_state.local_to_model[3]=x;p->parts.push_back(part);return p;}
struct Host {MaterialFrameStream stream;unsigned reads=0,texture_calls=0,state_calls=0;};
int32_t A2FO_N11_CALL read_asset(void* raw,uint32_t kind,const uint8_t* id,uint32_t n,uint32_t max,
    A2FO_N11_BytesSink sink,void* context){auto& h=*static_cast<Host*>(raw);++h.reads;
    return MaterialFrameStream::read_asset(&h.stream,kind,id,n,max,sink,context);}
int32_t A2FO_N11_CALL resolve_texture(void* raw,const uint8_t* id,uint32_t n,uint32_t part,
    const uint8_t* authored,uint32_t authored_n,uint32_t max,A2FO_N11_BytesSink sink,void* context){
    auto& h=*static_cast<Host*>(raw);++h.texture_calls;
    return MaterialFrameStream::resolve_texture(&h.stream,id,n,part,authored,authored_n,max,sink,context);}
int32_t A2FO_N11_CALL resolve_state(void* raw,const uint8_t* id,uint32_t n,uint32_t part,
    const float authored[16],A2FO_N11_PartStateSink sink,void* context){auto& h=*static_cast<Host*>(raw);++h.state_calls;
    return MaterialFrameStream::resolve_part_state(&h.stream,id,n,part,authored,sink,context);}
std::unique_ptr<Frame> frame(uint64_t serial){auto f=std::make_unique<Frame>();f->camera=1;f->generation=1;f->object_count=2;
    for(unsigned i=0;i<4;++i)f->view.world_to_clip[i*5]=1;
    f->view.viewport[2]=f->view.viewport[3]=128;
    for(unsigned i=0;i<2;++i){auto& o=f->objects[i];o.entity=i+1;o.instance=100+i;o.database=7;o.model_serial=serial;
        o.model_identity=ModelIdentity::plain_database_name;o.placement_identity=PlacementIdentity::authored_placement;
        std::strcpy(o.model_name,"hull.sod");for(unsigned j=0;j<4;++j)o.world[j*5]=1;
        o.world[0]=o.world[5]=o.world[10]=.5f;o.world[3]=i?.5f:-.5f;}
    return f;}
void update(MaterialFrameStream& s,const Frame& f,uint64_t fresh,Source& source,const Observations& observations){
    std::string error;need(s.update(f,fresh,resolve,&source,observations,error),error.empty()?"stream update":error.c_str());}
std::vector<uint8_t> pixels(const A2FO_N11_API4& api,A2FO_N11_Session session){A2FO_N11_Readback rb{};rb.struct_size=sizeof(rb);
    auto& base=api.base.base.base;need(base.readback(session,&rb)==S_FALSE&&rb.width==128&&rb.height==128,"readback metadata");
    std::vector<uint8_t> b(static_cast<size_t>(rb.required_bytes));rb.bgra=b.data();rb.capacity=b.size();ok(base.readback(session,&rb),"readback pixels");return b;}
void pixel(const std::vector<uint8_t>& b,unsigned x,unsigned y,int r,int g,int blue,const char* why){auto* q=b.data()+(y*128+x)*4;
    need(std::abs(int(q[2])-r)<5&&std::abs(int(q[1])-g)<5&&std::abs(int(q[0])-blue)<5,why);}
void background(const std::vector<uint8_t>& b,unsigned x,unsigned y,const char* why){auto* q=b.data()+(y*128+x)*4;
    need(q[2]==1&&q[1]==2&&q[0]==5,why);}
A2FO_N11_Usage usage(const A2FO_N11_API4& api,A2FO_N11_Session session){A2FO_N11_Usage u{};u.struct_size=sizeof(u);
    ok(api.base.base.base.usage(session,&u),"usage");return u;}
bool same(const A2FO_N11_Usage& a,const A2FO_N11_Usage& b){return a.device_epoch==b.device_epoch&&a.mesh_bytes==b.mesh_bytes&&
    a.texture_bytes==b.texture_bytes&&a.meshes==b.meshes&&a.textures==b.textures;}
void submit(const A2FO_N11_API4& api,A2FO_N11_Session session,const MaterialFrameStream& stream){auto c=stream.current();
    need(c&&c->frame().requires_part_states,"current complete pose");const float ambient[3]{1,1,1};
    ok(submit_captured(api,session,c->frame(),c->source_epoch(),nullptr,0,ambient),"captured GPU submit");}
void no_legacy(){auto h=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE|TH32CS_SNAPMODULE32,GetCurrentProcessId());need(h!=INVALID_HANDLE_VALUE,"module snapshot");
    MODULEENTRY32W e{};e.dwSize=sizeof(e);need(Module32FirstW(h,&e)!=0,"module first");do{
        need(_wcsicmp(e.szModule,L"d3d8.dll")&&_wcsicmp(e.szModule,L"d3d9.dll")&&_wcsnicmp(e.szModule,L"d3dx",4),"no legacy D3D modules");
    }while(Module32NextW(h,&e));const auto last=GetLastError();CloseHandle(h);need(last==ERROR_NO_MORE_FILES,"full module enumeration");}
void run(const char* path){auto dll=LoadLibraryA(path);need(dll,"DLL load");auto symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol,"interface export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));A2FO_N11_API4 api{};ok(native11_c_query4(get,&api),"C API4 query");
    need(api.base.base.base.version==4&&api.base.base.base.struct_size==sizeof(api)&&api.configure_part_states,"API4 contract");
    Host host;A2FO_N11_Host config{sizeof(config),0,128,128,&host,read_asset,nullptr};A2FO_N11_Session session=0;
    ok(api.base.base.base.create(&config,&session),"offscreen session");
    A2FO_N11_TextureBindings textures{sizeof(textures),0,&host,resolve_texture};ok(api.base.configure_texture_bindings(session,&textures),"stable texture callback");
    A2FO_N11_PartStates states{sizeof(states),0,&host,resolve_state};ok(api.configure_part_states(session,&states),"stable pose callback");
    Source source{model(11)};auto red=texture(31,201,0,0,255),green=texture(32,202,0,255,0);
    Observations observed{{100,observe(100,source.owner,red)},{101,observe(101,source.owner,green)}};
    auto f=frame(11);update(host.stream,*f,1,source,observed);auto first=host.stream.current();
    need(first->source_epoch()==1,"initial epoch");const std::string left=first->frame().objects[0].model_name,right=first->frame().objects[1].model_name;
    need(left!=right,"per instance views");first.reset();submit(api,session,host.stream);
    auto image=pixels(api,session);pixel(image,32,64,255,0,0,"left red");pixel(image,96,64,0,255,0,"right green");
    const auto resident=usage(api,session);need(resident.meshes==2&&resident.textures==3,"two views, selected colours and white");
    const auto hot_reads=host.reads;const auto initial_states=host.state_calls;
    // Same owners, new generation, changed world placement, pose, and visibility.
    ++f->generation;f->objects[1].world[3]=.25f;
    observed[100]=observe(100,source.owner,red,true,.75f);observed[101]=observe(101,source.owner,green,false,0);
    update(host.stream,*f,2,source,observed);auto second=host.stream.current();
    need(second->source_epoch()==1&&std::string(second->frame().objects[0].model_name)==left&&
         std::string(second->frame().objects[1].model_name)==right,"pose update keeps epoch and IDs");second.reset();
    submit(api,session,host.stream);need(host.reads==hot_reads&&host.state_calls==initial_states+2,"hot assets and live part callbacks");
    image=pixels(api,session);background(image,32,64,"old red footprint clears");pixel(image,56,64,255,0,0,"red pose moves");
    background(image,96,64,"hidden green clears");need(same(resident,usage(api,session)),"pose keeps GPU resources");
    // Reordering must retain identity by instance, not array ordinal.
    std::swap(f->objects[0],f->objects[1]);++f->generation;
    observed[101]=observe(101,source.owner,green,true,0);update(host.stream,*f,3,source,observed);
    auto reordered=host.stream.current();need(reordered->source_epoch()==1&&
        std::string(reordered->frame().objects[0].model_name)==right&&std::string(reordered->frame().objects[1].model_name)==left,
        "reorder retains instance view IDs");reordered.reset();
    submit(api,session,host.stream);need(host.reads==hot_reads,"reorder stays hot");image=pixels(api,session);
    pixel(image,56,64,255,0,0,"red remains with instance");pixel(image,80,64,0,255,0,"green follows reordered instance");
    need(same(resident,usage(api,session)),"reorder keeps GPU resources");
    // A new immutable texture owner is a source change even if the model is shared.
    auto blue=texture(33,203,255,0,0);observed[100]=observe(100,source.owner,blue,true,.75f);
    update(host.stream,*f,4,source,observed);need(host.stream.current()->source_epoch()==4,"texture owner advances epoch");
    submit(api,session,host.stream);image=pixels(api,session);pixel(image,56,64,0,0,255,"replacement texture blue");
    pixel(image,80,64,0,255,0,"other texture remains green");need(host.reads>hot_reads,"changed texture reloads assets");
    // A replacement model owner with new serial forces another epoch.
    auto replacement=model(12);source.owner=replacement;f->objects[0].model_serial=f->objects[1].model_serial=12;
    observed[100]=observe(100,replacement,blue,true,.75f);observed[101]=observe(101,replacement,green,true,0);
    update(host.stream,*f,5,source,observed);need(host.stream.current()->source_epoch()==5,"model owner advances epoch");
    submit(api,session,host.stream);image=pixels(api,session);pixel(image,56,64,0,0,255,"replacement model blue placement");
    pixel(image,80,64,0,255,0,"replacement model green placement");
    auto preserved=host.stream.current();Observations missing=observed;missing.erase(101);std::string error;
    need(!host.stream.update(*f,6,resolve,&source,missing,error)&&!error.empty()&&
         host.stream.current()==preserved&&host.stream.current()->source_epoch()==5,"invalid batch preserves current and epoch");
    preserved.reset();submit(api,session,host.stream);need(pixels(api,session)==image,"invalid batch preserves rendered frame");
    std::weak_ptr<const ModelAsset> weak_model=replacement;std::weak_ptr<const TextureAsset> weak_blue=blue;
    observed.clear();missing.clear();source.owner.reset();replacement.reset();blue.reset();green.reset();red.reset();
    need(!weak_model.expired()&&!weak_blue.expired(),"stream retains latest owners");need(host.stream.clear(),"stream clear");
    need(weak_model.expired()&&weak_blue.expired(),"stream releases owners after producer drops refs");
    // Failed fresh epoch 6 did not commit; clear retains the last accepted 5.
    source.owner=model(13);red=texture(34,204,0,0,255);green=texture(35,205,0,255,0);f=frame(13);
    observed={{100,observe(100,source.owner,red)},{101,observe(101,source.owner,green)}};
    update(host.stream,*f,6,source,observed);need(host.stream.current()->source_epoch()==6,"post clear epoch monotonic");
    submit(api,session,host.stream);image=pixels(api,session);pixel(image,32,64,255,0,0,"post clear red");pixel(image,96,64,0,255,0,"post clear green");
    need(host.stream.clear(),"final stream clear");ok(api.base.base.base.destroy(session),"session destroy");no_legacy();need(FreeLibrary(dll)!=0,"DLL unload");}
}
int main(int argc,char** argv){need(argc==2,"DLL path argument");run(argv[1]);std::puts("PASS native11 material stream GPU DLL client");}
