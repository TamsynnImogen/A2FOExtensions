#include "probe_bridge.hpp"
#include "probe_device.hpp"
#include "../../rust/d3d_resources_ffi.h"

#include <exception>

namespace a2fo::renderer {
HRESULT run_d3d11_window_probe(HWND window, UINT width, UINT height,
                             bool debug_layer,
                             void (*log)(const std::string&)) {
    ProbeDevice device;
    struct Context {
        ProbeDevice& device;
        HWND window;
        UINT width,height;
        bool debug_layer;
        void (*log)(const std::string&);
        std::exception_ptr exception;
    } context{device,window,width,height,debug_layer,log,{}};
    auto call=[](void* raw,std::uint32_t operation,const void* argument,
                 A2FoD3DReply* reply) noexcept -> int {
        auto& c=*static_cast<Context*>(raw);
        try {
            switch(operation) {
            case A2FO_D3D_PROBE_INIT:
                reply->hr=c.device.initialize(c.window,c.width,c.height,c.debug_layer,c.log);
                break;
            case A2FO_D3D_GET_ENV_PATH: {
                auto& buffer=*static_cast<const A2FoD3DPathBuffer*>(argument);
                reply->value=GetEnvironmentVariableA("LOCALAPPDATA",
                    reinterpret_cast<char*>(buffer.bytes),static_cast<DWORD>(buffer.capacity));
                break;
            }
            case A2FO_D3D_GET_TEMP_PATH: {
                auto& buffer=*static_cast<const A2FoD3DPathBuffer*>(argument);
                reply->value=GetTempPathA(static_cast<DWORD>(buffer.capacity),
                    reinterpret_cast<char*>(buffer.bytes));
                break;
            }
            case A2FO_D3D_CREATE_DIRECTORY: {
                const auto& path=*static_cast<const A2FoD3DPath*>(argument);
                const std::string name(reinterpret_cast<const char*>(path.bytes),path.length);
                reply->value=CreateDirectoryA(name.c_str(),nullptr)!=0;
                if(!reply->value)reply->hr=GetLastError();
                break;
            }
            case A2FO_D3D_PROBE_TRIANGLE: {
                const auto& path=*static_cast<const A2FoD3DPath*>(argument);
                reply->hr=c.device.present_test_triangle(
                    std::string(reinterpret_cast<const char*>(path.bytes),path.length));
                break;
            }
            default:return 0;
            }
            return 1;
        }catch(...){c.exception=std::current_exception();return 0;}
    };
    std::int32_t result=E_FAIL;
    const int called=a2fo_rs_d3d_probe_bridge(ERROR_ALREADY_EXISTS,
        &context,call,&result);
    if(context.exception)std::rethrow_exception(context.exception);
    return called?result:E_FAIL;
}
}
