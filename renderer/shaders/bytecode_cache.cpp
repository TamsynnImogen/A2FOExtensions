#include "bytecode_cache.hpp"
#include "../../rust/render_cache_state_ffi.h"

#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#include <new>

namespace a2fo::renderer {
namespace {
struct Module {HMODULE value=nullptr;~Module(){if(value)FreeLibrary(value);}};
struct File {HANDLE value=INVALID_HANDLE_VALUE;~File(){if(value!=INVALID_HANDLE_VALUE)CloseHandle(value);}};
struct TemporaryFile {
    std::string path;
    HANDLE file=INVALID_HANDLE_VALUE;
    bool committed=false;
    ~TemporaryFile(){if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);
        if(!committed&&!path.empty())DeleteFileA(path.c_str());}
};
struct Blob {ID3DBlob* value=nullptr;~Blob(){if(value)value->Release();}};
using CompileFn=HRESULT (WINAPI *)(LPCVOID,SIZE_T,LPCSTR,const D3D_SHADER_MACRO*,
    ID3DInclude*,LPCSTR,LPCSTR,UINT,UINT,ID3DBlob**,ID3DBlob**);

std::string span_string(A2FoRenderSpan span) {
    return span.size?std::string(reinterpret_cast<const char*>(span.data),span.size):std::string{};
}
bool read_all(HANDLE file,void* data,DWORD size) {
    DWORD done=0;return ReadFile(file,data,size,&done,nullptr)&&done==size;
}
bool write_all(HANDLE file,const void* data,DWORD size) {
    DWORD done=0;return WriteFile(file,data,size,&done,nullptr)&&done==size;
}
struct Context {
    const ShaderCompileRequest& request;
    ShaderCompileResult& result;
    Module module;
    File read_file;
    TemporaryFile temp;
    Blob shader,errors;
    CompileFn compile=nullptr;
    char module_path[32768]{};
    std::exception_ptr exception;
};
int shader_call(void* raw,std::uint32_t operation,A2FoRenderSpan input,
                A2FoShaderReply* reply) noexcept {
    auto& c=*static_cast<Context*>(raw);
    try {
        switch(operation) {
        case A2FO_SHADER_LOAD_47:
        case A2FO_SHADER_LOAD_43:
            c.module.value=LoadLibraryA(operation==A2FO_SHADER_LOAD_47?
                "d3dcompiler_47.dll":"d3dcompiler_43.dll");
            reply->value=c.module.value!=nullptr;
            if(!reply->value&&operation==A2FO_SHADER_LOAD_43)
                reply->hr=HRESULT_FROM_WIN32(GetLastError());
            break;
        case A2FO_SHADER_GET_PROC: {
            const FARPROC symbol=GetProcAddress(c.module.value,"D3DCompile");
            static_assert(sizeof(c.compile)==sizeof(symbol));
            std::memcpy(&c.compile,&symbol,sizeof(c.compile));
            reply->value=c.compile!=nullptr;
            if(!reply->value)reply->hr=HRESULT_FROM_WIN32(GetLastError());
            break;
        }
        case A2FO_SHADER_IDENTITY: {
            const DWORD length=GetModuleFileNameA(c.module.value,c.module_path,
                                                   sizeof(c.module_path));
            if(!length||length>=sizeof(c.module_path))break;
            WIN32_FILE_ATTRIBUTE_DATA attributes{};
            if(!GetFileAttributesExA(c.module_path,GetFileExInfoStandard,&attributes))break;
            reply->value=1;
            reply->first={reinterpret_cast<const std::uint8_t*>(c.module_path),length};
            reply->file_size=(std::uint64_t(attributes.nFileSizeHigh)<<32)|
                attributes.nFileSizeLow;
            reply->file_time=(std::uint64_t(attributes.ftLastWriteTime.dwHighDateTime)<<32)|
                attributes.ftLastWriteTime.dwLowDateTime;
            break;
        }
        case A2FO_SHADER_OPEN_READ: {
            const std::string path=span_string(input);
            c.read_file.value=CreateFileA(path.c_str(),GENERIC_READ,
                FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,nullptr);
            reply->value=c.read_file.value!=INVALID_HANDLE_VALUE;break;
        }
        case A2FO_SHADER_FILE_SIZE: {
            LARGE_INTEGER size{};
            reply->value=GetFileSizeEx(c.read_file.value,&size)!=0;
            reply->size64=size.QuadPart;break;
        }
        case A2FO_SHADER_READ:
            reply->value=read_all(c.read_file.value,
                const_cast<std::uint8_t*>(input.data),static_cast<DWORD>(input.size));break;
        case A2FO_SHADER_CLOSE_READ:
            if(c.read_file.value!=INVALID_HANDLE_VALUE) {
                CloseHandle(c.read_file.value);c.read_file.value=INVALID_HANDLE_VALUE;
            }break;
        case A2FO_SHADER_COMPILE: {
            LARGE_INTEGER start{},end{},frequency{};
            QueryPerformanceFrequency(&frequency);
            QueryPerformanceCounter(&start);
            reply->hr=c.compile(c.request.source.data(),c.request.source.size(),nullptr,
                nullptr,nullptr,c.request.entry.c_str(),c.request.target.c_str(),
                c.request.flags,0,&c.shader.value,&c.errors.value);
            QueryPerformanceCounter(&end);
            reply->clock_start=start.QuadPart;reply->clock_end=end.QuadPart;
            reply->frequency=frequency.QuadPart;
            if(c.errors.value)reply->second={
                static_cast<const std::uint8_t*>(c.errors.value->GetBufferPointer()),
                c.errors.value->GetBufferSize()};
            if(c.shader.value) {
                reply->value=1;
                reply->first={static_cast<const std::uint8_t*>(
                    c.shader.value->GetBufferPointer()),c.shader.value->GetBufferSize()};
            }break;
        }
        case A2FO_SHADER_CREATE_DIRECTORY: {
            const std::string path=span_string(input);
            reply->value=CreateDirectoryA(path.c_str(),nullptr)!=0;
            if(!reply->value)reply->hr=GetLastError();
            break;
        }
        case A2FO_SHADER_SUFFIX: {
            static volatile LONG serial=0;
            const LONG sequence=InterlockedIncrement(&serial);
            const DWORD thread=GetCurrentThreadId();
            const DWORD process=GetCurrentProcessId();
            reply->value=process;reply->extra=thread;
            reply->size64=sequence;break;
        }
        case A2FO_SHADER_OPEN_TEMP:
            c.temp.path=span_string(input);
            c.temp.file=CreateFileA(c.temp.path.c_str(),GENERIC_WRITE,0,nullptr,
                CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
            reply->value=c.temp.file!=INVALID_HANDLE_VALUE;
            if(!reply->value)c.temp.committed=true;
            break;
        case A2FO_SHADER_WRITE:
            reply->value=write_all(c.temp.file,input.data,
                static_cast<DWORD>(input.size));break;
        case A2FO_SHADER_FLUSH:
            reply->value=FlushFileBuffers(c.temp.file)!=0;break;
        case A2FO_SHADER_CLOSE_TEMP:
            if(c.temp.file!=INVALID_HANDLE_VALUE){CloseHandle(c.temp.file);
                c.temp.file=INVALID_HANDLE_VALUE;}break;
        case A2FO_SHADER_MOVE: {
            const std::string path=span_string(input);
            reply->value=MoveFileExA(c.temp.path.c_str(),path.c_str(),
                MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
            c.temp.committed=reply->value!=0;break;
        }
        case A2FO_SHADER_DELETE:
            DeleteFileA(c.temp.path.c_str());c.temp.committed=true;break;
        case A2FO_SHADER_PUBLISH: {
            if(input.size!=sizeof(A2FoShaderPublish))return 0;
            const auto& data=*reinterpret_cast<const A2FoShaderPublish*>(input.data);
            c.result.bytes.clear();
            if(data.bytes.size)c.result.bytes.assign(data.bytes.data,
                data.bytes.data+data.bytes.size);
            c.result.diagnostics=span_string(data.diagnostics);
            c.result.compile_us=data.compile_us;
            c.result.cache_hit=data.cache_hit!=0;
            c.result.cache_written=data.cache_written!=0;
            break;
        }
        default:return 0;
        }
        return 1;
    }catch(...){c.exception=std::current_exception();return 0;}
}
}
HRESULT compile_shader(const ShaderCompileRequest& request,
                       const std::string& cache_directory,
                       ShaderCompileResult& result) {
    try {
        result={};
        Context context{request,result,{},{},{},{},{},nullptr,{},{}};
        const A2FoShaderInput input{
            {reinterpret_cast<const std::uint8_t*>(request.source.data()),request.source.size()},
            {reinterpret_cast<const std::uint8_t*>(request.entry.data()),request.entry.size()},
            {reinterpret_cast<const std::uint8_t*>(request.target.data()),request.target.size()},
            {reinterpret_cast<const std::uint8_t*>(cache_directory.data()),cache_directory.size()},
            request.flags,E_INVALIDARG,E_FAIL,E_OUTOFMEMORY,ERROR_ALREADY_EXISTS,
            std::numeric_limits<std::vector<std::uint8_t>::size_type>::max()};
        const HRESULT status=a2fo_rs_shader_compile(&input,&context,shader_call);
        if(context.exception)std::rethrow_exception(context.exception);
        return status;
    }catch(const std::bad_alloc&){result={};return E_OUTOFMEMORY;}
    catch(...){result={};return E_FAIL;}
}
} // namespace a2fo::renderer
