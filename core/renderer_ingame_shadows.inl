// Opt-in main-view shadows. The native 3D body and every original DIP execute
// once; only CPU mirrors already used by the native MeshVB are copied here.
// The private GPU pass runs after the 3D body, before the outer HUD render.
namespace {

bool native11_owns_bloom_refresh(HMODULE image) noexcept {
    if(!image||image!=g_armada||g_frame_bloom_hook.target!=at(image,kFrameBloomRva)||
       g_frame_bloom_hook.length!=kExpectedFrameBloom.size()||!g_frame_bloom_hook.gateway||
       g_a2fo_nebula_frame_bloom_gateway!=g_frame_bloom_hook.gateway)return false;
    const auto target=reinterpret_cast<std::uintptr_t>(g_frame_bloom_hook.target);
    const auto gateway=reinterpret_cast<std::uintptr_t>(g_frame_bloom_hook.gateway);
    std::array<std::uint8_t,7> entry{};
    std::array<std::uint8_t,12> code{};
    SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(target),entry.data(),entry.size(),&copied)||copied!=entry.size()||
       !ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(gateway),code.data(),code.size(),&copied)||copied!=code.size()||
       entry[0]!=0xe9||entry[5]!=0x90||entry[6]!=0x90||code[7]!=0xe9||
       std::memcmp(code.data(),kExpectedFrameBloom.data(),kExpectedFrameBloom.size()))return false;
    std::uint32_t relative=0;std::memcpy(&relative,entry.data()+1,4);
    if(static_cast<std::uint32_t>(target+5+relative)!=
       reinterpret_cast<std::uintptr_t>(function_address(&a2fo_nebula_frame_bloom_hook)))return false;
    std::memcpy(&relative,code.data()+8,4);
    if(static_cast<std::uint32_t>(gateway+code.size()+relative)!=target+entry.size())return false;
    MEMORY_BASIC_INFORMATION page{};
    return VirtualQuery(reinterpret_cast<void*>(gateway),&page,sizeof(page))==sizeof(page)&&page.State==MEM_COMMIT&&
        (page.Protect==PAGE_EXECUTE_READ||page.Protect==PAGE_EXECUTE_READWRITE||page.Protect==PAGE_EXECUTE_WRITECOPY);
}

constexpr std::uintptr_t kShadowWorldBodyRva = 0x00198110;
constexpr std::uintptr_t kShadowMainCallRva = 0x0019886e;
constexpr std::array<std::uint8_t, 5> kShadowMainCall{{0xe8,0x9d,0xf8,0xff,0xff}};
constexpr std::uintptr_t kShadowDirectionalLightVtableRva = 0x002bc8fc;
using ShadowWorldBody = void (__cdecl*)(void*, void*, unsigned);
SRWLOCK g_ingame_shadow_lock = SRWLOCK_INIT;
bool g_refit_capture_mode = false;
std::unique_ptr<a2fo::shadows::Pass> g_ingame_shadow_pass;
a2fo::shadows::Settings g_ingame_shadow_settings{};
bool g_ingame_shadow_hook_installed = false;
volatile LONG g_ingame_shadow_owner = 0;
volatile LONG g_ingame_shadow_frames = 0;
thread_local bool g_ingame_shadow_view = false;
thread_local unsigned g_ingame_shadow_seen = 0;
thread_local unsigned g_ingame_shadow_captured = 0;
thread_local const char* g_ingame_shadow_skip = "no eligible hull draw";
thread_local const void* g_ingame_shadow_group = nullptr;
thread_local UINT g_ingame_shadow_group_primitives = 0;
thread_local std::vector<float> g_ingame_shadow_positions;
thread_local std::array<unsigned,8> g_ingame_shadow_rejections{};
thread_local char g_ingame_shadow_capture_failure[256]{};
thread_local unsigned g_ingame_shadow_previous_captured = 0;
thread_local bool g_ingame_shadow_previous_drawn = false;
volatile LONG g_ingame_shadow_capture_logs = 0;
// Identities only, never dereferenced. Accessed under g_ingame_shadow_lock.
std::array<std::array<const void*,3>,24> g_ingame_shadow_sample_keys{};
volatile LONG g_ingame_shadow_change_logs = 0;

void ingame_shadow_reject(unsigned category,const char* reason) noexcept {
    ++g_ingame_shadow_rejections[category];
    g_ingame_shadow_skip=reason;
}

