#pragma once

#include "host.h"
#include "../native11/model_store.hpp"
#include "../native11/texture_store.hpp"
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace a2fo::refit {
struct ModelUpload2 { std::uint32_t id=0,revision=0,node_count=0; };

// Owner-thread cache for immutable native11 snapshots. The caller initializes
// the host before constructing this object and must reset the host successfully
// before reset_local(new_epoch). No DLL, engine, or filesystem work occurs here.
class Resources2 final {
public:
    explicit Resources2(const A2FORefitApi2* api) noexcept;
    Resources2(const Resources2&)=delete;
    Resources2& operator=(const Resources2&)=delete;
    bool model(const std::shared_ptr<const native11::ModelAsset>&,
               ModelUpload2&,std::string& error) noexcept;
    bool texture(const std::shared_ptr<const native11::TextureAsset>&,
                 std::string& name,std::uint32_t& revision,std::string& error) noexcept;
    // Encoded bytes the caller read itself (a loose specular map), cached by `key`
    // (case-sensitive; the caller normalises) under the same texture budget.
    bool encoded(const std::string& key,const std::vector<std::uint8_t>& bytes,
                 std::string& name,std::uint32_t& revision,std::string& error) noexcept;
    // True, with its uploaded name, when `key` is already cached.
    bool cached(const std::string& key,std::string& name) const noexcept;
    static bool resolve_texture(void*,const std::shared_ptr<const native11::TextureAsset>&,
                                std::string& name,std::uint32_t& revision,std::string& error) noexcept;
    bool reset_local(const A2FORefitApi2* api,std::uint32_t new_epoch,std::string& error) noexcept;
private:
    struct ModelEntry { std::shared_ptr<const native11::ModelAsset> owner; ModelUpload2 upload; std::size_t bytes=0; };
    struct TextureEntry { std::shared_ptr<const native11::TextureAsset> owner; std::string name; std::size_t bytes=0; };
    const std::thread::id owner_;
    const A2FORefitApi2* api_;
    std::uint32_t epoch_=0,next_model_id_=1;
    std::size_t model_bytes_=0,texture_bytes_=0;
    std::map<std::uint64_t,ModelEntry> models_;
    std::map<std::uint64_t,TextureEntry> textures_;
    struct LooseEntry { std::string name; std::size_t bytes=0; };
    std::map<std::string,LooseEntry> loose_;
    std::uint32_t next_loose_=1;
    bool owner_thread(std::string& error) const noexcept;
};
} // namespace a2fo::refit
