#include "../modules/A2FOResources/api.hpp"
extern "C" __declspec(dllexport) std::int32_t A2FO_CALL
A2FOResources_GetCost(void*,std::uint32_t index){return index==A2FO_RESOURCE_TRITANIUM?7:0;}
extern "C" __declspec(dllexport) const char* A2FO_CALL
A2FOResources_GetPresentationText(void*,std::uint32_t index,std::uint32_t presentation){
    if(index!=A2FO_RESOURCE_TRITANIUM)return nullptr;
    return presentation==A2FO_RESOURCE_PRESENTATION_ICON?"m":"Metal";
}
