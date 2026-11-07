#include "hybrid_production.hpp"
#include "../../rust/hybrid_ffi.h"
#include <array>

namespace a2fo {
namespace {
using std::size_t;
A2foHybridPlan queue_plan(uint8_t op, const std::vector<ProductionJob>& jobs,
                          size_t capacity, const ProductionJob* candidate = nullptr,
                          uint32_t id = 0) noexcept {
    std::array<A2foHybridJob, kHybridProductionCapacity> rows{};
    for (size_t i = 0; i < jobs.size(); ++i)
        rows[i] = {jobs[i].queue_id, jobs[i].project_id,
                   static_cast<uint8_t>(jobs[i].method),
                   static_cast<uint8_t>(jobs[i].active)};
    A2foHybridJob input{};
    if (candidate)
        input = {candidate->queue_id, candidate->project_id,
                 static_cast<uint8_t>(candidate->method),
                 static_cast<uint8_t>(candidate->active)};
    return a2fo_rs_hybrid_queue(op, rows.data(), jobs.size(), capacity, input, id);
}
A2foHybridPlan list_plan(uint8_t op, const std::vector<ProductionListEntry>& entries,
                         ProductionMethod method, size_t index, uint32_t project) noexcept {
    std::array<A2foHybridEntry, 4 * kHybridBuildListCapacity> rows{};
    for (size_t i = 0; i < entries.size(); ++i)
        rows[i] = {static_cast<uint8_t>(entries[i].method), entries[i].index,
                   entries[i].project_id};
    return a2fo_rs_hybrid_list(op, rows.data(), entries.size(),
                                static_cast<uint8_t>(method), index, project);
}
}
const char* production_method_name(ProductionMethod method) noexcept {
    return a2fo_rs_hybrid_method_name(static_cast<uint8_t>(method));
}
const char* production_command_prefix(ProductionMethod method) noexcept {
    return a2fo_rs_hybrid_command_prefix(static_cast<uint8_t>(method));
}
std::string production_command(ProductionMethod method, size_t index) {
    return std::string(production_command_prefix(method)) + std::to_string(index);
}
bool parse_production_command(const std::string& command,
                              ProductionMethod& method, size_t& index) noexcept {
    uint8_t parsed_method = 0;
    size_t parsed_index = 0;
    if (!a2fo_rs_hybrid_parse_command(
            reinterpret_cast<const uint8_t*>(command.data()), command.size(),
            &parsed_method, &parsed_index)) return false;
    method = static_cast<ProductionMethod>(parsed_method);
    index = parsed_index;
    return true;
}
HybridProductionQueue::HybridProductionQueue(size_t capacity) noexcept
    : capacity_(capacity > kHybridProductionCapacity ? kHybridProductionCapacity : capacity) {
    try { jobs_.reserve(capacity_); } catch (...) { capacity_ = 0; }
}
EnqueueResult HybridProductionQueue::enqueue(const ProductionJob& job) {
    auto decision = queue_plan(0, jobs_, capacity_, &job);
    if (!decision.result) {
        ProductionJob queued = job;
        queued.active = false;
        jobs_.push_back(queued);
    }
    return static_cast<EnqueueResult>(decision.result);
}
const ProductionJob* HybridProductionQueue::start_next() noexcept {
    auto decision = queue_plan(1, jobs_, capacity_);
    if (!decision.result) return nullptr;
    if (decision.activate) {
        for (auto& job : jobs_) job.active = false;
        jobs_.front().active = true;
    }
    return &jobs_.front();
}
bool HybridProductionQueue::finish_active() noexcept {
    auto decision = queue_plan(2, jobs_, capacity_);
    if (!decision.result) return false;
    jobs_.erase(jobs_.begin());
    if (decision.clear) jobs_.clear();
    return true;
}
bool HybridProductionQueue::cancel(uint32_t id) noexcept {
    auto decision = queue_plan(3, jobs_, capacity_, nullptr, id);
    if (!decision.result) return false;
    jobs_.erase(jobs_.begin() + decision.index);
    return true;
}
void HybridProductionQueue::clear() noexcept { jobs_.clear(); }
const ProductionJob* HybridProductionQueue::active_job() const noexcept {
    return queue_plan(4, jobs_, capacity_).result ? &jobs_.front() : nullptr;
}
const ProductionJob* HybridProductionQueue::find(uint32_t id) const noexcept {
    auto decision = queue_plan(5, jobs_, capacity_, nullptr, id);
    return decision.result ? &jobs_[decision.index] : nullptr;
}
const std::vector<ProductionJob>& HybridProductionQueue::jobs() const noexcept { return jobs_; }
size_t HybridProductionQueue::size() const noexcept { return jobs_.size(); }
size_t HybridProductionQueue::capacity() const noexcept { return capacity_; }
bool HybridProductionQueue::empty() const noexcept { return jobs_.empty(); }
bool HybridProductionQueue::evolution_barrier() const noexcept {
    return queue_plan(6, jobs_, capacity_).result != 0;
}
bool HybridProductionQueue::accepts_new_jobs() const noexcept {
    return queue_plan(7, jobs_, capacity_).result != 0;
}
AddListEntryResult HybridBuildLists::add(ProductionMethod method, size_t index,
                                         uint32_t project) {
    auto decision = list_plan(0, entries_, method, index, project);
    if (!decision.result) entries_.push_back({method, index, project});
    return static_cast<AddListEntryResult>(decision.result);
}
const ProductionListEntry* HybridBuildLists::at(ProductionMethod method,
                                                 size_t index) const noexcept {
    auto decision = list_plan(1, entries_, method, index, 0);
    return decision.result ? &entries_[decision.index] : nullptr;
}
const ProductionListEntry* HybridBuildLists::explicit_entry_for(uint32_t project) const noexcept {
    auto decision = list_plan(2, entries_, ProductionMethod::legacy, 0, project);
    return decision.result ? &entries_[decision.index] : nullptr;
}
ProductionMethod HybridBuildLists::resolve(uint32_t project,
                                           ProductionMethod fallback) const noexcept {
    return static_cast<ProductionMethod>(
        list_plan(3, entries_, fallback, 0, project).result);
}
const std::vector<ProductionListEntry>& HybridBuildLists::entries() const noexcept {
    return entries_;
}
bool HybridBuildLists::empty() const noexcept { return entries_.empty(); }
}
