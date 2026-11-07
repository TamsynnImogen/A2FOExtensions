#pragma once
#include <cstddef>
#include <cstdint>
extern "C" {
typedef std::uint8_t(*A2foRsCoreClassify)(std::uint8_t,void*);
typedef std::uint64_t(*A2foRsCoreStringOp)(void*,std::uint32_t,std::uintptr_t,std::uintptr_t);
std::uint8_t a2fo_rs_stage8_core_validate(std::uint32_t,const std::uint8_t*,std::size_t,A2foRsCoreClassify,void*);
std::uint8_t a2fo_rs_stage8_core_relative(A2foRsCoreStringOp,void*);
std::uint64_t a2fo_rs_stage8_core_startup(std::uint32_t,A2foRsCoreStringOp,void*);
std::uint64_t a2fo_rs_stage8_core_registration(std::uint32_t,A2foRsCoreStringOp,void*);
std::uint64_t a2fo_rs_stage8_core_defaults(std::uint32_t,std::uint32_t,A2foRsCoreStringOp,void*);
void a2fo_rs_stage8_core_text(std::uint32_t,std::uint8_t*,std::size_t,std::uint8_t,A2foRsCoreClassify,void*);
std::uint32_t a2fo_rs_stage8_core_join(const std::uint8_t*,std::size_t,std::size_t);
void a2fo_rs_stage8_core_trim(const std::uint8_t*,std::size_t,std::size_t*,std::size_t*);
std::uint8_t a2fo_rs_stage8_core_suffix(const std::uint8_t*,std::size_t,std::uint32_t);
std::size_t a2fo_rs_stage8_core_basename(const std::uint8_t*,std::size_t);
std::uint64_t a2fo_rs_stage8_core_transaction(std::uint32_t,std::uint8_t,A2foRsCoreStringOp,void*);
std::uint64_t a2fo_rs_stage8_core_overlay(std::uint32_t,A2foRsCoreStringOp,void*);
std::uint64_t a2fo_rs_stage8_core_field_handler(std::uint32_t,A2foRsCoreStringOp,void*);
const std::uint8_t* a2fo_rs_stage8_core_owner(const std::uint8_t*);
}
