// End-to-end owned model source to offscreen API10 geometry policy.
#include "../renderer/native11/model_store.hpp"
#include "../renderer/native11/world_session.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <cstdint>
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
void identity(float value[16]){for(int i=0;i<16;++i)value[i]=i%5==0?1.f:0.f;}
std::uint32_t bits(float value){std::uint32_t result=0;std::memcpy(&result,&value,4);return result;}
OpenedFile file(){OpenedFile result;result.name="geometry.sod";
    result.bytes=native11_fixture::model("",true);return result;}
bool no_legacy_modules(){
    for(const char* name:{"d3d8.dll","d3d9.dll","d3dx8.dll","d3dx9_43.dll"})
        if(GetModuleHandleA(name))return false;
    return true;
}
struct Source {std::shared_ptr<const ModelAsset> model;};
std::shared_ptr<const ModelAsset> resolve(void* raw,std::uint32_t database,std::uint64_t serial){
    auto model=static_cast<Source*>(raw)->model;
    return model&&model->info.database==database&&model->info.serial==serial?model:nullptr;
}
std::shared_ptr<const OwnedWorldFrame> packet(Source& source,std::uint64_t generation,
    std::uint64_t source_epoch,std::uint32_t draw_control){
    auto frame=std::make_unique<Frame>();frame->camera=1;frame->generation=generation;
    frame->object_count=1;identity(frame->view.world_to_clip);
    frame->view.viewport[2]=frame->view.viewport[3]=32;
    auto& object=frame->objects[0];object.entity=40;object.instance=50;
    object.database=source.model->info.database;object.model_serial=source.model->info.serial;
    object.model_identity=ModelIdentity::plain_database_name;
    object.placement_identity=PlacementIdentity::authored_placement;
    identity(object.world);std::strcpy(object.model_name,"geometry.sod");
    auto observation=std::make_shared<MaterialObservation>();observation->instance=50;
    observation->database=object.database;observation->model_serial=object.model_serial;
    observation->model=source.model;observation->native_x87_control=draw_control;
    MaterialBinding binding{};binding.node_state_valid=true;
    binding.node_state.local_to_model[3]=.25f;
    observation->parts.push_back(binding);
    std::map<std::uint32_t,std::shared_ptr<const MaterialObservation>> values{{50,observation}};
    std::shared_ptr<const BoundMaterialCapture> bound;std::string error;
    need(build_bound_material_capture(*frame,source_epoch,resolve,&source,values,bound,error)&&
         error.empty()&&bound->frame().requires_part_states&&bound->native_x87_control(50)==draw_control,
         "owned material capture with explicit part translation and draw CW");
    auto world=std::make_shared<OwnedWorldFrame>();world->materials=bound;
    world->light_camera=1;world->light_generation=generation;world->complete_casters=true;
    world->lighting.ambient[0]=world->lighting.ambient[1]=world->lighting.ambient[2]=1.f;
    return world;
}
std::vector<std::uint8_t> read(WorldRenderSession& session){
    std::vector<std::uint8_t> pixels;std::uint32_t width=0,height=0;std::string error;
    need(session.readback(pixels,width,height,error)&&width==32&&height==32&&
         pixels.size()==32*32*4,"owned offscreen readback");return pixels;
}
int brightness(const std::vector<std::uint8_t>& pixels,unsigned x){
    const std::size_t offset=(16*32+x)*4;
    return int(pixels[offset])+int(pixels[offset+1])+int(pixels[offset+2]);
}
void run(const char* path){
    need(no_legacy_modules(),"legacy D3D8/9 modules absent before fixture");
    HMODULE dll=LoadLibraryA(path);need(dll!=nullptr,"renderer DLL load");
    FARPROC symbol=GetProcAddress(dll,"A2FO_Native11_GetInterface");need(symbol!=nullptr,"API export");
    A2FO_N11_GetInterface get=nullptr;std::memcpy(&get,&symbol,sizeof(get));
    A2FO_N11_API10 api10{};need(get(10,sizeof(api10),&api10)==S_OK,"API10 query");
    A2FO_N11_API9 api9{};need(get(9,sizeof(api9),&api9)==S_OK,"API9 query");
    constexpr auto load=encode_native_x87(0x037f);
    constexpr auto draw=encode_native_x87(0x0b7f);
    need(load!=draw,"fixture separates LOAD and DRAW CW");
    ModelAssetStore store;
    need(store.publish(7,bits(.5f),file(),load),"publish immutable scaled source");
    ModelAssetInfo first_info{};need(store.info(7,first_info),"first source metadata");
    Source original{store.find(7,first_info.serial)};need(original.model!=nullptr,"first source owner");
    auto first=packet(original,1,10,draw);
    std::weak_ptr<const ModelAsset> retained=original.model;
    original.model.reset();store.clear();
    need(store.size()==0&&!retained.expired()&&
         first->materials->base()->models().size()==1&&
         first->materials->base()->models().begin()->second==retained.lock(),
         "bound source survives store clear without mutable store alias");
    WorldRenderSession session;std::string error;
    need(session.initialize_native_geometry(api10,32,32,error),"API10 owned world session");
    need(session.render(first,error)&&session.current()==first&&session.accepted_generation()==1,
         "owned source renders with captured LOAD policy after store clear");
    auto scaled=read(session);
    need(brightness(scaled,15)<30&&brightness(scaled,20)>100,
         "owned API10 half-size footprint with unchanged part translation");
    A2FO_N11_Usage resident{};
    need(session.usage(resident,error)&&resident.meshes==1,"one scaled GPU mesh");
    need(store.publish(7,bits(.5f),file(),0),"publish source with unknown LOAD CW");
    ModelAssetInfo unknown_info{};need(store.info(7,unknown_info)&&
        unknown_info.serial>first_info.serial,"distinct unknown source version");
    Source unknown{store.find(7,unknown_info.serial)};
    auto failed=packet(unknown,2,11,draw);
    need(failed->materials->native_x87_control(50)==draw&&
         !session.render(failed,error)&&!error.empty()&&
         session.current()==first&&session.accepted_generation()==1&&read(session)==scaled,
         "unknown LOAD CW fails despite known DRAW CW without accepting frame");
    need(session.close(error),"API10 explicit close");
    need(session.initialize_all_parts(api9,32,32,error),"API9 owned all-parts session");
    need(session.render(first,error)&&session.accepted_generation()==1,"API9 original source frame");
    auto raw=read(session);
    need(brightness(raw,15)>100&&brightness(raw,20)>100&&raw!=scaled,
         "API9 preserves unscaled footprint from same pinned packet");
    need(session.close(error),"API9 explicit close");
    first.reset();original.model.reset();unknown.model.reset();failed.reset();store.clear();
    need(retained.expired(),"session and packets release pinned original source");
    need(FreeLibrary(dll)!=0&&no_legacy_modules(),"DLL unload without legacy graphics modules");
}
}
int main(int argc,char** argv){need(argc==2,"renderer DLL path required");
    run(argv[1]);std::puts("PASS native11 owned geometry world");}
