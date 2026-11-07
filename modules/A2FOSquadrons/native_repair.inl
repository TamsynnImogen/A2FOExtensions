// Included inside module.cpp's anonymous namespace. Native repair is allowed
// to eject normally before paid production starts: holding RepairDock occupied
// would block Shipyard construction. No UI callbacks create or charge jobs.
constexpr std::uintptr_t kRepairQueueVtableRva = 0x2b3744;
constexpr std::uintptr_t kRepairActivateRva = 0x13a6e0;
constexpr std::uintptr_t kFoNeedsRepairRva = 0x1dd0b8;
A2FO_ProducerPushRefitFn g_push_repair_job = nullptr;
A2FO_ProducerCancelQueuedJobFn g_cancel_repair_job = nullptr;
bool g_reported_missing_repair_bridge = false;
void* g_repair_activate_original = nullptr;
A2FO_InlineHook g_needs_repair_hook{};
thread_local bool g_inside_repair_activation = false;
bool g_repair_ready = false;

struct RepairVisit {
    std::uint32_t yard = 0;
    // Actual native Activate admission, never inferred from proximity.
    std::unordered_set<std::uint32_t> serviced;
    a2fo::squadrons::TicketId ticket = 0;
    std::uint32_t queue_id = 0;
    float retry_seconds = 0;
};
std::map<SquadId, RepairVisit> g_repair_visits;

std::uint32_t craft_command(void* craft) noexcept {
    return read_at<std::uint32_t>(read_at<void*>(craft, 0x44), 0x3c, ~0u);
}

bool repair_compatible(void* yard, const a2fo::squadrons::Squadron& squad) {
    struct Context {
        void* yard;
        const a2fo::squadrons::Squadron& squad;
        decltype(g_yard_build_items)::iterator found{};
        std::exception_ptr exception{};
    } context{yard, squad, {}, {}};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0:
                c.found = g_yard_build_items.find(object_class(c.yard));
                return 1;
            case 1: return c.yard != nullptr;
            case 2: return !object_expired(c.yard);
            case 3: return object_team(c.yard) ==
                static_cast<std::int32_t>(c.squad.team);
            case 4: return read_at<void*>(read_at<void*>(c.yard, 0x2ac), 0) ==
                at(kRepairQueueVtableRva);
            case 5: return c.found != g_yard_build_items.end();
            case 6: return c.found->second.count(c.squad.definition.odf) != 0;
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    const bool compatible = a2fo_rs_stage8_squadrons_repair_compatible(
        step, &context) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return compatible;
}

bool near_repair_yard(void* craft, void* yard) noexcept {
    // Entity::GetTransform is exactly entity->geometry + 0x44 on this image.
    void* craft_geometry = read_at<void*>(craft, 4);
    void* yard_geometry = read_at<void*>(yard, 4);
    if (!readable_range(craft_geometry, 0x74) ||
        !readable_range(yard_geometry, 0x74)) return false;
    float distance=0.0f;
    // Checked native reads retain axis order; Rust preserves x87 delta precision
    // and the original float distance spill between each native read pair.
    for(std::size_t axis=0;axis<3;++axis){
        const auto craft_word=read_at<std::uint32_t>(craft_geometry,0x68+axis*4,0);
        const auto yard_word=read_at<std::uint32_t>(yard_geometry,0x68+axis*4,0);
        a2fo_rs_stage10_squadrons_axis(&distance,craft_word,yard_word);
    }
    const auto radius=read_at<std::uint32_t>(yard_geometry,0x40,0);
    std::uint32_t bits;std::memcpy(&bits,&distance,sizeof(bits));
    return a2fo_rs_stage10_squadrons_near(bits,radius)!=0;
}

