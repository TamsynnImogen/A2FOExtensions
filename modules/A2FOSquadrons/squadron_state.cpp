#include "squadron_state.hpp"
#include "../../rust/domain_state_ffi.h"
#include <algorithm>
#include <exception>
#include <limits>
#include <vector>

namespace a2fo::squadrons {
namespace {
A2foRsBytes bytes(const std::string &value) noexcept {
    return {reinterpret_cast<const std::uint8_t *>(value.data()), value.size()};
}
A2foRsSqSlot rs(const Slot &slot) noexcept {
    return {slot.row, slot.ordinal, bytes(slot.odf), slot.member};
}
std::uint8_t slot_at(void *raw, std::size_t index, A2foRsSqSlot *output) noexcept {
    const auto &slots = *static_cast<const std::vector<Slot> *>(raw);
    if (index >= slots.size())
        return 0;
    *output = rs(slots[index]);
    return 1;
}
A2foRsSqRepair rs(const RepairContext &repair) noexcept {
    return {repair.yard,
            repair.team,
            static_cast<std::uint8_t>(repair.compatible),
            static_cast<std::uint8_t>(repair.accepted_repair_cycle),
            repair.serviced_members.data(),
            repair.serviced_members.size()};
}
A2foRsSqReplacement rs(const Replacement &value) noexcept {
    return {value.ticket, value.squad, value.yard,
            value.team,   value.slot,  bytes(value.odf)};
}
A2foRsSqView rs(const Squadron &squad) noexcept {
    return {squad.id,
            squad.team,
            squad.slots.size(),
            static_cast<std::uint8_t>(squad.definition.reinforce_at_yard),
            static_cast<std::uint8_t>(squad.replacement.has_value()),
            squad.replacement ? rs(*squad.replacement) : A2foRsSqReplacement{}};
}
std::vector<A2foRsSqSlot> rs_slots(const std::vector<Slot> &slots) {
    std::vector<A2foRsSqSlot> result;
    result.reserve(slots.size());
    for (const auto &slot : slots)
        result.push_back(rs(slot));
    return result;
}
std::vector<Slot> slots_for(const Definition &def) {
    std::vector<A2foRsSqRow> rows;
    rows.reserve(def.members.size());
    for (const auto &row : def.members)
        rows.push_back({row.row, row.count, bytes(row.odf)});
    std::vector<A2foRsSqSlot> expanded(def.size());
    const auto count =
        a2fo_rs_sq_expand(rows.data(), rows.size(), expanded.data(), expanded.size());
    std::vector<Slot> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto &slot = expanded[i];
        result.push_back(
            {slot.row, slot.ordinal,
             std::string(reinterpret_cast<const char *>(slot.odf.data), slot.odf.size),
             0});
    }
    return result;
}
using SquadMap = std::map<SquadId, Squadron>;
using MemberMap = std::map<ObjectId, SquadId>;
using LaunchSet = std::set<SquadId>;
struct Context {
    const SquadMap *squads;
    const MemberMap *membership;
    const LaunchSet *launching;
    Selection *selection = nullptr;
    std::exception_ptr error;
    Context(const SquadMap *s, const MemberMap *m, const LaunchSet *l,
            Selection *out = nullptr)
        : squads(s), membership(m), launching(l), selection(out) {}
};
std::uint8_t get_squad(void *raw, std::uint64_t id, A2foRsSqView *output) noexcept {
    auto &context = *static_cast<Context *>(raw);
    const auto it = context.squads->find(id);
    if (it == context.squads->end())
        return 0;
    *output = rs(it->second);
    return 1;
}
std::uint8_t next_squad(void *raw, std::uint64_t *cursor,
                        A2foRsSqView *output) noexcept {
    auto &context = *static_cast<Context *>(raw);
    const auto it = context.squads->upper_bound(*cursor);
    if (it == context.squads->end())
        return 0;
    *cursor = it->first;
    *output = rs(it->second);
    return 1;
}
std::uint8_t get_slot(void *raw, std::uint64_t id, std::size_t index,
                      A2foRsSqSlot *output) noexcept {
    auto &context = *static_cast<Context *>(raw);
    const auto it = context.squads->find(id);
    if (it == context.squads->end() || index >= it->second.slots.size())
        return 0;
    *output = rs(it->second.slots[index]);
    return 1;
}
std::uint64_t member_squad(void *raw, std::uint64_t member) noexcept {
    auto &context = *static_cast<Context *>(raw);
    const auto it = context.membership->find(member);
    return it == context.membership->end() ? 0 : it->second;
}
std::uint8_t launching(void *raw, std::uint64_t id) noexcept {
    return static_cast<Context *>(raw)->launching->count(id) != 0;
}
void emit_tile(void *raw, A2foRsSqTile tile) noexcept {
    auto &context = *static_cast<Context *>(raw);
    if (context.error)
        return;
    try {
        context.selection->tiles.push_back(
            {tile.squad, tile.representative, tile.live, tile.maximum});
    } catch (...) {
        context.error = std::current_exception();
    }
}
void emit_member(void *raw, std::uint64_t member) noexcept {
    auto &context = *static_cast<Context *>(raw);
    if (context.error)
        return;
    try {
        context.selection->members.push_back(member);
    } catch (...) {
        context.error = std::current_exception();
    }
}
A2foRsSqOps ops(Context &context) noexcept {
    return {&context,     get_squad, next_squad, get_slot,
            member_squad, launching, emit_tile,  emit_member};
}
void rethrow(const Context &context) {
    if (context.error)
        std::rethrow_exception(context.error);
}
std::vector<A2foRsSqCraft> crafts(const std::vector<Craft> &members,
                                  std::vector<std::string> &keys) {
    keys.reserve(members.size());
    for (const auto &craft : members)
        keys.push_back(odf_key(craft.odf));
    std::vector<A2foRsSqCraft> result;
    result.reserve(members.size());
    for (std::size_t i = 0; i < members.size(); ++i)
        result.push_back({members[i].id, members[i].team, bytes(keys[i])});
    return result;
}
A2foRsSqCraft craft(const Craft &value, std::string &key) {
    key = odf_key(value.odf);
    return {value.id, value.team, bytes(key)};
}
const char *create_error(int code) noexcept {
    switch (code) {
    case 2:
        return "initial complement must be complete";
    case 3:
        return "squad IDs exhausted";
    case 4:
        return "global squadron limit reached";
    case 5:
        return "team squadron limit reached";
    case 6:
        return "member identity, owner or ODF does not match its configured slot";
    case 7:
        return "a Craft cannot occupy multiple squadron slots";
    case 8:
        return "first launching member does not match slot zero";
    default:
        return "invalid squadron admission";
    }
}
const char *reserve_error(int code) noexcept {
    switch (code) {
    case 9:
        return "squadron no longer exists";
    case 10:
        return "initial squad launch is still in progress";
    case 11:
        return "a replacement is already reserved";
    case 12:
        return "all survivors must be in the accepted yard repair cycle";
    case 13:
        return "a squad member cannot serve as its repair yard";
    case 14:
        return "replacement IDs exhausted";
    case 15:
        return "squadron is already at full strength";
    default:
        return "invalid replacement reservation";
    }
}
struct SavedContext {
    const Snapshot *saved;
    const ClassResolver *resolver;
    std::string problem;
    std::exception_ptr exception;
};
std::uint8_t saved_squad(void *raw, std::size_t index, A2foRsSqView *output) noexcept {
    auto &context = *static_cast<SavedContext *>(raw);
    if (index >= context.saved->squads.size())
        return 0;
    *output = rs(context.saved->squads[index]);
    return 1;
}
std::uint8_t saved_row(void *raw, std::size_t squad, std::size_t index,
                       A2foRsSqRow *output) noexcept {
    auto &context = *static_cast<SavedContext *>(raw);
    const auto &rows = context.saved->squads[squad].definition.members;
    if (index >= rows.size())
        return 0;
    const auto &row = rows[index];
    *output = {row.row, row.count, bytes(row.odf)};
    return 1;
}
std::uint8_t saved_slot(void *raw, std::size_t squad, std::size_t index,
                        A2foRsSqSlot *output) noexcept {
    auto &context = *static_cast<SavedContext *>(raw);
    const auto &slots = context.saved->squads[squad].slots;
    if (index >= slots.size())
        return 0;
    *output = rs(slots[index]);
    return 1;
}
std::uint8_t saved_validate(void *raw, std::size_t index) noexcept {
    auto &context = *static_cast<SavedContext *>(raw);
    if (context.exception)
        return 0;
    try {
        context.problem = validate_definition(context.saved->squads[index].definition,
                                              *context.resolver);
        return context.problem.empty();
    } catch (...) {
        context.exception = std::current_exception();
        return 0;
    }
}
} // namespace

