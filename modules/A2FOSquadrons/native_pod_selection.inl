// Opt-in selection of a ResearchStation and its actual attached shipyards.
// Layout proof: ArmadaL RVA b9a5a/b9a6c initializes count/list at 2ac/2b0;
// ba68f stores each constructed child there; ba7ea removes that exact child.
// No ResearchPod layout is assumed for shipyard children. Reverse lookup uses
// only tracked, live roots and their native lists, never distance or ODF alone.
constexpr std::size_t kStationPodCountOffset = 0x2ac;
constexpr std::size_t kStationPodsOffset = 0x2b0;
constexpr std::uintptr_t kResearchStationVtableRva = 0x2b32a4;
constexpr std::uint32_t kMaxStationPodSlots = 64;
bool g_pod_group_layout_ready = false;
std::mutex g_pod_group_mutex;
std::unordered_set<void*> g_pod_group_classes;
std::unordered_set<void*> g_pod_yard_classes;
std::unordered_set<std::uint32_t> g_pod_group_roots;

bool pod_group_layout_matches() noexcept {
    const auto step = +[](void*, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        switch (op) {
        case 0: {
            const std::uint8_t expected[]{0x89,0x86,0xac,0x02,0,0};
            return signature_matches(0xb9a5a, expected);
        }
        case 1: {
            const std::uint8_t expected[]{0x89,0x86,0xb0,0x02,0,0};
            return signature_matches(0xb9a6c, expected);
        }
        case 2: {
            const std::uint8_t expected[]{0x8b,0x8e,0xb0,0x02,0,0,0x89,0x04,0x99};
            return signature_matches(0xba68f, expected);
        }
        case 3: {
            const std::uint8_t expected[]{
                0x8b,0x8e,0xb0,0x02,0,0,0xc7,0x04,0x81,0,0,0,0};
            return signature_matches(0xba7ea, expected);
        }
        default: return 0;
        }
    };
    return a2fo_rs_stage8_squadrons_pod_layout(step, nullptr) != 0;
}

void capture_pod_group_class(void* klass, const std::string& label,
                             const OdfFields& fields) {
    std::lock_guard<std::mutex> lock(g_pod_group_mutex);
    struct BytesView { const std::uint8_t* data; std::size_t size; };
    struct Context {
        void* klass;
        const OdfFields* fields;
        std::string selection;
        BytesView view{};
        std::exception_ptr exception{};
    } context{klass, &fields, {}, {}, {}};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0: g_pod_group_classes.erase(c.klass); return 1;
            case 1: g_pod_yard_classes.erase(c.klass); return 1;
            case 2: g_pod_yard_classes.insert(c.klass); return 1;
            case 3:
                c.selection = field_value(*c.fields, "selectPodsTogether");
                c.view = {reinterpret_cast<const std::uint8_t*>(
                              c.selection.data()), c.selection.size()};
                return reinterpret_cast<std::uintptr_t>(&c.view);
            case 4: return g_pod_group_layout_ready;
            case 5: g_pod_group_classes.insert(c.klass); return 1;
            case 6:
                log_line("Registered selectPodsTogether research station");
                return 1;
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_squadrons_capture_pod(
        step, &context, reinterpret_cast<const std::uint8_t*>(label.data()),
        label.size());
    if (context.exception) std::rethrow_exception(context.exception);
}

bool is_pod_group_root(void* craft) noexcept {
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        void* craft = opaque;
        switch (op) {
        case 0: return g_pod_group_layout_ready;
        case 1: return craft != nullptr;
        case 2: return !object_expired(craft);
        case 3: return g_pod_group_classes.count(object_class(craft)) != 0;
        case 4: return read_at<void*>(craft, 0, nullptr) ==
            at(kResearchStationVtableRva);
        default: return 0;
        }
    };
    return a2fo_rs_stage8_squadrons_pod_root(step, craft) != 0;
}

void track_pod_group_root(void* craft, std::uint32_t kind) {
    std::lock_guard<std::mutex> lock(g_pod_group_mutex);
    struct Context {
        void* craft;
        std::uint32_t handle = 0;
        std::exception_ptr exception{};
    } context{craft, 0, {}};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0: c.handle = object_handle(c.craft); return 1;
            case 1: g_pod_group_roots.erase(c.handle); return 1;
            case 2: return is_pod_group_root(c.craft);
            case 3: g_pod_group_roots.insert(c.handle); return 1;
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_squadrons_track_pod(
        step, &context, kind, A2FO_CRAFT_EVENT_CLEANUP,
        A2FO_CRAFT_EVENT_SIMULATE_POST, A2FO_CRAFT_EVENT_POST_LOAD);
    if (context.exception) std::rethrow_exception(context.exception);
}

