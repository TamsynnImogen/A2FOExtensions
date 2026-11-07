// Compile twice: with the original snapshot (-DSOL_BASELINE) and the port.
#ifdef SOL_BASELINE
#include "build/rust-migration-stage4/before/modules/A2FOSOLMaps/sol_format.hpp"
#else
#include "modules/A2FOSOLMaps/sol_format.hpp"
#endif

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;
struct Record { std::uint32_t tag; Bytes data; };
using Records = std::vector<Record>;

Bytes word(std::uint32_t value) {
    Bytes result(4);
    std::memcpy(result.data(), &value, 4);
    return result;
}
Bytes half(std::uint16_t value) {
    Bytes result(2);
    std::memcpy(result.data(), &value, 2);
    return result;
}
Bytes vec3(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    Bytes result;
    for (auto bits : {x, y, z}) {
        const auto value = word(bits);
        result.insert(result.end(), value.begin(), value.end());
    }
    return result;
}
Bytes text(const char* value, std::size_t size) {
    return Bytes(value, value + size);
}
Record number(std::uint32_t value) { return {4, word(value)}; }
Record string(const Bytes& value) { return {2, value}; }
void pair(Records& records, std::size_t i, Bytes value) {
    records[i] = number(static_cast<std::uint32_t>(value.size()));
    records[i + 1] = string(value);
}
Records valid_records() {
    Records r(40, number(0));
    pair(r, 4, text("Game\0hidden", 11));
    pair(r, 6, text("guid", 4));
    pair(r, 8, text("T\0binary", 8));
    pair(r, 10, text("path", 4));
    pair(r, 12, text("map.sol", 7));
    r[15] = {9, vec3(0x7fc01234u, 0x80000000u, 0x3f800000u)};
    r[16] = {9, vec3(0x40000000u, 0x40400000u, 0x40800000u)};
    pair(r, 19, text("mission.lua\0tail", 16));
    r[27] = number(1);
    r[28] = {9, vec3(0x3f000000u, 0xbf800000u, 0x41100000u)};
    pair(r, 29, text("Human\0skip", 10));
    r.push_back(number(1));
    r.push_back({3, half(7)});
    r.push_back(string(text("ship\0xx", 7)));
    r.push_back(number(0xdeadbeefu));
    r.push_back({9, vec3(0x3f800000u, 0x40000000u, 0x40400000u)});
    r.push_back(number(0xffffffffu));
    Bytes instance(40, 0);
    const auto name = text("I\0ignored", 9);
    std::copy(name.begin(), name.end(), instance.begin());
    r.push_back(string(instance));
    r.push_back({8, word(0xaabbccddu)});
    Bytes transform;
    for (std::uint32_t i = 0; i < 12; ++i) {
        const auto bits = word(0x3f800000u + i);
        transform.insert(transform.end(), bits.begin(), bits.end());
    }
    r.push_back({11, transform});
    Bytes runtime(40, 0);
    const auto marker = text("EmptyMission", 12);
    std::copy(marker.begin(), marker.end(), runtime.begin());
    r.push_back(string(runtime));
    return r;
}
std::size_t record_offset(const Records& records, std::size_t index,
                          const std::string& version = "3021") {
    std::size_t offset = std::string("version [1] =\r\n").size() + version.size() +
                         std::string("\r\nBinaryMode [1] =\r\ntrue\r\n").size();
    for (std::size_t i = 0; i < index; ++i) offset += 8 + records[i].data.size();
    return offset;
}
Bytes stream(const Records& records, const std::string& version = "3021") {
    const std::string header = "version [1] =\r\n" + version +
                               "\r\nBinaryMode [1] =\r\ntrue\r\n";
    Bytes out(header.begin(), header.end());
    for (const auto& r : records) {
        for (auto value : {r.tag, static_cast<std::uint32_t>(r.data.size())}) {
            const auto bytes = word(value);
            out.insert(out.end(), bytes.begin(), bytes.end());
        }
        out.insert(out.end(), r.data.begin(), r.data.end());
    }
    return out;
}
void hex(std::ostringstream& out, const std::string& value) {
    out << value.size() << ':' << std::hex << std::setfill('0');
    for (unsigned char c : value) out << std::setw(2) << static_cast<unsigned>(c);
    out << std::dec << ';';
}
std::uint32_t bits(float value) {
    std::uint32_t result;
    std::memcpy(&result, &value, 4);
    return result;
}
void vec(std::ostringstream& out, const a2fo::sol::Vec3& value) {
    out << bits(value.x) << ',' << bits(value.y) << ',' << bits(value.z) << ';';
}
std::string report(const Bytes& input) {
    a2fo::sol::Map map;
    map.version = 99;
    map.title = "sentinel";
    map.start_locations.push_back({{}, "sentinel"});
    map.objects.push_back({});
    std::string error = "old-error";
    const bool result = a2fo::sol::parse_bytes(input, map, &error);
    std::ostringstream out;
    out << result << '|' << map.version << '|';
    hex(out, error);
    for (const auto* field : {&map.game_type, &map.guid, &map.title,
                              &map.source_path, &map.filename, &map.mission_script}) hex(out, *field);
    vec(out, map.minimum_extent);
    vec(out, map.size);
    out << map.object_count_record_offset << ',' << map.runtime_mission_record_offset << '|';
    out << map.start_locations.size() << ':';
    for (const auto& start : map.start_locations) { vec(out, start.position); hex(out, start.type); }
    out << map.objects.size() << ':';
    for (const auto& object : map.objects) {
        hex(out, object.odf);
        out << object.id << ',' << object.team << ',' << object.opaque_id << ';';
        vec(out, object.position);
        hex(out, object.instance_name);
        for (float f : object.transform) out << bits(f) << ',';
    }
    return out.str();
}
void check(const std::string& label, const Bytes& bytes) {
    std::cout << label << '|' << report(bytes) << '\n';
}
}  // namespace

