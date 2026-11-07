#include "craft_view_capture.hpp"
#include "native_arithmetic.hpp"
#include <array>
#include <cstring>
#include <limits>

namespace a2fo::native11 {
namespace {
enum Field : std::size_t {entity_instance,instance_vtable,instance_database,database_vtable,
    engine,engine_camera,fade_enabled,game,settings,detailed_range,reveal_all,
    mode,skip_near,max_distance,camera_x,camera_y,camera_z,position_x,position_y,
    position_z,radius,instance_cull,near_min,near_max,near_amount,count};
using Words=std::array<std::uint32_t,count>;
bool at(void* context,Read read,std::uint32_t base,std::uint32_t offset,
    void* output,std::size_t size) {
    constexpr auto maximum=std::numeric_limits<std::uint32_t>::max();
    if(!base||!size||size>maximum||offset>maximum-(size-1)||
       base>maximum-offset-(size-1))return false;
    return read(context,base+offset,output,size);
}
bool word(void* context,Read read,std::uint32_t base,std::uint32_t offset,
    std::uint32_t& output) {return at(context,read,base,offset,&output,4);}
bool byte(void* context,Read read,std::uint32_t base,std::uint32_t offset,
    std::uint32_t& output) {
    std::uint8_t value=0;
    if(!at(context,read,base,offset,&value,1))return false;
    output=value;return true;
}
bool snapshot(const CraftViewSubject& object,std::uint32_t camera,CraftViewReadProfile profile,
    void* context,Read read,Words& out) {
    if(!word(context,read,object.entity,4,out[entity_instance])||
       !word(context,read,object.instance,0,out[instance_vtable])||
       !word(context,read,object.instance,0x80,out[instance_database])||
       !word(context,read,object.database,0,out[database_vtable])||
       !word(context,read,profile.engine_pointer,0,out[engine])||!out[engine]||
       !word(context,read,out[engine],0xfc,out[engine_camera])||
       !word(context,read,out[engine],0xe8,out[fade_enabled])||
       !word(context,read,profile.game_pointer,0,out[game])||!out[game]||
       !word(context,read,out[game],0x80,out[settings])||!out[settings]||
       !word(context,read,out[settings],0x40,out[detailed_range])||
       !byte(context,read,out[game],0x84,out[reveal_all])||
       !word(context,read,profile.mode_pointer,0,out[mode])||
       !byte(context,read,profile.skip_near_pointer,0,out[skip_near])||
       !word(context,read,profile.max_distance_pointer,0,out[max_distance]))return false;
    for(unsigned i=0;i<3;++i)
        if(!word(context,read,camera,0xe4+4*i,out[camera_x+i])||
           !word(context,read,object.entity,0xac+4*i,out[position_x+i]))return false;
    return word(context,read,object.instance,0x40,out[radius])&&
        byte(context,read,object.instance,0x84,out[instance_cull])&&
        word(context,read,profile.near_fade_pointer,0,out[near_min])&&
        word(context,read,profile.near_fade_pointer,4,out[near_max])&&
        word(context,read,profile.near_fade_pointer,8,out[near_amount]);
}
float as_float(std::uint32_t bits) noexcept {
    float out=0;std::memcpy(&out,&bits,4);return out;
}
bool fail(std::string& error,const char* why) noexcept {
    try{error=why;}catch(...){error.clear();}return false;
}
}
bool capture_craft_view(const Object& object,std::uint32_t camera,
    std::uint32_t native_x87_control,CraftViewReadProfile profile,
    void* read_context,Read read,CapturedCraftView& output,std::string& error) noexcept {
    if(object.model_identity!=ModelIdentity::plain_database_name||
       object.placement_identity!=PlacementIdentity::craft_runtime_scale)
        return fail(error,"invalid craft view capture target or profile");
    return capture_craft_view(CraftViewSubject{object.entity,object.instance,object.database,
        object.instance_vtable,object.database_vtable,object.model_serial},camera,
        native_x87_control,profile,read_context,read,output,error);
}
bool capture_craft_view(const CraftViewSubject& object,std::uint32_t camera,
    std::uint32_t native_x87_control,CraftViewReadProfile profile,
    void* read_context,Read read,CapturedCraftView& output,std::string& error) noexcept {
    try {
        if(!read||!camera||!object.entity||!object.instance||!object.database||
           !object.model_serial||
           !profile.engine_pointer||!profile.game_pointer||!profile.mode_pointer||
           !profile.skip_near_pointer||!profile.max_distance_pointer||!profile.near_fade_pointer||
           !profile.instance_vtable||!profile.database_vtable||
           object.instance_vtable!=profile.instance_vtable||
           object.database_vtable!=profile.database_vtable||
           !native_x87_control||!valid_native_x87_encoding(native_x87_control))
            return fail(error,"invalid craft view capture target or profile");
        Words first{},second{};
        if(!snapshot(object,camera,profile,read_context,read,first)||
           first[entity_instance]!=object.instance||
           first[instance_vtable]!=profile.instance_vtable||
           first[instance_database]!=object.database||
           first[database_vtable]!=profile.database_vtable||
           first[engine_camera]!=camera)
            return fail(error,"craft view owner or camera mismatch");
        if(!snapshot(object,camera,profile,read_context,read,second)||first!=second)
            return fail(error,"craft view fields changed during capture");
        CapturedCraftView candidate{};
        candidate.engine=first[engine];candidate.game=first[game];candidate.settings=first[settings];
        candidate.camera=camera;candidate.entity=object.entity;
        candidate.instance=object.instance;candidate.database=object.database;
        candidate.native_x87_control=native_x87_control;
        auto& input=candidate.input;
        for(unsigned i=0;i<3;++i){
            input.camera[i]=as_float(first[camera_x+i]);
            input.position[i]=as_float(first[position_x+i]);
        }
        input.radius=as_float(first[radius]);
        input.max_distance=as_float(first[max_distance]);input.mode=first[mode];
        input.detailed_range=first[detailed_range]!=0;
        input.reveal_all=first[reveal_all]!=0;
        input.instance_cull_flag=first[instance_cull]!=0;
        input.skip_near_fade=first[skip_near]!=0;
        input.fade_enabled=first[fade_enabled]!=0;
        input.near_fade_min=as_float(first[near_min]);
        input.near_fade_max=as_float(first[near_max]);
        input.near_fade_amount=as_float(first[near_amount]);
        if(!native_craft_view(input,native_x87_control,candidate.result))
            return fail(error,"invalid fresh craft view inputs");
        output=candidate;error.clear();return true;
    }catch(...){return fail(error,"craft view capture exception");}
}
} // namespace a2fo::native11