// waiting permits survivors still passing through the repair/output queues.
// A paid job starts only once every survivor has actually been serviced and
// left those queues. The same observations are checked again at completion.
bool observe_repair(SquadId id, const RepairVisit& visit,
                    a2fo::squadrons::RepairContext* context, bool* waiting) {
    *waiting = false;
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    struct ObserveContext {
        SquadId id;
        const RepairVisit* visit;
        a2fo::squadrons::RepairContext* output;
        bool* waiting;
        const a2fo::squadrons::Squadron* squad = nullptr;
        void* yard = nullptr;
        const a2fo::squadrons::Slot* slot = nullptr;
        void* craft = nullptr;
        std::uint32_t command = 0;
        std::uint32_t stage = 0;
        std::exception_ptr exception{};

        ObserveContext(SquadId value, const RepairVisit* source,
                       a2fo::squadrons::RepairContext* result, bool* wait)
            : id(value), visit(source), output(result), waiting(wait) {}
    } state{id, &visit, context, waiting};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& s = *static_cast<ObserveContext*>(opaque);
        try {
            switch (op) {
            case 0: s.squad = g_registry.find(s.id); return 1;
            case 1: s.yard = find_entity(s.visit->yard); return 1;
            case 2: return s.squad != nullptr;
            case 3: return s.squad->definition.reinforce_at_yard;
            case 4: return repair_compatible(s.yard, *s.squad);
            case 5:
                *s.output = {s.visit->yard, s.squad->team, true, true, {}};
                return 1;
            case 6: return s.squad->slots.size();
            case 7:
                s.slot = &s.squad->slots[index];
                return s.slot->member != 0;
            case 8:
                s.craft = find_entity(
                    static_cast<std::uint32_t>(s.slot->member));
                return 1;
            case 9: return s.craft != nullptr;
            case 10: return object_expired(s.craft);
            case 11: return static_cast<std::uint32_t>(object_team(s.craft));
            case 12: s.command = craft_command(s.craft); return s.command;
            case 13:
                return s.visit->serviced.count(
                    static_cast<std::uint32_t>(s.slot->member)) != 0;
            case 14:
                s.stage = read_at<std::uint32_t>(
                    s.craft, kCraftQueueStageOffset);
                return s.stage;
            case 15:
                return read_at<std::uint32_t>(
                    s.craft, kCraftQueueOwnerOffset) == s.visit->yard;
            case 16: *s.waiting = true; return 1;
            case 17: return near_repair_yard(s.craft, s.yard);
            case 18:
                s.output->serviced_members.push_back(s.slot->member);
                return 1;
            case 19: return s.squad->live_count() != 0;
            case 20:
                return static_cast<std::uint32_t>(
                    static_cast<std::int32_t>(s.squad->team));
            default: return 0;
            }
        } catch (...) {
            s.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    const bool result = a2fo_rs_stage8_squadrons_observe_repair(
        step, &state) != 0;
    if (state.exception) std::rethrow_exception(state.exception);
    return result;
}

bool native_job_exists(void* yard, std::uint32_t id) noexcept {
    struct Context { void* yard; void* node = nullptr; } context{yard, nullptr};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0: c.node = read_at<void*>(c.yard, 0x270); break;
        case 1: return read_at<std::uint32_t>(c.node, 0x0c);
        case 2: c.node = read_at<void*>(c.node, 8); break;
        default: return 0;
        }
        return reinterpret_cast<std::uintptr_t>(c.node);
    };
    return a2fo_rs_stage8_squadrons_native_job_exists(
        step, &context, id) != 0;
}

void end_repair_visit(SquadId id, bool cancel_native) {
    struct Context {
        SquadId id;
        decltype(g_repair_visits)::iterator found{};
        std::optional<RepairVisit> visit;
        void* yard = nullptr;
        std::exception_ptr exception{};

        explicit Context(SquadId value) : id(value) {}
    } context{id};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0:
                c.found = g_repair_visits.find(c.id);
                return c.found != g_repair_visits.end();
            case 1: c.visit.emplace(c.found->second); return 1;
            case 2: g_repair_visits.erase(c.found); return 1;
            case 3: {
                std::lock_guard<std::mutex> lock(g_registry_mutex);
                g_registry.cancel_replacement(c.visit->ticket);
                return 1;
            }
            case 4: c.yard = find_entity(c.visit->yard); return 1;
            case 5: return c.visit->queue_id != 0;
            case 6: return c.yard != nullptr;
            case 7: return object_expired(c.yard);
            case 8: return native_job_exists(c.yard, c.visit->queue_id);
            case 9: return g_cancel_repair_job != nullptr;
            case 10:
                g_cancel_repair_job(c.yard, c.visit->queue_id);
                return 1;
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_squadrons_end_repair(
        step, &context, static_cast<std::uint8_t>(cancel_native));
    if (context.exception) std::rethrow_exception(context.exception);
}

