#include "../modules/A2FORecoveryTests/recovery_tests.hpp"
#include <cstring>
#include <exception>
extern "C" int recovery_compare(const a2fo::recovery_tests::Snapshot* input,char* output,std::size_t capacity) noexcept {
    if(!input || !output)return -1;
    try {
        const auto comparison=a2fo::recovery_tests::compare(*input);
        const auto text=a2fo::recovery_tests::record_json(*input,comparison);
        if(text.size()+1>capacity)return -1;
        std::memcpy(output,text.c_str(),text.size()+1);
        return static_cast<int>(comparison.outcome);
    } catch(...) {return -1;}
}
