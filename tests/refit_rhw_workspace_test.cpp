#include "../build/refit/material-colours-candidate/source/renderer/refit/rhw_workspace.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using namespace a2fo::refit;

namespace {
int failures = 0;
void expect(bool yes, const char* what) {
    if (!yes) {std::cerr << "FAIL: " << what << '\n'; ++failures;}
}
bool same(const ProjectedVertex2& a, const ProjectedVertex2& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z && a.u == b.u &&
           a.v == b.v && a.rhw == b.rhw;
}
bool same(const std::vector<ProjectedTriangle2>& a, const std::vector<ProjectedTriangle2>& b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i)
        for (unsigned corner = 0; corner < 3; ++corner)
            if (!same(a[i][corner], b[i][corner])) return false;
    return true;
}
std::vector<ProjectedTriangle2> sentinel() {
    return {{{{91, 92, .93f, .94f, .95f, .96f},
              {81, 82, .83f, .84f, .85f, .86f},
              {71, 72, .73f, .74f, .75f, .76f}}}};
}
void vertex(std::vector<std::uint8_t>& bytes, std::uint32_t stride,
            std::uint32_t uv_offset, std::size_t index,
            const std::array<float, 4>& position, const std::array<float, 2>& uv) {
    auto* dest = bytes.data() + index * stride;
    std::memcpy(dest, position.data(), sizeof(float) * 4);
    std::memcpy(dest + uv_offset, uv.data(), sizeof(float) * 2);
}
std::vector<std::uint8_t> vertices(const RhwLayout2& layout) {
    // Colour and (for 0x1c4) specular slots remain 0xff/NaN patterns.
    std::vector<std::uint8_t> bytes(4 * layout.stride, 0xff);
    vertex(bytes, layout.stride, layout.uv_offset, 0, {10, 20, .25f, 2}, {.1f, .2f});
    vertex(bytes, layout.stride, layout.uv_offset, 1, {30, 40, .5f, 3}, {.3f, .4f});
    vertex(bytes, layout.stride, layout.uv_offset, 2, {50, 60, .75f, 4}, {.5f, .6f});
    // Vertex 3 is unreferenced and deliberately contains invalid float bytes.
    return bytes;
}
void rejects(const std::vector<std::uint8_t>& bytes,
             const std::vector<std::uint16_t>& indices, const RhwLayout2& layout,
             float offset, const char* what) {
    auto output = sentinel();
    const auto before = output;
    std::string error;
    const bool result = decode_rhw_triangles2(bytes, indices, layout, offset, output, error);
    expect(!result, what);
    expect(!error.empty(), "rejection gives error");
    expect(same(output, before), "rejection leaves output unchanged");
}
void checked_layout(std::uint32_t format, std::uint32_t stride, std::uint32_t model,
                    std::uint32_t uv_offset) {
    RhwLayout2 output{99, 99, 99};
    std::string error = "stale";
    expect(rhw_layout2(format, stride, model, output, error), "known layout accepted");
    expect(output.format == format && output.stride == stride &&
           output.uv_offset == uv_offset && error.empty(), "exact UV offset and cleared error");
}
void rejected_layout(std::uint32_t format, std::uint32_t stride, std::uint32_t model) {
    RhwLayout2 output{99, 98, 97};
    const auto before = output;
    std::string error;
    expect(!rhw_layout2(format, stride, model, output, error), "layout mismatch refused");
    expect(output.format == before.format && output.stride == before.stride &&
           output.uv_offset == before.uv_offset && !error.empty(),
           "layout rejection is transactional");
}
void valid_decode(const RhwLayout2& layout) {
    auto bytes = vertices(layout);
    const auto original = bytes;
    const std::vector<std::uint16_t> indices{2, 0, 1};
    const auto indices_before = indices;
    auto output = sentinel();
    std::string error = "stale";
    expect(decode_rhw_triangles2(bytes, indices, layout, .5f, output, error),
           "RHW triangle admitted");
    expect(error.empty() && output.size() == 1, "RHW decode count/error");
    if (output.size() == 1) {
        const ProjectedTriangle2 want = {{{50.5f, 60.5f, .75f, .5f, .6f, 4},
                                          {10.5f, 20.5f, .25f, .1f, .2f, 2},
                                          {30.5f, 40.5f, .5f, .3f, .4f, 3}}};
        for (unsigned i = 0; i < 3; ++i)
            expect(same(output[0][i], want[i]), "XYZ/RHW/UV and index order exact");
    }
    expect(bytes == original && indices == indices_before, "input buffers immutable");

    rejects(bytes, {}, layout, 0, "empty index list refused");
    rejects({}, indices, layout, 0, "empty vertex buffer refused");
    auto partial = bytes; partial.pop_back();
    rejects(partial, indices, layout, 0, "partial vertex stride refused");
    rejects(bytes, {0, 1}, layout, 0, "partial triangle refused");
    rejects(bytes, {0, 1, 4}, layout, 0, "index outside vertex buffer refused");
    rejects(bytes, indices, layout, std::numeric_limits<float>::infinity(),
            "nonfinite raster offset refused");
    RhwLayout2 bad = layout; ++bad.uv_offset;
    rejects(bytes, indices, bad, 0, "incorrect UV offset refused");
    bad = layout; ++bad.stride;
    rejects(bytes, indices, bad, 0, "incorrect stride refused");
    bad = layout; bad.format = 0;
    rejects(bytes, indices, bad, 0, "incorrect FVF refused");
    auto invalid = bytes;
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    vertex(invalid, layout.stride, layout.uv_offset, 0, {10, 20, .25f, 0}, {.1f, .2f});
    rejects(invalid, indices, layout, 0, "zero referenced RHW refused");
    invalid = bytes;
    vertex(invalid, layout.stride, layout.uv_offset, 0, {10, 20, .25f, nan}, {.1f, .2f});
    rejects(invalid, indices, layout, 0, "nonfinite referenced RHW refused");
    invalid = bytes;
    vertex(invalid, layout.stride, layout.uv_offset, 0, {10, 20, .25f, 2}, {nan, .2f});
    rejects(invalid, indices, layout, 0, "nonfinite referenced UV refused");
    invalid = bytes;
    vertex(invalid, layout.stride, layout.uv_offset, 0, {nan, 20, .25f, 2}, {.1f, .2f});
    rejects(invalid, indices, layout, 0, "nonfinite referenced X refused");
    invalid = bytes;
    const auto maximum = std::numeric_limits<float>::max();
    vertex(invalid, layout.stride, layout.uv_offset, 0, {maximum, 20, .25f, 2}, {.1f, .2f});
    rejects(invalid, indices, layout, maximum, "raster adjustment overflow refused");
    rejects(bytes, std::vector<std::uint16_t>(6147, 0), layout, 0,
            "index count above 6144 refused");
    rejects(std::vector<std::uint8_t>(65536u * layout.stride, 0), indices, layout, 0,
            "vertex count above 65535 refused");

    std::vector<std::uint8_t> maximum_vertices(65535u * layout.stride, 0xff);
    vertex(maximum_vertices, layout.stride, layout.uv_offset, 0,
           {1, 2, .5f, 1}, {.25f, .75f});
    std::vector<std::uint16_t> maximum_indices(6144, 0);
    output.clear(); error = "stale";
    expect(decode_rhw_triangles2(maximum_vertices, maximum_indices, layout, 0, output, error),
           "supported vertex/index limits admitted");
    expect(output.size() == 2048 && error.empty(), "limit output triangle count");
}
}

int main() {
    checked_layout(0x144, 28, 0, 20);
    checked_layout(0x144, 28, 1, 20);
    checked_layout(0x1c4, 32, 2, 24);
    rejected_layout(0x144, 28, 2);
    rejected_layout(0x1c4, 32, 0);
    rejected_layout(0x1c4, 32, 1);
    rejected_layout(0x144, 32, 0);
    rejected_layout(0x1c4, 28, 2);
    rejected_layout(0x999, 28, 1);
    valid_decode({0x144, 28, 20});
    valid_decode({0x1c4, 32, 24});
    if (failures) std::cerr << failures << " RHW workspace checks failed\n";
    else std::cout << "RHW layout and workspace decode checks passed\n";
    return failures ? 1 : 0;
}