std::uintptr_t __attribute__((fastcall)) squad_needs_repair(
    void* craft, void*, float* crew, std::uint32_t shields, std::uint32_t threshold) noexcept {
    struct Context {
        void* craft;
        float* crew;
        std::uint32_t shields;
        std::uint32_t threshold;
        std::unique_lock<std::mutex> lock;
        const a2fo::squadrons::Squadron* squad = nullptr;
        decltype(g_repair_visits)::iterator visit{};

        Context(void* value, float* crew_value, std::uint32_t shield_value,
                std::uint32_t threshold_value)
            : craft(value), crew(crew_value), shields(shield_value),
              threshold(threshold_value) {}
    } context{craft, crew, shields, threshold};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0:
            return a2fo_squadrons_call_thiscall_3(
                g_needs_repair_hook.gateway, c.craft,
                reinterpret_cast<std::uintptr_t>(c.crew),
                c.shields, c.threshold);
        case 1: return g_runtime_ready;
        case 2: return g_repair_ready;
        case 3: return g_inside_repair_activation;
        case 4:
            c.lock = std::unique_lock<std::mutex>(g_registry_mutex);
            c.squad = g_registry.containing(object_handle(c.craft));
            return c.squad != nullptr;
        case 5: return c.squad->definition.reinforce_at_yard;
        case 6: return c.squad->live_count() == c.squad->slots.size();
        case 7: return g_pending_launches.size();
        case 8: return g_pending_launches[index].squad == c.squad->id;
        case 9:
            c.visit = g_repair_visits.find(c.squad->id);
            return c.visit != g_repair_visits.end();
        case 10:
            return c.visit->second.serviced.count(object_handle(c.craft)) != 0;
        default: return 0;
        }
    };
    return a2fo_rs_stage8_squadrons_needs_repair(step, &context);
}

