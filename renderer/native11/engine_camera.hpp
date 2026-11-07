#pragma once

namespace a2fo::native11 {
// The installed Armada CPU camera's combined world-to-screen matrix is
// row-major and acts on row vectors. Its homogeneous U,V,depth,W output is
// projected to pixels with (width-1)*U/W and (height-1)*V/W.
// This input is not a D3D8 or D3D9 device matrix.
struct EngineCameraView {
    float world_to_clip[16]; // row-major, column-vector native11 matrix
    float viewport[4];      // x, y, width, height; copy of the supplied rectangle
};

// Uses caller-owned arrays and changes output only on success. The resulting
// matrix maps to the supplied D3D-style viewport without a half-pixel offset.
// The source matrix must be invertible. A one-pixel viewport legitimately
// collapses its clip axis, so the resulting matrix may be singular.
bool convert_engine_cpu_camera(const float engine_screen[16],
                               const float viewport[4], EngineCameraView& output);
} // namespace a2fo::native11
