#pragma once
#include "light_event.hpp"
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

namespace a2fo::native11 {
struct LightClassRecord {
    std::uint64_t class_id=0; // Opaque identity, never dereferenced.
    std::string source_odf;
    LightConfig config{};
    std::uint64_t version=0;
};

// Externally serialized: callers serialize update/find/clear themselves.
// The logical budget charges record/entry sizes plus owned ODF text; allocator
// overhead is not part of the 2 MiB accounting. Faulted IDs count toward size.
class LightClassStore {
public:
    bool update(const A2FO_GameObjectClassLoadedEvent& event,std::string& error) noexcept;
    std::shared_ptr<const LightClassRecord> find(std::uint64_t class_id) const noexcept;
    bool healthy() const noexcept { return !global_fault_&&fault_count_==0; }
    std::size_t size() const noexcept { return entries_.size(); }
    void clear() noexcept;
private:
    struct Entry {
        std::shared_ptr<const LightClassRecord> record;
        bool faulted=false;
    };
    static std::size_t cost(const Entry& entry) noexcept;
    void fault(std::uint64_t class_id) noexcept;
    std::map<std::uint64_t,Entry> entries_;
    std::size_t logical_bytes_=0;
    std::size_t fault_count_=0;
    std::uint64_t next_version_=1;
    bool global_fault_=false;
};
} // namespace a2fo::native11
