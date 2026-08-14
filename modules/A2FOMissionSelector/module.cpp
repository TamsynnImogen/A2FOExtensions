/*
 * File: modules/A2FOMissionSelector/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Replace Armada II's fixed single-player campaign and mission
 *          dialogs while retaining its native mission launch/progression path.
 */

#define NOMINMAX

#include "../../sdk/include/a2fo_module_api.h"

#include <windows.h>
#include <commctrl.h>
#include <objidl.h>
#include <gdiplus.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr const char* kModuleName = "A2FOMissionSelector";
constexpr std::uintptr_t kDoMissionSelectRva = 0x001d6d50;
constexpr std::uintptr_t kCampaignAvailableRva = 0x001dbd60;
constexpr std::uintptr_t kDoSingleRva = 0x001dbd90;
constexpr std::uintptr_t kSetupMissionRva = 0x001dcc00;
constexpr std::uintptr_t kShellStateRva = 0x003a8980;
constexpr std::uintptr_t kCurrentCampaignRva = 0x003a89a4;
constexpr std::uintptr_t kSelectedMissionRva = 0x003a89ac;
constexpr std::uintptr_t kCampaignMissionLimitRva = 0x003a89ad;
constexpr std::uintptr_t kTutorialMissionCountRva = 0x003a89b5;
constexpr std::uintptr_t kMissionFilenameTableRva = 0x003a8afc;
constexpr std::size_t kCampaignCount = 4;
constexpr std::size_t kMissionsPerCampaign = 10;
constexpr int kMaximumConfiguredCampaign = 127;
constexpr int kMaximumConfiguredMission = 511;
constexpr std::size_t kMaximumMissionName = 260;
constexpr std::streamoff kMaximumMetadataSize = 1024 * 1024;

constexpr std::array<std::uint8_t, 6> kExpectedDoSingle{
    0x8b, 0x0d, 0x08, 0xd5, 0x7a, 0x00};
constexpr std::array<std::uint8_t, 6> kExpectedDoMissionSelect{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::array<std::uint8_t, 6> kExpectedCampaignAvailable{
    0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08};
constexpr std::array<std::uint8_t, 6> kExpectedSetupMission{
    0x55, 0x8b, 0xec, 0x6a, 0xff, 0x68};

constexpr int kCampaignListId = 2101;
constexpr int kMissionListId = 2102;
constexpr int kOverviewId = 2103;
constexpr int kDetailsId = 2104;
constexpr int kPreviewId = 2105;
constexpr int kBackId = 2106;
constexpr int kStartId = IDOK;
constexpr std::int32_t kMainMenuShellState = 1;

const A2FO_ModuleApi* g_api = nullptr;
HMODULE g_armada = nullptr;
A2FO_InlineHook g_do_single_hook{};
A2FO_InlineHook g_do_mission_select_hook{};
volatile LONG g_accept_selected_mission = 0;
ULONG_PTR g_gdiplus_token = 0;
HFONT g_regular_font = nullptr;
HFONT g_heading_font = nullptr;
HBRUSH g_background_brush = nullptr;
HBRUSH g_field_brush = nullptr;
std::vector<std::string> g_roots;

template <typename T = void>
T* at(HMODULE module, std::uintptr_t rva) noexcept {
    return reinterpret_cast<T*>(
        reinterpret_cast<std::uint8_t*>(module) + rva);
}

void log_line(const std::string& message) noexcept {
    if (g_api && g_api->log) g_api->log(kModuleName, message.c_str());
}

bool readable_range(const void* address, std::size_t size) noexcept {
    if (!address || size == 0) return false;
    MEMORY_BASIC_INFORMATION information{};
    if (VirtualQuery(address, &information, sizeof(information)) !=
            sizeof(information) || information.State != MEM_COMMIT ||
        (information.Protect & (PAGE_GUARD | PAGE_NOACCESS)) != 0) {
        return false;
    }
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(address);
    const std::uintptr_t end = reinterpret_cast<std::uintptr_t>(
        information.BaseAddress) + information.RegionSize;
    return start <= end && size <= end - start;
}

template <std::size_t Size>
bool signature_matches(
    std::uintptr_t rva,
    const std::array<std::uint8_t, Size>& signature) noexcept {
    const void* address = at(g_armada, rva);
    return readable_range(address, signature.size()) &&
        std::memcmp(address, signature.data(), signature.size()) == 0;
}

std::string trim(std::string value) {
    const auto whitespace = [](unsigned char character) {
        return std::isspace(character) != 0;
    };
    value.erase(value.begin(), std::find_if_not(
        value.begin(), value.end(), whitespace));
    value.erase(std::find_if_not(
        value.rbegin(), value.rend(), whitespace).base(), value.end());
    if (value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (value.front() == '\'' && value.back() == '\''))) {
        value = value.substr(1, value.size() - 2);
    }
    return value;
}

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char character) {
                       return static_cast<char>(std::tolower(character));
                   });
    return value;
}

std::string join_path(const std::string& left, const std::string& right) {
    if (left.empty()) return right;
    if (right.empty()) return left;
    if (left.back() == '\\' || left.back() == '/') return left + right;
    return left + "\\" + right;
}

bool regular_file(const std::string& path) noexcept {
    const DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::string decode_escapes(const std::string& input) {
    std::string output;
    output.reserve(input.size());
    for (std::size_t index = 0; index < input.size(); ++index) {
        if (input[index] == '\\' && index + 1 < input.size()) {
            const char next = input[index + 1];
            if (next == 'n') {
                output.push_back('\r');
                output.push_back('\n');
                ++index;
                continue;
            }
            if (next == 't') {
                output.push_back('\t');
                ++index;
                continue;
            }
        }
        output.push_back(input[index]);
    }
    return output;
}

using Metadata = std::unordered_map<std::string, std::string>;

void merge_metadata_file(const std::string& path, Metadata& metadata) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return;
    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    if (size < 0 || size > kMaximumMetadataSize) {
        log_line("Ignored oversized mission selector metadata: " + path);
        return;
    }
    input.seekg(0, std::ios::beg);
    std::string section;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        line = trim(line);
        if (line.empty() || line.front() == ';' || line.front() == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = lower(trim(line.substr(1, line.size() - 2)));
            continue;
        }
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos) continue;
        const std::string key = lower(trim(line.substr(0, equals)));
        if (key.empty()) continue;
        metadata[section + "." + key] =
            decode_escapes(trim(line.substr(equals + 1)));
    }
}

