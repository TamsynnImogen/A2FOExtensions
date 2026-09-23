#pragma once

#include "squadron_config.hpp"

#include <map>
#include <set>

namespace a2fo::squadrons {

using SquadId = std::uint64_t;
using ObjectId = std::uint64_t;
using TicketId = std::uint64_t;
using TeamId = std::uint32_t;
// ObjectId is a nonzero, stable engine identity INCLUDING lifetime/generation,
// never a cast native pointer. The integration layer owns handle reconnection.
struct Craft {
    ObjectId id = 0;
    TeamId team = 0;
    std::string odf;
};

struct Slot {
    std::uint32_t row = 0;
    std::uint32_t ordinal = 0;
    std::string odf;
    ObjectId member = 0;
};

struct Replacement {
    TicketId ticket = 0;
    SquadId squad = 0;
    ObjectId yard = 0;
    TeamId team = 0;
    std::uint32_t slot = 0;
    std::string odf;
};

struct Squadron {
    SquadId id = 0;
    TeamId team = 0;
    Definition definition;
    std::vector<Slot> slots;
    std::optional<Replacement> replacement;
    std::uint32_t live_count() const noexcept;
    ObjectId representative() const noexcept;
    std::string badge() const;
};

struct RepairContext {
    ObjectId yard = 0;
    TeamId team = 0;
    bool compatible = false;
    bool accepted_repair_cycle = false;
    // A fresh native observation of ALL surviving members assigned to, and
    // present at, this yard. Proximity alone must not set accepted_repair_cycle.
    std::vector<ObjectId> serviced_members;
};

template<class T> struct Result {
    std::optional<T> value;
    std::string error;
    explicit operator bool() const noexcept { return value.has_value(); }
};

struct Removal {
    bool removed = false;
    bool squad_retired = false;
    // Caller cancels the native job and settles its payment/cap reservation.
    std::optional<Replacement> cancelled;
};

struct SelectionTile {
    SquadId squad = 0; // zero denotes an ordinary independent Craft
    ObjectId representative = 0;
    std::uint32_t live = 0;
    std::uint32_t maximum = 0;
};

struct Selection {
    std::vector<SelectionTile> tiles;
    std::vector<ObjectId> members;
};

// Structured persistence boundary; NOT a native save-file extension yet.
// Configured composition is frozen in each record. No native pointers, UI
// selection, wall clocks or derived reverse index are persisted.
struct Snapshot {
    std::uint32_t version = 1;
    SquadId next_squad = 1;
    TicketId next_ticket = 1;
    std::vector<Squadron> squads;
};

class Registry {
public:
    // Commit a complete, validated member set in stable slot order. This is
    // atomic MEMBERSHIP publication; native creation/cost rollback is separate.
    Result<SquadId> create(const Definition&, TeamId, const std::vector<Craft>&,
                           const ClassResolver&);
    // A completed build can launch one real Craft at a time. Keep its
    // configured vacant slots until the final output has left the yard.
    Result<SquadId> create_launching(const Definition&, TeamId,
                                    const Craft&, const ClassResolver&);
    bool add_launching(SquadId, std::size_t slot, const Craft&);
    bool finish_launching(SquadId);
    const Squadron* find(SquadId) const noexcept;
    const Squadron* containing(ObjectId) const noexcept;
    std::size_t size() const noexcept { return squads_.size(); }
    Removal remove(ObjectId);
    Removal change_owner(ObjectId, TeamId);

    // Request is a reservation, not permission to spawn or debit resources.
    // The synchronized native yard bridge must admit a real job or cancel it.
    Result<Replacement> reserve_replacement(SquadId, const RepairContext&);
    bool complete_replacement(TicketId, const Craft&, const RepairContext&);
    std::optional<Replacement> cancel_replacement(TicketId);
    std::vector<Replacement> cancel_yard(ObjectId);

    // Pure presentation/order planning. Does not modify native selection or
    // issue commands. Failure returns no partial command list or partial tiles.
    Result<Selection> select(const std::vector<Craft>& selected, TeamId,
                             std::size_t physical_command_limit) const;

    Snapshot snapshot() const;
    bool restore(const Snapshot&, const ClassResolver&, std::string& error);

private:
    bool reserved_yard(ObjectId) const noexcept;
    std::map<SquadId, Squadron> squads_;
    std::map<ObjectId, SquadId> membership_;
    std::set<SquadId> launching_;
    SquadId next_squad_ = 1;
    TicketId next_ticket_ = 1;
};

} // namespace a2fo::squadrons
