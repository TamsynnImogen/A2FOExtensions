#pragma once
#include <cstddef>

namespace mmm {
struct rule {};
struct hot_int { void dependent(rule*) {} };
struct IIIE { rule* create(int (*)(), void (*)(), const char*) { return nullptr; }
    void affected(rule*) {} };
struct character { IIIE& iiie(); };

// Compile-only types for the deliberately uncalled legacy SetupReload body.
template<class T> T memory_function(std::size_t) { return T{}; }
}
