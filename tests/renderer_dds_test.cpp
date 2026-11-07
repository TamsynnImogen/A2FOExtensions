#include "../renderer/textures/dds.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace a2fo::renderer::textures;
using Bytes = std::vector<std::uint8_t>;

void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

void word(Bytes& bytes, std::size_t at, UINT value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
}

constexpr UINT code(char a, char b, char c, char d) {
    return static_cast<UINT>(a) | (static_cast<UINT>(b) << 8) |
           (static_cast<UINT>(c) << 16) | (static_cast<UINT>(d) << 24);
}

Bytes fixture(UINT fourcc, DXGI_FORMAT format, UINT width, UINT height,
              UINT mip_count, UINT alpha = 0) {
    const bool dx10 = fourcc == code('D','X','1','0');
    Bytes b(dx10 ? 148 : 128, 0);
    word(b, 0, code('D','D','S',' '));
    word(b, 4, 124);
    word(b, 8, 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000 |
                   (mip_count > 1 ? 0x20000 : 0));
    word(b, 12, height); word(b, 16, width); word(b, 28, mip_count);
    word(b, 76, 32); word(b, 80, 4); word(b, 84, fourcc);
    word(b, 108, 0x1000 | (mip_count > 1 ? 0x8 | 0x400000 : 0));
    if (dx10) {
        word(b, 128, static_cast<UINT>(format));
        word(b, 132, 3); word(b, 140, 1); word(b, 144, alpha);
    }
    const UINT levels = mip_count ? mip_count : 1;
    for (UINT i = 0; i < levels; ++i) {
        UINT row = 0, slice = 0;
        require(compressed_mip_layout(format, width, height, row, slice), "fixture layout");
        if (i == 0) word(b, 20, slice);
        for (UINT n = 0; n < slice; ++n)
            b.push_back(static_cast<std::uint8_t>(i * 17 + n));
        width = std::max(1u, width / 2);
        height = std::max(1u, height / 2);
    }
    return b;
}

void accepted(const Bytes& b, DXGI_FORMAT format, AlphaMode alpha,
              UINT width, UINT height, UINT levels) {
    CompressedImage image;
    std::string error;
    require(parse_compressed_dds(b.data(), b.size(), image, error) == S_OK,
            "valid DDS parsed");
    require(error.empty() && image.format == format && image.alpha_mode == alpha &&
            image.width == width && image.height == height && image.mips.size() == levels,
            "parsed image metadata");
    const std::size_t header = b.size() - image.bytes.size();
    require(header >= 128 && image.bytes == Bytes(b.begin() + header, b.end()),
            "encoded payload copied exactly once and without header");
    require(validate_compressed_image(image, error) == S_OK && error.empty(),
            "parsed image validates");
    std::size_t next = 0;
    for (const auto& mip : image.mips) {
        UINT row = 0, slice = 0;
        require(compressed_mip_layout(format, mip.width, mip.height, row, slice) &&
                mip.row_pitch == row && mip.slice_pitch == slice && mip.offset == next,
                "contiguous mip offsets and pitches");
        next += slice;
    }
    require(next == image.bytes.size(), "exact mip payload length");
}

void rejected(const Bytes& b, HRESULT expected) {
    CompressedImage image;
    image.format = DXGI_FORMAT_BC3_UNORM; image.width = 37;
    image.mips.push_back({37, 1, 5, 6, 7}); image.bytes = {9, 8, 7};
    std::string error;
    require(parse_compressed_dds(b.data(), b.size(), image, error) == expected &&
            !error.empty(), "bad DDS rejected with reason");
    require(image.format == DXGI_FORMAT_BC3_UNORM && image.width == 37 &&
            image.mips.size() == 1 && image.mips[0].offset == 7 &&
            image.bytes == Bytes({9, 8, 7}), "failed parse leaves output unchanged");
}

