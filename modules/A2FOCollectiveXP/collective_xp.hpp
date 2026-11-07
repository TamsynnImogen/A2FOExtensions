#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace a2fo::collective_xp {

using PlayerId = std::uint32_t;
using UnitId = std::uint64_t; // Adapter supplies a stable identity including lifetime/generation.

struct ClassRank {
    std::string id;
    std::uint32_t level = 0; // Native XPlevel: base rank is 1.
    double next_xp = 0;       // Positive only when next_class is present.
    std::string next_class;   // Explicit native reference, never a guessed suffix.
};

enum class Status {
    ok, missing_class, malformed_chain, cycle, current_not_in_family,
    invalid_input, overflow, unknown_unit, unknown_player, invalid_snapshot
};

struct Resolution {
    Status status = Status::ok;
    std::string class_id;
    std::uint32_t level = 0;
    bool has_next = false;
    double next_xp = 0;
    explicit operator bool() const { return status == Status::ok; }
};

class Catalog {
public:
    bool add(const ClassRank& rank); // Duplicate IDs and malformed local records fail.
    Resolution resolve(const std::string& base, std::uint32_t target_level,
                       const std::string& current = {}) const;
    std::uint32_t native_level(const std::string& class_id) const;
private:
    std::map<std::string, ClassRank> ranks_;
};

struct Unit {
    UnitId id = 0;
    PlayerId player = 0;
    std::string base_class;
    std::string current_class;
    bool native_capable = false;
    bool completed = false;
    bool live = false;
};

struct PlayerState {
    bool enabled = false;
    std::uint32_t level = 1;
    double pool = 0;
    double multiplier = 1;
};

struct Progress {
    Status status = Status::ok;
    bool consumed = false;
    std::uint32_t levels_gained = 0;
    PlayerState state;
    double threshold = 0; // Zero means no eligible next rank.
};

class Controller {
public:
    explicit Controller(const Catalog& catalog) : catalog_(catalog) {}
    bool configure(PlayerId player, bool enabled, double multiplier);
    void reset_player(PlayerId player); // New native Team/match lifetime.
    Status upsert(const Unit& unit); // Atomic; caller resolves a family base at admission.
    bool remove(UnitId id);
    Status transfer(UnitId id, PlayerId new_player);
    Resolution target(UnitId id) const; // Pure promotion target; no native replacement.
    Progress award(PlayerId player, double xp);
    Progress reconcile(PlayerId player); // Call after creation/removal/capture/fleet loss.
    const PlayerState* state(PlayerId player) const;
    double threshold(PlayerId player, Status* status = nullptr) const;

    // Explicit v1 little-endian portable state. Units and thresholds are rebuilt.
    std::vector<std::uint8_t> snapshot() const;
    std::vector<std::uint8_t> snapshot_player(PlayerId player) const;
    bool restore(const std::vector<std::uint8_t>& bytes); // Transactional.
    bool restore_player(PlayerId player, const std::vector<std::uint8_t>& bytes);
private:
    Progress process(PlayerId player, double xp, bool award_requested);
    double threshold_for(PlayerId player, std::uint32_t level, Status& status) const;
    double cached_threshold(PlayerId player, std::uint32_t level, Status& status) const;
    const Catalog& catalog_;
    std::map<PlayerId, PlayerState> players_;
    std::map<UnitId, Unit> units_;
    mutable std::map<PlayerId, std::pair<std::uint32_t, double>> thresholds_;
};

} // namespace a2fo::collective_xp
