// Owned API7 session client. Uses only a private offscreen target and raw fixture bytes.
#include "../renderer/native11/primitive_session.hpp"
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* label){if(!yes){std::fprintf(stderr,"FAIL %s\n",label);std::exit(1);}}
std::shared_ptr<const PrimitiveFrame> packet(std::uint64_t generation,std::uint64_t epoch,
                                              float x,std::uint8_t red,std::uint8_t blue,
                                              bool bad_texture=false,bool bad_affine=false){
    auto result=std::make_shared<PrimitiveFrame>();result->generation=generation;result->source_epoch=epoch;
    PrimitiveMesh mesh{};mesh.id=11;mesh.vertices.resize(3);mesh.indices={0,1,2};
    const float positions[9]{-.4f,-.4f,.5f,.8f,-.4f,.5f,-.4f,.8f,.5f};
    for(unsigned i=0;i<3;++i){std::memcpy(mesh.vertices[i].position,positions+3*i,3*sizeof(float));
        mesh.vertices[i].normal[2]=-1;}
    result->meshes.push_back(std::move(mesh));
    PrimitiveTexture texture{};texture.id=21;texture.width=texture.height=1;
    texture.bgra={blue,0,red,255};if(bad_texture)texture.bgra.pop_back();
    result->textures.push_back(std::move(texture));
    PrimitiveDraw draw{};draw.mesh=11;draw.texture=21;draw.surface.flags=two_sided;
    draw.surface.local_to_world[3]=x;if(bad_affine)draw.surface.local_to_world[15]=0;
    result->draws.push_back(draw);
    result->settings.ambient[0]=result->settings.ambient[1]=result->settings.ambient[2]=1;
    result->settings.clear_colour[0]=result->settings.clear_colour[1]=result->settings.clear_colour[2]=0;
    return result; // no mutable alias escapes publication
}
int channel(const std::vector<std::uint8_t>& bytes,unsigned width,unsigned x,unsigned y,unsigned c){
    return bytes[(std::size_t(y)*width+x)*4+c];
}
PrimitiveRenderSession* active=nullptr;
decltype(A2FO_N11_API::create) real_create=nullptr;
decltype(A2FO_N11_API7::submit_primitives) real_submit=nullptr;
decltype(A2FO_N11_API::readback) real_readback=nullptr;
decltype(A2FO_N11_API::usage) real_usage=nullptr;
enum class ReadFault {none,metadata_width,pixel_width,pixel_sfalse};
ReadFault read_fault=ReadFault::none;
enum class UsageFault {none,size,reserved};
UsageFault usage_fault=UsageFault::none;
bool create_refused=false,submit_refused=false;
std::int32_t A2FO_N11_CALL checked_create(const A2FO_N11_Host* host,A2FO_N11_Session* out){
    std::string error;create_refused=!active->close(error)&&!error.empty();
    return real_create(host,out);
}
std::int32_t A2FO_N11_CALL checked_submit(A2FO_N11_Session id,const A2FO_N11_PrimitiveFrame* frame){
    std::string error;A2FO_N11_Usage usage{};
    submit_refused=!active->close(error)&&!error.empty()&&
                   !active->usage(usage,error)&&!error.empty();
    return real_submit(id,frame);
}
std::int32_t A2FO_N11_CALL checked_readback(A2FO_N11_Session id,A2FO_N11_Readback* output){
    if(read_fault==ReadFault::pixel_sfalse&&output&&output->bgra){
        A2FO_N11_Readback metadata{};metadata.struct_size=sizeof(metadata);
        const auto result=real_readback(id,&metadata);
        return result==S_FALSE?S_FALSE:result; // never fills the caller's pixel buffer
    }
    const auto result=real_readback(id,output);
    if(result>=0&&output&&
       ((read_fault==ReadFault::metadata_width&&!output->bgra)||
        (read_fault==ReadFault::pixel_width&&output->bgra)))++output->width;
    return result;
}
std::int32_t A2FO_N11_CALL checked_usage(A2FO_N11_Session id,A2FO_N11_Usage* output){
    const auto result=real_usage(id,output);
    if(result==S_OK&&output){
        if(usage_fault==UsageFault::size)--output->struct_size;
        if(usage_fault==UsageFault::reserved)output->reserved=1;
    }
    return result;
}
struct Foreign {PrimitiveRenderSession* session;bool usage_refused=false,close_refused=false;};
DWORD WINAPI foreign_methods(void* raw){auto& call=*static_cast<Foreign*>(raw);
    A2FO_N11_Usage usage{};std::string error;
    call.usage_refused=!call.session->usage(usage,error)&&!error.empty();
    call.close_refused=!call.session->close(error)&&!error.empty();return 0;
}
void read(PrimitiveRenderSession& session,std::vector<std::uint8_t>& bytes,
          std::uint32_t& width,std::uint32_t& height){
    std::string error;need(session.readback(bytes,width,height,error)&&error.empty(),"owned readback");
    need(bytes.size()==std::size_t(width)*height*4,"readback dimensions");
}
void run(const char* path){
    HMODULE dll=LoadLibraryA(path);need(dll!=nullptr,"load renderer DLL");
    const FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol!=nullptr,"API export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API7 api{};need(get(7,sizeof(api),&api)>=0,"query API7");
    std::string error;PrimitiveRenderSession rejected;
    auto malformed=api;malformed.base.base.base.base.base.base.struct_size=67;
    need(!rejected.initialize(malformed,32,32,error)&&!error.empty(),"malformed API7 refused");
    malformed=api;malformed.submit_primitives=nullptr;
    need(!rejected.initialize(malformed,32,32,error)&&!error.empty(),"missing submit callback refused");
    auto& base=api.base.base.base.base.base.base;
    real_create=base.create;base.create=checked_create;
    real_submit=api.submit_primitives;api.submit_primitives=checked_submit;
    real_readback=base.readback;base.readback=checked_readback;
    real_usage=base.usage;base.usage=checked_usage;
    PrimitiveRenderSession session;active=&session;
    need(session.initialize(api,32,32,error)&&create_refused,"initialize and create reentry refusal");
    std::vector<std::uint8_t> bytes{0x5a};std::uint32_t width=77,height=88;
    need(!session.readback(bytes,width,height,error)&&bytes==std::vector<std::uint8_t>{0x5a}&&
         width==77&&height==88,"no readback before first render");
    auto first=packet(1,1,0,255,0);std::weak_ptr<const PrimitiveFrame> first_weak=first;
    need(session.render(first,error)&&submit_refused&&session.current()==first&&
         session.accepted_generation()==1,"first render and submit reentry refusal");
    read(session,bytes,width,height);need(width==32&&height==32&&channel(bytes,32,16,16,2)>240,
        "first red primitive pixels");
    auto first_image=bytes;A2FO_N11_Usage resident{};
    need(session.usage(resident,error)&&resident.meshes==1&&resident.textures==2,
         "first manifest residency includes implicit white");
    bytes={0x5a};width=77;height=88;read_fault=ReadFault::metadata_width;
    need(!session.readback(bytes,width,height,error)&&!error.empty()&&
         bytes==std::vector<std::uint8_t>{0x5a}&&width==77&&height==88,
         "malformed readback metadata preserves caller outputs");
    read_fault=ReadFault::pixel_width;
    need(!session.readback(bytes,width,height,error)&&!error.empty()&&
         bytes==std::vector<std::uint8_t>{0x5a}&&width==77&&height==88,
         "changed pixel response header preserves caller outputs");
    read_fault=ReadFault::pixel_sfalse;
    need(!session.readback(bytes,width,height,error)&&!error.empty()&&
         bytes==std::vector<std::uint8_t>{0x5a}&&width==77&&height==88,
         "S_FALSE without pixel fill preserves caller outputs");
    read_fault=ReadFault::none;read(session,bytes,width,height);
    need(bytes==first_image,"normal readback recovers after malformed responses");
    A2FO_N11_Usage sentinel{};sentinel.struct_size=77;sentinel.meshes=99;
    usage_fault=UsageFault::size;
    need(!session.usage(sentinel,error)&&sentinel.struct_size==77&&sentinel.meshes==99,
         "malformed usage size preserves caller output");
    usage_fault=UsageFault::reserved;
    need(!session.usage(sentinel,error)&&sentinel.struct_size==77&&sentinel.meshes==99,
         "malformed usage reserved preserves caller output");
    usage_fault=UsageFault::none;
    need(session.usage(sentinel,error)&&sentinel.meshes==resident.meshes,
         "normal usage recovers after malformed responses");
    Foreign foreign{&session};HANDLE thread=CreateThread(nullptr,0,foreign_methods,&foreign,0,nullptr);
    need(thread&&WaitForSingleObject(thread,15000)==WAIT_OBJECT_0,"foreign methods returned");
    CloseHandle(thread);need(foreign.usage_refused&&foreign.close_refused,"foreign methods refused");
    auto second=packet(2,1,.5f,255,0);
    need(session.render(second,error)&&session.current()==second&&session.accepted_generation()==2,
         "hot pose accepted");
    A2FO_N11_Usage hot{};need(session.usage(hot,error)&&hot.meshes==resident.meshes&&
        hot.textures==resident.textures&&hot.mesh_bytes==resident.mesh_bytes&&
        hot.texture_bytes==resident.texture_bytes,"hot pose retains resources");
    read(session,bytes,width,height);
    need(channel(bytes,32,24,16,2)>240&&channel(bytes,32,16,16,2)<10,
         "hot pose moves pixels");
    first.reset();need(first_weak.expired(),"replaced owner released");
    const auto prior=bytes;
    auto stale=packet(2,1,0,255,0);
    need(!session.render(stale,error)&&session.current()==second&&
         session.accepted_generation()==2,"stale generation refused");
    read(session,bytes,width,height);need(bytes==prior,"stale frame preserves image");
    auto bad_resource=packet(3,2,0,0,255,true);
    need(!session.render(bad_resource,error)&&session.current()==second&&
         session.accepted_generation()==2,"bad new resource refused");
    read(session,bytes,width,height);need(bytes==prior,"bad resource preserves image");
    auto bad_state=packet(3,2,0,0,255,false,true);
    need(!session.render(bad_state,error)&&session.current()==second&&
         session.accepted_generation()==2,"bad new state refused after candidate upload");
    read(session,bytes,width,height);need(bytes==prior,"bad state preserves image");
    A2FO_N11_Usage still{};need(session.usage(still,error)&&still.meshes==resident.meshes&&
        still.textures==resident.textures&&still.mesh_bytes==resident.mesh_bytes&&
        still.texture_bytes==resident.texture_bytes,"failed new epoch rolled back resources");
    auto third=packet(3,2,0,0,255);std::weak_ptr<const PrimitiveFrame> last_weak=third;
    need(session.render(third,error)&&session.current()==third&&session.accepted_generation()==3,
         "failed generation retried");
    read(session,bytes,width,height);need(channel(bytes,32,16,16,0)>240,
         "new epoch blue primitive pixels");
    need(!session.resize(0,0,error)&&session.current()==third,"invalid resize refused");
    read(session,bytes,width,height);need(channel(bytes,32,16,16,0)>240,
         "invalid resize preserves image eligibility");
    need(session.resize(64,64,error),"valid resize");
    bytes={0x3c};width=77;height=88;
    need(!session.readback(bytes,width,height,error)&&bytes==std::vector<std::uint8_t>{0x3c}&&
         width==77&&height==88,"resize requires fresh render before readback");
    auto fourth=packet(4,2,0,0,255);need(session.render(fourth,error),"fresh clear frame after resize");
    read(session,bytes,width,height);need(width==64&&height==64&&channel(bytes,64,32,32,0)>240,
         "resized primitive image");
    second.reset();stale.reset();bad_resource.reset();bad_state.reset();third.reset();
    need(last_weak.expired(),"replaced third owner released");
    std::weak_ptr<const PrimitiveFrame> fourth_weak=fourth;fourth.reset();
    need(!fourth_weak.expired(),"session retains current packet");
    need(session.close(error)&&!session.current()&&fourth_weak.expired(),
         "explicit close releases last owner");
    active=nullptr;need(FreeLibrary(dll)!=0,"unload helper after close");
    need(first_image!=prior,"hot pose changed image");
    std::puts("PASS owned API7 primitive session, admission, retry, resize and lifetime");
}
}
int main(int argc,char** argv){need(argc==2,"renderer DLL path argument");run(argv[1]);}
