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

bool repair_command(std::uint32_t command) noexcept {
    return command == 0x11 || command == 0x35;
}

bool repair_compatible(void* yard, const a2fo::squadrons::Squadron& squad) {
    const auto found = g_yard_build_items.find(object_class(yard));
    return yard && !object_expired(yard) &&
        object_team(yard) == static_cast<std::int32_t>(squad.team) &&
        read_at<void*>(read_at<void*>(yard, 0x2ac), 0) == at(kRepairQueueVtableRva) &&
        found != g_yard_build_items.end() && found->second.count(squad.definition.odf);
}

bool near_repair_yard(void* craft, void* yard) noexcept {
    // Entity::GetTransform is exactly entity->geometry + 0x44 on this image.
    void* craft_geometry = read_at<void*>(craft, 4);
    void* yard_geometry = read_at<void*>(yard, 4);
    if (!readable_range(craft_geometry, 0x74) ||
        !readable_range(yard_geometry, 0x74)) return false;
    float distance = 0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const float delta = read_at<float>(craft_geometry, 0x68 + axis * 4) -
                            read_at<float>(yard_geometry, 0x68 + axis * 4);
        distance += delta * delta;
    }
    // Include the yard's native bounding sphere plus its local approach area.
    const float radius = read_at<float>(yard_geometry, 0x40);
    const float limit = 512.0f + (std::isfinite(radius) ? std::max(radius, 0.0f) : 0.0f);
    return std::isfinite(distance) && distance <= limit * limit;
}

// waiting permits survivors still passing through the repair/output queues.
// A paid job starts only once every survivor has actually been serviced and
// left those queues. The same observations are checked again at completion.
bool observe_repair(SquadId id, const RepairVisit& visit,
                    a2fo::squadrons::RepairContext* context, bool* waiting) {
    *waiting = false;
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    const auto* squad = g_registry.find(id);
    void* yard = find_entity(visit.yard);
    if (!squad || !squad->definition.reinforce_at_yard ||
        !repair_compatible(yard, *squad)) return false;
    *context = {visit.yard, squad->team, true, true, {}};
    for (const auto& slot : squad->slots) {
        if (!slot.member) continue;
        void* craft = find_entity(static_cast<std::uint32_t>(slot.member));
        if (!craft || object_expired(craft) ||
            object_team(craft) != static_cast<std::int32_t>(squad->team)) return false;
        const auto command = craft_command(craft);
        // Native Starbase::FinishBuild gives its output command 0x0a.
        // It remains an accepted yard output while that launch command runs.
        if (!repair_command(command) && command != 0 && command != 3 &&
            !(command == 0x0a && visit.serviced.count(static_cast<std::uint32_t>(slot.member))))
            return false;
        const auto stage = read_at<std::uint32_t>(craft, kCraftQueueStageOffset);
        if (stage && read_at<std::uint32_t>(craft, kCraftQueueOwnerOffset) != visit.yard)
            return false;
        if (!visit.serviced.count(static_cast<std::uint32_t>(slot.member))) {
            // A peer can still be travelling to its accepted repair order.
            if (!repair_command(command)) return false;
            *waiting = true;
        } else {
            if (!near_repair_yard(craft, yard)) return false;
            context->serviced_members.push_back(slot.member);
            if (stage) *waiting = true;
        }
    }
    return squad->live_count() != 0;
}

bool native_job_exists(void* yard, std::uint32_t id) noexcept {
    void* node = read_at<void*>(yard, 0x270);
    for (unsigned i = 0; node && i < 10; ++i, node = read_at<void*>(node, 8))
        if (read_at<std::uint32_t>(node, 0x0c) == id) return true;
    return false;
}

void end_repair_visit(SquadId id, bool cancel_native) {
    const auto found = g_repair_visits.find(id);
    if (found == g_repair_visits.end()) return;
    const auto visit = found->second;
    g_repair_visits.erase(found); // Native deletion may dispatch callbacks.
    {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        g_registry.cancel_replacement(visit.ticket);
    }
    void* yard = find_entity(visit.yard);
    if (cancel_native && visit.queue_id && yard && !object_expired(yard) &&
        native_job_exists(yard, visit.queue_id) && g_cancel_repair_job)
        g_cancel_repair_job(yard, visit.queue_id);
}

