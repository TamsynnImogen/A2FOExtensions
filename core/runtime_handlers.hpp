#pragma once
#include "../rust/runtime_handlers_ffi.h"
#include <cstddef>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace a2fo {
struct TextSpan {
    const char* data = nullptr;
    std::size_t size = 0;
    friend std::string operator+(const char* left, TextSpan right) {
        std::string result(left);
        result.append(right.data, right.size);
        return result;
    }
    friend std::string operator+(std::string left, TextSpan right) {
        left.append(right.data, right.size);
        return left;
    }
};
namespace detail {
template<class T, class = void> struct HasRequiredFields : std::false_type {};
template<class T> struct HasRequiredFields<T, std::void_t<decltype(std::declval<T>().required_odf_fields)>> : std::true_type {};
template<class T, class = void> struct HasEnabled : std::false_type {};
template<class T> struct HasEnabled<T, std::void_t<decltype(std::declval<T>().enabled)>> : std::true_type {};
template<class Pointer> uintptr_t pointer_bits(Pointer value) {
    static_assert(sizeof(Pointer) == sizeof(uintptr_t), "callback pointer width must match uintptr_t");
    uintptr_t bits{};
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}
template<class Pointer> Pointer from_pointer_bits(uintptr_t bits) {
    static_assert(sizeof(Pointer) == sizeof(uintptr_t), "callback pointer width must match uintptr_t");
    Pointer value{};
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
} // namespace detail

// The only persistent registration storage is the opaque Rust allocation.
// get() projects a native copy. borrow() and iterator dereference retain a
// stable Rust record without allocating native strings or vectors.
template<class Registration> class RustHandlerRegistry {
    void* store_ = nullptr;
    void ensure_store() {
        if (!store_) {
            store_ = a2fo_rs_runtime_handlers_new();
            if (!store_) throw std::bad_alloc();
        }
    }
public:
    struct Borrowed {
        decltype(std::declval<Registration>().handler) handler{};
        decltype(std::declval<Registration>().user_data) user_data{};
        bool enabled = true;
        TextSpan owner{};
        std::size_t field_count = 0;
        void* lease = nullptr;
        Borrowed() = default;
        explicit Borrowed(const A2foRsHandlerView& view) noexcept
            : handler(detail::from_pointer_bits<decltype(handler)>(view.handler)),
              user_data(detail::from_pointer_bits<decltype(user_data)>(view.user_data)),
              enabled(view.enabled != 0),
              owner{reinterpret_cast<const char*>(view.owner), view.owner_size},
              field_count(view.field_count), lease(view.lease) {}
        Borrowed(const Borrowed& other)
            : handler(other.handler), user_data(other.user_data), enabled(other.enabled),
              owner(other.owner), field_count(other.field_count), lease(other.lease) {
            if (lease && !a2fo_rs_runtime_handlers_retain(lease)) throw std::bad_alloc();
        }
        Borrowed(Borrowed&& other) noexcept
            : handler(other.handler), user_data(other.user_data), enabled(other.enabled),
              owner(other.owner), field_count(other.field_count), lease(std::exchange(other.lease, nullptr)) {}
        ~Borrowed() { a2fo_rs_runtime_handlers_release(lease); }
        Borrowed& operator=(const Borrowed& other) {
            if (this != &other) { Borrowed copy(other); swap(copy); }
            return *this;
        }
        Borrowed& operator=(Borrowed&& other) noexcept {
            if (this != &other) { Borrowed moved(std::move(other)); swap(moved); }
            return *this;
        }
        void swap(Borrowed& other) noexcept {
            std::swap(handler, other.handler); std::swap(user_data, other.user_data);
            std::swap(enabled, other.enabled); std::swap(owner, other.owner);
            std::swap(field_count, other.field_count); std::swap(lease, other.lease);
        }
    };
    RustHandlerRegistry() = default;
    RustHandlerRegistry(std::initializer_list<Registration> values) {
        RustHandlerRegistry complete;
        for (const auto& value : values) complete.push_back(value);
        swap(complete);
    }
    RustHandlerRegistry(const RustHandlerRegistry& other) {
        if (other.store_) {
            store_ = a2fo_rs_runtime_handlers_clone(other.store_);
            if (!store_) throw std::bad_alloc();
        }
    }
    RustHandlerRegistry(RustHandlerRegistry&& other) noexcept : store_(std::exchange(other.store_, nullptr)) {}
    ~RustHandlerRegistry() { a2fo_rs_runtime_handlers_drop(store_); }
    RustHandlerRegistry& operator=(const RustHandlerRegistry& other) {
        if (this != &other) { RustHandlerRegistry copy(other); swap(copy); }
        return *this;
    }
    RustHandlerRegistry& operator=(RustHandlerRegistry&& other) noexcept {
        if (this != &other) {
            a2fo_rs_runtime_handlers_drop(store_);
            store_ = std::exchange(other.store_, nullptr);
        }
        return *this;
    }
    void swap(RustHandlerRegistry& other) noexcept { std::swap(store_, other.store_); }
    std::size_t size() const noexcept { return a2fo_rs_runtime_handlers_len(store_); }
    bool empty() const noexcept { return size() == 0; }
    void clear() noexcept { a2fo_rs_runtime_handlers_clear(store_); }
    void push_back(const Registration& value) {
        std::vector<A2foRsHandlerFieldView> fields;
        if constexpr (detail::HasRequiredFields<Registration>::value) {
            fields.reserve(value.required_odf_fields.size());
            for (const auto& field : value.required_odf_fields)
                fields.push_back({reinterpret_cast<const uint8_t*>(field.data()), field.size()});
        }
        ensure_store();
        const uint8_t enabled = [&] { if constexpr (detail::HasEnabled<Registration>::value) return uint8_t(value.enabled); else return uint8_t(1); }();
        if (!a2fo_rs_runtime_handlers_push(store_, reinterpret_cast<const uint8_t*>(value.owner.data()),
                value.owner.size(), fields.data(), fields.size(), detail::pointer_bits(value.handler),
                detail::pointer_bits(value.user_data), enabled)) throw std::bad_alloc();
    }
    void push_back(Registration&& value) { push_back(value); }
    Borrowed borrow(std::size_t index) const {
        A2foRsHandlerView view{};
        if (!a2fo_rs_runtime_handlers_get(store_, index, &view)) throw std::out_of_range("handler index");
        return Borrowed(view);
    }
    Registration get(std::size_t index) const {
        auto borrowed = borrow(index);
        Registration value{};
        value.owner.assign(borrowed.owner.data, borrowed.owner.size);
        value.handler = borrowed.handler;
        value.user_data = borrowed.user_data;
        if constexpr (detail::HasEnabled<Registration>::value) value.enabled = borrowed.enabled;
        if constexpr (detail::HasRequiredFields<Registration>::value) {
            value.required_odf_fields.reserve(borrowed.field_count);
            for (std::size_t i = 0; i < borrowed.field_count; ++i) {
                A2foRsHandlerFieldView field{};
                if (!a2fo_rs_runtime_handlers_get_field(borrowed.lease, i, &field)) throw std::out_of_range("field index");
                value.required_odf_fields.emplace_back(reinterpret_cast<const char*>(field.bytes), field.size);
            }
        }
        return value;
    }
    Registration operator[](std::size_t index) const { return get(index); }
    void set_enabled(std::size_t index, bool enabled) {
        if (!a2fo_rs_runtime_handlers_set_enabled(store_, index, enabled ? 1 : 0))
            throw std::out_of_range("handler index");
    }
    void set_handler(std::size_t index, decltype(std::declval<Registration>().handler) handler) {
        if (!a2fo_rs_runtime_handlers_set_handler(store_, index, detail::pointer_bits(handler)))
            throw std::out_of_range("handler index");
    }
    class const_iterator {
        const RustHandlerRegistry* registry_ = nullptr;
        std::size_t index_ = 0;
        mutable std::optional<Borrowed> cache_{};
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Registration;
        using difference_type = std::ptrdiff_t;
        using pointer = const Borrowed*;
        using reference = const Borrowed&;
        const_iterator() = default;
        const_iterator(const RustHandlerRegistry* registry, std::size_t index) : registry_(registry), index_(index) {}
        reference operator*() const { if (!cache_) cache_.emplace(registry_->borrow(index_)); return *cache_; }
        pointer operator->() const { return &operator*(); }
        const_iterator& operator++() { ++index_; cache_.reset(); return *this; }
        const_iterator operator++(int) { auto old = *this; ++*this; return old; }
        friend bool operator==(const const_iterator& a, const const_iterator& b) { return a.registry_ == b.registry_ && a.index_ == b.index_; }
        friend bool operator!=(const const_iterator& a, const const_iterator& b) { return !(a == b); }
    };
    const_iterator begin() const { return {this, 0}; }
    const_iterator end() const { return {this, size()}; }
    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }
};
} // namespace a2fo
