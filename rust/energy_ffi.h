/* Private C ABI: values only; policy enum is represented as uint32_t. */
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foEnergyPolicy {
    float maximum;
    float recharge_per_second;
    uint32_t mode;
} A2foEnergyPolicy;
void a2fo_rs_energy_normalize(const A2foEnergyPolicy*, A2foEnergyPolicy*);
uint8_t a2fo_rs_energy_enabled(const A2foEnergyPolicy*);
uint8_t a2fo_rs_energy_requires_resupply(const A2foEnergyPolicy*);
float a2fo_rs_energy_clamp(float, const A2foEnergyPolicy*);
uint8_t a2fo_rs_energy_can_consume(float, float);
float a2fo_rs_energy_consume(float, float);
float a2fo_rs_energy_recharge(float, const A2foEnergyPolicy*, float, uint8_t);
float a2fo_rs_energy_reload(float, const A2foEnergyPolicy*, uint8_t);
#ifdef __cplusplus
}
#endif
