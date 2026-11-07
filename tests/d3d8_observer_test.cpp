// Synthetic COM and PE32 fixtures only. No window, game, input or graphics DLL.
#include "../renderer/d3d8_compat/observer.cpp"
#include <cstdlib>
#include <fstream>
#include <iterator>

namespace a2fo::renderer {
HRESULT run_d3d11_window_probe(HWND,UINT,UINT,bool,void (*)(const std::string&)) {
    std::abort(); // A legacy trace must never reach the GPU backend.
}
}
namespace {
using namespace a2fo::renderer;
struct Object { void** vtable; ULONG refs = 1; };
void* device_table[97]{};
void* factory_table[16]{};
void* buffer_table[14]{};
Object device{device_table}, factory{factory_table}, buffer{buffer_table};
DWORD observed_state=0,observed_value=0,observed_lock=0;
UINT observed_sdk=0;
unsigned shader_calls=0;
bool fail_shader=false;
void* lock_data=reinterpret_cast<void*>(0x12345678);
void require(bool value,const char* message) {
    if(!value) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
void log(const std::string& line) { std::printf("%s\n",line.c_str()); }
ULONG STDMETHODCALLTYPE addref(Object* self) { return ++self->refs; }
ULONG STDMETHODCALLTYPE release(Object* self) { return --self->refs; }
HRESULT STDMETHODCALLTYPE query(Object* self,REFIID,void** result) {
    if(!result)return E_POINTER;
    *result=self; ++self->refs; return S_OK;
}
HRESULT STDMETHODCALLTYPE set_state(IDirect3DDevice8*,D3DRENDERSTATETYPE state,DWORD value) {
    observed_state=state;observed_value=value;return value==0xdeadbeef ? D3DERR_INVALIDCALL : S_OK;
}
HRESULT STDMETHODCALLTYPE present(IDirect3DDevice8*,const RECT*,const RECT*,HWND,const RGNDATA*) { return D3DERR_DEVICELOST; }
HRESULT STDMETHODCALLTYPE create_vertex_shader(IDirect3DDevice8*,const DWORD*,const DWORD*,DWORD* shader,DWORD usage) {
    require(usage==0x10,"shader usage forwarded exactly");
    ++shader_calls;
    if(fail_shader)return D3DERR_INVALIDCALL;
    *shader=0x1001;return S_OK;
}
HRESULT STDMETHODCALLTYPE create_pixel_shader(IDirect3DDevice8*,const DWORD*,DWORD* shader) {
    ++shader_calls;*shader=0x2001;return S_OK;
}
HRESULT STDMETHODCALLTYPE create_buffer(IDirect3DDevice8*,UINT,DWORD,DWORD,D3DPOOL,IDirect3DVertexBuffer8** result) {
    if(!result)return E_POINTER;
    *result=reinterpret_cast<IDirect3DVertexBuffer8*>(&buffer);return S_OK;
}
HRESULT STDMETHODCALLTYPE buffer_device(IDirect3DVertexBuffer8*,IDirect3DDevice8** result) {
    if(!result)return E_POINTER;
    *result=reinterpret_cast<IDirect3DDevice8*>(&device);++device.refs;return S_OK;
}
HRESULT STDMETHODCALLTYPE buffer_lock(IDirect3DVertexBuffer8*,UINT offset,UINT size,BYTE** result,DWORD flags) {
    require(offset==8&&size==16,"lock offset/size exact");observed_lock=flags;*result=static_cast<BYTE*>(lock_data);return S_OK;
}
HRESULT STDMETHODCALLTYPE create_device(IDirect3D8*,UINT adapter,D3DDEVTYPE type,HWND focus,DWORD flags,D3DPRESENT_PARAMETERS* pp,IDirect3DDevice8** result) {
    require(adapter==2&&type==D3DDEVTYPE_HAL&&focus==nullptr&&flags==0x20,"CreateDevice arguments exact");
    *result=reinterpret_cast<IDirect3DDevice8*>(&device);pp->BackBufferWidth=640;return S_OK;
}
IDirect3D8* WINAPI native_factory(UINT sdk) { observed_sdk=sdk;return reinterpret_cast<IDirect3D8*>(&factory); }
void init_table(void** table) {
    table[0]=address(&query);table[1]=address(&addref);table[2]=address(&release);
}
HMODULE image() {
    auto* data=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    require(data!=nullptr,"allocate synthetic image");
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(data);dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=128;
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS32*>(data+128);nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_I386;
    nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR32_MAGIC;nt->OptionalHeader.SizeOfImage=4096;
    nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]={512,40};
    auto* imports=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(data+512);imports->Name=600;imports->OriginalFirstThunk=640;imports->FirstThunk=680;
    std::memcpy(data+600,"d3d8.dll",sizeof("d3d8.dll"));
    auto* thunk=reinterpret_cast<IMAGE_THUNK_DATA32*>(data+640);thunk->u1.AddressOfData=720;
    std::memcpy(data+722,"Direct3DCreate8",15);
    *reinterpret_cast<void**>(data+680)=address(&native_factory);
    return reinterpret_cast<HMODULE>(data);
}
}
int main(int argc,char** argv) {
    require(argc==2,"fixture output directory argument required");
    const std::string root=argv[1],ini=root+"\\A2FORenderer.ini";
    require(WritePrivateProfileStringA("D3D8Compat","Backend","d3d8",ini.c_str()),"write fixture config");
    require(WritePrivateProfileStringA("D3D8Compat","Trace","0",ini.c_str()),"write disabled trace");
    HMODULE pe=image();
    require(install_d3d8_observer(pe,nullptr,root,log)&&!runtime,"default path does not allocate/hook");
    require(WritePrivateProfileStringA("D3D8Compat","Backend","d3d11",ini.c_str()),"write unsupported backend");
    require(!install_d3d8_observer(pe,nullptr,root,log)&&!runtime,"unsupported full D3D11 does not masquerade as parity");
    require(WritePrivateProfileStringA("D3D8Compat","Backend","d3d8",ini.c_str()),"restore backend");
    require(WritePrivateProfileStringA("D3D8Compat","Trace","1",ini.c_str()),"enable fixture trace");
    require(WritePrivateProfileStringA("D3D8Compat","MaxEvents","32",ini.c_str()),"trace budget");
    require(install_d3d8_observer(pe,nullptr,root,log),"named import install");
    require(runtime&&runtime->factory==native_factory,"original import retained");
    require(patch_factory_import(pe),"repeat install idempotent");
    init_table(factory_table);init_table(device_table);init_table(buffer_table);
    factory_table[15]=address(&create_device);device_table[50]=address(&set_state);device_table[15]=address(&present);
    device_table[75]=address(&create_vertex_shader);device_table[87]=address(&create_pixel_shader);
    device_table[23]=address(&create_buffer);buffer_table[3]=address(&buffer_device);buffer_table[11]=address(&buffer_lock);
    const auto get_factory=function_at<Factory>(*reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(pe)+680));
    IDirect3D8* d3d=get_factory(D3D_SDK_VERSION);
    require(observed_sdk==D3D_SDK_VERSION&&d3d==reinterpret_cast<IDirect3D8*>(&factory),"native factory identity preserved");
    require(factory.vtable!=factory_table&&factory_table[15]==address(&create_device),"per-object table, provider table unchanged");
    D3DPRESENT_PARAMETERS pp{};pp.Windowed=TRUE;pp.BackBufferWidth=800;
    IDirect3DDevice8* dev=nullptr;
    require(SUCCEEDED(d3d->CreateDevice(2,D3DDEVTYPE_HAL,nullptr,0x20,&pp,&dev)),"forward CreateDevice");
    require(dev==reinterpret_cast<IDirect3DDevice8*>(&device)&&pp.BackBufferWidth==640,"native output/mutated PP exact");
    const DWORD declaration[]={D3DVSD_STREAM(0),D3DVSD_REG(0,D3DVSDT_FLOAT3),D3DVSD_END()};
    const DWORD vertex_code[]={0xfffe0101,1,0xc00f0000,0x90e40000,0x0000ffff};
    const DWORD pixel_code[]={0xffff0104,D3DSIO_DEF,0xa00f0000,0xffffffff,0x0000ffff,0,0,
                              D3DSIO_MOV,0x800f0000,0xa0e40000,D3DSIO_END};
    DWORD shader=0;
    runtime->trace=false;
    require(SUCCEEDED(dev->CreateVertexShader(declaration,vertex_code,&shader,0x10))&&shader==0x1001&&runtime->shader_captures==0,"Trace=0 forwards shader without capture");
    runtime->trace=true;
    require(SUCCEEDED(dev->CreateVertexShader(declaration,vertex_code,&shader,0x10))&&shader==0x1001,"vertex shader handle and HRESULT exact");
    require(SUCCEEDED(dev->CreatePixelShader(pixel_code,&shader))&&shader==0x2001,"pixel shader handle and HRESULT exact");
    fail_shader=true;
    const auto captures_before_failure=runtime->shader_captures;
    require(dev->CreateVertexShader(declaration,vertex_code,&shader,0x10)==D3DERR_INVALIDCALL&&runtime->shader_captures==captures_before_failure,"failed creation does not capture or change result");
    fail_shader=false;
    SYSTEM_INFO system{};GetSystemInfo(&system);
    auto* guarded=static_cast<BYTE*>(VirtualAlloc(nullptr,system.dwPageSize*2,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    require(guarded!=nullptr,"allocate guarded shader source");
    DWORD guard_protect=0;
    require(VirtualProtect(guarded+system.dwPageSize,system.dwPageSize,PAGE_NOACCESS,&guard_protect),"guard shader source");
    auto* last_word=reinterpret_cast<DWORD*>(guarded+system.dwPageSize-sizeof(DWORD));
    *last_word=D3DVSD_END();
    require(SUCCEEDED(dev->CreateVertexShader(last_word,nullptr,&shader,0x10)),"declaration END at guard boundary forwarded");
    require(read_shader_tokens(last_word,true).words.size()==1,"reader stops before guard page after END");
    *last_word=D3DVSD_STREAM(0);
    require(SUCCEEDED(dev->CreateVertexShader(last_word,nullptr,&shader,0x10))&&shader==0x1001,"unreadable diagnostics preserve accepted native result");
    require(std::strcmp(read_shader_tokens(last_word,true).status,"unreadable")==0,"incomplete unreadable payload explicitly rejected");
    VirtualFree(guarded,0,MEM_RELEASE);
    for(unsigned n=0;n<70;++n)require(SUCCEEDED(dev->CreateVertexShader(declaration,vertex_code,&shader,0x10)),"capture limit still forwards every creation");
    require(runtime->shader_captures==64&&runtime->shader_limit_reported&&shader_calls==76,"shader capture bound does not cap native calls");
    {
        std::ifstream file(runtime->trace_path);
        const std::string captured((std::istreambuf_iterator<char>(file)),std::istreambuf_iterator<char>());
        require(captured.find("\"stage\":\"pixel\"")!=std::string::npos&&captured.find("\"0xffffffff\",\"0x0000ffff\",\"0x00000000\",\"0x00000000\",\"0x00000001\"")!=std::string::npos,"constant END patterns retained inside full shader payload");
        require(captured.find("\"status\":\"unreadable\",\"words\":[]")!=std::string::npos,"unreadable capture contains no partial words");
    }
    require(SUCCEEDED(dev->SetRenderState(D3DRS_ALPHABLENDENABLE,1))&&observed_state==D3DRS_ALPHABLENDENABLE&&observed_value==1,"state pass-through");
    require(dev->SetRenderState(D3DRS_ZENABLE,0xdeadbeef)==D3DERR_INVALIDCALL,"failure HRESULT exact");
    void* identity=nullptr;
    require(SUCCEEDED(dev->QueryInterface(__uuidof(IUnknown),&identity))&&identity==dev,"IUnknown native identity");
    require(reinterpret_cast<IDirect3DDevice8*>(identity)->Release()==1,"QueryInterface reference exact");
    IDirect3DVertexBuffer8* vb=nullptr;
    require(SUCCEEDED(dev->CreateVertexBuffer(64,D3DUSAGE_DYNAMIC,D3DFVF_XYZ,D3DPOOL_DEFAULT,&vb)),"resource output observed");
    IDirect3DDevice8* owner=nullptr;
    require(SUCCEEDED(vb->GetDevice(&owner))&&owner==dev,"resource GetDevice retains native identity");owner->Release();
    BYTE* data=nullptr;
    require(SUCCEEDED(vb->Lock(8,16,&data,D3DLOCK_DISCARD|D3DLOCK_NOSYSLOCK))&&data==lock_data&&observed_lock==(D3DLOCK_DISCARD|D3DLOCK_NOSYSLOCK),"resource lock pointer/flags exact");
    require(dev->Present(nullptr,nullptr,nullptr,nullptr)==D3DERR_DEVICELOST,"lost-device result unmodified");
    for(unsigned n=0;n<9000;++n)dev->SetRenderState(D3DRS_TEXTUREFACTOR,n);
    require(runtime->events==32&&runtime->signatures.size()==8192&&runtime->dropped_signatures>0,"bounded samples/histograms, all calls counted");
    const auto previous=find_record(vb)->id;
    require(vb->Release()==0&&!find_record(vb),"resource final Release retires metadata");
    // Direct3D's internal ownership can resurrect a public-refcount-zero
    // surface/buffer without replacing its vtable. Exercise that exact case.
    require(vb->AddRef()==1&&find_record(vb),"implicit native object can re-enter stable dispatch after Release zero");
    require(vb->Release()==0&&!find_record(vb),"resurrected object can retire again");
    buffer.vtable=buffer_table;buffer.refs=1;observe(&buffer,Kind::IDirect3DVertexBuffer8);
    require(find_record(&buffer)->id>previous,"address reuse gets new identity");
    const auto public_id=find_record(&buffer)->id;
    buffer.vtable=buffer_table; // Simulated internal destruction/reconstruction.
    observe(&buffer,Kind::IDirect3DVertexBuffer8);
    require(find_record(&buffer)->id>public_id,"internal address reuse with new native table re-observed");
    reinterpret_cast<IDirect3DVertexBuffer8*>(&buffer)->Release();
    dev->Release();d3d->Release();
    require(runtime->objects.empty(),"all metadata retired without retained COM references");
    flush_d3d8_observer();
    require(runtime->methods["IDirect3DDevice8.SetRenderState"].calls==9002,"method counters survive sample cap");
    require(runtime->methods["IDirect3DDevice8.SetRenderState"].failures==1,"HRESULT failures counted");
    require(!patch_factory_import(reinterpret_cast<HMODULE>(1)),"malformed image rejected");
    auto* bytes=reinterpret_cast<unsigned char*>(pe);
    auto* imports=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(bytes+512);
    imports->Name=4096-sizeof("d3d8.dll")+1;
    std::memcpy(bytes+imports->Name,"d3d8.dll",sizeof("d3d8.dll")-1);
    require(!patch_factory_import(pe),"unterminated import at image end rejected");
    imports->Name=600;
    reinterpret_cast<IMAGE_THUNK_DATA32*>(bytes+640)->u1.AddressOfData=0xfffffffeu;
    require(!patch_factory_import(pe),"wrapping import-name RVA rejected");
    DWORD prior_protection=0;
    require(VirtualProtect(pe,4096,PAGE_EXECUTE,&prior_protection),"execute-only fixture page");
    require(!patch_factory_import(pe),"execute-only PE memory is not read");
    require(VirtualProtect(pe,4096,prior_protection,&prior_protection),"restore fixture page");
    for(int n=0;n<20;++n)flush_d3d8_observer();
    require(runtime->trace_limit_reported&&runtime->trace_bytes<4194304,"summaries respect total trace byte cap");
    VirtualFree(pe,0,MEM_RELEASE);
    std::puts("PASS: D3D8 observer native identity, call ABI, IAT, HRESULTs, lock flags, lifetime, bounds and fallback");
    return 0;
}