struct IngameShadowLock {
    IngameShadowLock() { AcquireSRWLockExclusive(&g_ingame_shadow_lock); }
    ~IngameShadowLock() { ReleaseSRWLockExclusive(&g_ingame_shadow_lock); }
};

// The list and record layout is also used by the existing DOT3 light collector
// and Refit scene/lights.rs. No native light, object or visibility state is written.
bool ingame_shadow_key(float out[3]) noexcept {
    const void* engine = phong_engine();
    const void* head = read_at<const void*>(engine, 0x60, nullptr);
    if (!readable_range(head, 12)) return false;
    const void* node = read_at<const void*>(head, 0, nullptr);
    float brightest = -1.0f;
    unsigned guard = 0;
    while (node && node != head && guard++ < 4096) {
        if (!readable_range(node, 12)) return false;
        const void* record = read_at<const void*>(node, 8, nullptr);
        node = read_at<const void*>(node, 0, nullptr);
        if (!readable_range(record, 0x40)) continue;
        const void* light = read_at<const void*>(record, 0, nullptr);
        if (read_at<const void*>(light, 0, nullptr) !=
            at(g_armada, kShadowDirectionalLightVtableRva)) continue;
        float colour[3]{}, direction[3]{};
        bool valid = true;
        for (unsigned i = 0; i < 3; ++i) {
            colour[i] = read_at<float>(record, 4 + i * 4, NAN);
            direction[i] = read_at<float>(record, 0x28 + i * 4, NAN);
            valid = valid && std::isfinite(colour[i]) && colour[i] >= 0 &&
                std::isfinite(direction[i]);
        }
        const float energy = colour[0] + colour[1] + colour[2];
        const float length = direction[0]*direction[0] + direction[1]*direction[1] +
            direction[2]*direction[2];
        if (valid && std::isfinite(energy) && energy > brightest &&
            std::isfinite(length) && length > 1e-8f) {
            const float scale = 1.0f / std::sqrt(length);
            for (unsigned i = 0; i < 3; ++i) out[i] = direction[i] * scale;
            brightest = energy;
        }
    }
    return node == head && brightest > 0;
}

