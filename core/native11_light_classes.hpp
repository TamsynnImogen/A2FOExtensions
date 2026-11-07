#pragma once
#include "../renderer/native11/light_class_store.hpp"
#include <string>
#include <vector>
namespace a2fo::native11_light_classes {
using Log=void(*)(const char*);
// Owned by one factory invocation, from before native lookup/construction until
// every copied-ODF callback has returned. Class pins are unavailable while any
// invocation is active. Exceptions/failed callbacks keep the source faulted
// until explicit reconfiguration; this is not a native subclass/type proof.
class ClassLoadScope {
public:
    ClassLoadScope() noexcept;
    ~ClassLoadScope();
    ClassLoadScope(const ClassLoadScope&)=delete;
    ClassLoadScope& operator=(const ClassLoadScope&)=delete;
    ClassLoadScope(ClassLoadScope&&)=delete;
    ClassLoadScope& operator=(ClassLoadScope&&)=delete;
    bool observe_class(void*,const std::string&,const std::map<std::string,std::string>&) noexcept;
    bool complete() noexcept;
    void fail() noexcept;
private:
    std::uint64_t ticket_=0;
    std::uint32_t owner_=0;
    bool counted_=false,observed_=false,completed_=false,failed_=false;
};
// Startup configuration only. The native factory uses ClassLoadScope's
// observer. Standalone copied events are accepted only outside every scope.
void configure_from_roots(const std::vector<std::string>&,Log) noexcept;
void configure(bool requested,Log) noexcept;
void dispatch_available(bool) noexcept;
bool requested() noexcept;
bool healthy() noexcept;
std::uint64_t generation() noexcept;
bool observe_class(void*,const std::string&,const std::map<std::string,std::string>&) noexcept;
std::shared_ptr<const native11::LightClassRecord> pin(std::uint64_t) noexcept;
void disable() noexcept;
}
