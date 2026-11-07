#pragma once
#include "../../sdk/include/a2fo_native11_api.h"
#include "scene_capture.hpp"
#include "scene.hpp"

namespace a2fo::native11 {
std::int32_t submit_captured(const A2FO_N11_API4&,A2FO_N11_Session,const Frame&,
    std::uint64_t source_epoch,const Light*,std::uint32_t light_count,
    const float ambient[3]) noexcept;
// Synchronous bridge for an owned capture. It neither loads the service nor
// resolves assets or presents. No engine address crosses the public DLL ABI.
// The caller owns session lifetime and authoritative source/lighting selection.
std::int32_t submit_captured(const A2FO_N11_API&,A2FO_N11_Session,const Frame&,
    std::uint64_t source_epoch,const Light*,std::uint32_t light_count,
    const float ambient[3]) noexcept;
std::int32_t submit_captured(const A2FO_N11_API2&,A2FO_N11_Session,const Frame&,
    std::uint64_t source_epoch,const Light*,std::uint32_t light_count,
    const float ambient[3]) noexcept;
std::int32_t submit_captured(const A2FO_N11_API3&,A2FO_N11_Session,const Frame&,
    std::uint64_t source_epoch,const Light*,std::uint32_t light_count,
    const float ambient[3]) noexcept;
}