Metadata load_metadata() {
    Metadata metadata;
    for (const std::string& root : g_roots) {
        merge_metadata_file(join_path(root, "mission_selector.ini"), metadata);
        merge_metadata_file(
            join_path(join_path(root, "misc"), "mission_selector.ini"),
            metadata);
    }
    return metadata;
}

std::string metadata_value(
    const Metadata& metadata, const std::string& section,
    const std::string& key, const std::string& fallback = {}) {
    const auto found = metadata.find(lower(section + "." + key));
    return found != metadata.end() ? found->second : fallback;
}

bool metadata_bool(
    const Metadata& metadata, const std::string& section,
    const std::string& key, bool fallback) {
    const std::string value = lower(metadata_value(metadata, section, key));
    if (value.empty()) return fallback;
    if (value == "1" || value == "true" || value == "yes" ||
        value == "on") {
        return true;
    }
    if (value == "0" || value == "false" || value == "no" ||
        value == "off") {
        return false;
    }
    return fallback;
}

int metadata_integer(
    const Metadata& metadata, const std::string& section,
    const std::string& key, int fallback, int minimum, int maximum) {
    const std::string value = metadata_value(metadata, section, key);
    if (value.empty()) return fallback;
    char* end = nullptr;
    const long parsed = std::strtol(value.c_str(), &end, 10);
    if (!end || end == value.c_str() || *end != '\0' ||
        parsed < minimum || parsed > maximum) {
        return fallback;
    }
    return static_cast<int>(parsed);
}

bool indexed_key(
    const std::string& value, const std::string& prefix,
    int maximum, int& index, std::size_t& end) {
    if (value.rfind(prefix, 0) != 0) return false;
    std::size_t cursor = prefix.size();
    if (cursor >= value.size() || !std::isdigit(
            static_cast<unsigned char>(value[cursor]))) {
        return false;
    }
    unsigned parsed = 0;
    while (cursor < value.size() && std::isdigit(
               static_cast<unsigned char>(value[cursor]))) {
        parsed = parsed * 10u + static_cast<unsigned>(value[cursor] - '0');
        if (parsed > static_cast<unsigned>(maximum)) return false;
        ++cursor;
    }
    index = static_cast<int>(parsed);
    end = cursor;
    return true;
}

std::set<int> configured_campaign_indices(const Metadata& metadata) {
    std::set<int> indices;
    for (const auto& item : metadata) {
        int index = 0;
        std::size_t end = 0;
        if (indexed_key(
                item.first, "campaign", kMaximumConfiguredCampaign,
                index, end) && end < item.first.size() &&
            item.first[end] == '.') {
            indices.insert(index);
        }
    }
    return indices;
}

std::set<int> configured_mission_indices(
    const Metadata& metadata, int campaign) {
    std::set<int> indices;
    const std::string prefix =
        "campaign" + std::to_string(campaign) + ".mission";
    for (const auto& item : metadata) {
        int index = 0;
        std::size_t end = 0;
        if (indexed_key(
                item.first, prefix, kMaximumConfiguredMission,
                index, end) && end < item.first.size() &&
            item.first[end] == '.') {
            indices.insert(index);
        }
    }
    return indices;
}

std::string safe_string(const char* value) noexcept {
    if (!value) return {};
    std::string result;
    result.reserve(64);
    for (std::size_t index = 0; index < kMaximumMissionName; ++index) {
        if (!readable_range(value + index, 1)) return {};
        const char character = value[index];
        if (character == '\0') return result;
        const unsigned char byte = static_cast<unsigned char>(character);
        if (byte < 0x20 && character != '\t') return {};
        result.push_back(character);
    }
    return {};
}

std::string file_stem(std::string value) {
    const std::size_t slash = value.find_last_of("\\/");
    if (slash != std::string::npos) value.erase(0, slash + 1);
    const std::size_t dot = value.find_last_of('.');
    if (dot != std::string::npos) value.erase(dot);
    return value;
}

std::string humanize_mission_name(const std::string& filename) {
    std::string value = file_stem(filename);
    std::string lowered = lower(value);
    const std::array<std::pair<const char*, const char*>, 4> stock_prefixes{{
        {"a2_tutorial", "Tutorial "},
        {"a2_fed", "Federation Mission "},
        {"a2_kling", "Klingon Mission "},
        {"a2_borg", "Borg Mission "},
    }};
    for (const auto& prefix : stock_prefixes) {
        const std::size_t length = std::strlen(prefix.first);
        if (lowered.rfind(prefix.first, 0) == 0) {
            std::string number = value.substr(length);
            while (number.size() > 1 && number.front() == '0') {
                number.erase(number.begin());
            }
            return std::string(prefix.second) + number;
        }
    }

    std::string output;
    output.reserve(value.size() + 8);
    for (std::size_t index = 0; index < value.size(); ++index) {
        const unsigned char current = static_cast<unsigned char>(value[index]);
        if (current == '_' || current == '-') {
            if (!output.empty() && output.back() != ' ') output.push_back(' ');
            continue;
        }
        if (index != 0) {
            const unsigned char previous =
                static_cast<unsigned char>(value[index - 1]);
            const bool word_boundary =
                (std::islower(previous) && std::isupper(current)) ||
                (!std::isdigit(previous) && std::isdigit(current));
            if (word_boundary && !output.empty() && output.back() != ' ') {
                output.push_back(' ');
            }
        }
        output.push_back(static_cast<char>(current));
    }
    return output.empty() ? filename : output;
}

