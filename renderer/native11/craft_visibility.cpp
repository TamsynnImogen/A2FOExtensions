#include "craft_visibility.hpp"
#include <cmath>
#include <utility>

#if !defined(__i386__) && !defined(__x86_64__)
#error Craft visibility threshold parity requires an x87 target
#endif

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_nodes=4096,max_depth=256;
bool fail(std::string& error,const char* message) noexcept {
    try {error=message;}catch(...){error.clear();}return false;
}
bool role(std::uint32_t id,std::size_t size) noexcept {
    return id==visibility_none||id<size;
}
float increment(float value) noexcept {
    constexpr float step=0.2f;
    float result=0;
    asm volatile("flds %1; fadds %2; fstps %0"
        :"=m"(result):"m"(value),"m"(step):"st","memory");
    return result;
}
struct Pending {std::uint32_t node;std::size_t depth;bool pruned;};
bool topology(const CraftVisibilityInput& input,
              std::vector<bool>& visible,const std::vector<std::uint32_t>& flags) {
    const auto size=input.nodes.size();
    if(!size)return input.root==visibility_none;
    if(input.root>=size||input.nodes[input.root].parent!=visibility_none||
       input.nodes[input.root].next_sibling!=visibility_none)return false;
    std::vector<std::uint8_t> seen(size);
    std::vector<Pending> stack{{input.root,1,false}};
    std::size_t count=0;
    while(!stack.empty()){
        const auto item=stack.back();stack.pop_back();
        if(item.node>=size||item.depth>max_depth||seen[item.node])return false;
        seen[item.node]=1;++count;
        const auto& node=input.nodes[item.node];
        visible[item.node]=!item.pruned&&!(flags[item.node]&1u);
        if(node.next_sibling!=visibility_none){
            if(node.next_sibling>=size||input.nodes[node.next_sibling].parent!=node.parent)return false;
            stack.push_back({node.next_sibling,item.depth,item.pruned});
        }
        if(node.first_child!=visibility_none){
            if(node.first_child>=size||input.nodes[node.first_child].parent!=item.node)return false;
            stack.push_back({node.first_child,item.depth+1,item.pruned||bool(flags[item.node]&2u)});
        }
    }
    return count==size;
}
}
bool evaluate_craft_visibility(const CraftVisibilityInput& input,
    CraftVisibilityResult& output,std::string& error) noexcept {
    try {
        const auto size=input.nodes.size();
        if(size>max_nodes||!std::isfinite(input.hull_fraction)||input.hull_fraction<0||
           !role(input.database_damage,size)||!role(input.damage_child,size)||
           !role(input.assimilation_group,size)||!role(input.base_group,size))
            return fail(error,"invalid visibility count, hull or role");
        for(auto id:input.system_groups)if(!role(id,size))
            return fail(error,"invalid system group role");
        CraftVisibilityResult candidate;
        candidate.low_bit_writes.resize(size);
        candidate.flags.reserve(size);
        for(const auto& node:input.nodes)candidate.flags.push_back(node.flags);
        candidate.visible.resize(size);
        // Check the entire owned tree before applying any role mutation.
        if(!topology(input,candidate.visible,candidate.flags))
            return fail(error,"invalid visibility topology, cycle or depth");
        float threshold=input.hull_fraction;
        if(!input.primary_database){
            if(input.database_damage!=visibility_none){candidate.flags[input.database_damage]|=3u;
                candidate.low_bit_writes[input.database_damage]|=3u;}
        }else{
            if(input.database_damage!=visibility_none){candidate.flags[input.database_damage]&=~3u;
                candidate.low_bit_writes[input.database_damage]|=3u;}
            if(input.damage_child!=visibility_none){
                std::vector<std::uint32_t> stack{input.damage_child};
                std::vector<std::uint8_t> visited(size);
                while(!stack.empty()){
                    const auto id=stack.back();stack.pop_back();
                    if(id>=size||visited[id])return fail(error,"invalid damage traversal");
                    visited[id]=1;
                    auto& flags=candidate.flags[id];
                    flags=(flags&~1u)|(threshold>=1.0f?1u:0u);
                    candidate.low_bit_writes[id]|=1u;
                    threshold=increment(threshold);
                    if(!std::isfinite(threshold))return fail(error,"nonfinite damage threshold");
                    const auto& node=input.nodes[id];
                    // LIFO: whole next-sibling subtree precedes first-child.
                    if(node.first_child!=visibility_none)stack.push_back(node.first_child);
                    if(node.next_sibling!=visibility_none)stack.push_back(node.next_sibling);
                }
            }
            for(std::size_t i=0;i<input.system_groups.size();++i){
                const auto id=input.system_groups[i];
                if(id!=visibility_none){candidate.flags[id]=(candidate.flags[id]&~2u)|
                    (input.system_hidden[i]?2u:0u);candidate.low_bit_writes[id]|=2u;}
            }
            if(input.assimilation_group!=visibility_none){
                candidate.flags[input.assimilation_group]=(candidate.flags[input.assimilation_group]&~3u)|
                    (input.assimilated?0u:3u);
                candidate.low_bit_writes[input.assimilation_group]|=3u;
            }
        }
        if(input.base_group!=visibility_none){
            candidate.flags[input.base_group]=(candidate.flags[input.base_group]&~3u)|
                ((input.instance_hidden||!input.context_enabled)?3u:0u);
            candidate.low_bit_writes[input.base_group]|=3u;
        }
        candidate.damage_threshold=threshold;
        // Visibility follows the final flags. A first topology pass above also
        // verifies no disconnected nodes before any role-specific work.
        if(!topology(input,candidate.visible,candidate.flags))
            return fail(error,"invalid final visibility topology");
        output=std::move(candidate);error.clear();return true;
    }catch(...){return fail(error,"visibility evaluation allocation failed");}
}
} // namespace a2fo::native11