void __cdecl ingame_shadow_world(void* camera, void* stuff, unsigned main) {
    const auto original = function_from_address<ShadowWorldBody>(at(g_armada, kShadowWorldBodyRva));
    if(g_refit_capture_mode)a2fo::refit_scene_test::before_world(camera,main);
    // The capture observer shares this existing checked main-view wrapper;
    // neither subsystem overwrites the other's CALL or replays native rendering.
    struct CaptureScope {
        void* camera;
        bool owned;
        ~CaptureScope() { a2fo::native11_capture::end(camera,owned); }
    } capture{camera,a2fo::native11_capture::begin(camera,
        (g_refit_capture_mode || InterlockedCompareExchange(&g_runtime_enabled,0,0)!=0) ? main : 0)};
    // Class configuration is prepared by the deferred startup worker. Bind
    // the read-only world-light sampler on the checked main-view owner thread,
    // after that producer is ready, rather than inheriting the worker's ID.
    if(capture.owned&&a2fo::native11_light_classes::requested()&&
       !a2fo::native11_world_lights::requested())
        a2fo::native11_world_lights::initialize(g_armada,true,log_line);
    struct RefitViewScope {
        void* camera;std::uint64_t ticket;
        ~RefitViewScope(){a2fo::refit_scene::end_view(camera,ticket);}
    } refit_view{camera,g_refit_capture_mode?a2fo::refit_scene::begin_view(camera,capture.owned):0};
    const LONG thread = static_cast<LONG>(GetCurrentThreadId());
    const bool enabled = (main & 0xffu) && g_ingame_shadow_settings.enabled &&
        g_dxvk_backend_active && g_hooks_ready &&
        InterlockedCompareExchange(&g_runtime_enabled,0,0) != 0;
    if (!enabled || g_ingame_shadow_view ||
        InterlockedCompareExchange(&g_ingame_shadow_owner,thread,0) != 0) {
        original(camera,stuff,main);
        return;
    }
    struct ViewOwner {
        ~ViewOwner() {
            g_ingame_shadow_view = false;
            g_ingame_shadow_group = nullptr;
            InterlockedExchange(&g_ingame_shadow_owner,0);
        }
    } owner;
    bool prepared = false;
    {
        const IngameShadowLock lock;
        try {
            if (!g_ingame_shadow_pass)
                g_ingame_shadow_pass = std::make_unique<a2fo::shadows::Pass>();
            g_ingame_shadow_pass->configure(g_ingame_shadow_settings);
            g_ingame_shadow_pass->begin_view();
            prepared = true;
        } catch (...) { /* Original rendering must remain available. */ }
    }
    g_ingame_shadow_view = prepared;
    g_ingame_shadow_seen = g_ingame_shadow_captured = 0;
    g_ingame_shadow_rejections.fill(0);
    g_ingame_shadow_capture_failure[0]=0;
    g_ingame_shadow_skip = "no eligible hull draw";
    g_ingame_shadow_group = nullptr;
    // No gameplay entry point is replayed: the original world renders exactly once.
    original(camera,stuff,main);
    g_ingame_shadow_view = false;
    g_ingame_shadow_group = nullptr;
    if (prepared) {
        const IngameShadowLock lock;
        const bool drawn = g_ingame_shadow_pass->finish_view();
        const LONG frame = InterlockedIncrement(&g_ingame_shadow_frames);
        const bool changed=drawn!=g_ingame_shadow_previous_drawn ||
            g_ingame_shadow_captured!=g_ingame_shadow_previous_captured;
        g_ingame_shadow_previous_drawn=drawn;
        g_ingame_shadow_previous_captured=g_ingame_shadow_captured;
        if (frame <= 12 || frame % 300 == 0 ||
            (changed && InterlockedIncrement(&g_ingame_shadow_change_logs)<=96)) {
            char message[1280]{};
            std::snprintf(message,sizeof(message),
                "In-game shadows: frame=%ld seen=%u captured=%u status=%s; %.200s; rejects(context/route/material/mirror/base/light/vertex/gpu)=%u/%u/%u/%u/%u/%u/%u/%u; capture-failure=%.160s; %.360s; skip=%.100s",
                frame,g_ingame_shadow_seen,g_ingame_shadow_captured,
                drawn ? "drawn" : "native",g_ingame_shadow_pass->diagnostic(),
                g_ingame_shadow_rejections[0],g_ingame_shadow_rejections[1],
                g_ingame_shadow_rejections[2],g_ingame_shadow_rejections[3],
                g_ingame_shadow_rejections[4],g_ingame_shadow_rejections[5],
                g_ingame_shadow_rejections[6],g_ingame_shadow_rejections[7],
                g_ingame_shadow_capture_failure[0]?g_ingame_shadow_capture_failure:"none",
                g_ingame_shadow_pass->capture_diagnostic(),
                g_ingame_shadow_skip);
            log_line(message);
        }
    }
}

bool load_ingame_shadow_settings() {
    auto settings = a2fo::shadows::Settings{};
    const auto roots = a2fo::discover_extension_roots(g_root_directory,GetCommandLineA());
    for (const auto& root : roots.roots) {
        const std::string file = root + "\\A2FORenderer.ini";
        const auto number = [&](const char* key, const char* fallback) {
            char text[64]{};
            GetPrivateProfileStringA("Shadows",key,fallback,text,sizeof(text),file.c_str());
            return std::string(text);
        };
        const auto enabled = number("Enabled","");
        if (!enabled.empty()) {
            if (enabled != "0" && enabled != "1") return false;
            settings.enabled = enabled == "1";
        }
        const auto integer = [&](const char* key,unsigned& target) {
            const auto text = number(key,"");
            if (text.empty()) return true;
            char* end = nullptr;
            const unsigned long value = std::strtoul(text.c_str(),&end,10);
            if (!end || *end || value > UINT_MAX || text[0] == '-') return false;
            target = static_cast<unsigned>(value);return true;
        };
        const auto real = [&](const char* key,float& target) {
            const auto text = number(key,"");if (text.empty()) return true;
            char* end = nullptr;const float value = std::strtof(text.c_str(),&end);
            if (!end || *end || !std::isfinite(value)) return false;
            target = value;return true;
        };
        if (!integer("Size",settings.size) || !integer("MaxDraws",settings.max_draws) ||
            !real("Strength",settings.strength) || !real("Bias",settings.bias) ||
            !real("BiasWorld",settings.bias_world)) return false;
    }
    if (settings.size < 512 || settings.size > 4096 ||
        (settings.size & (settings.size-1)) || !settings.max_draws ||
        settings.max_draws > 2048 || settings.strength < 0 || settings.strength > 0.8f ||
        settings.bias < 0 || settings.bias > 0.02f ||
        (settings.bias_world != -1.0f &&
            (settings.bias_world < 0 || settings.bias_world > 100.0f))) return false;
    g_ingame_shadow_settings = settings;
    return true;
}