std::string resolve_asset(const std::string& requested) {
    if (requested.empty()) return {};
    if (regular_file(requested)) return requested;
    for (auto root = g_roots.rbegin(); root != g_roots.rend(); ++root) {
        const std::array<std::string, 3> candidates{{
            join_path(*root, requested),
            join_path(join_path(*root, "bzn"), requested),
            join_path(join_path(*root, "bitmaps"), requested),
        }};
        for (const std::string& candidate : candidates) {
            if (regular_file(candidate)) return candidate;
        }
    }
    return {};
}

std::string resolve_thumbnail(
    const std::string& configured, const std::string& filename) {
    if (!configured.empty()) {
        const std::string resolved = resolve_asset(configured);
        if (!resolved.empty()) return resolved;
    }
    const std::string stem = file_stem(filename);
    for (const char* extension : {".png", ".bmp", ".jpg", ".jpeg"}) {
        const std::string resolved = resolve_asset(stem + extension);
        if (!resolved.empty()) return resolved;
    }
    return {};
}

struct MissionEntry {
    int index = 0;
    int launch_campaign = 0;
    int launch_mission = 0;
    std::string filename;
    std::string title;
    std::string description;
    std::string objectives;
    std::string thumbnail_path;
    bool unlocked = false;
    bool replace_launch_filename = false;
};

struct CampaignEntry {
    int index = 0;
    std::string title;
    std::string overview;
    bool unlocked = false;
    std::vector<MissionEntry> missions;
};

using CampaignAvailableFunction = bool (__cdecl*)(int campaign);

bool campaign_available(int campaign) noexcept {
    const auto function = reinterpret_cast<CampaignAvailableFunction>(
        at(g_armada, kCampaignAvailableRva));
    return function && function(campaign);
}

bool mission_unlocked(int campaign, int mission) noexcept {
    if (mission < 0 || mission >= static_cast<int>(kMissionsPerCampaign)) {
        return false;
    }
    if (campaign == 0) {
        const auto* count = at<std::int8_t>(
            g_armada, kTutorialMissionCountRva);
        if (!readable_range(count, sizeof(*count))) return mission == 0;
        return *count > 0 && mission < *count;
    }
    const auto* limits = at<std::int8_t>(
        g_armada, kCampaignMissionLimitRva);
    if (!readable_range(limits, kCampaignCount)) return mission == 0;
    const int maximum = limits[campaign];
    return maximum >= 0 && mission <= maximum;
}

std::vector<CampaignEntry> build_catalog() {
    static const std::array<const char*, kCampaignCount> default_titles{{
        "Tutorials", "Federation Campaign", "Klingon Campaign",
        "Borg Campaign"}};
    static const std::array<const char*, kCampaignCount> default_overviews{{
        "Learn Armada II's interface, economy, construction, and combat systems.",
        "Command Starfleet through the opening campaign.",
        "Lead the Klingon Empire after completing the Federation campaign.",
        "Direct the Borg campaign after completing the preceding campaigns."}};

    const Metadata metadata = load_metadata();
    auto** filenames = at<const char*>(g_armada, kMissionFilenameTableRva);
    if (!readable_range(
            filenames,
            sizeof(const char*) * kCampaignCount * kMissionsPerCampaign)) {
        return {};
    }

    std::vector<CampaignEntry> campaigns;
    const std::set<int> configured_campaigns =
        configured_campaign_indices(metadata);
    const int highest_campaign = configured_campaigns.empty()
        ? static_cast<int>(kCampaignCount) - 1
        : std::max(
              static_cast<int>(kCampaignCount) - 1,
              *configured_campaigns.rbegin());
    campaigns.reserve(static_cast<std::size_t>(highest_campaign + 1));
    for (int campaign_index = 0;
         campaign_index <= highest_campaign; ++campaign_index) {
        CampaignEntry campaign;
        campaign.index = campaign_index;
        const bool native_campaign =
            campaign_index < static_cast<int>(kCampaignCount);
        campaign.unlocked = native_campaign
            ? campaign_available(campaign.index) : true;
        const std::string section =
            "campaign" + std::to_string(campaign_index);
        if (!native_campaign) {
            campaign.unlocked = metadata_bool(
                metadata, section, "unlocked", campaign.unlocked);
        }
        const std::string fallback_title = native_campaign
            ? default_titles[static_cast<std::size_t>(campaign_index)]
            : "Custom Campaign " + std::to_string(campaign_index - 3);
        const std::string fallback_overview = native_campaign
            ? default_overviews[static_cast<std::size_t>(campaign_index)]
            : "A mod-defined campaign.";
        campaign.title = metadata_value(
            metadata, section, "title", fallback_title);
        campaign.overview = metadata_value(
            metadata, section, "overview", fallback_overview);

        std::set<int> mission_indices =
            configured_mission_indices(metadata, campaign_index);
        if (native_campaign) {
            for (int mission_index = 0;
                 mission_index < static_cast<int>(kMissionsPerCampaign);
                 ++mission_index) {
                mission_indices.insert(mission_index);
            }
        }
        for (const int mission_index : mission_indices) {
            const bool native_slot = native_campaign &&
                mission_index < static_cast<int>(kMissionsPerCampaign);
            const std::size_t row = native_slot
                ? static_cast<std::size_t>(campaign_index) *
                      kMissionsPerCampaign +
                      static_cast<std::size_t>(mission_index)
                : 0;
            const std::string native_filename = native_slot
                ? safe_string(filenames[row]) : std::string{};
            const std::string mission_section = section + ".mission" +
                std::to_string(mission_index);
            const std::string configured_filename = metadata_value(
                metadata, mission_section, "file");
            const std::string filename = configured_filename.empty()
                ? native_filename : configured_filename;
            if (filename.empty()) continue;
            if (filename.size() >= kMaximumMissionName) {
                log_line("Ignored overlong custom mission filename in [" +
                         mission_section + "]");
                continue;
            }
            MissionEntry mission;
            mission.index = mission_index;
            mission.filename = filename;
            mission.launch_campaign = native_slot ? campaign_index :
                metadata_integer(
                    metadata, mission_section, "nativecampaign", 1, 0,
                    static_cast<int>(kCampaignCount) - 1);
            mission.launch_mission = native_slot ? mission_index :
                metadata_integer(
                    metadata, mission_section, "nativemission", 0, 0,
                    static_cast<int>(kMissionsPerCampaign) - 1);
            const bool native_unlocked = native_slot
                ? mission_unlocked(campaign.index, mission.index) : true;
            const bool configured_unlocked = native_slot
                ? native_unlocked
                : metadata_bool(
                      metadata, mission_section, "unlocked", true);
            mission.unlocked = campaign.unlocked && configured_unlocked;
            mission.replace_launch_filename =
                !configured_filename.empty() || !native_slot;
            mission.title = metadata_value(
                metadata, mission_section, "title",
                humanize_mission_name(filename));
            mission.description = metadata_value(
                metadata, mission_section, "description",
                "Native mission file: " + filename);
            mission.objectives = metadata_value(
                metadata, mission_section, "objectives");
            mission.thumbnail_path = resolve_thumbnail(
                metadata_value(metadata, mission_section, "thumbnail"),
                filename);
            campaign.missions.push_back(std::move(mission));
        }
        if (native_campaign || configured_campaigns.count(campaign_index) != 0 ||
            !campaign.missions.empty()) {
            campaigns.push_back(std::move(campaign));
        }
    }
    return campaigns;
}

