#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>
#include <stdexcept>
#include A2FO_STAGE8_BUILDYARD_POLICY_HEADER
using namespace a2fo::module_technology_policy;

static void bytes(const std::string& text) {
    std::printf("%zu:", text.size());
    for (unsigned char byte : text) std::printf("%02x", byte);
}
static void definition(const Definition& item) {
    std::printf("def "); bytes(item.key);
    std::printf(" source "); bytes(item.source);
    std::printf(" unsupported %d reason ", item.unsupported);
    bytes(item.reason);
    std::printf(" requirements %zu\n", item.requirements.size());
    for (const auto& req : item.requirements) {
        std::printf("req race "); bytes(req.race);
        std::printf(" groups");
        for (auto group : req.logic_groups) std::printf(" %d", group);
        std::printf(" objects %zu peers %zu\n",req.objects.size(),req.peers.size());
        for (const auto& object : req.objects) {
            std::printf("obj "); bytes(object.odf);
            std::printf(" not %d\n",object.must_not_have);
        }
        for (const auto& peer : req.peers) {
            std::printf("peer ally %d race ", peer.ally); bytes(peer.race);
            std::printf(" objects %zu\n",peer.objects.size());
            for (const auto& object : peer.objects) {
                std::printf("peer_obj "); bytes(object.odf);
                std::printf(" not %d\n",object.must_not_have);
            }
        }
    }
}
static void evaluate_case(const Definition& item, int owner) {
    int count_calls=0, label_calls=0;
    Callbacks callbacks;
    callbacks.object_count = [&](std::string_view odf, int team) {
        ++count_calls;
        std::printf("cb count %d %d %.*s\n",count_calls,team,
                    static_cast<int>(odf.size()),odf.data());
        if (odf.find("missing")!=std::string_view::npos) return 0;
        if (odf.find("unknown")!=std::string_view::npos) return -1;
        return team==2 ? 1 : 0;
    };
    callbacks.race_matches = [](std::string_view race,int team) {
        std::printf("cb race %d %.*s\n",team,
                    static_cast<int>(race.size()),race.data());
        return race=="borg" && team==2;
    };
    callbacks.eligible_peer = [](int from,int other,bool ally) {
        std::printf("cb peer %d %d %d\n",from,other,ally);
        return other==2 && ally;
    };
    callbacks.display_name = [&](std::string_view odf) {
        ++label_calls;
        std::printf("cb label %d %.*s\n",label_calls,
                    static_cast<int>(odf.size()),odf.data());
        if (odf.find("missing")!=std::string_view::npos)
            return std::string("Missing")+std::to_string(label_calls);
        return std::string{};
    };
    const auto evaluated=evaluate(item,owner,callbacks);
    std::printf("eval owner %d available %d missing ",owner,
                evaluated.available);
    bytes(evaluated.missing_text);
    std::printf(" calls %d %d\n",count_calls,label_calls);
}
int main() {
    const std::vector<std::string> samples = {
        "",
        "<object MODULE=\"Test\"/>",
        "<root><object MODULE=\"Foo.odf\"><requirement><obj>bar</obj>"
            "</requirement></object></root>",
        "\xef\xbb\xbf<?xml version=\"1.0\"?><object module='A'>"
            "<!--comment--><requirement race='borg' logicGroups='1,-2,+3'>"
            "<obj mustNotHave='yes'>missing.odf</obj>"
            "<ally race='borg'><obj>have.odf</obj></ally>"
            "<enemy><obj mustNotHave='false'>unknown</obj></enemy>"
            "</requirement></object>",
        "<object MODULE='A' MODULE='B'><requirement><obj>one</obj></requirement></object>",
        "<object MODULE='A'><requirement logicGroups='1,,2'><obj>one</obj>"
            "</requirement></object>",
        "<object MODULE='A'><requirement logicGroups='-2147483648,2147483647'>"
            "<obj mustNotHave='0'>one</obj></requirement></object>",
        "<object MODULE='A'><requirement logicGroups='2147483648'/>"
            "</object>",
        "<object MODULE='A'><requirement><obj mustNotHave='bad'>one</obj>"
            "</requirement></object>",
        "<object MODULE='A'><requirement><obj><nested/></obj></requirement>"
            "</object>",
        "<object MODULE='A'><requirement><other/></requirement></object>",
        "<object MODULE='A' weird='1'/>",
        "<object MODULE='A'>&amp;&lt;&gt;&quot;&apos;&#x1F642;</object>",
        "<object MODULE='A'>&#0;</object>",
        "<object MODULE='A'>&bogus;</object>",
        "<object MODULE='A'><requirement race='<bad'/></object>",
        "<object MODULE='A'><requirement/></wrong>",
        "<!DOCTYPE root><root/>",
        "<object MODULE='A'><!--broken</object>",
        "<object MODULE='A'/>trailing",
        "<root><object MODULE='One'/><object MODULE='Two'/></root>",
    };
    for (std::size_t index=0;index<samples.size();++index) {
        std::printf("sample %zu ",index); bytes(samples[index]);std::puts("");
        const auto parsed=parse_document(samples[index],"fixture.xml");
        std::printf("parsed %zu error ",parsed.definitions.size());
        bytes(parsed.error);std::puts("");
        for (const auto& item : parsed.definitions) {
            definition(item);
            evaluate_case(item,1);
            evaluate_case(item,2);
        }
    }
    auto parsed=parse_document(
        "<root><object MODULE='gate'><requirement logicGroups='1,2'>"
        "<obj>missing</obj></requirement><requirement logicGroups='1'>"
        "<obj>have</obj></requirement></object></root>","groups.xml");
    for (const auto& item : parsed.definitions) evaluate_case(item,1);
    Definition merged{}; merged.key="merge.odf"; merged.source="first";
    Definition incoming{}; incoming.key="merge.odf"; incoming.source="second";
    incoming.unsupported=true; incoming.reason="new reason";
    incoming.requirements.push_back(Requirement{});
    const bool did_merge=merge_definitions(&merged,incoming);
    std::printf("merge %d %zu %d ",did_merge,merged.requirements.size(),
                merged.unsupported);
    bytes(merged.reason);std::puts("");
    Definition other{}; other.key="other";
    std::printf("merge_reject %d %d\n",
                merge_definitions(nullptr,incoming),
                merge_definitions(&merged,other));
    Definition live{};
    live.key="live.odf";
    Requirement first{};
    first.objects.push_back(Object{"first.odf",false});
    Requirement second{};
    second.objects.push_back(Object{"second.odf",false});
    live.requirements.push_back(first);
    live.requirements.push_back(second);
    Callbacks mutation;
    mutation.object_count=[&](std::string_view odf,int team){
        std::printf("mutate count %d %.*s\n",team,
                    static_cast<int>(odf.size()),odf.data());
        if(odf=="first.odf"){
            live.requirements[1].objects[0].must_not_have=true;
        }
        return 0;
    };
    const auto live_result=evaluate(live,1,mutation);
    std::printf("mutate result %d %d ",live_result.available,
                live.requirements[1].objects[0].must_not_have);
    bytes(live_result.missing_text);std::puts("");
    return 0;
}
