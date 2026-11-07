#pragma once
#include <cstddef>
#include <cstdint>

extern "C" {
using A2foStage8OdfLower = std::uint8_t (*)(std::uint8_t);
using A2foStage8OdfClass = bool (*)(std::uint8_t);
using A2foStage8OdfStep = std::int64_t (*)(void*,std::uint32_t,std::size_t);

void a2fo_rs_c_odf_lower(std::uint8_t*,std::size_t,A2foStage8OdfLower);
void a2fo_rs_c_odf_slashes(std::uint8_t*,std::size_t,std::uint8_t);
std::size_t a2fo_rs_c_odf_trim_slashes(const std::uint8_t*,std::size_t);
std::uint32_t a2fo_rs_c_odf_join(const std::uint8_t*,std::size_t,
    const std::uint8_t*,std::size_t);
bool a2fo_rs_c_odf_absolute(const std::uint8_t*,std::size_t,A2foStage8OdfClass);
bool a2fo_rs_c_odf_extension(const std::uint8_t*,std::size_t);
std::uint64_t a2fo_rs_c_odf_fpq_size(const std::uint8_t*,std::size_t);
bool a2fo_rs_c_odf_precedes(void*,A2foStage8OdfStep);
struct A2foStage8OdfScanEntry {
    const std::uint8_t* name;
    std::size_t size;
    std::uint32_t attributes;
};
using A2foStage8OdfScanNext = std::int32_t (*)(void*,A2foStage8OdfScanEntry*);
bool a2fo_rs_c_odf_scan(void*,unsigned,std::uint32_t,std::uint32_t,std::uint32_t,
    A2foStage8OdfScanNext,A2foStage8OdfStep);
bool a2fo_rs_c_odf_overlays(void*,std::uint32_t,std::uint32_t,A2foStage8OdfStep);
bool a2fo_rs_c_odf_root_mods(void*,std::size_t,A2foStage8OdfStep);
bool a2fo_rs_c_odf_collect_roots(void*,std::size_t,A2foStage8OdfStep);
bool a2fo_rs_c_odf_collect_archives(void*,std::size_t,A2foStage8OdfStep);
bool a2fo_rs_c_odf_refresh_ids(void*,A2foStage8OdfStep);
bool a2fo_rs_c_odf_ensure_id(void*,A2foStage8OdfStep);
bool a2fo_rs_c_odf_hash_entries(void*,std::size_t,A2foStage8OdfStep);
bool a2fo_rs_c_odf_winners(void*,std::size_t,A2foStage8OdfStep);
bool a2fo_rs_c_odf_register(void*,A2foStage8OdfStep);
bool a2fo_rs_c_odf_index_scan(void*,std::size_t,std::size_t,A2foStage8OdfStep);
bool a2fo_rs_c_odf_register_once(void*,A2foStage8OdfStep);
bool a2fo_rs_c_odf_lookup(void*,A2foStage8OdfStep);
struct A2foStage8OdfSpan { std::size_t first; std::size_t length; };
bool a2fo_rs_c_odf_trim_name(const std::uint8_t*,std::size_t,
    A2foStage8OdfSpan*);
std::size_t a2fo_rs_c_odf_basename_start(const std::uint8_t*,std::size_t);
std::uint32_t a2fo_rs_c_odf_resolve(void*,A2foStage8OdfStep);
bool a2fo_rs_c_odf_init(void*,A2foStage8OdfStep);
bool a2fo_rs_c_odf_list_count(std::int32_t);
bool a2fo_rs_c_odf_list_bounds_count(std::int32_t,
    std::int32_t,std::int32_t);
bool a2fo_rs_c_odf_list_capacity(std::int32_t,std::int32_t);
bool a2fo_rs_c_odf_hash_bucket(void*,std::uint32_t,A2foStage8OdfStep);
bool a2fo_rs_c_odf_bucket_admit(std::uint32_t,bool);
bool a2fo_rs_c_odf_validate(void*,A2foStage8OdfStep);
}
