// Included in module.cpp after class resolution. Costs come from completed
// native member classes, preserving includes, mod precedence and defaults.
constexpr std::size_t kClassBuildTimeOffset = 0x68;
constexpr std::size_t kClassNativeCostsOffset = 0x80;
constexpr std::array<const char*, 4> kAdditionalCostFields{{
    "tritaniumCost", "supplyCost", "creditsCost", "collectiveconnectionsCost"}};
std::unordered_map<void*, a2fo::squadrons::BuildEconomics> g_member_economics;
std::unordered_map<void*, a2fo::squadrons::BuildEconomics> g_squad_economics;
std::unordered_set<void*> g_resolving_economics;

void capture_member_economics(void* klass, const OdfFields& fields) {
    struct Context {
        void* klass;
        const OdfFields& fields;
        a2fo::squadrons::BuildEconomics values;
        std::int32_t parsed_value = 0;
        std::exception_ptr exception{};
        Context(void* value, const OdfFields& source)
            : klass(value), fields(source) {}
    } context{klass, fields};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t index) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0: g_member_economics.erase(c.klass); return 1;
            case 1: g_squad_economics.erase(c.klass); return 1;
            case 2:
                return readable_range(c.klass,
                    kClassNativeCostsOffset + 6 * sizeof(std::int32_t));
            case 3:
                c.values.seconds = read_at<float>(c.klass, kClassBuildTimeOffset);
                return 1;
            case 4:
                c.values.costs[index] = read_at<std::int32_t>(
                    c.klass, kClassNativeCostsOffset + index * 4);
                return 1;
            case 5: {
                const auto text = field_value(c.fields, kAdditionalCostFields[index]);
                c.parsed_value = 0;
                return a2fo_rs_stage10_squadrons_cost(
                    reinterpret_cast<const std::uint8_t*>(text.data()),text.size(),&c.parsed_value);
            }
            case 6:
                c.values.costs[index + 6] = c.parsed_value;
                return 1;
            case 7: g_member_economics[c.klass] = c.values; return 1;
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    a2fo_rs_stage8_squadrons_capture_economics(step, &context);
    if (context.exception) std::rethrow_exception(context.exception);
}

bool resolve_squad_economics(void* klass) {
    struct Context {
        void* klass;
        decltype(g_definitions)::iterator found{};
        bool inserted = false;
        Definition definition;
        a2fo::squadrons::BuildEconomics total;
        std::string error;
        std::exception_ptr exception{};
        explicit Context(void* value) : klass(value) {}
        ~Context() {
            if (inserted) g_resolving_economics.erase(klass);
        }
    } context{klass};
    const auto step = +[](void* opaque, std::uint32_t op,
                          std::uintptr_t) noexcept -> std::uintptr_t {
        auto& c = *static_cast<Context*>(opaque);
        try {
            switch (op) {
            case 0: return g_squad_economics.count(c.klass) != 0;
            case 1:
                c.found = g_definitions.find(c.klass);
                return c.found != g_definitions.end();
            case 2:
                c.inserted = g_resolving_economics.insert(c.klass).second;
                return c.inserted;
            case 3: c.definition = c.found->second; return 1;
            case 4:
                c.error = a2fo::squadrons::sum_build_economics(c.definition,
                    [](const std::string& odf)
                        -> std::optional<a2fo::squadrons::BuildEconomics> {
                        struct MemberContext {
                            const std::string& odf;
                            void* klass = nullptr;
                            std::optional<a2fo::squadrons::BuildEconomics> value;
                            std::exception_ptr exception{};
                            explicit MemberContext(const std::string& key)
                                : odf(key) {}
                        } member{odf};
                        const auto member_step = +[](void* opaque,
                                std::uint32_t code,
                                std::uintptr_t) noexcept -> std::uintptr_t {
                            auto& m = *static_cast<MemberContext*>(opaque);
                            try {
                                switch (code) {
                                case 0: return resolve_member_class(m.odf) ==
                                    MemberClass::mobile_craft;
                                case 1: {
                                    // resolve_member_class may dispatch a
                                    // reentrant class callback and rehash maps.
                                    const auto found = g_classes_by_odf.find(m.odf);
                                    if (found == g_classes_by_odf.end()) return 0;
                                    m.klass = found->second;
                                    return 1;
                                }
                                case 2: {
                                    const auto found = g_member_economics.find(m.klass);
                                    if (found == g_member_economics.end()) return 0;
                                    m.value = found->second;
                                    return 1;
                                }
                                case 3: return m.value.has_value();
                                default: return 0;
                                }
                            } catch (...) {
                                m.exception = std::current_exception();
                                return UINTPTR_MAX;
                            }
                        };
                        const bool resolved =
                            a2fo_rs_stage8_squadrons_resolve_economics_member(
                                member_step, &member) != 0;
                        if (member.exception)
                            std::rethrow_exception(member.exception);
                        return resolved ? std::move(member.value)
                                        : std::nullopt;
                    }, &c.total);
                return 1;
            case 5: return c.error.empty();
            case 6:
                return writable_range(c.klass,
                    kClassNativeCostsOffset + 6 * sizeof(std::int32_t));
            case 7:
                log_text("Squad economics rejected for '" + c.definition.odf +
                         "': " + (c.error.empty()
                             ? "native class is not writable" : c.error));
                return 1;
            case 8:
                std::memcpy(static_cast<std::uint8_t*>(c.klass) +
                    kClassBuildTimeOffset, &c.total.seconds,
                    sizeof(c.total.seconds));
                return 1;
            case 9:
                std::memcpy(static_cast<std::uint8_t*>(c.klass) +
                    kClassNativeCostsOffset, c.total.costs.data(),
                    6 * sizeof(std::int32_t));
                return 1;
            case 10: g_squad_economics[c.klass] = c.total; return 1;
            case 11: {
                char line[512]{};
                std::snprintf(line, sizeof(line),
                    "Squad economics '%s': %lu members, buildTime=%.3f; crew=%ld officers=%ld dilithium=%ld latinum=%ld metal=%ld biomatter=%ld tritanium=%ld supply=%ld credits=%ld connections=%ld",
                    c.definition.odf.c_str(),
                    static_cast<unsigned long>(c.definition.size()),
                    c.total.seconds,
                    static_cast<long>(c.total.costs[0]),
                    static_cast<long>(c.total.costs[1]),
                    static_cast<long>(c.total.costs[2]),
                    static_cast<long>(c.total.costs[3]),
                    static_cast<long>(c.total.costs[4]),
                    static_cast<long>(c.total.costs[5]),
                    static_cast<long>(c.total.costs[6]),
                    static_cast<long>(c.total.costs[7]),
                    static_cast<long>(c.total.costs[8]),
                    static_cast<long>(c.total.costs[9]));
                log_line(line);
                return 1;
            }
            default: return 0;
            }
        } catch (...) {
            c.exception = std::current_exception();
            return UINTPTR_MAX;
        }
    };
    const bool resolved = a2fo_rs_stage8_squadrons_resolve_economics(
        step, &context) != 0;
    if (context.exception) std::rethrow_exception(context.exception);
    return resolved;
}
