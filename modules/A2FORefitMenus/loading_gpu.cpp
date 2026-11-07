#include "loading_gpu.hpp"
#include "../../rust/refit_native_ffi.h"
#include <array>
#include <utility>

namespace a2fo::refit_menu::loading_native {
namespace {
struct Vertex { float x,y,z,rhw; DWORD colour; float u,v; };
static_assert(sizeof(Vertex)==sizeof(A2FORefitGpuVertex));
static_assert(alignof(Vertex)==alignof(A2FORefitGpuVertex));

struct Slot {
    LoadingTextureCache* cache{};
    IDirect3DTexture8* transient{};
    IDirect3DTexture8** texture{};
    IDirect3DDevice8** device{};
    UINT* width{};UINT* height{};UINT* texture_width{};UINT* texture_height{};
    std::uint64_t* revision{};
    bool* uploaded{};bool* alpha_preserved{};
    LoadingTextureCache::Stats* stats{};
    IDirect3DTexture8* value() const {return cache?*texture:transient;}
};
struct Bridge {
    IDirect3DDevice8* device{};
    Slot slots[2]{};
    DWORD token{};D3DVIEWPORT8 prior_viewport{};bool captured{},layered{};
    explicit Bridge(IDirect3DDevice8* value):device(value){}
    ~Bridge(){
        restore();
        for(int i=1;i>=0;--i)if(slots[i].transient)slots[i].transient->Release();
    }
    void restore(){
        if(!captured)return;
        if(layered)for(int i=1;i>=0;--i)if(slots[i].transient){
            slots[i].transient->Release();slots[i].transient=nullptr;
        }
        device->ApplyStateBlock(token);device->SetViewport(&prior_viewport);
        device->DeleteStateBlock(token);captured=false;
    }
    static Bridge& from(void* context){return *static_cast<Bridge*>(context);}
    static int get_caps(void* context,A2FORefitGpuCaps* result){
        D3DCAPS8 caps{};
        if(FAILED(from(context).device->GetDeviceCaps(&caps)))return 0;
        *result={caps.TextureCaps,caps.MaxTextureWidth,caps.MaxTextureHeight};return 1;
    }
    static int cache_state(void* context,int index,A2FORefitGpuCacheState* result){
        const auto& s=from(context).slots[index];
        if(!s.cache){*result={};return 1;}
        *result={*s.texture!=nullptr,*s.device==from(context).device,*s.uploaded,*s.alpha_preserved,
                 *s.width,*s.height,*s.texture_width,*s.texture_height,*s.revision};
        return 1;
    }
    static void reset(void* context,int index){from(context).slots[index].cache->reset();}
    static int create(void* context,int index,uint32_t tw,uint32_t th,
                      uint32_t width,uint32_t height){
        auto& b=from(context);auto& s=b.slots[index];
        IDirect3DTexture8** destination=s.cache?s.texture:&s.transient;
        if(FAILED(b.device->CreateTexture(tw,th,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,destination)))return 0;
        if(s.cache){
            *s.device=b.device;*s.width=width;*s.height=height;
            *s.texture_width=tw;*s.texture_height=th;++s.stats->creations;
        }
        return 1;
    }
    static int lock(void* context,int index,uint8_t** bits,int32_t* pitch){
        D3DLOCKED_RECT locked{};
        if(FAILED(from(context).slots[index].value()->LockRect(0,&locked,nullptr,0)))return 0;
        *bits=static_cast<uint8_t*>(locked.pBits);*pitch=locked.Pitch;return 1;
    }
    static int unlock(void* context,int index){
        return SUCCEEDED(from(context).slots[index].value()->UnlockRect(0));
    }
    static void mark_upload(void* context,int index,uint64_t revision,int preserved,int success){
        auto& s=from(context).slots[index];if(!s.cache)return;
        *s.uploaded=success!=0;
        if(success){*s.revision=revision;*s.alpha_preserved=preserved!=0;++s.stats->uploads;}
    }
    static int capture(void* context){
        auto& b=from(context);
        b.captured=SUCCEEDED(b.device->GetViewport(&b.prior_viewport))&&
                   SUCCEEDED(b.device->CreateStateBlock(D3DSBT_ALL,&b.token));
        return b.captured;
    }
    static void restore_callback(void* context){from(context).restore();}
    static int viewport(void* context,uint32_t width,uint32_t height){
        const D3DVIEWPORT8 viewport{0,0,width,height,0,1};
        return SUCCEEDED(from(context).device->SetViewport(&viewport));
    }
    static int render_state(void* context,int index,int layered){
        using Entry=std::pair<D3DRENDERSTATETYPE,DWORD>;
        static constexpr std::array<Entry,12> plain{{
            {D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},
            {D3DRS_ALPHABLENDENABLE,FALSE},{D3DRS_ALPHATESTENABLE,FALSE},
            {D3DRS_FOGENABLE,FALSE},{D3DRS_STENCILENABLE,FALSE},
            {D3DRS_LIGHTING,FALSE},{D3DRS_CULLMODE,D3DCULL_NONE},
            {D3DRS_FILLMODE,D3DFILL_SOLID},{D3DRS_CLIPPLANEENABLE,0},
            {D3DRS_COLORWRITEENABLE,0xf},{D3DRS_WRAP0,0}}};
        static constexpr std::array<Entry,14> layers{{
            {D3DRS_ZENABLE,FALSE},{D3DRS_ZWRITEENABLE,FALSE},
            {D3DRS_ALPHATESTENABLE,FALSE},{D3DRS_ALPHABLENDENABLE,TRUE},
            {D3DRS_SRCBLEND,D3DBLEND_SRCALPHA},
            {D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA},
            {D3DRS_FOGENABLE,FALSE},{D3DRS_STENCILENABLE,FALSE},
            {D3DRS_LIGHTING,FALSE},{D3DRS_CULLMODE,D3DCULL_NONE},
            {D3DRS_FILLMODE,D3DFILL_SOLID},{D3DRS_CLIPPLANEENABLE,0},
            {D3DRS_COLORWRITEENABLE,0xf},{D3DRS_WRAP0,0}}};
        const Entry entry=layered?layers[index]:plain[index];
        return SUCCEEDED(from(context).device->SetRenderState(entry.first,entry.second));
    }
    static int pixel_shader(void* context){return SUCCEEDED(from(context).device->SetPixelShader(0));}
    static int vertex_shader(void* context){
        return SUCCEEDED(from(context).device->SetVertexShader(D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1));
    }
    static int bind(void* context,int index){
        auto& b=from(context);
        return SUCCEEDED(b.device->SetTexture(0,b.slots[index].value()));
    }
    static int stage_state(void* context,int index,int layered){
        struct Entry {DWORD stage;D3DTEXTURESTAGESTATETYPE key;DWORD value;};
        static constexpr std::array<Entry,13> plain{{
            {0,D3DTSS_COLOROP,D3DTOP_SELECTARG1},{0,D3DTSS_COLORARG1,D3DTA_TEXTURE},
            {0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1},{0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE},
            {0,D3DTSS_RESULTARG,D3DTA_CURRENT},{0,D3DTSS_TEXCOORDINDEX,0},
            {0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE},
            {0,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP},{0,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP},
            {0,D3DTSS_MINFILTER,D3DTEXF_POINT},{0,D3DTSS_MAGFILTER,D3DTEXF_POINT},
            {0,D3DTSS_MIPFILTER,D3DTEXF_NONE},{1,D3DTSS_COLOROP,D3DTOP_DISABLE}}};
        static constexpr std::array<Entry,14> layers{{
            {0,D3DTSS_COLOROP,D3DTOP_MODULATE},{0,D3DTSS_COLORARG1,D3DTA_TEXTURE},
            {0,D3DTSS_COLORARG2,D3DTA_DIFFUSE},{0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1},
            {0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE},{0,D3DTSS_RESULTARG,D3DTA_CURRENT},
            {0,D3DTSS_TEXCOORDINDEX,0},{0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE},
            {0,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP},{0,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP},
            {0,D3DTSS_MINFILTER,D3DTEXF_LINEAR},{0,D3DTSS_MAGFILTER,D3DTEXF_LINEAR},
            {0,D3DTSS_MIPFILTER,D3DTEXF_NONE},{1,D3DTSS_COLOROP,D3DTOP_DISABLE}}};
        const Entry entry=layered?layers[index]:plain[index];
        return SUCCEEDED(from(context).device->SetTextureStageState(entry.stage,entry.key,entry.value));
    }
    static int draw(void* context,const A2FORefitGpuVertex* vertices){
        return SUCCEEDED(from(context).device->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP,2,vertices,sizeof(Vertex)));
    }
    A2FORefitGpuOps operations(){
        return {this,get_caps,cache_state,reset,create,lock,unlock,mark_upload,capture,
                restore_callback,viewport,render_state,pixel_shader,vertex_shader,bind,stage_state,draw};
    }
};
A2FORefitGpuImage input(const Artwork& image){
    return {image.width,image.height,image.pixels.data(),image.pixels.size()};
}
}