std::wstring widen(const std::string& value) {
    if (value.empty()) return {};
    const int size = MultiByteToWideChar(
        CP_ACP, 0, value.c_str(), -1, nullptr, 0);
    if (size <= 1) return {};
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(
        CP_ACP, 0, value.c_str(), -1, result.data(), size);
    result.pop_back();
    return result;
}

struct SelectorContext {
    std::vector<CampaignEntry> campaigns;
    HWND dialog = nullptr;
    HWND campaign_list = nullptr;
    HWND mission_list = nullptr;
    HWND overview = nullptr;
    HWND details = nullptr;
    HWND preview = nullptr;
    HWND start = nullptr;
    HWND back = nullptr;
    int campaign = 0;
    int mission_position = 0;
    int native_preview_handle = 0;
    std::unique_ptr<Gdiplus::Image> thumbnail;
    std::string thumbnail_error;
};

void set_control_font(HWND control, HFONT font = nullptr) noexcept {
    if (control) SendMessageA(
        control, WM_SETFONT,
        reinterpret_cast<WPARAM>(font ? font : g_regular_font), TRUE);
}

const CampaignEntry* selected_campaign(const SelectorContext& context) {
    if (context.campaign < 0 ||
        context.campaign >= static_cast<int>(context.campaigns.size())) {
        return nullptr;
    }
    return &context.campaigns[context.campaign];
}

const MissionEntry* selected_mission(const SelectorContext& context) {
    const CampaignEntry* campaign = selected_campaign(context);
    if (!campaign || context.mission_position < 0 ||
        context.mission_position >= static_cast<int>(campaign->missions.size())) {
        return nullptr;
    }
    return &campaign->missions[context.mission_position];
}

void load_thumbnail(SelectorContext& context) {
    context.thumbnail.reset();
    context.thumbnail_error.clear();
    const MissionEntry* mission = selected_mission(context);
    if (!mission || mission->thumbnail_path.empty() || !g_gdiplus_token) {
        if (mission && mission->thumbnail_path.empty()) {
            context.thumbnail_error = "No mission thumbnail was found.";
        }
        InvalidateRect(context.preview, nullptr, TRUE);
        return;
    }
    const std::wstring path = widen(mission->thumbnail_path);
    if (path.empty()) {
        context.thumbnail_error = "The thumbnail path could not be converted.";
        InvalidateRect(context.preview, nullptr, TRUE);
        return;
    }
    std::unique_ptr<Gdiplus::Image> image(
        Gdiplus::Image::FromFile(path.c_str(), FALSE));
    if (!image || image->GetLastStatus() != Gdiplus::Ok ||
        image->GetWidth() == 0 || image->GetHeight() == 0) {
        context.thumbnail_error = "The mission thumbnail could not be loaded.";
    } else {
        context.thumbnail = std::move(image);
    }
    InvalidateRect(context.preview, nullptr, TRUE);
}

void update_mission_details(SelectorContext& context) {
    const CampaignEntry* campaign = selected_campaign(context);
    const MissionEntry* mission = selected_mission(context);
    if (!campaign || !mission) {
        SetWindowTextA(context.details, "No mission is available.");
        EnableWindow(context.start, FALSE);
        context.thumbnail.reset();
        InvalidateRect(context.preview, nullptr, TRUE);
        return;
    }
    std::string details = mission->title + "\r\n\r\n" +
        mission->description;
    if (!mission->objectives.empty()) {
        details += "\r\n\r\nObjectives\r\n" + mission->objectives;
    }
    if (!mission->unlocked) {
        details += "\r\n\r\nThis mission is locked by native campaign progression.";
    }
    SetWindowTextA(context.details, details.c_str());
    EnableWindow(context.start, mission->unlocked ? TRUE : FALSE);
    load_thumbnail(context);
}

void populate_missions(SelectorContext& context) {
    SendMessageA(context.mission_list, LB_RESETCONTENT, 0, 0);
    const CampaignEntry* campaign = selected_campaign(context);
    if (!campaign) return;
    SetWindowTextA(context.overview, campaign->overview.c_str());
    for (std::size_t index = 0; index < campaign->missions.size(); ++index) {
        const MissionEntry& mission = campaign->missions[index];
        const std::string label = mission.unlocked
            ? mission.title : "[Locked] " + mission.title;
        const LRESULT row = SendMessageA(
            context.mission_list, LB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(label.c_str()));
        if (row != LB_ERR && row != LB_ERRSPACE) {
            SendMessageA(context.mission_list, LB_SETITEMDATA,
                         static_cast<WPARAM>(row), index);
        }
    }
    context.mission_position = 0;
    if (!campaign->missions.empty()) {
        auto first_unlocked = std::find_if(
            campaign->missions.begin(), campaign->missions.end(),
            [](const MissionEntry& mission) { return mission.unlocked; });
        if (first_unlocked != campaign->missions.end()) {
            context.mission_position = static_cast<int>(
                std::distance(campaign->missions.begin(), first_unlocked));
        }
        SendMessageA(context.mission_list, LB_SETCURSEL,
                     context.mission_position, 0);
    }
    update_mission_details(context);
}