void install_ingame_shadow_hook(bool refit_sources=false) noexcept {
    g_refit_capture_mode=refit_sources;
    try {
        // Revoke producers before changing requested domains. Destruction
        // ownership must be armed before any load observer can publish.
        a2fo::native11_assets::disable();a2fo::native11_textures::disable();
        a2fo::native11_lifecycle::disable();
        a2fo::native11_sprites::disable();
        const char* section=refit_sources?"Refit":"Native11";
        bool capture_requested=refit_sources;
        const auto capture_roots=a2fo::discover_extension_roots(g_root_directory,GetCommandLineA());
        for(const auto& root:capture_roots.roots) {
            const std::string file=root+"\\A2FORenderer.ini";
            char value[16]{};
            GetPrivateProfileStringA(section,"CaptureScene",capture_requested?"1":"0",
                value,sizeof(value),file.c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")) {
                log_line("Native11 scene capture disabled: CaptureScene must be 0 or 1");
                capture_requested=false;
                break;
            }
            capture_requested=std::strcmp(value,"1")==0;
        }
        const bool capture_enabled=a2fo::native11_capture::initialize(g_armada,capture_requested,log_line);
        bool assets_requested=refit_sources;
        for(const auto& root:capture_roots.roots) {
            const std::string file=root+"\\A2FORenderer.ini";
            char value[16]{};
            GetPrivateProfileStringA(section,"CaptureAssets",assets_requested?"1":"0",
                value,sizeof(value),file.c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")) {
                log_line("Native11 model capture disabled: CaptureAssets must be 0 or 1");
                assets_requested=false;
                break;
            }
            assets_requested=std::strcmp(value,"1")==0;
        }
        bool textures_requested=refit_sources;
        for(const auto& root:capture_roots.roots){
            const std::string file=root+"\\A2FORenderer.ini";char value[16]{};
            GetPrivateProfileStringA(section,"CaptureTextures",textures_requested?"1":"0",value,sizeof(value),file.c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){
                textures_requested=false;log_line("Native11 DDS capture disabled: CaptureTextures must be 0 or 1");break;
            }
            textures_requested=std::strcmp(value,"1")==0;
        }
        const bool lifetime_enabled=a2fo::native11_lifecycle::initialize(g_armada,assets_requested,textures_requested,log_line,g_fleet_ops);
        if((assets_requested||textures_requested)&&!lifetime_enabled)
            log_line("Native11 asset producers disabled: destruction ownership unavailable");
        const bool assets_enabled=a2fo::native11_assets::initialize(g_armada,assets_requested&&lifetime_enabled,log_line,g_fleet_ops);
        const bool dds_enabled=a2fo::native11_textures::initialize(g_fleet_ops,textures_requested&&lifetime_enabled,log_line);
        const bool tga_enabled=a2fo::native11_textures::initialize_tga(g_armada,textures_requested&&lifetime_enabled,log_line,a2fo_builtin_rgb_owns_open_read,g_fleet_ops);
        bool sprites_requested=false;
        for(const auto& root:capture_roots.roots){
            const std::string file=root+"\\A2FORenderer.ini";char value[16]{};
            GetPrivateProfileStringA(section,"CaptureSprites",sprites_requested?"1":"0",value,sizeof(value),file.c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){
                sprites_requested=false;log_line("Native11 sprite tracing disabled: CaptureSprites must be 0 or 1");break;
            }
            sprites_requested=std::strcmp(value,"1")==0;
        }
        if(refit_sources)sprites_requested=false;
        if(sprites_requested&&(!capture_enabled||(!dds_enabled&&!tga_enabled))){
            sprites_requested=false;log_line("Native11 sprite tracing requires active scene and texture capture");
        }
        a2fo::native11_sprites::initialize(g_armada,g_fleet_ops,sprites_requested,log_line,native11_owns_bloom_refresh);
        bool materials_requested=refit_sources;
        for(const auto& root:capture_roots.roots){
            const std::string file=root+"\\A2FORenderer.ini";char value[16]{};
            GetPrivateProfileStringA(section,"CaptureMaterials",materials_requested?"1":"0",value,sizeof(value),file.c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){
                materials_requested=false;log_line("Native11 material capture disabled: CaptureMaterials must be 0 or 1");break;
            }
            materials_requested=std::strcmp(value,"1")==0;
        }
        if(materials_requested&&(!capture_enabled||!assets_enabled||(!dds_enabled&&!tga_enabled))){
            materials_requested=false;log_line("Native11 material capture requires active scene, model and texture producers");
        }
        const bool materials_enabled=a2fo::native11_materials::initialize(g_armada,materials_requested,log_line);
        bool nodes_requested=refit_sources;
        for(const auto& root:capture_roots.roots){
            const std::string file=root+"\\A2FORenderer.ini";char value[16]{};
            GetPrivateProfileStringA(section,"CaptureNodes",nodes_requested?"1":"0",value,sizeof(value),file.c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){
                nodes_requested=false;log_line("Native11 node capture disabled: CaptureNodes must be 0 or 1");break;
            }
            nodes_requested=std::strcmp(value,"1")==0;
        }
        if(nodes_requested&&!materials_enabled){nodes_requested=false;log_line("Native11 node capture requires active material capture");}
        const bool nodes_enabled=a2fo::native11_nodes::initialize(g_armada,nodes_requested,log_line);
        if(nodes_requested&&!nodes_enabled)log_line("Native11 posed material batches unavailable: requested node capture failed; authored fallback refused");
        bool clock_requested=false;
        for(const auto& root:capture_roots.roots){
            const std::string file=root+"\\A2FORenderer.ini";char value[16]{};
            GetPrivateProfileStringA(section,"CaptureAnimationClock",clock_requested?"1":"0",value,sizeof(value),file.c_str());
            if(std::strcmp(value,"0")&&std::strcmp(value,"1")){
                clock_requested=false;log_line("Native11 animation clock disabled: CaptureAnimationClock must be 0 or 1");break;
            }
            clock_requested=std::strcmp(value,"1")==0;
        }
        a2fo::native11_animation_clock::initialize(g_armada,clock_requested,log_line,g_fleet_ops,
            a2fo::builtin_animations_native_playback);
        // Class configuration/dispatch is completed later by the startup
        // worker. The first owned main view above performs actual admission
        // on its render thread; never bind the sampler to this worker.
        a2fo::native11_world_lights::initialize(g_armada,false,log_line);
        if(!refit_sources&&!a2fo::native11_world_pipeline::configure_from_roots(capture_roots.roots,log_line))
            log_line("Native11 offscreen world unavailable: configuration or session lifecycle refused");
        if(refit_sources)g_ingame_shadow_settings.enabled=false;
        if (!refit_sources&&!load_ingame_shadow_settings()) {
            g_ingame_shadow_settings.enabled = false;
            log_line("In-game shadows disabled: invalid inherited [Shadows] settings");
        }
        if (!g_ingame_shadow_settings.enabled && !capture_enabled) return;
        if (g_ingame_shadow_hook_installed) return;
        // The CALL rel32 is base-independent. Its target and full bytes must
        // match the supported native body; no foreign hook is overwritten.
        void* site = at(g_armada,kShadowMainCallRva);
        if (!readable_range(site,kShadowMainCall.size()) ||
            std::memcmp(site,kShadowMainCall.data(),kShadowMainCall.size()) ||
            !a2fo_rs_windows_patch_call(site,function_address(&ingame_shadow_world),
                kShadowMainCall.data(),kShadowMainCall.size())) {
            g_ingame_shadow_settings.enabled = false;
            a2fo::native11_capture::disable();
            a2fo::native11_materials::disable();
            a2fo::native11_nodes::disable();
            a2fo::native11_world_lights::disable();
            a2fo::native11_world_pipeline::shutdown();
            log_line("Main 3D view wrapper unavailable: call ownership mismatch; shadows and native11 capture disabled");
            return;
        }
        g_ingame_shadow_hook_installed = true;
        if(g_ingame_shadow_settings.enabled)
            log_line("In-game shadows armed: world-bias-audit-4; main-view opaque CPU snapshots; managed DXVK; prototype post-light overlay");
    } catch (...) {
        g_ingame_shadow_settings.enabled = false;
        a2fo::native11_capture::disable();
        a2fo::native11_sprites::disable();
        a2fo::native11_lifecycle::disable();
        a2fo::native11_assets::disable();
        a2fo::native11_textures::disable();
        a2fo::native11_materials::disable();
        a2fo::native11_nodes::disable();
            a2fo::native11_world_lights::disable();
            a2fo::native11_world_pipeline::shutdown();
        log_line("Main 3D view wrapper unavailable: configuration allocation failed");
    }
}

} // namespace

