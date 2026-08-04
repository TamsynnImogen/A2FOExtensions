#include "hybrid_production.hpp"

#include <cassert>
#include <cstddef>

namespace {

a2fo::ProductionJob job(std::uint32_t queue_id,
                        std::uint32_t project_id,
                        a2fo::ProductionMethod method) {
    a2fo::ProductionJob result;
    result.queue_id = queue_id;
    result.project_id = project_id;
    result.method = method;
    return result;
}

}  // namespace

int main() {
    using a2fo::AddListEntryResult;
    using a2fo::EnqueueResult;
    using a2fo::HybridBuildLists;
    using a2fo::HybridProductionQueue;
    using a2fo::ProductionMethod;

    ProductionMethod parsed_method = ProductionMethod::legacy;
    std::size_t parsed_index = 0;
    assert(a2fo::production_command(ProductionMethod::construct, 3) ==
           "constructItem3");
    assert(a2fo::parse_production_command(
        "ResearchItem56", parsed_method, parsed_index));
    assert(parsed_method == ProductionMethod::research);
    assert(parsed_index == 56);
    assert(!a2fo::parse_production_command(
        "researchItem57", parsed_method, parsed_index));
    assert(!a2fo::parse_production_command(
        "yardItem-1", parsed_method, parsed_index));
    assert(!a2fo::parse_production_command(
        "evolveItem", parsed_method, parsed_index));

    HybridBuildLists lists;
    assert(lists.add(ProductionMethod::construct, 0, 100) ==
           AddListEntryResult::added);
    assert(lists.add(ProductionMethod::yard, 0, 200) ==
           AddListEntryResult::added);
    assert(lists.add(ProductionMethod::research, 4, 300) ==
           AddListEntryResult::added);
    assert(lists.add(ProductionMethod::evolve, 1, 400) ==
           AddListEntryResult::added);
    assert(lists.add(ProductionMethod::legacy, 0, 500) ==
           AddListEntryResult::invalid_method);
    assert(lists.add(ProductionMethod::yard, 0, 201) ==
           AddListEntryResult::duplicate_slot);
    assert(lists.add(ProductionMethod::research, 5, 200) ==
           AddListEntryResult::ambiguous_project);
    assert(lists.resolve(300, ProductionMethod::yard) ==
           ProductionMethod::research);
    assert(lists.resolve(999, ProductionMethod::yard) ==
           ProductionMethod::yard);

    HybridProductionQueue queue;
    assert(queue.capacity() == 10);
    a2fo::ProductionJob placed = job(
        1, 100, ProductionMethod::construct);
    placed.placement = {true, 12.0f, 1.5f, -9.0f, 0.75f};
    assert(queue.enqueue(placed) ==
           EnqueueResult::queued);
    assert(queue.enqueue(job(2, 200, ProductionMethod::yard)) ==
           EnqueueResult::queued);
    assert(queue.enqueue(job(3, 300, ProductionMethod::research)) ==
           EnqueueResult::queued);
    assert(queue.start_next()->queue_id == 1);
    assert(queue.start_next()->queue_id == 1);  // never starts two jobs
    assert(queue.active_job()->method == ProductionMethod::construct);
    assert(queue.active_job()->placement.present);
    assert(queue.active_job()->placement.x == 12.0f);
    assert(queue.active_job()->placement.facing == 0.75f);
    assert(queue.finish_active());
    assert(queue.start_next()->queue_id == 2);
    assert(queue.finish_active());
    assert(queue.start_next()->queue_id == 3);
    assert(queue.finish_active());
    assert(queue.empty());

    // Existing work stays ahead of evolution. As soon as evolution is
    // queued, every list is locked until that order is cancelled or acts.
    assert(queue.enqueue(job(10, 210, ProductionMethod::yard)) ==
           EnqueueResult::queued);
    assert(queue.enqueue(job(11, 410, ProductionMethod::evolve)) ==
           EnqueueResult::queued);
    assert(queue.evolution_barrier());
    assert(!queue.accepts_new_jobs());
    assert(queue.enqueue(job(12, 310, ProductionMethod::research)) ==
           EnqueueResult::evolution_barrier);
    assert(queue.start_next()->queue_id == 10);
    assert(queue.finish_active());
    assert(queue.start_next()->queue_id == 11);
    assert(queue.finish_active());
    assert(queue.empty());
    assert(queue.accepts_new_jobs());

    assert(queue.enqueue(job(20, 420, ProductionMethod::evolve)) ==
           EnqueueResult::queued);
    assert(queue.cancel(20));
    assert(!queue.evolution_barrier());
    assert(queue.enqueue(job(21, 220, ProductionMethod::yard)) ==
           EnqueueResult::queued);
    assert(queue.cancel(21));

    assert(queue.enqueue(job(0, 100, ProductionMethod::construct)) ==
           EnqueueResult::invalid_queue_id);
    assert(queue.enqueue(job(30, 0, ProductionMethod::construct)) ==
           EnqueueResult::invalid_project);
    assert(queue.enqueue(job(30, 230, ProductionMethod::yard)) ==
           EnqueueResult::queued);
    assert(queue.enqueue(job(30, 231, ProductionMethod::yard)) ==
           EnqueueResult::duplicate_queue_id);
    queue.clear();

    for (std::uint32_t index = 0; index < 10; ++index) {
        assert(queue.enqueue(job(100 + index, 1000 + index,
                                 ProductionMethod::yard)) ==
               EnqueueResult::queued);
    }
    assert(queue.enqueue(job(200, 2000, ProductionMethod::research)) ==
           EnqueueResult::queue_full);

    // Repeated production is just another tail insertion. It cannot jump
    // over an ordinary mixed-method job and starve it.
    queue.clear();
    assert(queue.enqueue(job(300, 3000, ProductionMethod::yard)) ==
           EnqueueResult::queued);
    assert(queue.enqueue(job(301, 3001, ProductionMethod::research)) ==
           EnqueueResult::queued);
    assert(queue.enqueue(job(302, 3000, ProductionMethod::yard)) ==
           EnqueueResult::queued);
    assert(queue.start_next()->queue_id == 300);
    assert(queue.finish_active());
    assert(queue.start_next()->queue_id == 301);
    assert(queue.finish_active());
    assert(queue.start_next()->queue_id == 302);
}