void populate_campaigns(SelectorContext& context) {
    for (std::size_t index = 0; index < context.campaigns.size(); ++index) {
        const CampaignEntry& campaign = context.campaigns[index];
        const std::string label = campaign.unlocked
            ? campaign.title : "[Locked] " + campaign.title;
        const LRESULT row = SendMessageA(
            context.campaign_list, LB_ADDSTRING, 0,
            reinterpret_cast<LPARAM>(label.c_str()));
        if (row != LB_ERR && row != LB_ERRSPACE) {
            SendMessageA(context.campaign_list, LB_SETITEMDATA,
                         static_cast<WPARAM>(row), index);
        }
    }
    context.campaign = 0;
    SendMessageA(context.campaign_list, LB_SETCURSEL, 0, 0);
    populate_missions(context);
}

void layout_controls(SelectorContext& context) {
    if (!context.dialog) return;
    RECT client{};
    GetClientRect(context.dialog, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    const int margin = 18;
    const int gap = 12;
    const int header_height = 54;
    const int button_height = 30;
    const int footer_y = height - margin - button_height;
    const int content_top = margin + header_height;
    const int content_height = footer_y - gap - content_top;
    const int campaign_width = std::max(150, width * 22 / 100);
    const int mission_width = std::max(190, width * 29 / 100);
    const int details_x = margin + campaign_width + gap + mission_width + gap;
    const int details_width = std::max(180, width - details_x - margin);
    const int preview_height = std::max(130, content_height * 46 / 100);

    MoveWindow(context.campaign_list, margin, content_top,
               campaign_width, content_height, TRUE);
    MoveWindow(context.mission_list, margin + campaign_width + gap,
               content_top, mission_width, content_height, TRUE);
    MoveWindow(context.preview, details_x, content_top,
               details_width, preview_height, TRUE);
    MoveWindow(context.details, details_x,
               content_top + preview_height + gap, details_width,
               content_height - preview_height - gap, TRUE);
    MoveWindow(context.overview, margin, margin,
               width - margin * 2, header_height - 8, TRUE);
    MoveWindow(context.start, width - margin - 116, footer_y,
               116, button_height, TRUE);
    MoveWindow(context.back, width - margin - 116 - gap - 104,
               footer_y, 104, button_height, TRUE);
}

void draw_button(const DRAWITEMSTRUCT* item) noexcept {
    if (!item) return;
    const bool disabled = (item->itemState & ODS_DISABLED) != 0;
    const bool pressed = (item->itemState & ODS_SELECTED) != 0;
    const COLORREF fill = disabled ? RGB(30, 32, 36)
        : pressed ? RGB(34, 92, 126) : RGB(22, 62, 86);
    HBRUSH brush = CreateSolidBrush(fill);
    FillRect(item->hDC, &item->rcItem, brush);
    DeleteObject(brush);
    FrameRect(item->hDC, &item->rcItem,
              reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
    char text[96]{};
    GetWindowTextA(item->hwndItem, text, sizeof(text));
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, disabled ? RGB(115, 115, 115) : RGB(240, 245, 250));
    SelectObject(item->hDC, g_regular_font);
    RECT rectangle = item->rcItem;
    DrawTextA(item->hDC, text, -1, &rectangle,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

void draw_list_item(
    SelectorContext& context, const DRAWITEMSTRUCT* item) noexcept {
    if (!item || item->itemID == static_cast<UINT>(-1)) return;
    const bool selected = (item->itemState & ODS_SELECTED) != 0;
    HBRUSH background = CreateSolidBrush(
        selected ? RGB(24, 72, 104) : RGB(10, 12, 16));
    FillRect(item->hDC, &item->rcItem, background);
    DeleteObject(background);
    char text[512]{};
    SendMessageA(item->hwndItem, LB_GETTEXT, item->itemID,
                 reinterpret_cast<LPARAM>(text));
    bool unlocked = true;
    if (item->CtlID == kCampaignListId) {
        const LRESULT data = SendMessageA(
            item->hwndItem, LB_GETITEMDATA, item->itemID, 0);
        if (data >= 0 && data < static_cast<LRESULT>(context.campaigns.size())) {
            unlocked = context.campaigns[static_cast<std::size_t>(data)].unlocked;
        }
    } else if (item->CtlID == kMissionListId) {
        const CampaignEntry* campaign = selected_campaign(context);
        const LRESULT data = SendMessageA(
            item->hwndItem, LB_GETITEMDATA, item->itemID, 0);
        if (campaign && data >= 0 &&
            data < static_cast<LRESULT>(campaign->missions.size())) {
            unlocked = campaign->missions[static_cast<std::size_t>(data)].unlocked;
        }
    }
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, unlocked ? RGB(235, 240, 245) : RGB(118, 122, 128));
    SelectObject(item->hDC, g_regular_font);
    RECT text_rectangle = item->rcItem;
    text_rectangle.left += 8;
    DrawTextA(item->hDC, text, -1, &text_rectangle,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    if (item->itemState & ODS_FOCUS) DrawFocusRect(item->hDC, &item->rcItem);
}

void draw_preview(
    SelectorContext& context, const DRAWITEMSTRUCT* item) noexcept {
    if (!item) return;
    FillRect(item->hDC, &item->rcItem, g_field_brush);
    FrameRect(item->hDC, &item->rcItem,
              reinterpret_cast<HBRUSH>(GetStockObject(GRAY_BRUSH)));
    RECT interior = item->rcItem;
    InflateRect(&interior, -2, -2);
    if (context.thumbnail) {
        const UINT source_width = context.thumbnail->GetWidth();
        const UINT source_height = context.thumbnail->GetHeight();
        const int available_width = interior.right - interior.left;
        const int available_height = interior.bottom - interior.top;
        const double scale = std::min(
            static_cast<double>(available_width) / source_width,
            static_cast<double>(available_height) / source_height);
        const int draw_width = std::max(1, static_cast<int>(source_width * scale));
        const int draw_height = std::max(1, static_cast<int>(source_height * scale));
        const int x = interior.left + (available_width - draw_width) / 2;
        const int y = interior.top + (available_height - draw_height) / 2;
        Gdiplus::Graphics graphics(item->hDC);
        graphics.SetInterpolationMode(
            Gdiplus::InterpolationModeHighQualityBicubic);
        graphics.DrawImage(context.thumbnail.get(), x, y, draw_width, draw_height);
        return;
    }
    const std::string text = context.thumbnail_error.empty()
        ? "Mission preview" : context.thumbnail_error;
    SetBkMode(item->hDC, TRANSPARENT);
    SetTextColor(item->hDC, RGB(130, 136, 145));
    SelectObject(item->hDC, g_regular_font);
    DrawTextA(item->hDC, text.c_str(), -1, &interior,
              DT_CENTER | DT_VCENTER | DT_WORDBREAK);
}

using SetupMissionFunction = bool (__cdecl*)(HWND, int*);

void start_selected_mission(SelectorContext& context) {
    const CampaignEntry* campaign = selected_campaign(context);
    const MissionEntry* mission = selected_mission(context);
    if (!campaign || !mission || !campaign->unlocked || !mission->unlocked) {
        MessageBeep(MB_ICONWARNING);
        return;
    }
    auto* campaign_index = at<std::int32_t>(g_armada, kCurrentCampaignRva);
    auto* mission_index = at<std::int8_t>(g_armada, kSelectedMissionRva);
    if (!readable_range(campaign_index, sizeof(*campaign_index)) ||
        !readable_range(mission_index, sizeof(*mission_index))) {
        MessageBoxA(context.dialog,
                    "The native campaign selection state is unavailable.",
                    "Mission Selector", MB_OK | MB_ICONERROR);
        return;
    }
    if (mission->launch_campaign < 0 ||
        mission->launch_campaign >= static_cast<int>(kCampaignCount) ||
        mission->launch_mission < 0 ||
        mission->launch_mission >= static_cast<int>(kMissionsPerCampaign)) {
        MessageBoxA(context.dialog,
                    "The configured native launch slot is invalid.",
                    "Mission Selector", MB_OK | MB_ICONERROR);
        return;
    }
    auto** filenames = at<const char*>(
        g_armada, kMissionFilenameTableRva);
    if (!readable_range(
            filenames,
            sizeof(const char*) * kCampaignCount * kMissionsPerCampaign)) {
        MessageBoxA(context.dialog,
                    "The native mission filename table is unavailable.",
                    "Mission Selector", MB_OK | MB_ICONERROR);
        return;
    }
    const std::size_t launch_row =
        static_cast<std::size_t>(mission->launch_campaign) *
            kMissionsPerCampaign +
        static_cast<std::size_t>(mission->launch_mission);
    const char* original_filename = filenames[launch_row];
    *campaign_index = mission->launch_campaign;
    *mission_index = static_cast<std::int8_t>(mission->launch_mission);
    if (mission->replace_launch_filename) {
        filenames[launch_row] = mission->filename.c_str();
    }
    const auto setup = reinterpret_cast<SetupMissionFunction>(
        at(g_armada, kSetupMissionRva));
    InterlockedExchange(&g_accept_selected_mission, 1);
    const bool launched = setup && setup(
        context.dialog, &context.native_preview_handle);
    InterlockedExchange(&g_accept_selected_mission, 0);
    if (mission->replace_launch_filename) {
        filenames[launch_row] = original_filename;
    }
    if (!launched && IsWindow(context.dialog)) {
        MessageBoxA(context.dialog,
                    "Armada rejected the selected native mission row.",
                    "Mission Selector", MB_OK | MB_ICONERROR);
    }
}

void exit_to_main_menu(HWND dialog, const char* source) noexcept {
    auto* shell_state = at<std::int32_t>(g_armada, kShellStateRva);
    if (readable_range(shell_state, sizeof(*shell_state))) {
        *shell_state = kMainMenuShellState;
        log_line(std::string("Selector exit via ") + source +
                 "; shell state set to main menu");
    } else {
        log_line(std::string("Selector exit via ") + source +
                 "; shell state address is unavailable");
    }
    SetLastError(ERROR_SUCCESS);
    if (!EndDialog(dialog, 1)) {
        log_line("EndDialog failed while leaving the mission selector (error " +
                 std::to_string(GetLastError()) + ")");
    }
}

INT_PTR CALLBACK selector_dialog_proc(
    HWND dialog, UINT message, WPARAM wparam, LPARAM lparam) {
    auto* context = reinterpret_cast<SelectorContext*>(
        GetWindowLongPtrA(dialog, GWLP_USERDATA));
    if (message == WM_INITDIALOG) {
        context = reinterpret_cast<SelectorContext*>(lparam);
        if (!context) return FALSE;
        SetWindowLongPtrA(dialog, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(context));
        context->dialog = dialog;

        RECT owner_rectangle{};
        HWND owner = GetWindow(dialog, GW_OWNER);
        if (!owner || !GetWindowRect(owner, &owner_rectangle)) {
            owner_rectangle = {0, 0, GetSystemMetrics(SM_CXSCREEN),
                               GetSystemMetrics(SM_CYSCREEN)};
        }
        const int owner_width = owner_rectangle.right - owner_rectangle.left;
        const int owner_height = owner_rectangle.bottom - owner_rectangle.top;
        const int width = std::min(
            std::max(owner_width * 86 / 100, 610),
            std::max(owner_width - 24, 400));
        const int height = std::min(
            std::max(owner_height * 82 / 100, 460),
            std::max(owner_height - 32, 340));
        SetWindowPos(dialog, HWND_TOP,
            owner_rectangle.left + (owner_width - width) / 2,
            owner_rectangle.top + (owner_height - height) / 2,
            width, height, SWP_NOACTIVATE);

        context->overview = CreateWindowExA(
            0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 0, 0, dialog,
            reinterpret_cast<HMENU>(kOverviewId), nullptr, nullptr);
        context->campaign_list = CreateWindowExA(
            WS_EX_CLIENTEDGE, "LISTBOX", "",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED |
                LBS_HASSTRINGS,
            0, 0, 0, 0, dialog,
            reinterpret_cast<HMENU>(kCampaignListId), nullptr, nullptr);
        context->mission_list = CreateWindowExA(
            WS_EX_CLIENTEDGE, "LISTBOX", "",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL |
                LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_OWNERDRAWFIXED |
                LBS_HASSTRINGS,
            0, 0, 0, 0, dialog,
            reinterpret_cast<HMENU>(kMissionListId), nullptr, nullptr);
        context->preview = CreateWindowExA(
            0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
            0, 0, 0, 0, dialog,
            reinterpret_cast<HMENU>(kPreviewId), nullptr, nullptr);
        context->details = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE |
                ES_READONLY | ES_AUTOVSCROLL,
            0, 0, 0, 0, dialog,
            reinterpret_cast<HMENU>(kDetailsId), nullptr, nullptr);
        context->start = CreateWindowExA(
            0, "BUTTON", "Start Mission",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, dialog,
            reinterpret_cast<HMENU>(kStartId), nullptr, nullptr);
        context->back = CreateWindowExA(
            0, "BUTTON", "Back",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0, 0, 0, 0, dialog,
            reinterpret_cast<HMENU>(kBackId), nullptr, nullptr);
        for (HWND control : {
                 context->overview, context->campaign_list,
                 context->mission_list, context->preview, context->details,
                 context->start, context->back}) {
            set_control_font(control);
        }
        set_control_font(context->overview, g_heading_font);
        SendMessageA(context->campaign_list, LB_SETITEMHEIGHT, 0, 30);
        SendMessageA(context->mission_list, LB_SETITEMHEIGHT, 0, 28);
        layout_controls(*context);
        populate_campaigns(*context);
        SetFocus(context->campaign_list);
        return FALSE;
    }
    if (!context) return FALSE;

    switch (message) {
        case WM_SIZE:
            layout_controls(*context);
            return TRUE;
        case WM_COMMAND: {
            const int identifier = LOWORD(wparam);
            const int notification = HIWORD(wparam);
            if (identifier == kCampaignListId &&
                notification == LBN_SELCHANGE) {
                const LRESULT row = SendMessageA(
                    context->campaign_list, LB_GETCURSEL, 0, 0);
                if (row != LB_ERR) {
                    const LRESULT data = SendMessageA(
                        context->campaign_list, LB_GETITEMDATA, row, 0);
                    if (data >= 0 &&
                        data < static_cast<LRESULT>(context->campaigns.size())) {
                        context->campaign = static_cast<int>(data);
                        populate_missions(*context);
                    }
                }
                return TRUE;
            }
            if (identifier == kMissionListId &&
                (notification == LBN_SELCHANGE ||
                 notification == LBN_DBLCLK)) {
                const LRESULT row = SendMessageA(
                    context->mission_list, LB_GETCURSEL, 0, 0);
                if (row != LB_ERR) {
                    const LRESULT data = SendMessageA(
                        context->mission_list, LB_GETITEMDATA, row, 0);
                    context->mission_position = static_cast<int>(data);
                    update_mission_details(*context);
                    if (notification == LBN_DBLCLK) {
                        start_selected_mission(*context);
                    }
                }
                return TRUE;
            }
            if (identifier == kStartId && notification == BN_CLICKED) {
                start_selected_mission(*context);
                return TRUE;
            }
            if (identifier == kBackId && notification == BN_CLICKED) {
                exit_to_main_menu(dialog, "Back button");
                return TRUE;
            }
            if (identifier == IDCANCEL) {
                exit_to_main_menu(dialog, "Escape key");
                return TRUE;
            }
            break;
        }
        case WM_DRAWITEM: {
            const auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
            if (!item) return FALSE;
            if (item->CtlID == kStartId || item->CtlID == kBackId) {
                draw_button(item);
            } else if (item->CtlID == kCampaignListId ||
                       item->CtlID == kMissionListId) {
                draw_list_item(*context, item);
            } else if (item->CtlID == kPreviewId) {
                draw_preview(*context, item);
            }
            return TRUE;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC dc = reinterpret_cast<HDC>(wparam);
            SetTextColor(dc, RGB(230, 235, 240));
            SetBkColor(dc, message == WM_CTLCOLORSTATIC
                ? RGB(8, 10, 14) : RGB(10, 12, 16));
            return reinterpret_cast<INT_PTR>(
                message == WM_CTLCOLORSTATIC
                    ? g_background_brush : g_field_brush);
        }
        case WM_CLOSE:
            exit_to_main_menu(dialog, "window close");
            return TRUE;
        default:
            break;
    }
    return FALSE;
}

#pragma pack(push, 2)
struct SelectorDialogTemplate {
    DLGTEMPLATE dialog;
    WORD menu;
    WORD window_class;
    WCHAR title[25];
    WORD point_size;
    WCHAR font[9];
};
#pragma pack(pop)

INT_PTR show_selector_dialog() {
    SelectorContext context;
    context.campaigns = build_catalog();
    const bool has_missions = std::any_of(
        context.campaigns.begin(), context.campaigns.end(),
        [](const CampaignEntry& campaign) {
            return !campaign.missions.empty();
        });
    if (!has_missions) {
        log_line("Native mission filename table was empty; using Armada's selector");
        return -1;
    }

    alignas(4) SelectorDialogTemplate dialog_template{};
    dialog_template.dialog.style =
        WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME |
        DS_CENTER | DS_SETFONT;
    dialog_template.dialog.dwExtendedStyle = WS_EX_APPWINDOW;
    dialog_template.dialog.cdit = 0;
    dialog_template.dialog.x = 0;
    dialog_template.dialog.y = 0;
    dialog_template.dialog.cx = 620;
    dialog_template.dialog.cy = 420;
    std::wcscpy(dialog_template.title, L"Single Player Missions");
    dialog_template.point_size = 9;
    std::wcscpy(dialog_template.font, L"Segoe UI");

    HWND owner = GetActiveWindow();
    if (!owner) owner = GetForegroundWindow();
    HINSTANCE instance = reinterpret_cast<HINSTANCE>(
        GetModuleHandleA("A2FOMissionSelector.dll"));
    return DialogBoxIndirectParamA(
        instance,
        reinterpret_cast<const DLGTEMPLATE*>(&dialog_template),
        owner, &selector_dialog_proc,
        reinterpret_cast<LPARAM>(&context));
}

using DoSingleFunction = int (__cdecl*)();
using DoMissionSelectFunction = int (__cdecl*)(HWND);

int __cdecl do_single_hook() noexcept {
    try {
        const INT_PTR result = show_selector_dialog();
        if (result != -1) {
            log_line("Selector dialog returned " + std::to_string(result));
            return static_cast<int>(result);
        }
    } catch (...) {
        log_line("Replacement selector raised an exception; using Armada's selector");
    }
    const auto original = reinterpret_cast<DoSingleFunction>(
        g_do_single_hook.gateway);
    return original ? original() : 0;
}

int __cdecl do_mission_select_hook(HWND parent) noexcept {
    if (InterlockedCompareExchange(
            &g_accept_selected_mission, 0, 1) == 1) {
        return 1;
    }
    const auto original = reinterpret_cast<DoMissionSelectFunction>(
        g_do_mission_select_hook.gateway);
    return original ? original(parent) : 0;
}

void initialize_drawing_resources() noexcept {
    if (!g_background_brush) {
        g_background_brush = CreateSolidBrush(RGB(8, 10, 14));
    }
    if (!g_field_brush) {
        g_field_brush = CreateSolidBrush(RGB(10, 12, 16));
    }
    if (!g_regular_font) {
        g_regular_font = CreateFontA(
            -16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    }
    if (!g_heading_font) {
        g_heading_font = CreateFontA(
            -20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    }
    if (!g_gdiplus_token) {
        Gdiplus::GdiplusStartupInput input;
        if (Gdiplus::GdiplusStartup(
                &g_gdiplus_token, &input, nullptr) != Gdiplus::Ok) {
            g_gdiplus_token = 0;
            log_line("GDI+ was unavailable; mission thumbnails are disabled");
        }
    }
}

void cache_roots() {
    g_roots.clear();
    if (!g_api || !g_api->extension_root_count || !g_api->extension_root) {
        return;
    }
    const std::uint32_t count = g_api->extension_root_count();
    for (std::uint32_t index = 0; index < count; ++index) {
        const char* root = g_api->extension_root(index);
        if (root && *root) g_roots.emplace_back(root);
    }
}

}  // namespace

extern "C" __declspec(dllexport)
bool A2FO_CALL A2FO_ModuleInit(const A2FO_ModuleApi* api) {
    if (!api || api->struct_size < A2FO_MODULE_API_V4_BASE_SIZE ||
        api->api_version != A2FO_MODULE_API_VERSION || !api->log ||
        !api->armada_module || !api->install_inline_hook ||
        !api->extension_root_count || !api->extension_root) {
        return false;
    }
    g_api = api;
    g_armada = static_cast<HMODULE>(api->armada_module());
    if (!g_armada ||
        !signature_matches(kDoSingleRva, kExpectedDoSingle) ||
        !signature_matches(kDoMissionSelectRva, kExpectedDoMissionSelect) ||
        !signature_matches(
            kCampaignAvailableRva, kExpectedCampaignAvailable) ||
        !signature_matches(kSetupMissionRva, kExpectedSetupMission)) {
        log_line("Single-player shell signatures did not match; replacement disabled");
        g_api = nullptr;
        g_armada = nullptr;
        return false;
    }
    cache_roots();
    initialize_drawing_resources();
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    if (!api->install_inline_hook(
            at(g_armada, kDoMissionSelectRva),
            reinterpret_cast<void*>(&do_mission_select_hook),
            kExpectedDoMissionSelect.size(), kExpectedDoMissionSelect.data(),
            &g_do_mission_select_hook)) {
        log_line("Native mission-acceptance bridge installation failed");
        return false;
    }
    if (!api->install_inline_hook(
            at(g_armada, kDoSingleRva),
            reinterpret_cast<void*>(&do_single_hook),
            kExpectedDoSingle.size(), kExpectedDoSingle.data(),
            &g_do_single_hook)) {
        log_line("Single-player entry hook failed; native selector remains active");
        return true;
    }
    log_line("Combined campaign and mission selector enabled; native SetupMission launch retained");
    return true;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Hooks are process-lifetime. Core shutdown occurs after the shell has
    // stopped dispatching menu callbacks, so drawing resources can be freed.
    if (g_gdiplus_token) {
        Gdiplus::GdiplusShutdown(g_gdiplus_token);
        g_gdiplus_token = 0;
    }
    if (g_regular_font) DeleteObject(g_regular_font);
    if (g_heading_font) DeleteObject(g_heading_font);
    if (g_background_brush) DeleteObject(g_background_brush);
    if (g_field_brush) DeleteObject(g_field_brush);
    g_regular_font = nullptr;
    g_heading_font = nullptr;
    g_background_brush = nullptr;
    g_field_brush = nullptr;
    g_roots.clear();
    g_api = nullptr;
    g_armada = nullptr;
}
