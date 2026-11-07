/* Per-player XP routed through Fleet Operations' existing rank chain. */
#include "../../sdk/include/a2fo_module_api.h"
#include "../../rust/module_runtime_a5_ffi.h"
#include "../../sdk/include/a2fo_supported_armada.hpp"
#include "../../sdk/include/a2fo_supported_fleetops.hpp"
#include "collective_xp.hpp"
#include "native_layout.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iterator>
#include <map>
#include <set>
#include <string>
#include <vector>

extern "C" {
void a2fo_collective_award_bridge();
void a2fo_collective_level_bridge();
void a2fo_collective_sleeper_bridge();
void a2fo_collective_saboteur_bridge();
void* a2fo_collective_award_continue = nullptr;
void* a2fo_collective_sleeper_continue = nullptr;
void* a2fo_collective_saboteur_continue = nullptr;
void* a2fo_collective_level_gateway = nullptr;
std::uintptr_t __cdecl a2fo_collective_delphi2(void*, std::uintptr_t, std::uintptr_t);
unsigned __cdecl a2fo_collective_thiscall1(void*, void*, void*);
}

namespace {
namespace xp = a2fo::collective_xp;
namespace native = xp::native;
constexpr char kName[] = "A2FOCollectiveXP";
// Team::LoadAll/CleanAll iterate exactly nine slots (neutral plus eight players).
constexpr unsigned kTeams = 9;
const A2FO_ModuleApi* api = nullptr;
HMODULE armada = nullptr, fleetops = nullptr;
bool ready = false, seeded = false, syncing = false, promoting = false;
xp::Catalog catalog;
xp::Controller pool(catalog);
struct Config { bool enabled = false; double multiplier = 1; };
struct Definition { xp::ClassRank rank; void* native_class = nullptr; std::string original; };
struct Observed { xp::Unit unit; void* pointer = nullptr; };
std::map<void*, Config> factions;
std::map<std::string, Definition> definitions;
std::map<void*, std::string> class_names;
std::map<std::string, std::set<std::string>> parents;
std::set<std::string> catalogued, reports;
std::map<xp::UnitId, Observed> units;
std::map<void*, xp::UnitId> identities;
std::uint64_t next_identity = 1;
A2FO_InlineHook level_hook{}, save_hook{}, load_hook{}, clean_hook{};

void* at(HMODULE image, std::uintptr_t offset) {
    return reinterpret_cast<char*>(image) + offset;
}
bool accessible(const void* pointer, std::size_t length, bool write = false) {
    if (!pointer || !length) return false;
    MEMORY_BASIC_INFORMATION info{};
    if (!VirtualQuery(pointer, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const auto p = reinterpret_cast<std::uintptr_t>(pointer);
    const auto end = reinterpret_cast<std::uintptr_t>(info.BaseAddress) + info.RegionSize;
    if (p > end || length > end - p) return false;
    return !write || (info.Protect & (PAGE_READWRITE | PAGE_WRITECOPY |
                                    PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY));
}
template<class T> T read(const void* p, std::size_t offset, T fallback = {}) {
    if (!p) return fallback;
    T result{};
    const void* address = static_cast<const char*>(p) + offset;
    if (!accessible(address, sizeof(T))) return fallback;
    std::memcpy(&result, address, sizeof(T));
    return result;
}
template<class T> bool write(void* p, std::size_t offset, const T& value) {
    auto* address = static_cast<char*>(p) + offset;
    if (!p || !accessible(address, sizeof(T), true)) return false;
    std::memcpy(address, &value, sizeof(T)); return true;
}
void log(const std::string& message) {
    if (api && api->log) api->log(kName, ("[CollectiveXP] " + message).c_str());
}
void once(const std::string& message) {
    if (reports.insert(message).second) log(message);
}
struct XpContext {
    const A2FO_ModuleApi* supplied=nullptr;
    const A2FO_OdfFieldView* fields=nullptr;
    unsigned count=0;
    std::string* output=nullptr;
    std::exception_ptr error;
    std::map<void*,xp::UnitId>::iterator cursor;
    std::vector<std::uint8_t> bytes;
    std::string text;
};
void __cdecl class_loaded(const A2FO_GameObjectClassLoadedEvent*,void*);
void __cdecl race_loaded(const A2FO_RaceLoadedEvent*,void*);
void __cdecl craft_event(const A2FO_CraftEvent*,void*);
unsigned __attribute__((fastcall)) save_team(void*,void*,void*);
unsigned __attribute__((fastcall)) load_team(void*,void*,void*);
void __cdecl clean_teams();
template<std::size_t N> bool matches(HMODULE,unsigned,const std::array<std::uint8_t,N>&);
std::uint64_t xp_chain_step(void*,std::uint32_t,A2foRsXpReply*) noexcept;
A2foRsXpBytes xp_bytes(const std::string& value) noexcept {
    return {reinterpret_cast<const std::uint8_t*>(value.data()),value.size()};
}
std::string xp_text(A2foRsXpBytes value) {
    return std::string(reinterpret_cast<const char*>(value.data),value.size);
}
std::uint8_t xp_field_get(void* opaque,std::uint32_t index,A2foRsXpBytes* name,
                          A2foRsXpBytes* value) noexcept {
    auto& context=*static_cast<XpContext*>(opaque);
    if(!context.fields||index>=context.count)return 0;
    const auto& field=context.fields[index];
    *name={reinterpret_cast<const std::uint8_t*>(field.name.data),field.name.size};
    *value={reinterpret_cast<const std::uint8_t*>(field.value.data),field.value.size};
    return field.name.data!=nullptr;
}
std::uint8_t xp_parse(void* opaque,A2foRsXpBytes value,double* output,
                      std::uint32_t* consumed) noexcept {
    auto& context=*static_cast<XpContext*>(opaque);
    if(context.error)return 0;
    try {
        const auto text=xp_text(value);char* end=nullptr;
        *output=std::strtod(text.c_str(),&end);
        *consumed=static_cast<std::uint32_t>(end-text.c_str());
        return 1;
    } catch(...) {context.error=std::current_exception();return 0;}
}
void* find_class(const std::string& name) {
    auto known = definitions.find(name);
    if (known != definitions.end()) return known->second.native_class;
    using Find = void* (__cdecl*)(const char*);
    return reinterpret_cast<Find>(at(armada, 0xcd370))(name.c_str());
}
// Only walk class chains on discovery/admission, never the world each frame.
std::uint64_t xp_chain_step(void* opaque,std::uint32_t operation,A2foRsXpReply* reply) noexcept {
    auto& context=*static_cast<XpContext*>(opaque);
    if(operation==49){syncing=false;return 1;}
    if(context.error)return 0;
    try {
        const std::string name=xp_text(reply->first);
        switch(operation) {
        case 0:return reinterpret_cast<std::uintptr_t>(find_class(name));
        case 1: {
            auto found=definitions.find(name);
            if(found==definitions.end())return 0;
            reply->word=found->second.rank.level;
            reply->number=found->second.rank.next_xp;
            reply->second=xp_bytes(found->second.rank.next_class);
            reply->third=xp_bytes(found->second.original);
            return 1;
        }
        case 2:return catalogued.count(name);
        case 3: {
            xp::ClassRank rank{name,static_cast<std::uint32_t>(reply->word),
                reply->number,xp_text(reply->second)};
            if(!catalog.add(rank))return 0;
            catalogued.insert(name);return 1;
        }
        case 4:return static_cast<bool>(catalog.resolve(name,
            static_cast<std::uint32_t>(reply->word),xp_text(reply->second)));
        case 5: {
            auto found=parents.find(name);
            if(found==parents.end())return 0;
            if(found->second.empty())return 0;
            reply->second=xp_bytes(*found->second.begin());
            return found->second.size();
        }
        case 6: {
            const auto next=xp_text(reply->second);
            Definition definition{xp::ClassRank{name,
                static_cast<std::uint32_t>(reply->word),reply->number,next},
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->other)),
                xp_text(reply->third)};
            definitions[name]=std::move(definition);
            class_names[reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->other))]=name;
            if(!next.empty())parents[next].insert(name);
            return 1;
        }
        case 7:log("Invalid collectiveXP/collectiveXPModifier; faction remains stock");return 1;
        case 8:factions[reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->other))]=
            Config{reply->word!=0,reply->number};return 1;
        case 9:return write<unsigned char>(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->other)),0x634,1);
        case 10:factions[reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->other))].enabled=false;
            log("Cannot enable native canGainXP; faction remains stock");return 1;
        case 20: {
            const auto player=static_cast<unsigned>(reply->word);
            void* team=read<void*>(at(armada,0x338db0),player*4);
            void* race=read<void*>(team,0x244);
            auto found=factions.find(race);
            if(found==factions.end())return 0;
            reply->other=found->second.enabled;
            reply->number=found->second.multiplier;return 1;
        }
        case 21: {
            const auto* state=pool.state(static_cast<unsigned>(reply->word));
            if(!state)return 0;
            reply->other=state->enabled;reply->number=state->pool;
            reply->third_word=state->level;
            return 1;
        }
        case 22:return pool.configure(static_cast<unsigned>(reply->word),true,reply->number);
        case 23:log("Player "+std::to_string(reply->word)+" enabled");return 1;
        case 24:return factions.size();
        case 25: {
            auto found=factions.begin();
            std::advance(found,static_cast<std::ptrdiff_t>(reply->word));
            return found!=factions.end()&&found->second.enabled;
        }
        case 26: {
            const auto progress=pool.reconcile(static_cast<unsigned>(reply->word));
            reply->word=progress.levels_gained;
            reply->other=(static_cast<std::uint64_t>(progress.state.level)<<1)|progress.consumed;
            reply->number=progress.state.pool;
            reply->second_number=progress.threshold;return 1;
        }
        case 27:log("Player "+std::to_string(reply->word)+" Level up: "+
            std::to_string(reply->other)+" -> "+
            std::to_string(static_cast<unsigned>(reply->number)));return 1;
        case 28: {
            char line[160]{};
            std::snprintf(line,sizeof(line),"Player %u Level=%u XP=%.6g/%.6g",
                static_cast<unsigned>(reply->word),static_cast<unsigned>(reply->other),
                reply->number,reply->second_number);
            log(line);return 1;
        }
        case 30:return read<unsigned>(reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)),
            static_cast<std::size_t>(reply->other),static_cast<unsigned>(reply->third_word));
        case 31:return reinterpret_cast<std::uintptr_t>(read<void*>(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)),
            static_cast<std::size_t>(reply->other)));
        case 32:return read<unsigned char>(
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)),
            static_cast<std::size_t>(reply->other),static_cast<unsigned char>(reply->third_word));
        case 33: {
            const float value=read<float>(
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)),
                static_cast<std::size_t>(reply->other));
            std::uint32_t bits=0;std::memcpy(&bits,&value,sizeof(bits));return bits;
        }
        case 34: {
            const auto bits=static_cast<std::uint32_t>(reply->third_word);
            float value=0;std::memcpy(&value,&bits,sizeof(value));
            return write<float>(reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)),
                static_cast<std::size_t>(reply->other),value);
        }
        case 35: {
            auto found=identities.find(reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)));
            return found==identities.end()?0:found->second;
        }
        case 36: {
            auto found=units.find(reply->word);
            if(found==units.end())return 0;
            reply->word=found->second.unit.player;
            reply->first=xp_bytes(found->second.unit.current_class);
            reply->second=xp_bytes(found->second.unit.base_class);
            return 1;
        }
        case 37:pool.remove(reply->word);units.erase(reply->word);return 1;
        case 38:log("Player "+std::to_string(reply->word)+" Unit removed: "+
            xp_text(reply->first));return 1;
        case 39:identities.erase(reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)));
            return 1;
        case 40: {
            auto found=class_names.find(reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)));
            if(found==class_names.end())return 0;
            reply->first=xp_bytes(found->second);return 1;
        }
        case 41: {
            const auto id=reply->word;const auto player=static_cast<unsigned>(reply->other);
            const auto craft=reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->third_word));
            xp::Unit unit{id,player,xp_text(reply->first),xp_text(reply->second),true,true,true};
            if(pool.upsert(unit)!=xp::Status::ok)return 0;
            units.emplace(id,Observed{unit,craft});identities[craft]=id;return 1;
        }
        case 42:return next_identity++;
        case 44:log("Player "+std::to_string(reply->word)+" Unit added: "+xp_text(reply->first));
            return 1;
        case 45:syncing=true;return 1;
        case 46:context.cursor=identities.begin();return 1;
        case 47: {
            if(context.cursor==identities.end())return 0;
            void* object=context.cursor->first;++context.cursor;
            return reinterpret_cast<std::uintptr_t>(object);
        }
        case 48:seeded=reply->word!=0;return 1;
        case 50:return accessible(reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)),
            static_cast<std::size_t>(reply->other));
        case 52:once("Unknown or ambiguous native rank family: "+xp_text(reply->first)+
            "; contribution omitted");return 1;
        case 53:return syncing;
        case 54:return promoting;
        case 55:return seeded;
        case 56:return reinterpret_cast<std::uintptr_t>(read<void*>(at(armada,0x361084),0));
        case 62:return a2fo_collective_thiscall1(save_hook.gateway,
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)),
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->other)));
        case 64:return reinterpret_cast<std::uintptr_t>(read<void*>(at(armada,0x338db0),
            static_cast<std::size_t>(reply->word)*4));
        case 65:context.bytes=pool.snapshot_player(static_cast<unsigned>(reply->word));
            reply->first={context.bytes.data(),context.bytes.size()};return context.bytes.size();
        case 66:return reinterpret_cast<bool (__cdecl*)(void*,const void*,unsigned,const char*)>(
            at(armada,0x12c680))(reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)),
            reply->first.data,static_cast<unsigned>(reply->first.size),"a2fo_collectiveXP");
        case 67:return a2fo_collective_thiscall1(load_hook.gateway,
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)),
            reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->other)));
        case 68:context.bytes.assign(33,0);
            if(!reinterpret_cast<bool (__cdecl*)(void*,void*,unsigned)>(at(armada,0x12d7a0))(
                reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)),
                context.bytes.data(),33))return 0;
            reply->first={context.bytes.data(),context.bytes.size()};return 1;
        case 69: {
            std::vector<std::uint8_t> bytes(reply->first.data,
                reply->first.data+reply->first.size);
            return pool.restore_player(static_cast<unsigned>(reply->word),bytes);
        }
        case 70:log("Missing/invalid collective XP save record; load rejected, XP was not reset");return 1;
        case 71:log("Restored player "+std::to_string(reply->word)+" collective XP");return 1;
        case 72:pool.reset_player(static_cast<unsigned>(reply->word));return 1;
        case 73:units.clear();identities.clear();reports.clear();seeded=false;next_identity=1;return 1;
        case 74:reinterpret_cast<void(__cdecl*)()>(clean_hook.gateway)();return 1;
        case 75: {
            const auto progress=pool.award(static_cast<unsigned>(reply->word),reply->number);
            reply->word=progress.levels_gained;
            reply->other=(static_cast<std::uint64_t>(progress.state.level)<<1)|progress.consumed;
            reply->number=progress.state.pool;reply->second_number=progress.threshold;return 1;
        }
        case 76:once("Invalid/overflowing XP award rejected for collective owner");return 1;
        case 77: {
            const auto target=pool.target(reply->word);
            return target?target.level:0;
        }
        case 78:promoting=reply->word!=0;return 1;
        case 79:return a2fo_collective_delphi2(level_hook.gateway,reply->word,1);
        case 80:once("Native promotion did not replace an eligible craft; retrying at its next rank callback");return 1;
        case 81: {
            auto found=class_names.find(reinterpret_cast<void*>(static_cast<std::uintptr_t>(reply->word)));
            if(found==class_names.end())return 0;
            reply->first=xp_bytes(found->second);return 1;
        }
        case 82:return catalog.native_level(name);
        case 83: {
            const auto resolved=catalog.resolve(name,static_cast<unsigned>(reply->word),
                xp_text(reply->second));
            if(!resolved)return 0;
            context.text=resolved.class_id;reply->third=xp_bytes(context.text);return 1;
        }
        case 84: {
            void* trees=read<void*>(at(fleetops,0x212f08),0);
            return reinterpret_cast<std::uintptr_t>(trees);
        }
        case 85:return a2fo_collective_delphi2(at(fleetops,0x120680),reply->word,reply->other);
        case 86:once("Player "+std::to_string(reply->word)+" Yard resolved "+
            xp_text(reply->first)+" -> "+xp_text(reply->second));return 1;
        case 87:return pool.threshold(static_cast<unsigned>(reply->word))>0;
        case 88:reply->number=pool.threshold(static_cast<unsigned>(reply->word));return 1;
        case 90:api=context.supplied;
            armada=static_cast<HMODULE>(api->armada_module());
            fleetops=static_cast<HMODULE>(api->fleetops_module());
            return armada&&fleetops;
        case 91: {
            const char* race_fields[]{"collectiveXP","collectiveXPModifier"};
            const char* class_fields[]{"XPlevel","nextrankXP","nextrankClass","originalClass"};
            switch(reply->word){
            case 0:return api->register_race_loaded_handler(kName,race_fields,2,race_loaded,nullptr);
            case 1:return api->register_game_object_class_loaded_handler(kName,class_fields,4,class_loaded,nullptr);
            case 2:return api->register_craft_event_handler_masked(kName,
                A2FO_CRAFT_EVENT_MASK_SIMULATE_PRE|A2FO_CRAFT_EVENT_MASK_CLEANUP|
                A2FO_CRAFT_EVENT_MASK_POST_LOAD,craft_event,nullptr);
            default:return 0;}
        }
        case 92: {
            std::uint32_t crc=0;
            const auto identity=a2fo::supported_armada::identify(armada);
            const bool rotation=a2fo::supported_armada::uses_native_station_rotation(identity);
            return identity!=a2fo::supported_armada::Identity::unsupported&&
                ((a2fo::supported_armada::file_range_crc32(fleetops,0x400,0x1fd054,crc)&&
                  (crc==native::fleetops_text_crc||
                   (rotation&&(crc==native::fleetops_september30_text_crc||
                               crc==native::fleetops_october12_text_crc))))||
                 (rotation&&a2fo::supported_fleetops::approved_profile(fleetops)));
        }
        case 93:switch(reply->word){
            case 0:return matches(fleetops,native::award_rva,native::award_bytes);
            case 1:return matches(fleetops,native::sleeper_award_rva,native::special_award_bytes);
            case 2:return matches(fleetops,native::saboteur_award_rva,native::special_award_bytes);
            case 3:return matches(fleetops,native::level_rva,native::level_bytes);
            case 4:return matches(fleetops,native::team_save_rva,native::save_bytes);
            case 5:return matches(fleetops,native::team_load_rva,native::load_bytes);
            case 6:return matches(armada,native::clean_teams_rva,native::clean_bytes);
            default:return 0;}
        case 94:a2fo_collective_award_continue=at(fleetops,native::award_rva+native::award_bytes.size());
            a2fo_collective_sleeper_continue=at(fleetops,native::sleeper_award_rva+native::special_award_bytes.size());
            a2fo_collective_saboteur_continue=at(fleetops,native::saboteur_award_rva+native::special_award_bytes.size());
            return 1;
        case 95: {
            const auto hook=[&](HMODULE image,unsigned rva,void* callback,
                                  const auto& bytes,A2FO_InlineHook& out){
                return api->install_inline_hook(at(image,rva),callback,bytes.size(),bytes.data(),&out)&&out.gateway;
            };
            switch(reply->word){
            case 0:return hook(fleetops,native::team_save_rva,reinterpret_cast<void*>(&save_team),native::save_bytes,save_hook);
            case 1:return hook(fleetops,native::team_load_rva,reinterpret_cast<void*>(&load_team),native::load_bytes,load_hook);
            case 2:return hook(armada,native::clean_teams_rva,reinterpret_cast<void*>(&clean_teams),native::clean_bytes,clean_hook);
            case 3: {
                const bool result=hook(fleetops,native::level_rva,
                    reinterpret_cast<void*>(&a2fo_collective_level_bridge),native::level_bytes,level_hook);
                if(result)a2fo_collective_level_gateway=level_hook.gateway;
                return result;
            }
            default:return 0;}
        }
        case 96:switch(reply->word){
            case 0:return api->patch_jump(at(fleetops,native::award_rva),
                reinterpret_cast<void*>(&a2fo_collective_award_bridge),
                native::award_bytes.data(),native::award_bytes.size());
            case 1:return api->patch_jump(at(fleetops,native::sleeper_award_rva),
                reinterpret_cast<void*>(&a2fo_collective_sleeper_bridge),
                native::special_award_bytes.data(),native::special_award_bytes.size());
            case 2:return api->patch_jump(at(fleetops,native::saboteur_award_rva),
                reinterpret_cast<void*>(&a2fo_collective_saboteur_bridge),
                native::special_award_bytes.data(),native::special_award_bytes.size());
            default:return 0;}
        case 97:ready=reply->word!=0;
            log(ready?"Native award, rank, Team save/load and match-reset hooks installed; factions opt in individually"
                     :"Unsupported native signatures; module remains stock/pass-through");
            return 1;
        case 98:ready=false;return 1;
        default:return 0;
        }
    } catch(...) {context.error=std::current_exception();return 0;}
}
void recalculate() {
    XpContext context{};
    a2fo_rs_runtime_xp_recalculate(syncing,xp_chain_step,&context);
    if(context.error){syncing=false;std::rethrow_exception(context.error);}
}
void __cdecl class_loaded(const A2FO_GameObjectClassLoadedEvent* event, void*) {
    const bool valid=event&&event->struct_size>=sizeof(*event)&&event->object_class;
    if(!valid)return;
    try {
        XpContext context{};context.fields=event->odf_fields;context.count=event->odf_field_count;
        const A2foRsXpBytes source{
            reinterpret_cast<const std::uint8_t*>(event->source_odf.data),event->source_odf.size};
        a2fo_rs_runtime_xp_class(1,reinterpret_cast<std::uintptr_t>(event->object_class),
            source,event->odf_field_count,xp_field_get,xp_parse,xp_chain_step,&context);
        if(context.error)std::rethrow_exception(context.error);
    } catch (...) { log("Class rank metadata allocation failed"); }
}
void __cdecl race_loaded(const A2FO_RaceLoadedEvent* event, void*) {
    const bool valid=event&&event->struct_size>=sizeof(*event)&&event->race;
    if(!valid)return;
    try {
        XpContext context{};context.fields=event->odf_fields;context.count=event->odf_field_count;
        a2fo_rs_runtime_xp_race(1,reinterpret_cast<std::uintptr_t>(event->race),
            ready,event->odf_field_count,xp_field_get,xp_parse,xp_chain_step,&context);
        if(context.error)std::rethrow_exception(context.error);
    } catch (...) { log("Faction configuration allocation failed"); }
}
void __cdecl craft_event(const A2FO_CraftEvent* event, void*) {
    const bool valid=ready&&event&&event->struct_size>=sizeof(*event)&&event->craft;
    if(!valid)return;
    try {
        XpContext context{};
        a2fo_rs_runtime_xp_craft_event(1,event->kind,
            reinterpret_cast<std::uintptr_t>(event->craft),seeded,xp_chain_step,&context);
        if(context.error){syncing=false;std::rethrow_exception(context.error);}
    } catch (...) { once("Craft observation failed; see native integration limitations"); }
}
unsigned __attribute__((fastcall)) save_team(void* team,void*,void* writer) {
    try {
        XpContext context{};
        const auto result=a2fo_rs_runtime_xp_save(ready,
            reinterpret_cast<std::uintptr_t>(team),reinterpret_cast<std::uintptr_t>(writer),
            xp_chain_step,&context);
        if(context.error)std::rethrow_exception(context.error);
        return result;
    } catch (...) { log("Could not save collective XP; save rejected"); return 0; }
}
unsigned __attribute__((fastcall)) load_team(void* team,void*,void* reader) {
    try {
        XpContext context{};
        const auto result=a2fo_rs_runtime_xp_load(ready,
            reinterpret_cast<std::uintptr_t>(team),reinterpret_cast<std::uintptr_t>(reader),
            xp_chain_step,&context);
        if(context.error)std::rethrow_exception(context.error);
        return result;
    } catch (...) { log("Could not restore collective XP; load rejected"); return 0; }
}
void __cdecl clean_teams() {
    // Native match lifetime boundary, including a load in the same process.
    XpContext context{};
    a2fo_rs_runtime_xp_clean(xp_chain_step,&context);
    if(context.error)std::rethrow_exception(context.error);
}
template<std::size_t N> bool matches(HMODULE image,unsigned rva,const std::array<std::uint8_t,N>& expected) {
    return accessible(at(image,rva),N) && !std::memcmp(at(image,rva),expected.data(),N);
}
} // namespace

