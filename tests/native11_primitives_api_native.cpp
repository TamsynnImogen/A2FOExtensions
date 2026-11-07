// Public API7 client. Raw geometry is fixture-owned, with no engine or GUI input.
#include "../sdk/include/a2fo_native11_primitives_api.h"
#include "../renderer/native11/assets.h"
#include <windows.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" std::int32_t native11_c_query7(A2FO_N11_GetInterface,A2FO_N11_API7*);
namespace {
void need(bool yes,const char* label){if(!yes){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
void ok(std::int32_t hr,const char* label){if(hr<0){std::fprintf(stderr,"FAIL %s hr=%08lx\n",label,static_cast<unsigned long>(hr));std::exit(1);}}
void identity(float* m){for(unsigned i=0;i<16;++i)m[i]=(i%5)==0?1.f:0.f;}
struct Host {
    A2FO_N11_API7* table=nullptr;A2FO_N11_Session session=0;
    const A2FO_N11_PrimitiveFrame* nested=nullptr;
    unsigned reads=0,logs=0;bool attempt_reentry=false,reentry_refused=false;
};
std::int32_t A2FO_N11_CALL unused_asset(void* raw,std::uint32_t,const std::uint8_t*,
    std::uint32_t,std::uint32_t,A2FO_N11_BytesSink,void*){
    ++static_cast<Host*>(raw)->reads;return 0;
}
void A2FO_N11_CALL log_failure(void* raw,const char* text,std::uint32_t count){
    auto& h=*static_cast<Host*>(raw);++h.logs;
    need(text&&count,"bounded failure log");
    if(h.attempt_reentry){h.attempt_reentry=false;
        h.reentry_refused=h.table->submit_primitives(h.session,h.nested)==E_UNEXPECTED;
    }
}
struct Packet {
    std::array<A2FO_N11_Vertex,3> left{},right{};
    std::array<std::uint32_t,3> indices{{0,1,2}};
    std::array<std::uint8_t,8> pixels{{0,0,255,255,0,255,0,255}};
    std::array<A2FO_N11_PrimitiveMesh,2> meshes{};
    A2FO_N11_PrimitiveTexture texture{};
    std::array<A2FO_N11_PrimitiveDraw,2> draws{};
    A2FO_N11_PrimitiveFrame frame{};
    Packet(){
        for(auto* vertices:{&left,&right}){
            const float positions[9]{-1,-1,.5f,3,-1,.5f,-1,3,.5f};
            for(unsigned i=0;i<3;++i){
                std::memcpy((*vertices)[i].position,positions+i*3,3*sizeof(float));
                (*vertices)[i].normal[2]=-1;(*vertices)[i].uv[0]=.25f;
                (*vertices)[i].uv[1]=.5f;
            }
        }
        meshes[0]={11,3,3,left.data(),indices.data()};
        meshes[1]={12,3,3,right.data(),indices.data()};
        texture={21,2,1,pixels.data(),static_cast<std::uint32_t>(pixels.size())};
        for(unsigned i=0;i<draws.size();++i){
            auto& d=draws[i];d.mesh=meshes[i].identity;d.texture=i?0:texture.identity;
            identity(d.local_to_world);d.local_to_world[0]=d.local_to_world[5]=.5f;
            d.local_to_world[3]=i?.5f:-.5f;
            d.colour[0]=d.colour[1]=d.colour[2]=d.colour[3]=1;
            if(i){d.colour[0]=d.colour[1]=0;}
            d.flags=1;d.alpha_cutoff=.5f;d.uv_transform[0]=d.uv_transform[4]=1;
        }
        frame.struct_size=sizeof(frame);frame.generation=frame.source_epoch=1;
        identity(frame.world_to_clip);frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=1;
        frame.clear_colour[3]=1;frame.shadow_resolution=512;frame.shadow_bias=.001f;
        frame.fog_end=1;frame.mesh_count=2;frame.texture_count=1;frame.draw_count=2;
        frame.meshes=meshes.data();frame.textures=&texture;frame.draws=draws.data();
    }
};
std::vector<std::uint8_t> image(const A2FO_N11_API& api,A2FO_N11_Session session){
    A2FO_N11_Readback out{};out.struct_size=sizeof(out);
    need(api.readback(session,&out)==S_FALSE&&out.width==32&&out.height==32&&out.required_bytes==4096,
        "readback metadata");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(out.required_bytes));
    out.bgra=bytes.data();out.capacity=bytes.size();ok(api.readback(session,&out),"readback bytes");
    return bytes;
}
int pixel(const std::vector<std::uint8_t>& bytes,unsigned x,unsigned channel){return bytes[(16*32+x)*4+channel];}
void colour(const std::vector<std::uint8_t>& bytes,unsigned x,int red,int green,int blue,const char* label){
    need(std::abs(pixel(bytes,x,2)-red)<6&&std::abs(pixel(bytes,x,1)-green)<6&&
         std::abs(pixel(bytes,x,0)-blue)<6,label);
}
A2FO_N11_Usage usage(const A2FO_N11_API& api,A2FO_N11_Session session){
    A2FO_N11_Usage value{};value.struct_size=sizeof(value);ok(api.usage(session,&value),"usage");return value;
}
void same_usage(const A2FO_N11_Usage& a,const A2FO_N11_Usage& b,const char* label){
    need(a.device_epoch==b.device_epoch&&a.meshes==b.meshes&&a.textures==b.textures&&
         a.mesh_bytes==b.mesh_bytes&&a.texture_bytes==b.texture_bytes,label);
}
struct Foreign {A2FO_N11_API7* table;A2FO_N11_Session session;const A2FO_N11_PrimitiveFrame* frame;
    std::int32_t result=0;};
DWORD WINAPI foreign_submit(void* raw){auto& f=*static_cast<Foreign*>(raw);
    f.result=f.table->submit_primitives(f.session,f.frame);return 0;}
void refused(A2FO_N11_API7& table,A2FO_N11_Session session,A2FO_N11_PrimitiveFrame& frame,
             const A2FO_N11_API& base,const std::vector<std::uint8_t>& prior,const char* label){
    need(table.submit_primitives(session,&frame)==E_INVALIDARG,label);
    need(image(base,session)==prior,"refusal preserves prior pixels");
}
void run(const char* path){
    HMODULE dll=LoadLibraryA(path);need(dll!=nullptr,"load helper");
    const FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol!=nullptr,"export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API a1{};A2FO_N11_API2 a2{};A2FO_N11_API3 a3{};
    A2FO_N11_API4 a4{};A2FO_N11_API5 a5{};A2FO_N11_API6 a6{};A2FO_N11_API7 a7{};
    ok(get(1,sizeof(a1),&a1),"query API1");ok(get(2,sizeof(a2),&a2),"query API2");
    ok(get(3,sizeof(a3),&a3),"query API3");ok(get(4,sizeof(a4),&a4),"query API4");
    ok(get(5,sizeof(a5),&a5),"query API5");ok(get(6,sizeof(a6),&a6),"query API6");
    ok(native11_c_query7(get,&a7),"C11 query API7");
    need(a1.version==1&&a1.struct_size==40&&a2.base.version==2&&a2.base.struct_size==48&&
         a3.base.base.version==3&&a3.base.base.struct_size==52&&
         a4.base.base.base.version==4&&a4.base.base.base.struct_size==56&&
         a5.base.base.base.base.version==5&&a5.base.base.base.base.struct_size==60&&
         a6.base.base.base.base.base.version==6&&a6.base.base.base.base.base.struct_size==64&&
         a7.base.base.base.base.base.base.version==7&&
         a7.base.base.base.base.base.base.struct_size==68&&a7.submit_primitives&&
         (a7.base.base.base.base.base.base.capabilities&A2FO_N11_CAP_PRIMITIVES)&&
         !(a1.capabilities&A2FO_N11_CAP_PRIMITIVES)&&
         !(a2.base.capabilities&A2FO_N11_CAP_PRIMITIVES)&&
         !(a3.base.base.capabilities&A2FO_N11_CAP_PRIMITIVES)&&
         !(a4.base.base.base.capabilities&A2FO_N11_CAP_PRIMITIVES)&&
         !(a5.base.base.base.base.capabilities&A2FO_N11_CAP_PRIMITIVES)&&
         !(a6.base.base.base.base.base.capabilities&A2FO_N11_CAP_PRIMITIVES),"API prefixes and capability");
    A2FO_N11_API7 malformed{};std::memset(&malformed,0x5a,sizeof(malformed));
    const auto before=malformed;
    need(get(7,67,&malformed)==E_INVALIDARG&&std::memcmp(&malformed,&before,sizeof(before))==0,
         "malformed query leaves output untouched");
    auto& api=a7.base.base.base.base.base.base;
    Host host;host.table=&a7;A2FO_N11_Host request{sizeof(request),0,32,32,&host,unused_asset,log_failure};
    ok(api.create(&request,&host.session),"offscreen session");Packet packet;
    ok(a7.submit_primitives(host.session,&packet.frame),"primitive frame");
    auto current=image(api,host.session);colour(current,8,255,0,0,"left textured red");
    colour(current,24,0,0,255,"right implicit white blue");
    const auto base_usage=usage(api,host.session);
    need(base_usage.meshes==2&&base_usage.textures==2&&host.reads==0,"owned resource manifest, no asset reads");
    packet.frame.generation=2;packet.draws[0].local_to_world[3]=-.4f;
    ok(a7.submit_primitives(host.session,&packet.frame),"hot pose frame");
    same_usage(base_usage,usage(api,host.session),"hot pose retained GPU resources");
    need(host.reads==0,"hot frame has no asset callback");current=image(api,host.session);
    packet.frame.generation=3;packet.draws[0].uv_transform[2]=.5f;
    packet.draws[0].sampling=2;ok(a7.submit_primitives(host.session,&packet.frame),"UV surface mapping");
    current=image(api,host.session);colour(current,8,0,255,0,"UV shift chooses green texel");
    same_usage(base_usage,usage(api,host.session),"surface change retained GPU resources");
    auto bad=packet.frame;bad.generation=0;refused(a7,host.session,bad,api,current,"zero generation");
    bad=packet.frame;bad.generation=4;bad.source_epoch=0;refused(a7,host.session,bad,api,current,"zero epoch");
    bad=packet.frame;bad.generation=4;bad.mesh_count=2049;refused(a7,host.session,bad,api,current,"mesh count bound");
    bad=packet.frame;bad.generation=4;bad.meshes=nullptr;refused(a7,host.session,bad,api,current,"missing mesh pointer");
    bad=packet.frame;bad.generation=4;bad.flags=4;refused(a7,host.session,bad,api,current,"unknown frame flag");
    bad=packet.frame;bad.generation=4;bad.reserved=1;refused(a7,host.session,bad,api,current,"frame reserved");
    auto draw_backup=packet.draws[0];packet.draws[0].options=16;
    bad=packet.frame;bad.generation=4;refused(a7,host.session,bad,api,current,"unknown surface option");
    packet.draws[0]=draw_backup;packet.draws[0].reserved=1;
    refused(a7,host.session,bad,api,current,"draw reserved");packet.draws[0]=draw_backup;
    packet.pixels[0]=255;packet.pixels[2]=0;bad=packet.frame;bad.generation=4;
    refused(a7,host.session,bad,api,current,"same-epoch changed texture bytes");
    packet.frame.generation=4;packet.frame.source_epoch=2;
    packet.draws[0].uv_transform[2]=0;
    ok(a7.submit_primitives(host.session,&packet.frame),"new epoch changed resource");
    current=image(api,host.session);colour(current,8,0,0,255,"new epoch blue texture bytes");
    same_usage(base_usage,usage(api,host.session),"new epoch replaced, not leaked resources");
    const auto prior=current;const auto prior_usage=usage(api,host.session);
    packet.frame.generation=5;packet.frame.source_epoch=3;packet.pixels[0]=0;packet.pixels[2]=255;
    packet.draws[0].local_to_world[15]=0;
    refused(a7,host.session,packet.frame,api,prior,"candidate upload with invalid affine");
    same_usage(prior_usage,usage(api,host.session),"failed candidate rolled back GPU resources");
    packet.draws[0].local_to_world[15]=1;
    ok(a7.submit_primitives(host.session,&packet.frame),"corrected candidate retry");
    current=image(api,host.session);same_usage(base_usage,usage(api,host.session),"retry resource count");
    Foreign foreign{&a7,host.session,&packet.frame};HANDLE thread=CreateThread(nullptr,0,foreign_submit,&foreign,0,nullptr);
    need(thread&&WaitForSingleObject(thread,15000)==WAIT_OBJECT_0,"foreign thread completed");
    CloseHandle(thread);need(foreign.result==E_UNEXPECTED,"foreign submit refused");
    packet.frame.generation=6;packet.draws[0].local_to_world[15]=0;
    host.nested=&packet.frame;host.attempt_reentry=true;
    refused(a7,host.session,packet.frame,api,current,"logged invalid affine");
    need(host.logs&&host.reentry_refused,"log callback reentry refused");
    packet.draws[0].local_to_world[15]=1;
    need(host.reads==0,"primitive path never invoked asset reader");
    ok(api.destroy(host.session),"destroy session");FreeLibrary(dll);
    std::puts("PASS public API7 primitive ABI, owned resources, hot pose/surface, epoch rollback and admission");
}
}
int main(int argc,char** argv){need(argc==2,"renderer DLL path argument");run(argv[1]);}
