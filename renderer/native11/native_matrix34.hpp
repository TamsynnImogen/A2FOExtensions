#pragma once
#include <array>
#include <string>

namespace a2fo::native11 {
using NativeMatrix34=std::array<float,12>;
// Native row-vector local * parent convention: three basis rows followed by
// translation. Preserve the caller's x87 PC/RC and the original per-element
// arithmetic order. Finite singular matrices and negative scales are valid.
// Failure leaves output untouched. No native code or live state is accessed.
bool native_matrix34_multiply(const NativeMatrix34& local,
    const NativeMatrix34& parent,NativeMatrix34& output,std::string& error) noexcept;
bool native_matrix34_scale(const NativeMatrix34& local,float scale,
    NativeMatrix34& output,std::string& error) noexcept;
// RenderInternal uses a distinct association for the camera-space translation
// (local.x*view.x + local.z*view.z, then local.y*view.y, then view translation).
bool native_matrix34_camera_translation(const NativeMatrix34& world,
    const NativeMatrix34& view,std::array<float,3>& output,std::string& error) noexcept;
} // namespace a2fo::native11
