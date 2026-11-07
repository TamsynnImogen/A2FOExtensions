#pragma once
#include "presentation_session.hpp"
#include "world_session.hpp"

namespace a2fo::native11 {
struct GameplayFrame {
    FrameIdentity identity{};
    std::uint32_t width=0,height=0;
    std::shared_ptr<const OwnedWorldFrame> world;
    std::uint32_t world_domains=0,absent_domains=0;
    bool complete_geometry=false;
    std::vector<FramePass> overlays;
};
// One API8 session: complete owned world, optional primitive overlays, present.
// Caller serializes calls and retains the DLL/HWND/SourceCurrent context to close.
class GameplayRenderSession final {
public:
    GameplayRenderSession()=default;
    GameplayRenderSession(const GameplayRenderSession&)=delete;
    GameplayRenderSession& operator=(const GameplayRenderSession&)=delete;
    GameplayRenderSession(GameplayRenderSession&&)=delete;
    GameplayRenderSession& operator=(GameplayRenderSession&&)=delete;
    ~GameplayRenderSession()=default;
    bool initialize(const A2FO_N11_API8&,std::uint32_t,std::uint32_t,HWND,std::string&) noexcept;
    bool initialize_all_parts(const A2FO_N11_API9&,std::uint32_t,std::uint32_t,HWND,
                              std::string&) noexcept;
    PresentationResult render(std::shared_ptr<const GameplayFrame>,SourceCurrent,void*,std::string&) noexcept;
    bool resize(std::uint32_t,std::uint32_t,HWND,std::string&) noexcept;
    bool readback(std::vector<std::uint8_t>& bgra,std::uint32_t& width,
                  std::uint32_t& height,std::string&) noexcept;
    bool close(std::string&) noexcept;
    std::shared_ptr<const GameplayFrame> current() const noexcept;
    std::uint64_t accepted_generation() const noexcept;
private:
    bool available() const noexcept;
    static bool checkpoint(void*) noexcept;
    static std::int32_t A2FO_N11_CALL read_asset(void*,std::uint32_t,const std::uint8_t*,
        std::uint32_t,std::uint32_t,A2FO_N11_BytesSink,void*) noexcept;
    static std::int32_t A2FO_N11_CALL resolve_texture(void*,const std::uint8_t*,
        std::uint32_t,std::uint32_t,const std::uint8_t*,std::uint32_t,std::uint32_t,
        A2FO_N11_BytesSink,void*) noexcept;
    static std::int32_t A2FO_N11_CALL resolve_part_state(void*,const std::uint8_t*,
        std::uint32_t,std::uint32_t,const float[16],A2FO_N11_PartStateSink,void*) noexcept;
    A2FO_N11_API8 api_{};
    A2FO_N11_Session session_=0;
    DWORD thread_=0;
    std::uint32_t width_=0,height_=0;
    std::uint64_t serial_=0,accepted_=0;
    mutable bool busy_=false;
    bool attached_=false;
    std::shared_ptr<const OwnedWorldFrame> pending_world_;
    mutable std::shared_ptr<const GameplayFrame> current_;
    std::shared_ptr<const OwnedWorldFrame> submitted_world_;
    std::shared_ptr<const PrimitiveFrame> submitted_overlay_;
    mutable SourceCurrent source_current_=nullptr;
    mutable void* source_context_=nullptr;
    FrameIdentity checking_{};
};
} // namespace a2fo::native11
