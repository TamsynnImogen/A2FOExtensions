#include "dependency_plan.hpp"
#include <cstring>
#include <set>
namespace a2fo::native11 {
namespace {
bool name(const char* value,std::size_t size,std::string& out){
    const auto* end=static_cast<const char*>(std::memchr(value,0,size));
    if(!end||end==value)return false;
    for(const char* at=value;at!=end;++at)if(static_cast<unsigned char>(*at)<32||static_cast<unsigned char>(*at)>126)return false;
    out.assign(value,end);return true;
}
}
std::shared_ptr<const ModelAsset> DependencyPlan::resolve(void* context,std::uint32_t db,std::uint64_t serial){
    const auto& plan=*static_cast<const DependencyPlan*>(context);
    const auto found=plan.models_.find({db,serial});return found==plan.models_.end()?nullptr:found->second;
}
bool build_dependency_plan(const Frame& frame,ResolveSnapshotModel resolve,void* context,
    ModelDependencyCache& cache,ParseSod parser,std::shared_ptr<const DependencyPlan>& output,std::string& error) noexcept {
    try {
        if(!resolve||!parser||frame.object_count>capacity){error="invalid dependency planner input";return false;}
        auto pending=std::shared_ptr<DependencyPlan>(new DependencyPlan);pending->frame_=frame;
        std::set<std::string> textures;std::map<std::uint64_t,std::uint32_t> serials;
        std::size_t bytes=0;
        for(std::size_t i=0;i<frame.object_count;++i){const auto& object=frame.objects[i];std::string captured;
            if(object.model_identity!=ModelIdentity::plain_database_name||!object.database||!object.model_serial||
               (object.placement_identity!=PlacementIdentity::authored_placement&&object.placement_identity!=PlacementIdentity::craft_runtime_scale)||
               !name(object.model_name,sizeof(object.model_name),captured)){error="unresolved dependency model identity";return false;}
            const auto serial=serials.emplace(object.model_serial,object.database);
            if(!serial.second&&serial.first->second!=object.database){error="dependency serial collision";return false;}
            const auto key=std::make_pair(object.database,object.model_serial);
            auto found=pending->models_.find(key);
            if(found==pending->models_.end()){
                auto asset=resolve(context,object.database,object.model_serial);std::string original;
                if(!asset||asset->info.database!=object.database||asset->info.serial!=object.model_serial||
                   !name(asset->info.name,sizeof(asset->info.name),original)||original!=captured||
                   asset->bytes.empty()||asset->bytes.size()>64u*1024*1024||asset->info.byte_count!=asset->bytes.size()||
                   asset->bytes.size()>128u*1024*1024-bytes){error="missing or conflicting dependency model version";return false;}
                bytes+=asset->bytes.size();
                std::shared_ptr<const ModelDependencies> dependencies;
                if(!cache.get(asset,parser,dependencies,error))return false;
                for(const auto& texture:dependencies->textures)if(textures.insert(texture).second){
                    if(textures.size()>256){error="frame texture dependency limit exceeded";return false;}
                    pending->textures_.push_back(texture);
                }
                found=pending->models_.emplace(key,std::move(asset)).first;
            }
            if(captured!=found->second->info.name){error="conflicting dependency object name";return false;}
        }
        output=std::move(pending);error.clear();return true;
    }catch(...){try{error="dependency planning allocation or parser failure";}catch(...){error.clear();}return false;}
}
}
