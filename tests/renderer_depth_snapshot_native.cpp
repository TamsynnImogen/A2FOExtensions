// Private, nonvisible D3D11 depth snapshot fixture. No game or input devices.
#include "../renderer/d3d11/game/contracts.hpp"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <memory>

using namespace a2fo::renderer::game;
namespace {
template<class T> struct Ref {T* p=nullptr;~Ref(){if(p)p->Release();} T** out(){return &p;} T* operator->()const{return p;}};
void need(bool yes,const char* name){if(!yes){std::fprintf(stderr,"FAIL %s\n",name);std::exit(1);}}
void ok(HRESULT hr,const char* name){if(FAILED(hr)){std::fprintf(stderr,"FAIL %s 0x%08lx\n",name,static_cast<unsigned long>(hr));std::exit(1);}}
struct Window {HWND h=nullptr;HINSTANCE instance=nullptr;~Window(){if(h)DestroyWindow(h);UnregisterClassA("A2FODepthSnapshotFixture",instance);}};
void compile(const char* code,const char* entry,const char* profile,ID3DBlob** out){
    Ref<ID3DBlob> errors;
    HRESULT result=D3DCompile(code,std::strlen(code),nullptr,nullptr,nullptr,entry,profile,
                              D3DCOMPILE_ENABLE_STRICTNESS,0,out,errors.out());
    if(FAILED(result)&&errors.p)std::fprintf(stderr,"shader: %s\n",static_cast<const char*>(errors->GetBufferPointer()));
    ok(result,"D3DCompile");
}
struct Gpu {
    ID3D11Device* device;Ref<ID3D11DeviceContext> context;
    Ref<ID3D11Texture2D> color,staging;
    Ref<ID3D11RenderTargetView> rtv;
    Ref<ID3D11Buffer> uav_buffer;
    Ref<ID3D11UnorderedAccessView> uav;
    Ref<ID3D11VertexShader> vs;
    Ref<ID3D11RasterizerState> raster;
    Ref<ID3D11PixelShader> red,green,blue,sample;
    Ref<ID3D11DepthStencilState> depth_stencil,depth_only,stencil_test,depth_off;
    explicit Gpu(ID3D11Device* d):device(d){
        device->GetImmediateContext(context.out());
        D3D11_TEXTURE2D_DESC desc{};desc.Width=desc.Height=32;desc.MipLevels=desc.ArraySize=1;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.SampleDesc.Count=1;
        desc.Usage=D3D11_USAGE_DEFAULT;desc.BindFlags=D3D11_BIND_RENDER_TARGET;
        ok(device->CreateTexture2D(&desc,nullptr,color.out()),"color texture");
        ok(device->CreateRenderTargetView(color.p,nullptr,rtv.out()),"color RTV");
        D3D11_BUFFER_DESC buffer{};buffer.ByteWidth=16;buffer.Usage=D3D11_USAGE_DEFAULT;
        buffer.BindFlags=D3D11_BIND_UNORDERED_ACCESS;buffer.MiscFlags=D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        buffer.StructureByteStride=4;
        ok(device->CreateBuffer(&buffer,nullptr,uav_buffer.out()),"OM UAV buffer");
        D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
        uav_desc.Format=DXGI_FORMAT_UNKNOWN;uav_desc.ViewDimension=D3D11_UAV_DIMENSION_BUFFER;
        uav_desc.Buffer.NumElements=4;
        ok(device->CreateUnorderedAccessView(uav_buffer.p,&uav_desc,uav.out()),"OM UAV view");
        D3D11_RASTERIZER_DESC rs{};rs.FillMode=D3D11_FILL_SOLID;rs.CullMode=D3D11_CULL_NONE;
        rs.DepthClipEnable=TRUE;
        ok(device->CreateRasterizerState(&rs,raster.out()),"rasterizer");
        context->RSSetState(raster.p);
        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ok(device->CreateTexture2D(&desc,nullptr,staging.out()),"color staging");
        const char* vsrc="float4 main(uint id:SV_VertexID):SV_Position { float2 p=id==0?float2(-1,-1):id==1?float2(3,-1):float2(-1,3);return float4(p,0,1);}";
        const char* psrc="struct O{float4 c:SV_Target;float d:SV_Depth;}; O main(){O o;o.c=float4(1,0,0,1);o.d=.2;return o;}";
        const char* gsrc="struct O{float4 c:SV_Target;float d:SV_Depth;}; O main(){O o;o.c=float4(0,1,0,1);o.d=.6;return o;}";
        const char* bsrc="float4 main():SV_Target{return float4(0,0,1,1);}";
        const char* ssrc="Texture2D<float> z:register(t0);float4 main(float4 p:SV_Position):SV_Target{return float4(z.Load(int3(int2(p.xy),0)),0,0,1);}";
        Ref<ID3DBlob> blob;compile(vsrc,"main","vs_5_0",blob.out());
        ok(device->CreateVertexShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,vs.out()),"VS");
        blob.p->Release();blob.p=nullptr;
        compile(psrc,"main","ps_5_0",blob.out());ok(device->CreatePixelShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,red.out()),"red PS");
        blob.p->Release();blob.p=nullptr;
        compile(gsrc,"main","ps_5_0",blob.out());ok(device->CreatePixelShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,green.out()),"green PS");
        blob.p->Release();blob.p=nullptr;
        compile(bsrc,"main","ps_5_0",blob.out());ok(device->CreatePixelShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,blue.out()),"blue PS");
        blob.p->Release();blob.p=nullptr;
        compile(ssrc,"main","ps_5_0",blob.out());ok(device->CreatePixelShader(blob->GetBufferPointer(),blob->GetBufferSize(),nullptr,sample.out()),"sample PS");
        D3D11_DEPTH_STENCIL_DESC state{};state.DepthEnable=TRUE;state.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ALL;
        state.DepthFunc=D3D11_COMPARISON_LESS;state.StencilEnable=TRUE;
        state.StencilReadMask=state.StencilWriteMask=0xff;
        state.FrontFace.StencilFunc=D3D11_COMPARISON_ALWAYS;state.FrontFace.StencilPassOp=D3D11_STENCIL_OP_REPLACE;
        state.BackFace=state.FrontFace;
        ok(device->CreateDepthStencilState(&state,depth_stencil.out()),"depth stencil state");
        state.StencilEnable=FALSE;
        ok(device->CreateDepthStencilState(&state,depth_only.out()),"depth only state");
        state.StencilEnable=TRUE;
        state.DepthEnable=FALSE;state.DepthWriteMask=D3D11_DEPTH_WRITE_MASK_ZERO;
        state.FrontFace.StencilFunc=D3D11_COMPARISON_EQUAL;state.FrontFace.StencilPassOp=D3D11_STENCIL_OP_KEEP;
        state.BackFace=state.FrontFace;
        ok(device->CreateDepthStencilState(&state,stencil_test.out()),"stencil test state");
        state.StencilEnable=FALSE;
        ok(device->CreateDepthStencilState(&state,depth_off.out()),"depth off state");
    }
    void viewport(float x,float y,float w,float h){D3D11_VIEWPORT v{};v.TopLeftX=x;v.TopLeftY=y;v.Width=w;v.Height=h;v.MaxDepth=1;context->RSSetViewports(1,&v);}
    void draw(ID3D11PixelShader* ps){context->VSSetShader(vs.p,nullptr,0);context->PSSetShader(ps,nullptr,0);
        context->IASetInputLayout(nullptr);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        context->Draw(3,0);}
    unsigned char channel_at(unsigned x,unsigned y,unsigned channel){
        context->CopyResource(staging.p,color.p);D3D11_MAPPED_SUBRESOURCE map{};
        ok(context->Map(staging.p,0,D3D11_MAP_READ,0,&map),"Map staging");
        const auto value=static_cast<const unsigned char*>(map.pData)[y*map.RowPitch+x*4+channel];
        context->Unmap(staging.p,0);return value;
    }
};
void run(UINT legacy,bool stencil){
    WNDCLASSEXA cls{};cls.cbSize=sizeof(cls);cls.hInstance=GetModuleHandleA(nullptr);
    cls.lpfnWndProc=DefWindowProcA;cls.lpszClassName="A2FODepthSnapshotFixture";
    need(RegisterClassExA(&cls)!=0,"register window");Window window;window.instance=cls.hInstance;
    window.h=CreateWindowExA(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,cls.lpszClassName,"private depth fixture",
        WS_POPUP,0,0,32,32,nullptr,nullptr,cls.hInstance,nullptr);
    need(window.h&&!IsWindowVisible(window.h),"hidden window");
    Backend backend;ok(backend.initialize(window.h,32,32,true,false,false,".",nullptr,legacy),"backend init");
    Gpu gpu(backend.device());
    std::shared_ptr<Target> source;ok(backend.create_target(32,32,legacy,true,source),"source depth");
    need(source!=nullptr,"source depth exists");
    std::shared_ptr<Texture> rejected;
    need(backend.snapshot_depth(nullptr,rejected)==invalid_call && !rejected,"reject null");
    need(backend.snapshot_depth(backend.backbuffer(),rejected)==invalid_call && !rejected,"reject color");
    Ref<ID3D11DepthStencilView> dsv;gpu.context->OMSetRenderTargets(1,&gpu.rtv.p,nullptr);
    // Retrieve the DSV from the public bind operation, then keep it for direct draws.
    ok(backend.bind_target(backend.backbuffer(),source),"bind source");
    gpu.context->OMGetRenderTargets(0,nullptr,dsv.out());need(dsv.p!=nullptr,"source DSV");
    gpu.context->OMSetRenderTargetsAndUnorderedAccessViews(1,&gpu.rtv.p,dsv.p,1,1,&gpu.uav.p,nullptr);
    gpu.context->ClearDepthStencilView(dsv.p,D3D11_CLEAR_DEPTH|(stencil?D3D11_CLEAR_STENCIL:0),.8f,7);
    const float black[4]{};gpu.context->ClearRenderTargetView(gpu.rtv.p,black);
    gpu.context->OMSetDepthStencilState(stencil?gpu.depth_stencil.p:gpu.depth_only.p,3);
    gpu.viewport(8,8,8,12);gpu.draw(gpu.red.p);
    Ref<ID3D11RenderTargetView> before_rtv;Ref<ID3D11DepthStencilView> before_dsv;
    Ref<ID3D11UnorderedAccessView> before_uav;
    Ref<ID3D11DepthStencilState> before_state;UINT before_ref=0;
    gpu.context->OMGetDepthStencilState(before_state.out(),&before_ref);
    gpu.context->OMGetRenderTargetsAndUnorderedAccessViews(1,before_rtv.out(),before_dsv.out(),1,1,before_uav.out());
    need(before_uav.p==gpu.uav.p,"OM UAV bound before snapshot");
    std::shared_ptr<Texture> first,second;
    ok(backend.snapshot_depth(source,first),"first snapshot");
    need(first!=nullptr,"first snapshot exists");
    const auto retained=first;
    need(backend.snapshot_depth(nullptr,first)==invalid_call&&first==retained,
         "failed snapshot preserves previous output");
    Ref<ID3D11RenderTargetView> after_rtv;Ref<ID3D11DepthStencilView> after_dsv;
    Ref<ID3D11UnorderedAccessView> after_uav;
    gpu.context->OMGetRenderTargetsAndUnorderedAccessViews(1,after_rtv.out(),after_dsv.out(),1,1,after_uav.out());
    need(before_rtv.p==after_rtv.p&&before_dsv.p==after_dsv.p,"OM bindings preserved");
    need(before_uav.p==after_uav.p,"OM UAV preserved");
    Ref<ID3D11DepthStencilState> after_state;UINT after_ref=0;
    gpu.context->OMGetDepthStencilState(after_state.out(),&after_ref);
    need(before_state.p==after_state.p&&before_ref==after_ref,"OM depth stencil state preserved");
    gpu.context->OMSetDepthStencilState(gpu.depth_only.p,0);
    gpu.viewport(0,0,32,32);gpu.draw(gpu.green.p);
    need(gpu.channel_at(10,10,0)>240,"near occluder survives farther depth draw");
    need(gpu.channel_at(2,2,1)>240,"far draw outside occluder");
    if(stencil){gpu.context->OMSetDepthStencilState(gpu.stencil_test.p,3);gpu.draw(gpu.blue.p);
        need(gpu.channel_at(10,10,2)>240&&gpu.channel_at(2,2,1)>240,"stencil test draw");}
    ok(backend.snapshot_depth(source,second),"second snapshot");
    gpu.context->ClearDepthStencilView(dsv.p,D3D11_CLEAR_DEPTH|(stencil?D3D11_CLEAR_STENCIL:0),.1f,0);
    auto sample=[&](Texture& texture,unsigned x,unsigned y){
        Ref<ID3D11ShaderResourceView> srv;ok(backend.texture_view(texture,srv.out()),"snapshot SRV");
        gpu.context->OMSetRenderTargets(1,&gpu.rtv.p,nullptr);
        gpu.context->OMSetDepthStencilState(gpu.depth_off.p,0);
        gpu.context->PSSetShaderResources(0,1,&srv.p);gpu.viewport(0,0,32,32);gpu.draw(gpu.sample.p);
        const unsigned char value=gpu.channel_at(x,y,0);
        ID3D11ShaderResourceView* empty=nullptr;gpu.context->PSSetShaderResources(0,1,&empty);
        return value;
    };
    const auto near_first=sample(*first,10,10),far_first=sample(*first,2,2);
    need(std::abs(int(near_first)-51)<=2&&std::abs(int(far_first)-204)<=2,"first sampled depth values");
    need(std::abs(int(sample(*second,10,10))-51)<=2,"second independent snapshot");
    need(backend.update_texture(*first,0,MipUpload{})==invalid_call,"snapshot immutable upload");
    std::shared_ptr<Target> invalid_target;
    need(backend.texture_target(*first,invalid_target)==invalid_call,"snapshot not render target");
    if(legacy==80){
        Backend no_auto;
        ok(no_auto.initialize(window.h,32,32,true,false,false,".",nullptr,0),"no-auto backend init");
        need(!no_auto.depthbuffer(),"no-auto has no depth target");
        std::shared_ptr<Texture> absent;
        need(no_auto.snapshot_depth(no_auto.depthbuffer(),absent)==invalid_call&&!absent,
             "no-auto snapshot rejected");
        need(no_auto.snapshot_depth(source,absent)==invalid_call&&!absent,
             "foreign-device depth rejected");
    }
    std::printf("PASS legacy=%u near=%u far=%u stencil=%u\n",legacy,near_first,far_first,stencil);
}
}
int main(){run(80,false);run(75,true);run(71,false);return 0;}
