#include "scene_capture.hpp"
#include "engine_camera.hpp"
#include <cmath>
#include <cstring>
#include <limits>

namespace a2fo::native11 {
bool Collector::configure_plain_database_vtable(std::uint32_t vtable) noexcept {
    if (active_) return false;
    plain_database_vtable_ = vtable;
    return true;
}
bool Collector::configure_craft_instance_vtable(std::uint32_t vtable) noexcept {
    if(active_)return false;
    craft_instance_vtable_=vtable;return true;
}
bool Collector::configure_model_serial_reader(void* context,ReadModelSerial reader) noexcept {
    if(active_)return false;
    serial_context_=context;serial_reader_=reader;return true;
}
bool Collector::capture_runtime_scale(Object& item) noexcept {
    if(!craft_instance_vtable_)return true;
    item.placement_identity=PlacementIdentity::unresolved;
    if(!read_at(item.instance,0,&item.instance_vtable,sizeof(item.instance_vtable)))return true;
    if(item.instance_vtable!=craft_instance_vtable_||item.model_identity!=ModelIdentity::plain_database_name){
        item.placement_identity=PlacementIdentity::unsupported_instance;return true;
    }
    std::uint8_t active=0;
    if(!read_at(item.instance,0x74,&active,sizeof(active)))return false;
    if(active&&!read_at(item.instance,0x28,item.runtime_scale,sizeof(item.runtime_scale)))return false;
    for(float scale:item.runtime_scale)if(!std::isfinite(scale)||scale==0)return false;
    // Plain DB draw scales each row of the engine's row-vector placement;
    // after transposition this scales columns, leaving translation untouched.
    for(int row=0;row<3;++row)for(int col=0;col<3;++col){
        const double value=double(item.world[row*4+col])*item.runtime_scale[col];
        if(!std::isfinite(value)||std::fabs(value)>std::numeric_limits<float>::max())return false;
        item.world[row*4+col]=static_cast<float>(value);
    }
    item.placement_identity=PlacementIdentity::craft_runtime_scale;return true;
}

bool Collector::read_at(std::uint32_t base, std::uint32_t offset,
                        void* output, std::size_t length) const noexcept {
    if (!read_ || !base || !length ||
        length - 1 > std::numeric_limits<std::uint32_t>::max() - offset ||
        base > std::numeric_limits<std::uint32_t>::max() - offset - (length - 1))
        return false;
    return read_(context_, base + offset, output, length);
}

void Collector::capture_model_identity(Object& item) noexcept {
    if (!plain_database_vtable_) return;
    if (!read_at(item.instance, 0x80, &item.database, sizeof(item.database)) ||
        !item.database ||
        !read_at(item.database, 0, &item.database_vtable, sizeof(item.database_vtable)))
        return;
    if (item.database_vtable != plain_database_vtable_) {
        item.model_identity = ModelIdentity::unsupported_database;
        return;
    }
    std::uint32_t name_address = 0;
    if (!read_at(item.database, 0x38, &name_address, sizeof(name_address)) ||
        !name_address) return;
    for (std::size_t i=0; i<sizeof(item.model_name); ++i) {
        char value = 0;
        if (!read_at(name_address, static_cast<std::uint32_t>(i), &value, 1) ||
            (value != 0 && (static_cast<unsigned char>(value) < 0x20 ||
                            static_cast<unsigned char>(value) > 0x7e))) {
            std::memset(item.model_name, 0, sizeof(item.model_name));
            return;
        }
        if (value == 0) {
            if (i == 0) return;
            item.model_identity = ModelIdentity::plain_database_name;
            if(serial_reader_)item.model_serial=serial_reader_(serial_context_,item.database,item.model_name);
            return;
        }
        if (i == sizeof(item.model_name)-1) {
            std::memset(item.model_name, 0, sizeof(item.model_name));
            return;
        }
        item.model_name[i] = value;
    }
}

bool Collector::begin(std::uint32_t camera, bool main) noexcept {
    if (active_ || !main || !camera) return false;
    for (Object& object : pending_.objects) object = Object{};
    for (float& value : pending_.engine_screen) value = 0.0f;
    pending_.view = EngineCameraView{};
    pending_.camera = camera;
    pending_.generation = 0;
    pending_.object_count = 0;
    invalid_ = false;
    active_ = true;
    return true;
}

void Collector::observe_post_frustum(std::uint32_t entity, std::uint32_t camera) noexcept {
    if (!active_ || invalid_ || camera != pending_.camera) return;
    if (!entity || pending_.object_count == capacity) {
        invalid_ = true;
        return;
    }
    std::uint32_t instance = 0;
    float matrix[12]{};
    if (!read_at(entity, 4, &instance, sizeof(instance)) || !instance ||
        !read_at(instance, 0x44, matrix, sizeof(matrix))) {
        invalid_ = true;
        return;
    }
    for (float value : matrix) if (!std::isfinite(value)) {
        invalid_ = true;
        return;
    }
    // Matrix34 stores row-vector basis rows and a translation row.
    const double determinant =
        double(matrix[0]) * (double(matrix[4])*matrix[8] - double(matrix[5])*matrix[7]) -
        double(matrix[1]) * (double(matrix[3])*matrix[8] - double(matrix[5])*matrix[6]) +
        double(matrix[2]) * (double(matrix[3])*matrix[7] - double(matrix[4])*matrix[6]);
    if (!std::isfinite(determinant) || determinant == 0.0) {
        invalid_ = true;
        return;
    }
    Object& item = pending_.objects[pending_.object_count];
    item.entity = entity;
    item.instance = instance;
    for (int row = 0; row < 3; ++row)
        for (int col = 0; col < 3; ++col)
            item.world[row*4+col] = matrix[col*3+row];
    for (int row = 0; row < 3; ++row) item.world[row*4+3] = matrix[9+row];
    item.world[12] = item.world[13] = item.world[14] = 0.0f;
    item.world[15] = 1.0f;
    capture_model_identity(item);
    if(!capture_runtime_scale(item)){invalid_=true;return;}
    ++pending_.object_count;
}

bool Collector::material_target(std::uint32_t instance,std::uint32_t camera,Object& output) const noexcept {
    if(!active_||invalid_||!instance||camera!=pending_.camera)return false;
    const Object* found=nullptr;
    for(std::size_t i=0;i<pending_.object_count;++i)if(pending_.objects[i].instance==instance){
        if(found)return false;
        found=&pending_.objects[i];
    }
    if(!found)return false;
    output=*found;return true;
}
bool Collector::end(std::uint32_t camera, Frame& output) noexcept {
    if (!active_ || camera != pending_.camera) return false;
    active_ = false;
    if (invalid_ || !read_at(camera, 0x160, pending_.engine_screen,
                             sizeof(pending_.engine_screen)) ||
        !read_at(camera, 0x1b0, pending_.view.viewport, sizeof(pending_.view.viewport)))
        return false;
    if (!convert_engine_cpu_camera(pending_.engine_screen, pending_.view.viewport, pending_.view))
        return false;
    pending_.generation = ++generation_;
    output = pending_;
    return true;
}

bool Collector::camera_view(std::uint32_t camera,EngineCameraView& output) const noexcept {
    if(!active_||invalid_||camera!=pending_.camera)return false;
    float screen[16]{},viewport[4]{};EngineCameraView next{};
    if(!read_at(camera,0x160,screen,sizeof(screen))||
       !read_at(camera,0x1b0,viewport,sizeof(viewport))||
       !convert_engine_cpu_camera(screen,viewport,next))return false;
    output=next;return true;
}

void Collector::abort() noexcept {
    active_ = false;
    invalid_ = false;
    pending_.object_count = 0;
}
} // namespace a2fo::native11
