#include "../../sdk/include/a2fo_module_provider.hpp"
/*
 * File: modules/A2FOMissionSelector/module.cpp
 * Module: A2FOHookExtensions (source-module)
 * Purpose: Replace Armada II's fixed single-player campaign and mission
 *          dialogs while retaining its native mission launch/progression path.
 */

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "../../sdk/include/a2fo_module_api.h"
#include "../../sdk/include/a2fo_menu_scene.h"
#include "../../rust/stage8_c_mission_ffi.h"
#include "../../rust/stage10_mission_completion_ffi.h"

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
#include <cstdlib>
#include <exception>
#include <fstream>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

extern "C" HWND A2FO_CALL a2fo_mission_selector_call_thiscall_0(
    void* function, void* object);

namespace {

    constexpr const char* kModuleName = "A2FOMissionSelector";
    constexpr std::uintptr_t kDoMissionSelectRva = 0x001d6d50;
    constexpr std::uintptr_t kCampaignAvailableRva = 0x001dbd60;
    constexpr std::uintptr_t kDoSingleRva = 0x001dbd90;
    constexpr std::uintptr_t kSetupMissionRva = 0x001dcc00;
    constexpr std::uintptr_t kNativeDialogHostRva = 0x0022bc70;
    constexpr std::uintptr_t kDisplayEnginePointerRva = 0x003ad508;
    constexpr std::uintptr_t kShellStateRva = 0x003a8980;
    constexpr std::uintptr_t kCurrentCampaignRva = 0x003a89a4;
    constexpr std::uintptr_t kSelectedMissionRva = 0x003a89ac;
    constexpr std::uintptr_t kCampaignMissionLimitRva = 0x003a89ad;
    constexpr std::uintptr_t kTutorialMissionCountRva = 0x003a89b5;
    constexpr std::uintptr_t kMissionFilenameTableRva = 0x003a8afc;
    constexpr std::size_t kCampaignCount = 4;
    constexpr std::size_t kMissionsPerCampaign = 10;
    constexpr std::size_t kMissionTableSize = kCampaignCount * kMissionsPerCampaign;
    constexpr int kMaximumConfiguredCampaign = 127;
    constexpr int kMaximumConfiguredMission = 511;
    constexpr std::size_t kMaximumMissionName = 260;
    constexpr std::streamoff kMaximumMetadataSize = 1024 * 1024;
    constexpr WORD kSinglePlayerDialogResource = 115;
    constexpr const char* kDefaultCampaignBackground =
    "single\\singleplay.png";

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
                    std::array<char, kMaximumMissionName> g_launch_filename_storage{};
                    const char* g_original_launch_filename = nullptr;
                    std::size_t g_launch_filename_row = kMissionTableSize;

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

                        HWND native_dialog_host() noexcept {
                            void** display_engine_slot = at<void*>(
                                g_armada, kDisplayEnginePointerRva);
                            if (!readable_range(
                                    display_engine_slot,
                                    sizeof(*display_engine_slot)) ||
                                !readable_range(
                                    at(g_armada, kNativeDialogHostRva), 6)) {
                                return nullptr;
                            }
                            void* display_engine = *display_engine_slot;
                            if (!display_engine) return nullptr;
                            HWND host = a2fo_mission_selector_call_thiscall_0(
                                at(g_armada, kNativeDialogHostRva),
                                display_engine);
                            return host && IsWindow(host) ? host : nullptr;
                        }

                        std::string trim(std::string value) {
                            const auto whitespace=+[](std::uint8_t byte,std::uint32_t) noexcept ->bool {
                                return std::isspace(byte)!=0;
                            };
                            const auto span=a2fo_rs_c_mission_trim(
                                reinterpret_cast<const std::uint8_t*>(value.data()),
                                value.size(),whitespace);
                            value.erase(0,span.begin);
                            value.erase(span.end-span.begin);
                            if(span.quoted)value=value.substr(1,value.size()-2);
                            return value;
                        }

                        std::string lower(std::string value) {
                            const auto convert=+[](std::uint8_t byte) noexcept ->std::uint8_t {
                                return static_cast<std::uint8_t>(std::tolower(byte));
                            };
                            a2fo_rs_c_mission_lower(
                                reinterpret_cast<std::uint8_t*>(value.data()),
                                value.size(),convert);
                            return value;
                        }

                        std::string join_path(const std::string& left, const std::string& right) {
                            switch(a2fo_rs_c_mission_join_action(
                                reinterpret_cast<const std::uint8_t*>(left.data()),left.size(),
                                reinterpret_cast<const std::uint8_t*>(right.data()),right.size())){
                            case 0:return right;
                            case 1:return left;
                            case 2:return left+right;
                            default:return left+"\\"+right;
                            }
                        }

