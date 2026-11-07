#include "mesh_scope.hpp"
#include <cmath>
#include <cstring>

namespace a2fo::refit {
namespace {
bool fail(std::string& error,const char* reason) noexcept {
    try { error=reason; } catch(...) { error.clear(); }
    return false;
}
}

bool mesh_model_format_supported2(const native11::ModelAsset& model) noexcept {
    const auto& bytes=model.bytes;
    if(bytes.size()<14)return false;
    if(std::memcmp(bytes.data(),"Storm3D_SW",10)!=0&&
       std::memcmp(bytes.data(),"StarTrekDB",10)!=0)return false;
    const std::uint32_t bits=std::uint32_t(bytes[10])|
        (std::uint32_t(bytes[11])<<8)|(std::uint32_t(bytes[12])<<16)|
        (std::uint32_t(bytes[13])<<24);
    static_assert(sizeof(float)==sizeof(bits));
    float version=0;
    std::memcpy(&version,&bits,sizeof(version));
    return std::isfinite(version)&&version>=1.7f&&version<=1.93f;
}

bool select_opaque_mesh2(const native11::MaterialObservation& source,
                         std::uint32_t native_node,std::uint32_t native_group_count,
                         MeshSelection2& output,std::string& error) noexcept {
    try {
        if(!source.model||!source.instance||!source.database||!source.model_serial||
           source.model->info.database!=source.database||
           source.model->info.serial!=source.model_serial)
            return fail(error,"mesh observation model identity mismatch");
        if(!native_node)return fail(error,"native mesh node is missing");
        if(native_group_count!=1)return fail(error,"native mesh must have one group");
        const native11::MaterialBinding* selected=nullptr;
        for(const auto& binding:source.parts) {
            if(binding.native_node!=native_node)continue;
            if(selected)return fail(error,"duplicate native mesh node bindings");
            selected=&binding;
        }
        if(!selected)return fail(error,"native mesh node is not observed");
        if(selected->group_index!=0||selected->node_index==UINT32_MAX)
            return fail(error,"native mesh group or node index is invalid");
        if(!selected->node_state_valid||!native11::valid_part_state(selected->node_state)||
           !selected->node_state.visible)
            return fail(error,"native mesh pose is missing, invalid, or hidden");
        if(selected->source_triangles==0||selected->source_triangles>21845)
            return fail(error,"native mesh triangle count is unsupported");
        if(selected->source_flags&~std::uint32_t{1})
            return fail(error,"native mesh material flags are unsupported");
        auto owned=std::make_shared<native11::MaterialObservation>();
        owned->instance=source.instance;
        owned->database=source.database;
        owned->model_serial=source.model_serial;
        owned->native_x87_control=source.native_x87_control;
        owned->model=source.model;
        owned->parts.push_back(*selected);
        MeshSelection2 next{std::move(owned),selected->source_triangles,
                            selected->node_index,selected->group_index};
        output=std::move(next);
        error.clear();
        return true;
    } catch(...) { return fail(error,"mesh selection allocation failed"); }
}

bool select_opaque_group2(const native11::MaterialObservation& source,
                          std::uint32_t native_node,std::uint32_t native_group_count,
                          std::uint32_t group_index,
                          MeshSelection2& output,std::string& error) noexcept {
    try {
        if(!source.model||!source.instance||!source.database||!source.model_serial||
           source.model->info.database!=source.database||
           source.model->info.serial!=source.model_serial)
            return fail(error,"mesh observation model identity mismatch");
        if(!native_node)return fail(error,"native mesh node is missing");
        if(native_group_count!=2)return fail(error,"native mesh must have two groups");
        if(group_index>=native_group_count)return fail(error,"native mesh group index is out of range");
        const native11::MaterialBinding* selected=nullptr;
        std::uint32_t node_index=UINT32_MAX;bool seen[2]{};
        for(const auto& binding:source.parts) {
            if(binding.native_node!=native_node)continue;
            if(binding.group_index>=native_group_count)
                return fail(error,"native mesh binding group is out of range");
            if(seen[binding.group_index])return fail(error,"duplicate native mesh group bindings");
            seen[binding.group_index]=true;
            if(node_index!=UINT32_MAX&&binding.node_index!=node_index)
                return fail(error,"native mesh groups name different authored nodes");
            node_index=binding.node_index;
            if(binding.group_index==group_index)selected=&binding;
        }
        if(!seen[0]||!seen[1])return fail(error,"native mesh groups are not all observed");
        if(!selected)return fail(error,"native mesh node is not observed");
        if(selected->node_index==UINT32_MAX)return fail(error,"native mesh group or node index is invalid");
        if(!selected->node_state_valid||!native11::valid_part_state(selected->node_state)||
           !selected->node_state.visible)
            return fail(error,"native mesh pose is missing, invalid, or hidden");
        if(selected->source_triangles==0||selected->source_triangles>21845)
            return fail(error,"native mesh triangle count is unsupported");
        if(selected->source_flags&~std::uint32_t{1})
            return fail(error,"native mesh material flags are unsupported");
        auto owned=std::make_shared<native11::MaterialObservation>();
        owned->instance=source.instance;
        owned->database=source.database;
        owned->model_serial=source.model_serial;
        owned->native_x87_control=source.native_x87_control;
        owned->model=source.model;
        owned->parts.push_back(*selected);
        MeshSelection2 next{std::move(owned),selected->source_triangles,
                            selected->node_index,selected->group_index};
        output=std::move(next);
        error.clear();
        return true;
    } catch(...) { return fail(error,"mesh selection allocation failed"); }
}
} // namespace a2fo::refit
