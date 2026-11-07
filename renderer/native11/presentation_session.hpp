#pragma once
#include "frame_bundle.hpp"
#include "primitive_service.hpp"
#include "../../sdk/include/a2fo_native11_presentation_api.h"
#include <windows.h>
#include <memory>
#include <string>

namespace a2fo::native11 {
enum class PresentationResult { fallback, presented, occluded };
using SourceCurrent = bool (*)(void*,const FrameIdentity&) noexcept;

// Externally serialized. Explicit close on the creating thread precedes DLL unload.
// The SourceCurrent callback and its context must remain valid until close,
// including while current()/accepted_generation() revalidate publication.
class PresentationSession final {
public:
    PresentationSession()=default;
    PresentationSession(const PresentationSession&)=delete;
    PresentationSession& operator=(const PresentationSession&)=delete;
    ~PresentationSession()=default;
    bool initialize(const A2FO_N11_API8&,std::uint32_t width,std::uint32_t height,HWND,std::string&) noexcept;
    PresentationResult render(std::shared_ptr<const FrameBundle>,SourceCurrent,void*,std::string&) noexcept;
    bool resize(std::uint32_t width,std::uint32_t height,HWND,std::string&) noexcept;
    bool close(std::string&) noexcept;
    std::shared_ptr<const FrameBundle> current() const noexcept;
    std::uint64_t accepted_generation() const noexcept;
private:
    bool available() const noexcept;
    static std::int32_t A2FO_N11_CALL unused_asset(void*,std::uint32_t,const std::uint8_t*,std::uint32_t,std::uint32_t,A2FO_N11_BytesSink,void*) noexcept;
    A2FO_N11_API8 api_{};
    A2FO_N11_Session session_=0;
    DWORD thread_=0;
    std::uint32_t width_=0,height_=0;
    std::uint64_t serial_=0,accepted_=0;
    mutable bool busy_=false;
    bool attached_=false;
    mutable std::shared_ptr<const FrameBundle> current_;
    mutable SourceCurrent source_current_=nullptr;
    mutable void* source_context_=nullptr;
};
}
