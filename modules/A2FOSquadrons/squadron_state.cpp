#include "squadron_state.hpp"

#include <algorithm>
#include <limits>
#include <set>

namespace a2fo::squadrons {
namespace {
std::vector<Slot> slots_for(const Definition& def) {
    std::vector<Slot> slots;
    slots.reserve(def.size());
    for (const auto& row : def.members)
        for (std::uint32_t ordinal = 0; ordinal < row.count; ++ordinal)
            slots.push_back({row.row, ordinal, row.odf, 0});
    return slots;
}

bool repair_ready(const Squadron& squad, const RepairContext& repair) {
    if (!squad.definition.reinforce_at_yard || !repair.yard ||
        repair.team != squad.team || !repair.compatible ||
        !repair.accepted_repair_cycle || squad.live_count() == 0) return false;
    std::set<ObjectId> expected;
    for (const auto& slot : squad.slots) if (slot.member) expected.insert(slot.member);
    const std::set<ObjectId> present(repair.serviced_members.begin(),
                                     repair.serviced_members.end());
    return present.size() == repair.serviced_members.size() && present == expected;
}
} // namespace

std::uint32_t Squadron::live_count() const noexcept {
    return static_cast<std::uint32_t>(std::count_if(slots.begin(), slots.end(),
                                       [](const Slot& slot) { return slot.member != 0; }));
}

ObjectId Squadron::representative() const noexcept {
    for (const auto& slot : slots) if (slot.member) return slot.member;
    return 0;
}

std::string Squadron::badge() const {
    return std::to_string(live_count()) + "/" + std::to_string(slots.size());
}

const Squadron* Registry::find(SquadId id) const noexcept {
    const auto found = squads_.find(id);
    return found == squads_.end() ? nullptr : &found->second;
}

const Squadron* Registry::containing(ObjectId member) const noexcept {
    const auto found = membership_.find(member);
    return found == membership_.end() ? nullptr : find(found->second);
}

bool Registry::reserved_yard(ObjectId id) const noexcept {
    return std::any_of(squads_.begin(), squads_.end(), [id](const auto& item) {
        return item.second.replacement && item.second.replacement->yard == id;
    });
}

Result<SquadId> Registry::create(const Definition& def, TeamId team,
                                  const std::vector<Craft>& members,
                                  const ClassResolver& resolve) {
    if (const auto error = validate_definition(def, resolve); !error.empty())
        return {{}, error};
    if (members.size() != def.size()) return {{}, "initial complement must be complete"};
    if (next_squad_ == std::numeric_limits<SquadId>::max()) return {{}, "squad IDs exhausted"};
    if (squads_.size() >= kMaxSquads) return {{}, "global squadron limit reached"};
    const auto team_count = std::count_if(squads_.begin(), squads_.end(),
                            [team](const auto& item) { return item.second.team == team; });
    if (static_cast<std::size_t>(team_count) >= kMaxSquadsPerTeam)
        return {{}, "team squadron limit reached"};

    Squadron squad{next_squad_, team, def, slots_for(def), {}};
    std::map<ObjectId, SquadId> new_membership;
    for (std::size_t i = 0; i < members.size(); ++i) {
        const auto& craft = members[i];
        if (!craft.id || reserved_yard(craft.id) || craft.team != team || odf_key(craft.odf) != squad.slots[i].odf)
            return {{}, "member identity, owner or ODF does not match its configured slot"};
        if (membership_.count(craft.id) || !new_membership.emplace(craft.id, squad.id).second)
            return {{}, "a Craft cannot occupy multiple squadron slots"};
        squad.slots[i].member = craft.id;
    }
    // Allocate everything before publishing either index. map::merge moves
    // already allocated nodes, so an allocation failure leaves no half squad.
    squads_.emplace(squad.id, std::move(squad));
    membership_.merge(new_membership);
    return {next_squad_++, {}};
}

Result<SquadId> Registry::create_launching(
    const Definition& def, TeamId team, const Craft& first,
    const ClassResolver& resolve) {
    if (const auto error = validate_definition(def, resolve); !error.empty())
        return {{}, error};
    if (next_squad_ == std::numeric_limits<SquadId>::max())
        return {{}, "squad IDs exhausted"};
    if (squads_.size() >= kMaxSquads)
        return {{}, "global squadron limit reached"};
    const auto team_count = std::count_if(squads_.begin(), squads_.end(),
        [team](const auto& item) { return item.second.team == team; });
    if (static_cast<std::size_t>(team_count) >= kMaxSquadsPerTeam)
        return {{}, "team squadron limit reached"};
    Squadron squad{next_squad_, team, def, slots_for(def), {}};
    if (!first.id || first.team != team ||
        odf_key(first.odf) != squad.slots[0].odf ||
        membership_.count(first.id) || reserved_yard(first.id))
        return {{}, "first launching member does not match slot zero"};
    squad.slots[0].member = first.id;
    std::map<ObjectId, SquadId> new_membership;
    std::set<SquadId> new_launching;
    new_membership.emplace(first.id, squad.id);
    new_launching.insert(squad.id);
    squads_.emplace(squad.id, std::move(squad));
    membership_.merge(new_membership);
    launching_.merge(new_launching);
    return {next_squad_++, {}};
}

bool Registry::add_launching(SquadId id, std::size_t index,
                            const Craft& craft) {
    const auto found = squads_.find(id);
    if (found == squads_.end() || !launching_.count(id) ||
        index >= found->second.slots.size() ||
        !craft.id || craft.team != found->second.team ||
        membership_.count(craft.id) || reserved_yard(craft.id)) return false;
    auto& slot = found->second.slots[index];
    if (slot.member || odf_key(craft.odf) != slot.odf) return false;
    membership_.emplace(craft.id, id);
    slot.member = craft.id;
    return true;
}

bool Registry::finish_launching(SquadId id) {
    if (!launching_.erase(id)) return false;
    const auto found = squads_.find(id);
    if (found != squads_.end() && found->second.live_count() == 0)
        squads_.erase(found);
    return true;
}

Removal Registry::remove(ObjectId member) {
    const auto member_it = membership_.find(member);
    if (member_it == membership_.end()) return {};
    const auto squad_it = squads_.find(member_it->second);
    auto& squad = squad_it->second;
    for (auto& slot : squad.slots) if (slot.member == member) slot.member = 0;
    membership_.erase(member_it);
    Removal result{true,
        squad.live_count() == 0 && !launching_.count(squad.id), {}};
    if (result.squad_retired) {
        result.cancelled = std::move(squad.replacement);
        squads_.erase(squad_it);
    }
    return result;
}

Removal Registry::change_owner(ObjectId member, TeamId new_owner) {
    const auto* squad = containing(member);
    if (!squad || squad->team == new_owner) return {};
    return remove(member);
}

Result<Replacement> Registry::reserve_replacement(SquadId id, const RepairContext& repair) {
    const auto found = squads_.find(id);
    if (found == squads_.end()) return {{}, "squadron no longer exists"};
    auto& squad = found->second;
    if (launching_.count(id)) return {{}, "initial squad launch is still in progress"};
    if (squad.replacement) return {{}, "a replacement is already reserved"};
    if (!repair_ready(squad, repair)) return {{}, "all survivors must be in the accepted yard repair cycle"};
    if (membership_.count(repair.yard)) return {{}, "a squad member cannot serve as its repair yard"};
    if (next_ticket_ == std::numeric_limits<TicketId>::max()) return {{}, "replacement IDs exhausted"};
    for (std::uint32_t i = 0; i < squad.slots.size(); ++i) {
        if (squad.slots[i].member) continue;
        Replacement request{next_ticket_, id, repair.yard, squad.team, i, squad.slots[i].odf};
        // Copy the return value before committing the reservation.
        Result<Replacement> result{request, {}};
        squad.replacement = std::move(request);
        ++next_ticket_;
        return result;
    }
    return {{}, "squadron is already at full strength"};
}

bool Registry::complete_replacement(TicketId ticket, const Craft& craft,
                                      const RepairContext& repair) {
    if (!ticket || !craft.id || membership_.count(craft.id) || reserved_yard(craft.id)) return false;
    for (auto& item : squads_) {
        auto& squad = item.second;
        if (!squad.replacement || squad.replacement->ticket != ticket) continue;
        const auto& request = *squad.replacement;
        if (repair.yard != request.yard || !repair_ready(squad, repair) ||
            craft.team != request.team || odf_key(craft.odf) != request.odf ||
            craft.id == repair.yard || squad.slots[request.slot].member) return false;
        membership_.emplace(craft.id, squad.id);
        squad.slots[request.slot].member = craft.id;
        squad.replacement.reset();
        return true;
    }
    return false;
}

std::optional<Replacement> Registry::cancel_replacement(TicketId ticket) {
    for (auto& item : squads_) {
        auto& pending = item.second.replacement;
        if (!pending || pending->ticket != ticket) continue;
        auto result = std::move(pending);
        pending.reset();
        return result;
    }
    return {};
}

std::vector<Replacement> Registry::cancel_yard(ObjectId yard) {
    std::vector<Replacement> result;
    // Allocate/copy before mutation so callers cannot lose a refund record.
    for (const auto& item : squads_)
        if (item.second.replacement && item.second.replacement->yard == yard)
            result.push_back(*item.second.replacement);
    for (const auto& request : result) cancel_replacement(request.ticket);
    return result;
}

Result<Selection> Registry::select(const std::vector<Craft>& selected, TeamId team,
                                    std::size_t physical_limit) const {
    std::set<SquadId> groups;
    std::set<ObjectId> ordinary;
    for (const auto& craft : selected) {
        if (!craft.id || craft.team != team) continue;
        if (const auto* squad = containing(craft.id)) {
            if (squad->team != team) return {{}, "stale ownership in squad selection"};
            groups.insert(squad->id);
        } else ordinary.insert(craft.id);
    }
    Selection result;
    for (const auto id : groups) {
        const auto& squad = *find(id);
        if (squad.live_count() > physical_limit - result.members.size())
            return {{}, "expanded selection exceeds verified native command capacity"};
        result.tiles.push_back({id, squad.representative(), squad.live_count(),
                               static_cast<std::uint32_t>(squad.slots.size())});
        for (const auto& slot : squad.slots) if (slot.member) result.members.push_back(slot.member);
    }
    if (ordinary.size() > physical_limit - result.members.size())
        return {{}, "expanded selection exceeds verified native command capacity"};
    for (const auto id : ordinary) {
        result.tiles.push_back({0, id, 1, 1});
        result.members.push_back(id);
    }
    return {std::move(result), {}};
}

Snapshot Registry::snapshot() const {
    Snapshot result{1, next_squad_, next_ticket_, {}};
    result.squads.reserve(squads_.size());
    for (const auto& item : squads_) result.squads.push_back(item.second);
    return result;
}

bool Registry::restore(const Snapshot& saved, const ClassResolver& resolve, std::string& error) {
    const auto fail = [&](const std::string& reason) { error = reason; return false; };
    if (saved.version != 1 || !saved.next_squad || !saved.next_ticket || saved.squads.size() > kMaxSquads)
        return fail("unsupported or invalid squadron snapshot header");
    Registry candidate;
    std::map<TeamId, std::uint32_t> team_counts;
    std::set<TicketId> tickets;
    for (const auto& squad : saved.squads) {
        if (!squad.id || squad.id >= saved.next_squad || candidate.squads_.count(squad.id))
            return fail("duplicate or invalid saved squad identity");
        if (++team_counts[squad.team] > kMaxSquadsPerTeam) return fail("saved team exceeds squadron limit");
        if (const auto problem = validate_definition(squad.definition, resolve); !problem.empty()) return fail(problem);
        const auto configured = slots_for(squad.definition);
        if (squad.slots.size() != configured.size() || squad.live_count() == 0)
            return fail("saved squad has invalid slots or no survivors");
        for (std::size_t i = 0; i < configured.size(); ++i) {
            const auto& slot = squad.slots[i];
            if (slot.row != configured[i].row || slot.ordinal != configured[i].ordinal || slot.odf != configured[i].odf)
                return fail("saved slot differs from configured composition");
            if (slot.member && !candidate.membership_.emplace(slot.member, squad.id).second)
                return fail("saved Craft occupies multiple slots");
        }
        if (squad.replacement) {
            const auto& request = *squad.replacement;
            if (!squad.definition.reinforce_at_yard || !request.ticket || request.ticket >= saved.next_ticket ||
                !tickets.insert(request.ticket).second || request.squad != squad.id ||
                request.team != squad.team || !request.yard || request.slot >= squad.slots.size() ||
                squad.slots[request.slot].member || request.odf != squad.slots[request.slot].odf)
                return fail("invalid saved replacement reservation");
        }
        candidate.squads_.emplace(squad.id, squad);
    }
    for (const auto& item : candidate.squads_)
        if (item.second.replacement && candidate.membership_.count(item.second.replacement->yard))
            return fail("saved repair yard is a squad member");
    squads_.swap(candidate.squads_);
    membership_.swap(candidate.membership_);
    launching_.clear();
    next_squad_ = saved.next_squad;
    next_ticket_ = saved.next_ticket;
    error.clear();
    return true;
}

} // namespace a2fo::squadrons
