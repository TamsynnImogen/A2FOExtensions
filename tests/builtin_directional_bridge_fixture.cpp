// Synthetic optional provider for headless combat integration tests only.
#include "../modules/A2FODirectionalShields/api.hpp"

namespace {
unsigned calls[3]{};
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FODirectionalShields_ConnectDamageBridge() {
    ++calls[0];
    return true;
}

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FODirectionalShields_BeginDamage(
    void*, const void*, A2FO_DirectionalShieldDamageScope*) {
    ++calls[1];
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FODirectionalShields_EndDamage(A2FO_DirectionalShieldDamageScope*) {
    ++calls[2];
}

extern "C" __declspec(dllexport)
unsigned A2FO_CALL A2FOBuiltinFixture_DirectionalCalls(unsigned which) {
    return which < 3 ? calls[which] : 0;
}
