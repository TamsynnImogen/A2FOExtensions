#include "opened_file.hpp"
#include <limits>
#include <new>
#include <utility>

namespace a2fo::native11 {
namespace {
constexpr std::uint32_t hard_limit=256u*1024u*1024u;
constexpr std::uint32_t last_field=0x1c;
constexpr std::uint32_t word_size=4;
bool read_word(void* ctx,AssetMemoryRead read,std::uint32_t address,std::uint32_t& value) {
    if (address>std::numeric_limits<std::uint32_t>::max()-(word_size-1)) return false;
    std::uint8_t bytes[word_size];
    if (!read(ctx,address,bytes,word_size)) return false;
    value=std::uint32_t(bytes[0])|(std::uint32_t(bytes[1])<<8)|
          (std::uint32_t(bytes[2])<<16)|(std::uint32_t(bytes[3])<<24);
    return true;
}
OpenedFileStatus capture_file(void* ctx,AssetMemoryRead read,
    std::uint32_t stream,std::uint32_t nameAddress,std::uint32_t expectedVtable,
    std::uint32_t maxBytes,OpenedFile& out,bool initial) noexcept {
    if (!read||!stream||!nameAddress||!expectedVtable||!maxBytes||maxBytes>hard_limit)
        return OpenedFileStatus::invalid_arguments;
    if (stream>std::numeric_limits<std::uint32_t>::max()-last_field-(word_size-1))
        return OpenedFileStatus::invalid_arguments;
    try {
        std::uint32_t vtable=0,begin=0,end=0,cursor=0;
        if (!read_word(ctx,read,stream,vtable)) return OpenedFileStatus::unreadable;
        if (vtable!=expectedVtable) return OpenedFileStatus::wrong_type;
        if (!read_word(ctx,read,stream+0x14,begin)||
            !read_word(ctx,read,stream+0x18,end)||
            !read_word(ctx,read,stream+0x1c,cursor)) return OpenedFileStatus::unreadable;
        if (!begin||end<=begin||cursor<begin||cursor>end||(initial&&cursor!=begin)) return OpenedFileStatus::invalid_span;
        const std::uint32_t size=end-begin;
        if (size>maxBytes) return OpenedFileStatus::budget_exceeded;
        OpenedFile candidate;
        candidate.stream=stream;candidate.buffer_begin=begin;candidate.buffer_end=end;
        bool terminated=false;
        for (std::uint32_t i=0;i<1024;++i) {
            if (nameAddress>std::numeric_limits<std::uint32_t>::max()-i)
                return OpenedFileStatus::invalid_name;
            std::uint8_t c=0;
            if (!read(ctx,nameAddress+i,&c,1)) return OpenedFileStatus::unreadable;
            if (c==0) { terminated=true;break; }
            if (c<0x20||c>0x7e) return OpenedFileStatus::invalid_name;
            candidate.name.push_back(static_cast<char>(c));
        }
        if (!terminated||candidate.name.empty()) return OpenedFileStatus::invalid_name;
        candidate.bytes.resize(size);
        if (!read(ctx,begin,candidate.bytes.data(),size)) return OpenedFileStatus::unreadable;
        out=std::move(candidate);
        return OpenedFileStatus::ok;
    } catch (const std::bad_alloc&) {
        return OpenedFileStatus::out_of_memory;
    } catch (...) {
        return OpenedFileStatus::unreadable;
    }
}
}
OpenedFileStatus capture_opened_file(void* ctx,AssetMemoryRead read,
    std::uint32_t stream,std::uint32_t nameAddress,std::uint32_t expectedVtable,
    std::uint32_t maxBytes,OpenedFile& out) noexcept {
    return capture_file(ctx,read,stream,nameAddress,expectedVtable,maxBytes,out,true);
}
OpenedFileStatus capture_consumed_file(void* ctx,AssetMemoryRead read,
    std::uint32_t stream,std::uint32_t nameAddress,std::uint32_t expectedVtable,
    std::uint32_t maxBytes,OpenedFile& out) noexcept {
    return capture_file(ctx,read,stream,nameAddress,expectedVtable,maxBytes,out,false);
}
} // namespace a2fo::native11
