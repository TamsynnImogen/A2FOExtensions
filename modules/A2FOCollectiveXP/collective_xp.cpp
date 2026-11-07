#include "collective_xp.hpp"
#include "../../rust/domain_state_ffi.h"
#include "../../rust/wide_math_ffi.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>

namespace a2fo::collective_xp {
namespace {
static_assert(static_cast<int>(Status::invalid_snapshot) == 9);
static_assert(std::numeric_limits<double>::is_iec559 && sizeof(double) == 8,
              "Collective XP snapshots require IEEE-754 binary64 doubles");
A2foRsBytes bytes(const std::string &value) noexcept {
    return {reinterpret_cast<const std::uint8_t *>(value.data()), value.size()};
}
std::string string(A2foRsBytes value) {
    return {reinterpret_cast<const char *>(value.data ? value.data
                                                      : reinterpret_cast<const std::uint8_t *>("")),
            value.size};
}
A2foRsPlayer rs(const PlayerState &value) noexcept {
    return {static_cast<std::uint8_t>(value.enabled), value.level, value.pool, value.multiplier};
}
PlayerState cpp(A2foRsPlayer value) noexcept {
    return {value.enabled != 0, value.level, value.pool, value.multiplier};
}
A2foRsUnit rs(const Unit &value) noexcept {
    return {value.id,
            value.player,
            bytes(value.base_class),
            bytes(value.current_class),
            static_cast<std::uint8_t>(value.native_capable),
            static_cast<std::uint8_t>(value.completed),
            static_cast<std::uint8_t>(value.live)};
}
Resolution cpp(A2foRsResolution value) {
    Resolution out;
    out.status = static_cast<Status>(value.status);
    out.class_id = string(value.class_id);
    out.level = value.level;
    out.has_next = value.has_next != 0;
    out.next_xp = value.next_xp;
    return out;
}
struct CatalogContext {
    const std::map<std::string, ClassRank> *ranks;
    std::exception_ptr error;
};
std::uint8_t rank_lookup(void *raw, A2foRsBytes id, A2foRsRank *output) noexcept {
    auto &context = *static_cast<CatalogContext *>(raw);
    if (context.error)
        return 0;
    try {
        const auto it = context.ranks->find(string(id));
        if (it == context.ranks->end())
            return 0;
        const auto &rank = it->second;
        *output = {bytes(rank.id), rank.level, rank.next_xp, bytes(rank.next_class)};
        return 1;
    } catch (...) {
        context.error = std::current_exception();
        return 0;
    }
}
using PlayerMap = std::map<PlayerId, PlayerState>;
using UnitMap = std::map<UnitId, Unit>;
using CacheMap = std::map<PlayerId, std::pair<std::uint32_t, double>>;
struct XpContext {
    const PlayerMap *players;
    UnitMap *units;
    CacheMap *thresholds;
    const Catalog *catalog;
    std::exception_ptr error;
    std::string resolved_id;
    alignas(16) std::uint8_t weighted_total[16]{};
    XpContext(const PlayerMap *p, UnitMap *u, CacheMap *t, const Catalog *c)
        : players(p), units(u), thresholds(t), catalog(c) {}
};
std::uint8_t get_player(void *raw, std::uint32_t id, A2foRsPlayer *output) noexcept {
    auto &context = *static_cast<XpContext *>(raw);
    const auto it = context.players->find(id);
    if (it == context.players->end())
        return 0;
    *output = rs(it->second);
    return 1;
}
std::uint8_t get_unit(void *raw, std::uint64_t id, A2foRsUnit *output) noexcept {
    auto &context = *static_cast<XpContext *>(raw);
    const auto it = context.units->find(id);
    if (it == context.units->end())
        return 0;
    *output = rs(it->second);
    return 1;
}
std::uint8_t next_unit(void *raw, std::uint64_t *cursor, A2foRsUnit *output) noexcept {
    auto &context = *static_cast<XpContext *>(raw);
    const auto it = context.units->upper_bound(*cursor);
    if (it == context.units->end())
        return 0;
    *cursor = it->first;
    *output = rs(it->second);
    return 1;
}
void erase_unit(void *raw, std::uint64_t id) noexcept {
    static_cast<XpContext *>(raw)->units->erase(id);
}
void erase_player(void *raw, std::uint32_t id) noexcept {
    auto &context = *static_cast<XpContext *>(raw);
    const_cast<PlayerMap *>(context.players)->erase(id);
}
void erase_cache(void *raw, std::uint32_t id) noexcept {
    static_cast<XpContext *>(raw)->thresholds->erase(id);
}
std::uint8_t cache_get(void *raw, std::uint32_t id, std::uint32_t level, double *output) noexcept {
    auto &context = *static_cast<XpContext *>(raw);
    const auto it = context.thresholds->find(id);
    if (it == context.thresholds->end() || it->second.first != level)
        return 0;
    *output = it->second.second;
    return 1;
}
void cache_set(void *raw, std::uint32_t id, std::uint32_t level, double value) noexcept {
    auto &context = *static_cast<XpContext *>(raw);
    if (context.error)
        return;
    try {
        (*context.thresholds)[id] = {level, value};
    } catch (...) {
        context.error = std::current_exception();
    }
}
A2foRsResolution resolve(void *raw, A2foRsBytes base, std::uint32_t level,
                         A2foRsBytes current) noexcept {
    auto &context = *static_cast<XpContext *>(raw);
    if (context.error)
        return {static_cast<int>(Status::missing_class), {}, 0, 0, 0};
    try {
        const auto value = context.catalog->resolve(string(base), level, string(current));
        context.resolved_id = value.class_id;
        return {static_cast<int>(value.status), bytes(context.resolved_id), value.level,
                static_cast<std::uint8_t>(value.has_next), value.next_xp};
    } catch (...) {
        context.error = std::current_exception();
        return {static_cast<int>(Status::missing_class), {}, 0, 0, 0};
    }
}
std::uint32_t native_level(void *raw, A2foRsBytes id) noexcept {
    auto &context = *static_cast<XpContext *>(raw);
    if (context.error)
        return 0;
    try {
        return context.catalog->native_level(string(id));
    } catch (...) {
        context.error = std::current_exception();
        return 0;
    }
}
void weighted_reset(void *raw) noexcept {
    a2fo_rs_xp_wide_reset(static_cast<XpContext *>(raw)->weighted_total);
}
std::uint8_t weighted_add(void *raw, double value, double multiplier) noexcept {
    return a2fo_rs_xp_wide_add(static_cast<XpContext *>(raw)->weighted_total, value, multiplier);
}
std::uint8_t weighted_result(void *raw, double *out) noexcept {
    return a2fo_rs_xp_wide_result(static_cast<XpContext *>(raw)->weighted_total, out);
}
A2foRsXpOps ops(XpContext &context) noexcept {
    return {&context,     get_player,     get_unit,     next_unit,      erase_unit,
            erase_player, erase_cache,    cache_get,    cache_set,      resolve,
            native_level, weighted_reset, weighted_add, weighted_result};
}
void rethrow(const XpContext &context) {
    if (context.error)
        std::rethrow_exception(context.error);
}
} // namespace

bool Catalog::add(const ClassRank &rank) {
    const A2foRsRank record{bytes(rank.id), rank.level, rank.next_xp, bytes(rank.next_class)};
    return a2fo_rs_xp_rank_valid(record) != 0 && ranks_.emplace(rank.id, rank).second;
}
Resolution Catalog::resolve(const std::string &base, std::uint32_t level,
                            const std::string &current) const {
    CatalogContext context{&ranks_, {}};
    const auto result =
        a2fo_rs_xp_resolve({&context, rank_lookup}, bytes(base), level, bytes(current));
    if (context.error)
        std::rethrow_exception(context.error);
    return cpp(result);
}
std::uint32_t Catalog::native_level(const std::string &id) const {
    const auto it = ranks_.find(id);
    return it == ranks_.end() ? 0 : it->second.level;
}
bool Controller::configure(PlayerId player, bool enabled, double multiplier) {
    if (!a2fo_rs_xp_configure_valid(multiplier))
        return false;
    auto &state = players_[player];
    state.enabled = enabled;
    state.multiplier = multiplier;
    thresholds_.erase(player);
    return true;
}
void Controller::reset_player(PlayerId player) {
    XpContext context{&players_, &units_, &thresholds_, &catalog_};
    a2fo_rs_xp_reset(ops(context), player);
    rethrow(context);
}
Status Controller::upsert(const Unit &unit) {
    XpContext context{&players_, &units_, &thresholds_, &catalog_};
    const auto status = static_cast<Status>(a2fo_rs_xp_upsert_valid(ops(context), rs(unit)));
    rethrow(context);
    if (status != Status::ok)
        return status;
    const auto old = units_.find(unit.id);
    if (old != units_.end())
        thresholds_.erase(old->second.player);
    units_[unit.id] = unit;
    thresholds_.erase(unit.player);
    return Status::ok;
}
bool Controller::remove(UnitId id) {
    XpContext context{&players_, &units_, &thresholds_, &catalog_};
    const bool allowed = a2fo_rs_xp_remove_allowed(ops(context), id) != 0;
    if (!allowed)
        return false;
    const auto it = units_.find(id);
    thresholds_.erase(it->second.player);
    units_.erase(it);
    return true;
}
Status Controller::transfer(UnitId id, PlayerId new_player) {
    XpContext context{&players_, &units_, &thresholds_, &catalog_};
    const auto status = static_cast<Status>(a2fo_rs_xp_transfer_allowed(ops(context), id));
    if (status != Status::ok)
        return status;
    auto &unit = units_.find(id)->second;
    thresholds_.erase(unit.player);
    thresholds_.erase(new_player);
    unit.player = new_player;
    return Status::ok;
}
Resolution Controller::target(UnitId id) const {
    XpContext context{&players_, const_cast<UnitMap *>(&units_), &thresholds_, &catalog_};
    const auto result = a2fo_rs_xp_target(ops(context), id);
    rethrow(context);
    return cpp(result);
}
const PlayerState *Controller::state(PlayerId player) const {
    const auto it = players_.find(player);
    return it == players_.end() ? nullptr : &it->second;
}
double Controller::threshold_for(PlayerId player, std::uint32_t level, Status &status) const {
    XpContext context{&players_, const_cast<UnitMap *>(&units_), &thresholds_, &catalog_};
    std::int32_t raw = 0;
    const double result = a2fo_rs_xp_threshold(ops(context), player, level, &raw);
    rethrow(context);
    status = static_cast<Status>(raw);
    return result;
}
double Controller::cached_threshold(PlayerId player, std::uint32_t level, Status &status) const {
    return threshold_for(player, level, status);
}
double Controller::threshold(PlayerId player, Status *status) const {
    const auto *owner = state(player);
    Status local;
    const auto value = owner ? cached_threshold(player, owner->level, local) : 0;
    if (!owner)
        local = Status::unknown_player;
    if (status)
        *status = local;
    return value;
}
Progress Controller::process(PlayerId player, double xp, bool award_requested) {
    XpContext context{&players_, &units_, &thresholds_, &catalog_};
    const auto result = a2fo_rs_xp_process(ops(context), player, xp, award_requested);
    rethrow(context);
    Progress out;
    out.status = static_cast<Status>(result.status);
    out.consumed = result.consumed != 0;
    out.levels_gained = result.levels_gained;
    out.state = cpp(result.state);
    out.threshold = result.threshold;
    if (result.commit)
        players_.find(player)->second = out.state;
    return out;
}
Progress Controller::award(PlayerId player, double xp) { return process(player, xp, true); }
Progress Controller::reconcile(PlayerId player) { return process(player, 0, false); }
std::vector<std::uint8_t> Controller::snapshot() const {
    std::vector<A2foRsPlayerRecord> records;
    records.reserve(players_.size());
    for (const auto &[id, state] : players_)
        records.push_back({id, rs(state)});
    std::vector<std::uint8_t> out(8 + 25 * records.size());
    const auto size = a2fo_rs_xp_snapshot(records.data(), records.size(), out.data(), out.size());
    out.resize(size);
    return out;
}
std::vector<std::uint8_t> Controller::snapshot_player(PlayerId player) const {
    std::vector<A2foRsPlayerRecord> records;
    const auto it = players_.find(player);
    if (it != players_.end())
        records.push_back({it->first, rs(it->second)});
    std::vector<std::uint8_t> out(8 + 25 * records.size());
    const auto size = a2fo_rs_xp_snapshot(records.data(), records.size(), out.data(), out.size());
    out.resize(size);
    return out;
}
bool Controller::restore(const std::vector<std::uint8_t> &bytes) {
    std::vector<A2foRsPlayerRecord> records(bytes.size() / 25);
    std::size_t count = 0;
    if (!a2fo_rs_xp_restore(bytes.data(), bytes.size(), records.data(), records.size(), &count))
        return false;
    PlayerMap next;
    for (std::size_t i = 0; i < count; ++i)
        next.emplace(records[i].id, cpp(records[i].state));
    players_.swap(next);
    thresholds_.clear();
    return true;
}
bool Controller::restore_player(PlayerId player, const std::vector<std::uint8_t> &bytes) {
    A2foRsPlayerRecord record{};
    if (!a2fo_rs_xp_restore_one(player, bytes.data(), bytes.size(), &record))
        return false;
    players_[player] = cpp(record.state);
    thresholds_.erase(player);
    return true;
}
} // namespace a2fo::collective_xp
