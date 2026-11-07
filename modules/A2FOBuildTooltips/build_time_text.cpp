#include "build_time_text.hpp"
#include "../../rust/text_geometry_ffi.h"
namespace a2fo::build_tooltips {
float reconcile_global_build_time(float raw, float adjusted, float modifier) noexcept {
    return a2fo_rs_text_reconcile(raw, adjusted, modifier);
}
bool format_build_time_token(const char *separator, float seconds, bool compact, char *output,
                             std::size_t capacity) noexcept {
    return a2fo_rs_text_build_token(separator, seconds, compact, output, capacity) != 0;
}
bool format_resource_cost_token(const char *separator, const char *name, std::int32_t cost,
                                bool compact, char *output, std::size_t capacity) noexcept {
    return a2fo_rs_text_resource_token(separator, name, cost, compact, output, capacity) != 0;
}
} // namespace a2fo::build_tooltips
