#include <d3d8.h>
#include "../renderer/textures/upload.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

namespace {

using a2fo::renderer::compat::TextureUpload;
using a2fo::renderer::compat::prepare_texture_upload;

void require(bool condition, const char* reason) {
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", reason); std::exit(1); }
}

void require_bytes(const TextureUpload& upload,
                   std::initializer_list<std::uint8_t> expected,
                   const char* reason) {
    require(upload.bytes == std::vector<std::uint8_t>(expected), reason);
}

void test_argb_and_xrgb() {
    const std::array<std::uint8_t, 24> source{{
        0x33, 0x22, 0x11, 0x44, 0x01, 0x02, 0x03, 0x00, 0xee, 0xee, 0xee, 0xee,
        0xfe, 0x00, 0x7f, 0x80, 0x40, 0x50, 0x60, 0xff, 0xee, 0xee, 0xee, 0xee}};
    TextureUpload upload;
    require(SUCCEEDED(prepare_texture_upload(D3DFMT_A8R8G8B8, 2, 2,
            source.data(), source.size(), 12, upload)), "ARGB padded rows accepted");
    require(upload.format == DXGI_FORMAT_R8G8B8A8_UNORM &&
            upload.width == 2 && upload.height == 2 && upload.row_pitch == 8,
            "ARGB upload metadata");
    require_bytes(upload, {0x11, 0x22, 0x33, 0x44, 0x03, 0x02, 0x01, 0x00,
                           0x7f, 0x00, 0xfe, 0x80, 0x60, 0x50, 0x40, 0xff},
                  "ARGB channel order, alpha and row padding");
    require(SUCCEEDED(prepare_texture_upload(D3DFMT_X8R8G8B8, 2, 1,
            source.data(), 8, 8, upload)), "XRGB accepted");
    require_bytes(upload, {0x11, 0x22, 0x33, 0xff, 0x03, 0x02, 0x01, 0xff},
                  "XRGB unused alpha becomes opaque");
}

void test_565_and_luminance() {
    const std::array<std::uint8_t, 8> rgb565{{
        0x00, 0xf8, 0xe0, 0x07, 0x1f, 0x00, 0x10, 0x84}};
    TextureUpload upload;
    require(SUCCEEDED(prepare_texture_upload(D3DFMT_R5G6B5, 4, 1,
            rgb565.data(), rgb565.size(), 8, upload)), "565 accepted");
    require(upload.row_pitch == 16 && upload.format == DXGI_FORMAT_R8G8B8A8_UNORM,
            "565 expanded metadata");
    require_bytes(upload, {255, 0, 0, 255, 0, 255, 0, 255,
                           0, 0, 255, 255, 132, 130, 132, 255},
                  "565 endpoints and nearest normalized midpoint");

    const std::array<std::uint8_t, 6> luminance{{0, 128, 0xee, 255, 7, 0xee}};
    require(SUCCEEDED(prepare_texture_upload(D3DFMT_L8, 2, 2,
            luminance.data(), luminance.size(), 3, upload)), "L8 padded rows accepted");
    require(upload.row_pitch == 8 && upload.height == 2, "L8 expanded metadata");
    require_bytes(upload, {0, 0, 0, 255, 128, 128, 128, 255,
                           255, 255, 255, 255, 7, 7, 7, 255},
                  "L8 replicated to RGB with opaque alpha");
}

void test_packed_and_single_channel_formats() {
    TextureUpload upload;

    const std::array<std::uint8_t, 16> rgb888{{
        0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0xee, 0xee,
        0xaa, 0xbb, 0xcc, 0x10, 0x20, 0x30, 0xee, 0xee}};
    require(SUCCEEDED(prepare_texture_upload(D3DFMT_R8G8B8, 2, 2,
            rgb888.data(), rgb888.size(), 8, upload)),
            "RGB888 padded rows accepted");
    require(upload.row_pitch == 8 && upload.height == 2,
            "RGB888 expanded metadata");
    require_bytes(upload, {0x33, 0x22, 0x11, 255, 0x66, 0x55, 0x44, 255,
                           0xcc, 0xbb, 0xaa, 255, 0x30, 0x20, 0x10, 255},
                  "RGB888 channels, opaque alpha and row padding");

    const std::array<std::uint8_t, 4> x1555{{0x00, 0x7c, 0xe0, 0x03}};
    require(SUCCEEDED(prepare_texture_upload(D3DFMT_X1R5G5B5, 2, 1,
            x1555.data(), x1555.size(), 4, upload)), "X1R5G5B5 accepted");
    require_bytes(upload, {255, 0, 0, 255, 0, 255, 0, 255},
                  "X1R5G5B5 channels and opaque alpha");

    const std::array<std::uint8_t, 4> a1555{{0x00, 0xfc, 0xe0, 0x03}};
    require(SUCCEEDED(prepare_texture_upload(D3DFMT_A1R5G5B5, 2, 1,
            a1555.data(), a1555.size(), 4, upload)), "A1R5G5B5 accepted");
    require_bytes(upload, {255, 0, 0, 255, 0, 255, 0, 0},
                  "A1R5G5B5 channels and one-bit alpha");

    const std::array<std::uint8_t, 4> a4444{{0x23, 0xf1, 0xbc, 0x8a}};
    require(SUCCEEDED(prepare_texture_upload(D3DFMT_A4R4G4B4, 2, 1,
            a4444.data(), a4444.size(), 4, upload)), "A4R4G4B4 accepted");
    require_bytes(upload, {17, 34, 51, 255, 170, 187, 204, 136},
                  "A4R4G4B4 channels and normalized alpha");

    const std::array<std::uint8_t, 10> a8l8{{
        0x20, 0x40, 0xe0, 0x80, 0xee, 0x01, 0x00, 0xff, 0xfe, 0xee}};
    require(SUCCEEDED(prepare_texture_upload(D3DFMT_A8L8, 2, 2,
            a8l8.data(), a8l8.size(), 5, upload)), "A8L8 padded rows accepted");
    require(upload.row_pitch == 8, "A8L8 expanded row pitch");
    require_bytes(upload, {0x20, 0x20, 0x20, 0x40, 0xe0, 0xe0, 0xe0, 0x80,
                           0x01, 0x01, 0x01, 0x00, 0xff, 0xff, 0xff, 0xfe},
                  "A8L8 luminance replication, alpha and row padding");

    const std::array<std::uint8_t, 5> alpha{{0x12, 0xab, 0xee, 0x00, 0xff}};
    require(SUCCEEDED(prepare_texture_upload(D3DFMT_A8, 2, 2,
            alpha.data(), alpha.size(), 3, upload)), "A8 padded rows accepted");
    require_bytes(upload, {255, 255, 255, 0x12, 255, 255, 255, 0xab,
                           255, 255, 255, 0x00, 255, 255, 255, 0xff},
                  "A8 white color with source alpha and row padding");
}

void test_block_compressed() {
    std::array<std::uint8_t, 36> bc1{};
    for (unsigned i = 0; i < 16; ++i) {
        bc1[i] = static_cast<std::uint8_t>(i + 1);
        bc1[20 + i] = static_cast<std::uint8_t>(i + 41);
    }
    bc1[16] = bc1[17] = bc1[18] = bc1[19] = 0xee;
    TextureUpload upload;
    require(SUCCEEDED(prepare_texture_upload(D3DFMT_DXT1, 5, 7,
            bc1.data(), bc1.size(), 20, upload)), "odd-size BC1 with padded block rows");
    require(upload.format == DXGI_FORMAT_BC1_UNORM && upload.width == 5 &&
            upload.height == 7 && upload.row_pitch == 16 && upload.bytes.size() == 32,
            "BC1 dimensions and ceil-block pitch");
    for (unsigned i = 0; i < 16; ++i) {
        require(upload.bytes[i] == i + 1 && upload.bytes[16 + i] == i + 41,
                "BC1 blocks copied without row padding");
    }

    std::array<std::uint8_t, 40> bc2_or_bc3{};
    for (unsigned i = 0; i < 16; ++i) {
        bc2_or_bc3[i] = static_cast<std::uint8_t>(i + 3);
        bc2_or_bc3[24 + i] = static_cast<std::uint8_t>(i + 53);
    }
    for (const auto format : {D3DFMT_DXT2, D3DFMT_DXT3,
                              D3DFMT_DXT4, D3DFMT_DXT5}) {
        require(SUCCEEDED(prepare_texture_upload(format, 3, 5,
                bc2_or_bc3.data(), bc2_or_bc3.size(), 24, upload)),
                "odd-size BC2/BC3 with padded block rows");
        require(upload.format == ((format == D3DFMT_DXT2 || format == D3DFMT_DXT3)
                ? DXGI_FORMAT_BC2_UNORM : DXGI_FORMAT_BC3_UNORM) &&
                upload.row_pitch == 16 && upload.bytes.size() == 32,
                "BC2/BC3 format and block pitch");
        for (unsigned i = 0; i < 16; ++i) {
            require(upload.bytes[i] == i + 3 && upload.bytes[16 + i] == i + 53,
                    "DXT2/3/4/5 blocks copied verbatim without padding");
        }
    }
}

void test_invalid_inputs_are_transactional() {
    TextureUpload output;
    output.format = DXGI_FORMAT_BC3_UNORM;
    output.width = 17;
    output.height = 19;
    output.row_pitch = 23;
    output.bytes = {1, 2, 3, 4};
    const auto unchanged = [&] {
        require(output.format == DXGI_FORMAT_BC3_UNORM && output.width == 17 &&
                output.height == 19 && output.row_pitch == 23 &&
                output.bytes == std::vector<std::uint8_t>({1, 2, 3, 4}),
                "failed upload leaves output unchanged");
    };
    const std::array<std::uint8_t, 16> source{};
    const auto invalid = [&](D3DFORMAT format, UINT width, UINT height,
                             const void* bytes, std::size_t length, UINT pitch) {
        require(prepare_texture_upload(format, width, height, bytes, length,
                                       pitch, output) == D3DERR_INVALIDCALL,
                "invalid upload rejected");
        unchanged();
    };
    invalid(D3DFMT_A8R8G8B8, 1, 1, nullptr, 4, 4);
    invalid(D3DFMT_A8R8G8B8, 0, 1, source.data(), source.size(), 4);
    invalid(D3DFMT_A8R8G8B8, 1, 0, source.data(), source.size(), 4);
    invalid(D3DFMT_A8R8G8B8, 16385, 1, source.data(), source.size(), 65535);
    invalid(D3DFMT_A8R8G8B8, 1, 16385, source.data(), source.size(), 4);
    invalid(D3DFMT_UNKNOWN, 1, 1, source.data(), source.size(), 4);
    invalid(D3DFMT_P8, 1, 1, source.data(), source.size(), 1);
    invalid(D3DFMT_A8R8G8B8, 2, 1, source.data(), source.size(), 7);
    invalid(D3DFMT_A8R8G8B8, 2, 2, source.data(), 15, 8);
    invalid(D3DFMT_L8, 2, 2, source.data(), 4, 3);
    invalid(D3DFMT_DXT1, 5, 7, source.data(), source.size(), 15);
    invalid(D3DFMT_DXT3, 3, 5, source.data(), source.size(), 15);
    invalid(D3DFMT_DXT5, 3, 5, source.data(), source.size(), 16);
    if (sizeof(std::size_t) == 4) {
        invalid(D3DFMT_L8, 1, 16384, source.data(),
                std::numeric_limits<std::size_t>::max(),
                std::numeric_limits<UINT>::max());
    }
}

} // namespace

int main() {
    test_argb_and_xrgb();
    test_565_and_luminance();
    test_packed_and_single_channel_formats();
    test_block_compressed();
    test_invalid_inputs_are_transactional();
    std::puts("PASS: D3D8 texture upload conversion and validation");
    return 0;
}