void test_valid() {
    auto ati2 = fixture(code('A','T','I','2'), DXGI_FORMAT_BC5_UNORM, 8, 8, 4);
    word(ati2, 24, 1); // Real Compressonator legacy ATI2 header, not a volume.
    accepted(ati2, DXGI_FORMAT_BC5_UNORM, AlphaMode::unknown, 8, 8, 4);
    const auto bc5 = fixture(code('D','X','1','0'), DXGI_FORMAT_BC5_SNORM, 5, 9, 4, 4);
    accepted(bc5, DXGI_FORMAT_BC5_SNORM, AlphaMode::custom, 5, 9, 4);
    auto bc7 = fixture(code('D','X','1','0'), DXGI_FORMAT_BC7_UNORM_SRGB, 7, 3, 3, 2);
    accepted(bc7, DXGI_FORMAT_BC7_UNORM_SRGB, AlphaMode::premultiplied, 7, 3, 3);
    for (const auto format : {DXGI_FORMAT_BC1_UNORM_SRGB,
                              DXGI_FORMAT_BC2_UNORM_SRGB,
                              DXGI_FORMAT_BC3_UNORM_SRGB,
                              DXGI_FORMAT_BC4_UNORM,
                              DXGI_FORMAT_BC4_SNORM,
                              DXGI_FORMAT_BC5_UNORM,
                              DXGI_FORMAT_BC7_UNORM})
        accepted(fixture(code('D','X','1','0'), format, 4, 4, 1),
                 format, AlphaMode::unknown, 4, 4, 1);
    bc7.push_back(0xee);
    CompressedImage image;
    std::string error;
    require(parse_compressed_dds(bc7.data(), bc7.size(), image, error) == S_OK &&
            image.bytes.size() == bc7.size() - 149, "trailing file byte omitted");
    for (const auto item : {
        std::pair<UINT, DXGI_FORMAT>{code('D','X','T','1'), DXGI_FORMAT_BC1_UNORM},
        {code('D','X','T','3'), DXGI_FORMAT_BC2_UNORM},
        {code('D','X','T','5'), DXGI_FORMAT_BC3_UNORM},
        {code('A','T','I','1'), DXGI_FORMAT_BC4_UNORM},
        {code('B','C','4','S'), DXGI_FORMAT_BC4_SNORM},
        {code('A','T','I','2'), DXGI_FORMAT_BC5_UNORM},
        {code('B','C','5','S'), DXGI_FORMAT_BC5_SNORM}})
        accepted(fixture(item.first, item.second, 4, 4, 1), item.second,
                 AlphaMode::unknown, 4, 4, 1);
    accepted(fixture(code('D','X','T','2'), DXGI_FORMAT_BC2_UNORM, 4, 4, 1),
             DXGI_FORMAT_BC2_UNORM, AlphaMode::premultiplied, 4, 4, 1);
    accepted(fixture(code('D','X','T','4'), DXGI_FORMAT_BC3_UNORM, 4, 4, 1),
             DXGI_FORMAT_BC3_UNORM, AlphaMode::premultiplied, 4, 4, 1);
    auto omitted = bc5;
    word(omitted, 8, 0x2 | 0x4); // historical writers omit optional flags
    word(omitted, 28, 4);
    accepted(omitted, DXGI_FORMAT_BC5_SNORM, AlphaMode::custom, 5, 9, 4);
    auto hint = bc5;
    word(hint, 20, 1); // incorrect linear-size hint must not alter BC offsets
    accepted(hint, DXGI_FORMAT_BC5_SNORM, AlphaMode::custom, 5, 9, 4);
    word(hint, 8, 0x2 | 0x4 | 0x8); // pitch hint instead of linear-size hint
    word(hint, 20, 999);
    accepted(hint, DXGI_FORMAT_BC5_SNORM, AlphaMode::custom, 5, 9, 4);
    word(hint, 8, 0x2 | 0x4); // no pitch flag, stale field value
    accepted(hint, DXGI_FORMAT_BC5_SNORM, AlphaMode::custom, 5, 9, 4);
    word(hint, 8, 0x2 | 0x4 | 0x8 | 0x80000); // both hints set, neither trusted
    word(hint, 20, 0);
    accepted(hint, DXGI_FORMAT_BC5_SNORM, AlphaMode::custom, 5, 9, 4);
    hint.pop_back();
    rejected(hint, E_INVALIDARG); // unreliable hint cannot excuse missing blocks
    auto zero = fixture(code('D','X','T','1'), DXGI_FORMAT_BC1_UNORM, 4, 4, 0);
    accepted(zero, DXGI_FORMAT_BC1_UNORM, AlphaMode::unknown, 4, 4, 1);
    word(zero, 8, 0x2 | 0x4 | 0x20000 | 0x80000);
    accepted(zero, DXGI_FORMAT_BC1_UNORM, AlphaMode::unknown, 4, 4, 1);
}

