#include "amd_dot3_compat.hpp"

#include <cstdio>
#include <cstring>

int main() {
    using namespace a2fo::amd_dot3;

    if (kUncreatedShaderHandle != 0xffffffffu) {
        std::fprintf(stderr, "AMD DOT3 uncreated-handle sentinel changed\n");
        return 1;
    }

    if (policy_from_ini(0) != Policy::disabled ||
        policy_from_ini(1) != Policy::automatic ||
        policy_from_ini(2) != Policy::forced ||
        policy_from_ini(-1) != Policy::automatic ||
        policy_from_ini(99) != Policy::automatic) {
        std::fprintf(stderr, "AMD DOT3 INI policy normalization failed\n");
        return 1;
    }

    if (should_apply(Policy::disabled, true, kAmdPciVendorId) ||
        should_apply(Policy::automatic, false, 0) ||
        should_apply(Policy::automatic, true, 0x10de) ||
        !should_apply(Policy::automatic, true, kAmdPciVendorId) ||
        !should_apply(Policy::forced, false, 0)) {
        std::fprintf(stderr, "AMD DOT3 adapter selection failed\n");
        return 1;
    }

    if (kStockDeclaration.front() != kRemappedDeclaration.front() ||
        kStockDeclaration.back() != kRemappedDeclaration.back() ||
        kStockDeclaration[1] != kRemappedDeclaration[1]) {
        std::fprintf(stderr, "AMD DOT3 stream framing changed\n");
        return 1;
    }
    for (std::size_t index = 2; index < 7; ++index) {
        const std::uint32_t stock_type =
            kStockDeclaration[index] & 0xffff0000u;
        const std::uint32_t remapped_type =
            kRemappedDeclaration[index] & 0xffff0000u;
        if (stock_type != remapped_type) {
            std::fprintf(stderr, "AMD DOT3 field type/stride changed\n");
            return 1;
        }
        const std::uint32_t remapped_register =
            kRemappedDeclaration[index] & 0x0000ffffu;
        if (remapped_register != index + 5) {
            std::fprintf(stderr, "AMD DOT3 TEXCOORD register remap changed\n");
            return 1;
        }
    }

    if (std::strcmp(kStockShaderPath, kRemappedShaderPath) == 0 ||
        sizeof(kStockShaderPath) != 29 ||
        std::strlen(kRemappedShaderPath) >= sizeof(kStockShaderPath)) {
        std::fprintf(stderr, "AMD DOT3 shader path replacement is invalid\n");
        return 1;
    }

    if (sizeof(VertexElement9) != 8 ||
        kDirectD3d9StockDeclaration.front().usage !=
            kDeclUsagePosition ||
        kDirectD3d9StockDeclaration.back().stream != 0xffffu ||
        kDirectD3d9StockDeclaration.back().type != kDeclTypeUnused ||
        kDirectD3d9RemappedDeclaration.front().usage !=
            kDeclUsagePosition ||
        kDirectD3d9RemappedDeclaration.back().stream != 0xffffu ||
        kDirectD3d9RemappedDeclaration.back().type != kDeclTypeUnused) {
        std::fprintf(stderr,
                     "Direct-D3D9 DOT3 declaration framing changed\n");
        return 1;
    }
    for (std::size_t index = 0;
         index < kDirectD3d9StockDeclaration.size(); ++index) {
        const VertexElement9& stock =
            kDirectD3d9StockDeclaration[index];
        const VertexElement9& remapped =
            kDirectD3d9RemappedDeclaration[index];
        if (stock.stream != remapped.stream ||
            stock.offset != remapped.offset ||
            stock.type != remapped.type ||
            stock.method != remapped.method) {
            std::fprintf(stderr,
                         "Direct-D3D9 DOT3 stream layout changed\n");
            return 1;
        }
        if (index >= 1 && index <= 5 &&
            (remapped.usage != kDeclUsageTexcoord ||
             remapped.usage_index != index - 1)) {
            std::fprintf(stderr,
                         "Direct-D3D9 TEXCOORD semantics changed\n");
            return 1;
        }
    }
    if (std::strcmp(kDirectD3d9RemappedShaderPath,
                    "Shaders\\dot3_amd9.nvv") != 0) {
        std::fprintf(stderr,
                     "Direct-D3D9 shader path replacement is invalid\n");
        return 1;
    }

    return 0;
}
