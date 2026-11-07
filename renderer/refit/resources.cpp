#include "resources.hpp"
#include "../native11/model_geometry_policy.hpp"
#include <limits>
#include <utility>

namespace a2fo::refit {
namespace {
constexpr std::size_t budget=64u*1024u*1024u, max_records=256, max_model_input=4u*1024u*1024u;
bool fail(std::string& error,const char* message) noexcept {
    try { error=message; } catch(...) { error.clear(); }
    return false;
}
struct Transform {
    native11::ModelGeometryPolicy policy;
    std::string error;
};
std::int32_t transform_positions(void* context,float* xyz,std::uint32_t count) noexcept {
    auto& state=*static_cast<Transform*>(context);
    return native11::apply_model_geometry_policy(state.policy,xyz,count,state.error)?1:0;
}
bool api_ready(const A2FORefitApi2* api) noexcept {
    return api && api->size>=sizeof(A2FORefitApi2) && api->abi_version==A2FO_REFIT_ABI2
        && api->upload_model_transformed && api->upload_encoded_texture;
}
}

Resources2::Resources2(const A2FORefitApi2* api) noexcept : owner_(std::this_thread::get_id()),api_(api) {}
bool Resources2::owner_thread(std::string& error) const noexcept {
    return std::this_thread::get_id()==owner_ || fail(error,"wrong resource owner thread");
}
bool Resources2::model(const std::shared_ptr<const native11::ModelAsset>& asset,
                       ModelUpload2& output,std::string& error) noexcept {
    try {
        if(!owner_thread(error))return false;
        if(!api_ready(api_))return fail(error,"Refit ABI2 host is unavailable");
        if(!asset || !asset->info.serial)return fail(error,"model snapshot or serial is missing");
        const auto found=models_.find(asset->info.serial);
        if(found!=models_.end()) {
            if(found->second.owner.get()!=asset.get())return fail(error,"model serial ownership collision");
            output=found->second.upload;error.clear();return true;
        }
        const auto bytes=asset->bytes.size();
        if(!bytes || bytes>max_model_input || asset->info.byte_count!=bytes)
            return fail(error,"model source bytes are invalid or exceed host limit");
        if(models_.size()>=max_records || bytes>budget-model_bytes_ || next_model_id_==0)
            return fail(error,"model cache budget exceeded");
        const native11::ModelGeometryPolicy policy{asset->info.parser_scalar_bits,
                                                   asset->info.parser_native_x87_control};
        if(!native11::valid_model_geometry_policy(policy))
            return fail(error,"unsupported captured model geometry policy");
        Transform transform{policy,{}};
        A2FORefitModel2 input{};
        input.base={sizeof(input),next_model_id_,1,asset->bytes.data(),static_cast<std::uint32_t>(bytes)};
        input.context=&transform;input.transform_positions=transform_positions;
        std::map<std::uint64_t,ModelEntry> staged;
        staged.emplace(asset->info.serial,ModelEntry{asset,{next_model_id_,1,0},bytes});
        auto node=staged.extract(asset->info.serial);
        std::uint32_t node_count=0;
        const auto result=api_->upload_model_transformed(&input,&node_count);
        if(result!=A2FO_REFIT_OK) {
            error=transform.error.empty()?"Refit model upload refused code "+std::to_string(result):transform.error;
            return false;
        }
        if(!node_count)return fail(error,"Refit returned no model nodes");
        const ModelUpload2 admitted{next_model_id_,1,node_count};
        node.mapped().upload=admitted;
        models_.insert(std::move(node));
        model_bytes_+=bytes;++next_model_id_;
        output=admitted;error.clear();return true;
    } catch(...) { return fail(error,"model cache allocation failed"); }
}

bool Resources2::texture(const std::shared_ptr<const native11::TextureAsset>& asset,
                         std::string& name,std::uint32_t& revision,std::string& error) noexcept {
    try {
        if(!owner_thread(error))return false;
        if(!api_ready(api_))return fail(error,"Refit ABI2 host is unavailable");
        if(!asset || !asset->serial || !asset->bytes)return fail(error,"texture snapshot or source is missing");
        if(asset->flags&~native11::texture_has_alpha)return fail(error,"unknown native texture flags");
        if(asset->colour_key)return fail(error,"colour-key texture is unsupported by Refit ABI2");
        const auto found=textures_.find(asset->serial);
        if(found!=textures_.end()) {
            if(found->second.owner.get()!=asset.get())return fail(error,"texture serial ownership collision");
            name=found->second.name;revision=1;error.clear();return true;
        }
        const auto bytes=asset->bytes->size();
        if(!bytes || bytes>budget || bytes>std::numeric_limits<std::uint32_t>::max())
            return fail(error,"encoded texture bytes are invalid or exceed host limit");
        if(textures_.size()+loose_.size()>=max_records || bytes>budget-texture_bytes_)
            return fail(error,"texture cache budget exceeded");
        std::string key="tex_"+std::to_string(asset->serial);
        std::map<std::uint64_t,TextureEntry> staged;
        staged.emplace(asset->serial,TextureEntry{asset,key,bytes});
        auto node=staged.extract(asset->serial);
        A2FORefitEncodedTexture2 input{sizeof(input),key.c_str(),static_cast<std::uint32_t>(key.size()),
            1,asset->bytes->data(),static_cast<std::uint32_t>(bytes),0};
        const auto result=api_->upload_encoded_texture(&input);
        if(result!=A2FO_REFIT_OK) {
            error="Refit texture upload refused code "+std::to_string(result);return false;
        }
        textures_.insert(std::move(node));
        texture_bytes_+=bytes;name=std::move(key);revision=1;error.clear();return true;
    } catch(...) { return fail(error,"texture cache allocation failed"); }
}

bool Resources2::cached(const std::string& key,std::string& name) const noexcept {
    try {
        const auto found=loose_.find(key);
        if(found==loose_.end())return false;
        name=found->second.name;return true;
    } catch(...) { return false; }
}

bool Resources2::encoded(const std::string& key,const std::vector<std::uint8_t>& bytes,
                         std::string& name,std::uint32_t& revision,std::string& error) noexcept {
    try {
        if(!owner_thread(error))return false;
        if(!api_ready(api_))return fail(error,"Refit ABI2 host is unavailable");
        if(key.empty())return fail(error,"encoded texture key is empty");
        if(cached(key,name)){revision=1;error.clear();return true;}
        const auto size=bytes.size();
        if(!size || size>budget || size>std::numeric_limits<std::uint32_t>::max())
            return fail(error,"encoded texture bytes are invalid or exceed host limit");
        if(textures_.size()+loose_.size()>=max_records || size>budget-texture_bytes_ || !next_loose_)
            return fail(error,"texture cache budget exceeded");
        std::string upload="loose_"+std::to_string(next_loose_);
        LooseEntry entry{upload,size};
        A2FORefitEncodedTexture2 input{sizeof(input),upload.c_str(),static_cast<std::uint32_t>(upload.size()),
            1,bytes.data(),static_cast<std::uint32_t>(size),0};
        const auto result=api_->upload_encoded_texture(&input);
        if(result!=A2FO_REFIT_OK){error="Refit encoded texture upload refused code "+std::to_string(result);return false;}
        ++next_loose_;loose_.emplace(key,std::move(entry));texture_bytes_+=size;
        name=std::move(upload);revision=1;error.clear();return true;
    } catch(...) { return fail(error,"encoded texture cache allocation failed"); }
}

bool Resources2::resolve_texture(void* context,const std::shared_ptr<const native11::TextureAsset>& asset,
                                 std::string& name,std::uint32_t& revision,std::string& error) noexcept {
    if(!context)return fail(error,"texture resolver context is missing");
    return static_cast<Resources2*>(context)->texture(asset,name,revision,error);
}

bool Resources2::reset_local(const A2FORefitApi2* api,std::uint32_t new_epoch,std::string& error) noexcept {
    if(!owner_thread(error))return false;
    if(!api_ready(api) || !new_epoch || new_epoch<=epoch_)
        return fail(error,"new host API and increasing content epoch are required");
    // Caller has already reset the host. This method has no external side effects.
    models_.clear();textures_.clear();loose_.clear();next_loose_=1;model_bytes_=texture_bytes_=0;
    next_model_id_=1;epoch_=new_epoch;api_=api;error.clear();return true;
}
} // namespace a2fo::refit