                        bool regular_file(const std::string& path) noexcept {
                            const DWORD attributes = GetFileAttributesA(path.c_str());
                            return attributes != INVALID_FILE_ATTRIBUTES &&
                            (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
                        }

                        std::string decode_escapes(const std::string& input) {
                            std::string output;
                            output.reserve(input.size());
                            struct Context {std::string* output;std::exception_ptr error{};}
                                context{&output};
                            const auto emit=+[](void* opaque,std::uint8_t byte) noexcept ->bool {
                                auto& state=*static_cast<Context*>(opaque);
                                try{state.output->push_back(static_cast<char>(byte));return true;}
                                catch(...){state.error=std::current_exception();return false;}
                            };
                            a2fo_rs_c_mission_decode_escapes(
                                reinterpret_cast<const std::uint8_t*>(input.data()),
                                input.size(),&context,emit);
                            if(context.error)std::rethrow_exception(context.error);
                            return output;
                        }

                        using Metadata = std::unordered_map<std::string, std::string>;
                        using MissionLabels = std::unordered_map<std::string, std::string>;
                        using CampaignLabels = std::unordered_map<std::string, std::string>;
                        std::string mission_label_key(std::string value);
                        void merge_metadata_file(const std::string&,Metadata&);
                        void merge_label_map(const std::string&,MissionLabels&);
                        void merge_campaign_label_map(const std::string&,CampaignLabels&);

                        void merge_root_files(Metadata& output,std::uint32_t mode) {
                            struct State {
                                std::vector<std::string>::const_iterator current;
                                std::vector<std::string>::const_iterator end;
                                Metadata& output;
                                std::uint32_t mode;
                                std::exception_ptr error{};
                            } state{g_roots.begin(),g_roots.end(),output,mode};
                            const auto step=+[](void* opaque,std::uint32_t operation,
                                std::size_t) noexcept ->std::int64_t {
                                auto& s=*static_cast<State*>(opaque);
                                try {switch(operation) {
                                case 0:return s.current!=s.end;
                                case 1: {
                                    const auto path=join_path(*s.current,
                                        s.mode==0?"mission_selector.ini":"label.map");
                                    if(s.mode==0)merge_metadata_file(path,s.output);
                                    else if(s.mode==1)merge_label_map(path,s.output);
                                    else merge_campaign_label_map(path,s.output);
                                    return 0;
                                }
                                case 2: {
                                    const auto path=join_path(join_path(*s.current,"misc"),
                                        s.mode==0?"mission_selector.ini":"label.map");
                                    if(s.mode==0)merge_metadata_file(path,s.output);
                                    else if(s.mode==1)merge_label_map(path,s.output);
                                    else merge_campaign_label_map(path,s.output);
                                    return 0;
                                }
                                case 3:++s.current;return 0;
                                default:return 0;
                                }}catch(...){s.error=std::current_exception();return INT64_MIN;}
                            };
                            a2fo_rs_c_mission_roots(&state,step);
                            if(state.error)std::rethrow_exception(state.error);
                        }

                        struct MissionFileParser {
                            std::ifstream* input;
                            std::string line;
                            std::string section;
                            Metadata* metadata;
                            MissionLabels* mission_labels;
                            CampaignLabels* campaign_labels;
                            std::uint32_t mode;
                            std::exception_ptr error{};
                        };

                        std::int32_t mission_file_next(
                            void* opaque,A2foStage8MissionLine* view) noexcept {
                            auto& parser=*static_cast<MissionFileParser*>(opaque);
                            try {
                                if(!std::getline(*parser.input,parser.line))return 0;
                                if(!parser.line.empty()&&parser.line.back()=='\r')
                                    parser.line.pop_back();
                                parser.line=trim(parser.line);
                                view->data=reinterpret_cast<const std::uint8_t*>(parser.line.data());
                                view->size=parser.line.size();
                                return 1;
                            } catch(...) {
                                parser.error=std::current_exception();
                                return -1;
                            }
                        }

                        std::int64_t mission_file_step(
                            void* opaque,std::uint32_t operation,std::size_t position) noexcept {
                            auto& parser=*static_cast<MissionFileParser*>(opaque);
                            try {
                                if(parser.mode==0) {
                                    if(operation==0) {
                                        parser.section=lower(trim(parser.line.substr(
                                            1,parser.line.size()-2)));
                                    } else {
                                        const std::string key=lower(trim(parser.line.substr(0,position)));
                                        if(!key.empty())
                                            (*parser.metadata)[parser.section+"."+key]=
                                                decode_escapes(trim(parser.line.substr(position+1)));
                                    }
                                    return 0;
                                }
                                if(operation==0) {
                                    const std::string lowered=lower(parser.line);
                                    return a2fo_rs_stage10_mission_label_header(
                                        reinterpret_cast<const std::uint8_t*>(lowered.data()),
                                        lowered.size(),parser.mode);
                                }
                                const auto fields=a2fo_rs_stage10_mission_label_fields(
                                    reinterpret_cast<const std::uint8_t*>(parser.line.data()),
                                    parser.line.size(),position);
                                if(!fields.valid)return 0;
                                if(parser.mode==1) {
                                    std::string key=trim(parser.line.substr(fields.key_begin,
                                        fields.key_end-fields.key_begin));
                                    const std::string value=trim(parser.line.substr(
                                        fields.value_begin,fields.value_end-fields.value_begin));
                                    if(key.empty()||value.empty())return 0;
                                    const std::string lowered_key=lower(key);
                                    if(a2fo_rs_stage10_mission_skip_label_key(
                                        reinterpret_cast<const std::uint8_t*>(lowered_key.data()),
                                        lowered_key.size()))return 0;
                                    key=mission_label_key(std::move(key));
                                    if(!key.empty())
                                        (*parser.mission_labels)[key]=decode_escapes(value);
                                } else {
                                    const std::string key=lower(trim(parser.line.substr(
                                        fields.key_begin,fields.key_end-fields.key_begin)));
                                    const std::string value=trim(parser.line.substr(
                                        fields.value_begin,fields.value_end-fields.value_begin));
                                    if(!key.empty()&&!value.empty())
                                        (*parser.campaign_labels)[key]=decode_escapes(value);
                                }
                                return 0;
                            } catch(...) {
                                parser.error=std::current_exception();
                                return INT64_MIN;
                            }
                        }

                        void merge_metadata_file(const std::string& path, Metadata& metadata) {
                            std::ifstream input(path, std::ios::binary);
                            if (!input) return;
                            input.seekg(0, std::ios::end);
                            const std::streamoff size=input.tellg();
                            if(size<0||size>kMaximumMetadataSize){
                                log_line("Ignored oversized mission selector metadata: "+path);
                                return;
                            }
                            input.seekg(0, std::ios::beg);
                            MissionFileParser parser{&input,{},{},&metadata,nullptr,nullptr,0};
                            a2fo_rs_c_mission_parse(&parser,0,mission_file_next,mission_file_step);
                            if(parser.error)std::rethrow_exception(parser.error);
                        }

                        Metadata load_metadata() {
                            Metadata metadata;
                            merge_root_files(metadata,0);
                            return metadata;
                        }

                        std::string metadata_value(
                            const Metadata& metadata, const std::string& section,
                            const std::string& key, const std::string& fallback = {}) {
                            const auto found = metadata.find(lower(section + "." + key));
                            return found != metadata.end() ? found->second : fallback;
                            }

                            std::string mission_label_key(std::string value) {
                                value = lower(trim(std::move(value)));
                                const auto begin=a2fo_rs_c_mission_label_basename(
                                    reinterpret_cast<std::uint8_t*>(value.data()),value.size());
                                if(begin)return value.substr(begin);
                                return value;
                            }

                            void merge_label_map(const std::string& path, MissionLabels& labels) {
                                std::ifstream input(path, std::ios::binary);
                                if(!input)return;
                                input.seekg(0,std::ios::end);
                                const std::streamoff size=input.tellg();
                                if(size<0||size>kMaximumMetadataSize){
                                    log_line("Ignored oversized label.map: "+path);
                                    return;
                                }
                                input.seekg(0,std::ios::beg);
                                MissionFileParser parser{&input,{},{},nullptr,&labels,nullptr,1};
                                a2fo_rs_c_mission_parse(&parser,1,mission_file_next,mission_file_step);
                                if(parser.error)std::rethrow_exception(parser.error);
                            }

                            MissionLabels load_mission_labels() {
                                MissionLabels labels;
                                // Extension roots are ordered Data -> parent mods -> active mod. Reading
                                // in that order deliberately gives the active mod the final override.
                                merge_root_files(labels,1);
                                return labels;
                            }

                            std::string mission_label_value(
                                const MissionLabels& labels, const std::string& filename) {
                                const auto found = labels.find(mission_label_key(filename));
                                return found != labels.end() ? found->second : std::string{};
                                }

                                void merge_campaign_label_map(
                                    const std::string& path,CampaignLabels& labels) {
                                    std::ifstream input(path,std::ios::binary);
                                    if(!input)return;
                                    input.seekg(0,std::ios::end);
                                    const std::streamoff size=input.tellg();
                                    if(size<0||size>kMaximumMetadataSize){
                                        log_line("Ignored oversized label.map: "+path);
                                        return;
                                    }
                                    input.seekg(0,std::ios::beg);
                                    MissionFileParser parser{&input,{},{},nullptr,nullptr,&labels,2};
                                    a2fo_rs_c_mission_parse(&parser,2,mission_file_next,mission_file_step);
                                    if(parser.error)std::rethrow_exception(parser.error);
                                }

                                    CampaignLabels load_campaign_labels() {
                                        CampaignLabels labels;
                                        // Extension roots are ordered Data -> parent mods -> active mod, so later
                                        // label.map files override the stock/parent campaign names naturally.
                                        merge_root_files(labels,2);
                                        return labels;
                                    }

                                    std::string campaign_label_value(
                                        const CampaignLabels& labels, int campaign_index) {
                                        static const std::array<const char*, kCampaignCount> keys{{
                                            "tutorials",
                                            "federation_campaign",
                                            "klingon_campaign",
                                            "borg_campaign",
                                        }};
                                        const int key_index=a2fo_rs_stage10_mission_campaign_label_index(
                                            campaign_index);
                                        if (key_index < 0) {
                                            return {};
                                            }
                                            const auto found = labels.find(keys[static_cast<std::size_t>(key_index)]);
                                        return found != labels.end() ? found->second : std::string{};
                                        }

                                        bool metadata_bool(
                                            const Metadata& metadata, const std::string& section,
                                            const std::string& key, bool fallback) {
                                            const std::string value = lower(metadata_value(metadata, section, key));
                                            const auto token=a2fo_rs_c_mission_bool_token(
                                                reinterpret_cast<const std::uint8_t*>(value.data()),
                                                value.size());
                                            return token<0?fallback:token!=0;
                                            }

                                            int metadata_integer(
                                                const Metadata& metadata, const std::string& section,
                                                const std::string& key, int fallback, int minimum, int maximum) {
                                                const std::string value = metadata_value(metadata, section, key);
                                                if (value.empty()) return fallback;
                                                char* end = nullptr;
                                                const long parsed = std::strtol(value.c_str(), &end, 10);
                                                if (!end || end == value.c_str() || *end != '\0' ||
                                                    !a2fo_rs_c_mission_integer_admit(
                                                        static_cast<std::int64_t>(parsed),minimum,maximum)) {
                                                    return fallback;
                                                    }
                                                    return static_cast<int>(parsed);
                                                }

                                                    std::set<int> collect_configured_indices(
                                                        const Metadata& metadata,const std::string& prefix,
                                                        int maximum) {
                                                        struct State {
                                                            Metadata::const_iterator current;
                                                            Metadata::const_iterator end;
                                                            std::set<int> indices{};
                                                            std::exception_ptr error{};
                                                        } state{metadata.begin(),metadata.end()};
                                                        const auto next=+[](void* opaque,
                                                            A2foStage8MissionLine* view) noexcept ->std::int32_t {
                                                            auto& s=*static_cast<State*>(opaque);
                                                            if(s.current==s.end)return 0;
                                                            view->data=reinterpret_cast<const std::uint8_t*>(
                                                                s.current->first.data());
                                                            view->size=s.current->first.size();
                                                            return 1;
                                                        };
                                                        const auto digit=+[](std::uint8_t byte,
                                                            std::uint32_t) noexcept ->bool {
                                                            return std::isdigit(byte)!=0;
                                                        };
                                                        const auto step=+[](void* opaque,std::uint32_t operation,
                                                            std::size_t index) noexcept ->std::int64_t {
                                                            auto& s=*static_cast<State*>(opaque);
                                                            try{
                                                                if(operation==0)s.indices.insert(
                                                                    static_cast<int>(index));
                                                                else ++s.current;
                                                                return 0;
                                                            }catch(...){s.error=std::current_exception();return INT64_MIN;}
                                                        };
                                                        a2fo_rs_c_mission_collect_indices(&state,
                                                            reinterpret_cast<const std::uint8_t*>(prefix.data()),
                                                            prefix.size(),static_cast<std::uint32_t>(maximum),
                                                            digit,next,step);
                                                        if(state.error)std::rethrow_exception(state.error);
                                                        return state.indices;
                                                    }

                                                    std::set<int> configured_campaign_indices(
                                                        const Metadata& metadata) {
                                                        return collect_configured_indices(metadata,"campaign",
                                                            kMaximumConfiguredCampaign);
                                                    }

                                                    std::set<int> configured_mission_indices(
                                                        const Metadata& metadata,int campaign) {
                                                        const std::string prefix="campaign"+
                                                            std::to_string(campaign)+".mission";
                                                        return collect_configured_indices(metadata,prefix,
                                                            kMaximumConfiguredMission);
                                                    }

                                                        std::string safe_string(const char* value) noexcept {
                                                            if(!value)return {};
                                                            std::string result;
                                                            struct State {
                                                                const char* value;
                                                                std::string* result;
                                                                char byte=0;
                                                                std::exception_ptr error{};
                                                            } state{value,&result};
                                                            const auto step=+[](void* opaque,std::uint32_t operation,
                                                                std::size_t index) noexcept ->std::int64_t {
                                                                auto& s=*static_cast<State*>(opaque);
                                                                try{switch(operation) {
                                                                case 0:s.result->reserve(64);return 0;
                                                                case 1:return readable_range(s.value+index,1);
                                                                case 2:s.byte=s.value[index];
                                                                    return static_cast<unsigned char>(s.byte);
                                                                case 3:s.result->push_back(s.byte);return 0;
                                                                default:return 0;
                                                                }}catch(...){s.error=std::current_exception();return INT64_MIN;}
                                                            };
                                                            const bool complete=a2fo_rs_c_mission_safe_string(
                                                                &state,kMaximumMissionName,step);
                                                            if(state.error)std::terminate();
                                                            if(!complete)return {};
                                                            return result;
                                                        }

                                                        std::string file_stem(std::string value) {
                                                            const auto span=a2fo_rs_c_mission_file_stem(
                                                                reinterpret_cast<const std::uint8_t*>(value.data()),
                                                                value.size());
                                                            if(span.begin)value.erase(0,span.begin);
                                                            if(span.end-span.begin<value.size())
                                                                value.erase(span.end-span.begin);
                                                            return value;
                                                        }

                                                        std::string humanize_mission_name(const std::string& filename) {
                                                            std::string value = file_stem(filename);
                                                            std::string lowered = lower(value);
                                                            const auto stock=a2fo_rs_c_mission_humanize_stock(
                                                                reinterpret_cast<const std::uint8_t*>(value.data()),
                                                                reinterpret_cast<const std::uint8_t*>(lowered.data()),
                                                                value.size());
                                                            if(stock.kind!=0){
                                                                static const char* titles[]{"Tutorial ","Federation Mission ",
                                                                    "Klingon Mission ","Borg Mission "};
                                                                static const std::size_t lengths[]{11,6,8,7};
                                                                const std::size_t which=stock.kind-1;
                                                                std::string number=value.substr(lengths[which]);
                                                                number.erase(0,stock.begin-lengths[which]);
                                                                return std::string(titles[which])+number;
                                                            }
                                                            std::string output;
                                                            output.reserve(value.size() + 8);
                                                            struct Context {std::string* output;std::exception_ptr error{};}
                                                                context{&output};
                                                            const auto classify=+[](std::uint8_t byte,
                                                                std::uint32_t kind) noexcept ->bool {
                                                                switch(kind){
                                                                case 0:return std::islower(byte)!=0;
                                                                case 1:return std::isupper(byte)!=0;
                                                                default:return std::isdigit(byte)!=0;
                                                                }
                                                            };
                                                            const auto emit=+[](void* opaque,
                                                                std::uint8_t byte) noexcept ->bool {
                                                                auto& state=*static_cast<Context*>(opaque);
                                                                try{state.output->push_back(static_cast<char>(byte));return true;}
                                                                catch(...){state.error=std::current_exception();return false;}
                                                            };
                                                            a2fo_rs_c_mission_humanize(
                                                                reinterpret_cast<const std::uint8_t*>(value.data()),
                                                                value.size(),classify,&context,emit);
                                                            if(context.error)std::rethrow_exception(context.error);
                                                            return output.empty() ? filename : output;
                                                        }

                                                        std::string resolve_asset(const std::string& requested) {
                                                            struct Context {const std::string& requested;
                                                                std::optional<std::array<std::string,3>> candidates{};
                                                                std::string result{};std::exception_ptr error{};
                                                            } context{requested};
                                                            const auto step=+[](void* opaque,std::uint32_t op,
                                                                std::size_t root) noexcept ->std::int64_t {
                                                                auto& c=*static_cast<Context*>(opaque);
                                                                try{switch(op){
                                                                case 0:return !c.requested.empty();
                                                                case 1:if(regular_file(c.requested)){
                                                                    c.result=c.requested;return 1;}return 0;
                                                                case 2:{
                                                                    const std::string& base=g_roots[root];
                                                                    c.candidates.emplace(std::array<std::string,3>{{
                                                                        join_path(base,c.requested),
                                                                        join_path(join_path(base,"bzn"),c.requested),
                                                                        join_path(join_path(base,"bitmaps"),c.requested)}});
                                                                    return 0;
                                                                }
                                                                case 3:case 4:case 5:{
                                                                    const auto& candidate=(*c.candidates)[op-3];
                                                                    if(regular_file(candidate)){c.result=candidate;return 1;}
                                                                    return 0;}
                                                                case 6:c.candidates.reset();return 0;
                                                                default:return 0;
                                                                }}catch(...){c.error=std::current_exception();return INT64_MIN;}
                                                            };
                                                            a2fo_rs_c_mission_resolve_asset(&context,g_roots.size(),step);
                                                            if(context.error)std::rethrow_exception(context.error);
                                                            return context.result;
                                                        }

                                                        std::string resolve_thumbnail(
                                                            const std::string& configured, const std::string& filename) {
                                                            struct Context {const std::string& configured;
                                                                const std::string& filename;std::string stem{},result{};
                                                                std::exception_ptr error{};
                                                            } context{configured,filename};
                                                            const auto step=+[](void* opaque,std::uint32_t op,
                                                                std::size_t index) noexcept ->std::int64_t {
                                                                auto& c=*static_cast<Context*>(opaque);
                                                                try{switch(op){
                                                                case 0:return !c.configured.empty();
                                                                case 1:c.result=resolve_asset(c.configured);return !c.result.empty();
                                                                case 2:c.stem=file_stem(c.filename);return 0;
                                                                case 3:{static const char* extensions[]{".png",".bmp",".jpg",".jpeg"};
                                                                    c.result=resolve_asset(c.stem+extensions[index]);
                                                                    return !c.result.empty();}
                                                                default:return 0;
                                                                }}catch(...){c.error=std::current_exception();return INT64_MIN;}
                                                            };
                                                            a2fo_rs_c_mission_resolve_thumbnail(&context,step);
                                                            if(context.error)std::rethrow_exception(context.error);
                                                            return context.result;
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
                                                                std::string menu_view;
                                                                bool unlocked = false;
                                                                bool replace_launch_filename = false;
                                                            };

                                                            struct CampaignEntry {
                                                                int index = 0;
                                                                std::string title;
                                                                std::string overview;
                                                                std::string background_path;
                                                                std::string menu_view;
                                                                bool unlocked = false;
                                                                std::vector<MissionEntry> missions;
                                                            };

                                                            using CampaignAvailableFunction = bool (__cdecl*)(int campaign);

                                                            bool campaign_available(int campaign) noexcept {
                                                                const auto function = reinterpret_cast<CampaignAvailableFunction>(
                                                                    at(g_armada, kCampaignAvailableRva));
                                                                return function && function(campaign);
                                                            }

                                                            bool mission_unlocked(int campaign,int mission) noexcept {
                                                                struct State {
                                                                    int campaign;
                                                                    const std::int8_t* count=nullptr;
                                                                    const std::int8_t* limits=nullptr;
                                                                } state{campaign};
                                                                const auto step=+[](void* opaque,std::uint32_t operation,
                                                                    std::size_t) noexcept ->std::int64_t {
                                                                    auto& s=*static_cast<State*>(opaque);
                                                                    switch(operation) {
                                                                    case 0:s.count=at<std::int8_t>(g_armada,
                                                                        kTutorialMissionCountRva);
                                                                        return readable_range(s.count,sizeof(*s.count));
                                                                    case 1:return *s.count;
                                                                    case 2:s.limits=at<std::int8_t>(g_armada,
                                                                        kCampaignMissionLimitRva);
                                                                        return readable_range(s.limits,kCampaignCount);
                                                                    case 3:return s.limits[s.campaign];
                                                                    default:return 0;
                                                                    }
                                                                };
                                                                return a2fo_rs_c_mission_unlocked(&state,campaign,mission,
                                                                    static_cast<int>(kMissionsPerCampaign),step);
                                                            }

                                                            std::vector<CampaignEntry> build_catalog() {
                                                                static const std::array<const char*, kCampaignCount> default_titles{{
                                                                    "Tutorials", "Federation Campaign", "Klingon Campaign",
                                                                    "Borg Campaign"}};
                                                                static const std::array<const char*, kCampaignCount> default_views{{
                                                                    "tutorials", "federation", "klingon", "borg"}};
                                                                static const std::array<const char*, kCampaignCount> default_overviews{{
                                                                    "Learn Armada II's interface, economy, construction, and combat systems.",
                                                                    "Command Starfleet through the opening campaign.",
                                                                    "Lead the Klingon Empire after completing the Federation campaign.",
                                                                    "Direct the Borg campaign after completing the preceding campaigns."}};
                                                                const Metadata metadata=load_metadata();
                                                                const MissionLabels mission_labels=load_mission_labels();
                                                                const CampaignLabels campaign_labels=load_campaign_labels();
                                                                auto** filenames=at<const char*>(g_armada,kMissionFilenameTableRva);
                                                                const bool filenames_valid=readable_range(filenames,
                                                                    sizeof(const char*)*kCampaignCount*kMissionsPerCampaign);
                                                                std::vector<CampaignEntry> campaigns;
                                                                if(!filenames_valid)return campaigns;
                                                                const std::set<int> configured_campaigns=
                                                                    configured_campaign_indices(metadata);
                                                                const bool request_ini=lower(metadata_value(
                                                                    metadata,"selector","catalogmode"))=="ini";
                                                                const int configured_max=configured_campaigns.empty() ? 0
                                                                    : *configured_campaigns.rbegin();
                                                                struct Context {
                                                                    const Metadata* metadata;
                                                                    const MissionLabels* mission_labels;
                                                                    const CampaignLabels* campaign_labels;
                                                                    const std::set<int>* configured_campaigns;
                                                                    const char* const* filenames;
                                                                    std::vector<CampaignEntry>* campaigns;
                                                                    CampaignEntry campaign{};
                                                                    std::set<int> mission_indices{};
                                                                    std::string section{};
                                                                    std::string mission_section{};
                                                                    std::string configured_filename{};
                                                                    std::string filename{};
                                                                    int campaign_index=0;
                                                                    int mission_index=0;
                                                                    bool native_campaign=false;
                                                                    bool native_slot=false;
                                                                    std::exception_ptr error{};
                                                                } context{&metadata,&mission_labels,&campaign_labels,
                                                                    &configured_campaigns,filenames,&campaigns};
                                                                const auto step=+[](void* opaque,std::uint32_t operation,
                                                                    std::size_t argument) noexcept ->std::int64_t {
                                                                    auto& state=*static_cast<Context*>(opaque);
                                                                    try {
                                                                        switch(operation) {
                                                                        case 0:
                                                                            log_line(argument==0
                                                                                ? "catalogMode=ini had no configured campaigns; using the native catalog"
                                                                                : "Using the authoritative mission_selector.ini catalog");
                                                                            return 0;
                                                                        case 1:
                                                                            state.campaigns->reserve(argument);
                                                                            return 0;
                                                                        case 2: {
                                                                            state.campaign=CampaignEntry{};
                                                                            state.campaign_index=static_cast<int>(argument);
                                                                            state.campaign.index=state.campaign_index;
                                                                            state.native_campaign=a2fo_rs_stage10_mission_native_campaign(
                                                                                state.campaign_index,static_cast<int>(kCampaignCount));
                                                                            state.campaign.unlocked=state.native_campaign
                                                                                ? campaign_available(state.campaign.index):true;
                                                                            state.section="campaign"+
                                                                                std::to_string(state.campaign_index);
                                                                            if(!state.native_campaign)
                                                                                state.campaign.unlocked=metadata_bool(
                                                                                    *state.metadata,state.section,"unlocked",
                                                                                    state.campaign.unlocked);
                                                                            const std::string native_campaign_title=
                                                                                state.native_campaign ? campaign_label_value(
                                                                                    *state.campaign_labels,state.campaign_index)
                                                                                    : std::string{};
                                                                            const std::string fallback_title=state.native_campaign
                                                                                ? (native_campaign_title.empty()
                                                                                    ? default_titles[static_cast<std::size_t>(state.campaign_index)]
                                                                                    : native_campaign_title)
                                                                                : "Custom Campaign "+
                                                                                    std::to_string(state.campaign_index-3);
                                                                            const std::string fallback_overview=state.native_campaign
                                                                                ? default_overviews[static_cast<std::size_t>(state.campaign_index)]
                                                                                : "A mod-defined campaign.";
                                                                            state.campaign.title=metadata_value(*state.metadata,
                                                                                state.section,"title",fallback_title);
                                                                            state.campaign.overview=metadata_value(*state.metadata,
                                                                                state.section,"overview",fallback_overview);
                                                                            state.campaign.menu_view=metadata_value(*state.metadata,
                                                                                state.section,"menuview",state.native_campaign
                                                                                    ? default_views[static_cast<std::size_t>(state.campaign_index)]
                                                                                    : "");
                                                                            const std::string configured_background=metadata_value(
                                                                                *state.metadata,state.section,"background");
                                                                            if(!configured_background.empty()) {
                                                                                state.campaign.background_path=resolve_asset(
                                                                                    configured_background);
                                                                                if(state.campaign.background_path.empty())
                                                                                    log_line("Campaign background was not found for ["+
                                                                                        state.section+"]: "+configured_background);
                                                                            }
                                                                            if(state.campaign.background_path.empty())
                                                                                state.campaign.background_path=resolve_asset(
                                                                                    kDefaultCampaignBackground);
                                                                            return 0;
                                                                        }
                                                                        case 3:
                                                                            state.mission_indices=configured_mission_indices(
                                                                                *state.metadata,state.campaign_index);
                                                                            return 0;
                                                                        case 4:
                                                                            state.mission_indices.insert(static_cast<int>(argument));
                                                                            return 0;
                                                                        case 5: {
                                                                            auto mission=state.mission_indices.begin();
                                                                            std::advance(mission,static_cast<std::ptrdiff_t>(argument));
                                                                            return *mission;
                                                                        }
                                                                        case 6: {
                                                                            state.mission_index=static_cast<int>(argument);
                                                                            state.native_slot=a2fo_rs_stage10_mission_native_slot(
                                                                                state.native_campaign,state.mission_index,
                                                                                static_cast<int>(kMissionsPerCampaign));
                                                                            const std::size_t row=state.native_slot
                                                                                ? static_cast<std::size_t>(state.campaign_index)*
                                                                                    kMissionsPerCampaign+
                                                                                    static_cast<std::size_t>(state.mission_index)
                                                                                : 0;
                                                                            const std::string native_filename=state.native_slot
                                                                                ? safe_string(state.filenames[row]):std::string{};
                                                                            state.mission_section=state.section+".mission"+
                                                                                std::to_string(state.mission_index);
                                                                            state.configured_filename=metadata_value(
                                                                                *state.metadata,state.mission_section,"file");
                                                                            state.filename=state.configured_filename.empty()
                                                                                ? native_filename:state.configured_filename;
                                                                            return static_cast<std::int64_t>(state.filename.size());
                                                                        }
                                                                        case 7:
                                                                            log_line("Ignored overlong custom mission filename in ["+
                                                                                state.mission_section+"]");
                                                                            return 0;
                                                                        case 8: {
                                                                            MissionEntry mission;
                                                                            mission.index=state.mission_index;
                                                                            mission.filename=state.filename;
                                                                            mission.launch_campaign=state.native_slot
                                                                                ? state.campaign_index:metadata_integer(
                                                                                    *state.metadata,state.mission_section,
                                                                                    "nativecampaign",1,0,
                                                                                    static_cast<int>(kCampaignCount)-1);
                                                                            mission.launch_mission=state.native_slot
                                                                                ? state.mission_index:metadata_integer(
                                                                                    *state.metadata,state.mission_section,
                                                                                    "nativemission",0,0,
                                                                                    static_cast<int>(kMissionsPerCampaign)-1);
                                                                            const bool native_unlocked=state.native_slot
                                                                                ? mission_unlocked(state.campaign.index,mission.index)
                                                                                : true;
                                                                            const bool configured_unlocked=state.native_slot
                                                                                ? native_unlocked:metadata_bool(*state.metadata,
                                                                                    state.mission_section,"unlocked",true);
                                                                            const auto flags=a2fo_rs_stage10_mission_flags(
                                                                                state.campaign.unlocked,configured_unlocked,
                                                                                !state.configured_filename.empty(),state.native_slot);
                                                                            mission.unlocked=flags.unlocked;
                                                                            mission.replace_launch_filename=flags.replace_launch_filename;
                                                                            const std::string native_title=mission_label_value(
                                                                                *state.mission_labels,state.filename);
                                                                            mission.title=metadata_value(*state.metadata,
                                                                                state.mission_section,"title",native_title.empty()
                                                                                    ? humanize_mission_name(state.filename)
                                                                                    : native_title);
                                                                            mission.description=metadata_value(*state.metadata,
                                                                                state.mission_section,"description",
                                                                                "Native mission file: "+state.filename);
                                                                            mission.objectives=metadata_value(*state.metadata,
                                                                                state.mission_section,"objectives");
                                                                            mission.menu_view=metadata_value(*state.metadata,
                                                                                state.mission_section,"menuview");
                                                                            mission.thumbnail_path=resolve_thumbnail(metadata_value(
                                                                                *state.metadata,state.mission_section,"thumbnail"),
                                                                                state.filename);
                                                                            state.campaign.missions.push_back(std::move(mission));
                                                                            return 0;
                                                                        }
                                                                        case 9:return static_cast<std::int64_t>(state.campaign.missions.size());
                                                                        case 10:return state.configured_campaigns->count(
                                                                            state.campaign_index)!=0;
                                                                        case 11:
                                                                            state.campaigns->push_back(std::move(state.campaign));
                                                                            return 0;
                                                                        case 12:return static_cast<std::int64_t>(
                                                                            state.mission_indices.size());
                                                                        default:return 0;
                                                                        }
                                                                    } catch(...) {
                                                                        state.error=std::current_exception();
                                                                        return INT64_MIN;
                                                                    }
                                                                };
                                                                const A2foStage8MissionCatalogPlan plan{
                                                                    configured_campaigns.size(),configured_max,
                                                                    static_cast<std::uint32_t>(kCampaignCount),
                                                                    static_cast<std::uint32_t>(kMissionsPerCampaign),
                                                                    kMaximumMissionName,request_ini,filenames_valid};
                                                                a2fo_rs_c_mission_catalog(&context,plan,step);
                                                                if(context.error)std::rethrow_exception(context.error);
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
std::array<std::unique_ptr<Gdiplus::Image>, 4> button_images;
HWND hovered_button = nullptr;
                                                                std::unique_ptr<Gdiplus::Image> background;
                                                                std::string background_source_path;
                                                                bool departure_started = false;
                                                                std::unique_ptr<Gdiplus::Image> thumbnail;
                                                                std::string thumbnail_error;
                                                            };

                                                            void set_control_font(HWND control, HFONT font = nullptr) noexcept {
                                                                if (control) SendMessageA(
                                                                    control, WM_SETFONT,
                                                                    reinterpret_cast<WPARAM>(font ? font : g_regular_font), TRUE);
                                                            }

                                                            const CampaignEntry* selected_campaign(const SelectorContext& context) {
                                                                if (!a2fo_rs_c_mission_index_valid(
                                                                    context.campaign,static_cast<int>(context.campaigns.size()))) {
                                                                    return nullptr;
                                                                    }
                                                                    return &context.campaigns[context.campaign];
                                                            }

                                                            const MissionEntry* selected_mission(const SelectorContext& context) {
                                                                const CampaignEntry* campaign = selected_campaign(context);
                                                                if (!campaign || !a2fo_rs_c_mission_index_valid(
                                                                    context.mission_position,
                                                                    static_cast<int>(campaign->missions.size()))) {
                                                                    return nullptr;
                                                                    }
                                                                    return &campaign->missions[context.mission_position];
                                                            }

                                                            const std::string& selected_menu_view(const SelectorContext& context) noexcept {
                                                                struct State {
                                                                    const SelectorContext& selector;
                                                                    const MissionEntry* mission=nullptr;
                                                                    const CampaignEntry* campaign=nullptr;
                                                                } state{context};
                                                                const auto step=+[](void* opaque,std::uint32_t operation,
                                                                                    std::size_t) noexcept ->std::int64_t {
                                                                    auto& s=*static_cast<State*>(opaque);
                                                                    switch(operation) {
                                                                    case 0:s.mission=selected_mission(s.selector);
                                                                        return s.mission!=nullptr;
                                                                    case 1:return !s.mission->menu_view.empty();
                                                                    case 2:s.campaign=selected_campaign(s.selector);
                                                                        return s.campaign!=nullptr;
                                                                    default:return 0;
                                                                    }
                                                                };
                                                                const auto selected=a2fo_rs_c_mission_menu_view(&state,step);
                                                                static const std::string empty;
                                                                return selected==1 ? state.mission->menu_view
                                                                    : selected==2 ? state.campaign->menu_view : empty;
                                                            }

                                                            HMODULE scene_module() noexcept {
                                                                return a2fo::startup_module_provider("A2FORefitMenus.dll", {"A2FORefitMenus_PresentSceneV1", "A2FORefitMenus_SetMissionV1", "A2FORefitMenus_BeginDepartureV1", "A2FORefitMenus_EndDepartureV1"});
                                                            }

                                                            template <typename Function>
                                                            Function scene_export(const char* name) noexcept {
                                                                HMODULE module = scene_module();
                                                                if (!module) return nullptr;
                                                                const FARPROC address = GetProcAddress(module, name);
                                                                static_assert(sizeof(Function) == sizeof(address),
                                                                    "Win32 function pointer sizes must match");
                                                                Function function = nullptr;
                                                                std::memcpy(&function, &address, sizeof(function));
                                                                return function;
                                                            }

                                                            bool present_scene_region(
                                                                SelectorContext& context, HDC dc, const RECT& target,
                                                                int dialog_x, int dialog_y, int width, int height) noexcept {
                                                                const auto present = scene_export<A2FO_MenuScenePresentV1>(
                                                                    "A2FORefitMenus_PresentSceneV1");
                                                                if (!present || width <= 0 || height <= 0) return false;
                                                                const int saved = SaveDC(dc);
                                                                if (!saved) return false;
                                                                IntersectClipRect(dc, target.left, target.top, target.right, target.bottom);
                                                                OffsetViewportOrgEx(dc, -dialog_x, -dialog_y, nullptr);
                                                                const std::string& view = selected_menu_view(context);
                                                                const BOOL drawn = present(
                                                                    context.dialog, dc, width, height, view.c_str(),
                                                                    context.departure_started ? A2FO_MENU_SCENE_DEPARTURE : 0);
                                                                RestoreDC(dc, saved);
                                                                return drawn != FALSE;
                                                            }

                                                            void redraw_dialog_background(SelectorContext& context) noexcept {
                                                                if (!context.dialog) return;
                                                                RedrawWindow(
                                                                    context.dialog, nullptr, nullptr,
                                                                    RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
                                                            }

                                                            void load_campaign_background(SelectorContext& context) {
                                                                struct State {
                                                                    SelectorContext& selector;
                                                                    std::string path{};
                                                                    std::wstring wide_path{};
                                                                    std::exception_ptr error{};
                                                                } state{context};
                                                                const auto step=+[](void* opaque,std::uint32_t operation,
                                                                    std::size_t) noexcept ->std::int64_t {
                                                                    auto& s=*static_cast<State*>(opaque);
                                                                    auto& c=s.selector;
                                                                    try {switch(operation) {
                                                                    case 0: {
                                                                        const CampaignEntry* campaign=selected_campaign(c);
                                                                        s.path=campaign?campaign->background_path:std::string{};
                                                                        return s.path==c.background_source_path;
                                                                    }
                                                                    case 1:c.background_source_path=s.path;
                                                                        c.background.reset();return 0;
                                                                    case 2:return !s.path.empty()&&g_gdiplus_token!=0;
                                                                    case 3:s.wide_path=widen(s.path);return !s.wide_path.empty();
                                                                    case 4:log_line("Campaign background path could not be converted: "+
                                                                        s.path);return 0;
                                                                    case 5: {
                                                                        std::unique_ptr<Gdiplus::Image> image(
                                                                            Gdiplus::Image::FromFile(s.wide_path.c_str(),FALSE));
                                                                        if(!image||image->GetLastStatus()!=Gdiplus::Ok||
                                                                            image->GetWidth()==0||image->GetHeight()==0)
                                                                            log_line("Campaign background could not be loaded: "+
                                                                                s.path);
                                                                        else c.background=std::move(image);
                                                                        return 0;
                                                                    }
                                                                    case 6:redraw_dialog_background(c);return 0;
                                                                    default:return 0;
                                                                    }}catch(...){s.error=std::current_exception();return INT64_MIN;}
                                                                };
                                                                a2fo_rs_c_mission_background(&state,step);
                                                                if(state.error)std::rethrow_exception(state.error);
                                                            }

                                                            void draw_dialog_background_region(
                                                                SelectorContext& context, HDC device_context,
                                                                const RECT& target, int dialog_x,
                                                                int dialog_y) noexcept {
                                                                    if (!device_context || !context.dialog) return;
                                                                    RECT dialog_client{};
                                                                    if (!GetClientRect(context.dialog, &dialog_client)) return;
                                                                    const int available_width =
                                                                        dialog_client.right - dialog_client.left;
                                                                    const int available_height =
                                                                        dialog_client.bottom - dialog_client.top;
                                                                    if (present_scene_region(context, device_context, target,
                                                                            dialog_x, dialog_y, available_width, available_height)) {
                                                                        Gdiplus::Graphics graphics(device_context);
                                                                        Gdiplus::SolidBrush shade(Gdiplus::Color(96, 0, 0, 0));
                                                                        graphics.FillRectangle(&shade,
                                                                            static_cast<INT>(target.left),
                                                                            static_cast<INT>(target.top),
                                                                            static_cast<INT>(target.right - target.left),
                                                                            static_cast<INT>(target.bottom - target.top));
                                                                        return;
                                                                    }
                                                                    FillRect(device_context, &target, g_background_brush);
                                                                    if (!context.background) return;
                                                                    const UINT source_width = context.background->GetWidth();
                                                                    const UINT source_height = context.background->GetHeight();
                                                                    if (source_width == 0 || source_height == 0 ||
                                                                        available_width <= 0 || available_height <= 0) {
                                                                        return;
                                                                    }

                                                                    const double scale = std::max(
                                                                        static_cast<double>(available_width) / source_width,
                                                                        static_cast<double>(available_height) / source_height);
                                                                    const int draw_width = std::max(
                                                                        1, static_cast<int>(source_width * scale));
                                                                    const int draw_height = std::max(
                                                                        1, static_cast<int>(source_height * scale));
                                                                    const int x = dialog_client.left +
                                                                        (available_width - draw_width) / 2 - dialog_x;
                                                                    const int y = dialog_client.top +
                                                                        (available_height - draw_height) / 2 - dialog_y;

                                                                    Gdiplus::Graphics graphics(device_context);
                                                                    graphics.SetInterpolationMode(
                                                                        Gdiplus::InterpolationModeHighQualityBicubic);
                                                                    graphics.DrawImage(
                                                                        context.background.get(), x, y,
                                                                        draw_width, draw_height);
                                                                    Gdiplus::SolidBrush shade(
                                                                        Gdiplus::Color(96, 0, 0, 0));
                                                                    graphics.FillRectangle(
                                                                        &shade,
                                                                        static_cast<INT>(target.left),
                                                                        static_cast<INT>(target.top),
                                                                        static_cast<INT>(target.right - target.left),
                                                                        static_cast<INT>(target.bottom - target.top));
                                                                }

                                                            void draw_dialog_background(
                                                                SelectorContext& context, HDC device_context) noexcept {
                                                                    if (!device_context || !context.dialog) return;
                                                                    RECT client{};
                                                                    if (!GetClientRect(context.dialog, &client)) return;
                                                                    draw_dialog_background_region(
                                                                        context, device_context, client, 0, 0);
                                                                }

                                                                void load_thumbnail(SelectorContext& context) {
                                                                    struct State {
                                                                        SelectorContext& selector;
                                                                        const MissionEntry* mission=nullptr;
                                                                        std::wstring path{};
                                                                        std::exception_ptr error{};
                                                                    } state{context};
                                                                    const auto step=+[](void* opaque,std::uint32_t operation,
                                                                        std::size_t) noexcept ->std::int64_t {
                                                                        auto& s=*static_cast<State*>(opaque);
                                                                        auto& c=s.selector;
                                                                        try {switch(operation) {
                                                                        case 0:c.thumbnail.reset();c.thumbnail_error.clear();return 0;
                                                                        case 1:s.mission=selected_mission(c);return s.mission!=nullptr;
                                                                        case 2:return s.mission->thumbnail_path.empty();
                                                                        case 3:c.thumbnail_error="No mission thumbnail was found.";
                                                                            return 0;
                                                                        case 4:return g_gdiplus_token!=0;
                                                                        case 5:s.path=widen(s.mission->thumbnail_path);
                                                                            return !s.path.empty();
                                                                        case 6:c.thumbnail_error="The thumbnail path could not be converted.";
                                                                            return 0;
                                                                        case 7:InvalidateRect(c.preview,nullptr,TRUE);return 0;
                                                                        case 8: {
                                                                            std::unique_ptr<Gdiplus::Image> image(
                                                                                Gdiplus::Image::FromFile(s.path.c_str(),FALSE));
                                                                            if(!image||image->GetLastStatus()!=Gdiplus::Ok||
                                                                                image->GetWidth()==0||image->GetHeight()==0)
                                                                                c.thumbnail_error="The mission thumbnail could not be loaded.";
                                                                            else c.thumbnail=std::move(image);
                                                                            return 0;
                                                                        }
                                                                        default:return 0;
                                                                        }}catch(...){s.error=std::current_exception();return INT64_MIN;}
                                                                    };
                                                                    a2fo_rs_c_mission_thumbnail(&state,step);
                                                                    if(state.error)std::rethrow_exception(state.error);
                                                                }

                                                                void update_mission_details(SelectorContext& context) {
                                                                    struct State {
                                                                        SelectorContext& selector;
                                                                        const CampaignEntry* campaign=nullptr;
                                                                        const MissionEntry* mission=nullptr;
                                                                        std::string details{};
                                                                        std::exception_ptr error{};
                                                                    } state{context};
                                                                    const auto step=+[](void* opaque,std::uint32_t operation,
                                                                        std::size_t) noexcept ->std::int64_t {
                                                                        auto& s=*static_cast<State*>(opaque);
                                                                        auto& c=s.selector;
                                                                        try {switch(operation) {
                                                                        case 0:s.campaign=selected_campaign(c);
                                                                            return s.campaign!=nullptr;
                                                                        case 1:s.mission=selected_mission(c);
                                                                            return s.mission!=nullptr;
                                                                        case 2:SetWindowTextA(c.details,"No mission is available.");
                                                                            EnableWindow(c.start,FALSE);
                                                                            c.thumbnail.reset();
                                                                            InvalidateRect(c.preview,nullptr,TRUE);
                                                                            InvalidateRect(c.dialog,nullptr,TRUE);
                                                                            InvalidateRect(c.overview,nullptr,FALSE);
                                                                            return 0;
                                                                        case 3:s.details=s.mission->title+"\r\n\r\n"+
                                                                            s.mission->description;return 0;
                                                                        case 4:return !s.mission->objectives.empty();
                                                                        case 5:s.details+="\r\n\r\nObjectives\r\n"+
                                                                            s.mission->objectives;return 0;
                                                                        case 6:return s.mission->unlocked;
                                                                        case 7:s.details+="\r\n\r\nThis mission is locked by native campaign progression.";
                                                                            return 0;
                                                                        case 8:SetWindowTextA(c.details,s.details.c_str());
                                                                            EnableWindow(c.start,s.mission->unlocked?TRUE:FALSE);
                                                                            return 0;
                                                                        case 9:load_thumbnail(c);return 0;
                                                                        case 10:InvalidateRect(c.dialog,nullptr,TRUE);
                                                                            InvalidateRect(c.overview,nullptr,FALSE);return 0;
                                                                        default:return 0;
                                                                        }}catch(...){s.error=std::current_exception();return INT64_MIN;}
                                                                    };
                                                                    a2fo_rs_c_mission_details(&state,step);
                                                                    if(state.error)std::rethrow_exception(state.error);
                                                                }

                                                                void populate_missions(SelectorContext& context) {
                                                                    struct State {
                                                                        SelectorContext& selector;
                                                                        const CampaignEntry* campaign=nullptr;
                                                                        std::exception_ptr error{};
                                                                    } state{context};
                                                                    const auto step=+[](void* opaque,std::uint32_t operation,
                                                                        std::size_t index) noexcept ->std::int64_t {
                                                                        auto& s=*static_cast<State*>(opaque);
                                                                        auto& c=s.selector;
                                                                        try {switch(operation) {
                                                                        case 0:SendMessageA(c.mission_list,LB_RESETCONTENT,0,0);return 0;
                                                                        case 1:s.campaign=selected_campaign(c);
                                                                            load_campaign_background(c);
                                                                            return s.campaign!=nullptr;
                                                                        case 2:SetWindowTextA(c.overview,"");
                                                                            InvalidateRect(c.overview,nullptr,FALSE);
                                                                            redraw_dialog_background(c);return 0;
                                                                        case 3:SetWindowTextA(c.overview,s.campaign->overview.c_str());
                                                                            InvalidateRect(c.overview,nullptr,FALSE);
                                                                            redraw_dialog_background(c);return 0;
                                                                        case 4:return static_cast<std::int64_t>(
                                                                            s.campaign->missions.size());
                                                                        case 5: {
                                                                            const MissionEntry& mission=s.campaign->missions[index];
                                                                            const std::string label=mission.unlocked
                                                                                ? mission.title:"[Locked] "+mission.title;
                                                                            const LRESULT row=SendMessageA(c.mission_list,
                                                                                LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
                                                                            if(row!=LB_ERR&&row!=LB_ERRSPACE)
                                                                                SendMessageA(c.mission_list,LB_SETITEMDATA,
                                                                                    static_cast<WPARAM>(row),index);
                                                                            return 0;
                                                                        }
                                                                        case 6:c.mission_position=0;return 0;
                                                                        case 7:return static_cast<std::int64_t>(
                                                                            s.campaign->missions.size());
                                                                        case 8:return s.campaign->missions[index].unlocked;
                                                                        case 9:c.mission_position=static_cast<int>(index);return 0;
                                                                        case 10:SendMessageA(c.mission_list,LB_SETCURSEL,
                                                                            c.mission_position,0);return 0;
                                                                        case 11:update_mission_details(c);return 0;
                                                                        default:return 0;
                                                                        }}catch(...){s.error=std::current_exception();return INT64_MIN;}
                                                                    };
                                                                    a2fo_rs_c_mission_populate_missions(&state,step);
                                                                    if(state.error)std::rethrow_exception(state.error);
                                                                }

                                                                void populate_campaigns(SelectorContext& context) {
                                                                    struct State {SelectorContext& selector;std::exception_ptr error{};}
                                                                        state{context};
                                                                    const auto step=+[](void* opaque,std::uint32_t operation,
                                                                        std::size_t index) noexcept ->std::int64_t {
                                                                        auto& s=*static_cast<State*>(opaque);
                                                                        auto& c=s.selector;
                                                                        try {switch(operation) {
                                                                        case 0:return static_cast<std::int64_t>(c.campaigns.size());
                                                                        case 1: {
                                                                            const CampaignEntry& campaign=c.campaigns[index];
                                                                            const std::string label=campaign.unlocked
                                                                                ? campaign.title:"[Locked] "+campaign.title;
                                                                            const LRESULT row=SendMessageA(c.campaign_list,
                                                                                LB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str()));
                                                                            if(row!=LB_ERR&&row!=LB_ERRSPACE)
                                                                                SendMessageA(c.campaign_list,LB_SETITEMDATA,
                                                                                    static_cast<WPARAM>(row),index);
                                                                            return 0;
                                                                        }
                                                                        case 2:c.campaign=0;
                                                                            SendMessageA(c.campaign_list,LB_SETCURSEL,0,0);
                                                                            return 0;
                                                                        case 3:populate_missions(c);return 0;
                                                                        default:return 0;
                                                                        }}catch(...){s.error=std::current_exception();return INT64_MIN;}
                                                                    };
                                                                    a2fo_rs_c_mission_populate_campaigns(&state,step);
                                                                    if(state.error)std::rethrow_exception(state.error);
                                                                }

                                                                void layout_controls(SelectorContext& context) {
                                                                    if(!context.dialog)return;
                                                                    RECT client{};
                                                                    GetClientRect(context.dialog,&client);
                                                                    A2foStage8MissionLayout layout{};
                                                                    layout.width=client.right-client.left;
                                                                    layout.height=client.bottom-client.top;
                                                                    layout.has_height_image=context.button_images[0]!=nullptr;
                                                                    layout.image_height=layout.has_height_image
                                                                        ? static_cast<int>(context.button_images[0]->GetHeight()):0;
                                                                    layout.has_width_image=context.button_images[0]!=nullptr;
                                                                    layout.image_width=layout.has_width_image
                                                                        ? static_cast<int>(context.button_images[0]->GetWidth()):0;
                                                                    a2fo_rs_c_mission_layout(&layout);
                                                                    MoveWindow(context.campaign_list,
                                                                        layout.rectangles[0][0],layout.rectangles[0][1],
                                                                        layout.rectangles[0][2],layout.rectangles[0][3],TRUE);
                                                                    MoveWindow(context.mission_list,
                                                                        layout.rectangles[1][0],layout.rectangles[1][1],
                                                                        layout.rectangles[1][2],layout.rectangles[1][3],TRUE);
                                                                    MoveWindow(context.preview,
                                                                        layout.rectangles[2][0],layout.rectangles[2][1],
                                                                        layout.rectangles[2][2],layout.rectangles[2][3],TRUE);
                                                                    MoveWindow(context.details,
                                                                        layout.rectangles[3][0],layout.rectangles[3][1],
                                                                        layout.rectangles[3][2],layout.rectangles[3][3],TRUE);
                                                                    MoveWindow(context.overview,
                                                                        layout.rectangles[4][0],layout.rectangles[4][1],
                                                                        layout.rectangles[4][2],layout.rectangles[4][3],TRUE);
                                                                    MoveWindow(context.start,
                                                                        layout.rectangles[5][0],layout.rectangles[5][1],
                                                                        layout.rectangles[5][2],layout.rectangles[5][3],TRUE);
                                                                    MoveWindow(context.back,
                                                                        layout.rectangles[6][0],layout.rectangles[6][1],
                                                                        layout.rectangles[6][2],layout.rectangles[6][3],TRUE);
                                                                }

#include "standard_buttons.inl"

                                                                void draw_campaign_overview(
                                                                    SelectorContext& context,
                                                                    const DRAWITEMSTRUCT* item) noexcept {
                                                                        if (!item || !context.overview) return;
                                                                        POINT origin{};
                                                                        MapWindowPoints(
                                                                            context.overview, context.dialog,
                                                                            &origin, 1);
                                                                        draw_dialog_background_region(
                                                                            context, item->hDC, item->rcItem,
                                                                            origin.x, origin.y);

                                                                        const CampaignEntry* campaign =
                                                                            selected_campaign(context);
                                                                        if (!campaign || campaign->overview.empty()) return;
                                                                        const int old_background_mode =
                                                                            SetBkMode(item->hDC, TRANSPARENT);
                                                                        const COLORREF old_text_colour = SetTextColor(
                                                                            item->hDC, RGB(230, 235, 240));
                                                                        HGDIOBJ old_font = SelectObject(
                                                                            item->hDC, g_heading_font);
                                                                        RECT text_rectangle = item->rcItem;
                                                                        DrawTextA(
                                                                            item->hDC, campaign->overview.c_str(), -1,
                                                                            &text_rectangle,
                                                                            DT_LEFT | DT_TOP | DT_WORDBREAK |
                                                                            DT_NOPREFIX);
                                                                        SelectObject(item->hDC, old_font);
                                                                        SetTextColor(item->hDC, old_text_colour);
                                                                        SetBkMode(item->hDC, old_background_mode);
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

                                                                        template <std::size_t Size>
                                                                        void copy_scene_text(char (&destination)[Size],
                                                                            const std::string& source) noexcept {
                                                                            const std::size_t length =
                                                                                a2fo_rs_c_mission_copy_length(source.size(),Size);
                                                                            std::memcpy(destination, source.data(), length);
                                                                            destination[length] = '\0';
                                                                        }

                                                                        void restore_launch_filename_override() noexcept {
                                                                            struct State {const char** filenames=nullptr;} state;
                                                                            const auto step=+[](void* opaque,std::uint32_t operation,
                                                                                std::size_t row) noexcept ->std::int64_t {
                                                                                auto& s=*static_cast<State*>(opaque);
                                                                                switch(operation) {
                                                                                case 0:return static_cast<std::int64_t>(g_launch_filename_row);
                                                                                case 1:s.filenames=at<const char*>(g_armada,
                                                                                    kMissionFilenameTableRva);
                                                                                    return readable_range(s.filenames,
                                                                                        sizeof(const char*)*kMissionTableSize);
                                                                                case 2:return s.filenames[row]==
                                                                                    g_launch_filename_storage.data();
                                                                                case 3:s.filenames[row]=g_original_launch_filename;
                                                                                    return 0;
                                                                                case 4:g_original_launch_filename=nullptr;
                                                                                    g_launch_filename_row=kMissionTableSize;
                                                                                    g_launch_filename_storage.fill('\0');return 0;
                                                                                default:return 0;
                                                                                }
                                                                            };
                                                                            a2fo_rs_c_mission_restore(&state,kMissionTableSize,step);
                                                                        }

                                                                        bool retain_launch_filename_override(
                                                                            const std::string& filename,
                                                                            std::size_t launch_row) noexcept {
                                                                            struct State {
                                                                                const std::string& filename;
                                                                                std::size_t row;
                                                                                const char** filenames=nullptr;
                                                                            } state{filename,launch_row};
                                                                            const auto step=+[](void* opaque,std::uint32_t operation,
                                                                                std::size_t) noexcept ->std::int64_t {
                                                                                auto& s=*static_cast<State*>(opaque);
                                                                                switch(operation) {
                                                                                case 0:restore_launch_filename_override();return 0;
                                                                                case 1:return static_cast<std::int64_t>(
                                                                                    g_launch_filename_storage.size());
                                                                                case 2:s.filenames=at<const char*>(g_armada,
                                                                                    kMissionFilenameTableRva);
                                                                                    return readable_range(s.filenames,
                                                                                        sizeof(const char*)*kMissionTableSize);
                                                                                case 3:
                                                                                    std::copy(s.filename.begin(),s.filename.end(),
                                                                                        g_launch_filename_storage.begin());
                                                                                    g_launch_filename_storage[s.filename.size()]='\0';
                                                                                    g_original_launch_filename=s.filenames[s.row];
                                                                                    g_launch_filename_row=s.row;
                                                                                    s.filenames[s.row]=g_launch_filename_storage.data();
                                                                                    return 0;
                                                                                default:return 0;
                                                                                }
                                                                            };
                                                                            return a2fo_rs_c_mission_retain(&state,filename.size(),
                                                                                launch_row,kMissionTableSize,step);
                                                                        }

                                                                        void start_selected_mission(SelectorContext& context) {
                                                                            struct Context {
                                                                                SelectorContext& selector;
                                                                                const CampaignEntry* campaign=nullptr;
                                                                                const MissionEntry* mission=nullptr;
                                                                                std::int32_t* campaign_index=nullptr;
                                                                                std::int8_t* mission_index=nullptr;
                                                                                const char** filenames=nullptr;
                                                                                SetupMissionFunction setup=nullptr;
                                                                                std::exception_ptr error{};
                                                                            } state{context};
                                                                            const auto step=+[](void* opaque,std::uint32_t op,
                                                                                                std::size_t row) noexcept ->std::int64_t {
                                                                                auto& c=*static_cast<Context*>(opaque);
                                                                                try{switch(op){
                                                                                case 0:c.campaign=selected_campaign(c.selector);return c.campaign!=nullptr;
                                                                                case 1:c.mission=selected_mission(c.selector);return c.mission!=nullptr;
                                                                                case 2:return c.campaign->unlocked;
                                                                                case 3:return c.mission->unlocked;
                                                                                case 4:MessageBeep(MB_ICONWARNING);return 0;
                                                                                case 5:
                                                                                    c.campaign_index=at<std::int32_t>(g_armada,kCurrentCampaignRva);
                                                                                    c.mission_index=at<std::int8_t>(g_armada,kSelectedMissionRva);
                                                                                    return readable_range(c.campaign_index,sizeof(*c.campaign_index))&&
                                                                                           readable_range(c.mission_index,sizeof(*c.mission_index));
                                                                                case 6:MessageBoxA(c.selector.dialog,
                                                                                    "The native campaign selection state is unavailable.",
                                                                                    "Mission Selector",MB_OK|MB_ICONERROR);return 0;
                                                                                case 7:return c.mission->launch_campaign;
                                                                                case 8:return c.mission->launch_mission;
                                                                                case 9:MessageBoxA(c.selector.dialog,
                                                                                    "The configured native launch slot is invalid.",
                                                                                    "Mission Selector",MB_OK|MB_ICONERROR);return 0;
                                                                                case 10:
                                                                                    c.filenames=at<const char*>(g_armada,kMissionFilenameTableRva);
                                                                                    return readable_range(c.filenames,
                                                                                        sizeof(const char*)*kCampaignCount*kMissionsPerCampaign);
                                                                                case 11:MessageBoxA(c.selector.dialog,
                                                                                    "The native mission filename table is unavailable.",
                                                                                    "Mission Selector",MB_OK|MB_ICONERROR);return 0;
                                                                                case 12:
                                                                                    *c.campaign_index=c.mission->launch_campaign;
                                                                                    *c.mission_index=static_cast<std::int8_t>(c.mission->launch_mission);
                                                                                    restore_launch_filename_override();return 0;
                                                                                case 13:return c.mission->replace_launch_filename;
                                                                                case 14:return retain_launch_filename_override(c.mission->filename,row);
                                                                                case 15:MessageBoxA(c.selector.dialog,
                                                                                    "The configured mission filename could not be retained for launch.",
                                                                                    "Mission Selector",MB_OK|MB_ICONERROR);return 0;
                                                                                case 16:
                                                                                    log_line("Launching "+c.mission->filename+
                                                                                        " through native campaign "+
                                                                                        std::to_string(c.mission->launch_campaign)+
                                                                                        ", mission "+std::to_string(c.mission->launch_mission));
                                                                                    return 0;
                                                                                case 17:
                                                                                    if(const auto set_mission=scene_export<A2FO_MenuSceneMissionV1>(
                                                                                            "A2FORefitMenus_SetMissionV1")){
                                                                                        A2FO_MenuMissionV1 details{};
                                                                                        details.size=sizeof(details);details.version=1;
                                                                                        copy_scene_text(details.map,c.mission->filename);
                                                                                        copy_scene_text(details.title,c.mission->title);
                                                                                        copy_scene_text(details.briefing,c.mission->description);
                                                                                        copy_scene_text(details.objectives,c.mission->objectives);
                                                                                        set_mission(&details);
                                                                                    }return 0;
                                                                                case 18:
                                                                                    if(const auto begin=scene_export<A2FO_MenuSceneBeginDepartureV1>(
                                                                                            "A2FORefitMenus_BeginDepartureV1")){
                                                                                        const std::string& view=selected_menu_view(c.selector);
                                                                                        c.selector.departure_started=begin(view.c_str())!=FALSE;
                                                                                    }return 0;
                                                                                case 19:
                                                                                    c.setup=reinterpret_cast<SetupMissionFunction>(
                                                                                        at(g_armada,kSetupMissionRva));
                                                                                    InterlockedExchange(&g_accept_selected_mission,1);return 0;
                                                                                case 20:return c.setup&&c.setup(c.selector.dialog,
                                                                                    &c.selector.native_preview_handle);
                                                                                case 21:InterlockedExchange(&g_accept_selected_mission,0);return 0;
                                                                                case 22:
                                                                                    if(c.selector.departure_started){
                                                                                        if(const auto end=scene_export<A2FO_MenuSceneEndDepartureV1>(
                                                                                                "A2FORefitMenus_EndDepartureV1"))end();
                                                                                        c.selector.departure_started=false;
                                                                                    }return 0;
                                                                                case 23:restore_launch_filename_override();return 0;
                                                                                case 24:
                                                                                    if(IsWindow(c.selector.dialog))MessageBoxA(c.selector.dialog,
                                                                                        "Armada rejected the selected native mission row.",
                                                                                        "Mission Selector",MB_OK|MB_ICONERROR);
                                                                                    return 0;
                                                                                default:return 0;
                                                                                }}catch(...){c.error=std::current_exception();return INT64_MIN;}
                                                                            };
                                                                            a2fo_rs_c_mission_start(&state,step);
                                                                            if(state.error)std::rethrow_exception(state.error);
                                                                        }

                                                                        void exit_to_main_menu(HWND dialog, const char* source) noexcept {
                                                                            struct State {
                                                                                HWND dialog;
                                                                                const char* source;
                                                                                std::int32_t* shell_state=nullptr;
                                                                            } state{dialog,source};
                                                                            const auto step=+[](void* opaque,std::uint32_t operation,
                                                                                std::size_t) noexcept ->std::int64_t {
                                                                                auto& s=*static_cast<State*>(opaque);
                                                                                switch(operation) {
                                                                                case 0:s.shell_state=at<std::int32_t>(g_armada,
                                                                                    kShellStateRva);
                                                                                    return readable_range(s.shell_state,
                                                                                        sizeof(*s.shell_state));
                                                                                case 1:*s.shell_state=kMainMenuShellState;
                                                                                    log_line(std::string("Selector exit via ")+
                                                                                        s.source+"; shell state set to main menu");
                                                                                    return 0;
                                                                                case 2:log_line(std::string("Selector exit via ")+
                                                                                    s.source+"; shell state address is unavailable");
                                                                                    return 0;
                                                                                case 3:SetLastError(ERROR_SUCCESS);
                                                                                    return EndDialog(s.dialog,1);
                                                                                case 4:log_line("EndDialog failed while leaving the mission selector (error "+
                                                                                    std::to_string(GetLastError())+")");return 0;
                                                                                default:return 0;
                                                                                }
                                                                            };
                                                                            a2fo_rs_c_mission_exit(&state,step);
                                                                        }

                                                                        bool shell_client_rectangle(
                                                                            HWND shell,RECT* output) noexcept {
                                                                            if(!shell||!output)return false;
                                                                            struct State {
                                                                                HWND shell;
                                                                                RECT client{};
                                                                                POINT origin{};
                                                                                A2foStage8MissionShellRect geometry{};
                                                                            } state{shell};
                                                                            const auto step=+[](void* opaque,std::uint32_t operation,
                                                                                std::size_t) noexcept ->std::int64_t {
                                                                                auto& s=*static_cast<State*>(opaque);
                                                                                switch(operation) {
                                                                                case 0:return GetClientRect(s.shell,&s.client);
                                                                                case 1:return ClientToScreen(s.shell,&s.origin);
                                                                                case 2:s.geometry.origin_x=s.origin.x;
                                                                                    s.geometry.origin_y=s.origin.y;
                                                                                    s.geometry.client_left=s.client.left;
                                                                                    s.geometry.client_top=s.client.top;
                                                                                    s.geometry.client_right=s.client.right;
                                                                                    s.geometry.client_bottom=s.client.bottom;
                                                                                    return 0;
                                                                                default:return 0;
                                                                                }
                                                                            };
                                                                            const bool valid=a2fo_rs_c_mission_shell_rect(
                                                                                &state,&state.geometry,step);
                                                                            if(state.geometry.filled){
                                                                                output->left=state.geometry.left;
                                                                                output->top=state.geometry.top;
                                                                                output->right=state.geometry.right;
                                                                                output->bottom=state.geometry.bottom;
                                                                            }
                                                                            return valid;
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
                                                                                if (!shell_client_rectangle(
                                                                                        owner, &owner_rectangle)) {
                                                                                    owner_rectangle = {0, 0, GetSystemMetrics(SM_CXSCREEN),
                                                                                        GetSystemMetrics(SM_CYSCREEN)};
                                                                                }
                                                                                const int owner_width = owner_rectangle.right - owner_rectangle.left;
                                                                                const int owner_height = owner_rectangle.bottom - owner_rectangle.top;
                                                                                SetWindowPos(dialog, HWND_TOP,
                                                                                             owner_rectangle.left,
                                                                                             owner_rectangle.top,
                                                                                             owner_width, owner_height,
                                                                                             SWP_NOOWNERZORDER);

                                                                                context->overview = CreateWindowExA(
                                                                                    0, "STATIC", "", WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
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
load_standard_buttons(*context);
for (HWND button : {context->start, context->back}) {
    SetWindowSubclass(button, &standard_button_subclass, 1,
                      reinterpret_cast<DWORD_PTR>(context));
}
                                                                                     layout_controls(*context);
                                                                                     populate_campaigns(*context);
                                                                                     SetFocus(context->campaign_list);
                                                                                     return FALSE;
                                                                            }
                                                                            if (!context) return FALSE;

                                                                            switch (message) {
                                                                                case WM_ERASEBKGND:
                                                                                    draw_dialog_background(
                                                                                        *context, reinterpret_cast<HDC>(wparam));
                                                                                    return TRUE;
                                                                                case WM_SIZE:
                                                                                    layout_controls(*context);
                                                                                    InvalidateRect(dialog, nullptr, TRUE);
                                                                                    return TRUE;
                                                                                case WM_COMMAND: {
                                                                                    struct State {
                                                                                        SelectorContext& selector;
                                                                                        HWND dialog;
                                                                                        std::exception_ptr error{};
                                                                                    } state{*context,dialog};
                                                                                    const auto step=+[](void* opaque,std::uint32_t operation,
                                                                                        std::size_t argument) noexcept ->std::int64_t {
                                                                                        auto& s=*static_cast<State*>(opaque);
                                                                                        auto& c=s.selector;
                                                                                        try{switch(operation) {
                                                                                        case 0:return SendMessageA(c.campaign_list,
                                                                                            LB_GETCURSEL,0,0);
                                                                                        case 1:return SendMessageA(c.campaign_list,
                                                                                            LB_GETITEMDATA,argument,0);
                                                                                        case 2:return static_cast<LRESULT>(
                                                                                            c.campaigns.size());
                                                                                        case 3:c.campaign=static_cast<int>(argument);
                                                                                            populate_missions(c);return 0;
                                                                                        case 4:return SendMessageA(c.mission_list,
                                                                                            LB_GETCURSEL,0,0);
                                                                                        case 5:return SendMessageA(c.mission_list,
                                                                                            LB_GETITEMDATA,argument,0);
                                                                                        case 6:c.mission_position=static_cast<int>(argument);
                                                                                            update_mission_details(c);return 0;
                                                                                        case 7:start_selected_mission(c);return 0;
                                                                                        case 8:exit_to_main_menu(s.dialog,"Back button");return 0;
                                                                                        case 9:exit_to_main_menu(s.dialog,"Escape key");return 0;
                                                                                        default:return 0;
                                                                                        }}catch(...){s.error=std::current_exception();return INT64_MIN;}
                                                                                    };
                                                                                    const A2foStage8MissionCommand codes{
                                                                                        LOWORD(wparam),HIWORD(wparam),kCampaignListId,
                                                                                        kMissionListId,kStartId,kBackId,IDCANCEL,
                                                                                        LBN_SELCHANGE,LBN_DBLCLK,BN_CLICKED,LB_ERR};
                                                                                    const auto result=a2fo_rs_c_mission_command(
                                                                                        &state,codes,step);
                                                                                    if(state.error)std::rethrow_exception(state.error);
                                                                                    if(result==1)return TRUE;
                                                                                    break;
                                                                                }
                                                                                case WM_DRAWITEM: {
                                                                                    const auto* item = reinterpret_cast<DRAWITEMSTRUCT*>(lparam);
                                                                                    if (!item) return FALSE;
                                                                                    if (item->CtlID == kOverviewId) {
                                                                                        draw_campaign_overview(*context, item);
                                                                                    } else if (item->CtlID == kStartId || item->CtlID == kBackId) {
draw_button(*context, item);
                                                                                    } else if (item->CtlID == kCampaignListId ||
                                                                                        item->CtlID == kMissionListId) {
                                                                                        draw_list_item(*context, item);
                                                                                        } else if (item->CtlID == kPreviewId) {
                                                                                            draw_preview(*context, item);
                                                                                        }
                                                                                        return TRUE;
                                                                                }
                                                                                case WM_CTLCOLORSTATIC: {
                                                                                    HDC dc = reinterpret_cast<HDC>(wparam);
                                                                                    SetTextColor(dc, RGB(230, 235, 240));
                                                                                    SetBkColor(dc, RGB(8, 10, 14));
                                                                                    return reinterpret_cast<INT_PTR>(g_background_brush);
                                                                                }
                                                                                case WM_CTLCOLOREDIT:
                                                                                case WM_CTLCOLORLISTBOX: {
                                                                                    HDC dc = reinterpret_cast<HDC>(wparam);
                                                                                    SetTextColor(dc, RGB(230, 235, 240));
                                                                                    SetBkColor(dc, RGB(10, 12, 16));
                                                                                    return reinterpret_cast<INT_PTR>(g_field_brush);
                                                                                }
                                                                                case WM_CLOSE:
                                                                                    exit_to_main_menu(dialog, "window close");
                                                                                    return TRUE;
                                                                                default:
                                                                                    break;
                                                                            }
                                                                            return FALSE;
                                                                            }

                                                                            INT_PTR show_selector_dialog() {
                                                                                struct State {
                                                                                    SelectorContext context{};
                                                                                    HWND owner=nullptr;
                                                                                    std::exception_ptr error{};
                                                                                } state;
                                                                                const auto step=+[](void* opaque,std::uint32_t operation,
                                                                                    std::size_t argument) noexcept ->std::int64_t {
                                                                                    auto& s=*static_cast<State*>(opaque);
                                                                                    try {switch(operation) {
                                                                                    case 0:
                                                                                        s.context.campaigns=build_catalog();
                                                                                        return static_cast<std::int64_t>(s.context.campaigns.size());
                                                                                    case 1:return !s.context.campaigns[argument].missions.empty();
                                                                                    case 2:log_line("Native mission filename table was empty; using Armada's selector");
                                                                                        return 0;
                                                                                    case 3:s.owner=native_dialog_host();return s.owner!=nullptr;
                                                                                    case 4:log_line("Armada's native shell window is unavailable; using its original selector");
                                                                                        return 0;
                                                                                    case 5:return DialogBoxParamA(
                                                                                        reinterpret_cast<HINSTANCE>(g_armada),
                                                                                        MAKEINTRESOURCEA(kSinglePlayerDialogResource),
                                                                                        s.owner,&selector_dialog_proc,
                                                                                        reinterpret_cast<LPARAM>(&s.context));
                                                                                    default:return 0;
                                                                                    }}catch(...){s.error=std::current_exception();return INT64_MIN;}
                                                                                };
                                                                                const auto result=a2fo_rs_c_mission_show(&state,step);
                                                                                if(state.error)std::rethrow_exception(state.error);
                                                                                return static_cast<INT_PTR>(result);
                                                                            }

                                                                            using DoSingleFunction = int (__cdecl*)();
                                                                            using DoMissionSelectFunction = int (__cdecl*)(HWND);

                                                                            int __cdecl do_single_hook() noexcept {
                                                                                const auto step=+[](void*,std::uint32_t operation,
                                                                                    std::size_t result) noexcept ->std::int64_t {
                                                                                    switch(operation) {
                                                                                    case 0:
                                                                                        try{return show_selector_dialog();}
                                                                                        catch(...){return INT64_MIN;}
                                                                                    case 1:
                                                                                        try{log_line("Selector dialog returned "+
                                                                                            std::to_string(static_cast<INT_PTR>(result)));
                                                                                            return 0;
                                                                                        }catch(...){return INT64_MIN;}
                                                                                    case 2:log_line("Replacement selector raised an exception; using Armada's selector");
                                                                                        return 0;
                                                                                    case 3: {
                                                                                        const auto original=reinterpret_cast<DoSingleFunction>(
                                                                                            g_do_single_hook.gateway);
                                                                                        return original ? original():0;
                                                                                    }
                                                                                    default:return 0;
                                                                                    }
                                                                                };
                                                                                return a2fo_rs_c_mission_single_hook(nullptr,step);
                                                                            }

                                                                            int __cdecl do_mission_select_hook(HWND parent) noexcept {
                                                                                const auto step=+[](void* opaque,std::uint32_t operation,
                                                                                    std::size_t) noexcept ->std::int64_t {
                                                                                    if(operation==0)
                                                                                        return InterlockedCompareExchange(
                                                                                            &g_accept_selected_mission,0,1);
                                                                                    const auto original=reinterpret_cast<DoMissionSelectFunction>(
                                                                                        g_do_mission_select_hook.gateway);
                                                                                    return original ? original(static_cast<HWND>(opaque)):0;
                                                                                };
                                                                                return a2fo_rs_c_mission_accept_hook(parent,step);
                                                                            }

                                                                            void initialize_drawing_resources() noexcept {
                                                                                const auto step=+[](void*,std::uint32_t operation,
                                                                                    std::size_t) noexcept ->std::int64_t {
                                                                                    switch(operation) {
                                                                                    case 0:return g_background_brush!=nullptr;
                                                                                    case 1:g_background_brush=CreateSolidBrush(RGB(8,10,14));return 0;
                                                                                    case 2:return g_field_brush!=nullptr;
                                                                                    case 3:g_field_brush=CreateSolidBrush(RGB(10,12,16));return 0;
                                                                                    case 4:return g_regular_font!=nullptr;
                                                                                    case 5:g_regular_font=CreateFontA(-16,0,0,0,FW_NORMAL,
                                                                                        FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                                                                                        CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                                                                                        DEFAULT_PITCH|FF_DONTCARE,"Segoe UI");return 0;
                                                                                    case 6:return g_heading_font!=nullptr;
                                                                                    case 7:g_heading_font=CreateFontA(-20,0,0,0,FW_SEMIBOLD,
                                                                                        FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,
                                                                                        CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,
                                                                                        DEFAULT_PITCH|FF_DONTCARE,"Segoe UI");return 0;
                                                                                    case 8:return g_gdiplus_token!=0;
                                                                                    case 9:{Gdiplus::GdiplusStartupInput input;
                                                                                        return Gdiplus::GdiplusStartup(
                                                                                            &g_gdiplus_token,&input,nullptr)==Gdiplus::Ok;}
                                                                                    case 10:g_gdiplus_token=0;
                                                                                        log_line("GDI+ was unavailable; mission images are disabled");
                                                                                        return 0;
                                                                                    default:return 0;
                                                                                    }
                                                                                };
                                                                                a2fo_rs_c_mission_drawing_init(nullptr,step);
                                                                            }

                                                                            void cache_roots() {
                                                                                struct State {std::exception_ptr error{};} state;
                                                                                const auto step=+[](void* opaque,std::uint32_t operation,
                                                                                    std::size_t index) noexcept ->std::int64_t {
                                                                                    auto& s=*static_cast<State*>(opaque);
                                                                                    try{switch(operation) {
                                                                                    case 0:g_roots.clear();return 0;
                                                                                    case 1:return g_api&&g_api->extension_root_count&&
                                                                                        g_api->extension_root;
                                                                                    case 2:return g_api->extension_root_count();
                                                                                    case 3:{const char* root=g_api->extension_root(
                                                                                        static_cast<std::uint32_t>(index));
                                                                                        if(root&&*root)g_roots.emplace_back(root);
                                                                                        return 0;}
                                                                                    default:return 0;
                                                                                    }}catch(...){s.error=std::current_exception();return INT64_MIN;}
                                                                                };
                                                                                a2fo_rs_c_mission_cache_roots(&state,step);
                                                                                if(state.error)std::rethrow_exception(state.error);
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
    struct Context {const A2FO_ModuleApi* api;std::exception_ptr error{};}
        context{api};
    const auto step=+[](void* opaque,std::uint32_t op,
                        std::size_t) noexcept ->std::int64_t {
        auto& c=*static_cast<Context*>(opaque);
        try{switch(op){
        case 0:g_armada=static_cast<HMODULE>(c.api->armada_module());return g_armada!=nullptr;
        case 1:return signature_matches(kDoSingleRva,kExpectedDoSingle);
        case 2:return signature_matches(kDoMissionSelectRva,kExpectedDoMissionSelect);
        case 3:return signature_matches(kCampaignAvailableRva,kExpectedCampaignAvailable);
        case 4:return signature_matches(kSetupMissionRva,kExpectedSetupMission);
        case 5:
            log_line("Single-player shell signatures did not match; replacement disabled");
            g_api=nullptr;g_armada=nullptr;return 0;
        case 6:cache_roots();return 0;
        case 7:initialize_drawing_resources();return 0;
        case 8:{INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_STANDARD_CLASSES};
            InitCommonControlsEx(&controls);return 0;}
        case 9:return c.api->install_inline_hook(
            at(g_armada,kDoMissionSelectRva),
            reinterpret_cast<void*>(&do_mission_select_hook),
            kExpectedDoMissionSelect.size(),kExpectedDoMissionSelect.data(),
            &g_do_mission_select_hook);
        case 10:log_line("Native mission-acceptance bridge installation failed");return 0;
        case 11:return c.api->install_inline_hook(
            at(g_armada,kDoSingleRva),reinterpret_cast<void*>(&do_single_hook),
            kExpectedDoSingle.size(),kExpectedDoSingle.data(),&g_do_single_hook);
        case 12:log_line("Single-player entry hook failed; native selector remains active");return 0;
        case 13:log_line("Combined campaign and mission selector enabled; native SetupMission launch retained");return 0;
        default:return 0;
        }}catch(...){c.error=std::current_exception();return INT64_MIN;}
    };
    const bool result=a2fo_rs_c_mission_init(&context,step);
    if(context.error)std::rethrow_exception(context.error);
    return result;
}

extern "C" __declspec(dllexport)
void A2FO_CALL A2FO_ModuleShutdown() {
    // Hooks are process-lifetime. Core shutdown occurs after shell callbacks stop.
    const auto step=+[](void*,std::uint32_t operation,std::size_t) noexcept ->std::int64_t {
        switch(operation) {
        case 0:restore_launch_filename_override();return 0;
        case 1:return g_gdiplus_token!=0;
        case 2:Gdiplus::GdiplusShutdown(g_gdiplus_token);g_gdiplus_token=0;return 0;
        case 3:return g_regular_font!=nullptr;
        case 4:DeleteObject(g_regular_font);return 0;
        case 5:return g_heading_font!=nullptr;
        case 6:DeleteObject(g_heading_font);return 0;
        case 7:return g_background_brush!=nullptr;
        case 8:DeleteObject(g_background_brush);return 0;
        case 9:return g_field_brush!=nullptr;
        case 10:DeleteObject(g_field_brush);return 0;
        case 11:g_regular_font=nullptr;g_heading_font=nullptr;
            g_background_brush=nullptr;g_field_brush=nullptr;return 0;
        case 12:g_roots.clear();return 0;
        case 13:g_api=nullptr;g_armada=nullptr;return 0;
        default:return 0;
        }
    };
    a2fo_rs_c_mission_shutdown(nullptr,step);
}
