/*
 * File: modules/A2FOHybridBuild/hybrid_production.hpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: HybridBuild public interfaces for queue-state callbacks and constructor placement policy integration.
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo {

// buildItem<N> remains the legacy, classLabel-selected route. The four
// explicit methods are carried beside each queue entry so a future engine
// adapter does not have to infer behavior from the producer's classLabel.
enum class ProductionMethod : std::uint8_t {
    legacy,
    construct,
    yard,
    research,
    evolve,
};

constexpr std::size_t kHybridProductionCapacity = 10;
constexpr std::size_t kHybridBuildListCapacity = 57;

const char* production_method_name(ProductionMethod method) noexcept;
const char* production_command_prefix(ProductionMethod method) noexcept;

// Produces the mod-facing ODF spelling, for example constructItem3. Parsing is
// case-insensitive because ParameterDB commands are case-insensitive too.
std::string production_command(ProductionMethod method, std::size_t index);
bool parse_production_command(const std::string& command,
                              ProductionMethod& method,
                              std::size_t& index) noexcept;

struct ProductionPlacement {
    bool present = false;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float facing = 0.0f;
};

struct ProductionJob {
    std::uint32_t queue_id = 0;
    std::uint32_t project_id = 0;
    ProductionMethod method = ProductionMethod::legacy;
    ProductionPlacement placement{};
    bool active = false;
};

enum class EnqueueResult : std::uint8_t {
    queued,
    invalid_project,
    invalid_queue_id,
    duplicate_queue_id,
    queue_full,
    evolution_barrier,
};

// Platform-independent state machine for the engine bridge. It deliberately
// owns no Armada pointers: queue IDs/project IDs are stable enough for
// synchronized commands and save/load reconstruction, while native pointers
// are resolved only when an executor actually runs a job.
class HybridProductionQueue {
public:
    explicit HybridProductionQueue(
        std::size_t capacity = kHybridProductionCapacity) noexcept;

    EnqueueResult enqueue(const ProductionJob& job);
    const ProductionJob* start_next() noexcept;
    bool finish_active() noexcept;
    bool cancel(std::uint32_t queue_id) noexcept;
    void clear() noexcept;

    const ProductionJob* active_job() const noexcept;
    const ProductionJob* find(std::uint32_t queue_id) const noexcept;
    const std::vector<ProductionJob>& jobs() const noexcept;

    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;
    bool empty() const noexcept;
    bool evolution_barrier() const noexcept;
    bool accepts_new_jobs() const noexcept;

private:
    std::size_t capacity_;
    std::vector<ProductionJob> jobs_;
};

struct ProductionListEntry {
    ProductionMethod method = ProductionMethod::legacy;
    std::size_t index = 0;
    std::uint32_t project_id = 0;
};

enum class AddListEntryResult : std::uint8_t {
    added,
    invalid_method,
    invalid_index,
    invalid_project,
    duplicate_slot,
    ambiguous_project,
};

// Explicit list membership is unique by target class. Armada's synchronized
// build command carries the selected class, not the ODF key that created its
// button, so putting the same target in two explicit method lists would be
// ambiguous on every peer.
class HybridBuildLists {
public:
    AddListEntryResult add(ProductionMethod method, std::size_t index,
                           std::uint32_t project_id);

    const ProductionListEntry* at(ProductionMethod method,
                                  std::size_t index) const noexcept;
    const ProductionListEntry* explicit_entry_for(
        std::uint32_t project_id) const noexcept;
    ProductionMethod resolve(std::uint32_t project_id,
                             ProductionMethod legacy_fallback) const noexcept;
    const std::vector<ProductionListEntry>& entries() const noexcept;
    bool empty() const noexcept;

private:
    std::vector<ProductionListEntry> entries_;
};

}  // namespace a2fo
