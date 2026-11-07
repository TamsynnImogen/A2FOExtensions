#include "light_class_store.hpp"
#include <limits>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_classes=4096;
constexpr std::size_t max_source_odf=259;
constexpr std::size_t max_logical_bytes=2*1024*1024;
std::uint64_t class_id(const void* p) noexcept {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(p));
}
}

std::size_t LightClassStore::cost(const Entry& entry) noexcept {
    return sizeof(std::uint64_t)+sizeof(Entry)+(entry.record ? sizeof(LightClassRecord)+entry.record->source_odf.size() : 0);
}
void LightClassStore::fault(std::uint64_t id) noexcept {
    auto it=entries_.find(id);
    if (it!=entries_.end()) {
        if (!it->second.faulted) ++fault_count_;
        logical_bytes_-=cost(it->second);
        it->second.record.reset();
        it->second.faulted=true;
        logical_bytes_+=cost(it->second);
        return;
    }
    const auto fault_cost=cost(Entry{{},true});
    if (entries_.size()==max_classes||logical_bytes_+fault_cost>max_logical_bytes) {
        global_fault_=true;
        return;
    }
    try {
        entries_.emplace(id,Entry{{},true});
        logical_bytes_+=fault_cost;
        ++fault_count_;
    } catch (...) { global_fault_=true; }
}
bool LightClassStore::update(const A2FO_GameObjectClassLoadedEvent& event,std::string& error) noexcept {
    try {
        const std::uint64_t id=class_id(event.object_class);
        if (event.struct_size<sizeof(event)||!id||event.odf_field_count>128||
            (event.odf_field_count&&!event.odf_fields)||
            event.source_odf.size>max_source_odf||
            (event.source_odf.size&&!event.source_odf.data)) {
            global_fault_=true;
            error="invalid or oversized class event; clear required";
            return false;
        }
        LightConfig config{};
        error.clear();
        if (!copy_light_config(event,config,error)) {
            if (error.empty()||error=="copied light configuration allocation failure") global_fault_=true;
            else fault(id);
            return false;
        }
        if (next_version_==std::numeric_limits<std::uint64_t>::max()) {
            global_fault_=true;
            error="light class version exhausted; clear cannot recover version";
            return false;
        }
        auto record=std::make_shared<LightClassRecord>();
        record->class_id=id;
        if (event.source_odf.size)
            record->source_odf.assign(event.source_odf.data,event.source_odf.size);
        record->config=config;
        record->version=next_version_;
        auto it=entries_.find(id);
        const Entry replacement{std::move(record),false};
        const std::size_t old_cost=it==entries_.end()?0:cost(it->second);
        const std::size_t new_bytes=logical_bytes_-old_cost+cost(replacement);
        if ((it==entries_.end()&&entries_.size()==max_classes)||new_bytes>max_logical_bytes) {
            global_fault_=true;
            error="light class store capacity exceeded; clear required";
            return false;
        }
        if (it==entries_.end()) entries_.emplace(id,replacement);
        else {
            if (it->second.faulted) --fault_count_;
            it->second=replacement;
        }
        logical_bytes_=new_bytes;
        ++next_version_;
        error.clear();
        return true;
    } catch (...) {
        global_fault_=true;
        try { error="light class store allocation failure; clear required"; } catch (...) { error.clear(); }
        return false;
    }
}
std::shared_ptr<const LightClassRecord> LightClassStore::find(std::uint64_t id) const noexcept {
    if (global_fault_||!id) return {};
    const auto it=entries_.find(id);
    return it==entries_.end()||it->second.faulted ? nullptr : it->second.record;
}
void LightClassStore::clear() noexcept {
    entries_.clear();
    logical_bytes_=0;
    fault_count_=0;
    global_fault_=false;
}
} // namespace a2fo::native11
