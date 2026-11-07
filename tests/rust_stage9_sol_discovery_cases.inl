// Calls the actual source dispatch. Expected virtual-mask decisions below are
// fixed from the frozen C++ behavior; the native trace supplies the observation.
void discovery_scenarios() {
    static_assert(sizeof(LONG) == sizeof(std::int32_t));
    using Source = std::pair<const char*, std::string>;
    const auto saved_sources = g_sources;
    const LONG saved_count = InterlockedCompareExchange(&g_discovery_dispatch_count, 0, 0);
    const HMODULE saved_armada = g_armada;
    std::array<std::uint8_t, 16> known_maps{};
    const auto run = [&](const char* name, LONG seed, LONG expected_count,
                         const char* mask, bool expected_virtual,
                         std::initializer_list<Source> sources,
                         bool have_armada = true, bool have_known_maps = true) {
        g_sources.clear();
        for (const auto& source : sources)
            g_sources[source.first] = {source.second, source.first, 0};
        InterlockedExchange(&g_discovery_dispatch_count, seed);
        g_armada = have_armada ? saved_armada : nullptr;
        std::printf("discovery-begin:%s:seed=%ld:virtual=%u\n", name,
                    static_cast<long>(seed), static_cast<unsigned>(expected_virtual));
        a2fo_sol_add_maps_with_mask_dispatch(
            have_known_maps ? known_maps.data() : nullptr, mask);
        const LONG after = InterlockedCompareExchange(&g_discovery_dispatch_count, 0, 0);
        assert(after == expected_count);
        std::printf("discovery-end:%s:count=%ld\n", name, static_cast<long>(after));
        g_armada = saved_armada;
    };

    run("gate-no-armada", 0, 0, "native.bzn", false,
        {{"first.sol", "R:/maps/bzn/first.sol"}}, false, true);
    run("gate-null-knownmaps", 0, 0, "native.bzn", false,
        {{"first.sol", "R:/maps/bzn/first.sol"}}, true, false);

    // First eight passes log; the ninth does not. InterlockedIncrement wraps
    // the signed 32-bit maximum to the minimum.
    run("seed-six", 6, 7, "native.bzn", false, {});
    run("seed-seven", 7, 8, "native.bzn", false, {});
    run("seed-eight", 8, 9, "native.bzn", false, {});
    run("seed-max", std::numeric_limits<LONG>::max(),
        std::numeric_limits<LONG>::min(), "native.bzn", false, {});

    run("null-mask", 0, 1, nullptr, true,
        {{"first.sol", "R:/maps/bzn/first.sol"}});
    run("empty-mask", 0, 1, "", true,
        {{"first.sol", "R:/maps/bzn/first.sol"}});
    run("nonempty-mask", 0, 1, "native.bzn", true,
        {{"first.sol", "R:/maps/bzn/first.sol"}});

    run("lower-backslash", 0, 1, "native.bzn", true,
        {{"first.sol", "R:\\maps\\bzn\\first.sol"}});
    run("upper-backslash", 0, 1, "native.bzn", true,
        {{"first.sol", "R:\\maps\\BZN\\first.sol"}});
    run("forward-slash", 0, 1, "native.bzn", true,
        {{"first.sol", "R:/maps/bzn/first.sol"}});
    run("mixed-slashes", 0, 1, "native.bzn", true,
        {{"first.sol", "R:/maps\\bZn/first.sol"}});
    run("sol-parent", 0, 1, "native.bzn", false,
        {{"first.sol", "R:/maps/sol/first.sol"}});
    run("other-parent", 0, 1, "native.bzn", false,
        {{"first.sol", "R:/maps/other/first.sol"}});
    run("no-separator-file", 0, 1, "native.bzn", false,
        {{"first.sol", "first.sol"}});
    run("no-separator-bzn", 0, 1, "native.bzn", true,
        {{"first.sol", "bzn"}});
    run("root-slash", 0, 1, "native.bzn", false,
        {{"first.sol", "/first.sol"}});
    run("trailing-slash", 0, 1, "native.bzn", true,
        {{"first.sol", "R:/maps/bzn/"}});

    const std::string nul_before_final_leaf(
        "R:/maps/bzn\0/sol/first.sol", sizeof("R:/maps/bzn\0/sol/first.sol") - 1);
    const std::string nul_inside_parent(
        "R:/maps/sol\0/bzn/first.sol", sizeof("R:/maps/sol\0/bzn/first.sol") - 1);
    run("embedded-nul-before-sol", 0, 1, "native.bzn", false,
        {{"first.sol", nul_before_final_leaf}});
    run("embedded-nul-before-bzn", 0, 1, "native.bzn", true,
        {{"first.sol", nul_inside_parent}});

    // std::map key order is a, b, c. The bzn winner at b stops the scan;
    // the later bzn entry must not cause a second virtual native call.
    run("ordered-first-bzn", 0, 1, "native.bzn", true,
        {{"c.sol", "R:/maps/bzn/c.sol"},
         {"a.sol", "R:/maps/sol/a.sol"},
         {"b.sol", "R:/maps/BZN/b.sol"}});
    run("ordered-no-bzn", 0, 1, "native.bzn", false,
        {{"c.sol", "R:/maps/other/c.sol"},
         {"a.sol", "R:/maps/sol/a.sol"},
         {"b.sol", "R:/maps/sol/b.sol"}});

    const LONG final_count = InterlockedCompareExchange(&g_discovery_dispatch_count, 0, 0);
    std::printf("discovery-final:count=%ld\n", static_cast<long>(final_count));
    g_sources = saved_sources;
    g_armada = saved_armada;
    InterlockedExchange(&g_discovery_dispatch_count, saved_count);
}
