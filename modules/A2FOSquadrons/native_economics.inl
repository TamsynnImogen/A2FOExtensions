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
    g_member_economics.erase(klass);
    g_squad_economics.erase(klass);
    if (!readable_range(klass, kClassNativeCostsOffset + 6 * sizeof(std::int32_t))) return;
    a2fo::squadrons::BuildEconomics values;
    values.seconds = read_at<float>(klass, kClassBuildTimeOffset);
    for (std::size_t i = 0; i < 6; ++i)
        values.costs[i] = read_at<std::int32_t>(klass, kClassNativeCostsOffset + i * 4);
    for (std::size_t i = 0; i < kAdditionalCostFields.size(); ++i) {
        const auto text = field_value(fields, kAdditionalCostFields[i]);
        std::int32_t value = 0;
        const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
        // Match Resources: absent or malformed optional costs default to zero.
        if (parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && value >= 0)
            values.costs[i + 6] = value;
    }
    g_member_economics[klass] = values;
}

bool resolve_squad_economics(void* klass) {
    if (g_squad_economics.count(klass)) return true;
    const auto found = g_definitions.find(klass);
    if (found == g_definitions.end() || !g_resolving_economics.insert(klass).second) return false;
    struct Scope {
        void* klass;
        ~Scope() { g_resolving_economics.erase(klass); }
    } scope{klass};
    // Loading a member can dispatch more class callbacks and rehash the maps.
    const Definition definition = found->second;
    a2fo::squadrons::BuildEconomics total;
    const auto error = a2fo::squadrons::sum_build_economics(definition,
        [](const std::string& odf) -> std::optional<a2fo::squadrons::BuildEconomics> {
            if (resolve_member_class(odf) != MemberClass::mobile_craft) return {};
            const auto native = g_classes_by_odf.find(odf);
            if (native == g_classes_by_odf.end()) return {};
            const auto values = g_member_economics.find(native->second);
            return values == g_member_economics.end()
                ? std::optional<a2fo::squadrons::BuildEconomics>{} : values->second;
        }, &total);
    if (!error.empty() || !writable_range(klass, kClassNativeCostsOffset + 6 * sizeof(std::int32_t))) {
        log_text("Squad economics rejected for '" + definition.odf + "': " +
            (error.empty() ? "native class is not writable" : error));
        return false;
    }
    // Commit only after all members and all ten resources validate. The normal
    // native build button, payment, progress and refund paths read these fields.
    std::memcpy(static_cast<std::uint8_t*>(klass) + kClassBuildTimeOffset,
        &total.seconds, sizeof(total.seconds));
    std::memcpy(static_cast<std::uint8_t*>(klass) + kClassNativeCostsOffset,
        total.costs.data(), 6 * sizeof(std::int32_t));
    g_squad_economics[klass] = total;
    char line[512]{};
    std::snprintf(line, sizeof(line),
        "Squad economics '%s': %lu members, buildTime=%.3f; crew=%ld officers=%ld dilithium=%ld latinum=%ld metal=%ld biomatter=%ld tritanium=%ld supply=%ld credits=%ld connections=%ld",
        definition.odf.c_str(), static_cast<unsigned long>(definition.size()), total.seconds,
        static_cast<long>(total.costs[0]), static_cast<long>(total.costs[1]),
        static_cast<long>(total.costs[2]), static_cast<long>(total.costs[3]),
        static_cast<long>(total.costs[4]), static_cast<long>(total.costs[5]),
        static_cast<long>(total.costs[6]), static_cast<long>(total.costs[7]),
        static_cast<long>(total.costs[8]), static_cast<long>(total.costs[9]));
    log_line(line);
    return true;
}
