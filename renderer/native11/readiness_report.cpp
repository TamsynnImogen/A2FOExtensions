#include "readiness_report.hpp"
#include <algorithm>
#include <set>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::size_t max_world=4096,max_json=1024*1024;
constexpr const char* missing_gate="missing_game_object_geometry";
constexpr const char* features_gate="unproven_game_features_absence";
constexpr const char* background_gate="unproven_background_caster_absence";
constexpr const char* effects_gate="unproven_effect_caster_absence";
void fail(std::string& error,const char* reason) noexcept {
    try {error=reason;}catch(...){error.clear();}
}
bool valid_member(const WorldMember& member) noexcept {
    return member.entity&&member.object_class&&member.class_version;
}
bool add(std::string& text,const std::string& piece) {
    if(piece.size()>max_json-text.size())return false;
    text+=piece;return true;
}
bool quoted(std::string& text,const std::string& value) {
    if(!add(text,"\""))return false;
    const char hex[]="0123456789abcdef";
    for(unsigned char c:value){
        if(c=='\"'||c=='\\'){
            if(!add(text,std::string("\\")+static_cast<char>(c)))return false;
        }else if(c<0x20){
            std::string escaped="\\u00";escaped.push_back(hex[c>>4]);escaped.push_back(hex[c&15]);
            if(!add(text,escaped))return false;
        }else if(!add(text,std::string(1,static_cast<char>(c))))return false;
    }
    return add(text,"\"");
}
bool valid_gates(const ReadinessReport& report) {
    std::size_t index=0;
    if(report.missing_objects){
        if(report.gates.empty()||report.gates.front()!=missing_gate)return false;
        ++index;
    }
    for(const char* gate:{features_gate,background_gate,effects_gate})
        if(index<report.gates.size()&&report.gates[index]==gate)++index;
    return index==report.gates.size()&&report.observed_domain_ready==report.gates.empty();
}
}

bool make_readiness_report(std::uint32_t camera,std::uint64_t generation,
    const CoverageReport& coverage,DomainEvidence domains,ReadinessReport& output,
    std::string& error) noexcept {
    try {
        if(!camera||!generation||coverage.world_objects>max_world||
           coverage.observed_objects>max_world||coverage.missing.size()>max_world||
           coverage.covered.size()>max_world||
           coverage.observed_objects!=coverage.covered.size()||
           coverage.missing.size()>coverage.world_objects||
           coverage.covered.size()!=coverage.world_objects-coverage.missing.size()||
           coverage.game_objects_complete!=coverage.missing.empty()){
            fail(error,"invalid readiness coverage counts or identity");return false;
        }
        std::set<std::uint32_t> entities,instances;
        for(const auto& member:coverage.missing){
            if(!valid_member(member)||!entities.insert(member.entity).second||
               (member.instance&&!instances.insert(member.instance).second)){
                fail(error,"invalid or duplicate readiness member ID");return false;
            }
        }
        for(const auto& covered:coverage.covered){const auto& member=covered.member;
            if(!valid_member(member)||!member.instance||!entities.insert(member.entity).second||
               !instances.insert(member.instance).second){
                fail(error,"invalid or duplicate readiness member ID");return false;
            }
        }
        ReadinessReport pending{};pending.camera=camera;pending.generation=generation;
        pending.world_objects=coverage.world_objects;
        pending.observed_objects=coverage.observed_objects;
        pending.missing_objects=coverage.missing.size();
        pending.game_objects_complete=coverage.game_objects_complete;
        pending.missing_members=coverage.missing;
        std::sort(pending.missing_members.begin(),pending.missing_members.end(),
            [](const WorldMember& a,const WorldMember& b){return a.entity<b.entity;});
        if(!coverage.game_objects_complete)pending.gates.emplace_back(missing_gate);
        if(!domains.no_game_features)pending.gates.emplace_back(features_gate);
        if(!domains.no_background_casters)pending.gates.emplace_back(background_gate);
        if(!domains.no_effect_casters)pending.gates.emplace_back(effects_gate);
        pending.observed_domain_ready=pending.gates.empty();
        output=std::move(pending);error.clear();return true;
    }catch(...){fail(error,"readiness report allocation failure");return false;}
}

bool format_readiness_report(const ReadinessReport& report,std::string& output,
    std::string& error) noexcept {
    try {
        if(!report.camera||!report.generation||report.world_objects>max_world||
           report.observed_objects>max_world||report.missing_objects>max_world||
           report.missing_objects!=report.missing_members.size()||
           report.missing_objects>report.world_objects||
           report.observed_objects!=report.world_objects-report.missing_objects||
           report.game_objects_complete!=(report.missing_objects==0)||!valid_gates(report)){
            fail(error,"invalid readiness report");return false;
        }
        std::string pending;pending.reserve(512);
        if(!add(pending,"{\"scope\":")||
           !quoted(pending,"checked_GameObject_geometry_and_other_caster_domain_absence_gates")||
           !add(pending,",\"camera\":"+std::to_string(report.camera)+
               ",\"generation\":"+std::to_string(report.generation)+
               ",\"world_objects\":"+std::to_string(report.world_objects)+
               ",\"observed_objects\":"+std::to_string(report.observed_objects)+
               ",\"missing_objects\":"+std::to_string(report.missing_objects)+
               ",\"game_objects_complete\":"+
               (report.game_objects_complete?"true":"false")+
               ",\"observed_domain_ready\":"+
               (report.observed_domain_ready?"true":"false")+
               ",\"gates\":[")){
            fail(error,"readiness JSON exceeds 1 MiB");return false;
        }
        for(std::size_t i=0;i<report.gates.size();++i)
            if((i&&!add(pending,","))||!quoted(pending,report.gates[i])){
                fail(error,"readiness JSON exceeds 1 MiB");return false;
            }
        if(!add(pending,"],\"missing_members\":[")){
            fail(error,"readiness JSON exceeds 1 MiB");return false;
        }
        std::set<std::uint32_t> missing_instances;
        for(std::size_t i=0;i<report.missing_members.size();++i){
            const auto& member=report.missing_members[i];
            if(!valid_member(member)||
               (i&&report.missing_members[i-1].entity>=member.entity)||
               (member.instance&&!missing_instances.insert(member.instance).second)||
               (i&&!add(pending,","))||
               !add(pending,"{\"opaque_entity_id\":"+std::to_string(member.entity)+
                   ",\"opaque_instance_id\":"+std::to_string(member.instance)+
                   ",\"opaque_class_id\":"+std::to_string(member.object_class)+
                   ",\"class_version\":"+std::to_string(member.class_version)+"}")){
                fail(error,"invalid or oversized readiness JSON member");return false;
            }
        }
        if(!add(pending,"]}")){fail(error,"readiness JSON exceeds 1 MiB");return false;}
        output=std::move(pending);error.clear();return true;
    }catch(...){fail(error,"readiness JSON allocation failure");return false;}
}
} // namespace a2fo::native11
