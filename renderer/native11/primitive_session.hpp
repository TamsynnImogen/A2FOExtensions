#pragma once
#include "primitive_service.hpp"
namespace a2fo::native11 {
// Externally serialized API7 client. Keep its DLL loaded and close on the
// creating thread before destruction/unload; destructor does no GPU work.
// Published packets and all payloads must have no mutable aliases.
class PrimitiveRenderSession final {
public:
    PrimitiveRenderSession()=default;
    PrimitiveRenderSession(const PrimitiveRenderSession&)=delete;
    PrimitiveRenderSession& operator=(const PrimitiveRenderSession&)=delete;
    ~PrimitiveRenderSession()=default;
    bool initialize(const A2FO_N11_API7&,std::uint32_t width,std::uint32_t height,std::string&) noexcept;
    bool render(std::shared_ptr<const PrimitiveFrame>,std::string&) noexcept;
    bool resize(std::uint32_t width,std::uint32_t height,std::string&) noexcept;
    bool readback(std::vector<std::uint8_t>&,std::uint32_t& width,std::uint32_t& height,std::string&) noexcept;
    bool usage(A2FO_N11_Usage&,std::string&) noexcept;
    bool close(std::string&) noexcept;
    std::shared_ptr<const PrimitiveFrame> current() const noexcept;
    std::uint64_t accepted_generation() const noexcept;
private:
    bool available() const noexcept;
    static std::int32_t A2FO_N11_CALL unused_asset(void*,std::uint32_t,const std::uint8_t*,std::uint32_t,std::uint32_t,A2FO_N11_BytesSink,void*) noexcept;
    static void A2FO_N11_CALL log(void*,const char*,std::uint32_t) noexcept;
    A2FO_N11_API7 api_{};
    A2FO_N11_Session session_=0;
    std::uint32_t thread_=0,width_=0,height_=0;
    bool busy_=false,image_eligible_=false;
    std::uint64_t generation_=0;
    std::shared_ptr<const PrimitiveFrame> current_;
    std::string last_log_;
};
}
