#include "squadron_config.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>

namespace a2fo::squadrons {
namespace {
std::string trim(std::string value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
}

std::string value_text(std::string value) {
    value = trim(std::move(value));
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"') ||
                              (value.front() == '\'' && value.back() == '\'')))
        value = value.substr(1, value.size() - 2);
    return value;
}

std::string lower(std::string value) {
    for (char& ch : value) if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
    return value;
}

bool integer(const std::string& value, std::uint32_t max, std::uint32_t& out) {
    if (value.empty()) return false;
    out = 0;
    for (const char ch : value) {
        if (ch < '0' || ch > '9') return false;
        const auto digit = static_cast<std::uint32_t>(ch - '0');
        if (digit > max || out > (max - digit) / 10) return false;
        out = out * 10 + digit;
    }
    return true;
}

std::string shape_error(const Definition& def) {
    if (def.odf.empty() || odf_key(def.odf) != def.odf)
        return "squadron ODF must be a canonical basename";
    if (def.members.empty() || def.members.size() > kMaxMemberRows)
        return "a squadron needs 1 to 16 member rows";
    std::uint32_t total = 0;
    std::optional<std::uint32_t> previous;
    for (const auto& row : def.members) {
        if (row.row >= kMaxMemberRows || (previous && row.row <= *previous))
            return "member rows must be unique, ascending indices 0 through 15";
        if (row.odf.empty() || odf_key(row.odf) != row.odf)
            return "member ODF must be a canonical basename";
        if (row.odf == def.odf) return "a squadron cannot contain itself";
        if (row.count == 0 || row.count > kMaxMembers - total)
            return "member counts must be positive and total at most 32";
        total += row.count;
        previous = row.row;
    }
    return {};
}
} // namespace

std::string odf_key(std::string value) {
    value = lower(value_text(std::move(value)));
    if (value.size() > 4 && value.compare(value.size() - 4, 4, ".odf") == 0)
        value.resize(value.size() - 4);
    if (value.empty() || value.size() > 127) return {};
    for (char ch : value) {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') ||
              ch == '_' || ch == '-')) return {};
    }
    return value;
}

std::uint32_t Definition::size() const noexcept {
    std::uint32_t total = 0;
    for (const auto& row : members) {
        if (row.count > std::numeric_limits<std::uint32_t>::max() - total) return 0;
        total += row.count;
    }
    return total;
}

DefinitionResult parse_definition(const std::string& odf, const OdfFields& fields) {
    Definition def;
    def.odf = odf_key(odf);
    std::map<std::string, std::string> values;
    for (const auto& field : fields) {
        const auto name = lower(trim(field.first));
        if (name != "classlabel" && name.rfind("squad", 0) != 0) continue;
        if (!values.emplace(name, value_text(field.second)).second)
            return {{}, "duplicate effective field: " + name};
    }
    const auto label = values.find("classlabel");
    if (label == values.end() || lower(label->second) != "squadron")
        return {{}, "classLabel must be squadron"};

    struct RowInput { std::optional<std::string> odf, count; };
    std::map<std::uint32_t, RowInput> rows;
    for (const auto& field : values) {
        const auto& name = field.first;
        if (name == "classlabel") continue;
        if (name == "squadreinforceatyard") {
            if (field.second != "0" && field.second != "1")
                return {{}, "squadReinforceAtYard must be 0 or 1"};
            def.reinforce_at_yard = field.second == "1";
            continue;
        }
        const bool count = name.rfind("squadmembercount", 0) == 0;
        const std::string prefix = count ? "squadmembercount" : "squadmember";
        std::uint32_t index = 0;
        if (name.rfind(prefix, 0) != 0 ||
            !integer(name.substr(prefix.size()), kMaxMemberRows - 1, index) ||
            name.substr(prefix.size()) != std::to_string(index))
            return {{}, "unknown squadron field or invalid row index: " + name};
        if (count) rows[index].count = field.second;
        else rows[index].odf = field.second;
    }
    for (const auto& item : rows) {
        if (!item.second.odf || !item.second.count)
            return {{}, "member row " + std::to_string(item.first) +
                         " needs both squadMember and squadMemberCount"};
        std::uint32_t count = 0;
        if (!integer(*item.second.count, kMaxMembers, count) || count == 0)
            return {{}, "invalid count for member row " + std::to_string(item.first)};
        def.members.push_back({item.first, odf_key(*item.second.odf), count});
    }
    if (const auto error = shape_error(def); !error.empty()) return {{}, error};
    return {std::move(def), {}};
}

std::string validate_definition(const Definition& def, const ClassResolver& resolve) {
    if (const auto error = shape_error(def); !error.empty()) return error;
    if (!resolve) return "member class resolver is required";
    for (const auto& row : def.members) {
        switch (resolve(row.odf)) {
        case MemberClass::mobile_craft: break;
        case MemberClass::missing: return "missing member ODF: " + row.odf;
        case MemberClass::squadron: return "nested squadrons are unsupported: " + row.odf;
        default: return "member is not a supported mobile Craft: " + row.odf;
        }
    }
    return {};
}

std::string sum_build_economics(const Definition& def,
    const EconomicsResolver& resolve, BuildEconomics* output) {
    if (const auto error = shape_error(def); !error.empty()) return error;
    if (!resolve || !output) return "member economics resolver/output is required";
    BuildEconomics total;
    double seconds = 0;
    for (const auto& row : def.members) {
        const auto member = resolve(row.odf);
        if (!member) return "missing member economics: " + row.odf;
        if (!std::isfinite(member->seconds) || member->seconds < 0)
            return "invalid member build time: " + row.odf;
        seconds += static_cast<double>(member->seconds) * row.count;
        if (seconds > std::numeric_limits<float>::max()) return "squad build time overflow";
        for (std::size_t i = 0; i < total.costs.size(); ++i) {
            const std::int64_t cost = member->costs[i];
            if (cost < 0) return "negative member cost: " + row.odf;
            const auto sum = total.costs[i] + cost * row.count;
            if (sum > std::numeric_limits<std::int32_t>::max()) return "squad resource cost overflow";
            total.costs[i] = static_cast<std::int32_t>(sum);
        }
    }
    total.seconds = static_cast<float>(seconds);
    *output = total;
    return {};
}
} // namespace a2fo::squadrons
