#include "../modules/A2FOMissionSelector/module.cpp"

#include <cassert>
#include <string>

extern "C" HWND A2FO_CALL a2fo_mission_selector_call_thiscall_0(void*, void*) {
    return nullptr;
}

int main() {
    SelectorContext context;
    CampaignEntry campaign;
    campaign.menu_view = "campaign-custom";
    MissionEntry mission;
    mission.menu_view = "mission-custom";
    campaign.missions.push_back(mission);
    context.campaigns.push_back(campaign);

    assert(selected_menu_view(context) == "mission-custom");
    context.campaigns[0].missions[0].menu_view.clear();
    assert(selected_menu_view(context) == "campaign-custom");
    context.campaigns[0].menu_view.clear();
    assert(selected_menu_view(context).empty());

    char text[5]{};
    copy_scene_text(text, std::string("abcdef"));
    assert(std::string(text) == "abcd");
    copy_scene_text(text, std::string{});
    assert(text[0] == '\0');
}
