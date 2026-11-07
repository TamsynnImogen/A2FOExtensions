#pragma once
#include "presentation_session.hpp"
#include <string>
namespace a2fo::native11 {
// Owns a single explicit native-service DLL and its API8 presentation session.
// Call shutdown on the creating thread; destruction performs no GPU or DLL work.
class NativeStartup final {
public:
    NativeStartup()=default;
    NativeStartup(const NativeStartup&)=delete;
    NativeStartup& operator=(const NativeStartup&)=delete;
    ~NativeStartup()=default;
    bool initialize(const std::wstring& absolute_dll,std::uint32_t width,std::uint32_t height,HWND,std::string&) noexcept;
    PresentationResult render(std::shared_ptr<const FrameBundle>,SourceCurrent,void*,std::string&) noexcept;
    bool resize(std::uint32_t width,std::uint32_t height,HWND,std::string&) noexcept;
    bool shutdown(std::string&) noexcept;
    std::shared_ptr<const FrameBundle> current() const noexcept;
    std::uint64_t accepted_generation() const noexcept;
private:
    HMODULE dll_=nullptr;
    bool initialized_=false;
    PresentationSession session_;
};
}
