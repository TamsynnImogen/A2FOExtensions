// Static-model integration host. A hidden private HWND is used only for DXGI;
// there is no input synthesis, focus change, game launch or legacy renderer.
#include "../renderer/native11/renderer.hpp"
#include "../renderer/native11/assets.h"
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
#include <stdexcept>
#include <string>
#include <vector>

namespace fs=std::filesystem;
namespace n=a2fo::native11;
namespace m=a2fo::shadows;
namespace {
void require(bool value,const std::string& why){if(!value)throw std::runtime_error(why);}
void checked(HRESULT hr,const char* why){if(FAILED(hr)){
    char text[200];std::snprintf(text,sizeof(text),"%s HRESULT=0x%08lx",why,static_cast<unsigned long>(hr));
    throw std::runtime_error(text);
}}
std::string lower(std::string s){for(char& c:s)if(c>='A'&&c<='Z')c+=32;return s;}
std::vector<std::uint8_t> read(const fs::path& p,std::uint64_t budget=64ull*1024*1024){
    const auto length=fs::file_size(p);require(length>0&&length<=budget,"asset length outside budget");
    std::vector<std::uint8_t> data(static_cast<std::size_t>(length));
    std::ifstream f(p,std::ios::binary);require(bool(f.read(reinterpret_cast<char*>(data.data()),data.size())),"asset read failed");
    return data;
}
struct Part {std::vector<n::Vertex> vertices;std::vector<std::uint32_t> indices;n::Draw draw;std::string texture;};
struct Model {std::vector<Part> parts;std::string error;};
int receive_part(void* user,const A2FO_N11_Part* p){
    auto& model=*static_cast<Model*>(user);
    try{
        require(p&&p->vertices&&p->indices&&p->vertex_count&&p->index_count,"empty model part");
        require(p->vertex_count<=1048576&&p->index_count<=1048576&&model.parts.size()<8192,"model budget");
        require(p->texture_name_length<=1024,"texture name budget");
        static_assert(sizeof(n::Vertex)==sizeof(A2FO_N11_Vertex));
        Part out;out.vertices.resize(p->vertex_count);
        std::memcpy(out.vertices.data(),p->vertices,out.vertices.size()*sizeof(n::Vertex));
        out.indices.assign(p->indices,p->indices+p->index_count);
        std::copy(p->local_to_model,p->local_to_model+16,out.draw.local_to_world);
        std::copy(p->colour,p->colour+4,out.draw.colour);
        std::copy(p->emissive,p->emissive+3,out.draw.emissive);out.draw.flags=p->flags;
        if(p->texture_name_length){require(p->texture_name!=nullptr,"null texture name");
            out.texture.assign(reinterpret_cast<const char*>(p->texture_name),p->texture_name_length);
            require(out.texture.find('\0')==std::string::npos&&out.texture.find("..") == std::string::npos,"invalid texture name");}
        model.parts.push_back(std::move(out));return 1;
    }catch(const std::exception& e){model.error=e.what();return 0;}catch(...){model.error="part callback failed";return 0;}
}
struct ImageUpload {
    n::Renderer* renderer;std::uint32_t handle=0,format=0,mips=0;
    std::uint64_t bytes=0;HRESULT hr=E_FAIL;
};
int receive_texture(void* user,const A2FO_N11_Texture* texture){
    auto& out=*static_cast<ImageUpload*>(user);
    try{
        require(texture&&texture->mips&&texture->mip_count<=15&&texture->reserved==0,"texture callback layout");
        std::array<n::TextureMip,15> levels{};
        for(std::uint32_t i=0;i<texture->mip_count;++i){const auto& m=texture->mips[i];
            levels[i]={m.width,m.height,m.row_pitch,m.slice_pitch,m.offset,m.byte_count};}
        n::TextureView view{static_cast<n::TextureFormat>(texture->format),static_cast<n::TextureAlpha>(texture->alpha_mode),
            texture->width,texture->height,levels.data(),texture->mip_count,texture->payload,texture->payload_bytes};
        out.hr=out.renderer->upload_texture(view,out.handle);
        out.format=texture->format;out.mips=texture->mip_count;out.bytes=texture->payload_bytes;
        return SUCCEEDED(out.hr)?1:0;
    }catch(...){out.hr=E_FAIL;return 0;}
}
m::Vec3 subtract(m::Vec3 a,m::Vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
m::Vec3 cross(m::Vec3 a,m::Vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float dot(m::Vec3 a,m::Vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
m::Vec3 normal(m::Vec3 a){const float length=std::sqrt(dot(a,a));require(length>0&&std::isfinite(length),"camera direction invalid");return {a.x/length,a.y/length,a.z/length};}
m::Mat4 camera(const Model& model,float aspect){
    m::Bounds box{};
    for(const auto& part:model.parts){m::Mat4 world{};std::copy(part.draw.local_to_world,part.draw.local_to_world+16,world.v.begin());
        for(const auto& v:part.vertices){m::Vec3 p{};require(m::transform(world,{v.position[0],v.position[1],v.position[2]},p),"model transform invalid");m::include(box,p);}}
    require(box.valid,"model has no bounds");
    const m::Vec3 centre{(box.low.x+box.high.x)*.5f,(box.low.y+box.high.y)*.5f,(box.low.z+box.high.z)*.5f};
    const float radius=std::max({box.high.x-box.low.x,box.high.y-box.low.y,box.high.z-box.low.z,1.f})*.5f;
    const m::Vec3 eye{centre.x+1.9f*radius,centre.y+1.45f*radius,centre.z-2.4f*radius};
    const auto forward=normal(subtract(centre,eye));const auto right=normal(cross({0,1,0},forward));const auto up=cross(forward,right);
    const m::Mat4 view{{right.x,right.y,right.z,-dot(right,eye),up.x,up.y,up.z,-dot(up,eye),
                       forward.x,forward.y,forward.z,-dot(forward,eye),0,0,0,1}};
    const float clip_near=radius*.01f,clip_far=radius*12.f,y=1.f/std::tan(.75f*.5f),z=clip_far/(clip_far-clip_near);
    const m::Mat4 projection{{y/aspect,0,0,0,0,y,0,0,0,0,z,-clip_near*z,0,0,1,0}};
    return m::multiply(projection,view);
}
void save_tga(const fs::path& path,const std::vector<std::uint8_t>& pixels,std::uint32_t w,std::uint32_t h){
    require(w<=65535&&h<=65535&&pixels.size()==std::size_t(w)*h*4,"readback dimensions");
    std::uint8_t header[18]{};header[2]=2;header[12]=w&255;header[13]=w>>8;
    header[14]=h&255;header[15]=h>>8;header[16]=32;header[17]=0x28;
    std::ofstream f(path,std::ios::binary|std::ios::trunc);require(bool(f),"create TGA output");
    f.write(reinterpret_cast<const char*>(header),sizeof(header));
    f.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());require(bool(f),"write TGA output");
}
struct Window {HWND hwnd=nullptr;~Window(){if(hwnd)DestroyWindow(hwnd);}};
void run(int argc,char** argv){
    require(argc>=4,"usage: native11_preview.exe MODEL_SOD OUTPUT_TGA TEXTURE_ROOT [PARENT_TEXTURE_ROOT...]");
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    std::map<std::string,fs::path> textures;
    for(int i=3;i<argc;++i){require(fs::is_directory(argv[i]),"texture root unavailable");
        for(const auto& entry:fs::recursive_directory_iterator(argv[i],fs::directory_options::skip_permission_denied)){
            if(entry.is_symlink()||!entry.is_regular_file())continue;
            const auto ext=lower(entry.path().extension().string());if(ext!=".tga"&&ext!=".dds")continue;
            textures.emplace(lower(entry.path().filename().string()),entry.path());
        }}
    Model model;const auto file=read(argv[1]);
    require(a2fo_native11_parse_sod(file.data(),static_cast<std::uint32_t>(file.size()),receive_part,&model)!=0,
            "CPU SOD adapter rejected input: "+model.error);
    require(!model.parts.empty(),"static model contains no selected mesh parts");
    constexpr std::uint32_t width=960,height=640;
    Window window;window.hwnd=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,"STATIC","Native11 private model host",
        WS_POPUP,0,0,width,height,nullptr,nullptr,GetModuleHandleA(nullptr),nullptr);
    require(window.hwnd&&!IsWindowVisible(window.hwnd),"hidden private HWND");
    n::Renderer renderer;checked(renderer.initialize(window.hwnd,width,height),"native D3D11 initialize");
    std::map<std::string,std::uint32_t> loaded;std::vector<n::Draw> draws;std::uint64_t triangles=0;
    for(auto& part:model.parts){
        checked(renderer.upload_mesh({part.vertices.data(),static_cast<std::uint32_t>(part.vertices.size()),part.indices.data(),
            static_cast<std::uint32_t>(part.indices.size())},part.draw.mesh),"native mesh upload");
        triangles+=part.indices.size()/3;
        if(!part.texture.empty()){
            auto name=lower(fs::path(part.texture).filename().string());
            auto found=textures.find(name);
            if(found==textures.end())for(const char* extension:{".tga",".dds"}){
                const auto candidate=textures.find(name+extension);if(candidate!=textures.end()){found=candidate;break;}}
            require(found!=textures.end(),"diffuse texture missing: "+name);
            const auto key=found->first;auto cached=loaded.find(key);
            if(cached==loaded.end()){
                const auto bytes=read(found->second,256ull*1024*1024);ImageUpload upload{&renderer};
                require(a2fo_native11_parse_texture(bytes.data(),static_cast<std::uint32_t>(bytes.size()),receive_texture,&upload)!=0,
                    "image decode/upload failed: "+key);checked(upload.hr,"native texture upload");
                loaded.emplace(key,upload.handle);part.draw.texture=upload.handle;
                std::printf("texture %s format=%u mips=%u bytes=%llu\n",found->second.string().c_str(),upload.format,upload.mips,static_cast<unsigned long long>(upload.bytes));
            }else part.draw.texture=cached->second;
        }
        draws.push_back(part.draw);
    }
    n::Light key;key.direction[0]=-.5f;key.direction[1]=-.8f;key.direction[2]=.1f;key.intensity=1.0f;
    n::FrameView frame;const auto vp=camera(model,float(width)/height);
    std::copy(vp.v.begin(),vp.v.end(),frame.world_to_clip);
    frame.ambient[0]=frame.ambient[1]=frame.ambient[2]=.18f;
    frame.lights=&key;frame.light_count=1;frame.draws=draws.data();frame.draw_count=static_cast<std::uint32_t>(draws.size());
    checked(renderer.render(frame,false),"direct native scene render");
    std::vector<std::uint8_t> pixels;std::uint32_t w=0,h=0;checked(renderer.readback(pixels,w,h),"native scene readback");
    std::size_t foreground=0;for(std::size_t i=0;i+3<pixels.size();i+=4)if(pixels[i]>25||pixels[i+1]>25||pixels[i+2]>25)++foreground;
    require(foreground>100,"native scene did not draw a visible model");save_tga(argv[2],pixels,w,h);
    std::printf("PASS native11 static model: parts=%zu textures=%zu triangles=%llu visible_pixels=%zu output=%s\n",
        draws.size(),loaded.size(),static_cast<unsigned long long>(triangles),foreground,argv[2]);
    std::puts("Direct D3D11 scene input; authored base pose/highest LOD/diffuse preview. Animation, sprites and bump channels omitted.");
}
}
int main(int argc,char** argv){try{run(argc,argv);return 0;}catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}}