extern "C" int __cdecl a2fo_collective_award(void* killer,void* victim_enhancement) noexcept {
    try {
        XpContext context{};
        const auto result=a2fo_rs_runtime_xp_award(ready,0,
            reinterpret_cast<std::uintptr_t>(killer),
            reinterpret_cast<std::uintptr_t>(victim_enhancement),xp_chain_step,&context);
        if(context.error){syncing=false;std::rethrow_exception(context.error);}
        return result;
    } catch (...) { log("Collective XP award failed"); return 1; }
}
extern "C" int __cdecl a2fo_collective_class_award(void* recipient,void* victim_class) noexcept {
    void* enhancement[]{nullptr,victim_class};
    return a2fo_collective_award(recipient,enhancement);
}
extern "C" int __cdecl a2fo_collective_level(void* craft) noexcept {
    try {
        XpContext context{};
        const auto result=a2fo_rs_runtime_xp_level(ready,
            reinterpret_cast<std::uintptr_t>(craft),xp_chain_step,&context);
        if(context.error){syncing=false;std::rethrow_exception(context.error);}
        return result;
    } catch (...) { promoting=false; log("Native collective promotion failed"); return 1; }
}
extern "C" __declspec(dllexport) void* __cdecl A2FOCollectiveXP_ResolveBuildClass(void* producer,void* base_class) noexcept {
    try {
        XpContext context{};
        const auto result=a2fo_rs_runtime_xp_resolve(ready,
            reinterpret_cast<std::uintptr_t>(producer),reinterpret_cast<std::uintptr_t>(base_class),
            xp_chain_step,&context);
        if(context.error)std::rethrow_exception(context.error);
        return reinterpret_cast<void*>(result);
    } catch (...) { log("Yard resolution failed; admission rejected"); return nullptr; }
}
extern "C" __declspec(dllexport) bool __cdecl A2FOCollectiveXP_GetProgress(unsigned player,unsigned* level,double* current,double* required) noexcept {
    try {
    XpContext context{};A2foRsXpPublicProgress progress{};
    if(!a2fo_rs_runtime_xp_get_progress(ready,player,&progress,xp_chain_step,&context))return false;
    if(context.error)std::rethrow_exception(context.error);
    if(level)*level=progress.level;
    if(current)*current=progress.current;
    if(required)*required=progress.required;
    return true;
    } catch (...) { return false; }
}
extern "C" __declspec(dllexport) bool __cdecl A2FOCollectiveXP_IsEnabledForObject(void* object) noexcept {
    XpContext context{};
    return a2fo_rs_runtime_xp_is_enabled(ready,reinterpret_cast<std::uintptr_t>(object),
        xp_chain_step,&context)!=0;
}
extern "C" __declspec(dllexport) void __cdecl A2FOCollectiveXP_Recalculate() noexcept {
    if (ready) try { recalculate(); } catch (...) { log("Fleet recalculation failed"); }
}
extern "C" __declspec(dllexport) bool __cdecl A2FO_ModuleInit(const A2FO_ModuleApi* supplied) {
    const bool valid=supplied && supplied->api_version==A2FO_MODULE_API_VERSION &&
        A2FO_MODULE_API_HAS(supplied,register_craft_event_handler_masked) &&
        supplied->register_craft_event_handler_masked && supplied->register_race_loaded_handler &&
        supplied->register_game_object_class_loaded_handler && supplied->install_inline_hook &&
        supplied->patch_jump;
    XpContext context{};context.supplied=supplied;
    const bool result=a2fo_rs_runtime_xp_init(valid,xp_chain_step,&context)!=0;
    if(context.error)std::rethrow_exception(context.error);
    return result;
}
extern "C" __declspec(dllexport) void __cdecl A2FO_ModuleShutdown() {
    XpContext context{};a2fo_rs_runtime_xp_shutdown(xp_chain_step,&context);
}

