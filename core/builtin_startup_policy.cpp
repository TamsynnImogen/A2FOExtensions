#include "../rust/builtin_runtime_ffi.h"
#include "builtin_modules.hpp"
#include "builtin_startup_modules.hpp"
#include "extension_roots.hpp"
#include "module_policy.hpp"
#include <exception>
namespace a2fo {
namespace {
struct PolicyContext {
    const std::string &root;
    const std::string &command;
    ModulePolicy policy;
    std::exception_ptr error;
};
std::size_t count(void *p) noexcept {
    auto &c = *static_cast<PolicyContext *>(p);
    try {
        const auto roots = discover_extension_roots(c.root, c.command);
        c.policy = evaluate_module_policy(roots.roots, discover_installed_modules(c.root));
        return c.policy.entries.size();
    } catch (...) {
        c.error = std::current_exception();
        return 0;
    }
}
A2foRsRuntimeEntry get(void *p, std::size_t i) noexcept {
    const auto &e = static_cast<PolicyContext *>(p)->policy.entries[i];
    return {{reinterpret_cast<const std::uint8_t *>(e.name.data()), e.name.size()},
            static_cast<std::uint32_t>(e.state)};
}
} // namespace
bool startup_builtin_selected(BuiltinFeature f, const std::string &root,
                              const std::string &command) {
    PolicyContext c{root, command, {}, {}};
    const bool selected = a2fo_rs_startup_selected(static_cast<int>(f), &c, count, get) != 0;
    if (c.error)
        std::rethrow_exception(c.error);
    return selected;
}
} // namespace a2fo
