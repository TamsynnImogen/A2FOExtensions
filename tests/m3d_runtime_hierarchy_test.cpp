#include "../modules/A2FOM3D/module.cpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>

#ifdef NDEBUG
#error M3D hierarchy assertions must remain enabled
#endif
static_assert(sizeof(void*) == 4, "M3D hierarchy test requires a 32-bit target");

namespace {

// Pinned ArmadaL node constructor: parent +0x18, first child +0x1c,
// next sibling +0x20. Keep fixture offsets literal so swapped module
// constants cannot make the regression test pass.
struct FakeNode {
    alignas(void*) std::array<std::uint8_t, 0x150> bytes{};
};

struct FakeGeometry {
    alignas(void*) std::array<std::uint8_t, 0x50> bytes{};
};

void store_pointer(std::uint8_t* bytes, std::size_t offset, void* value) {
    std::memcpy(bytes + offset, &value, sizeof(value));
}

void* load_pointer(const std::uint8_t* bytes, std::size_t offset) {
    void* value = nullptr;
    std::memcpy(&value, bytes + offset, sizeof(value));
    return value;
}

void name_node(FakeNode& node, const char* name) {
    store_pointer(node.bytes.data(), 0x08, const_cast<char*>(name));
}

void prepend_child(FakeNode& parent, FakeNode& child) {
    store_pointer(child.bytes.data(), 0x18, parent.bytes.data());
    store_pointer(child.bytes.data(), 0x20,
                  load_pointer(parent.bytes.data(), 0x1c));
    store_pointer(parent.bytes.data(), 0x1c, child.bytes.data());
}

void check_unchanged(
    const std::array<FakeNode*, 5>& nodes,
    const std::array<std::array<std::uint8_t, 0x150>, 5>& original_nodes,
    const FakeGeometry& geometry,
    const std::array<std::uint8_t, 0x50>& original_geometry) {
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        assert(nodes[index]->bytes == original_nodes[index]);
    }
    assert(geometry.bytes == original_geometry);
}

} // namespace

int main() {
    FakeNode root, group_a, group_b, mesh_a, mesh_b;
    FakeGeometry geometry;
    name_node(root, "root");
    name_node(group_a, "group_a");
    name_node(group_b, "group_b");
    name_node(mesh_a, "mesh_a");
    name_node(mesh_b, "mesh_b");
    prepend_child(root, group_a);
    prepend_child(root, group_b);
    prepend_child(group_a, mesh_a);
    prepend_child(group_b, mesh_b);
    store_pointer(geometry.bytes.data(), 0x3c, root.bytes.data());

    // The second root child is a sibling of group_b; each mesh is a
    // grandchild of root under its intended group.
    assert(load_pointer(root.bytes.data(), 0x1c) == group_b.bytes.data());
    assert(load_pointer(group_b.bytes.data(), 0x20) == group_a.bytes.data());
    assert(load_pointer(group_b.bytes.data(), 0x1c) == mesh_b.bytes.data());
    assert(load_pointer(group_a.bytes.data(), 0x1c) == mesh_a.bytes.data());
    assert(load_pointer(mesh_a.bytes.data(), 0x18) == group_a.bytes.data());
    assert(load_pointer(mesh_b.bytes.data(), 0x18) == group_b.bytes.data());

    const std::array<FakeNode*, 5> nodes{
        &root, &group_a, &group_b, &mesh_a, &mesh_b};
    const std::array<std::array<std::uint8_t, 0x150>, 5> original_nodes{
        root.bytes, group_a.bytes, group_b.bytes, mesh_a.bytes, mesh_b.bytes};
    const auto original_geometry = geometry.bytes;

    a2fo::m3d::ConversionStats conversion;
    conversion.hierarchy_bindings = {
        {"root", "", "root"},
        {"group_a", "root", "group_a"},
        {"group_b", "root", "group_b"},
        {"mesh_a", "group_a", "mesh_a"},
        {"mesh_b", "group_b", "mesh_b"},
    };
    const auto correct = audit_runtime_hierarchy(
        geometry.bytes.data(), conversion, "fixture");
    assert(correct.discovered == 5);
    assert(correct.expected == 5);
    assert(correct.mismatched == 0);
    assert(correct.missing == 0);
    check_unchanged(nodes, original_nodes, geometry, original_geometry);

    // A wrong authored parent and an absent runtime node are reported,
    // without changing a valid native graph to fit the authored metadata.
    conversion.hierarchy_bindings[4].parent_name = "group_a";
    conversion.hierarchy_bindings.push_back({"absent", "root", "absent"});
    const auto invalid = audit_runtime_hierarchy(
        geometry.bytes.data(), conversion, "fixture");
    assert(invalid.discovered == 5);
    assert(invalid.expected == 6);
    assert(invalid.mismatched == 1);
    assert(invalid.missing == 1);
    check_unchanged(nodes, original_nodes, geometry, original_geometry);

    std::cout << "m3d runtime hierarchy audit: passed\n";
}
