// Private offscreen renderer test for opt-in geometry sharing across model views.
#include "../renderer/native11/renderer.hpp"
#include "../renderer/native11/submission.hpp"
#include "native11_model_fixture.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <iterator>
#include <string>
#include <vector>
using namespace a2fo::native11;
namespace {
void need(bool good,const char* label) {
    if (!good) { std::fprintf(stderr,"FAIL %s\n",label);std::exit(1); }
}
void ok(HRESULT hr,const char* label) {
    if (FAILED(hr)) { std::fprintf(stderr,"FAIL %s hr=%08lx\n",label,static_cast<unsigned long>(hr));std::exit(1); }
}
std::vector<std::uint8_t> tga(std::uint8_t b,std::uint8_t g,std::uint8_t r) {
    std::vector<std::uint8_t> bytes(22);bytes[2]=2;bytes[12]=1;bytes[14]=1;
    bytes[16]=24;bytes[17]=0x20;bytes[18]=b;bytes[19]=g;bytes[20]=r;
    return bytes;
}
std::vector<std::uint8_t> altered_vertex() {
    auto bytes=native11_fixture::model("authored.tga");
    const float vertices[]{-.75f,-.75f,.2f,.75f,-.75f,.2f,0.f,.75f,.2f};
    const auto* first=reinterpret_cast<const std::uint8_t*>(vertices);
    auto found=std::search(bytes.begin(),bytes.end(),first,first+sizeof(vertices));
    need(found!=bytes.end(),"fixture vertex payload located");
    const float changed=-.65f;std::memcpy(&*found,&changed,sizeof(changed));
    return bytes;
}
std::vector<std::uint8_t> altered_local_matrix() {
    auto bytes=native11_fixture::model("authored.tga");
    const std::uint8_t marker[]{4,0,'m','e','s','h',0,0};
    auto found=std::search(bytes.begin(),bytes.end(),std::begin(marker),std::end(marker));
    need(found!=bytes.end()&&std::size_t(bytes.end()-found)>=sizeof(marker)+48,
         "fixture mesh transform located");
    const float shifted=.2f;
    std::memcpy(&*(found+sizeof(marker)+9*4),&shifted,sizeof(shifted));
    return bytes;
}
struct Reader {
    std::map<std::string,std::vector<std::uint8_t>> models,textures;
    unsigned model_reads=0,texture_reads=0,resolutions=0;
    bool forbid_reads=false;
    Reader() {
        const auto same=native11_fixture::model("authored.tga");
        for (const char* name:{"view-a","view-b","view-c"})models.emplace(name,same);
        models.emplace("view-d",altered_vertex());
        models.emplace("view-e",altered_local_matrix());
        textures.emplace("red.tga",tga(0,0,255));
        textures.emplace("green.tga",tga(0,255,0));
    }
    static bool read(void* raw,AssetKind kind,const std::string& id,
        std::vector<std::uint8_t>& out,std::string& error) {
        auto& state=*static_cast<Reader*>(raw);
        if (state.forbid_reads) { error="unexpected asset read";return false; }
        const auto& source=kind==AssetKind::model?state.models:state.textures;
        if (kind==AssetKind::model) ++state.model_reads;else ++state.texture_reads;
        const auto found=source.find(id);
        if (found==source.end()) { error="missing asset: "+id;return false; }
        out=found->second;return true;
    }
    static bool select(void* raw,const std::string& model,std::uint32_t part,
        const std::string& authored,std::string& selected,std::string& error) {
        auto& state=*static_cast<Reader*>(raw);++state.resolutions;
        if (part||authored!="authored.tga") { error="unexpected fixture part";return false; }
        selected=model=="view-a"?"red.tga":model=="view-c"?"missing.tga":"green.tga";
        return true;
    }
    AssetReader callback() { return {this,read,select,nullptr}; }
};
void object(Object& out,std::uint32_t id,const char* model,float x) {
    out.entity=id;out.instance=id+100;out.database=7;
    out.model_identity=ModelIdentity::plain_database_name;
    out.placement_identity=PlacementIdentity::authored_placement;
    std::strcpy(out.model_name,model);
    for (int i=0;i<16;++i)out.world[i]=(i%5)==0?1.f:0.f;
    out.world[0]=out.world[5]=out.world[10]=.5f;out.world[3]=x;
}
std::unique_ptr<Frame> frame(const char* first,const char* second) {
    auto captured=std::make_unique<Frame>();captured->camera=1;captured->generation=1;
    captured->object_count=2;
    for (int i=0;i<16;++i)captured->view.world_to_clip[i]=(i%5)==0?1.f:0.f;
    captured->view.viewport[2]=captured->view.viewport[3]=128;
    object(captured->objects[0],1,first,-.5f);
    object(captured->objects[1],2,second,.5f);
    return captured;
}
ResourceUsage usage(Renderer& renderer) {
    ResourceUsage result{};ok(renderer.resource_usage(result),"resource usage");return result;
}
bool same_usage(const ResourceUsage& a,const ResourceUsage& b) {
    return a.epoch==b.epoch&&a.meshes==b.meshes&&a.textures==b.textures&&
        a.mesh_bytes==b.mesh_bytes&&a.texture_bytes==b.texture_bytes;
}
std::vector<std::uint8_t> image(Renderer& renderer) {
    std::vector<std::uint8_t> bytes;std::uint32_t width=0,height=0;
    ok(renderer.readback(bytes,width,height),"offscreen readback");
    need(width==128&&height==128&&bytes.size()==128*128*4,"readback dimensions");
    return bytes;
}
void colour(const std::vector<std::uint8_t>& bytes,unsigned x,unsigned y,
    int r,int g,int b,const char* label) {
    const auto* p=bytes.data()+(y*128+x)*4;
    need(std::abs(int(p[2])-r)<5&&std::abs(int(p[1])-g)<5&&
         std::abs(int(p[0])-b)<5,label);
}
void baseline(Renderer& renderer) {
    const auto empty=usage(renderer);
    need(empty.meshes==0&&empty.textures==1&&empty.texture_bytes==4,
         "clear releases all shared meshes and selected textures once");
}
void run() {
    Renderer renderer;ok(renderer.initialize_offscreen(128,128),"private offscreen D3D11 target");
    Reader reader;auto both=frame("view-a","view-b");
    const float ambient[3]{1,1,1};std::string error;
    StaticMaterialPolicy sharing{};sharing.share_geometry=true;
    {
        SceneSubmission scene(renderer,reader.callback(),sharing);
        ok(scene.render_preview(*both,1,nullptr,0,ambient,error),"shared geometry first frame");
        const auto shared=usage(renderer);
        need(shared.meshes==1&&shared.textures==3,"two coloured views share one GPU mesh");
        const auto first=image(renderer);
        colour(first,32,64,255,0,0,"left view selected red texture");
        colour(first,96,64,0,255,0,"right view selected green texture");
        const auto reads=reader.model_reads+reader.texture_reads;
        reader.forbid_reads=true;
        ok(scene.render_preview(*both,1,nullptr,0,ambient,error),"hot cached submission");
        need(reader.model_reads+reader.texture_reads==reads&&
             same_usage(shared,usage(renderer))&&image(renderer)==first,
             "hot submission rereads and uploads nothing");
        reader.forbid_reads=false;
        auto bad=frame("view-a","view-c");
        need(FAILED(scene.render_preview(*bad,1,nullptr,0,ambient,error))&&
             same_usage(shared,usage(renderer))&&image(renderer)==first,
             "missing second texture preserves first shared mesh and prior image");
        const auto failed_reads=reader.model_reads+reader.texture_reads;
        reader.textures.emplace("missing.tga",tga(0,255,0));
        need(FAILED(scene.render_preview(*bad,1,nullptr,0,ambient,error))&&
             reader.model_reads+reader.texture_reads==failed_reads,
             "failed model stays cached within source epoch");
        ok(scene.render_preview(*bad,2,nullptr,0,ambient,error),
           "new source epoch recovers missing texture");
        need(usage(renderer).meshes==1,"recovered views still share one mesh");
        colour(image(renderer),96,64,0,255,0,"recovered right green pixels");
        scene.clear();baseline(renderer);
        auto different=frame("view-a","view-d");
        ok(scene.render_preview(*different,3,nullptr,0,ambient,error),"changed vertex scene");
        need(usage(renderer).meshes==2,"one changed vertex prevents geometry sharing");
        scene.clear();baseline(renderer);
        auto transformed=frame("view-a","view-e");
        ok(scene.render_preview(*both,4,nullptr,0,ambient,error),"unshifted placement reference");
        const auto unshifted=image(renderer);
        ok(scene.render_preview(*transformed,4,nullptr,0,ambient,error),"different authored part matrix");
        const auto shifted=image(renderer);
        need(usage(renderer).meshes==1&&shifted!=unshifted,
             "authored matrix leaves mesh shared but changes independent placement");
        colour(shifted,32,64,255,0,0,"other view remains independently placed");
        scene.clear();baseline(renderer);
        const auto device_epoch=usage(renderer).epoch;
        renderer.shutdown();ok(renderer.initialize_offscreen(128,128),"device recreation");
        ok(scene.render_preview(*both,4,nullptr,0,ambient,error),"cache reload after device epoch");
        const auto recreated=usage(renderer);
        need(recreated.epoch>device_epoch&&recreated.meshes==1&&recreated.textures==3,
             "device epoch refresh retains shared geometry policy");
        scene.clear();baseline(renderer);
    }
    {
        SceneSubmission legacy(renderer,reader.callback());
        ok(legacy.render_preview(*both,5,nullptr,0,ambient,error),"default policy frame");
        need(usage(renderer).meshes==2,"default policy retains separate meshes");
        colour(image(renderer),32,64,255,0,0,"default left red pixels");
        colour(image(renderer),96,64,0,255,0,"default right green pixels");
        legacy.clear();baseline(renderer);
    }
    need(!GetModuleHandleA("d3d8.dll")&&!GetModuleHandleA("d3d9.dll")&&
         !GetModuleHandleA("D3DX81ab.dll")&&!GetModuleHandleA("d3dx9_43.dll"),
         "no legacy graphics modules");
    renderer.shutdown();
    std::puts("PASS native11 opt-in shared geometry across coloured model views");
}
}
int main() { run(); }
