#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace a2fo::native11 {
using AssetMemoryRead=bool(*)(void*,std::uint32_t address,void*,std::size_t);
struct OpenedFile {
    std::string name;
    std::vector<std::uint8_t> bytes;
    std::uint32_t stream=0,buffer_begin=0,buffer_end=0;
};
enum class OpenedFileStatus {
    ok,invalid_arguments,unreadable,wrong_type,invalid_span,invalid_name,
    budget_exceeded,out_of_memory
};
OpenedFileStatus capture_opened_file(void* ctx,AssetMemoryRead read,
    std::uint32_t stream,std::uint32_t nameAddress,std::uint32_t expectedVtable,
    std::uint32_t maxBytes,OpenedFile& out) noexcept;
// After a parser returns, copy the same actual-read span while its stream is
// still alive. The cursor may have advanced within [begin,end]. No seek/replay.
OpenedFileStatus capture_consumed_file(void* ctx,AssetMemoryRead read,
    std::uint32_t stream,std::uint32_t nameAddress,std::uint32_t expectedVtable,
    std::uint32_t maxBytes,OpenedFile& out) noexcept;
} // namespace a2fo::native11
