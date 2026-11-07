#pragma once
#include "material_stream.hpp"
#include "world_coverage.hpp"

namespace a2fo::native11 {
struct DomainEvidence {
    // True means the host has certified this other caster domain ABSENT.
    // Present non-GameObject geometry has no representation in this assembler.
    // No absence is inferred from the checked GameObject list.
    bool no_game_features=false;
    bool no_background_casters=false;
    bool no_effect_casters=false;
};

// Externally serialized. Inputs and their immutable owners must remain stable
// during update. Only the latest admitted frame owns geometry after return.
class ObservedWorldAssembler final {
public:
    bool update(std::shared_ptr<const BoundMaterialCapture> visible,
        const WorldLightCapture& world,
        const std::vector<std::shared_ptr<const BoundMaterialCapture>>& shadow_candidates,
        DomainEvidence domains,std::uint64_t fresh_epoch,std::string& error) noexcept;
    std::shared_ptr<const BoundMaterialCapture> current() const noexcept {return stream_.current();}
    // Updated after a valid audit even if coverage or domain proof blocks publish.
    const CoverageReport& report() const noexcept {return report_;}
    // Drops the current owner and report; the stream's fresh-epoch guard remains.
    bool clear() noexcept;
private:
    MaterialFrameStream stream_;
    CoverageReport report_;
};
} // namespace a2fo::native11
