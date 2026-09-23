// Exercise the actual class-loaded adapter, including recursive member loads.
std::array<std::uint8_t, 0x200> cost_fighter{}, cost_bomber{}, cost_squad{};
bool lazy_cost_members = false;

void dispatch_cost_class(void* klass, const char* source, const OdfFields& fields) {
    std::vector<A2FO_OdfFieldView> views;
    for (const auto& field : fields)
        views.push_back({{field.first.data(), static_cast<std::uint32_t>(field.first.size())},
            {field.second.data(), static_cast<std::uint32_t>(field.second.size())}});
    A2FO_GameObjectClassLoadedEvent event{};
    event.struct_size = sizeof(event);
    event.object_class = klass;
    event.source_odf = {source, static_cast<std::uint32_t>(std::strlen(source))};
    event.odf_fields = views.data();
    event.odf_field_count = views.size();
    class_loaded_handler(&event, nullptr);
}

void* __cdecl cost_find_class(const char* odf) {
    const bool fighter = std::strcmp(odf, "cost_fighter") == 0;
    const bool bomber = std::strcmp(odf, "cost_bomber") == 0;
    if (!fighter && !bomber) return nullptr;
    void* klass = fighter ? cost_fighter.data() : cost_bomber.data();
    if (lazy_cost_members && !g_classes_by_odf.count(odf)) {
        // More callbacks while resolving a squad must not invalidate its data.
        dispatch_cost_class(klass, odf, {{"classLabel", "craft"},
            {"TRITANIUMCOST", fighter ? "3" : "11"},
            {"supplyCost", fighter ? "4" : "13"},
            {"creditsCost", fighter ? "5" : "17"},
            {"collectiveconnectionsCost", fighter ? "6" : "19"}});
    }
    return klass;
}

void economics_native_tests() {
    g_registry = Registry{};
    g_definitions.clear();
    g_classes_by_odf.clear();
    g_classlabels_by_odf.clear();
    g_member_economics.clear();
    g_squad_economics.clear();
    const std::array<std::int32_t, 6> fighter{{10, 2, 55, 0, 0, 0}};
    const std::array<std::int32_t, 6> bomber{{20, 3, 100, 5, 7, 9}};
    std::memcpy(cost_fighter.data() + kClassNativeCostsOffset, fighter.data(), sizeof(fighter));
    std::memcpy(cost_bomber.data() + kClassNativeCostsOffset, bomber.data(), sizeof(bomber));
    put<float>(cost_fighter.data(), kClassBuildTimeOffset, 6);
    put<float>(cost_bomber.data(), kClassBuildTimeOffset, 12.5f);
    const auto fighter_before = cost_fighter, bomber_before = cost_bomber;
    cost_squad.fill(0);
    put<float>(cost_squad.data(), kClassBuildTimeOffset, 999);
    put<std::int32_t>(cost_squad.data(), kClassNativeCostsOffset + 8, 1);
    jump_to(at(kGameObjectClassFindRva), reinterpret_cast<void*>(&cost_find_class));
    lazy_cost_members = true;
    dispatch_cost_class(cost_squad.data(), "cost_patrol.odf", {
        {"classLabel", "squadron"}, {"buildTime", "999"}, {"dilithiumCost", "1"},
        {"squadMember0", "cost_fighter"}, {"squadMemberCount0", "3"},
        {"squadMember5", "cost_bomber"}, {"squadMemberCount5", "2"}});
    assert(g_squad_economics.count(cost_squad.data()));
    assert(read_at<float>(cost_squad.data(), kClassBuildTimeOffset) == 43);
    assert(read_at<std::int32_t>(cost_squad.data(), kClassNativeCostsOffset + 8) == 365);
    assert(read_at<std::int32_t>(cost_squad.data(), kClassNativeCostsOffset) == 70);
    assert(read_at<std::int32_t>(cost_squad.data(), kClassNativeCostsOffset + 4) == 12);
    assert(cost_fighter == fighter_before && cost_bomber == bomber_before);
    std::array<std::int32_t, 4> added{};
    assert(A2FOSquadrons_GetAdditionalCosts(cost_squad.data(), added.data(), 4));
    const std::array<std::int32_t, 4> expected{{31, 38, 49, 56}};
    assert(added == expected);
    assert(!A2FOSquadrons_GetAdditionalCosts(cost_fighter.data(), added.data(), 4));
    assert(!A2FOSquadrons_GetAdditionalCosts(cost_squad.data(), added.data(), 3));
    assert(!A2FOSquadrons_GetAdditionalCosts(cost_squad.data(), nullptr, 4));
    const auto squad_before = cost_squad;
    // Missing members fail without publishing a partial/zero price.
    dispatch_cost_class(cost_squad.data(), "cost_patrol.odf", {
        {"classLabel", "squadron"}, {"squadMember0", "missing"}, {"squadMemberCount0", "3"}});
    assert(cost_squad == squad_before && !g_squad_economics.count(cost_squad.data()));
    A2FO_ProducerEvent admission{};
    admission.struct_size = sizeof(admission);
    admission.kind = A2FO_PRODUCER_EVENT_ADMIT;
    admission.producer = yard.data();
    admission.target_class = cost_squad.data();
    assert(!producer_event_handler(&admission, nullptr));
    g_definitions.clear();
    g_squad_economics.clear();
    g_member_economics.clear();
    g_classes_by_odf.clear();
    lazy_cost_members = false;
    std::cout << "PASS native squad economics: recursive member loading, count weighting, all ten resources, ordinary classes unchanged, failed admission\n";
}
