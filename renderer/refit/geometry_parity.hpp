#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::refit {
inline constexpr double geometry_xy_tolerance2=1.0/256.0;
inline constexpr double geometry_z_tolerance2=1e-6;
inline constexpr double geometry_uv_tolerance2=1e-6;
inline constexpr double geometry_rhw_relative_tolerance2=1e-6;
// Screen-space triangle area (px^2) below which facing is numerically
// indeterminate between the engine's float RHW path and this double-precision
// projection, and which covers no sample either way. Live rom_talon slivers
// were 3.4e-5..9.5e-5 px^2 (median triangle 1.41 px^2). Only consulted after
// the strict one-to-one match fails.
inline constexpr double geometry_sliver_area2=1e-3;

struct AuthoredVertex2 {float x,y,z,u,v;};
struct ProjectedVertex2 {float x,y,z,u,v,rhw=1.f;};
using AuthoredTriangle2=std::array<AuthoredVertex2,3>;
using ProjectedTriangle2=std::array<ProjectedVertex2,3>;
enum class GeometryCull2 {None,Clockwise,CounterClockwise};

// World is row-vector Matrix34 (basis rows, then translation); view and
// projection are row-vector 4x4 matrices. Viewport is x,y,width,height.
// The caller must supply the complete authored and native triangle streams.
// Failure leaves visible_triangles unchanged.
bool check_geometry_parity2(const std::vector<AuthoredTriangle2>& authored,
    const std::array<float,12>& world,const std::array<float,16>& view,
    const std::array<float,16>& projection,const std::array<float,4>& viewport,
    GeometryCull2 cull,const std::vector<ProjectedTriangle2>& native,
    std::uint32_t& visible_triangles,std::string& error) noexcept;

// Diagnostic only, for the failure path: never admits anything. Describes the
// expected visible counts under each cull mode, and, after the same canonical
// sort used by the check, the largest per-component deltas (screen xy, z, uv)
// between expected and native triangles of the requested cull mode.
std::string explain_geometry_parity2(const std::vector<AuthoredTriangle2>& authored,
    const std::array<float,12>& world,const std::array<float,16>& view,
    const std::array<float,16>& projection,const std::array<float,4>& viewport,
    GeometryCull2 cull,const std::vector<ProjectedTriangle2>& native) noexcept;
} // namespace a2fo::refit