std::uintptr_t __attribute__((fastcall)) squad_needs_repair(
    void* craft, void*, float* crew, std::uint32_t shields, std::uint32_t threshold) noexcept {
    const auto native = a2fo_squadrons_call_thiscall_3(g_needs_repair_hook.gateway, craft,
        reinterpret_cast<std::uintptr_t>(crew), shields, threshold);
    if (!g_runtime_ready || !g_repair_ready || g_inside_repair_activation || (native & 0xff))
        return native;
    std::lock_guard<std::mutex> lock(g_registry_mutex);
    const auto* squad = g_registry.containing(object_handle(craft));
    if (!squad || !squad->definition.reinforce_at_yard ||
        squad->live_count() == squad->slots.size()) return native;
    for (const auto& pending : g_pending_launches) if (pending.squad == squad->id) return native;
    const auto visit = g_repair_visits.find(squad->id);
    // Let native repair and output finish after actual berth admission.
    if (visit != g_repair_visits.end() && visit->second.serviced.count(object_handle(craft)))
        return native;
    return 1;
}

std::uintptr_t __attribute__((fastcall)) squad_repair_activate(
    void* queue, void*, void* craft, std::uint32_t elapsed_bits) noexcept {
    try {
        if (g_runtime_ready && g_repair_ready && repair_command(craft_command(craft))) {
            void* yard = read_at<void*>(queue, 0x1c);
            std::lock_guard<std::mutex> lock(g_registry_mutex);
            const auto* squad = g_registry.containing(object_handle(craft));
            if (squad && squad->definition.reinforce_at_yard &&
                squad->live_count() < squad->slots.size() && repair_compatible(yard, *squad)) {
                auto found = g_repair_visits.find(squad->id);
                if (found == g_repair_visits.end()) {
                    RepairVisit visit;
                    visit.yard = object_handle(yard);
                    found = g_repair_visits.emplace(squad->id, std::move(visit)).first;
                    log_line("Squad repair accepted: waiting for survivors to clear the repair berth");
                }
                if (found->second.yard == object_handle(yard))
                    found->second.serviced.insert(object_handle(craft));
            }
        }
    } catch (...) { log_line("Squad repair observation failed; native repair continues"); }
    const bool previous = g_inside_repair_activation;
    g_inside_repair_activation = true;
    const auto result = a2fo_squadrons_call_thiscall_2(g_repair_activate_original, queue,
        reinterpret_cast<std::uintptr_t>(craft), elapsed_bits);
    g_inside_repair_activation = previous;
    return result;
}

void update_reinforcement(void* craft, std::uint32_t kind, float seconds) {
    if (!g_repair_ready) return;
    if (!g_push_repair_job || !g_cancel_repair_job) {
        const auto module = GetModuleHandleA("A2FOFeaturePack.dll");
        const auto function = module ? GetProcAddress(module, "A2FO_ProducerPushRefit") : nullptr;
        const auto cancel = module ? GetProcAddress(module, "A2FO_ProducerCancelQueuedJob") : nullptr;
        static_assert(sizeof(function) == sizeof(g_push_repair_job));
        static_assert(sizeof(cancel) == sizeof(g_cancel_repair_job));
        std::memcpy(&g_push_repair_job, &function, sizeof(function));
        std::memcpy(&g_cancel_repair_job, &cancel, sizeof(cancel));
        if ((!g_push_repair_job || !g_cancel_repair_job) && !g_reported_missing_repair_bridge) {
            log_line("Squad reinforcement requires updated A2FOFeaturePack: paid push/exact-cancel bridge unavailable");
            g_reported_missing_repair_bridge = true;
        }
    }
    const auto handle = object_handle(craft);
    std::vector<SquadId> affected;
    for (const auto& entry : g_repair_visits) {
        std::lock_guard<std::mutex> lock(g_registry_mutex);
        const auto* squad = g_registry.find(entry.first);
        if (!squad || entry.second.yard == handle || squad->representative() == handle ||
            (kind == A2FO_CRAFT_EVENT_CLEANUP && entry.second.serviced.count(handle)))
            affected.push_back(entry.first);
    }
    for (auto id : affected) {
        auto found = g_repair_visits.find(id);
        if (found == g_repair_visits.end()) continue;
        if (kind == A2FO_CRAFT_EVENT_CLEANUP && found->second.yard == handle) {
            end_repair_visit(id, false);
            continue;
        }
        a2fo::squadrons::RepairContext context;
        bool waiting = false;
        if (!observe_repair(id, found->second, &context, &waiting)) {
            end_repair_visit(id, true);
            continue;
        }
        auto& visit = found->second;
        void* yard = find_entity(visit.yard);
        if (visit.queue_id) {
            if (!native_job_exists(yard, visit.queue_id)) end_repair_visit(id, false);
            continue;
        }
        if (waiting || !g_push_repair_job || !g_cancel_repair_job ||
            kind != A2FO_CRAFT_EVENT_SIMULATE_POST) continue;
        // Tick once per squad, not once for each survivor and the yard.
        {
            std::lock_guard<std::mutex> lock(g_registry_mutex);
            const auto* squad = g_registry.find(id);
            if (!squad || squad->representative() != handle) continue;
            if (squad->live_count() == squad->slots.size()) {
                g_repair_visits.erase(found);
                continue;
            }
        }
        if (std::isfinite(seconds) && seconds > 0) visit.retry_seconds -= seconds;
        if (visit.retry_seconds > 0) continue;
        visit.retry_seconds = 1.0f;
        a2fo::squadrons::Result<a2fo::squadrons::Replacement> request;
        {
            std::lock_guard<std::mutex> lock(g_registry_mutex);
            request = g_registry.reserve_replacement(id, context);
        }
        if (!request) continue;
        void* target = find_class(request.value->odf);
        const auto queue_id = target ? g_push_repair_job(yard, target) : 0;
        if (!queue_id) {
            std::lock_guard<std::mutex> lock(g_registry_mutex);
            g_registry.cancel_replacement(request.value->ticket);
            continue; // Full queue, tech, capacity or resources: native admission owns these.
        }
        visit.ticket = request.value->ticket;
        visit.queue_id = queue_id;
        log_text("Squad replacement queued at normal cost/build time: " + request.value->odf);
    }
}