template <typename Resolve>
std::vector<std::uint32_t> attached_yard_peers(void* craft, Resolve resolve) {
    std::lock_guard<std::mutex> lock(g_pod_group_mutex);
    struct Context {
        void* craft;
        Resolve resolve;
        std::vector<std::uint32_t> result;
        std::uint32_t handle = 0;
        std::int32_t team = -1;
        bool root_click = false;
        decltype(g_pod_group_roots)::iterator it{};
        std::uint32_t root_handle = 0;
        void* root = nullptr;
        std::uint32_t count = 0;
        void* slots = nullptr;
        bool contains_clicked = false;
        std::optional<std::vector<std::uint32_t>> members;
        void* child = nullptr;
        std::uint32_t child_handle = 0;
        std::exception_ptr exception{};

        Context(void* value, Resolve fn) : craft(value), resolve(fn) {}
    } context{craft, resolve};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t value) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0: return g_pod_group_layout_ready;
            case 1: return c.craft != nullptr;
            case 2: return object_expired(c.craft);
            case 3: c.handle = object_handle(c.craft); return c.handle;
            case 4:
                c.team = object_team(c.craft);
                return static_cast<std::uint32_t>(c.team);
            case 5: return c.resolve(c.handle) == c.craft;
            case 6:
                c.root_click = is_pod_group_root(c.craft);
                return c.root_click;
            case 7: return g_pod_yard_classes.count(object_class(c.craft)) != 0;
            case 8: g_pod_group_roots.insert(c.handle); return 1;
            case 9: c.it = g_pod_group_roots.begin(); return 1;
            case 10: return c.it != g_pod_group_roots.end();
            case 11: c.root_handle = *c.it; return 1;
            case 12:
                c.root = c.resolve(c.root_handle);
                return is_pod_group_root(c.root) &&
                    object_handle(c.root) == c.root_handle;
            case 13: c.it = g_pod_group_roots.erase(c.it); return 1;
            case 14: ++c.it; return 1;
            case 15: return static_cast<std::uint32_t>(object_team(c.root));
            case 16:
                c.count = read_at<std::uint32_t>(
                    c.root, kStationPodCountOffset, kMaxStationPodSlots + 1);
                return c.count;
            case 17:
                c.slots = read_at<void*>(c.root, kStationPodsOffset, nullptr);
                return 1;
            case 18:
                return readable_range(c.slots, value * sizeof(void*));
            case 19:
                c.contains_clicked = c.root == c.craft;
                c.members.emplace(std::initializer_list<std::uint32_t>{
                    c.root_handle});
                return 1;
            case 20:
                c.child = read_at<void*>(c.slots,
                    value * sizeof(void*), nullptr);
                return 1;
            case 21: return c.child != nullptr;
            case 22: return object_expired(c.child);
            case 23: return object_team(c.child) == c.team;
            case 24: return g_pod_yard_classes.count(object_class(c.child)) != 0;
            case 25: c.child_handle = object_handle(c.child); return c.child_handle;
            case 26: return c.resolve(c.child_handle) == c.child;
            case 27:
                if (c.child == c.craft) c.contains_clicked = true;
                return 1;
            case 28:{
                const auto equal=+[](void* opaque,std::uint32_t op,std::uintptr_t index) noexcept -> std::uintptr_t {
                    const auto& c=*static_cast<Context*>(opaque);
                    return op==0 ? c.members->size() : static_cast<std::uintptr_t>((*c.members)[index]==c.child_handle);
                };
                return !a2fo_rs_stage10_squadrons_scan(&c,equal);
            }
            case 29: c.members->push_back(c.child_handle); return 1;
            case 30: return c.contains_clicked;
            case 31: return c.members->size();
            case 32: return (*c.members)[value];
            case 33: c.result.push_back(static_cast<std::uint32_t>(value)); return 1;
            case 34: return c.root == c.craft;
            case 35: c.members.reset(); return 1;
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_squadrons_attached_peers(step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
    return std::move(context.result);
}