std::uintptr_t __attribute__((fastcall)) squad_repair_activate(
    void* queue, void*, void* craft, std::uint32_t elapsed_bits) noexcept {
    struct Context {
        void* queue;
        void* craft;
        std::uint32_t elapsed_bits;
        void* yard = nullptr;
        std::unique_lock<std::mutex> lock;
        const a2fo::squadrons::Squadron* squad = nullptr;
        decltype(g_repair_visits)::iterator visit{};
        bool previous = false;
        std::exception_ptr exception{};

        Context(void* q, void* c, std::uint32_t bits)
            : queue(q), craft(c), elapsed_bits(bits) {}
    } context{queue, craft, elapsed_bits};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        if (op >= 12) {
            switch (op) {
            case 12:
                c.previous = g_inside_repair_activation;
                g_inside_repair_activation = true;
                return 1;
            case 13:
                return a2fo_squadrons_call_thiscall_2(
                    g_repair_activate_original, c.queue,
                    reinterpret_cast<std::uintptr_t>(c.craft), c.elapsed_bits);
            case 14: g_inside_repair_activation = c.previous; return 1;
            case 15:
                log_line("Squad repair observation failed; native repair continues");
                return 1;
            case 16: if (c.lock.owns_lock()) c.lock.unlock(); return 1;
            case 17: return static_cast<bool>(c.exception);
            default: return 0;
            }
        }
        try {
            switch (op) {
            case 0: return g_runtime_ready;
            case 1: return g_repair_ready;
            case 2: return craft_command(c.craft);
            case 3: c.yard = read_at<void*>(c.queue, 0x1c); return 1;
            case 4:
                c.lock = std::unique_lock<std::mutex>(g_registry_mutex);
                c.squad = g_registry.containing(object_handle(c.craft));
                return c.squad != nullptr;
            case 5: return c.squad->definition.reinforce_at_yard;
            case 6: return c.squad->live_count() < c.squad->slots.size();
            case 7: return repair_compatible(c.yard, *c.squad);
            case 8:
                c.visit = g_repair_visits.find(c.squad->id);
                return c.visit != g_repair_visits.end();
            case 9: {
                RepairVisit visit;
                visit.yard = object_handle(c.yard);
                c.visit = g_repair_visits.emplace(
                    c.squad->id, std::move(visit)).first;
                log_line("Squad repair accepted: waiting for survivors to clear the repair berth");
                return 1;
            }
            case 10: return c.visit->second.yard == object_handle(c.yard);
            case 11:
                c.visit->second.serviced.insert(object_handle(c.craft));
                return 1;
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    return a2fo_rs_stage8_squadrons_activate_repair(step, &context);
}

void update_reinforcement(void* craft, std::uint32_t kind, float seconds) {
    struct Context {
        void* craft;
        float seconds;
        HMODULE module = nullptr;
        FARPROC function = nullptr;
        FARPROC cancel = nullptr;
        FARPROC ready_export = nullptr;
        using ProducerEventsReadyFn = bool (A2FO_CALL*)() noexcept;
        ProducerEventsReadyFn producer_events_ready = nullptr;
        std::uint32_t handle = 0;
        decltype(g_repair_visits)::iterator scan{};
        std::unique_lock<std::mutex> lock;
        const a2fo::squadrons::Squadron* squad = nullptr;
        std::vector<SquadId> affected;
        SquadId id = 0;
        decltype(g_repair_visits)::iterator found{};
        std::optional<a2fo::squadrons::RepairContext> repair;
        bool waiting = false;
        void* yard = nullptr;
        a2fo::squadrons::Result<a2fo::squadrons::Replacement> request;
        void* target = nullptr;
        std::uint32_t queue_id = 0;
        std::exception_ptr exception{};

        Context(void* value, float elapsed) : craft(value), seconds(elapsed) {}
    } context{craft, seconds};
    std::uint32_t seconds_bits = 0;
    static_assert(sizeof(seconds_bits) == sizeof(seconds));
    std::memcpy(&seconds_bits, &seconds, sizeof(seconds_bits));
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t value) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0: return g_repair_ready;
            case 1: return !g_push_repair_job || !g_cancel_repair_job;
            case 2:
                c.module = a2fo::startup_module_provider(
                    "A2FOFeaturePack.dll",
                    {"A2FO_ProducerPushRefit", "A2FO_ProducerCancelQueuedJob",
                     "A2FO_ProducerEventsReady"});
                c.function = c.module
                    ? GetProcAddress(c.module, "A2FO_ProducerPushRefit") : nullptr;
                c.cancel = c.module
                    ? GetProcAddress(c.module, "A2FO_ProducerCancelQueuedJob") : nullptr;
                c.ready_export = c.module
                    ? GetProcAddress(c.module, "A2FO_ProducerEventsReady") : nullptr;
                static_assert(sizeof(c.ready_export) == sizeof(c.producer_events_ready));
                std::memcpy(&c.producer_events_ready, &c.ready_export,
                            sizeof(c.producer_events_ready));
                return 1;
            case 3: return c.producer_events_ready && c.producer_events_ready();
            case 4:
                static_assert(sizeof(c.function) == sizeof(g_push_repair_job));
                static_assert(sizeof(c.cancel) == sizeof(g_cancel_repair_job));
                std::memcpy(&g_push_repair_job, &c.function, sizeof(c.function));
                std::memcpy(&g_cancel_repair_job, &c.cancel, sizeof(c.cancel));
                return 1;
            case 5: return !g_push_repair_job || !g_cancel_repair_job;
            case 6: return !g_reported_missing_repair_bridge;
            case 7:
                log_line("Squad reinforcement requires ready A2FOFeaturePack paid push/exact-cancel bridge");
                g_reported_missing_repair_bridge = true;
                return 1;
            case 8: c.handle = object_handle(c.craft); return c.handle;
            case 9: c.scan = g_repair_visits.begin(); return 1;
            case 10: return c.scan != g_repair_visits.end();
            case 12:
                c.lock = std::unique_lock<std::mutex>(g_registry_mutex);
                c.squad = g_registry.find(c.scan->first);
                return 1;
            case 13: return c.squad == nullptr;
            case 14: return c.scan->second.yard == c.handle;
            case 15: return c.squad->representative() == c.handle;
            case 16: return c.scan->second.serviced.count(c.handle) != 0;
            case 17: c.affected.push_back(c.scan->first); return 1;
            case 18:
                c.lock.unlock();
                ++c.scan;
                return 1;
            case 19: return c.affected.size();
            case 20:
                c.id = c.affected[value];
                c.found = g_repair_visits.find(c.id);
                c.repair.reset();
                c.waiting = false;
                c.queue_id = 0;
                return c.found != g_repair_visits.end();
            case 21: return c.found->second.yard == c.handle;
            case 22: end_repair_visit(c.id, false); return 1;
            case 23:
                c.repair.emplace();
                return observe_repair(c.id, c.found->second,
                                      &*c.repair, &c.waiting);
            case 24: end_repair_visit(c.id, true); return 1;
            case 25: c.yard = find_entity(c.found->second.yard); return 1;
            case 26: return c.found->second.queue_id;
            case 27: return native_job_exists(c.yard, c.found->second.queue_id);
            case 28: end_repair_visit(c.id, false); return 1;
            case 29: return c.waiting;
            case 30: return g_push_repair_job != nullptr;
            case 31: return g_cancel_repair_job != nullptr;
            case 32:
                c.lock = std::unique_lock<std::mutex>(g_registry_mutex);
                c.squad = g_registry.find(c.id);
                return 1;
            case 33: return c.squad != nullptr;
            case 34: return c.squad->representative() == c.handle;
            case 35: return c.squad->live_count() == c.squad->slots.size();
            case 36: g_repair_visits.erase(c.found); return 1;
            case 37: c.lock.unlock(); return 1;
            case 38:a2fo_rs_stage10_squadrons_retry(&c.found->second.retry_seconds,&c.seconds);return 1;
            case 39:{
                std::uint32_t bits;std::memcpy(&bits,&c.found->second.retry_seconds,sizeof(bits));
                return a2fo_rs_stage10_squadrons_retry_pending(bits);
            }
            case 40: c.found->second.retry_seconds = 1.0f; return 1;
            case 41: {
                std::lock_guard<std::mutex> lock(g_registry_mutex);
                c.request = g_registry.reserve_replacement(c.id, *c.repair);
                return static_cast<bool>(c.request);
            }
            case 42:
                c.target = find_class(c.request.value->odf);
                c.queue_id = 0;
                return 1;
            case 43: return reinterpret_cast<std::uintptr_t>(c.target);
            case 44:
                c.queue_id = g_push_repair_job(c.yard, c.target);
                return 1;
            case 45: return c.queue_id;
            case 46: {
                std::lock_guard<std::mutex> lock(g_registry_mutex);
                g_registry.cancel_replacement(c.request.value->ticket);
                return 1;
            }
            case 47: c.found->second.ticket = c.request.value->ticket; return 1;
            case 48: c.found->second.queue_id = c.queue_id; return 1;
            case 49:
                log_text("Squad replacement queued at normal cost/build time: " +
                         c.request.value->odf);
                return 1;
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_squadrons_update_repair(
        step, &context, kind, A2FO_CRAFT_EVENT_CLEANUP,
        A2FO_CRAFT_EVENT_SIMULATE_POST, seconds_bits);
    if (context.exception) std::rethrow_exception(context.exception);
}

void finish_reinforcement(void* yard, std::uint32_t queue_id, void* result) {
    struct Context {
        void* yard;
        std::uint32_t queue_id;
        void* result;
        decltype(g_repair_visits)::iterator it{};
        std::optional<a2fo::squadrons::RepairContext> repair;
        bool waiting = false;
        std::string odf;
        std::exception_ptr exception{};

        Context(void* producer, std::uint32_t id, void* output)
            : yard(producer), queue_id(id), result(output) {}
    } context{yard, queue_id, result};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0: c.it = g_repair_visits.begin(); return 1;
            case 1: return c.it != g_repair_visits.end();
            case 2: return c.it->second.yard;
            case 3: return object_handle(c.yard);
            case 4: return c.queue_id != 0;
            case 5: return c.it->second.queue_id == c.queue_id;
            case 6: ++c.it; return 1;
            case 7: c.repair.emplace(); return c.result != nullptr;
            case 8:
                return observe_repair(c.it->first, c.it->second,
                                      &*c.repair, &c.waiting);
            case 9: return c.waiting;
            case 10:
                c.odf = class_odf_name(object_class(c.result));
                return 1;
            case 11: {
                std::lock_guard<std::mutex> lock(g_registry_mutex);
                return g_registry.complete_replacement(
                    c.it->second.ticket,
                    {object_handle(c.result),
                     static_cast<std::uint32_t>(object_team(c.result)), c.odf},
                    *c.repair);
            }
            case 12: end_repair_visit(c.it->first, false); return 1;
            case 13:
                log_line("Squad replacement completed outside its repair cycle; retained as an ordinary ship");
                return 1;
            case 14:
                c.it->second.serviced.insert(object_handle(c.result));
                return 1;
            case 15: c.it->second.ticket = 0; return 1;
            case 16: c.it->second.queue_id = 0; return 1;
            case 17: c.it->second.retry_seconds = 0; return 1;
            case 18:
                log_line("Paid replacement joined its squad and entered native yard output");
                return 1;
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_squadrons_finish_repair(step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
}

bool install_repair_hooks() noexcept {
    const std::uint8_t needs_signature[]{0x55, 0x8b, 0xec, 0x83, 0xc4, 0xcc};
    const std::uint8_t activate_signature[]{0x55, 0x8b, 0xec, 0x51, 0x53};
    struct Context {
        const std::uint8_t* needs_signature;
        const std::uint8_t* activate_signature;
        std::uint8_t* fo = nullptr;
        void** slot = nullptr;
        DWORD previous = 0;
        bool installed = false;
    } context{needs_signature, activate_signature, nullptr, nullptr, 0, false};
    // FeaturePack owns ActDelete and has already detoured it in a normal mod
    // load. Never demand its original prologue or call its grouped UI handler.
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        switch (op) {
        case 0:
            c.fo = reinterpret_cast<std::uint8_t*>(g_fleet_ops);
            c.slot = reinterpret_cast<void**>(at(kRepairQueueVtableRva + 0x2c));
            return 1;
        case 1: return g_api->install_inline_hook != nullptr;
        case 2: return readable_range(c.slot, 4);
        case 3: return *c.slot == at(kRepairActivateRva);
        case 4: return readable_range(at(kRepairActivateRva), 5);
        case 5: return std::memcmp(at(kRepairActivateRva), c.activate_signature, 5) == 0;
        case 6:
            log_line("Squad repair Activate vtable/signature unavailable");
            return 1;
        case 7: return VirtualProtect(c.slot, 4, PAGE_READWRITE, &c.previous) != 0;
        case 8:
            c.installed = g_api->install_inline_hook(
                c.fo + kFoNeedsRepairRva,
                reinterpret_cast<void*>(&squad_needs_repair), 6,
                c.needs_signature, &g_needs_repair_hook);
            return c.installed;
        case 9:
            g_repair_activate_original = *c.slot;
            InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(c.slot),
                reinterpret_cast<void*>(&squad_repair_activate));
            g_repair_ready = true;
            return 1;
        case 10:
            log_line("Squad NeedsRepair hook rejected: entry signature or hook installation failed");
            return 1;
        case 11: {
            DWORD ignored;
            VirtualProtect(c.slot, 4, c.previous, &ignored);
            return 1;
        }
        default: return 0;
        }
    };
    return a2fo_rs_stage8_squadrons_install_repair(step, &context) != 0;
}
