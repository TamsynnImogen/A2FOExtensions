#pragma once
namespace a2fo {
// The class dispatcher restores TLS even when its native operation callback
// records an exception and the Rust coordinator stops before the restore step.
template<class T> class RestoreClassLoadContext {
public:
    explicit RestoreClassLoadContext(T*& target) noexcept:target_(target),previous_(target) {}
    ~RestoreClassLoadContext() { target_=previous_; }
    RestoreClassLoadContext(const RestoreClassLoadContext&)=delete;
    RestoreClassLoadContext& operator=(const RestoreClassLoadContext&)=delete;
private:
    T*& target_;
    T* previous_;
};
}
