#pragma once

// Pure MODULE technology-tree policy. Native callers supply completed-object
// counts and team relationships; this file has no Windows or game dependency.
#include "../../rust/stage8_c_buildyard_ffi.h"
#include <algorithm>
#include <exception>
#include <cctype>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace a2fo::module_technology_policy {

struct Object { std::string odf; bool must_not_have = false; };
struct Peer {
    bool ally = true;
    std::string race;
    std::vector<Object> objects;
};
struct Requirement {
    std::string race;
    std::vector<Object> objects;
    std::vector<Peer> peers;
    std::vector<std::int32_t> logic_groups;
};
struct Definition {
    std::string key;
    std::string source;
    std::vector<Requirement> requirements;
    bool unsupported = false;
    std::string reason;
};
struct ParseResult {
    std::vector<Definition> definitions;
    std::string error;
    explicit operator bool() const { return error.empty(); }
};
struct Callbacks {
    std::function<int(std::string_view, int)> object_count;
    std::function<bool(std::string_view, int)> race_matches;
    std::function<bool(int, int, bool)> eligible_peer;
    std::function<std::string(std::string_view)> display_name;
};
struct Evaluation { bool available = false; std::string missing_text; };



inline ParseResult parse_document(std::string_view xml, std::string source) {
    ParseResult result;
    struct Context {
        ParseResult& result;
        const std::string& source;
        Definition definition{};
        Requirement requirement{};
        Peer peer{};
        std::exception_ptr error{};
    } context{result, source};
    const auto classify = +[](std::uint8_t byte,
                              std::uint32_t kind) noexcept -> bool {
        switch (kind) {
        case 0: return std::isspace(byte) != 0;
        case 1: return std::isalpha(byte) != 0;
        case 2: return std::isalnum(byte) != 0;
        default: return false;
        }
    };
    const auto emit = +[](void* opaque,
        const A2foStage8BuildyardEvent* event) noexcept -> bool {
        auto& c = *static_cast<Context*>(opaque);
        try {
            const std::string_view value(
                reinterpret_cast<const char*>(event->a.data), event->a.size);
            switch (event->op) {
            case 0: c.definition = Definition{};
                c.definition.key = value;
                c.definition.source = c.source;
                return true;
            case 1: c.definition.unsupported = true;
                if (c.definition.reason.empty()) c.definition.reason = value;
                return true;
            case 2: c.requirement = Requirement{};
                c.requirement.race = value;
                return true;
            case 3: c.requirement.logic_groups.push_back(event->value);
                return true;
            case 4: c.requirement.objects.push_back(
                Object{std::string(value), event->value != 0});
                return true;
            case 5: c.peer = Peer{};
                c.peer.ally = event->value != 0;
                c.peer.race = value;
                return true;
            case 6: c.peer.objects.push_back(
                Object{std::string(value), event->value != 0});
                return true;
            case 7: c.requirement.peers.push_back(std::move(c.peer));
                return true;
            case 8: c.definition.requirements.push_back(
                std::move(c.requirement));
                return true;
            case 9: c.result.definitions.push_back(std::move(c.definition));
                return true;
            case 10: c.result.error = value;
                return true;
            default: return false;
            }
        } catch (...) {
            c.error = std::current_exception();
            return false;
        }
    };
    a2fo_rs_c_buildyard_parse(
        reinterpret_cast<const std::uint8_t*>(xml.data()), xml.size(),
        classify, &context, emit);
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

// Same-key XML declarations append requirement nodes; a declaration with no
// requirements contributes no nodes to the existing native requirement list.
inline bool merge_definitions(Definition* existing, const Definition& incoming) {
    if (!existing) return false;
    const auto plan = a2fo_rs_c_buildyard_merge(
        {reinterpret_cast<const std::uint8_t*>(existing->key.data()),
         existing->key.size()},
        {reinterpret_cast<const std::uint8_t*>(incoming.key.data()),
         incoming.key.size()},
        incoming.unsupported, existing->reason.empty());
    if (plan == 0) return false;
    existing->requirements.insert(existing->requirements.end(),
                                  incoming.requirements.begin(),
                                  incoming.requirements.end());
    if (plan >= 2) {
        existing->unsupported = true;
        if (plan == 3) existing->reason = incoming.reason;
    }
    return true;
}

inline Evaluation evaluate(const Definition& definition, int owner,
                           const Callbacks& callbacks) {
    Evaluation result{};
    struct Context {
        const Definition& definition;
        const Callbacks& callbacks;
        Evaluation& result;
        std::string display{};
        std::exception_ptr error{};
    } context{definition, callbacks, result};
    const auto classify = +[](std::uint8_t byte,
                              std::uint32_t kind) noexcept -> bool {
        switch (kind) {
        case 0: return std::isspace(byte) != 0;
        case 1: return std::isalpha(byte) != 0;
        case 2: return std::isalnum(byte) != 0;
        default: return false;
        }
    };
    const auto step = +[](void* opaque,
        A2foStage8BuildyardEvalRequest* request) noexcept -> std::int64_t {
        auto& c = *static_cast<Context*>(opaque);
        auto view = [&](const std::string& text) {
            request->text = {
                reinterpret_cast<const std::uint8_t*>(text.data()),
                text.size()};
        };
        try {
            const auto op = request->op;
            if (op == 0) return c.definition.unsupported;
            if (op == 1) return static_cast<bool>(c.callbacks.object_count) &&
                static_cast<bool>(c.callbacks.race_matches) &&
                static_cast<bool>(c.callbacks.eligible_peer);
            if (op == 2) return c.definition.requirements.size();
            const auto& requirement =
                c.definition.requirements[request->requirement];
            if (op == 3) { view(requirement.race); return 0; }
            if (op == 4) return requirement.objects.size();
            if (op == 6) return requirement.peers.size();
            if (op == 11) return requirement.logic_groups.size();
            if (op == 12) return requirement.logic_groups[request->object];
            if (op == 7) return requirement.peers[request->peer].ally;
            if (op == 8) {
                view(requirement.peers[request->peer].race); return 0;
            }
            if (op == 9) return requirement.peers[request->peer].objects.size();
            if (op == 21) {
                const std::string& race = request->peer == SIZE_MAX
                    ? requirement.race : requirement.peers[request->peer].race;
                return c.callbacks.race_matches(race, request->team);
            }
            if (op == 22) return c.callbacks.eligible_peer(
                request->team, request->other, request->ally);
            const auto& object = request->peer == SIZE_MAX
                ? requirement.objects[request->object]
                : requirement.peers[request->peer].objects[request->object];
            if (op == 5 || op == 10) {
                view(object.odf); return object.must_not_have;
            }
            if (op == 20) return c.callbacks.object_count(
                object.odf, request->team);
            if (op == 23) {
                c.display = c.callbacks.display_name
                    ? c.callbacks.display_name(object.odf) : std::string{};
                view(c.display);
                return 0;
            }
            return 0;
        } catch (...) {
            c.error = std::current_exception();
            return INT64_MIN;
        }
    };
    const auto done = +[](void* opaque, bool available,
        const std::uint8_t* text, std::size_t size) noexcept -> bool {
        auto& c = *static_cast<Context*>(opaque);
        try {
            c.result.available = available;
            c.result.missing_text.assign(
                reinterpret_cast<const char*>(text), size);
            return true;
        } catch (...) {
            c.error = std::current_exception();
            return false;
        }
    };
    a2fo_rs_c_buildyard_evaluate(&context, owner, classify, step, done);
    if (context.error) std::rethrow_exception(context.error);
    return result;
}

} // namespace a2fo::module_technology_policy
