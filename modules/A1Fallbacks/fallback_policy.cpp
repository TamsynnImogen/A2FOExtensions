#include "fallback_policy.hpp"
#include "../../rust/lead_ffi.h"
#include <cstdint>
#include <exception>
namespace a1fallbacks {
namespace {
struct Context {
    SpriteFallbackNames result;
    std::exception_ptr error;
};
void receive(void *p, std::uint32_t kind, const std::uint8_t *data, std::size_t length) noexcept {
    auto &c = *static_cast<Context *>(p);
    if (c.error)
        return;
    try {
        (kind == 0 ? c.result.build_button : c.result.faction_icon)
            .assign(reinterpret_cast<const char *>(data), length);
    } catch (...) {
        c.error = std::current_exception();
    }
}
} // namespace
SpriteFallbackNames sprite_fallback_names(std::string_view object, std::string_view faction) {
    Context c{};
    a2fo_rs_lead_fallback(reinterpret_cast<const std::uint8_t *>(object.data()), object.size(),
                          reinterpret_cast<const std::uint8_t *>(faction.data()), faction.size(),
                          &c, receive);
    if (c.error)
        std::rethrow_exception(c.error);
    return c.result;
}
} // namespace a1fallbacks
