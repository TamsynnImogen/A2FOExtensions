// Headless public ABI/GPU regression for pristine versus authored-all SOD scope.
#include "../sdk/include/a2fo_native11_model_scope_api.h"
#include <windows.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
extern "C" std::int32_t native11_c_query9(A2FO_N11_GetInterface,A2FO_N11_API9*);
extern "C" std::int32_t native11_c_submit_all(const A2FO_N11_API9*,A2FO_N11_Session,const A2FO_N11_Frame*);
static_assert(sizeof(A2FO_N11_API9)==80,"i686 API9 layout");
static_assert(offsetof(A2FO_N11_API9,submit_world_all_parts)==76,"API9 append offset");
namespace {
void need(bool pass,const char* what){if(!pass){std::fprintf(stderr,"FAIL %s\n",what);std::exit(1);}}
void ok(std::int32_t hr,const char* what){if(hr<0){std::fprintf(stderr,"FAIL %s hr=%08lx\n",what,static_cast<unsigned long>(hr));std::exit(1);}}
void identity(float out[16]){for(int i=0;i<16;++i)out[i]=(i%5)==0?1.f:0.f;}
std::vector<std::uint8_t> model(){
    std::vector<std::uint8_t> d;
    auto half=[&](std::uint16_t v){d.push_back(v&255);d.push_back(v>>8);};
    auto full=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)d.push_back((v>>(8*i))&255);};
    auto real=[&](float v){std::uint32_t bits;std::memcpy(&bits,&v,4);full(bits);};
    auto str=[&](const char* s){half(static_cast<std::uint16_t>(std::strlen(s)));while(*s)d.push_back(*s++);};
    const char magic[]="Storm3D_SW";d.insert(d.end(),magic,magic+10);real(1.93f);
    half(1);str("mat");for(float v:{0.f,0.f,0.f,1.f,1.f,1.f,0.f,0.f,0.f,1.f})real(v);
    d.push_back(2);d.push_back(0);
    // Same layout as selection_sod in rust/a2fo_native11_model: ties select lod1.
    const char* names[]={"root","geometry","lod0","lod1","low","high","damage","overlay"};
    const char* parents[]={"","root","geometry","geometry","lod0","lod1","root","damage"};
    const char* textures[]={"blue.tga","red.tga","green.tga"};
    half(8);
    for(unsigned node=0;node<8;++node){
        const bool mesh=node==4||node==5||node==7;
        half(mesh?1:0);str(names[node]);str(parents[node]);
        for(float v:{1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f})real(v);
        if(!mesh)continue;
        str("opaque");full(4);full(1);str(textures[node==4?0:node==5?1:2]);full(0);str("");half(0);
        half(3);half(3);half(1);
        const float centre=node==4?-.6f:node==5?0.f:.6f;
        for(float v:{centre-.28f,-.75f,.2f,centre+.28f,-.75f,.2f,centre,.75f,.2f})real(v);
        for(unsigned i=0;i<6;++i)real(0.f);
        half(1);str("mat");for(unsigned i=0;i<3;++i){half(i);half(i);}
        d.push_back(1);half(0);
    }
    half(0);half(0);return d;
}
std::vector<std::uint8_t> tga(std::uint8_t b,std::uint8_t g,std::uint8_t r){
    std::vector<std::uint8_t> d(22);d[2]=2;d[12]=d[14]=1;d[16]=24;d[17]=0x20;
    d[18]=b;d[19]=g;d[20]=r;return d;
}
struct Host{
    std::vector<std::uint8_t> sod=model(),blue=tga(255,0,0),red=tga(0,0,255),green=tga(0,255,0);
    unsigned reads=0,parts=0;std::vector<std::uint32_t> ordinals;
    int shown=0,missing=-1;bool reenter=false;std::int32_t reentry=0;
    const A2FO_N11_API9* api=nullptr;A2FO_N11_Session session=0;const A2FO_N11_Frame* frame=nullptr;
};
std::int32_t A2FO_N11_CALL asset(void* raw,std::uint32_t kind,const std::uint8_t* id,
    std::uint32_t length,std::uint32_t limit,A2FO_N11_BytesSink sink,void* context){
    auto& h=*static_cast<Host*>(raw);++h.reads;
    const std::string key(reinterpret_cast<const char*>(id),length);
    const std::vector<std::uint8_t>* data=nullptr;
    if(kind==1&&key=="scope-view")data=&h.sod;
    if(kind==2)data=key=="blue.tga"?&h.blue:key=="red.tga"?&h.red:key=="green.tga"?&h.green:nullptr;
    return data&&data->size()<=limit?sink(context,data->data(),static_cast<std::uint32_t>(data->size())):0;
}
std::int32_t A2FO_N11_CALL part(void* raw,const std::uint8_t* id,std::uint32_t length,
    std::uint32_t ordinal,const float authored[16],A2FO_N11_PartStateSink sink,void* context){
    auto& h=*static_cast<Host*>(raw);++h.parts;h.ordinals.push_back(ordinal);
    need(std::string(reinterpret_cast<const char*>(id),length)=="scope-view","part model identity");
    if(h.reenter){h.reentry=native11_c_submit_all(h.api,h.session,h.frame);h.reenter=false;}
    if(static_cast<int>(ordinal)==h.missing)return 0;
    A2FO_N11_PartState value{};value.struct_size=sizeof(value);
    value.visible=static_cast<int>(ordinal)==h.shown?1:0;
    std::memcpy(value.local_to_model,authored,sizeof(value.local_to_model));
    return sink(context,&value);
}
std::vector<std::uint8_t> pixels(const A2FO_N11_API& api,A2FO_N11_Session session){
    A2FO_N11_Readback read{};read.struct_size=sizeof(read);
    need(api.readback(session,&read)==S_FALSE&&read.width==32&&read.height==32&&
         read.required_bytes==32*32*4,"readback metadata");
    std::vector<std::uint8_t> out(read.required_bytes);read.bgra=out.data();read.capacity=out.size();
    ok(api.readback(session,&read),"GPU readback");return out;
}
std::array<int,3> sample(const std::vector<std::uint8_t>& p,unsigned x){
    const auto index=(16*32+x)*4;return {p[index],p[index+1],p[index+2]};
}
void colour(const std::vector<std::uint8_t>& p,unsigned x,int channel,const char* what){
    auto c=sample(p,x);need(c[channel]>100&&c[(channel+1)%3]<35&&c[(channel+2)%3]<35,what);
}
A2FO_N11_Usage usage(const A2FO_N11_API& api,A2FO_N11_Session session){
    A2FO_N11_Usage out{};out.struct_size=sizeof(out);ok(api.usage(session,&out),"usage");return out;
}
bool same_usage(const A2FO_N11_Usage& a,const A2FO_N11_Usage& b){
    return a.device_epoch==b.device_epoch&&a.mesh_bytes==b.mesh_bytes&&
        a.texture_bytes==b.texture_bytes&&a.meshes==b.meshes&&a.textures==b.textures;
}
void run(const char* path){
    HMODULE dll=LoadLibraryA(path);need(dll!=nullptr,"DLL load");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol!=nullptr,"GetInterface export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API9 api9{};ok(native11_c_query9(get,&api9),"C API9 query");
    auto& api=api9.base.base.base.base.base.base.base.base;
    need(api.version==9&&api.struct_size==80&&api.reserved==0&&
         (api.capabilities&A2FO_N11_CAP_ALL_MODEL_PARTS)&&api9.submit_world_all_parts,
         "API9 header and appended entry");
    A2FO_N11_API9 bad;std::memset(&bad,0x5a,sizeof(bad));const auto sentinel=bad;
    need(get(9,sizeof(bad)-1,&bad)==E_INVALIDARG&&std::memcmp(&bad,&sentinel,sizeof(bad))==0,
         "short API9 query leaves output untouched");
    need(get(UINT32_MAX,sizeof(bad),&bad)==E_NOINTERFACE&&std::memcmp(&bad,&sentinel,sizeof(bad))==0,
         "unknown version leaves output untouched");
    Host host;A2FO_N11_Host request{sizeof(request),0,32,32,&host,asset,nullptr};
    A2FO_N11_Session session=0;ok(api.create(&request,&session),"offscreen create");
    host.api=&api9;host.session=session;
    A2FO_N11_PartStates states{sizeof(states),0,&host,part};
    HWND window=CreateWindowExW(0,L"STATIC",L"Native11 model scope fixture",WS_POPUP,
                               0,0,32,32,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    need(window&&!IsWindowVisible(window),"hidden presentation fixture");
    A2FO_N11_PresentationTarget target{sizeof(target),0,window,32,32,0};
    ok(api9.base.configure_presentation(session,&target),"attach hidden target");
    A2FO_N11_Present present{sizeof(present),0,0,0};
    std::string name="scope-view";A2FO_N11_Object object{};identity(object.local_to_world);
    object.model_identifier=reinterpret_cast<const std::uint8_t*>(name.data());
    object.model_identifier_length=static_cast<std::uint32_t>(name.size());
    A2FO_N11_Frame frame{};frame.struct_size=sizeof(frame);frame.generation=frame.source_epoch=1;
    frame.flags=A2FO_N11_FRAME_COMPLETE_CASTERS;identity(frame.world_to_clip);
    frame.viewport[2]=frame.viewport[3]=32;frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;
    frame.object_count=1;frame.objects=&object;host.frame=&frame;
    frame.flags=A2FO_N11_FRAME_COMPLETE_CASTERS|A2FO_N11_FRAME_REQUIRE_PART_STATES;
    need(api9.submit_world_all_parts(session,&frame)==E_INVALIDARG&&host.reads==0,
         "all parts rejects absent resolver before asset reads");
    ok(api9.base.base.base.base.base.configure_part_states(session,&states),"part states");
    frame.flags=A2FO_N11_FRAME_REQUIRE_PART_STATES;
    need(api9.submit_world_all_parts(session,&frame)==E_INVALIDARG&&host.reads==0,
         "all parts requires complete caster declaration before asset reads");
    frame.flags=A2FO_N11_FRAME_COMPLETE_CASTERS;
    need(api9.submit_world_all_parts(session,&frame)==E_INVALIDARG&&host.reads==0,
         "all parts requires explicit part states before asset reads");
    auto old=api9.base.base.base.base.submit_world;
    ok(old(session,&frame),"API5 pristine highest LOD");
    auto pristine=pixels(api,session);
    colour(pristine,16,2,"API5 renders high red only");
    need(sample(pristine,6)[0]<35&&sample(pristine,26)[1]<35,
         "API5 excludes coarse and damage footprints");
    need(host.ordinals==std::vector<std::uint32_t>{0}&&host.reads==2,
         "API5 has one part and one texture");
    host.ordinals.clear();frame.flags|=A2FO_N11_FRAME_REQUIRE_PART_STATES;
    host.shown=1;host.reenter=true;++frame.generation;
    ok(native11_c_submit_all(&api9,session,&frame),"C API9 all parts submission");
    auto high=pixels(api,session);colour(high,16,2,"API9 high red");
    need(host.ordinals==std::vector<std::uint32_t>({0,1,2})&&host.reentry==E_UNEXPECTED,
         "all authored parts ordered and callback reentry refused");
    const auto resident=usage(api,session);const auto reads=host.reads;
    need(reads==6&&resident.meshes==3,"scope switch loads all three meshes and textures");
    host.shown=0;host.ordinals.clear();++frame.generation;
    ok(api9.submit_world_all_parts(session,&frame),"show coarse LOD");
    auto coarse=pixels(api,session);colour(coarse,6,0,"coarse blue visible");
    need(sample(coarse,16)[2]<35&&sample(coarse,26)[1]<35,
         "coarse selection hides high and damage footprints");
    need(host.ordinals==std::vector<std::uint32_t>({0,1,2})&&host.reads==reads&&
         same_usage(resident,usage(api,session)),"coarse toggle keeps resources and epoch");
    host.shown=2;host.ordinals.clear();++frame.generation;
    ok(api9.submit_world_all_parts(session,&frame),"show damage group");
    auto damage=pixels(api,session);colour(damage,26,1,"damage green visible");
    need(sample(damage,6)[0]<35&&sample(damage,16)[2]<35,
         "damage selection hides both LOD footprints");
    need(host.ordinals==std::vector<std::uint32_t>({0,1,2})&&host.reads==reads&&
         same_usage(resident,usage(api,session)),"damage toggle keeps resources and epoch");
    auto presented=api9.base.present(session,&present);
    need(presented==S_OK||presented==S_FALSE,"hidden target presents submitted frame");
    host.missing=2;host.ordinals.clear();++frame.generation;
    need(api9.submit_world_all_parts(session,&frame)<0&&pixels(api,session)==damage&&
         host.reads==reads,"missing part rejects and preserves last GPU image");
    need(api9.base.present(session,&present)==E_UNEXPECTED,
         "failed missing-part frame is ineligible for presentation");
    host.missing=-1;frame.flags=A2FO_N11_FRAME_COMPLETE_CASTERS;
    need(api9.submit_world_all_parts(session,&frame)==E_INVALIDARG&&
         pixels(api,session)==damage,"API9 requires part-state flag");
    host.shown=0;++frame.generation;ok(old(session,&frame),"API5 switches back to pristine scope");
    colour(pixels(api,session),16,2,"API5 still high red after scope switch");
    need(host.reads==reads+2,"scope switch rereads pristine model and texture");
    target.window=nullptr;target.width=target.height=0;
    ok(api9.base.configure_presentation(session,&target),"detach hidden target");
    need(DestroyWindow(window)!=0,"destroy hidden target");
    ok(api.destroy(session),"destroy");need(FreeLibrary(dll)!=0,"DLL unload");
}
}
int main(int argc,char** argv){need(argc==2,"renderer DLL path required");run(argv[1]);std::puts("native11 model scope native: ok");}
