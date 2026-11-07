#ifndef A2FO_RUST_MOTION_FFI_H
#define A2FO_RUST_MOTION_FFI_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float x, y, z;
} A2foRsVec3;
typedef struct {
    float values[12];
} A2foRsMatrix34;
typedef struct {
    float yaw, pitch;
} A2foRsAimAngles;
typedef struct {
    float yaw_min, yaw_max, pitch_min, pitch_max, yaw_rate, pitch_rate;
} A2foRsAimLimits;
typedef struct {
    int32_t mode;
    float yaw, pitch, yaw_angle, pitch_angle, cone_angle;
} A2foRsArcConfig;
typedef struct {
    float start[3], end[3];
    int32_t style;
} A2foRsArcLine;
uint32_t a2fo_rs_motion_random_seed(uint32_t seed);
uint32_t a2fo_rs_motion_next_u32(uint32_t *state);
float a2fo_rs_motion_unit(uint32_t *state);
float a2fo_rs_motion_range(uint32_t *state, float minimum, float maximum);
typedef float (*A2foRsRangeCallback)(void *context, float minimum, float maximum);
typedef float (*A2foRsUnitCallback)(void *context);
A2foRsVec3 a2fo_rs_motion_shell(void *context, A2foRsRangeCallback range,
                                A2foRsUnitCallback unit, float minimum, float maximum);
float a2fo_rs_motion_length_squared(A2foRsVec3 value);
float a2fo_rs_motion_length(A2foRsVec3 value);
A2foRsVec3 a2fo_rs_motion_normalized(A2foRsVec3 value, A2foRsVec3 fallback);
A2foRsVec3 a2fo_rs_motion_add(A2foRsVec3 left, A2foRsVec3 right);
A2foRsVec3 a2fo_rs_motion_subtract(A2foRsVec3 left, A2foRsVec3 right);
A2foRsVec3 a2fo_rs_motion_multiply(A2foRsVec3 value, float scale);
A2foRsVec3 a2fo_rs_motion_project(A2foRsVec3 point, A2foRsVec3 centre, float radius,
                                  A2foRsVec3 fallback);
A2foRsVec3 a2fo_rs_motion_transform_point(const A2foRsMatrix34 *matrix,
                                          A2foRsVec3 value);
A2foRsVec3 a2fo_rs_motion_transform_direction(const A2foRsMatrix34 *matrix,
                                              A2foRsVec3 value);
A2foRsVec3 a2fo_rs_motion_inverse_transform_point(const A2foRsMatrix34 *matrix,
                                                  A2foRsVec3 value);
uint8_t a2fo_rs_motion_advance(A2foRsVec3 *position, A2foRsVec3 *direction,
                               A2foRsVec3 target, float speed, float turn_rate,
                               float elapsed, float arrival);
uint8_t a2fo_rs_motion_constrain_radius(A2foRsVec3 *position, float radius);
uint8_t a2fo_rs_motion_constrain_sphere(A2foRsVec3 previous, A2foRsVec3 *position,
                                        A2foRsVec3 *direction, A2foRsVec3 centre,
                                        float radius);
uint8_t a2fo_rs_motion_separate(A2foRsVec3 *left_position, A2foRsVec3 *left_direction,
                                A2foRsVec3 *right_position, A2foRsVec3 *right_direction,
                                float minimum, A2foRsVec3 fallback);
A2foRsMatrix34 a2fo_rs_motion_facing(A2foRsVec3 position, A2foRsVec3 forward,
                                     A2foRsVec3 up);
float a2fo_rs_turret_clamp(float value, float minimum, float maximum);
float a2fo_rs_turret_normalize(float value);
A2foRsAimAngles a2fo_rs_turret_calculate(const A2foRsMatrix34 *matrix,
                                         const float *target, A2foRsAimLimits limits,
                                         A2foRsAimAngles fallback);
A2foRsAimAngles a2fo_rs_turret_advance(A2foRsAimAngles current, A2foRsAimAngles desired,
                                       A2foRsAimLimits limits, float elapsed);
A2foRsMatrix34 a2fo_rs_turret_compose(const A2foRsMatrix34 *matrix,
                                      A2foRsAimAngles angles);
uint8_t a2fo_rs_arc_defer(uint8_t cannon_imp, uint8_t primary);
float a2fo_rs_arc_normalize(float value);
void a2fo_rs_arc_normalize_config(A2foRsArcConfig *config);
uint8_t a2fo_rs_arc_allows(A2foRsArcConfig config, const A2foRsMatrix34 *matrix,
                           const float *target);
size_t a2fo_rs_arc_lines(A2foRsArcConfig config, const A2foRsMatrix34 *matrix,
                         const float *origin, float radius, A2foRsArcLine *output,
                         size_t capacity);
#ifdef __cplusplus
}
#endif
#endif