int main() {
    const Records valid = valid_records();
    {
        a2fo::sol::Map map;
        std::string error;
        assert(a2fo::sol::parse_bytes(stream(valid), map, &error));
        assert(map.version == 3021 && map.game_type == "Game" && map.title == "T");
        assert(map.start_locations.size() == 1 && map.objects.size() == 1);
        assert(map.objects[0].id == 0xdeadbeefu && map.objects[0].team == -1);
        assert(bits(map.minimum_extent.x) == 0x7fc01234u);
        assert(map.object_count_record_offset == record_offset(valid, 40));
        assert(map.runtime_mission_record_offset == record_offset(valid, 49));
    }
    check("valid", stream(valid));
    check("empty", {});
    check("version-whitespace", stream(valid, "  +3021junk"));
    check("version-nul", stream(valid, std::string("3021\0junk", 9)));
    check("version-negative", stream(valid, "-1"));
    check("version-overflow", stream(valid, "4294967296"));
    Bytes no_marker = stream(valid);
    const std::string marker_name = "BinaryMode";
    const auto marker_at = std::search(no_marker.begin(), no_marker.end(),
                                       marker_name.begin(), marker_name.end());
    assert(marker_at != no_marker.end());
    *marker_at = 'X';
    check("no-marker", no_marker);
    Bytes short_record = stream(valid);
    short_record.push_back(3);
    check("short-record", short_record);
    Bytes too_long = stream(valid);
    const auto corrupt = word(100);
    const std::size_t last_size = record_offset(valid, valid.size() - 1) + 4;
    std::copy(corrupt.begin(), corrupt.end(), too_long.begin() + last_size);
    check("record-overrun", too_long);
    Records bad = valid;
    bad[6] = number(99);
    check("bad-metadata", stream(bad));
    bad = valid; bad[27] = number(65);
    check("bad-start-count", stream(bad));
    bad = valid; bad[30].tag = 4;
    check("bad-start", stream(bad));
    bad = valid; bad[28].tag = 4;
    check("bad-start-position", stream(bad));
    bad = valid; bad[27] = number(3);
    check("truncated-start", stream(bad));
    bad = valid; bad[40] = number(2);
    check("missing-object", stream(bad));
    bad = valid; bad[41] = {3, half(0)};
    check("no-object-table", stream(bad));
    bad = valid; bad[42] = string(Bytes(7, 0));
    check("empty-odf", stream(bad));

    // Every byte boundary in the valid stream, including tagged headers and
    // payloads. This also checks which fields survive each failed parse.
    const Bytes complete = stream(valid);
    for (std::size_t length = 0; length <= complete.size(); ++length) {
        Bytes truncated(complete.begin(), complete.begin() + length);
        check("truncate-" + std::to_string(length), truncated);
    }

    // Change every record's tag and declared byte length without constructing
    // large payloads. These exercise record framing and validation order.
    for (std::size_t index = 0; index < valid.size(); ++index) {
        const std::size_t offset = record_offset(valid, index);
        for (std::uint32_t tag : {0u, 2u, 4u, 9u, 11u, 0xffffffffu}) {
            Bytes changed = complete;
            const Bytes encoded = word(tag);
            std::copy(encoded.begin(), encoded.end(), changed.begin() + offset);
            check("tag-" + std::to_string(index) + '-' + std::to_string(tag), changed);
        }
        for (std::uint32_t length : {0u, 1u, 7u, 12u, 40u, 48u, 0xffffffffu}) {
            Bytes changed = complete;
            const Bytes encoded = word(length);
            std::copy(encoded.begin(), encoded.end(), changed.begin() + offset + 4);
            check("size-" + std::to_string(index) + '-' + std::to_string(length), changed);
        }
    }

    // A few direct count and scan-policy changes retain valid tagged framing.
    for (std::uint32_t count : {0u, 2u, 3u, 64u, 65u, 100001u, 0xffffffffu}) {
        bad = valid; bad[27] = number(count);
        check("start-count-" + std::to_string(count), stream(bad));
        bad = valid; bad[40] = number(count);
        check("object-count-" + std::to_string(count), stream(bad));
    }
    bad = valid;
    bad.insert(bad.begin() + 40, {4, word(0)});
    check("unknown-before-object", stream(bad));
    bad = valid;
    bad.insert(bad.begin() + 49, {17, Bytes{1, 2, 3}});
    check("unknown-after-object", stream(bad));
    bad = valid;
    bad.push_back({0, {}});
    check("unknown-trailing-empty", stream(bad));

    // Bounded byte mutations sample scalar bits, strings, opaque records, and
    // tag framing without depending on a random-device seed.
    std::uint32_t state = 0x51a1f00du;
    for (std::size_t i = 0; i < 256; ++i) {
        state = state * 1664525u + 1013904223u;
        const std::size_t offset = state % complete.size();
        state = state * 1664525u + 1013904223u;
        Bytes changed = complete;
        changed[offset] ^= static_cast<std::uint8_t>(1u + (state & 255u));
        check("byte-" + std::to_string(i) + '-' + std::to_string(offset), changed);
    }
    return 0;
}