std::uint32_t Squadron::live_count() const noexcept {
    return a2fo_rs_sq_live_at(const_cast<std::vector<Slot> *>(&slots), slot_at,
                              slots.size());
}
ObjectId Squadron::representative() const noexcept {
    return a2fo_rs_sq_representative_at(const_cast<std::vector<Slot> *>(&slots),
                                        slot_at, slots.size());
}
std::string Squadron::badge() const {
    char buffer[64]{};
    const auto size =
        a2fo_rs_sq_badge(live_count(), slots.size(),
                         reinterpret_cast<std::uint8_t *>(buffer), sizeof(buffer));
    return {buffer, size};
}
const Squadron *Registry::find(SquadId id) const noexcept {
    const auto found = squads_.find(id);
    return found == squads_.end() ? nullptr : &found->second;
}
const Squadron *Registry::containing(ObjectId member) const noexcept {
    const auto found = membership_.find(member);
    return found == membership_.end() ? nullptr : find(found->second);
}
bool Registry::reserved_yard(ObjectId id) const noexcept {
    Context context{&squads_, &membership_, &launching_};
    return a2fo_rs_sq_reserved_yard(ops(context), id) != 0;
}
Result<SquadId> Registry::create(const Definition &def, TeamId team,
                                 const std::vector<Craft> &members,
                                 const ClassResolver &resolve) {
    if (const auto error = validate_definition(def, resolve); !error.empty())
        return {{}, error};
    const auto slots = slots_for(def);
    const auto records = rs_slots(slots);
    std::vector<std::string> keys;
    const auto members_rs = crafts(members, keys);
    Context context{&squads_, &membership_, &launching_};
    const auto plan = a2fo_rs_sq_create(ops(context), next_squad_, squads_.size(), team,
                                        records.data(), records.size(),
                                        members_rs.data(), members_rs.size(), 0);
    if (plan.code != 0)
        return {{}, create_error(plan.code)};
    Squadron squad{next_squad_, team, def, slots, {}};
    std::map<ObjectId, SquadId> new_membership;
    for (std::size_t i = 0; i < members.size(); ++i) {
        new_membership.emplace(members[i].id, squad.id);
        squad.slots[i].member = members[i].id;
    }
    squads_.emplace(squad.id, std::move(squad));
    membership_.merge(new_membership);
    return {next_squad_++, {}};
}
Result<SquadId> Registry::create_launching(const Definition &def, TeamId team,
                                           const Craft &first,
                                           const ClassResolver &resolve) {
    if (const auto error = validate_definition(def, resolve); !error.empty())
        return {{}, error};
    const auto slots = slots_for(def);
    const auto records = rs_slots(slots);
    std::string key;
    const auto first_rs = craft(first, key);
    Context context{&squads_, &membership_, &launching_};
    const auto plan =
        a2fo_rs_sq_create(ops(context), next_squad_, squads_.size(), team,
                          records.data(), records.size(), &first_rs, 1, 1);
    if (plan.code != 0)
        return {{}, create_error(plan.code)};
    Squadron squad{next_squad_, team, def, slots, {}};
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
bool Registry::add_launching(SquadId id, std::size_t index, const Craft &value) {
    std::string key;
    const auto record = craft(value, key);
    Context context{&squads_, &membership_, &launching_};
    const auto plan = a2fo_rs_sq_add_launching(ops(context), id, index, record);
    if (plan.code != 0)
        return false;
    membership_.emplace(value.id, id);
    squads_.find(id)->second.slots[index].member = value.id;
    return true;
}
bool Registry::finish_launching(SquadId id) {
    Context context{&squads_, &membership_, &launching_};
    const auto plan = a2fo_rs_sq_finish_launching(ops(context), id);
    if (plan.code != 0)
        return false;
    launching_.erase(id);
    if (plan.retire)
        squads_.erase(id);
    return true;
}
Removal Registry::remove(ObjectId member) {
    Context context{&squads_, &membership_, &launching_};
    const auto plan = a2fo_rs_sq_remove(ops(context), member, 0, 0);
    if (plan.code != 0)
        return {};
    const auto member_it = membership_.find(member);
    const auto squad_it = squads_.find(plan.id);
    auto &squad = squad_it->second;
    for (auto &slot : squad.slots)
        if (slot.member == member)
            slot.member = 0;
    membership_.erase(member_it);
    Removal result{true, plan.retire != 0, {}};
    if (result.squad_retired) {
        result.cancelled = std::move(squad.replacement);
        squads_.erase(squad_it);
    }
    return result;
}
Removal Registry::change_owner(ObjectId member, TeamId new_owner) {
    Context context{&squads_, &membership_, &launching_};
    const auto plan = a2fo_rs_sq_remove(ops(context), member, new_owner, 1);
    return plan.code == 0 ? remove(member) : Removal{};
}
Result<Replacement> Registry::reserve_replacement(SquadId id,
                                                  const RepairContext &repair) {
    Context context{&squads_, &membership_, &launching_};
    const auto plan = a2fo_rs_sq_reserve(ops(context), id, next_ticket_, rs(repair));
    if (plan.code != 0)
        return {{}, reserve_error(plan.code)};
    auto &squad = squads_.find(id)->second;
    Replacement request{next_ticket_, id,        repair.yard,
                        squad.team,   plan.slot, squad.slots[plan.slot].odf};
    Result<Replacement> result{request, {}};
    squad.replacement = std::move(request);
    ++next_ticket_;
    return result;
}
bool Registry::complete_replacement(TicketId ticket, const Craft &value,
                                    const RepairContext &repair) {
    std::string key;
    const auto record = craft(value, key);
    Context context{&squads_, &membership_, &launching_};
    const auto plan = a2fo_rs_sq_complete(ops(context), ticket, record, rs(repair));
    if (plan.code != 0)
        return false;
    auto &squad = squads_.find(plan.id)->second;
    membership_.emplace(value.id, squad.id);
    squad.slots[plan.slot].member = value.id;
    squad.replacement.reset();
    return true;
}
std::optional<Replacement> Registry::cancel_replacement(TicketId ticket) {
    Context context{&squads_, &membership_, &launching_};
    const auto plan = a2fo_rs_sq_cancel(ops(context), ticket);
    if (plan.code != 0)
        return {};
    auto &pending = squads_.find(plan.id)->second.replacement;
    auto result = std::move(pending);
    pending.reset();
    return result;
}
std::vector<Replacement> Registry::cancel_yard(ObjectId yard) {
    Context context{&squads_, &membership_, &launching_};
    std::vector<TicketId> tickets(squads_.size());
    const auto count =
        a2fo_rs_sq_cancel_yard(ops(context), yard, tickets.data(), tickets.size());
    std::vector<Replacement> result;
    result.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        const auto plan = a2fo_rs_sq_cancel(ops(context), tickets[i]);
        result.push_back(*squads_.find(plan.id)->second.replacement);
    }
    for (std::size_t i = 0; i < count; ++i)
        cancel_replacement(tickets[i]);
    return result;
}
Result<Selection> Registry::select(const std::vector<Craft> &selected, TeamId team,
                                   std::size_t physical_limit) const {
    std::vector<A2foRsSqCraft> records;
    records.reserve(selected.size());
    for (const auto &value : selected)
        records.push_back({value.id, value.team, {}});
    Selection result;
    Context context{&squads_, &membership_, &launching_, &result};
    const auto code = a2fo_rs_sq_select(ops(context), records.data(), records.size(),
                                        team, physical_limit);
    rethrow(context);
    if (code == 16)
        return {{}, "stale ownership in squad selection"};
    if (code == 17)
        return {{}, "expanded selection exceeds verified native command capacity"};
    return {std::move(result), {}};
}
Snapshot Registry::snapshot() const {
    Snapshot result{1, next_squad_, next_ticket_, {}};
    std::vector<SquadId> ids(squads_.size());
    Context context{&squads_, &membership_, &launching_};
    const auto count = a2fo_rs_sq_snapshot_ids(ops(context), ids.data(), ids.size());
    result.squads.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        result.squads.push_back(squads_.find(ids[i])->second);
    return result;
}
bool Registry::restore(const Snapshot &saved, const ClassResolver &resolve,
                       std::string &error) {
    SavedContext context{&saved, &resolve, {}, {}};
    A2foRsSqSavedOps callbacks{&context, saved_squad, saved_row, saved_slot,
                               saved_validate};
    const auto code =
        a2fo_rs_sq_restore_check(callbacks, saved.version, saved.next_squad,
                                 saved.next_ticket, saved.squads.size());
    if (context.exception)
        std::rethrow_exception(context.exception);
    if (code != 0) {
        switch (code) {
        case 18:
            error = "unsupported or invalid squadron snapshot header";
            break;
        case 19:
            error = "duplicate or invalid saved squad identity";
            break;
        case 20:
            error = "saved team exceeds squadron limit";
            break;
        case 21:
            error = context.problem;
            break;
        case 22:
            error = "saved squad has invalid slots or no survivors";
            break;
        case 23:
            error = "saved slot differs from configured composition";
            break;
        case 24:
            error = "saved Craft occupies multiple slots";
            break;
        case 25:
            error = "invalid saved replacement reservation";
            break;
        case 26:
            error = "saved repair yard is a squad member";
            break;
        default:
            error = "invalid squadron snapshot";
            break;
        }
        return false;
    }
    SquadMap candidate_squads;
    MemberMap candidate_members;
    for (const auto &squad : saved.squads) {
        for (const auto &slot : squad.slots)
            if (slot.member)
                candidate_members.emplace(slot.member, squad.id);
        candidate_squads.emplace(squad.id, squad);
    }
    squads_.swap(candidate_squads);
    membership_.swap(candidate_members);
    launching_.clear();
    next_squad_ = saved.next_squad;
    next_ticket_ = saved.next_ticket;
    error.clear();
    return true;
}
} // namespace a2fo::squadrons
