#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "../modules/A2FORefitMenus/native_model.hpp"
#include "../modules/A2FORefitMenus/interaction.hpp"

using namespace a2fo::refit_menu;

namespace {

struct Memory {
    std::map<std::uintptr_t, std::vector<std::uint8_t>> blocks;

    void add(std::uintptr_t address, std::size_t size) {
        assert(blocks.emplace(address, std::vector<std::uint8_t>(size)).second);
    }

    bool operator()(std::uintptr_t address, void* output, std::size_t size) const {
        auto it = blocks.upper_bound(address);
        if (it == blocks.begin()) return false;
        --it;
        const std::uintptr_t offset = address - it->first;
        if (offset > it->second.size() || size > it->second.size() - offset) return false;
        std::memcpy(output, it->second.data() + offset, size);
        return true;
    }

    template<class T>
    void put(std::uintptr_t address, T value) {
        auto it = blocks.upper_bound(address);
        assert(it != blocks.begin());
        --it;
        const std::uintptr_t offset = address - it->first;
        assert(offset <= it->second.size());
        assert(sizeof(value) <= it->second.size() - offset);
        std::memcpy(it->second.data() + offset, &value, sizeof(value));
    }

    void bytes(std::uintptr_t address, const std::string& value) {
        for (std::size_t i = 0; i < value.size(); ++i)
            put<std::uint8_t>(address + i, static_cast<std::uint8_t>(value[i]));
    }
};

struct Fixture {
    static constexpr std::uintptr_t form = 0x60000000u;
    static constexpr std::uintptr_t image_size = 0x200000u;
    static constexpr std::uintptr_t action_base = 0x60001000u;
    static constexpr std::uintptr_t visual_base = 0x60002000u;
    static constexpr std::uintptr_t caption_base = 0x60003000u;

    std::uintptr_t image;
    Memory memory;

    explicit Fixture(std::uintptr_t loaded_image = 0x5a800000u) : image(loaded_image) {
        memory.add(form, native::form_size);
        memory.put<std::uint32_t>(form, static_cast<std::uint32_t>(image + native::main_vmt_rva));
        for (std::size_t i = 0; i < entry_count; ++i) {
            const auto action = action_at(i);
            const auto caption = caption_at(i);
            memory.add(action, 0x90);
            memory.put<std::uint32_t>(form + native::action_fields[i],
                                      static_cast<std::uint32_t>(action));
            memory.put<std::uint32_t>(action,
                static_cast<std::uint32_t>(image + 0x2000 + i * 4));
            memory.put<std::uint8_t>(action + native::action_visible_offset, 1);
            memory.put<std::uint8_t>(action + native::action_enabled_offset, 1);
            const std::string text = i == 0 ? "Localized launch" :
                i == 5 ? std::string("Cr\xe9" "dits") :
                default_snapshot().entries[i].caption;
            memory.add(caption - 4, text.size() + 4);
            memory.put<std::int32_t>(caption - 4, static_cast<std::int32_t>(text.size()));
            memory.bytes(caption, text);
            memory.put<std::uint32_t>(action + native::action_caption_offset,
                                      static_cast<std::uint32_t>(caption));
        }
        for (std::size_t i = 0; i < native::visual_fields.size(); ++i) {
            const auto control = visual_at(i);
            memory.add(control, 0x60);
            memory.put<std::uint32_t>(form + native::visual_fields[i],
                                      static_cast<std::uint32_t>(control));
            memory.put<std::uint32_t>(control,
                static_cast<std::uint32_t>(image + 0x3000 + i * 4));
            memory.put<std::uint32_t>(control + 4, static_cast<std::uint32_t>(form));
            memory.put<std::uint32_t>(control + 0x30, static_cast<std::uint32_t>(form));
            memory.put<std::uint8_t>(control + native::visible_offset,
                                     static_cast<std::uint8_t>(i % 2));
        }
    }

