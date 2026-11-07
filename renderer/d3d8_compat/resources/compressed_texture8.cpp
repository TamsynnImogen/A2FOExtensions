#include "compressed_texture8.hpp"
#include "../../../rust/render_draw_ffi.h"
#include "../../d3d11/texture_handle.hpp"
#include <algorithm>
#include <cstring>
#include <map>
#include <memory>
#include <new>
#include <vector>
namespace a2fo::renderer::compat {
namespace {
constexpr A2foRsRenderHr kHr{D3D_OK, E_POINTER, E_NOINTERFACE,
    D3DERR_INVALIDCALL, D3DERR_NOTFOUND, D3DERR_MOREDATA, E_OUTOFMEMORY};
static_assert(sizeof(GUID) == 16);
static_assert(sizeof(DWORD) == 4);
struct GuidLess {bool operator()(const GUID& a,const GUID& b)const{return std::memcmp(&a,&b,sizeof(GUID))<0;}};
class PrivateData {
    struct Value {
        std::vector<BYTE> bytes;
        IUnknown* object=nullptr;
        ~Value(){if(object)object->Release();}
    };
    std::map<GUID,std::unique_ptr<Value>,GuidLess> values_;
public:
    HRESULT set(REFGUID guid,const void* data,DWORD size,DWORD flags) noexcept {
        const HRESULT admission=a2fo_rs_texture_private_set(data!=nullptr,size,flags,
            D3DSPD_IUNKNOWN,sizeof(IUnknown*),kHr);
        if(FAILED(admission))return admission;
        try {
            auto value=std::make_unique<Value>();
            if(flags&D3DSPD_IUNKNOWN){
                value->object=static_cast<IUnknown*>(const_cast<void*>(data));
                value->object->AddRef();
            }else{
                const auto* begin=static_cast<const BYTE*>(data);
                value->bytes.assign(begin,begin+size);
            }
            auto found=values_.find(guid);
            if(found==values_.end())values_.emplace(guid,std::move(value));
            else found->second.swap(value);
            return D3D_OK;
        }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return D3DERR_INVALIDCALL;}
    }
    HRESULT get(REFGUID guid,void* data,DWORD* size) noexcept {
        const auto found=values_.find(guid);
        const bool exists=found!=values_.end();
        const auto plan=a2fo_rs_texture_private_get(size!=nullptr,exists,
            exists&&found->second->object!=nullptr,
            exists?static_cast<DWORD>(found->second->bytes.size()):0,
            size?*size:0,data!=nullptr,sizeof(IUnknown*),kHr);
        if(size&&exists)*size=plan.needed;
        if(FAILED(plan.hr)||!plan.copy)return plan.hr;
        const auto& value=*found->second;
        if(value.object){
            IUnknown* object=value.object;object->AddRef();
            std::memcpy(data,&object,sizeof(object));
        }else if(plan.needed)std::memcpy(data,value.bytes.data(),plan.needed);
        return plan.hr;
    }
    void clear() noexcept {values_.clear();}
    HRESULT erase(REFGUID guid) noexcept {
        const auto found=values_.find(guid);
        const HRESULT hr=a2fo_rs_texture_private_erase(found!=values_.end(),kHr);
        if(FAILED(hr))return hr;
        auto value=std::move(found->second);values_.erase(found);return hr;
    }
};
class Texture;
class Surface;
A2foRsTextureRefs texture_refs(Texture*) noexcept;
A2foRsTextureRefs surface_refs(Surface*) noexcept;
class Surface final:public IDirect3DSurface8 {
public:
    Texture* texture;
    UINT level;
    LONG refs=0; // implicit surface storage lives until the parent texture dies
    PrivateData data;
    Surface(Texture* owner,UINT index):texture(owner),level(index){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8**) override;
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID g,const void* p,DWORD n,DWORD f) override{return data.set(g,p,n,f);}
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID g,void* p,DWORD* n) override{return data.get(g,p,n);}
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID g) override{return data.erase(g);}
    HRESULT STDMETHODCALLTYPE GetContainer(REFIID,void**) override;
    HRESULT STDMETHODCALLTYPE GetDesc(D3DSURFACE_DESC*) override;
    HRESULT STDMETHODCALLTYPE LockRect(D3DLOCKED_RECT*,const RECT*,DWORD) override;
    HRESULT STDMETHODCALLTYPE UnlockRect() override;
};
D3DFORMAT legacy_format(DXGI_FORMAT format) {
    static constexpr A2foRsFormatPair formats[]{
        {DXGI_FORMAT_BC1_UNORM,D3DFMT_DXT1},{DXGI_FORMAT_BC1_UNORM_SRGB,D3DFMT_DXT1},
        {DXGI_FORMAT_BC2_UNORM,D3DFMT_DXT3},{DXGI_FORMAT_BC2_UNORM_SRGB,D3DFMT_DXT3},
        {DXGI_FORMAT_BC3_UNORM,D3DFMT_DXT5},{DXGI_FORMAT_BC3_UNORM_SRGB,D3DFMT_DXT5},
        {DXGI_FORMAT_BC4_UNORM,MAKEFOURCC('A','T','I','1')},
        {DXGI_FORMAT_BC4_SNORM,MAKEFOURCC('B','C','4','S')},
        {DXGI_FORMAT_BC5_UNORM,MAKEFOURCC('A','T','I','2')},
        {DXGI_FORMAT_BC5_SNORM,MAKEFOURCC('B','C','5','S')},
        {DXGI_FORMAT_BC7_UNORM,MAKEFOURCC('B','C','7',' ')},
        {DXGI_FORMAT_BC7_UNORM_SRGB,MAKEFOURCC('B','C','7',' ')},
    };
    return static_cast<D3DFORMAT>(a2fo_rs_texture_legacy_format(format,
        formats,sizeof(formats)/sizeof(formats[0]),D3DFMT_UNKNOWN));
}
class Texture final:public IDirect3DTexture8,public CompressedTextureBinding {
public:
    LONG refs=1;
    LONG implicit_refs=0;
    IDirect3DDevice8* parent;
    textures::CompressedImage image;
    TextureInterpretation interpretation;
    std::unique_ptr<CompressedTextureHandle> gpu;
    std::vector<std::unique_ptr<Surface>> surfaces;
    std::vector<bool> locked;
    DWORD priority=0,lod=0;
    PrivateData data;
    Texture(IDirect3DDevice8* owner,textures::CompressedImage&& input,
            TextureInterpretation mode,std::unique_ptr<CompressedTextureHandle>&& handle)
        :parent(owner),image(std::move(input)),interpretation(mode),gpu(std::move(handle)),
         surfaces(image.mips.size()),locked(image.mips.size(),false){parent->AddRef();}
    ~Texture(){surfaces.clear();data.clear();gpu.reset();}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out) override {
        const GUID known[]{IID_IUnknown,IID_IDirect3DResource8,
            IID_IDirect3DBaseTexture8,IID_IDirect3DTexture8,
            IID_CompressedTextureBinding,IID_IDirect3DSurface8};
        const auto plan=a2fo_rs_texture_query(
            reinterpret_cast<const std::uint8_t*>(&iid),
            reinterpret_cast<const std::uint8_t*>(known),0,out!=nullptr,kHr);
        if(!out)return plan.hr;
        *out=nullptr;
        if(FAILED(plan.hr))return plan.hr;
        *out=plan.kind==2?static_cast<void*>(static_cast<IDirect3DTexture8*>(this)):
            static_cast<void*>(static_cast<CompressedTextureBinding*>(this));
        AddRef();return plan.hr;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return a2fo_rs_texture_addref(texture_refs(this));}
    ULONG STDMETHODCALLTYPE Release() override {return a2fo_rs_texture_release(texture_refs(this));}
    ULONG STDMETHODCALLTYPE RetainBinding() override {return a2fo_rs_texture_retain_binding(texture_refs(this));}
    ULONG STDMETHODCALLTYPE ReleaseBinding() override {return a2fo_rs_texture_release_binding(texture_refs(this));}
    HRESULT STDMETHODCALLTYPE GetTexture(IDirect3DTexture8** out) override {
        const HRESULT hr=a2fo_rs_texture_output_valid(out!=nullptr,E_POINTER,kHr);
        if(FAILED(hr))return hr;
        AddRef();*out=static_cast<IDirect3DTexture8*>(this);return hr;
    }
    HRESULT STDMETHODCALLTYPE GetDevice(IDirect3DDevice8** out) override {
        const HRESULT hr=a2fo_rs_texture_output_valid(out!=nullptr,D3DERR_INVALIDCALL,kHr);
        if(FAILED(hr))return hr;
        parent->AddRef();*out=parent;return hr;
    }
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID g,const void* p,DWORD n,DWORD f) override{return data.set(g,p,n,f);}
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID g,void* p,DWORD* n) override{return data.get(g,p,n);}
    HRESULT STDMETHODCALLTYPE FreePrivateData(REFGUID g) override{return data.erase(g);}
    DWORD STDMETHODCALLTYPE SetPriority(DWORD value) override {
        std::uint32_t next=0;const auto old=a2fo_rs_texture_exchange_u32(priority,value,&next);
        priority=next;return old;
    }
    DWORD STDMETHODCALLTYPE GetPriority() override{return priority;}
    void STDMETHODCALLTYPE PreLoad() override{} // already uploaded
    D3DRESOURCETYPE STDMETHODCALLTYPE GetType() override{return D3DRTYPE_TEXTURE;}
    DWORD STDMETHODCALLTYPE SetLOD(DWORD value) override {
        std::uint32_t next=0;const auto old=a2fo_rs_texture_set_lod(lod,value,GetLevelCount(),&next);
        lod=next;return old;
    }
    DWORD STDMETHODCALLTYPE GetLOD() override{return lod;}
    DWORD STDMETHODCALLTYPE GetLevelCount() override{return static_cast<DWORD>(image.mips.size());}
    HRESULT STDMETHODCALLTYPE GetLevelDesc(UINT level,D3DSURFACE_DESC* out) override {
        const HRESULT hr=a2fo_rs_texture_level_valid(out!=nullptr,level,image.mips.size(),kHr);
        if(FAILED(hr))return hr;
        const auto& mip=image.mips[level];D3DSURFACE_DESC value{};
        value.Format=legacy_format(image.format);value.Type=D3DRTYPE_SURFACE;
        value.Pool=D3DPOOL_MANAGED;value.Size=mip.slice_pitch;
        value.MultiSampleType=D3DMULTISAMPLE_NONE;value.Width=mip.width;value.Height=mip.height;
        *out=value;return hr;
    }
    HRESULT STDMETHODCALLTYPE GetSurfaceLevel(UINT level,IDirect3DSurface8** out) override {
        const HRESULT hr=a2fo_rs_texture_level_valid(out!=nullptr,level,image.mips.size(),kHr);
        if(!out)return hr;
        *out=nullptr;
        if(FAILED(hr))return hr;
        try{
            if(!surfaces[level])surfaces[level]=std::make_unique<Surface>(this,level);
            surfaces[level]->AddRef();*out=surfaces[level].get();return hr;
        }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return D3DERR_INVALIDCALL;}
    }
    HRESULT STDMETHODCALLTYPE LockRect(UINT level,D3DLOCKED_RECT* out,const RECT* region,DWORD flags) override {
        const HRESULT admission=a2fo_rs_texture_lock_pre(out!=nullptr,level,
            image.mips.size(),level<locked.size()&&locked[level],flags,D3DLOCK_READONLY,
            D3DLOCK_READONLY|D3DLOCK_NOSYSLOCK|D3DLOCK_NO_DIRTY_UPDATE,kHr);
        if(FAILED(admission))return admission;
        const auto& mip=image.mips[level];
        const A2foRsRect rectangle=region?A2foRsRect{region->left,region->top,
            region->right,region->bottom}:A2foRsRect{};
        const auto plan=a2fo_rs_texture_lock(out!=nullptr,level,image.mips.size(),
            locked[level],flags,D3DLOCK_READONLY,
            D3DLOCK_READONLY|D3DLOCK_NOSYSLOCK|D3DLOCK_NO_DIRTY_UPDATE,
            region!=nullptr,rectangle,{mip.width,mip.height,mip.row_pitch,mip.offset},
            image.format,DXGI_FORMAT_BC1_UNORM,DXGI_FORMAT_BC1_UNORM_SRGB,
            DXGI_FORMAT_BC4_UNORM,DXGI_FORMAT_BC4_SNORM,kHr);
        if(FAILED(plan.hr))return plan.hr;
        D3DLOCKED_RECT value{};value.Pitch=plan.pitch;
        value.pBits=image.bytes.data()+plan.offset;
        locked[level]=true;*out=value;return plan.hr;
    }
    HRESULT STDMETHODCALLTYPE UnlockRect(UINT level) override {
        const HRESULT hr=a2fo_rs_texture_unlock(level,locked.size(),
            level<locked.size()&&locked[level],kHr);
        if(SUCCEEDED(hr))locked[level]=false;
        return hr;
    }
    HRESULT STDMETHODCALLTYPE AddDirtyRect(const RECT*) override{return D3DERR_INVALIDCALL;}
    HRESULT STDMETHODCALLTYPE GetBinding(ID3D11ShaderResourceView** out,CompressedBinding* binding) override {
        if(out)*out=nullptr;
        const HRESULT admission=a2fo_rs_texture_binding_valid(out!=nullptr,binding!=nullptr,kHr);
        if(FAILED(admission))return admission;
        const HRESULT hr=gpu->get_view(out);
        if(a2fo_rs_texture_binding_after(hr))*binding={image.format,interpretation,lod};
        return hr;
    }

};
int32_t inc_public(void* raw) noexcept {return InterlockedIncrement(&static_cast<Texture*>(raw)->refs);}
int32_t dec_public(void* raw) noexcept {return InterlockedDecrement(&static_cast<Texture*>(raw)->refs);}
int32_t inc_implicit(void* raw) noexcept {return InterlockedIncrement(&static_cast<Texture*>(raw)->implicit_refs);}
int32_t dec_implicit(void* raw) noexcept {return InterlockedDecrement(&static_cast<Texture*>(raw)->implicit_refs);}
int32_t load_public(void* raw) noexcept {return InterlockedCompareExchange(&static_cast<Texture*>(raw)->refs,0,0);}
void parent_addref(void* raw) noexcept {static_cast<Texture*>(raw)->parent->AddRef();}
void parent_release(void* raw) noexcept {static_cast<Texture*>(raw)->parent->Release();}
void delete_texture(void* raw) noexcept {delete static_cast<Texture*>(raw);}
void unused_void(void*) noexcept {}
int32_t unused_count(void*) noexcept {return 0;}
int32_t surface_inc(void* raw) noexcept {return InterlockedIncrement(&static_cast<Surface*>(raw)->refs);}
int32_t surface_dec(void* raw) noexcept {return InterlockedDecrement(&static_cast<Surface*>(raw)->refs);}
void surface_texture_addref(void* raw) noexcept {static_cast<Surface*>(raw)->texture->AddRef();}
void surface_texture_release(void* raw) noexcept {static_cast<Surface*>(raw)->texture->Release();}
A2foRsTextureRefs texture_refs(Texture* texture) noexcept {
    return {texture,inc_public,dec_public,inc_implicit,dec_implicit,load_public,
        parent_addref,parent_release,delete_texture,unused_void,unused_void};
}
A2foRsTextureRefs surface_refs(Surface* surface) noexcept {
    return {surface,surface_inc,surface_dec,unused_count,unused_count,unused_count,
        unused_void,unused_void,unused_void,surface_texture_addref,surface_texture_release};
}
HRESULT Surface::QueryInterface(REFIID iid,void** out){
    const GUID known[]{IID_IUnknown,IID_IDirect3DResource8,
        IID_IDirect3DBaseTexture8,IID_IDirect3DTexture8,
        IID_CompressedTextureBinding,IID_IDirect3DSurface8};
    const auto plan=a2fo_rs_texture_query(
        reinterpret_cast<const std::uint8_t*>(&iid),
        reinterpret_cast<const std::uint8_t*>(known),1,out!=nullptr,kHr);
    if(!out)return plan.hr;
    *out=nullptr;
    if(FAILED(plan.hr))return plan.hr;
    *out=static_cast<IDirect3DSurface8*>(this);AddRef();return plan.hr;
}
ULONG Surface::AddRef(){return a2fo_rs_surface_addref(surface_refs(this));}
ULONG Surface::Release(){return a2fo_rs_surface_release(surface_refs(this));}
HRESULT Surface::GetDevice(IDirect3DDevice8** out){return texture->GetDevice(out);}
HRESULT Surface::GetContainer(REFIID iid,void** out){return texture->QueryInterface(iid,out);}
HRESULT Surface::GetDesc(D3DSURFACE_DESC* out){return texture->GetLevelDesc(level,out);}
HRESULT Surface::LockRect(D3DLOCKED_RECT* out,const RECT* r,DWORD flags){return texture->LockRect(level,out,r,flags);}
HRESULT Surface::UnlockRect(){return texture->UnlockRect(level);}
}
HRESULT create_compressed_texture8(IDirect3DDevice8* owner,ID3D11Device* device,
    textures::CompressedImage&& image,TextureInterpretation interpretation,
    IDirect3DTexture8** output,std::string& error) noexcept {
    if(output)*output=nullptr;
    try{
        const auto plan=a2fo_rs_texture_create(output!=nullptr,owner!=nullptr,
            device!=nullptr,static_cast<UINT>(interpretation),
            static_cast<UINT>(TextureInterpretation::raw),
            static_cast<UINT>(TextureInterpretation::normal_xy),image.format,
            DXGI_FORMAT_BC5_UNORM,DXGI_FORMAT_BC5_SNORM,kHr);
        if(FAILED(plan.hr)){
            if(plan.error_kind==1)
                error="[D3D8Compat] compressed texture requires owner, device and output";
            else if(plan.error_kind==2)
                error="[D3D8Compat] XY reconstruction requires BC5";
            return plan.hr;
        }
        auto handle=std::make_unique<CompressedTextureHandle>();
        HRESULT hr=handle->initialize(device,image,error);if(FAILED(hr))return hr;
        auto pending=std::make_unique<Texture>(owner,std::move(image),interpretation,std::move(handle));
        *output=static_cast<IDirect3DTexture8*>(pending.release());return D3D_OK;
    }catch(const std::bad_alloc&){return E_OUTOFMEMORY;}catch(...){return D3DERR_INVALIDCALL;}
}
}
