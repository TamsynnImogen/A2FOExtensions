// Include the actual backend once in this private fixture so failure injection
// can reach its device without adding a production diagnostic/device API.
#include "../renderer/native11/renderer.cpp"
#include "../renderer/native11/submission.hpp"
#include <windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <vector>
using namespace a2fo::native11;
extern "C" unsigned native11_sdk_device_slots();
extern "C" unsigned native11_sdk_depth_view_slot();
extern "C" unsigned native11_sdk_texture_slot();
namespace a2fo::native11 {
struct RendererFixtureAccess {static ID3D11Device* device(Renderer& r){return r.p_->device;}};
}
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
void ok(HRESULT hr,const char* why){if(FAILED(hr)){std::fprintf(stderr,"FAIL %s hr=%08lx\n",why,static_cast<unsigned long>(hr));std::exit(1);}}
std::vector<std::uint8_t> model(std::array<float,3> colour,const char* texture="hull.dds",const char* second=nullptr,
                                bool fullbright=false,const char* shader="default"){
    std::vector<std::uint8_t> b;
    auto word=[&](std::uint16_t v){b.push_back(v&255);b.push_back(v>>8);};
    auto dword=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)b.push_back((v>>(i*8))&255);};
    auto number=[&](float value){std::uint32_t v;std::memcpy(&v,&value,4);dword(v);};
    auto text=[&](const char* p){word(std::strlen(p));while(*p)b.push_back(*p++);};
    const char magic[]="Storm3D_SW";b.insert(b.end(),magic,magic+10);number(1.93f);word(1);text("mat");
    for(float v:{0.f,0.f,0.f,colour[0],colour[1],colour[2],0.f,0.f,0.f,1.f})number(v);
    b.push_back(2);b.push_back(fullbright?1:0);word(second?2:1);
    for(unsigned i=0;i<(second?2u:1u);++i){
        word(1);text(i?"mesh2":"mesh1");text("");
        for(float v:{1.f,0.f,0.f,0.f,1.f,0.f,0.f,0.f,1.f,.2f,0.f,0.f})number(v);
        text(shader);dword(4);dword(1);text(i?second:texture);dword(0);text("");word(0);
        word(3);word(3);word(1);
        for(float v:{0.f,0.f,.2f,.5f,0.f,.2f,0.f,.3f,.2f})number(v);
        for(unsigned j=0;j<6;++j)number(0);
        word(1);text("mat");for(unsigned j=0;j<3;++j){word(j);word(j);}
        b.push_back(0);word(0);
    }
    word(0);word(0);return b;
}
std::vector<std::uint8_t> texture(unsigned colour,unsigned alpha=0){
    std::vector<std::uint8_t> b(156);
    auto put=[&](unsigned offset,std::uint32_t value){for(unsigned i=0;i<4;++i)b[offset+i]=(value>>(i*8))&255;};
    std::memcpy(b.data(),"DDS ",4);put(4,124);put(8,0x1007);put(12,4);put(16,4);put(28,1);put(76,32);put(80,4);
    std::memcpy(b.data()+84,"DX10",4);put(108,0x1000);put(128,71);put(132,3);put(140,1);put(144,alpha);
    b[148]=colour&255;b[149]=colour>>8;return b;
}
struct Reader {
    std::map<std::string,std::vector<std::uint8_t>> models,textures;
    unsigned model_reads=0,texture_reads=0;bool block=false,reenter=false;
    SceneSubmission* submission=nullptr;Frame* frame=nullptr;bool reentry_rejected=false;
};
bool read(void* raw,AssetKind kind,const std::string& id,std::vector<std::uint8_t>& bytes,std::string& error){
    auto& r=*static_cast<Reader*>(raw);
    if(r.block){error="unexpected read while cache should be hot";return false;}
    if(kind==AssetKind::model){
        ++r.model_reads;
        if(r.reenter){r.reenter=false;const float ambient[3]{1,1,1};std::string nested;
            r.reentry_rejected=r.submission->render_preview(*r.frame,2,nullptr,0,ambient,nested)==E_UNEXPECTED;}
    }else ++r.texture_reads;
    const auto& source=kind==AssetKind::model?r.models:r.textures;const auto found=source.find(id);
    if(found==source.end()){error="missing authoritative identifier: "+id;return false;}
    bytes=found->second;return true;
}
struct Window {HWND h=nullptr;~Window(){if(h)DestroyWindow(h);}};
unsigned depth_view_failures=0;
HRESULT STDMETHODCALLTYPE fail_depth_view(ID3D11Device*,ID3D11Resource*,const D3D11_DEPTH_STENCIL_VIEW_DESC*,ID3D11DepthStencilView** out){
    ++depth_view_failures;if(out)*out=nullptr;return E_OUTOFMEMORY;
}
struct FailDepthView {
    void** slot;void* original;
    static void write(void** slot,void* value){
        DWORD previous=0;need(VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&previous)!=0,"private device slot writable");
        InterlockedExchangePointer(reinterpret_cast<void* volatile*>(slot),value);
        DWORD ignored=0;need(VirtualProtect(slot,sizeof(void*),previous,&ignored)!=0,"private device slot protection restored");
    }
    explicit FailDepthView(Renderer& renderer){
        need(native11_sdk_device_slots()==43&&native11_sdk_depth_view_slot()==10,"SDK-derived device vtable layout");
        auto* device=RendererFixtureAccess::device(renderer);
        slot=*reinterpret_cast<void***>(device)+native11_sdk_depth_view_slot();original=*slot;
        const auto failure=&fail_depth_view;void* replacement=nullptr;
        static_assert(sizeof(failure)==sizeof(replacement));std::memcpy(&replacement,&failure,sizeof(failure));
        // Patch one slot in this private process only, retaining any extended
        // device interface methods that the driver may query during resize.
        write(slot,replacement);
    }
    ~FailDepthView(){write(slot,original);}
};
HRESULT STDMETHODCALLTYPE fail_texture(ID3D11Device*,const D3D11_TEXTURE2D_DESC*,const D3D11_SUBRESOURCE_DATA*,ID3D11Texture2D** output){
    if(output)*output=nullptr;
    return E_OUTOFMEMORY;
}
struct FailTexture {
    void** slot;void* original;
    explicit FailTexture(Renderer& renderer){
        need(native11_sdk_device_slots()==43&&native11_sdk_texture_slot()==5,"SDK-derived texture allocation slot");
        slot=*reinterpret_cast<void***>(RendererFixtureAccess::device(renderer))+native11_sdk_texture_slot();original=*slot;
        const auto failure=&fail_texture;void* replacement=nullptr;std::memcpy(&replacement,&failure,sizeof(failure));FailDepthView::write(slot,replacement);
    }
    ~FailTexture(){FailDepthView::write(slot,original);}
};
struct Pixel {int r,g,b;};
Pixel pixel(Renderer& renderer,unsigned x,unsigned y){
    std::vector<std::uint8_t> data;std::uint32_t w=0,h=0;ok(renderer.readback(data,w,h),"scene readback");
    need(x<w&&y<h,"scene pixel bounds");const auto* p=data.data()+(y*w+x)*4;return{p[2],p[1],p[0]};
}
void colour(Pixel p,int r,int g,int b,const char* why){need(std::abs(p.r-r)<4&&std::abs(p.g-g)<4&&std::abs(p.b-b)<4,why);}
bool same(const ResourceUsage& a,const ResourceUsage& b){return a.epoch==b.epoch&&a.mesh_bytes==b.mesh_bytes&&a.texture_bytes==b.texture_bytes&&a.meshes==b.meshes&&a.textures==b.textures;}
void object(Object& out,const char* model_name,float x){
    out.entity=1;out.instance=2;out.database=3;out.model_identity=ModelIdentity::plain_database_name;
    std::strcpy(out.model_name,model_name);std::fill(out.world,out.world+16,0.f);
    out.world[0]=.5f;out.world[5]=out.world[10]=out.world[15]=1;out.world[3]=x;out.world[7]=.2f;
}
void run(){
    Window window;window.h=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,"STATIC","private native11 scene submission",WS_POPUP,
        0,0,32,32,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);need(window.h&&!IsWindowVisible(window.h),"private hidden scene HWND");
    Renderer renderer;ok(renderer.initialize(window.h,32,32),"scene device");
    Reader reader;reader.models["actual-a.sod"]=model({1,0,0});reader.models["actual-b.sod"]=model({0,1,0});
    reader.textures["hull.dds"]=texture(0xffff);
    SceneSubmission scene(renderer,{&reader,read});auto frame=std::make_unique<Frame>();
    frame->camera=1;frame->generation=1;frame->object_count=2;
    const float screen[16]{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};const float rect[4]{0,0,32,32};
    need(convert_engine_cpu_camera(screen,rect,frame->view),"owned CPU camera conversion");
    object(frame->objects[0],"actual-a.sod",.05f);object(frame->objects[1],"actual-b.sod",.55f);
    const float ambient[3]{1,1,1};std::string error;
    ok(scene.render_preview(*frame,1,nullptr,0,ambient,error),"first captured scene submission");
    colour(pixel(renderer,6,8),255,0,0,"object-times-node world transform red instance");
    colour(pixel(renderer,22,8),0,255,0,"independent second instance placement");
    const auto reads_before_invalid_policy=reader.model_reads+reader.texture_reads;
    for(float fraction:{std::numeric_limits<float>::quiet_NaN(),-.1f,1.1f}){
        SceneSubmission invalid_policy(renderer,{&reader,read},{true,fraction});
        need(invalid_policy.render_preview(*frame,1,nullptr,0,ambient,error)==E_INVALIDARG,
             "invalid retained emission rejected");
        need(reader.model_reads+reader.texture_reads==reads_before_invalid_policy,
             "invalid retained emission rejected before asset reads");
        colour(pixel(renderer,6,8),255,0,0,"invalid material policy preserves target");
    }
    ResourceUsage usage{};ok(renderer.resource_usage(usage),"scene resident usage");
    need(usage.meshes==2&&usage.textures==2&&usage.texture_bytes==12,"models share one encoded texture plus white");
    need(reader.model_reads==2&&reader.texture_reads==2,"provider resolves exact model/texture identifiers");
    {
        FailDepthView fail(renderer);
        need(renderer.resize(48,48)==E_OUTOFMEMORY&&depth_view_failures==1,"actual resize target creation failure");
        need(renderer.render(FrameView{},false)==E_UNEXPECTED,"failed targets cannot draw");
        scene.clear();ResourceUsage empty{};ok(renderer.resource_usage(empty),"residency remains available without targets");
        need(empty.meshes==0&&empty.textures==1&&empty.texture_bytes==4,"clear releases cached GPU resources after resize failure");
        const auto reads=reader.model_reads+reader.texture_reads;
        need(scene.render_preview(*frame,1,nullptr,0,ambient,error)==E_UNEXPECTED,"missing targets refuse scene submission");
        need(reader.model_reads+reader.texture_reads==reads,"unavailable targets cannot read/cache assets");
    }
    ok(renderer.resize(32,32),"target recreation retry");
    ok(scene.render_preview(*frame,1,nullptr,0,ambient,error),"scene reload after failed resize cleanup");
    ResourceUsage recovered{};ok(renderer.resource_usage(recovered),"recovered resource usage");
    need(same(usage,recovered),"resize failure cleanup did not leak resident budget");
    scene.clear();
    {FailTexture fail(renderer);need(scene.render_preview(*frame,1,nullptr,0,ambient,error)==E_OUTOFMEMORY,"actual transient GPU texture allocation failure");}
    ok(scene.render_preview(*frame,1,nullptr,0,ambient,error),"transient GPU allocation retries in the same source/device epoch");
    ResourceUsage retried{};ok(renderer.resource_usage(retried),"usage after transient allocation retry");
    need(same(usage,retried),"transient allocation rollback and retry preserve resident budget");
    reader.block=true;ok(scene.render_preview(*frame,1,nullptr,0,ambient,error),"hot cache does not reread provider");reader.block=false;
    auto bad=std::make_unique<Frame>(*frame);bad->objects[1].model_identity=ModelIdentity::unresolved;
    need(scene.render_preview(*bad,1,nullptr,0,ambient,error)==E_INVALIDARG,"incomplete scene rejected before presentation");
    colour(pixel(renderer,22,8),0,255,0,"incomplete scene preserves previous target");
    reader.models["late-fail.sod"]=model({1,1,1},"tx-good.dds","tx-custom.dds");
    reader.textures["tx-good.dds"]=texture(0xf800);reader.textures["tx-custom.dds"]=texture(0xffff,4);
    bad->object_count=1;object(bad->objects[0],"late-fail.sod",.05f);
    need(scene.render_preview(*bad,1,nullptr,0,ambient,error)==E_NOTIMPL,"later unsupported GPU texture fails whole model");
    ResourceUsage after{};ok(renderer.resource_usage(after),"rolled back usage");
    need(same(usage,after),"failed model upload releases all newly created resources");
    colour(pixel(renderer,22,8),0,255,0,"failed model cannot publish a partial frame");
    const auto reads=reader.model_reads+reader.texture_reads;
    need(scene.render_preview(*bad,1,nullptr,0,ambient,error)==E_NOTIMPL&&reader.model_reads+reader.texture_reads==reads,"failed asset cached within explicit source epoch");
    reader.models["actual-c.sod"]=model({1,1,1});reader.textures["hull.dds"]=texture(0x001f);
    object(bad->objects[0],"actual-c.sod",.05f);
    need(scene.render_preview(*bad,1,nullptr,0,ambient,error)==E_INVALIDARG,"changed shared texture cannot silently mix inside epoch");
    reader.models["actual-a.sod"]=model({1,1,1});frame->object_count=1;
    ok(scene.render_preview(*frame,2,nullptr,0,ambient,error),"new source epoch refreshes models and textures");
    colour(pixel(renderer,6,8),0,0,255,"new authoritative content replaces cached appearance");
    need(pixel(renderer,22,8).g<8,"prior instance removed in new complete frame");
    ok(renderer.resource_usage(after),"refreshed usage");need(after.meshes==1&&after.textures==2,"source epoch releases old cache residency");
    const auto epoch=after.epoch;renderer.shutdown();ok(renderer.initialize(window.h,32,32),"recreated native scene device");
    ok(scene.render_preview(*frame,2,nullptr,0,ambient,error),"device epoch reloads assets without stale handles");
    colour(pixel(renderer,6,8),0,0,255,"device recovery reload appearance");
    ok(renderer.resource_usage(after),"recreated usage");need(after.epoch==epoch+1&&after.meshes==1&&after.textures==2,"recreated scene cache bound to new device");
    scene.clear();ok(renderer.resource_usage(after),"cleared usage");need(after.meshes==0&&after.textures==1&&after.texture_bytes==4,"clear returns model and texture budgets");
    Reader material_reader;material_reader.textures["hull.dds"]=texture(0xffff);
    material_reader.models["fullbright.sod"]=model({1,0,0},"hull.dds",nullptr,true);
    material_reader.models["fullbright-alpha.sod"]=model({1,0,0},"hull.dds",nullptr,true,"alpha");
    material_reader.models["fullbright-additive.sod"]=model({1,0,0},"hull.dds",nullptr,true,"additive");
    auto material_frame=std::make_unique<Frame>(*frame);material_frame->object_count=1;
    object(material_frame->objects[0],"fullbright.sod",.05f);
    const float dark[3]{0,0,0};
    {
        SceneSubmission default_materials(renderer,{&material_reader,read});
        ok(default_materials.render_preview(*material_frame,3,nullptr,0,dark,error),"default fullbright material");
        colour(pixel(renderer,6,8),255,0,0,"default policy preserves fullbright red");
        default_materials.clear();
        SceneSubmission relit_materials(renderer,{&material_reader,read},{true,.02f});
        ok(relit_materials.render_preview(*material_frame,3,nullptr,0,dark,error),"relit fullbright material");
        const Pixel retained=pixel(renderer,6,8);
        need(retained.r>=3&&retained.r<=8&&retained.g<4&&retained.b<4,
             "relit fullbright retains configured emission");
        Light top_light{};top_light.direction[1]=0.f;top_light.direction[2]=1.f;top_light.intensity=1.f;
        ok(relit_materials.render_preview(*material_frame,3,&top_light,1,dark,error),"directionally lit fullbright material");
        colour(pixel(renderer,6,8),255,0,0,"relit fullbright receives directional light");
        for(const char* name:{"fullbright-alpha.sod","fullbright-additive.sod"}){
            object(material_frame->objects[0],name,.05f);
            ok(default_materials.render_preview(*material_frame,3,nullptr,0,dark,error),"authored blended fullbright reference");
            const auto authored=pixel(renderer,6,8);need(authored.r>=250,"authored blended material is visibly emissive");
            ok(relit_materials.render_preview(*material_frame,3,nullptr,0,dark,error),"blended fullbright material");
            colour(pixel(renderer,6,8),authored.r,authored.g,authored.b,"blended fullbright preserves authored glow and background blend");
        }
    }
    reader.submission=&scene;reader.frame=frame.get();reader.reenter=true;
    ok(scene.render_preview(*frame,2,nullptr,0,ambient,error),"outer render survives reentrant provider attempt");
    need(reader.reentry_rejected,"reentrant submission rejected without corrupting cache");
    need(!GetModuleHandleA("d3d8.dll")&&!GetModuleHandleA("d3d9.dll")&&!GetModuleHandleA("D3DX81ab.dll")&&!GetModuleHandleA("d3dx9_43.dll"),"no legacy renderer dependencies");
    std::puts("PASS native11 owned scene -> exact asset IDs -> shared GPU cache -> native target; transforms, epoch recovery, rollback, incomplete-frame and reentry guards");
}
}
int main(){run();}
