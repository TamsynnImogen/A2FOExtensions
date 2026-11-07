// Offline, synthetic three-object capture for pristine static SOD content.
// The fixture transforms and camera are not measurements of game state.
#include "../renderer/native11/content.hpp"
#include "../renderer/native11/submission.hpp"
#include "../renderer/native11/asset_snapshot.hpp"
#include "../renderer/shadows/math.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs=std::filesystem;
namespace n=a2fo::native11;
namespace m=a2fo::shadows;
namespace {
constexpr std::uint32_t width=960,height=640;
void require(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
void checked(HRESULT hr,const char* what){if(FAILED(hr)){
    char message[160];std::snprintf(message,sizeof(message),"%s HRESULT=0x%08lx",what,static_cast<unsigned long>(hr));
    throw std::runtime_error(message);
}}
std::string lower(std::string value){for(char& c:value)if(c>='A'&&c<='Z')c+=32;return value;}
std::vector<std::uint8_t> read_file(const fs::path& path,std::uint64_t limit){
    const auto length=fs::file_size(path);require(length>0&&length<=limit,"asset length outside budget: "+path.string());
    std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    std::ifstream file(path,std::ios::binary);
    require(bool(file.read(reinterpret_cast<char*>(data.data()),data.size())),"asset read failed: "+path.string());
    return data;
}
struct FilesystemAssetReader {
    std::map<std::string,fs::path> models,textures;
    static bool read(void* context,n::AssetKind kind,const std::string& id,
                     std::vector<std::uint8_t>& bytes,std::string& error){
        try{
            auto& self=*static_cast<FilesystemAssetReader*>(context);
            if(kind==n::AssetKind::model){
                const auto it=self.models.find(id);
                if(it==self.models.end()){error="unmapped model: "+id;return false;}
                bytes=read_file(it->second,64ull*1024*1024);return true;
            }
            const auto name=lower(fs::path(id).filename().string());
            if(name.empty()||name=="."||name==".."||id.find("..")!=std::string::npos){error="invalid texture identifier";return false;}
            auto it=self.textures.find(name);
            if(it==self.textures.end())for(const char* suffix:{".tga",".dds"}){
                it=self.textures.find(name+suffix);if(it!=self.textures.end())break;
            }
            if(it==self.textures.end()){error="missing texture: "+id;return false;}
            bytes=read_file(it->second,256ull*1024*1024);return true;
        }catch(const std::exception& e){error=e.what();return false;}
    }
    void add_root(const fs::path& root){
        require(fs::is_directory(root),"texture root unavailable: "+root.string());
        fs::recursive_directory_iterator it(root,fs::directory_options::skip_permission_denied),end;
        for(;it!=end;++it){
            if(it->is_symlink()){if(it->is_directory())it.disable_recursion_pending();continue;}
            if(!it->is_regular_file())continue;
            const auto extension=lower(it->path().extension().string());
            if(extension==".tga"||extension==".dds")textures.emplace(lower(it->path().filename().string()),it->path());
        }
    }
};
std::uint64_t fixture_serial(void* context,std::uint32_t database,const char* name){
    n::ModelAssetInfo info{};
    return static_cast<n::ModelAssetStore*>(context)->info(database,info)&&
        std::strcmp(info.name,name)==0 ? info.serial : 0;
}
std::shared_ptr<const n::ModelAsset> fixture_model(void* context,std::uint32_t database,std::uint64_t serial){
    return static_cast<n::ModelAssetStore*>(context)->find(database,serial);
}
struct SnapshotAssetReader {
    const n::BoundCapture* bound=nullptr;
    static std::int32_t A2FO_N11_CALL sink(void* context,const std::uint8_t* data,std::uint32_t length){
        try{auto& out=*static_cast<std::vector<std::uint8_t>*>(context);out.assign(data,data+length);return 1;}
        catch(...){return 0;}
    }
    static bool read(void* context,n::AssetKind kind,const std::string& id,
                     std::vector<std::uint8_t>& bytes,std::string& error){
        auto& self=*static_cast<SnapshotAssetReader*>(context);
        const auto result=n::BoundCapture::read_asset(const_cast<n::BoundCapture*>(self.bound),
            kind==n::AssetKind::model?1u:2u,reinterpret_cast<const std::uint8_t*>(id.data()),
            static_cast<std::uint32_t>(id.size()),kind==n::AssetKind::model?64u*1024*1024:256u*1024*1024,
            sink,&bytes);
        if(result==1)return true;
        error="snapshot asset unavailable: "+id;return false;
    }
};
m::Vec3 sub(m::Vec3 a,m::Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
m::Vec3 cross(m::Vec3 a,m::Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float dot(m::Vec3 a,m::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
m::Vec3 unit(m::Vec3 a){float length=std::sqrt(dot(a,a));require(length>0&&std::isfinite(length),"invalid camera vector");return {a.x/length,a.y/length,a.z/length};}
m::Bounds model_bounds(const n::ModelContent& model){
    m::Bounds bounds{};
    for(const auto& part:model.parts){m::Mat4 transform{};
        std::copy(part.local_to_model,part.local_to_model+16,transform.v.begin());
        for(const auto& vertex:part.vertices){m::Vec3 point{};
            require(m::transform(transform,{vertex.position[0],vertex.position[1],vertex.position[2]},point),"invalid model transform");
            m::include(bounds,point);
        }
    }
    require(bounds.valid,"model has no bounds");return bounds;
}
struct Placement {float scale; m::Vec3 centre,offset;};
Placement placement(m::Bounds box,m::Vec3 offset){
    const m::Vec3 centre{(box.low.x+box.high.x)*.5f,(box.low.y+box.high.y)*.5f,(box.low.z+box.high.z)*.5f};
    const float radius=std::max({box.high.x-box.low.x,box.high.y-box.low.y,box.high.z-box.low.z})*.5f;
    require(radius>0&&std::isfinite(radius),"model has degenerate bounds");
    return {80.f/radius,centre,offset};
}
m::Bounds world_bounds(m::Bounds original,const Placement& p){
    m::Bounds result{};
    for(float x:{original.low.x,original.high.x})for(float y:{original.low.y,original.high.y})
        for(float z:{original.low.z,original.high.z})m::include(result,{(x-p.centre.x)*p.scale+p.offset.x,
            (y-p.centre.y)*p.scale+p.offset.y,(z-p.centre.z)*p.scale+p.offset.z});
    return result;
}
void merge(m::Bounds& into,const m::Bounds& from){m::include(into,from.low);m::include(into,from.high);}
m::Mat4 camera(m::Bounds box){
    const m::Vec3 centre{(box.low.x+box.high.x)*.5f,(box.low.y+box.high.y)*.5f,(box.low.z+box.high.z)*.5f};
    const float radius=std::max({box.high.x-box.low.x,box.high.y-box.low.y,box.high.z-box.low.z,1.f})*.5f;
    const m::Vec3 eye{centre.x+1.9f*radius,centre.y+1.45f*radius,centre.z-2.4f*radius};
    const auto forward=unit(sub(centre,eye)),right=unit(cross({0,1,0},forward)),up=cross(forward,right);
    const m::Mat4 view{{right.x,right.y,right.z,-dot(right,eye),up.x,up.y,up.z,-dot(up,eye),
        forward.x,forward.y,forward.z,-dot(forward,eye),0,0,0,1}};
    const float near_clip=radius*.01f,far_clip=radius*12.f,y=1.f/std::tan(.75f*.5f),z=far_clip/(far_clip-near_clip);
    const m::Mat4 projection{{y/(float(width)/height),0,0,0,0,y,0,0,0,0,z,-near_clip*z,0,0,1,0}};
    return m::multiply(projection,view);
}
std::uint32_t address(const void* value){return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(value));}
bool own_read(void*,std::uint32_t source,void* output,std::size_t length){
    SIZE_T copied=0;
    return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(static_cast<std::uintptr_t>(source)),
        output,length,&copied)!=0&&copied==length;
}
struct FakeObject {
    std::array<std::uint8_t,8> entity{};
    std::array<std::uint8_t,0x80+4> instance{};
    std::array<std::uint8_t,0x38+4> database{};
    std::string name;
    FakeObject(const std::string& model,const Placement& p):name(model){
        const auto instance_address=address(instance.data()),database_address=address(database.data()),name_address=address(name.c_str());
        const std::uint32_t vtable=0x1234;
        std::memcpy(entity.data()+4,&instance_address,4);
        std::memcpy(instance.data()+0x80,&database_address,4);
        std::memcpy(database.data(),&vtable,4);
        std::memcpy(database.data()+0x38,&name_address,4);
        const float s=p.scale;
        const float matrix[12]{s,0,0,0,s,0,0,0,s,
            p.offset.x-s*p.centre.x,p.offset.y-s*p.centre.y,p.offset.z-s*p.centre.z};
        std::memcpy(instance.data()+0x44,matrix,sizeof(matrix));
    }
};
struct FakeCamera {std::array<std::uint8_t,0x1b0+16> data{};
    explicit FakeCamera(const m::Mat4& vp){
        float screen[16]{};
        for(int col=0;col<4;++col){screen[col*4]=.5f*(vp.v[col]+vp.v[12+col]);
            screen[col*4+1]=.5f*(vp.v[12+col]-vp.v[4+col]);
            screen[col*4+2]=vp.v[8+col];screen[col*4+3]=vp.v[12+col];}
        std::memcpy(data.data()+0x160,screen,sizeof(screen));
        const float viewport[4]{0,0,float(width),float(height)};
        std::memcpy(data.data()+0x1b0,viewport,sizeof(viewport));
    }
};
void save_tga(const fs::path& path,const std::vector<std::uint8_t>& pixels,std::uint32_t w,std::uint32_t h){
    require(w<=65535&&h<=65535&&pixels.size()==std::size_t(w)*h*4,"readback dimensions");
    std::uint8_t header[18]{};header[2]=2;header[12]=w&255;header[13]=w>>8;
    header[14]=h&255;header[15]=h>>8;header[16]=32;header[17]=0x28;
    std::ofstream file(path,std::ios::binary|std::ios::trunc);require(bool(file),"create TGA output");
    file.write(reinterpret_cast<const char*>(header),sizeof(header));
    file.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());require(bool(file),"write TGA output");
}
void run(int argc,char** argv){
    require(argc>=5,"usage: native11_scene_preview.exe OUTPUT_TGA MODEL_A_SOD MODEL_B_SOD TEXTURE_ROOT [PARENT_TEXTURE_ROOT...] [--shadows] [--relight] [--snapshot]");
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    FilesystemAssetReader files;
    const std::string name_a=fs::path(argv[2]).filename().string(),name_b=fs::path(argv[3]).filename().string();
    require(!name_a.empty()&&!name_b.empty()&&name_a!=name_b&&name_a.size()<260&&name_b.size()<260,
        "model filenames must be distinct, nonempty, and fit capture names");
    files.models.emplace(name_a,fs::path(argv[2]));files.models.emplace(name_b,fs::path(argv[3]));
    bool shadows=false,relight=false,snapshot=false;
    for(int i=4;i<argc;++i){if(std::string(argv[i])=="--shadows")shadows=true;
        else if(std::string(argv[i])=="--relight")relight=true;
        else if(std::string(argv[i])=="--snapshot")snapshot=true;else files.add_root(argv[i]);}
    n::AssetReader reader{&files,FilesystemAssetReader::read};
    n::ModelContent a,b;std::string error;
    require(n::load_model_content(reader,name_a,a,error),"model A content: "+error);
    require(n::load_model_content(reader,name_b,b,error),"model B content: "+error);
    auto describe=[](const char* label,const n::ModelContent& model){
        for(std::size_t i=0;i<std::min<std::size_t>(model.parts.size(),8);++i){const auto& p=model.parts[i];
            std::printf("Material %s part=%zu diffuse=%.4g,%.4g,%.4g emission=%.4g,%.4g,%.4g flags=%u\n",
                label,i,p.colour[0],p.colour[1],p.colour[2],p.emissive[0],p.emissive[1],p.emissive[2],p.flags);
        }
    };
    describe("A",a);describe("B",b);
    const auto bounds_a=model_bounds(a),bounds_b=model_bounds(b);
    const Placement pa=placement(bounds_a,{-130,0,30}),pb=placement(bounds_b,{130,0,30}),pc=placement(bounds_a,{0,0,210});
    m::Bounds combined{};merge(combined,world_bounds(bounds_a,pa));merge(combined,world_bounds(bounds_b,pb));merge(combined,world_bounds(bounds_a,pc));
    FakeCamera camera_fixture(camera(combined));
    std::array<std::unique_ptr<FakeObject>,3> objects{
        std::make_unique<FakeObject>(name_a,pa),std::make_unique<FakeObject>(name_b,pb),std::make_unique<FakeObject>(name_a,pc)};
    n::ModelAssetStore model_store;
    if(snapshot){
        // The third fixture placement deliberately shares the first database.
        const auto first_database=address(objects[0]->database.data());
        std::memcpy(objects[2]->instance.data()+0x80,&first_database,sizeof(first_database));
        std::vector<std::uint8_t> raw;
        require(FilesystemAssetReader::read(&files,n::AssetKind::model,name_a,raw,error),"snapshot model A: "+error);
        n::OpenedFile opened_a;opened_a.name=name_a;opened_a.bytes=std::move(raw);
        require(model_store.publish(first_database,0,std::move(opened_a)),"snapshot publish model A");
        const auto second_database=address(objects[1]->database.data());
        require(FilesystemAssetReader::read(&files,n::AssetKind::model,name_b,raw,error),"snapshot model B: "+error);
        n::OpenedFile opened_b;opened_b.name=name_b;opened_b.bytes=std::move(raw);
        require(model_store.publish(second_database,0,std::move(opened_b)),"snapshot publish model B");
    }
    n::Collector collector(nullptr,own_read);require(collector.configure_plain_database_vtable(0x1234),"configure collector");
    if(snapshot)require(collector.configure_model_serial_reader(&model_store,fixture_serial),"configure fixture serial reader");
    const auto camera_address=address(camera_fixture.data.data());
    require(collector.begin(camera_address,true),"begin collector");
    for(const auto& object:objects)collector.observe_post_frustum(address(object->entity.data()),camera_address);
    auto frame=std::make_unique<n::Frame>();require(collector.end(camera_address,*frame),"end collector");
    require(frame->object_count==3,"collector object count");
    for(std::size_t i=0;i<objects.size();++i)
        require(frame->objects[i].model_identity==n::ModelIdentity::plain_database_name&&
            frame->objects[i].model_name==objects[i]->name,"collector model identity mismatch");
    std::shared_ptr<const n::BoundCapture> bound;
    SnapshotAssetReader snapshot_reader;
    if(snapshot){
        std::map<std::string,std::shared_ptr<const std::vector<std::uint8_t>>> unique_textures;
        for(const n::ModelContent* content:{&a,&b})for(const auto& texture:content->textures){
            if(unique_textures.find(texture.identifier)!=unique_textures.end())continue;
            std::vector<std::uint8_t> raw;
            require(FilesystemAssetReader::read(&files,n::AssetKind::texture,texture.identifier,raw,error),
                "snapshot texture: "+texture.identifier+": "+error);
            unique_textures.emplace(texture.identifier,
                std::make_shared<const std::vector<std::uint8_t>>(std::move(raw)));
        }
        std::vector<n::SnapshotTexture> textures;
        for(const auto& entry:unique_textures)textures.push_back({entry.first,entry.second});
        require(n::build_bound_capture(*frame,1,fixture_model,&model_store,textures,bound,error),
            "snapshot bind: "+error);
        require(frame->objects[0].model_serial&&frame->objects[1].model_serial&&
            frame->objects[2].model_serial==frame->objects[0].model_serial,
            "fixture snapshot serials");
        model_store.clear();files.models.clear();files.textures.clear();
        for(auto& object:objects)object.reset();
        snapshot_reader.bound=bound.get();
        reader={&snapshot_reader,SnapshotAssetReader::read};
    }
    const n::Frame* render_frame=snapshot?&bound->frame():frame.get();
    n::Renderer renderer;checked(renderer.initialize_offscreen(width,height),"offscreen D3D11 initialize");
    n::SceneSubmission scene(renderer,reader,{relight,.02f});
    n::Light light{};light.direction[0]=-.5f;light.direction[1]=-.8f;light.direction[2]=.1f;light.intensity=1.f;
    const float ambient[3]{.18f,.18f,.18f};
    n::ShadowSettings shadow_settings{};shadow_settings.light_mask=shadows?1:0;
    shadow_settings.resolution=1024;shadow_settings.depth_bias=.0005f;
    checked(scene.render_preview(*render_frame,1,&light,1,ambient,error,shadow_settings),"scene submission");
    n::ResourceUsage first{},second{};checked(renderer.resource_usage(first),"resource usage first");
    checked(scene.render_preview(*render_frame,1,&light,1,ambient,error,shadow_settings),"scene submission repeat");
    checked(renderer.resource_usage(second),"resource usage repeat");
    require(first.epoch==second.epoch&&first.meshes==second.meshes&&first.textures==second.textures&&
        first.mesh_bytes==second.mesh_bytes&&first.texture_bytes==second.texture_bytes,"hot cache changed resources");
    require(second.meshes==a.parts.size()+b.parts.size(),"unexpected mesh upload count");
    require(second.textures>=1&&second.textures<=a.textures.size()+b.textures.size()+1,"unexpected texture upload count");
    std::vector<std::uint8_t> pixels;std::uint32_t w=0,h=0;checked(renderer.readback(pixels,w,h),"readback");
    std::size_t visible=0;for(std::size_t i=0;i+3<pixels.size();i+=4)
        if(pixels[i]>25||pixels[i+1]>25||pixels[i+2]>25)++visible;
    require(visible>1000,"scene has insufficient visible pixels");save_tga(argv[1],pixels,w,h);
    std::printf("PASS native11 scene: objects=%zu parts_A=%zu parts_B=%zu meshes=%u textures=%u mesh_bytes=%llu texture_bytes=%llu visible_pixels=%zu shadow_mask=%u relight=%u output=%s\n",
        frame->object_count,a.parts.size(),b.parts.size(),second.meshes,second.textures,
        static_cast<unsigned long long>(second.mesh_bytes),static_cast<unsigned long long>(second.texture_bytes),visible,shadow_settings.light_mask,relight?1u:0u,argv[1]);
    if(snapshot)std::puts("snapshot=1: explicit offline filesystem fixture, immutable bound assets; submission performed no filesystem lookups.");
    std::puts("Pristine static content and synthetic fixture transforms only; game scale and dynamic state unresolved.");
}
}
int main(int argc,char** argv){try{run(argc,argv);return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
