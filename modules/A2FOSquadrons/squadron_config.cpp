#include "squadron_config.hpp"
#include "../../rust/gameplay_ffi.h"

#include <exception>
#include <vector>

namespace a2fo::squadrons {
namespace {
A2foRsBytes bytes(const std::string &value) noexcept {
    return {reinterpret_cast<const std::uint8_t *>(value.data()), value.size()};
}
std::vector<A2foRsMember> members(const Definition &definition) {
    std::vector<A2foRsMember> result;
    result.reserve(definition.members.size());
    for (const auto &row : definition.members)
        result.push_back({row.row, row.count, bytes(row.odf)});
    return result;
}
struct Context {
    Definition definition;
    std::string text;
    std::exception_ptr failure;
    const ClassResolver *class_resolver = nullptr;
    const EconomicsResolver *economics_resolver = nullptr;
};
void set_text(void *raw, const std::uint8_t *text, std::size_t length) noexcept {
    auto &context = *static_cast<Context *>(raw);
    if (context.failure)
        return;
    try {
        context.text.assign(reinterpret_cast<const char *>(text), length);
    } catch (...) {
        context.failure = std::current_exception();
    }
}
void set_odf(void *raw, const std::uint8_t *text, std::size_t length) noexcept {
    auto &context = *static_cast<Context *>(raw);
    if (context.failure)
        return;
    try {
        context.definition.odf.assign(reinterpret_cast<const char *>(text), length);
    } catch (...) {
        context.failure = std::current_exception();
    }
}
void add_member(void *raw, std::uint32_t row, const std::uint8_t *odf, std::size_t length,
                std::uint32_t count) noexcept {
    auto &context = *static_cast<Context *>(raw);
    if (context.failure)
        return;
    try {
        context.definition.members.push_back(
            {row, std::string(reinterpret_cast<const char *>(odf), length), count});
    } catch (...) {
        context.failure = std::current_exception();
    }
}
std::uint32_t resolve_class(void *raw, const std::uint8_t *odf, std::size_t length) noexcept {
    auto &context = *static_cast<Context *>(raw);
    if (context.failure)
        return 2;
    try {
        return static_cast<std::uint32_t>(
            (*context.class_resolver)(std::string(reinterpret_cast<const char *>(odf), length)));
    } catch (...) {
        context.failure = std::current_exception();
        return 2;
    }
}
std::uint8_t resolve_economics(void *raw, const std::uint8_t *odf, std::size_t length,
                               A2foRsEconomics *output) noexcept {
    auto &context = *static_cast<Context *>(raw);
    if (context.failure)
        return 0;
    try {
        const auto value =
            (*context.economics_resolver)(std::string(reinterpret_cast<const char *>(odf), length));
        if (!value)
            return 0;
        for (std::size_t i = 0; i < 10; ++i)
            output->costs[i] = value->costs[i];
        output->seconds = value->seconds;
        return 1;
    } catch (...) {
        context.failure = std::current_exception();
        return 0;
    }
}
void rethrow(const Context &context) {
    if (context.failure)
        std::rethrow_exception(context.failure);
}
} // namespace

std::string odf_key(std::string value) {
    Context context;
    a2fo_rs_gameplay_odf_key(bytes(value), &context, set_text);
    rethrow(context);
    return context.text;
}

std::uint32_t Definition::size() const noexcept {
    std::uint32_t total = 0;
    std::uint8_t overflow = 0;
    for (const auto &row : members) {
        total = a2fo_rs_gameplay_definition_size_add(total, row.count, &overflow);
        if (overflow)
            return 0;
    }
    return total;
}

DefinitionResult parse_definition(const std::string &odf, const OdfFields &fields) {
    std::vector<A2foRsField> native;
    native.reserve(fields.size());
    for (const auto &field : fields)
        native.push_back({bytes(field.first), bytes(field.second)});
    Context context;
    std::uint8_t reinforce = 1;
    a2fo_rs_gameplay_parse_squadron(bytes(odf), native.data(), native.size(), &context, set_odf,
                                    add_member, &reinforce, set_text);
    rethrow(context);
    if (!context.text.empty())
        return {{}, std::move(context.text)};
    context.definition.reinforce_at_yard = reinforce != 0;
    return {std::move(context.definition), {}};
}

std::string validate_definition(const Definition &definition, const ClassResolver &resolve) {
    const auto native = members(definition);
    Context context;
    context.class_resolver = &resolve;
    a2fo_rs_gameplay_validate_squadron(bytes(definition.odf), native.data(), native.size(),
                                       &context, resolve ? resolve_class : nullptr, set_text);
    rethrow(context);
    return context.text;
}

std::string sum_build_economics(const Definition &definition, const EconomicsResolver &resolve,
                                BuildEconomics *output) {
    const auto native = members(definition);
    Context context;
    context.economics_resolver = &resolve;
    A2foRsEconomics result{};
    a2fo_rs_gameplay_economics_squadron(bytes(definition.odf), native.data(), native.size(),
                                        &context, resolve ? resolve_economics : nullptr,
                                        output ? &result : nullptr, set_text);
    rethrow(context);
    if (!context.text.empty())
        return context.text;
    for (std::size_t i = 0; i < 10; ++i)
        output->costs[i] = result.costs[i];
    output->seconds = result.seconds;
    return {};
}
} // namespace a2fo::squadrons
