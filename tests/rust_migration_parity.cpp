// Deterministic, byte-exact differential transcript for the frozen C++ policy
// and the Rust-backed adapters. Compile this same source against either set.
#include "module_policy.hpp"
#include "resupply_policy.hpp"
#include "builtin_state.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::uint32_t seed = 0x72a5d39bu;
std::uint32_t next() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
std::string hex(const std::string& bytes) {
    static const char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (unsigned char byte : bytes) { result += digits[byte >> 4]; result += digits[byte & 15]; }
    return result;
}
void number(std::string& out, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) out += static_cast<char>(value >> (i * 8));
}
void string(std::string& out, const std::string& value) {
    number(out, value.size()); out += value;
}
void strings(std::string& out, const std::vector<std::string>& values) {
    number(out, values.size()); for (const auto& value : values) string(out, value);
}
void emit(const std::string& label, const std::string& data) { std::cout << label << ' ' << hex(data) << '\n'; }
std::string random_name() {
    static const unsigned char alphabet[] = "aAbBcC01_-. /\\:\t\r\n\"\0\xff";
    std::string result;
    for (unsigned i = next() % 20; i; --i) result += static_cast<char>(alphabet[next() % (sizeof(alphabet) - 1)]);
    return result;
}
void rule(std::string& out, const a2fo::ModuleRules& value) {
    number(out, value.has_section); strings(out, value.required); strings(out, value.rejected);
    strings(out, value.active); strings(out, value.diagnostics);
}
void policy(std::string& out, const a2fo::ModulePolicy& value) {
    number(out, value.managed); number(out, value.valid); number(out, value.entries.size());
    for (const auto& entry : value.entries) {
        string(out, entry.key); string(out, entry.name); string(out, entry.filename);
        string(out, entry.path); number(out, static_cast<unsigned>(entry.state)); number(out, entry.installed);
    }
    strings(out, value.diagnostics);
}
void filter(std::string& out, const a2fo::energy_systems::ResupplyTargetFilter& value) {
    number(out, value.valid); number(out, value.default_allow);
    for (const auto* map : {&value.types, &value.odfs}) {
        std::vector<std::pair<std::string, bool>> entries(map->begin(), map->end());
        std::sort(entries.begin(), entries.end());
        number(out, entries.size());
        for (const auto& entry : entries) { string(out, entry.first); number(out, entry.second); }
    }
}
void module_cases() {
    const std::vector<std::string> names = {
        "A2FOEnergySystems", " A2FOEnergySystems.DLL ", "../bad.dll", "C:\\bad.dll",
        "\"A2FOEnergySystems\"", "A2FOInfoIni", "", "..", "x.dll.dll",
        std::string("A2FO\0Energy", 11), std::string("A2FO\xffEnergy", 11)};
    for (unsigned i = 0; i < 240; ++i) {
        const auto name = i < names.size() ? names[i] : random_name();
        std::string out; string(out, name); string(out, a2fo::normalize_module_name(name));
        string(out, a2fo::module_name_key(name)); emit("module_name/" + std::to_string(i), out);
    }
    const std::vector<std::string> snippets = {
        "", "[modules]\nactive1=A2FOEnergySystems\n", "[modules]\r\nrequired1=A2FOInfoIni\r\nreject2=A2FOWreckage\r\n",
        "[mod]\nname=Hello\n[modules]\nactive1=\"A2FOEnergySystems.dll\" ; comment\n",
        "[modules]\nactive1=A2FOInfoIni\nactive1=A2FOWreckage\nrequired0=../bad\n",
        "#comment\n[modules] # trailing\nactive1=A2FOInfoIni\n",
        "[modules]\nactive1=A2FOInfoIni\0bad\n", "[modules]\nactive1=A2FO\xffInfoIni\n",
        "[modules]\nactive1=A2FOInfoIni\n[other]\nactive1=A2FOWreckage\n"};
    std::vector<a2fo::ModuleRules> rules;
    for (unsigned i = 0; i < 140; ++i) {
        std::string source = snippets[i % snippets.size()];
        if (i >= snippets.size()) {
            source += (i % 2 ? "\r\n" : "\n");
            source += (i % 3 ? "[modules]\n" : "[mod]\n");
            for (unsigned j = 0; j < i % 6; ++j) source +=
                std::string(j % 2 ? "required" : "active") + std::to_string(j) + "=" + random_name() + (j % 3 ? "\n" : " ;comment\r\n");
        }
        auto parsed = a2fo::module_rules_from_info(source);
        std::string out; string(out, source); rule(out, parsed);
        emit("module_parse/" + std::to_string(i), out);
        rules.push_back(std::move(parsed));
        const auto rewritten = a2fo::rewrite_active_modules(source,
            {"A2FOInfoIni", "A2FOEnergySystems", i % 2 ? "a2foinfoini.dll" : "../bad"});
        emit("module_rewrite/" + std::to_string(i), rewritten);
    }
    const std::vector<a2fo::InstalledModule> installed = {
        {"A2FOInfoIni", "A2FOInfoIni.dll", "Data/modules/A2FOInfoIni.dll"},
        {"A2FOEnergySystems", "A2FOEnergySystems.dll", "Mod/modules/A2FOEnergySystems.dll"},
        {"A2FOWreckage", "A2FOWreckage.dll", "Data/modules/A2FOWreckage.dll"},
        {"a2foinfoini", "a2foinfoini.DLL", "Parent/modules/a2foinfoini.DLL"}};
    for (unsigned i = 0; i < 120; ++i) {
        std::vector<a2fo::ModuleRules> chain;
        if (i % 7) chain.push_back(rules[(i * 3) % rules.size()]);
        if (i % 5) chain.push_back(rules[(i * 5 + 1) % rules.size()]);
        if (i % 3) chain.push_back(rules[(i * 7 + 2) % rules.size()]);
        std::vector<a2fo::InstalledModule> inventory = installed;
        if (i % 2) inventory.pop_back();
        if (i % 4 == 0) inventory.clear();
        std::string out; policy(out, a2fo::evaluate_module_rules(chain, inventory));
        emit("module_evaluate/" + std::to_string(i), out);
    }
    for (unsigned i = 0; i < 4; ++i) {
        a2fo::ModuleRules parent, child;
        parent.has_section = child.has_section = true;
        parent.required = {"A2FOInfoIni"};
        parent.rejected = {i % 2 ? "A2FOWreckage" : "A2FOInfoIni"};
        parent.active = {"A2FOWreckage"};
        child.active = {i % 2 ? "A2FOEnergySystems" : "A2FOInfoIni"};
        std::string out; policy(out, a2fo::evaluate_module_rules({parent, child},
            i < 2 ? installed : std::vector<a2fo::InstalledModule>{}));
        emit("module_conflict/" + std::to_string(i), out);
    }
}
void resupply_cases() {
    using namespace a2fo::energy_systems;
    {
        ResupplyTargetFilter omitted;
        std::string out; filter(out, omitted);
        number(out, resupply_target_allowed(omitted, "", "", false, false));
        number(out, resupply_target_allowed(omitted, "", "", true, false));
        number(out, resupply_target_allowed(omitted, "", "", true, true));
        emit("resupply_omitted", out);
    }
    const std::vector<std::string> values = {"", " \t\"FiGhTeR\" \r\n", "mods/ships/Fed_Scout.ODF",
        "C:\\Fleet\\Fed_Scout.ODF", "./", "..", "x.odf", ".odf", "a..odf",
        "foo bar", "\"foo\"bar\"", std::string("f\0oo", 4), std::string("f\xffoo", 4)};
    for (unsigned i = 0; i < 160; ++i) {
        const auto raw = i < values.size() ? values[i] : random_name();
        std::string out; string(out, raw); string(out, normalize_resupply_type(raw));
        string(out, normalize_resupply_odf(raw)); emit("resupply_normalize/" + std::to_string(i), out);
    }
    std::vector<std::string> inputs = {"", " \"   \" ", "* !type:bomber odf:fed_bomber !odf:fed_scout.odf",
        "type:fighter !odf:fed_fighter !type:bomber odf:fed_bomber", "TYPE:Fighter type:fighter ODF:Fed_Scout.odf odf:fed_scout",
        "type:fighter !type:FIGHTER", "odf:fed_scout !ODF:fed_scout.odf", "!", "!*", "fighter", "type:",
        "odf:bad/name.odf", "type:foo:bar", "\"type:fighter", "type:fighter\"", "type:\"fighter\"",
        "type:fighter \"odf:fed_scout\"", std::string("type:fighter\0 !type:fighter", 27), "type:f\xffoo"};
    std::string boundary = "type:fighter"; boundary.resize(4094, ' '); inputs.push_back(boundary);
    boundary += ' '; inputs.push_back(boundary);
    inputs.emplace_back(); for (unsigned i = 0; i < 257; ++i) inputs.back() += "type:fighter ";
    for (unsigned i = 0; i < 180; ++i) {
        std::string raw = i < inputs.size() ? inputs[i] : (i % 3 ? "type:" : "!odf:") + random_name();
        if (i >= inputs.size() && i % 7 == 0) raw = "* type:fighter !odf:fed_scout.odf "+random_name();
        auto parsed = parse_resupply_targets(raw);
        std::string out; string(out, raw); filter(out, parsed);
        for (const std::string& type : {"", "fighter", "bomber", "f\xffoo"}) {
            for (const std::string& odf : {"", "fed_scout", "fed_bomber", "fed_fighter"}) {
                for (int same = 0; same < 2; ++same) for (int self = 0; self < 2; ++self)
                    number(out, resupply_target_allowed(parsed, type, odf, same, self));
            }
        }
        emit("resupply_parse/" + std::to_string(i), out);
    }
}
void builtin_cases() {
    using namespace a2fo;
    for (int i = 0; i <= static_cast<int>(BuiltinFeature::count); ++i) {
        const auto feature = static_cast<BuiltinFeature>(i);
        std::string out; number(out, builtin_requires_restart(feature));
        const char* name = canonical_builtin_name(feature);
        string(out, name ? std::string(name) : std::string());
        if (name) {
            for (const auto& candidate : {std::string(name), std::string(name) + ".DLL", std::string("../") + name}) {
                BuiltinFeature found = BuiltinFeature::count;
                number(out, find_builtin_feature(candidate, found)); number(out, static_cast<unsigned>(found));
            }
        }
        emit("builtin_identity/" + std::to_string(i), out);
    }
    for (unsigned i = 0; i < 180; ++i) {
        auto name = random_name(); BuiltinFeature found = BuiltinFeature::count;
        std::string out; string(out, name); number(out, find_builtin_feature(name, found));
        number(out, static_cast<unsigned>(found)); emit("builtin_find/" + std::to_string(i), out);
    }
    const std::vector<std::vector<std::string>> roots = {{}, {"/"}, {"/", "C:\\Fleet Ops\\Data"},
        {"C:\\Fleet Ops\\Data", "C:\\Fleet Ops\\Data\\Mods\\Parent"}, {"", "x"}, {"a/b/", "x\\y"}};
    for (unsigned i = 0; i < 120; ++i) {
        auto active = roots[i % roots.size()]; auto selected = roots[(i / 2) % roots.size()];
        if (i % 4 == 0) selected = active;
        if (i % 3 == 0 && !selected.empty()) selected.back() += "/";
        if (i % 7 == 0 && !selected.empty()) selected.front() = random_name();
        std::string out; strings(out, active); strings(out, selected);
        number(out, same_module_roots(active, selected)); emit("builtin_roots/" + std::to_string(i), out);
    }
    for (unsigned i = 0; i < 64; ++i) {
        BuiltinState state;
        std::string out;
        auto snapshot = [&] { number(out, state.is_prepared()); number(out, state.available());
            number(out, state.requested()); number(out, state.enabled()); number(out, state.activation()); };
        snapshot(); number(out, state.request(i % 2)); snapshot();
        state.prepared(i % 3 != 0, i % 5 == 0); snapshot();
        number(out, state.request(true)); snapshot();
        number(out, state.request(true)); snapshot(); // no-op generation
        number(out, state.request(false)); snapshot();
        number(out, state.request(true)); snapshot(); // off/on generation
        for (unsigned j = 0; j < 32; ++j) {
            const bool enabled = (next() & 3) != 0;
            number(out, state.request(enabled)); snapshot();
            if (j % 9 == 0) { state.prepared(j % 2 == 0, j % 3 == 0); snapshot(); }
        }
        emit("builtin_state/" + std::to_string(i), out);
    }
}
} // namespace
int main() { module_cases(); resupply_cases(); builtin_cases(); }
