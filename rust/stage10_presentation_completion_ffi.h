#pragma once
#include <cstddef>
#include <cstdint>
#include <exception>
#include <utility>
extern "C" {
using A2foStage10PresentationStep = std::uint64_t (*)(void*, std::uint32_t, std::uint64_t);
std::uint64_t a2fo_rs_stage10_presentation_flow(std::uint32_t, void*, A2foStage10PresentationStep, const std::uint8_t*, std::uint64_t);
void a2fo_rs_stage10_presentation_layer_alpha(std::uint32_t*, std::size_t);
}

// Native iterators and functors never cross the C ABI. Rust controls traversal;
// callbacks retain native dereference, increment, storage and exception scopes.
template<class Iterator, class Predicate, class Action>
std::pair<Iterator,std::size_t> a2fo_stage10_presentation_walk(
    Iterator begin, Iterator end, Predicate predicate, Action action, unsigned kind) {
    struct State {
        Iterator cursor, end;
        Predicate& predicate;
        Action& action;
        std::exception_ptr error{};
        std::uint8_t failed{};
    } state{begin,end,predicate,action};
    const auto step = +[](void* opaque, std::uint32_t id, std::uint64_t) noexcept -> std::uint64_t {
        auto& c = *static_cast<State*>(opaque);
        try {
            switch(id) {
            case 0: return c.cursor != c.end;
            case 1: return c.predicate(*c.cursor);
            case 2: c.action(*c.cursor); return 0;
            case 3: ++c.cursor; return 0;
            default: return 0;
            }
        } catch (...) { c.error=std::current_exception(); c.failed=1; return 0; }
    };
    const auto count = a2fo_rs_stage10_presentation_flow(kind,&state,step,&state.failed,0);
    if(state.error) std::rethrow_exception(state.error);
    return {state.cursor,static_cast<std::size_t>(count)};
}
template<class Iterator, class Predicate>
Iterator a2fo_stage10_presentation_find(Iterator begin, Iterator end, Predicate predicate) {
    return a2fo_stage10_presentation_walk(begin,end,predicate,[](auto&){},1).first;
}
template<class Iterator, class Action>
void a2fo_stage10_presentation_each(Iterator begin, Iterator end, Action action) {
    a2fo_stage10_presentation_walk(begin,end,[](const auto&){return true;},action,0);
}
template<class Iterator, class Predicate>
std::size_t a2fo_stage10_presentation_count(Iterator begin, Iterator end, Predicate predicate) {
    return a2fo_stage10_presentation_walk(begin,end,predicate,[](auto&){},2).second;
}
template<class Iterator, class Predicate>
Iterator a2fo_stage10_presentation_remove(Iterator begin, Iterator end, Predicate predicate) {
    struct State {
        Iterator cursor, end, result;
        Predicate& predicate;
        std::exception_ptr error{};
        std::uint8_t failed{};
    } state{begin,end,end,predicate};
    const auto step = +[](void* opaque, std::uint32_t id, std::uint64_t) noexcept -> std::uint64_t {
        auto& c=*static_cast<State*>(opaque);
        try {
            switch(id) {
            case 0:return c.cursor!=c.end;
            case 1:return c.predicate(*c.cursor);
            case 3:++c.cursor;return 0;
            case 4:c.result=c.cursor;return 0;
            case 5:*c.result=std::move(*c.cursor);return 0;
            case 6:++c.result;return 0;
            default:return 0;
            }
        }catch(...){c.error=std::current_exception();c.failed=1;return 0;}
    };
    a2fo_rs_stage10_presentation_flow(7,&state,step,&state.failed,0);
    if(state.error)std::rethrow_exception(state.error);
    return state.result;
}
