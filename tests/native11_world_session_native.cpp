// Public API5 client for the externally synchronized owned world session.
#include "../renderer/native11/world_session.hpp"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
struct Source {std::shared_ptr<const ModelAsset> model;};
std::shared_ptr<const ModelAsset> resolve(void* raw,std::uint32_t db,std::uint64_t serial){
    auto model=static_cast<Source*>(raw)->model;
    return model&&model->info.database==db&&model->info.serial==serial?model:nullptr;
}
std::shared_ptr<const ModelAsset> model(){auto asset=std::make_shared<ModelAsset>();
    asset->info.database=7;asset->info.serial=11;std::strcpy(asset->info.name,"hull.sod");
    asset->bytes=native11_fixture::model();asset->info.byte_count=asset->bytes.size();return asset;}
std::shared_ptr<const BoundMaterialCapture> capture(Source& source,std::uint64_t generation,float x,bool observed=true){
    auto frame=std::make_unique<Frame>();frame->camera=1;frame->generation=generation;
    frame->object_count=1;for(int i=0;i<4;++i){frame->view.world_to_clip[i*5]=1;
        frame->objects[0].world[i*5]=1;}
    frame->view.viewport[2]=frame->view.viewport[3]=32;
    auto& object=frame->objects[0];object.instance=101;object.entity=201;
    object.database=7;object.model_serial=11;object.model_identity=ModelIdentity::plain_database_name;
    object.placement_identity=PlacementIdentity::authored_placement;object.world[3]=x;
    std::strcpy(object.model_name,"hull.sod");
    auto observation=std::make_shared<MaterialObservation>();observation->instance=101;
    observation->database=7;observation->model_serial=11;observation->model=source.model;
    MaterialBinding part{};part.node_state_valid=observed;
    observation->parts.push_back(part);
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> observations{{101,observation}};
    std::shared_ptr<const BoundMaterialCapture> bound;std::string error;
    need(build_bound_material_capture(*frame,10,resolve,&source,observations,bound,error),"sealed material capture");
    return bound;
}
std::shared_ptr<const OwnedWorldFrame> packet(const std::shared_ptr<const BoundMaterialCapture>& material){
    auto value=std::make_shared<OwnedWorldFrame>();value->materials=material;
    value->light_camera=material->frame().camera;value->light_generation=material->frame().generation;
    value->complete_casters=true;value->lighting.ambient[0]=value->lighting.ambient[1]=value->lighting.ambient[2]=.1f;
    return value;
}
WorldRenderSession* active=nullptr;
decltype(A2FO_N11_API5::submit_world) real_submit=nullptr;
bool refused=false;
decltype(A2FO_N11_API::create) real_create=nullptr;
bool create_close_refused=false;
A2FO_N11_Host retained_host{};
decltype(A2FO_N11_API::readback) real_readback=nullptr;
std::int32_t A2FO_N11_CALL count_bytes(void* raw,const std::uint8_t*,std::uint32_t){
    ++*static_cast<unsigned*>(raw);return 1;
}
std::int32_t A2FO_N11_CALL checked_readback(A2FO_N11_Session session,A2FO_N11_Readback* request){
    constexpr char id[]="n11:view:10:101";unsigned calls=0;
    need(retained_host.read_asset(retained_host.context,1,
        reinterpret_cast<const std::uint8_t*>(id),sizeof(id)-1,64*1024*1024,
        count_bytes,&calls)==0&&calls==0,"readback cannot resolve retained submission assets");
    return real_readback(session,request);
}
std::int32_t A2FO_N11_CALL checked_create(const A2FO_N11_Host* host,A2FO_N11_Session* session){
    retained_host=*host;
    std::string error;create_close_refused=!active->close(error)&&!error.empty();
    return real_create(host,session);
}
std::int32_t A2FO_N11_CALL checked_submit(A2FO_N11_Session session,const A2FO_N11_Frame* frame){
    std::string error;refused=!active->close(error)&&!error.empty()&&
        !active->render({},error)&&!error.empty();
    return real_submit(session,frame);
}
struct Foreign {WorldRenderSession* session;bool refused=false;};
DWORD WINAPI foreign_usage(void* raw){auto& request=*static_cast<Foreign*>(raw);
    A2FO_N11_Usage value{};std::string error;
    request.refused=!request.session->usage(value,error)&&!error.empty();return 0;}
int channel(const std::vector<std::uint8_t>& pixels,unsigned width,unsigned x,unsigned y){
    return pixels[(y*width+x)*4+2];}
