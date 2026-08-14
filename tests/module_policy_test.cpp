#include "../core/module_policy.hpp"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

const a2fo::ModulePolicyEntry* find_entry(
    const a2fo::ModulePolicy& policy, const std::string& name) {
    const std::string key = a2fo::module_name_key(name);
    for (const auto& entry : policy.entries) {
        if (entry.key == key) return &entry;
    }
    return nullptr;
}

a2fo::InstalledModule installed(const char* name) {
    a2fo::InstalledModule result;
    result.name = name;
    result.filename = std::string(name) + ".dll";
    result.path = std::string("Data\\modules\\") + result.filename;
    return result;
}

}  // namespace

int main() {
    const a2fo::ModuleRules parsed = a2fo::module_rules_from_info(
        "[mod]\r\nName=Example\r\n\r\n[modules]\r\n"
        "required0 = \"A2FOFireArcs.dll\"\r\n"
        "required4 = 'A2FOSwarmSystem'\r\n"
        "reject2 = \"A2FOCheats\"\r\n"
        "active0 = \"A2FOTurrets\"\r\n"
        "active9 = \"A2FOTurrets.dll\"\r\n");
    expect(parsed.has_section, "parses [modules]");
    expect(parsed.required.size() == 2, "parses sparse requiredX keys");
    expect(parsed.rejected.size() == 1, "parses rejectX");
    expect(parsed.active.size() == 1, "deduplicates active names");
    expect(parsed.required[0] == "A2FOFireArcs", "strips .dll suffix");

    const std::vector<a2fo::InstalledModule> inventory = {
        installed("A2FOCheats"), installed("A2FOFireArcs"),
        installed("A2FOSwarmSystem"), installed("A2FOTurrets"),
        installed("A2FORGBTextures")};

    const a2fo::ModulePolicy legacy =
        a2fo::evaluate_module_rules({a2fo::ModuleRules{}}, inventory);
    expect(!legacy.managed, "missing section is legacy mode");
    for (const auto& entry : legacy.entries) {
        expect(entry.state == a2fo::ModulePolicyState::active,
               "legacy mode activates every global module");
    }

    const a2fo::ModulePolicy managed =
        a2fo::evaluate_module_rules({parsed}, inventory);
    expect(managed.managed && managed.valid, "managed policy is valid");
    expect(find_entry(managed, "A2FOFireArcs")->state ==
               a2fo::ModulePolicyState::required,
           "required module is locked active");
    expect(find_entry(managed, "A2FOCheats")->state ==
               a2fo::ModulePolicyState::rejected,
           "rejected module is blocked");
    expect(find_entry(managed, "A2FOTurrets")->state ==
               a2fo::ModulePolicyState::active,
           "active module is selected");
    expect(find_entry(managed, "A2FORGBTextures")->state ==
               a2fo::ModulePolicyState::inactive,
           "unlisted module is inactive in managed mode");

    a2fo::ModuleRules parent;
    parent.has_section = true;
    parent.required = {"A2FOFireArcs"};
    parent.active = {"A2FOTurrets"};
    a2fo::ModuleRules child;
    child.has_section = true;
    child.rejected = {"A2FOCheats"};
    child.active = {"A2FORGBTextures"};
    const a2fo::ModulePolicy inherited =
        a2fo::evaluate_module_rules({parent, child}, inventory);
    expect(find_entry(inherited, "A2FOFireArcs")->state ==
               a2fo::ModulePolicyState::required,
           "parent requirement is inherited");
    expect(find_entry(inherited, "A2FOTurrets")->state ==
               a2fo::ModulePolicyState::inactive,
           "child active list replaces parent preference");
    expect(find_entry(inherited, "A2FORGBTextures")->state ==
               a2fo::ModulePolicyState::active,
           "child active selection wins");

    child.rejected.push_back("A2FOFireArcs");
    const a2fo::ModulePolicy conflict =
        a2fo::evaluate_module_rules({parent, child}, inventory);
    expect(!conflict.valid, "required/rejected conflict is invalid");
    expect(find_entry(conflict, "A2FOFireArcs")->state ==
               a2fo::ModulePolicyState::conflict,
           "conflict is represented explicitly");

    const std::string rewritten = a2fo::rewrite_active_modules(
        "[mod]\r\nName=Example\r\n\r\n[modules]\r\n"
        "required0 = \"A2FOFireArcs\"\r\n"
        "active7 = \"OldModule\"\r\n"
        "reject0 = \"A2FOCheats\"\r\n\r\n[other]\r\nValue=1\r\n",
        {"A2FOTurrets.dll", "A2FORGBTextures", "a2foturrets"});
    expect(rewritten.find("active7") == std::string::npos,
           "removes old activeX entries");
    expect(rewritten.find("required0 = \"A2FOFireArcs\"") !=
               std::string::npos,
           "preserves required entries");
    expect(rewritten.find("reject0 = \"A2FOCheats\"") !=
               std::string::npos,
           "preserves rejected entries");
    expect(rewritten.find("active0 = \"A2FOTurrets\"") !=
               std::string::npos,
           "writes normalized active0");
    expect(rewritten.find("active1 = \"A2FORGBTextures\"") !=
               std::string::npos,
           "writes deterministic active indices");
    expect(rewritten.find("[other]\r\nValue=1") != std::string::npos,
           "preserves unrelated sections and newline style");

    const std::string created = a2fo::rewrite_active_modules(
        "[mod]\nName=Legacy\n", {"A2FOSwarmSystem"});
    expect(created.find("[modules]\nactive0 = \"A2FOSwarmSystem\"") !=
               std::string::npos,
           "creates a modules section");

    std::cout << "module policy tests passed\n";
    return 0;
}
