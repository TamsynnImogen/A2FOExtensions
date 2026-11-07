#include "../renderer/shadows/pass.hpp"
#include "../renderer/shadows/shader.hpp"
#include <windows.h>
#include <unknwn.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <utility>

namespace {
struct UnsupportedDevice final : IUnknown {
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void** out) override {
        if(out)*out=nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {return 1;}
    ULONG STDMETHODCALLTYPE Release() override {return 1;}
};
int failures=0;
void check(bool condition,const char* why) {
    if(!condition) {std::fprintf(stderr,"FAIL: %s\n",why);++failures;}
}
}
int main() {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    std::puts("shadow native fixture: start");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    HMODULE compiler=LoadLibraryA("d3dcompiler_47.dll");
    if(!compiler)compiler=LoadLibraryA("d3dcompiler_43.dll");
    if(compiler) {
        const FARPROC proc=GetProcAddress(compiler,"D3DCompile");
        pD3DCompile compile=nullptr;
        static_assert(sizeof(proc)==sizeof(compile));
        std::memcpy(&compile,&proc,sizeof(proc));
        check(compile!=nullptr,"D3DCompile export");
        if(compile) for(const auto& shader : {
            std::pair<const char*,const char*>{a2fo::shadows::shader::vertex,"vs_3_0"},
            {a2fo::shadows::shader::depth_pixel,"ps_3_0"},
            {a2fo::shadows::shader::overlay_pixel,"ps_3_0"}}) {
            ID3DBlob *blob=nullptr,*errors=nullptr;
            const HRESULT hr=compile(shader.first,std::strlen(shader.first),nullptr,nullptr,
                                      nullptr,"main",shader.second,0,0,&blob,&errors);
            if(FAILED(hr) && errors)
                std::fprintf(stderr,"shader error: %s\n",
                             static_cast<const char*>(errors->GetBufferPointer()));
            check(SUCCEEDED(hr) && blob,"SM3 shader compilation");
            if(blob)blob->Release();
            if(errors)errors->Release();
        }
        FreeLibrary(compiler);
    } else std::printf("D3DCompile unavailable: shader check skipped (loader error %lu)\n",GetLastError());
    a2fo::shadows::Pass pass;
    a2fo::shadows::Capture capture{};
    a2fo::shadows::Draw draw{};
    draw.primitive_count=1;
    const float triangle[9]{0,0,0,1,0,0,0,1,0};
    UnsupportedDevice unsupported;
    check(!pass.capture(&unsupported,capture,draw,triangle,3),"disabled capture");
    check(!pass.finish_view(),"empty disabled view");
    a2fo::shadows::Settings settings{};
    settings.enabled=true;
    settings.size=513;
    pass.configure(settings);
    check(!pass.capture(&unsupported,capture,draw,triangle,3),"invalid size disables capture");
    check(std::strstr(pass.diagnostic(),"invalid")!=nullptr,"invalid settings diagnostic");
    settings.size=1024;
    for(float invalid : {-2.0f,-0.5f,101.0f,std::numeric_limits<float>::infinity(),
                         std::numeric_limits<float>::quiet_NaN()}) {
        settings.bias_world=invalid;
        pass.configure(settings);
        check(std::strstr(pass.diagnostic(),"invalid")!=nullptr,"invalid world bias rejected");
    }
    settings.bias_world=-1.0f;
    pass.configure(settings);
    pass.begin_view();
    check(!pass.capture(nullptr,capture,draw,triangle,3),"null device rejected");
    check(!pass.capture(&unsupported,capture,draw,triangle,2),"triangle count mismatch rejected");
    check(std::strstr(pass.diagnostic(),"snapshot")!=nullptr,"snapshot diagnostic");
    draw.primitive_count=262145;
    check(!pass.capture(&unsupported,capture,draw,triangle,3),"oversized snapshot rejected before access");
    draw.primitive_count=1;
    float bad_triangle[9]{0,0,0,1,0,0,0,1,0};
    bad_triangle[2]=std::numeric_limits<float>::infinity();
    check(!pass.capture(&unsupported,capture,draw,bad_triangle,3),"nonfinite snapshot rejected");
    capture.bounds_extent[0]=capture.bounds_extent[1]=capture.bounds_extent[2]=1.0f;
    capture.light_direction[1]=-1.0f;
    check(!pass.capture(&unsupported,capture,draw,triangle,3),"non-D3D9 device rejected");
    check(std::strstr(pass.diagnostic(),"D3D9")!=nullptr,"D3D9 diagnostic");
    check(!pass.finish_view(),"no draw after QI failure");
    pass.release();
    pass.begin_view();
    check(!pass.finish_view(),"release clears view");
    if(failures)return 1;
    std::puts("shadow pass headless lifecycle: PASS");
    return 0;
}
