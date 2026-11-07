#include "vs11.hpp"
#include "../../rust/lead_ffi.h"
#include <exception>
#include <utility>
namespace a2fo::renderer::shaders {
namespace {
struct Context {
    std::string *source;
    std::string *error;
    std::exception_ptr failure;
};
void receive(void *p, std::uint32_t kind, const std::uint8_t *text, std::size_t length) noexcept {
    auto &c = *static_cast<Context *>(p);
    if (c.failure)
        return;
    try {
        auto *out = kind == 0 ? c.source : c.error;
        if (out)
            out->assign(reinterpret_cast<const char *>(text), length);
    } catch (...) {
        c.failure = std::current_exception();
    }
}
} // namespace
std::string vs11_output_interface() {
    std::string source;
    Context c{&source, nullptr, {}};
    a2fo_rs_lead_vs_interface(&c, receive);
    if (c.failure)
        std::rethrow_exception(c.failure);
    return source;
}
bool translate_vs11(const std::uint32_t *words, std::size_t count, Vs11Translation &out,
                    std::string &error) {
    out = {};
    error.clear();
    Vs11Translation result;
    Context c{&result.hlsl, &error, {}};
    const bool ok = a2fo_rs_lead_vs11(words, count, &c, receive, &result.input_mask,
                                      result.output_masks.data(), &result.instruction_count) != 0;
    if (c.failure)
        std::rethrow_exception(c.failure);
    if (ok)
        out = std::move(result);
    return ok;
}
} // namespace a2fo::renderer::shaders
