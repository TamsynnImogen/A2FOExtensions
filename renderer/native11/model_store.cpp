#include "model_store.hpp"
#include "native_arithmetic.hpp"
#include <cstring>
#include <limits>
#include <utility>

namespace a2fo::native11 {

void ModelAssetStore::invalidate(std::uint32_t database) noexcept {
    const auto found = records_.find(database);
    if (found == records_.end()) return;
    bytes_ -= found->second->info.byte_count;
    records_.erase(found);
}

bool ModelAssetStore::publish(std::uint32_t database, std::uint32_t parser_scalar_bits,
                              OpenedFile&& file, std::uint32_t native_x87_control) noexcept {
    invalidate(database); // Even a rejected replacement must retire the old binding.
    const auto count = file.bytes.size();
    if (!valid_native_x87_encoding(native_x87_control) || !database || count == 0 ||
        count > std::numeric_limits<std::uint32_t>::max() ||
        file.name.empty() || file.name.size() >= sizeof(ModelAssetInfo::name) ||
        records_.size() >= max_records_ || count > max_bytes_ ||
        bytes_ > max_bytes_ - count || last_serial_ == std::numeric_limits<std::uint64_t>::max())
        return false;
    for (unsigned char character : file.name)
        if (character < 0x20 || character > 0x7e) return false;
    try {
        auto asset = std::make_shared<ModelAsset>();
        asset->info.serial = last_serial_ + 1;
        asset->info.database = database;
        asset->info.byte_count = static_cast<std::uint32_t>(count);
        asset->info.parser_scalar_bits = parser_scalar_bits;
        asset->info.parser_native_x87_control = native_x87_control;
        std::memcpy(asset->info.name, file.name.data(), file.name.size());
        asset->bytes = std::move(file.bytes);
        records_.emplace(database, std::move(asset));
        bytes_ += count;
        ++last_serial_;
        return true;
    } catch (...) {
        return false;
    }
}

bool ModelAssetStore::info(std::uint32_t database, ModelAssetInfo& output) const noexcept {
    const auto found = records_.find(database);
    if (found == records_.end()) return false;
    output = found->second->info;
    return true;
}

std::shared_ptr<const ModelAsset> ModelAssetStore::find(std::uint32_t database,
                                                        std::uint64_t serial) const noexcept {
    const auto found = records_.find(database);
    return found != records_.end() && found->second->info.serial == serial ?
        found->second : nullptr;
}

void ModelAssetStore::clear() noexcept {
    records_.clear();
    bytes_ = 0;
}

} // namespace a2fo::native11
