/*
 * File: modules/A2FOHybridBuild/hybrid_production.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Core HybridBuild production policy: construction execution, command filtering, and queue sidecar lifecycle.
 */

#include "hybrid_production.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

namespace a2fo {
namespace {

bool ascii_equal_case_insensitive(const std::string& value,
                                  const char* expected,
                                  std::size_t length) noexcept {
    for (std::size_t index = 0; index < length; ++index) {
        const auto left = static_cast<unsigned char>(value[index]);
        const auto right = static_cast<unsigned char>(expected[index]);
        if (std::tolower(left) != std::tolower(right)) return false;
    }
    return true;
}

bool is_explicit_method(ProductionMethod method) noexcept {
    return method == ProductionMethod::construct ||
           method == ProductionMethod::yard ||
           method == ProductionMethod::research ||
           method == ProductionMethod::evolve;
}

}  // namespace

const char* production_method_name(ProductionMethod method) noexcept {
    switch (method) {
        case ProductionMethod::legacy: return "legacy";
        case ProductionMethod::construct: return "construct";
        case ProductionMethod::yard: return "yard";
        case ProductionMethod::research: return "research";
        case ProductionMethod::evolve: return "evolve";
    }
    return "unknown";
}

const char* production_command_prefix(ProductionMethod method) noexcept {
    switch (method) {
        case ProductionMethod::legacy: return "buildItem";
        case ProductionMethod::construct: return "constructItem";
        case ProductionMethod::yard: return "yardItem";
        case ProductionMethod::research: return "researchItem";
        case ProductionMethod::evolve: return "evolveItem";
    }
    return "";
}

std::string production_command(ProductionMethod method, std::size_t index) {
    return std::string(production_command_prefix(method)) +
           std::to_string(index);
}

bool parse_production_command(const std::string& command,
                              ProductionMethod& method,
                              std::size_t& index) noexcept {
    constexpr ProductionMethod methods[] = {
        ProductionMethod::construct,
        ProductionMethod::yard,
        ProductionMethod::research,
        ProductionMethod::evolve,
        ProductionMethod::legacy,
    };
    for (const ProductionMethod candidate : methods) {
        const char* prefix = production_command_prefix(candidate);
        const std::size_t prefix_length = std::char_traits<char>::length(
            prefix);
        if (command.size() <= prefix_length ||
            !ascii_equal_case_insensitive(command, prefix, prefix_length)) {
            continue;
        }

        std::size_t parsed = 0;
        for (std::size_t cursor = prefix_length;
             cursor < command.size(); ++cursor) {
            const unsigned char ch = static_cast<unsigned char>(
                command[cursor]);
            if (!std::isdigit(ch)) return false;
            const std::size_t digit = static_cast<std::size_t>(ch - '0');
            if (parsed > (std::numeric_limits<std::size_t>::max() - digit) /
                             10) {
                return false;
            }
            parsed = parsed * 10 + digit;
        }
        if (parsed >= kHybridBuildListCapacity) return false;
        method = candidate;
        index = parsed;
        return true;
    }
    return false;
}

HybridProductionQueue::HybridProductionQueue(std::size_t capacity) noexcept
    : capacity_(capacity) {
    if (capacity_ > kHybridProductionCapacity) {
        capacity_ = kHybridProductionCapacity;
    }
    try {
        jobs_.reserve(capacity_);
    } catch (...) {
        capacity_ = 0;
    }
}

EnqueueResult HybridProductionQueue::enqueue(const ProductionJob& job) {
    if (job.project_id == 0) return EnqueueResult::invalid_project;
    if (job.queue_id == 0) return EnqueueResult::invalid_queue_id;
    if (find(job.queue_id)) return EnqueueResult::duplicate_queue_id;
    // The barrier test comes before capacity so the UI receives the useful
    // reason even while an evolve job occupies the final native slot.
    if (evolution_barrier()) return EnqueueResult::evolution_barrier;
    if (jobs_.size() >= capacity_) return EnqueueResult::queue_full;

    ProductionJob queued = job;
    queued.active = false;
    jobs_.push_back(queued);
    return EnqueueResult::queued;
}

const ProductionJob* HybridProductionQueue::start_next() noexcept {
    if (jobs_.empty()) return nullptr;
    if (jobs_.front().active) return &jobs_.front();
    // A valid queue can only have an active front item. Normalizing all flags
    // here also makes reconstructed sidecars fail safe after an old save.
    for (ProductionJob& job : jobs_) job.active = false;
    jobs_.front().active = true;
    return &jobs_.front();
}

bool HybridProductionQueue::finish_active() noexcept {
    if (jobs_.empty() || !jobs_.front().active) return false;
    const bool evolved = jobs_.front().method == ProductionMethod::evolve;
    jobs_.erase(jobs_.begin());
    if (evolved) {
        // An evolve order is required to be last. Clearing defensively keeps
        // corrupt or legacy reconstructed state from transferring production
        // orders to the replacement object.
        jobs_.clear();
    }
    return true;
}

bool HybridProductionQueue::cancel(std::uint32_t queue_id) noexcept {
    const auto found = std::find_if(
        jobs_.begin(), jobs_.end(), [queue_id](const ProductionJob& job) {
            return job.queue_id == queue_id;
        });
    if (found == jobs_.end()) return false;
    jobs_.erase(found);
    return true;
}

void HybridProductionQueue::clear() noexcept {
    jobs_.clear();
}

const ProductionJob* HybridProductionQueue::active_job() const noexcept {
    return !jobs_.empty() && jobs_.front().active ? &jobs_.front() : nullptr;
}

const ProductionJob* HybridProductionQueue::find(
    std::uint32_t queue_id) const noexcept {
    const auto found = std::find_if(
        jobs_.begin(), jobs_.end(), [queue_id](const ProductionJob& job) {
            return job.queue_id == queue_id;
        });
    return found == jobs_.end() ? nullptr : &*found;
}

const std::vector<ProductionJob>& HybridProductionQueue::jobs() const noexcept {
    return jobs_;
}

std::size_t HybridProductionQueue::size() const noexcept {
    return jobs_.size();
}

std::size_t HybridProductionQueue::capacity() const noexcept {
    return capacity_;
}

bool HybridProductionQueue::empty() const noexcept {
    return jobs_.empty();
}

bool HybridProductionQueue::evolution_barrier() const noexcept {
    return std::any_of(jobs_.begin(), jobs_.end(),
                       [](const ProductionJob& job) {
                           return job.method == ProductionMethod::evolve;
                       });
}

bool HybridProductionQueue::accepts_new_jobs() const noexcept {
    return !evolution_barrier() && jobs_.size() < capacity_;
}

AddListEntryResult HybridBuildLists::add(ProductionMethod method,
                                          std::size_t index,
                                          std::uint32_t project_id) {
    if (!is_explicit_method(method)) {
        return AddListEntryResult::invalid_method;
    }
    if (index >= kHybridBuildListCapacity) {
        return AddListEntryResult::invalid_index;
    }
    if (project_id == 0) return AddListEntryResult::invalid_project;
    if (at(method, index)) return AddListEntryResult::duplicate_slot;
    if (explicit_entry_for(project_id)) {
        return AddListEntryResult::ambiguous_project;
    }
    entries_.push_back({method, index, project_id});
    return AddListEntryResult::added;
}

const ProductionListEntry* HybridBuildLists::at(
    ProductionMethod method, std::size_t index) const noexcept {
    const auto found = std::find_if(
        entries_.begin(), entries_.end(),
        [method, index](const ProductionListEntry& entry) {
            return entry.method == method && entry.index == index;
        });
    return found == entries_.end() ? nullptr : &*found;
}

const ProductionListEntry* HybridBuildLists::explicit_entry_for(
    std::uint32_t project_id) const noexcept {
    const auto found = std::find_if(
        entries_.begin(), entries_.end(),
        [project_id](const ProductionListEntry& entry) {
            return entry.project_id == project_id;
        });
    return found == entries_.end() ? nullptr : &*found;
}

ProductionMethod HybridBuildLists::resolve(
    std::uint32_t project_id,
    ProductionMethod legacy_fallback) const noexcept {
    const ProductionListEntry* explicit_entry = explicit_entry_for(
        project_id);
    return explicit_entry ? explicit_entry->method : legacy_fallback;
}

const std::vector<ProductionListEntry>& HybridBuildLists::entries()
    const noexcept {
    return entries_;
}

bool HybridBuildLists::empty() const noexcept {
    return entries_.empty();
}

}  // namespace a2fo
