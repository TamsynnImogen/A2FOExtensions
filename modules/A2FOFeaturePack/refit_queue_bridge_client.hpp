/* Producer-side wrappers for the optional RefitYards callback bridge. */

#pragma once

#include <cstdint>

namespace a2fo {

bool consume_refit_synchronized_command(
    void* source, void* target_class) noexcept;
bool cancel_refit_synchronized_command(void* source) noexcept;
bool refit_has_waiting_job(void* producer) noexcept;
void notify_refit_job_finished(
    void* producer, std::uint32_t queue_id,
    void* target_class, void* result) noexcept;
void notify_refit_job_removed(
    void* producer, std::uint32_t queue_id, void* target_class,
    std::uint32_t removal_kind) noexcept;

}  // namespace a2fo