extern "C" void release_ingame_shadows() noexcept {
    const IngameShadowLock lock;
    if (g_ingame_shadow_pass) g_ingame_shadow_pass->release();
}

extern "C" void set_ingame_shadow_group(const void* group, UINT primitives) noexcept {
    g_ingame_shadow_group = g_ingame_shadow_view ? group : nullptr;
    g_ingame_shadow_group_primitives = primitives;
}

extern "C" void capture_ingame_shadow_draw(IDirect3DDevice8* device,
    D3DPRIMITIVETYPE type,UINT minimum,UINT vertices,UINT start,UINT primitives) noexcept {
    if (!g_ingame_shadow_view || !device) return;
    ++g_ingame_shadow_seen;
    void* craft = current_render_craft();
    if (!craft || g_craft_render_overflow || current_craft_cloak_state(craft) != 0) {
        ingame_shadow_reject(0,"no uncloaked Craft context");return;
    }
    if (!g_general_render_mesh || g_m3d_active_render_mesh ||
        type != D3DPT_TRIANGLELIST || minimum || start || !primitives ||
        primitives > 21845 || primitives != g_ingame_shadow_group_primitives) {
        ingame_shadow_reject(1,"outside native MeshVB/group route");return;
    }
    DWORD blend = 0,source = 0,destination = 0,test = 0,write = 0,depth = 0;
    DWORD stencil = 0,planes = 0,fill = 0;
    if (FAILED(device->GetRenderState(D3DRS_ALPHABLENDENABLE,&blend)) ||
        FAILED(device->GetRenderState(D3DRS_SRCBLEND,&source)) ||
        FAILED(device->GetRenderState(D3DRS_DESTBLEND,&destination)) ||
        FAILED(device->GetRenderState(D3DRS_ALPHATESTENABLE,&test)) ||
        FAILED(device->GetRenderState(D3DRS_ZWRITEENABLE,&write)) ||
        FAILED(device->GetRenderState(D3DRS_ZENABLE,&depth)) ||
        FAILED(device->GetRenderState(D3DRS_STENCILENABLE,&stencil)) ||
        FAILED(device->GetRenderState(D3DRS_CLIPPLANEENABLE,&planes)) ||
        FAILED(device->GetRenderState(D3DRS_FILLMODE,&fill)) ||
        test || !write || depth != D3DZB_TRUE || stencil || planes || fill != D3DFILL_SOLID ||
        (blend && (source != D3DBLEND_ONE || destination != D3DBLEND_ZERO))) {
        ingame_shadow_reject(2,"transparent, cutout, stencil, clipped, wireframe or non-depth-writing material");return;
    }
    const void* group = g_ingame_shadow_group;
    const UINT count = read_at<UINT>(group,0x10,0);
    const UINT triangles = read_at<UINT>(group,0x14,0);
    const auto* positions = read_at<const unsigned char*>(group,4,nullptr);
    const auto* indices = read_at<const unsigned short*>(group,0xc,nullptr);
    if (!count || count > 65535 || vertices != count || triangles < primitives ||
        !readable_range(positions,static_cast<std::size_t>(count)*68) ||
        !readable_range(indices,static_cast<std::size_t>(primitives)*6)) {
        ingame_shadow_reject(3,"native CPU vertex/index mirror rejected");return;
    }
    UINT base = 0;IDirect3DIndexBuffer8* buffer = nullptr;
    const HRESULT got = device->GetIndices(&buffer,&base);if (buffer) buffer->Release();
    if (FAILED(got) || !buffer || base) {
        ingame_shadow_reject(4,"unsupported native base vertex");return;
    }
    a2fo::shadows::Capture capture{};
    if (FAILED(device->GetVertexShaderConstant(2,capture.clip,4)) ||
        !ingame_shadow_key(capture.light_direction)) {
        ingame_shadow_reject(5,"native clip matrix or directional light unavailable");return;
    }
    try {
        g_ingame_shadow_positions.resize(static_cast<std::size_t>(primitives)*9);
        float low[3]{},high[3]{};
        for (UINT i = 0; i < primitives*3; ++i) {
            const unsigned index = indices[i];
            if (index >= count) { ingame_shadow_reject(6,"native index outside mirror");return; }
            for (unsigned axis = 0; axis < 3; ++axis) {
                float value = 0;
                // Both complete CPU mirrors were checked above and remain owned
                // by this synchronous native draw. Avoid VirtualQuery per float.
                std::memcpy(&value,positions + index*68 + axis*4,sizeof(value));
                if (!std::isfinite(value) || std::fabs(value) > 1e6f) {
                    ingame_shadow_reject(6,"nonfinite native position");return;
                }
                g_ingame_shadow_positions[i*3+axis] = value;
                if (!i) low[axis] = high[axis] = value;
                else { low[axis] = std::min(low[axis],value);high[axis] = std::max(high[axis],value); }
            }
        }
        for (unsigned axis = 0; axis < 3; ++axis) {
            capture.bounds_center[axis] = (low[axis]+high[axis])*0.5f;
            capture.bounds_extent[axis] = std::max(0.001f,(high[axis]-low[axis])*0.5f);
        }
        const IngameShadowLock lock;
        const a2fo::shadows::Draw draw{0,minimum,vertices,start,primitives};
        if (g_ingame_shadow_pass && g_ingame_shadow_pass->capture(device,capture,draw,
                g_ingame_shadow_positions.data(),primitives*3)) {
            ++g_ingame_shadow_captured;
            const std::array<const void*,3> sample_key{{craft,g_general_render_mesh,
                g_ingame_shadow_group}};
            const auto logged=static_cast<unsigned>(g_ingame_shadow_capture_logs);
            const bool known=std::find(g_ingame_shadow_sample_keys.begin(),
                g_ingame_shadow_sample_keys.begin()+logged,sample_key)!=
                g_ingame_shadow_sample_keys.begin()+logged;
            if (!known && logged<g_ingame_shadow_sample_keys.size()) {
                g_ingame_shadow_sample_keys[logged]=sample_key;
                ++g_ingame_shadow_capture_logs;
                char message[1000]{};
                const auto& lights=g_cloak_composite;
                const bool matching=lights.active && lights.craft==craft;
                std::snprintf(message,sizeof(message),
                    "In-game shadow sample: craft=%p mesh=%p craft-pos=(%.3f,%.3f,%.3f) collected-lights=%u/%u matching=%u; %.380s",
                    craft,g_general_render_mesh,read_at<float>(craft,0xac,NAN),
                    read_at<float>(craft,0xb0,NAN),read_at<float>(craft,0xb4,NAN),
                    static_cast<unsigned>(lights.count),static_cast<unsigned>(lights.capacity),
                    matching?1u:0u,
                    g_ingame_shadow_pass->capture_diagnostic());
                log_line(message);
                // Native c6 vectors point towards the source in mesh-local
                // space. Log every collected light, rather than suppressing
                // evidence whenever a map contains more than one light.
                if (matching && lights.count<=kCloakCompositeLights)
                    for (unsigned i=0;i<lights.count;++i) {
                        const auto& direction=lights.constants[i*2];
                        const auto& colour=lights.constants[i*2+1];
                        std::snprintf(message,sizeof(message),
                            "In-game shadow native-light: sample=%u index=%u local-to-source=(%.5f,%.5f,%.5f) rgb=(%.4f,%.4f,%.4f)",
                            logged+1,i,direction[0],direction[1],direction[2],
                            colour[0],colour[1],colour[2]);
                        log_line(message);
                    }
            }
        } else {
            ingame_shadow_reject(7,"GPU capture rejected");
            if (g_ingame_shadow_pass)
                std::snprintf(g_ingame_shadow_capture_failure,sizeof(g_ingame_shadow_capture_failure),
                    "%s",g_ingame_shadow_pass->diagnostic());
        }
    } catch (...) { ingame_shadow_reject(7,"snapshot allocation failed"); }
}