void test_invalid() {
    const HRESULT unsupported = HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED);
    auto b = fixture(code('D','X','1','0'), DXGI_FORMAT_BC7_UNORM, 4, 4, 1);
    for (std::size_t n : {std::size_t(0), std::size_t(4), std::size_t(127),
                          std::size_t(128), std::size_t(147), b.size() - 1}) {
        Bytes short_b(b.begin(), b.begin() + n);
        rejected(short_b, E_INVALIDARG);
    }
    auto t = b; word(t, 12, 0); rejected(t, E_INVALIDARG);
    t = b; word(t, 16, 16385); rejected(t, E_INVALIDARG);
    t = b; word(t, 4, 123); rejected(t, E_INVALIDARG);
    t = b; word(t, 76, 31); rejected(t, E_INVALIDARG);
    t = b; word(t, 8, 0x1 | 0x4 | 0x1000); rejected(t, E_INVALIDARG);
    t = b; word(t, 28, 4); rejected(t, E_INVALIDARG);
    t = b; word(t, 112, 0x200); rejected(t, unsupported);
    t = b; word(t, 112, 0x200000); rejected(t, unsupported);
    t = b; word(t, 24, 2); rejected(t, unsupported);
    t = b; word(t, 24, 1); word(t, 8, 0x8A1007); rejected(t, unsupported);
    t = b; word(t, 132, 4); rejected(t, unsupported);
    t = b; word(t, 140, 2); rejected(t, unsupported);
    t = b; word(t, 136, 4); rejected(t, unsupported);
    t = b; word(t, 144, 8); rejected(t, E_INVALIDARG);
    t = b; word(t, 144, 5); rejected(t, E_INVALIDARG);
    t = b; word(t, 128, DXGI_FORMAT_BC7_TYPELESS); rejected(t, unsupported);
    t = b; word(t, 128, DXGI_FORMAT_BC6H_UF16); rejected(t, unsupported);
    t = b; word(t, 128, DXGI_FORMAT_R8G8B8A8_UNORM); rejected(t, unsupported);
    t = b; word(t, 80, 0x40); rejected(t, unsupported);
    t = b; word(t, 80, 0x44); rejected(t, E_INVALIDARG);
    t = b; word(t, 84, code('N','O','P','E')); rejected(t, unsupported);
}

void test_forged_images_and_layout() {
    UINT row = 71, slice = 73;
    require(!compressed_mip_layout(DXGI_FORMAT_BC7_UNORM, 0, 4, row, slice) &&
            row == 71 && slice == 73, "invalid layout leaves outputs unchanged");
    require(!compressed_mip_layout(DXGI_FORMAT_BC6H_UF16, 4, 4, row, slice) &&
            row == 71 && slice == 73, "unsupported layout leaves outputs unchanged");
    require(!compressed_mip_layout(DXGI_FORMAT_BC7_UNORM, std::numeric_limits<UINT>::max(),
                                    4, row, slice), "oversize layout rejected");
    auto b = fixture(code('D','X','1','0'), DXGI_FORMAT_BC7_UNORM, 4, 4, 1);
    CompressedImage image;
    std::string error;
    require(parse_compressed_dds(b.data(), b.size(), image, error) == S_OK, "base image parsed");
    image.mips[0].offset = 1;
    require(validate_compressed_image(image, error) == E_INVALIDARG, "forged offset rejected");
    image.mips[0].offset = 0; image.mips[0].row_pitch = 8;
    require(validate_compressed_image(image, error) == E_INVALIDARG, "forged pitch rejected");
    image.mips[0].row_pitch = 16; image.bytes.push_back(0);
    require(validate_compressed_image(image, error) == E_INVALIDARG, "extra payload rejected");
    image.bytes.pop_back(); image.bytes.pop_back();
    require(validate_compressed_image(image, error) == E_INVALIDARG, "short payload rejected");
    image.format = DXGI_FORMAT_BC7_TYPELESS;
    require(validate_compressed_image(image, error) == HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED),
            "forged typeless format rejected");
}
} // namespace

int main() {
    test_valid(); test_invalid(); test_forged_images_and_layout();
    std::puts("PASS: compressed DDS parsing, layout and validation");
}
