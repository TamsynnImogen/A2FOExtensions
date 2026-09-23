#pragma once

namespace a2fo {

// A cloak controller is active through both transition directions. Keep
// flat-normal lighting until the unit is fully visible again, without
// changing shared mesh/texture flags or the persisted global bump option.
constexpr bool renderer_flat_normal_for_draw(
    bool global_flat_normal, bool neutral_bump_when_cloaked,
    unsigned long cloak_state) noexcept {
    return global_flat_normal ||
        (neutral_bump_when_cloaked &&
         cloak_state >= 1 && cloak_state <= 3);
}

// The ordinary fixed-function path can only add emissive maps. DOT3 draws can
// additionally add the extension's specular overlay. Hooks still balance
// their pre/post state stacks when this returns false, but skip all device and
// texture discovery work.
constexpr bool renderer_extension_draw_required(
    bool dot3_draw, bool emissive_maps_enabled,
    bool specular_maps_enabled) noexcept {
    return emissive_maps_enabled ||
        (dot3_draw && specular_maps_enabled);
}

// Storm3D normally sends every material through its CPU triangle sorter while
// an otherwise opaque model is participating in a global alpha transition
// (cloak, decloak, construction fade, and similar effects). The fast non-bump
// compatibility route (global or scoped to a cloaked craft) may instead draw
// that whole opaque material through its
// existing MeshVB after applying Storm3D's own z-sort blend state immediately.
// Mode 1 also admits order-independent additive materials; mode 2 deliberately
// admits every blend mode as a faster, potentially order-inexact test path.
constexpr bool renderer_fast_nonbump_alpha_meshvb_allowed(
    bool fast_nonbump_enabled, bool renderer_active,
    bool fast_shader_rejected, bool external_renderer,
    unsigned long destination_blend, unsigned long policy_mode) noexcept {
    constexpr unsigned long kOpaqueDestinationBlend = 1;  // D3DBLEND_ZERO
    constexpr unsigned long kAdditiveDestinationBlend = 2;  // D3DBLEND_ONE
    return fast_nonbump_enabled && renderer_active &&
        !fast_shader_rejected && !external_renderer &&
        policy_mode != 0 &&
        (policy_mode >= 2 ||
         destination_blend == kOpaqueDestinationBlend ||
         destination_blend == kAdditiveDestinationBlend);
}

}  // namespace a2fo
