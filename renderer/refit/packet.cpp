#include "packet.hpp"
#include "../native11/native_arithmetic.hpp"
#include "../native11/native_matrix34.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <set>

namespace a2fo::refit {
namespace {
bool fail(std::string& error,const char* message) noexcept {
    try {error=message;} catch(...) {error.clear();} return false;
}
bool finite_array(const float* values,std::size_t count) noexcept {
    for(std::size_t i=0;i<count;++i)if(!std::isfinite(values[i])||std::fabs(values[i])>1e12f)return false;
    return true;
}
bool affine(const float* m) noexcept {
    return finite_array(m,16)&&m[12]==0&&m[13]==0&&m[14]==0&&m[15]==1;
}
native11::NativeMatrix34 row34(const float* m) noexcept {
    return {m[0],m[4],m[8],m[1],m[5],m[9],m[2],m[6],m[10],m[3],m[7],m[11]};
}
bool printable(const std::string& name) noexcept {
    if(name.empty()||name.size()>255)return false;
    for(unsigned char c:name)
        if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||
             (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return false;
    return true;
}
bool light_valid(const A2FORefitLight& light) noexcept {
    return (light.kind==A2FO_REFIT_DIRECTIONAL||light.kind==A2FO_REFIT_POINT)&&
        finite_array(light.position,3)&&finite_array(light.direction,3)&&
        finite_array(light.colour,3)&&std::isfinite(light.intensity)&&
        std::isfinite(light.start)&&std::isfinite(light.range)&&
        light.intensity>=0&&light.intensity<=1e12f&&light.start>=0&&light.start<=1e12f&&
        light.range>=0&&light.range<=1e12f&&
        light.colour[0]>=0&&light.colour[1]>=0&&light.colour[2]>=0&&
        finite_array(light.colour,3)&&
        (light.kind!=A2FO_REFIT_POINT||light.range>0)&&
        (light.kind!=A2FO_REFIT_DIRECTIONAL||
         std::hypot(light.direction[0],light.direction[1],light.direction[2])>=1e-6f)&&
        double(light.colour[0])*light.intensity<=1e12&&
        double(light.colour[1])*light.intensity<=1e12&&
        double(light.colour[2])*light.intensity<=1e12;
}
bool nonsingular(const native11::NativeMatrix34& m) noexcept {
    const float cx=m[4]*m[8]-m[5]*m[7];
    const float cy=m[5]*m[6]-m[3]*m[8];
    const float cz=m[3]*m[7]-m[4]*m[6];
    return std::fabs(m[0]*cx+m[1]*cy+m[2]*cz)>1e-12f;
}
} // namespace

bool build_packet2(const native11::Object& object,const native11::MaterialObservation& observation,
    const CameraView2& camera,const Presentation2& presentation,
    std::uint32_t model_id,std::uint32_t model_revision,
    std::uint32_t node_count,std::uint32_t source_epoch,std::uint32_t serial,
    const std::vector<A2FORefitLight>& lights,void* texture_context,ResolveTexture2 resolve_texture,
    std::shared_ptr<const Packet2>& output,std::string& error) noexcept {
    try {
        const auto& model=observation.model;
        if(!object.entity||!object.instance||!object.database||!object.model_serial||
           object.model_identity!=native11::ModelIdentity::plain_database_name||
           (object.placement_identity!=native11::PlacementIdentity::authored_placement&&
            object.placement_identity!=native11::PlacementIdentity::craft_runtime_scale)||
           !model||model->info.database!=object.database||model->info.serial!=object.model_serial||
           !model->info.byte_count||model->bytes.size()!=model->info.byte_count||
           model->bytes.size()>64u*1024u*1024u||observation.model!=model||
           observation.instance!=object.instance||observation.database!=object.database||
           observation.model_serial!=object.model_serial||!model_id||!model_revision||
           !node_count||node_count>4096||!source_epoch||!serial)
            return fail(error,"invalid object, model, or packet identity");
        if(object.placement_identity==native11::PlacementIdentity::craft_runtime_scale)
            for(float scale:object.runtime_scale)if(!std::isfinite(scale)||scale==0)
                return fail(error,"invalid runtime scale identity");
        const auto* end=static_cast<const char*>(std::memchr(object.model_name,0,sizeof(object.model_name)));
        const auto* model_end=static_cast<const char*>(std::memchr(model->info.name,0,sizeof(model->info.name)));
        if(!end||end==object.model_name||!model_end||model_end==model->info.name||
           std::string(object.model_name,end)!=std::string(model->info.name,model_end))
            return fail(error,"model name mismatch");
        if(!affine(object.world)||!finite_array(camera.world_to_clip,16)||
           !finite_array(camera.viewport,4)||!finite_array(camera.eye,3)||
           !finite_array(&camera.time,1))
            return fail(error,"invalid world or camera matrix");
        if((presentation.flags&~(A2FO_REFIT_TEAM|A2FO_REFIT_UNLIT))||
           presentation.alpha!=1||!finite_array(presentation.team,3)||
           !finite_array(presentation.ambient,3)||!finite_array(&presentation.self_illum,1)||
           presentation.team[0]<0||presentation.team[1]<0||presentation.team[2]<0||
           presentation.ambient[0]<0||presentation.ambient[1]<0||presentation.ambient[2]<0||
           presentation.self_illum<0)
            return fail(error,"invalid DIRECT presentation");
        const float* v=camera.viewport;
        if(v[0]<0||v[1]<0||v[2]<1||v[3]<1||v[2]>16384||v[3]>16384||
           double(v[0])+v[2]>UINT32_MAX||double(v[1])+v[3]>UINT32_MAX||
           v[0]!=std::floor(v[0])||v[1]!=std::floor(v[1])||
           v[2]!=std::floor(v[2])||v[3]!=std::floor(v[3]))
            return fail(error,"invalid viewport");
        if(!native11::valid_native_x87_encoding(observation.native_x87_control)||
           observation.native_x87_control==0)return fail(error,"unknown native arithmetic");
        if(observation.parts.empty()||observation.parts.size()>8192||lights.size()>32)
            return fail(error,"part or light limit");
        unsigned directional_count=0;
        for(const auto& light:lights) {
            if(!light_valid(light))return fail(error,"invalid light");
            if(light.kind==A2FO_REFIT_DIRECTIONAL&&++directional_count>4)
                return fail(error,"directional light limit");
        }
        native11::NativeArithmeticScope arithmetic(observation.native_x87_control);
        if(!arithmetic.valid()||!arithmetic.known())return fail(error,"invalid native arithmetic");
        auto next=std::shared_ptr<Packet2>(new Packet2);
        next->parts_.reserve(observation.parts.size());
        next->texture_names_.reserve(observation.parts.size());
        next->lights_=lights;
        std::set<std::pair<std::uint32_t,std::uint32_t>> seen;
        const auto parent=row34(object.world);
        for(const auto& binding:observation.parts) {
            if(binding.node_index>=node_count||binding.group_index==UINT32_MAX||
               !seen.insert({binding.node_index,binding.group_index}).second||
               !binding.node_state_valid||!native11::valid_part_state(binding.node_state))
                return fail(error,"invalid or missing part state");
            if(!binding.node_state.visible)continue;
            if(binding.texture&&(binding.texture->colour_key||
               (binding.texture->flags&~native11::texture_has_alpha)||
               !binding.texture->serial||!binding.texture->texture||!binding.texture->bytes||
               binding.texture->bytes->empty()))return fail(error,"unsupported texture asset");
            if(!binding.texture&& !binding.authored_texture.empty())
                return fail(error,"missing texture asset");
            native11::NativeMatrix34 world{};
            if(!native11::native_matrix34_multiply(row34(binding.node_state.local_to_model),
                parent,world,error))return false;
            if(!finite_array(world.data(),world.size())||!nonsingular(world))
                return fail(error,"invalid composed part world");
            A2FORefitPart2 part{};
            part.size=sizeof(part);part.model_id=model_id;part.model_revision=model_revision;
            part.node_index=binding.node_index;part.group_index=binding.group_index;
            part.flags=presentation.flags;
            std::memcpy(part.world,world.data(),sizeof(part.world));
            part.alpha=presentation.alpha;
            std::memcpy(part.team,presentation.team,sizeof(part.team));
            std::memcpy(part.ambient,presentation.ambient,sizeof(part.ambient));
            part.self_illum=presentation.self_illum;
            if(binding.texture) {
                if(!resolve_texture)return fail(error,"missing texture resolver");
                std::string name,detail;std::uint32_t revision=0;
                if(!resolve_texture(texture_context,binding.texture,name,revision,detail)||
                   !printable(name)||!revision)return fail(error,"unresolved texture identity");
                next->texture_names_.push_back(std::move(name));part.texture_revision=revision;
            } else next->texture_names_.emplace_back();
            next->parts_.push_back(part);
        }
        for(std::size_t i=0;i<next->parts_.size();++i) {
            const auto& name=next->texture_names_[i];
            next->parts_[i].texture_name=name.empty()?nullptr:name.c_str();
            next->parts_[i].texture_name_length=static_cast<std::uint32_t>(name.size());
        }
        auto& frame=next->frame_;
        frame.size=sizeof(frame);frame.source_epoch=source_epoch;frame.serial=serial;
        frame.x=static_cast<std::uint32_t>(v[0]);frame.y=static_cast<std::uint32_t>(v[1]);
        frame.width=static_cast<std::uint32_t>(v[2]);frame.height=static_cast<std::uint32_t>(v[3]);
        frame.flags=A2FO_REFIT_FRAME_DIRECT;
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
            frame.view[row*4+col]=camera.world_to_clip[col*4+row];
        for(unsigned i=0;i<4;++i)frame.projection[i*4+i]=1;
        std::memcpy(frame.eye,camera.eye,sizeof(frame.eye));frame.time=camera.time;
        frame.parts=next->parts_.empty()?nullptr:next->parts_.data();
        frame.part_count=static_cast<std::uint32_t>(next->parts_.size());
        frame.lights=next->lights_.empty()?nullptr:next->lights_.data();
        frame.light_count=static_cast<std::uint32_t>(next->lights_.size());
        output=std::move(next);error.clear();return true;
    } catch(...) {return fail(error,"packet build exception");}
}
} // namespace a2fo::refit
