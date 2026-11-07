#include "../renderer/native11/readiness_report.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <string>
using namespace a2fo::native11;
namespace {
void need(bool yes,const char* why){if(!yes){std::fprintf(stderr,"FAIL %s\n",why);std::exit(1);}}
WorldMember member(std::uint32_t entity,std::uint32_t instance){return {entity,instance,1001,7};}
CoverageReport partial(){CoverageReport report{};report.world_objects=2;report.observed_objects=1;
    report.missing={member(202,102)};report.covered={{member(201,101),true,false}};
    report.game_objects_complete=false;return report;}
CoverageReport complete(){auto report=partial();report.missing.clear();
    report.covered.push_back({member(202,102),false,true});report.observed_objects=2;
    report.game_objects_complete=true;return report;}
}
int main(){
    ReadinessReport report;std::string error;
    auto coverage=partial();
    need(make_readiness_report(1,9,coverage,{},report,error)&&error.empty()&&
        report.camera==1&&report.generation==9&&report.world_objects==2&&
        report.observed_objects==1&&report.missing_objects==1&&
        report.missing_members.size()==1&&!report.game_objects_complete&&
        !report.observed_domain_ready&&report.gates.size()==4&&
        report.gates[0]=="missing_game_object_geometry"&&
        report.gates[1]=="unproven_game_features_absence"&&
        report.gates[2]=="unproven_background_caster_absence"&&
        report.gates[3]=="unproven_effect_caster_absence",
        "partial report has ordered independent gates");
    coverage.missing[0].entity=999;coverage.covered.clear();
    need(report.missing_members[0].entity==202,"report owns copied opaque IDs");
    std::string json;
    need(format_readiness_report(report,json,error)&&error.empty()&&
        json.find("\"scope\":\"checked_GameObject_geometry_and_other_caster_domain_absence_gates\"")!=std::string::npos&&
        json.find("\"observed_domain_ready\":false")!=std::string::npos&&
        json.find("\"opaque_entity_id\":202")!=std::string::npos&&
        json.find("odf")==std::string::npos,"bounded local diagnostic JSON");
    const auto first_json=json;
    need(format_readiness_report(report,json,error)&&json==first_json,"deterministic formatting");
    auto full=complete();
    need(make_readiness_report(1,10,full,{},report,error)&&
        report.game_objects_complete&&!report.observed_domain_ready&&report.gates.size()==3,
        "complete GameObject list alone cannot certify other domains");
    need(make_readiness_report(1,11,full,{true,true,true},report,error)&&
        report.game_objects_complete&&report.observed_domain_ready&&report.gates.empty()&&
        report.missing_members.empty(),"narrow observed-domain readiness");
    need(format_readiness_report(report,json,error)&&
        json.find("\"observed_domain_ready\":true")!=std::string::npos&&
        json.find("\"missing_members\":[]")!=std::string::npos,
        "ready report stays narrow and empty of missing IDs");
    const auto accepted=report;
    auto reject=[&](const CoverageReport& bad,const char* name){
        need(!make_readiness_report(1,12,bad,{true,true,true},report,error)&&
            report.generation==accepted.generation&&!error.empty(),name);};
    need(!make_readiness_report(0,12,full,{true,true,true},report,error)&&
        report.generation==11,"zero camera refused transactionally");
    need(!make_readiness_report(1,0,full,{true,true,true},report,error)&&
        report.generation==11,"zero generation refused transactionally");
    auto bad=full;bad.world_objects=4097;reject(bad,"over budget count");
    bad=full;bad.observed_objects=1;reject(bad,"observed count mismatch");
    bad=full;bad.covered[1].member.entity=201;reject(bad,"duplicate entity ID");
    bad=full;bad.covered[1].member.instance=101;reject(bad,"duplicate instance ID");
    bad=full;bad.covered[1].member.entity=0;reject(bad,"zero opaque entity ID");
    bad=full;bad.game_objects_complete=false;reject(bad,"inconsistent complete flag");
    auto malformed=report;malformed.gates={"missing_game_object_geometry"};
    const auto old_json=json;
    need(!format_readiness_report(malformed,json,error)&&json==old_json,
        "formatter refuses misleading gate state transactionally");
    CoverageReport many{};many.world_objects=4096;many.game_objects_complete=false;
    for(std::uint32_t i=1;i<=4096;++i)
        many.missing.push_back({i,i,std::numeric_limits<std::uint32_t>::max(),
                                std::numeric_limits<std::uint64_t>::max()});
    need(make_readiness_report(1,13,many,{},report,error),"maximum bounded member report");
    need(format_readiness_report(report,json,error)&&json.size()<=1024*1024&&
        json.find("\"opaque_entity_id\":4096")!=std::string::npos,
        "maximum valid member report fits bounded JSON");
    const auto maximum_json=json;
    report.missing_members[1].instance=report.missing_members[0].instance;
    need(!format_readiness_report(report,json,error)&&json==maximum_json&&
        !error.empty(),"formatter rejects duplicate nonzero instance transactionally");
    std::puts("native11 readiness report test passed");
}
