// Keep cached MeshVB vertices and GPU lighting for alpha materials, but order
// their triangles before the final textured draw. This is per-group sorting;
// it does not replace the engine's inter-object transparent submission order.
namespace {

struct TransparentTriangle {
    float depth = 0.0f;
    UINT ordinal = 0;
    std::array<unsigned short, 3> indices{};
};

struct TransparentIndexDraw {
    IDirect3DDevice8* device = nullptr;
    IDirect3DIndexBuffer8* previous = nullptr;
    UINT base = 0;
    bool active = false;
};

thread_local TransparentIndexDraw g_transparent_index_draw{};
thread_local std::vector<TransparentTriangle> g_transparent_triangles;
IDirect3DDevice8* g_transparent_index_owner = nullptr; // Core-owned device.
IDirect3DIndexBuffer8* g_transparent_index_buffer = nullptr;
UINT g_transparent_index_capacity = 0;
volatile LONG g_transparent_index_logged = 0;
volatile LONG g_transparent_index_failed = 0;

void restore_transparent_mesh_indices() noexcept {
    auto& draw = g_transparent_index_draw;
    if (draw.active && draw.device)
        draw.device->SetIndices(draw.previous, draw.base);
    if (draw.previous) draw.previous->Release();
    draw = {};
}

void report_transparent_index_failure(const char* reason) noexcept {
    if (InterlockedCompareExchange(&g_transparent_index_failed, 1, 0) != 0) return;
    char message[240]{};
    std::snprintf(message, sizeof(message),
        "Transparent MeshVB index ordering unavailable: %s; GPU lighting retained with original indices",
        reason);
    log_line(message);
}

bool ensure_transparent_mesh_indices(IDirect3DDevice8* device, UINT bytes) noexcept {
    if (g_transparent_index_owner && g_transparent_index_owner != device) return false;
    if (g_transparent_index_buffer && bytes <= g_transparent_index_capacity) return true;
    const UINT capacity = (bytes + 4095u) & ~4095u;
    IDirect3DIndexBuffer8* replacement = nullptr;
    if (FAILED(device->CreateIndexBuffer(capacity,
            D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY, D3DFMT_INDEX16,
            D3DPOOL_DEFAULT, &replacement)) || !replacement) return false;
    if (g_transparent_index_buffer) g_transparent_index_buffer->Release();
    g_transparent_index_buffer = replacement;
    g_transparent_index_capacity = capacity;
    g_transparent_index_owner = device;
    return true;
}

}  // namespace

extern "C" void release_transparent_mesh_indices(IDirect3DDevice8* device) noexcept {
    if (g_transparent_index_owner != device) return;
    restore_transparent_mesh_indices();
    if (g_transparent_index_buffer) g_transparent_index_buffer->Release();
    g_transparent_index_buffer = nullptr;
    g_transparent_index_owner = nullptr;
    g_transparent_index_capacity = 0;
}