void run(const char* path){
    HMODULE dll=LoadLibraryA(path);need(dll,"renderer DLL load");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol,"API export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API5 api{};need(get(5,sizeof(api),&api)>=0,"API5 query");
    real_submit=api.submit_world;api.submit_world=checked_submit;
    real_create=api.base.base.base.base.create;api.base.base.base.base.create=checked_create;
    real_readback=api.base.base.base.base.readback;api.base.base.base.base.readback=checked_readback;
    WorldRenderSession session;active=&session;std::string error;
    need(session.initialize(api,32,32,error)&&create_close_refused,"session initialize and close reentry refusal");
    A2FO_N11_API5 bad=api;bad.submit_world=nullptr;
    WorldRenderSession rejected;need(!rejected.initialize(bad,32,32,error),"incomplete API5 refused");
    Source source{model()};auto first=capture(source,1,0);
    auto unposed=packet(capture(source,1,0,false));
    need(!session.render(unposed,error)&&session.accepted_generation()==0,"unobserved pose cannot become world submission");
    unposed.reset();
    auto first_packet=packet(first);std::weak_ptr<const BoundMaterialCapture> first_weak=first;
    need(session.render(first_packet,error)&&refused&&session.accepted_generation()==1&&
        session.current()==first_packet,"first owned render and reentry refusal");
    std::vector<std::uint8_t> pixels;std::uint32_t width=0,height=0;
    need(session.readback(pixels,width,height,error)&&width==32&&height==32&&
        channel(pixels,width,16,16)>20,"first material pixels");
    A2FO_N11_Usage resident{};need(session.usage(resident,error)&&resident.meshes==1,
        "material resident after first render");
    Foreign foreign{&session};HANDLE worker=CreateThread(nullptr,0,foreign_usage,&foreign,0,nullptr);
    need(worker,"foreign call thread");need(WaitForSingleObject(worker,15000)==WAIT_OBJECT_0&&
        foreign.refused,"foreign session method refused");CloseHandle(worker);
    auto second=capture(source,2,.5f);auto second_packet=packet(second);
    Source replacement{model()};auto stale_epoch=packet(capture(replacement,2,.5f));
    need(!session.render(stale_epoch,error)&&session.current()==first_packet,
        "changed resource owner inside source epoch refused");
    stale_epoch.reset();replacement.model.reset();
    auto incomplete=std::make_shared<OwnedWorldFrame>(*second_packet);incomplete->complete_casters=false;
    need(!session.render(incomplete,error)&&session.current()==first_packet&&
        session.accepted_generation()==1,"incomplete casters refused before submit");
    auto mismatch=std::make_shared<OwnedWorldFrame>(*second_packet);mismatch->light_generation=3;
    need(!session.render(mismatch,error)&&session.current()==first_packet,"light generation mismatch refused");
    need(session.render(second_packet,error)&&session.accepted_generation()==2,
        "valid second generation accepted");
    need(session.readback(pixels,width,height,error)&&
        channel(pixels,width,24,16)>20&&channel(pixels,width,16,16)<10,
        "new placement replaces old pixels");
    A2FO_N11_Usage hot{};need(session.usage(hot,error)&&hot.meshes==resident.meshes&&
        hot.mesh_bytes==resident.mesh_bytes,"same source epoch remains resident");
    first_packet.reset();first.reset();need(first_weak.expired(),"replaced frame owners released");
    auto third=capture(source,3,.25f);auto third_packet=packet(third);
    auto failed=std::make_shared<OwnedWorldFrame>(*third_packet);
    Light invalid{};invalid.type=static_cast<LightType>(99);failed->lighting.lights.push_back(invalid);
    need(!session.render(failed,error)&&session.current()==second_packet&&
        session.accepted_generation()==2,"service failure retains owner and generation");
    need(session.render(third_packet,error)&&session.accepted_generation()==3,
        "failed generation may retry");
    need(!session.resize(0,0,error)&&session.current()==third_packet,"invalid resize retains packet");
    need(session.resize(64,64,error),"valid resize recovery");
    need(session.readback(pixels,width,height,error)&&width==64&&height==64,
        "resized target dimensions");
    auto fourth=capture(source,4,.25f);auto fourth_packet=packet(fourth);
    need(session.render(fourth_packet,error)&&session.readback(pixels,width,height,error)&&
        channel(pixels,width,20,16)>20,"render after resize");
    std::weak_ptr<const BoundMaterialCapture> last_weak=fourth;
    second_packet.reset();second.reset();third_packet.reset();third.reset();failed.reset();
    fourth_packet.reset();fourth.reset();incomplete.reset();mismatch.reset();source.model.reset();
    need(!last_weak.expired(),"session holds accepted frame");
    need(session.close(error)&&!session.current()&&last_weak.expired(),
        "explicit owner-thread close releases frame");
    active=nullptr;
    A2FO_N11_API6 api6{};need(get(6,sizeof(api6),&api6)>=0,"API6 query for shared session");
    real_submit=api6.base.submit_world;api6.base.submit_world=checked_submit;
    real_create=api6.base.base.base.base.base.create;
    api6.base.base.base.base.base.create=checked_create;
    WorldRenderSession shared;active=&shared;create_close_refused=refused=false;
    A2FO_N11_API6 malformed6=api6;malformed6.base.base.base.base.base.struct_size=63;
    need(!shared.initialize_shared(malformed6,32,32,error)&&!error.empty(),
        "malformed API6 metadata refused");
    malformed6=api6;malformed6.configure_mesh_sharing=nullptr;
    need(!shared.initialize_shared(malformed6,32,32,error)&&!error.empty(),
        "missing API6 mesh configuration refused");
    need(shared.initialize_shared(api6,32,32,error)&&create_close_refused,
        "shared session configures policy and refuses create reentry");
    Source shared_source{model()};auto shared_capture=capture(shared_source,1,0);
    auto shared_packet=packet(shared_capture);
    std::weak_ptr<const BoundMaterialCapture> shared_weak=shared_capture;
    need(shared.render(shared_packet,error)&&refused&&shared.accepted_generation()==1&&
        shared.current()==shared_packet,"API6 shared path renders owned packet and refuses submit reentry");
    need(shared.readback(pixels,width,height,error)&&width==32&&height==32&&
        channel(pixels,width,16,16)>20,"API6 shared path readback");
    A2FO_N11_Usage shared_usage{};
    need(shared.usage(shared_usage,error)&&shared_usage.meshes==1,
        "one-view shared session usage without deduplication claim");
    shared_packet.reset();shared_capture.reset();shared_source.model.reset();
    need(!shared_weak.expired()&&shared.close(error)&&shared_weak.expired(),
        "API6 owner-thread close releases retained capture");
    active=nullptr;need(FreeLibrary(dll)!=0,"renderer DLL unloaded after both sessions close");
}
}
int main(int argc,char** argv){need(argc==2,"usage: native11_world_session_native.exe RENDERER_DLL");
    run(argv[1]);std::puts("PASS native11 owned world session");}