void finish_reinforcement(void* yard, std::uint32_t queue_id, void* result) {
    for (auto& entry : g_repair_visits) {
        auto& visit = entry.second;
        if (visit.yard != object_handle(yard) || !queue_id || visit.queue_id != queue_id) continue;
        a2fo::squadrons::RepairContext context;
        bool waiting = false;
        bool committed = false;
        if (result && observe_repair(entry.first, visit, &context, &waiting) && !waiting) {
            const auto odf = class_odf_name(object_class(result));
            std::lock_guard<std::mutex> lock(g_registry_mutex);
            committed = g_registry.complete_replacement(visit.ticket,
                {object_handle(result), static_cast<std::uint32_t>(object_team(result)), odf}, context);
        }
        if (!committed) {
            // A paid native result remains an ordinary ship if a last-moment
            // cancellation/capture invalidated membership. Never destroy it.
            end_repair_visit(entry.first, false);
            log_line("Squad replacement completed outside its repair cycle; retained as an ordinary ship");
            return;
        }
        visit.serviced.insert(object_handle(result));
        visit.ticket = 0;
        visit.queue_id = 0;
        visit.retry_seconds = 0;
        log_line("Paid replacement joined its squad and entered native yard output");
        return;
    }
}

bool install_repair_hooks() noexcept {
    const std::uint8_t needs_signature[]{0x55, 0x8b, 0xec, 0x83, 0xc4, 0xcc};
    const std::uint8_t activate_signature[]{0x55, 0x8b, 0xec, 0x51, 0x53};
    auto* fo = reinterpret_cast<std::uint8_t*>(g_fleet_ops);
    auto** slot = reinterpret_cast<void**>(at(kRepairQueueVtableRva + 0x2c));
    if (!g_api->install_inline_hook || !readable_range(slot, 4) ||
        *slot != at(kRepairActivateRva) ||
        !readable_range(at(kRepairActivateRva), sizeof(activate_signature)) ||
        std::memcmp(at(kRepairActivateRva), activate_signature, sizeof(activate_signature))) {
        log_line("Squad repair Activate vtable/signature unavailable");
        return false;
    }
    // FeaturePack owns ActDelete and has already detoured it in a normal mod
    // load. Never demand its original prologue or call its grouped UI handler.
    DWORD previous = 0;
    if (!VirtualProtect(slot, 4, PAGE_READWRITE, &previous)) return false;
    const bool installed = g_api->install_inline_hook(fo + kFoNeedsRepairRva,
        reinterpret_cast<void*>(&squad_needs_repair), sizeof(needs_signature),
        needs_signature, &g_needs_repair_hook);
    if (installed) {
        g_repair_activate_original = *slot;
        InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(slot),
            reinterpret_cast<void*>(&squad_repair_activate));
        g_repair_ready = true;
    } else {
        log_line("Squad NeedsRepair hook rejected: entry signature or hook installation failed");
    }
    DWORD ignored;
    VirtualProtect(slot, 4, previous, &ignored);
    return installed;
}