extern "C" void __cdecl a2fo_nebula_transparent_indices_pre(
    IDirect3DDevice8* device, const void* group, UINT primitive_count) noexcept {
    if (!device || !g_cloak_composite.active ||
        !cloak_composite_selected(device) || g_transparent_index_draw.active ||
        primitive_count < 2) return;
    DWORD enabled = FALSE, source = 0, destination = 0, depth_write = TRUE;
    if (FAILED(device->GetRenderState(D3DRS_ALPHABLENDENABLE, &enabled)) ||
        FAILED(device->GetRenderState(D3DRS_SRCBLEND, &source)) ||
        FAILED(device->GetRenderState(D3DRS_DESTBLEND, &destination)) ||
        FAILED(device->GetRenderState(D3DRS_ZWRITEENABLE, &depth_write))) return;
    // Full cloak's ONE/ONE accumulation is order-independent. Do not burden
    // it, opaque objects, or depth-writing cutouts with per-frame sorting.
    if (!enabled || depth_write || destination != D3DBLEND_INVSRCALPHA ||
        (source != D3DBLEND_SRCALPHA && source != D3DBLEND_ONE)) return;

    const UINT vertices = read_at<UINT>(group, 0x10, 0);
    const UINT triangles = read_at<UINT>(group, 0x14, 0);
    const auto* positions = read_at<const unsigned char*>(group, 4, nullptr);
    const auto* indices = read_at<const unsigned short*>(group, 0xc, nullptr);
    if (!vertices || vertices > 65535 || primitive_count > triangles ||
        primitive_count > 21845 || !readable_range(positions, vertices * 68u) ||
        !readable_range(indices, primitive_count * 6u)) {
        report_transparent_index_failure("native group layout");
        return;
    }
    // c4/c5 are the clip-Z/clip-W rows used by the native m4x4 oPos shader.
    // Normalized depth works for perspective and editor orthographic views.
    std::array<std::array<float, 4>, 2> projection{};
    if (FAILED(device->GetVertexShaderConstant(4, projection.data(), 2))) return;
    try {
        g_transparent_triangles.resize(primitive_count);
    } catch (...) {
        report_transparent_index_failure("triangle scratch allocation");
        return;
    }
    for (UINT triangle = 0; triangle < primitive_count; ++triangle) {
        auto& entry = g_transparent_triangles[triangle];
        entry.ordinal = triangle;
        std::memcpy(entry.indices.data(), indices + triangle * 3u, 6);
        std::array<float, 3> centre{};
        for (unsigned short index : entry.indices) {
            if (index >= vertices) {
                report_transparent_index_failure("native vertex index");
                return;
            }
            std::array<float, 3> position{};
            std::memcpy(position.data(), positions + index * 68u, 12);
            for (unsigned axis = 0; axis < 3; ++axis)
                centre[axis] += position[axis] / 3.0f;
        }
        float clip_z = projection[0][3], clip_w = projection[1][3];
        for (unsigned axis = 0; axis < 3; ++axis) {
            clip_z += centre[axis] * projection[0][axis];
            clip_w += centre[axis] * projection[1][axis];
        }
        if (!std::isfinite(clip_z) || !std::isfinite(clip_w) ||
            std::fabs(clip_w) < 1.0e-12f) {
            report_transparent_index_failure("invalid projected depth");
            return;
        }
        entry.depth = clip_z / clip_w;
        if (!std::isfinite(entry.depth)) return;
    }
    std::sort(g_transparent_triangles.begin(), g_transparent_triangles.end(),
        [](const TransparentTriangle& left, const TransparentTriangle& right) {
            if (left.depth != right.depth) return left.depth > right.depth;
            return left.ordinal < right.ordinal;
        });
    const UINT bytes = primitive_count * 6u;
    if (!ensure_transparent_mesh_indices(device, bytes)) {
        report_transparent_index_failure("dynamic index buffer allocation");
        return;
    }
    BYTE* output = nullptr;
    if (FAILED(g_transparent_index_buffer->Lock(0, bytes, &output, D3DLOCK_DISCARD))) {
        report_transparent_index_failure("dynamic index buffer lock");
        return;
    }
    for (UINT triangle = 0; triangle < primitive_count; ++triangle)
        std::memcpy(output + triangle * 6u,
            g_transparent_triangles[triangle].indices.data(), 6);
    if (FAILED(g_transparent_index_buffer->Unlock())) {
        report_transparent_index_failure("dynamic index buffer unlock");
        return;
    }
    auto& draw = g_transparent_index_draw;
    if (FAILED(device->GetIndices(&draw.previous, &draw.base))) {
        restore_transparent_mesh_indices();
        return;
    }
    draw.device = device;
    if (FAILED(device->SetIndices(g_transparent_index_buffer, draw.base))) {
        restore_transparent_mesh_indices();
        return;
    }
    draw.active = true;
    if (InterlockedCompareExchange(&g_transparent_index_logged, 1, 0) == 0)
        log_line("Transparent MeshVB indices sorted back-to-front; cached vertices and GPU lighting retained; additive full-cloak draws bypass sorting");
}

extern "C" void __cdecl a2fo_nebula_transparent_indices_post() noexcept {
    restore_transparent_mesh_indices();
}