    static constexpr std::uintptr_t action_at(std::size_t i) {
        return action_base + i * 0x100;
    }
    static constexpr std::uintptr_t visual_at(std::size_t i) {
        return visual_base + i * 0x100;
    }
    static constexpr std::uintptr_t caption_at(std::size_t i) {
        return caption_base + i * 0x100 + 4;
    }
};

void assert_rejected_unchanged(Fixture& fixture) {
    Snapshot output = default_snapshot();
    output.entries[0].caption = "SENTINEL";
    output.version = "OLD VERSION";
    std::array<native::Visual, 9> visuals{};
    visuals[0] = {0x1234, 1};
    assert(!native::model(fixture.memory, Fixture::form, fixture.image,
                          Fixture::image_size, output, &visuals));
    assert(output.entries[0].caption == "SENTINEL");
    assert(output.version == "OLD VERSION");
    assert(visuals[0].control == 0x1234 && visuals[0].visible == 1);
}

void test_capture() {
    Fixture fixture;
    fixture.memory.put<std::uint8_t>(Fixture::action_at(0) +
                                     native::action_visible_offset, 0);
    fixture.memory.put<std::uint8_t>(Fixture::action_at(0) +
                                     native::action_enabled_offset, 0);
    Snapshot output;
    std::array<native::Visual, 9> visuals{};
    assert(native::model(fixture.memory, Fixture::form, fixture.image,
                         Fixture::image_size, output, &visuals));
    assert(output.entries[0].caption == "Localized launch");
    assert(!output.entries[0].visible && !output.entries[0].enabled);
    assert(output.entries[5].caption == std::string("Cr\xe9" "dits"));
    assert(output.entries[6].caption == "EXIT");
    assert(output.version == "FLEET OPERATIONS / A2FO EXTENSIONS");
    for (std::size_t i = 0; i < visuals.size(); ++i) {
        assert(visuals[i].control == Fixture::visual_at(i));
        assert(visuals[i].visible == i % 2);
    }

    Fixture relocated(0x70000000u);
    assert(native::model(relocated.memory, Fixture::form, relocated.image,
                         Fixture::image_size, output, &visuals));
    assert(output.entries[0].caption == "Localized launch");
}

void test_rejections() {
    {
        Fixture fixture;
        fixture.memory.put<std::uint32_t>(Fixture::form,
            static_cast<std::uint32_t>(fixture.image + native::main_vmt_rva + 4));
        assert_rejected_unchanged(fixture);
    }
    {
        Fixture fixture;
        fixture.memory.put<std::uint32_t>(Fixture::action_at(0),
            static_cast<std::uint32_t>(fixture.image + Fixture::image_size));
        assert_rejected_unchanged(fixture);
    }
    {
        Fixture fixture;
        fixture.memory.put<std::uint8_t>(Fixture::action_at(0) +
                                        native::action_enabled_offset, 2);
        assert_rejected_unchanged(fixture);
    }
    {
        Fixture fixture;
        fixture.memory.put<std::int32_t>(Fixture::caption_at(0) - 4, 513);
        assert_rejected_unchanged(fixture);
    }
    {
        Fixture fixture;
        fixture.memory.put<std::uint32_t>(Fixture::visual_at(8) + 4, 0);
        assert_rejected_unchanged(fixture);
    }
    {
        Fixture fixture;
        fixture.memory.put<std::uint32_t>(Fixture::visual_at(8) + 0x30, 0);
        assert_rejected_unchanged(fixture);
    }
    {
        Fixture fixture;
        fixture.memory.put<std::uint32_t>(Fixture::form + native::action_fields[6], 0);
        assert_rejected_unchanged(fixture);
    }
    {
        Fixture fixture;
        fixture.memory.blocks.erase(Fixture::caption_at(6) - 4);
        assert_rejected_unchanged(fixture);
    }
}

void test_interaction() {
    Snapshot snapshot = default_snapshot();
    const Layout geometry = layout(800, 600, snapshot);
    const auto center = [&](int row) {
        const Rect& r = geometry.rows[static_cast<std::size_t>(row)];
        return std::array<int, 2>{{r.x + r.width / 2, r.y + r.height / 2}};
    };
    const auto first = center(0);
    const auto second = center(1);
    Interaction input;
    input.move(geometry, snapshot, first[0], first[1]);
    assert(input.hot == 0);
    input.down(geometry, snapshot, first[0], first[1]);
    assert(input.pressed == 0);
    assert(input.up(geometry, snapshot, first[0], first[1]));
    assert(!input.up(geometry, snapshot, first[0], first[1]));
    assert(!input.request(snapshot, 1)); // only one pending action
    assert(input.take(snapshot) == 0);
    assert(input.take(snapshot) == -1);

    snapshot.entries[0].enabled = false;
    input.down(geometry, snapshot, first[0], first[1]);
    assert(input.pressed == -1);
    assert(!input.up(geometry, snapshot, first[0], first[1]));
    snapshot.entries[0].enabled = true;
    snapshot.entries[1].visible = false;
    input.down(geometry, snapshot, second[0], second[1]);
    assert(input.pressed == -1);
    assert(!input.up(geometry, snapshot, second[0], second[1]));
    snapshot.entries[1].visible = true;

    input.down(geometry, snapshot, first[0], first[1]);
    input.move(geometry, snapshot, second[0], second[1]);
    assert(input.pressed == -1);
    assert(!input.up(geometry, snapshot, second[0], second[1]));
    input.down(geometry, snapshot, first[0], first[1]);
    input.leave();
    assert(input.hot == -1 && input.pressed == -1);
    assert(!input.up(geometry, snapshot, first[0], first[1]));

    assert(input.request(snapshot, 1));
    snapshot.entries[1].enabled = false;
    assert(input.take(snapshot) == -1);
    snapshot.entries[1].enabled = true;
    assert(input.request(snapshot, 1));
    snapshot.entries[1].visible = false;
    assert(input.take(snapshot) == -1);
    assert(!input.request(snapshot, 1));
    assert(!input.request(snapshot, -1));
    assert(!input.request(snapshot, static_cast<int>(entry_count)));

    snapshot.entries[0].enabled = false;
    assert(next_enabled(snapshot, -1, 1) == 2);
    assert(next_enabled(snapshot, 2, -1) == 6);
    assert(next_enabled(snapshot, 6, 1) == 2);
}

} // namespace

int main() {
    test_capture();
    test_rejections();
    test_interaction();
}