bool draw_loading_image(IDirect3DDevice8* device,const Artwork& image){
    if(!device)return false;
    Bridge bridge(device);const auto ops=bridge.operations();
    return a2fo_refit_gpu_draw(input(image),0,0,&ops)!=0;
}
void LoadingTextureCache::reset(){
    if(texture_)texture_->Release();
    texture_=nullptr;device_=nullptr;width_=height_=texture_width_=texture_height_=0;
    uploaded_=false;alpha_preserved_=false;
}
bool draw_loading_image(IDirect3DDevice8* device,const Artwork& image,
                        LoadingTextureCache& cache,std::uint64_t revision){
    if(!device)return false;
    Bridge bridge(device);auto& slot=bridge.slots[0];
    slot.cache=&cache;slot.texture=&cache.texture_;slot.device=&cache.device_;
    slot.width=&cache.width_;slot.height=&cache.height_;
    slot.texture_width=&cache.texture_width_;slot.texture_height=&cache.texture_height_;
    slot.revision=&cache.revision_;slot.uploaded=&cache.uploaded_;
    slot.alpha_preserved=&cache.alpha_preserved_;slot.stats=&cache.stats_;
    const auto ops=bridge.operations();
    return a2fo_refit_gpu_draw(input(image),revision,1,&ops)!=0;
}
bool draw_loading_layers(IDirect3DDevice8* device,const Artwork& background,
                         const Artwork& overlay,int out_width,int out_height,
                         LoadingTextureCache* background_cache,LoadingTextureCache* overlay_cache,
                         std::uint64_t background_revision,std::uint64_t overlay_revision){
    if(!device)return false;
    Bridge bridge(device);bridge.layered=true;
    LoadingTextureCache* caches[2]={background_cache,overlay_cache};
    for(int i=0;i<2;++i){
        if(!caches[i])continue;
        auto& slot=bridge.slots[i];auto& cache=*caches[i];
        slot.cache=&cache;slot.texture=&cache.texture_;slot.device=&cache.device_;
        slot.width=&cache.width_;slot.height=&cache.height_;
        slot.texture_width=&cache.texture_width_;slot.texture_height=&cache.texture_height_;
        slot.revision=&cache.revision_;slot.uploaded=&cache.uploaded_;
        slot.alpha_preserved=&cache.alpha_preserved_;slot.stats=&cache.stats_;
    }
    const auto ops=bridge.operations();
    return a2fo_refit_gpu_layers(input(background),input(overlay),out_width,out_height,
                                 background_revision,overlay_revision,
                                 background_cache!=nullptr,overlay_cache!=nullptr,
                                 background_cache&&background_cache==overlay_cache,&ops)!=0;
}
}
