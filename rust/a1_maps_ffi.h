/* Private A1 map-policy ABI. All pointers denote live byte spans or POD. */
#pragma once
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct A2foA1MapHeader {
    uint32_t version;
    size_t metadata_offset;
    uint8_t mission_name[64];
    uint32_t has_map_bounds;
    float minimum_extent[3];
    float map_size[3];
} A2foA1MapHeader;
typedef struct A2foA1MapTail {
    size_t first_object_offset;
    size_t mission_offset;
    uint32_t object_count;
} A2foA1MapTail;
typedef struct A2foA1Mdf {
    size_t start_location_count;
    uint32_t locations[8][2];
} A2foA1Mdf;
uint8_t a2fo_rs_a1_mission_record_at(const uint8_t* data, size_t size);
uint8_t a2fo_rs_a1_parse_header(const uint8_t* data, size_t size, A2foA1MapHeader* header);
uint8_t a2fo_rs_a1_compatible_bounds(const A2foA1MapHeader* header, float* minimum_extent, float* map_size);
uint8_t a2fo_rs_a1_locate_tail(const uint8_t* data, size_t size, A2foA1MapTail* tail);
uint8_t a2fo_rs_a1_parse_mdf(const uint8_t* data, size_t size, A2foA1Mdf* mdf);
uint8_t a2fo_rs_a1_world_position(const A2foA1MapHeader* header, uint32_t x, uint32_t y, float* output);
#ifdef __cplusplus
}
#endif
