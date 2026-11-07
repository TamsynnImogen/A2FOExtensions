// Separate legacy-SDK TU; the GPU fixture uses modern headers in its own TU.
#include "../renderer/d3d8_compat/resources/compressed_texture8.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
using namespace a2fo::renderer;
using namespace a2fo::renderer::compat;
namespace {
void require(bool value,const char* message){if(!value){std::fprintf(stderr,"FAIL: texture8 %s\n",message);std::exit(1);}}
struct Probe: IUnknown {
    LONG refs=1;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,void** out)override{
        if(!out)return E_POINTER;
        *out=nullptr;if(!IsEqualGUID(iid,IID_IUnknown))return E_NOINTERFACE;
        *out=this;AddRef();return S_OK;
    }
    ULONG STDMETHODCALLTYPE AddRef()override{return ++refs;}
    ULONG STDMETHODCALLTYPE Release()override{return --refs;}
};
constexpr GUID key={0x20001100,0x1200,0x5600,{0x72,0x33,0x18,0x29,0x44,0x55,0x66,0x77}};
constexpr GUID object_key={0x30001100,0x1200,0x5600,{0x72,0x33,0x18,0x29,0x44,0x55,0x66,0x77}};
}
extern "C" HRESULT exercise_compressed_texture8(HWND window,ID3D11Device* modern,
    const textures::CompressedImage& input,ID3D11ShaderResourceView** view) {
    *view=nullptr;
    IDirect3D8* factory=Direct3DCreate8(D3D_SDK_VERSION);require(factory!=nullptr,"native test parent factory");
    D3DPRESENT_PARAMETERS parameters{};parameters.BackBufferWidth=parameters.BackBufferHeight=8;
    parameters.BackBufferFormat=D3DFMT_X8R8G8B8;parameters.BackBufferCount=1;
    parameters.SwapEffect=D3DSWAPEFFECT_DISCARD;parameters.hDeviceWindow=window;parameters.Windowed=TRUE;
    IDirect3DDevice8* parent=nullptr;
    require(SUCCEEDED(factory->CreateDevice(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,window,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,&parameters,&parent)),"native test parent device");
    // Native parent is an identity/lifetime fixture only. The facade is NEVER
    // submitted to this provider's SetTexture or other native draw methods.
    auto image=input;IDirect3DTexture8* texture=nullptr;std::string error;
    const auto interpretation=(image.format==DXGI_FORMAT_BC5_UNORM||image.format==DXGI_FORMAT_BC5_SNORM)?
        TextureInterpretation::normal_xy:TextureInterpretation::raw;
    const HRESULT hr=create_compressed_texture8(parent,modern,std::move(image),interpretation,&texture,error);
    if(FAILED(hr))std::fprintf(stderr,"texture8 creation: %s\n",error.c_str());
    require(SUCCEEDED(hr)&&texture,"modern compressed facade");
    require(texture->GetLevelCount()==input.mips.size()&&texture->GetType()==D3DRTYPE_TEXTURE,"resource type and all mips");
    IDirect3DDevice8* device=nullptr;require(texture->GetDevice(&device)==D3D_OK&&device==parent,"GetDevice identity");device->Release();
    for(const auto* iid:{&IID_IUnknown,&IID_IDirect3DResource8,&IID_IDirect3DBaseTexture8,&IID_IDirect3DTexture8}){
        void* identity=nullptr;require(texture->QueryInterface(*iid,&identity)==S_OK&&identity==texture,"canonical COM identity");
        static_cast<IDirect3DTexture8*>(identity)->Release();
    }
    void* unknown=texture;require(texture->QueryInterface(key,&unknown)==E_NOINTERFACE&&!unknown,"foreign QI clears output");
    require(texture->SetPriority(17)==0&&texture->GetPriority()==17&&texture->SetPriority(9)==17,"priority roundtrip");
    const BYTE blob[]={7,13,29};require(texture->SetPrivateData(key,blob,3,0)==D3D_OK,"private blob set");
    IDirect3DTexture8* reference_texture=nullptr;
    require(parent->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&reference_texture)==D3D_OK,
            "native private-data reference texture");
    require(reference_texture->SetPrivateData(key,blob,3,0)==D3D_OK,"native private-data reference");
    for(DWORD query_size:{DWORD(0),DWORD(3)}){
        DWORD native_size=query_size,facade_size=query_size;
        const HRESULT native_hr=reference_texture->GetPrivateData(key,nullptr,&native_size);
        const HRESULT facade_hr=texture->GetPrivateData(key,nullptr,&facade_size);
        if(native_hr!=facade_hr||native_size!=facade_size)
            std::fprintf(stderr,"PRIVATE_DATA size=%lu native=0x%08lx facade=0x%08lx native_size=%lu facade_size=%lu\n",
                query_size,static_cast<unsigned long>(native_hr),static_cast<unsigned long>(facade_hr),native_size,facade_size);
        require(native_hr==facade_hr&&native_size==facade_size,"private size-query HRESULT matches D3D8 reference");
    }
    reference_texture->Release();
    DWORD size=0;require(texture->GetPrivateData(key,nullptr,&size)==D3D_OK&&size==3,"private size query");
    BYTE got[3]={};size=2;require(texture->GetPrivateData(key,got,&size)==D3DERR_MOREDATA&&size==3,"small blob buffer");
    size=3;require(texture->GetPrivateData(key,got,&size)==D3D_OK&&!std::memcmp(got,blob,3),"private blob get");
    require(texture->FreePrivateData(key)==D3D_OK&&texture->FreePrivateData(key)==D3DERR_NOTFOUND,"private blob release");
    Probe probe;require(texture->SetPrivateData(object_key,&probe,sizeof(IUnknown*),D3DSPD_IUNKNOWN)==D3D_OK&&probe.refs==2,"private IUnknown retained");
    IUnknown* object=nullptr;size=sizeof(object);
    require(texture->GetPrivateData(object_key,&object,&size)==D3D_OK&&object==&probe&&probe.refs==3,"private IUnknown getter retains");
    object->Release();require(texture->FreePrivateData(object_key)==D3D_OK&&probe.refs==1,"private IUnknown freed");
    for(UINT level=0;level<input.mips.size();++level){
        D3DSURFACE_DESC desc{};require(texture->GetLevelDesc(level,&desc)==D3D_OK&&desc.Width==input.mips[level].width&&
            desc.Height==input.mips[level].height&&desc.Size==input.mips[level].slice_pitch&&desc.Pool==D3DPOOL_MANAGED,"mip descriptor");
        D3DLOCKED_RECT locked{};require(texture->LockRect(level,&locked,nullptr,0)==D3DERR_INVALIDCALL,"immutable write lock refused");
        require(texture->LockRect(level,&locked,nullptr,D3DLOCK_READONLY)==D3D_OK&&
            locked.Pitch==static_cast<INT>(input.mips[level].row_pitch)&&
            !std::memcmp(locked.pBits,input.bytes.data()+input.mips[level].offset,input.mips[level].slice_pitch),"exact encoded read shadow");
        IDirect3DSurface8* surface=nullptr;require(texture->GetSurfaceLevel(level,&surface)==D3D_OK,"surface facade");
        require(surface->LockRect(&locked,nullptr,D3DLOCK_READONLY)==D3DERR_INVALIDCALL,"surface and texture share lock");
        require(surface->UnlockRect()==D3D_OK&&texture->UnlockRect(level)==D3DERR_INVALIDCALL,"one shared unlock");
        require(surface->SetPrivateData(key,blob,3,0)==D3D_OK,"surface private data");
        IDirect3DSurface8* identity=surface;require(surface->Release()==0,"implicit surface public release zero");
        require(texture->GetSurfaceLevel(level,&surface)==D3D_OK&&surface==identity,"implicit surface resurrects with same identity");
        size=3;require(surface->GetPrivateData(key,got,&size)==D3D_OK&&!std::memcmp(got,blob,3),"surface private data survives release zero");
        surface->Release();
    }
    D3DLOCKED_RECT lock{};const RECT bad={1,0,4,4};
    require(texture->LockRect(0,&lock,&bad,D3DLOCK_READONLY)==D3DERR_INVALIDCALL,"misaligned block rectangle refused");
    if(input.width>=8&&input.height>=8){
        const RECT block={4,4,8,8};
        const auto& mip=input.mips[0];
        const UINT block_bytes=mip.row_pitch/((mip.width+3)/4);
        require(texture->LockRect(0,&lock,nullptr,D3DLOCK_READONLY)==D3D_OK,"whole mip for subrectangle base");
        const auto* shadow=static_cast<const BYTE*>(lock.pBits);
        require(texture->UnlockRect(0)==D3D_OK,"whole mip unlock before subrectangle");
        require(texture->LockRect(0,&lock,&block,D3DLOCK_READONLY)==D3D_OK&&
            lock.pBits==shadow+mip.row_pitch+block_bytes&&
            !std::memcmp(lock.pBits,input.bytes.data()+mip.offset+mip.row_pitch+block_bytes,block_bytes),
            "aligned subrectangle points at correct compressed block");
        require(texture->UnlockRect(0)==D3D_OK,"subrectangle unlock");
    }
    lock.Pitch=123;lock.pBits=nullptr;
    require(texture->LockRect(0,&lock,nullptr,D3DLOCK_READONLY|D3DLOCK_DISCARD)==D3DERR_INVALIDCALL&&
        lock.Pitch==123&&!lock.pBits,"invalid lock flags preserve output");
    IDirect3DSurface8* absent=texture->GetLevelCount()?reinterpret_cast<IDirect3DSurface8*>(1):nullptr;
    require(texture->GetSurfaceLevel(texture->GetLevelCount(),&absent)==D3DERR_INVALIDCALL&&!absent,
        "invalid surface level clears output");
    require(texture->AddDirtyRect(nullptr)==D3DERR_INVALIDCALL,"immutable dirty update refused");
    require(texture->SetLOD(99)==0&&texture->GetLOD()==input.mips.size()-1,"LOD clamps");
    CompressedTextureBinding* binding=nullptr;
    require(texture->QueryInterface(IID_CompressedTextureBinding,reinterpret_cast<void**>(&binding))==S_OK,"typed private backend interface");
    CompressedBinding metadata;
    require(binding->GetBinding(view,&metadata)==S_OK&&*view&&metadata.minimum_lod==input.mips.size()-1&&
        metadata.format==input.format&&metadata.interpretation==interpretation,"modern view and explicit sampling metadata");
    texture->SetLOD(0);binding->Release();
    IDirect3DSurface8* held=nullptr;require(texture->GetSurfaceLevel(0,&held)==D3D_OK,"retained surface");
    require(held->SetPrivateData(object_key,&probe,sizeof(IUnknown*),D3DSPD_IUNKNOWN)==D3D_OK&&probe.refs==2,"surface object retained implicitly");
    texture->Release(); // Surface reference keeps the texture/parent alive.
    IDirect3DTexture8* container=nullptr;
    require(held->GetContainer(IID_IDirect3DTexture8,reinterpret_cast<void**>(&container))==S_OK,"container after public texture release");
    held->Release();
    CompressedTextureBinding* retained=nullptr;
    require(container->QueryInterface(IID_CompressedTextureBinding,reinterpret_cast<void**>(&retained))==S_OK,
            "device binding interface");
    require(retained->RetainBinding()==1,"device state implicit ownership");
    retained->Release(); // State owns an implicit reference, not a public COM ref.
    require(container->Release()==0&&probe.refs==2,"bound texture storage survives public release zero");
    IDirect3DTexture8* revived=nullptr;
    require(retained->GetTexture(&revived)==S_OK&&revived==container,"GetTexture revives canonical bound interface");
    require(revived->GetDevice(&device)==D3D_OK&&device==parent,"revived texture retains its device");device->Release();
    require(revived->Release()==0&&probe.refs==2,"public ref zero preserves implicit surface private data");
    require(retained->ReleaseBinding()==0&&probe.refs==1,"unbind destroys texture and implicit surfaces without a COM cycle");
    parent->Release();factory->Release();
    std::printf("TEXTURE8 format=%u levels=%zu lifetime=pass readonly_locks=pass private_data=pass\n",
                unsigned(input.format),input.mips.size());
    return S_OK;
}
