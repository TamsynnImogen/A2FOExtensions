// Policy-only native playback coverage test. Synthetic addresses are never dereferenced.
#include "../modules/A2FOAnimations/native_observation.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
using a2fo::animations::NativeEvaluationScope;
using a2fo::animations::NativePlaybackCoverage;
using a2fo::animations::NativePlaybackProof;

void need(bool okay,const char* message){
    if(!okay){std::fprintf(stderr,"FAIL %s\n",message);std::exit(1);}
}
template<class T> T* synthetic(std::uintptr_t value){return reinterpret_cast<T*>(value);}

NativePlaybackProof sentinel(){
    NativePlaybackProof result{};
    result.image=reinterpret_cast<HMODULE>(std::uintptr_t{0x60060u});
    result.target=synthetic<void>(0x70070u);
    result.replacement=synthetic<void>(0x80080u);
    result.gateway=synthetic<void>(0x90090u);
    result.length=37;
    result.epoch=0x123456789abcdef0ull;
    return result;
}
bool unchanged(const NativePlaybackProof& actual,const NativePlaybackProof& expected){
    return actual.image==expected.image&&actual.target==expected.target&&
        actual.replacement==expected.replacement&&actual.gateway==expected.gateway&&
        actual.length==expected.length&&actual.epoch==expected.epoch;
}
void refused(NativePlaybackCoverage& coverage,HMODULE image,const char* message){
    auto output=sentinel();const auto before=output;
    need(!coverage.snapshot(image,output)&&unchanged(output,before),message);
}
NativePlaybackProof descriptor(HMODULE image){
    NativePlaybackProof proof{};
    proof.image=image;
    proof.target=synthetic<void>(0x10010u);
    proof.replacement=synthetic<void>(0x20020u);
    proof.gateway=synthetic<void>(0x30030u);
    proof.length=6;
    proof.epoch=0; // The coverage epoch, rather than caller input, is published.
    return proof;
}
void accepted(NativePlaybackCoverage& coverage,HMODULE image,
    const NativePlaybackProof& expected,std::uint64_t& previous,const char* message){
    auto output=sentinel();
    need(coverage.snapshot(image,output)&&output.image==expected.image&&
         output.target==expected.target&&output.replacement==expected.replacement&&
         output.gateway==expected.gateway&&output.length==expected.length&&
         output.epoch>previous&&output.epoch!=0,message);
    previous=output.epoch;
}
}

int main(){
    NativePlaybackCoverage coverage;
    const auto image=reinterpret_cast<HMODULE>(std::uintptr_t{0x40040u});
    const auto other=reinterpret_cast<HMODULE>(std::uintptr_t{0x50050u});
    const auto proof=descriptor(image);
    std::uint64_t epoch=0;

    refused(coverage,image,"unconfigured source refused atomically");
    coverage.configure(false,proof);
    refused(coverage,image,"not-ready source refused atomically");
    coverage.configure(true,proof);
    refused(coverage,other,"wrong image refused atomically");
    accepted(coverage,image,proof,epoch,"owned ready descriptor published with coverage epoch");

    need(coverage.begin_class_update(),"first class update begins");
    refused(coverage,image,"pending class update blocks admission");
    need(coverage.begin_class_update(),"nested class update begins");
    coverage.end_class_update(false);
    refused(coverage,image,"outer class update still blocks admission");
    coverage.end_class_update(false);
    accepted(coverage,image,proof,epoch,"completed empty class updates advance epoch");

    need(coverage.begin_class_update(),"custom class update begins");
    coverage.end_class_update(true);
    refused(coverage,image,"custom class blocks native fallback");
    coverage.configure(true,proof);
    refused(coverage,image,"reconfiguration cannot erase published custom class");
    need(coverage.begin_class_update(),"empty registry publication begins");
    refused(coverage,image,"pending empty registry publication blocks admission");
    coverage.end_class_update(false);
    accepted(coverage,image,proof,epoch,"completed empty registry restores fallback at new epoch");

    need(coverage.begin_class_update(),"second custom registry update begins");
    coverage.end_class_update(true);
    refused(coverage,image,"second custom registry blocks fallback");
    need(coverage.begin_evaluate(),"evaluation begins with custom class active");
    refused(coverage,image,"in-flight evaluation blocks admission");
    need(coverage.begin_class_update(),"class removal begins during evaluation");
    coverage.end_class_update(false);
    refused(coverage,image,"last custom class removal cannot bypass in-flight evaluation");
    coverage.end_evaluate();
    accepted(coverage,image,proof,epoch,"completed evaluation advances native epoch");

    try {
        NativeEvaluationScope scope(coverage);
        refused(coverage,image,"scope blocks admission during evaluation");
        throw 1;
    } catch(int) {}
    accepted(coverage,image,proof,epoch,"scope releases evaluation on exception");

    coverage.configure(false,proof);
    refused(coverage,image,"disable during reinitialization invalidates admission");
    coverage.configure(true,proof);
    accepted(coverage,image,proof,epoch,"reinitialization publishes later epoch");

    auto invalid=proof;invalid.gateway=nullptr;
    coverage.configure(true,invalid);
    refused(coverage,image,"incomplete descriptor refused atomically");
    coverage.configure(true,proof);
    accepted(coverage,image,proof,epoch,"valid descriptor recovers with later epoch");

    std::puts("PASS native animation playback coverage policy");
}
