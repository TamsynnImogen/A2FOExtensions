#include "../renderer/refit/resources.hpp"
#include "../renderer/native11/native_arithmetic.hpp"
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
using a2fo::refit::ModelUpload2;
using a2fo::refit::Resources2;
using a2fo::native11::ModelAsset;
using a2fo::native11::TextureAsset;
struct Host {
    unsigned model_calls=0,texture_calls=0;
    bool refuse_model=false,refuse_texture=false;
    std::array<float,9> transformed{};
    std::string texture_name;
    std::vector<std::uint8_t> encoded;
} host;
extern "C" std::int32_t model_upload(const A2FORefitModel2* input,std::uint32_t* nodes) {
    ++host.model_calls;
    assert(input && input->base.size==sizeof(*input) && input->base.id>0 && input->base.revision==1);
    assert(input->base.bytes && input->base.byte_count>0 && input->transform_positions);
    if(host.refuse_model)return A2FO_REFIT_DEVICE;
    auto xyz=std::array<float,9>{1.f,2.f,-3.f,.5f,-.25f,4.f,0.f,1.f,2.f};
    assert(input->transform_positions(input->context,xyz.data(),3)==1);
    host.transformed=xyz;*nodes=1;return A2FO_REFIT_OK;
}
extern "C" std::int32_t texture_upload(const A2FORefitEncodedTexture2* input) {
    ++host.texture_calls;
    assert(input && input->size==sizeof(*input) && input->revision==1 && input->flags==0);
    assert(input->name && input->bytes && input->byte_count>0);
    if(host.refuse_texture)return A2FO_REFIT_DEVICE;
    host.texture_name.assign(input->name,input->name_length);
    host.encoded.assign(input->bytes,input->bytes+input->byte_count);
    return A2FO_REFIT_OK;
}
std::uint32_t bits(float f) { std::uint32_t b=0;std::memcpy(&b,&f,4);return b; }
std::shared_ptr<const ModelAsset> model(std::uint64_t serial,float scale=2.f,std::size_t bytes=16) {
    auto p=std::make_shared<ModelAsset>();p->info.serial=serial;p->info.database=7;
    p->info.parser_scalar_bits=bits(scale);
    p->info.parser_native_x87_control=a2fo::native11::encode_native_x87(0x037f);
    p->bytes.assign(bytes,0x5a);p->info.byte_count=static_cast<std::uint32_t>(p->bytes.size());
    return p;
}
std::shared_ptr<const TextureAsset> texture(std::uint64_t serial,std::size_t bytes=12,bool colour_key=false) {
    auto p=std::make_shared<TextureAsset>();p->serial=serial;p->colour_key=colour_key;
    p->bytes=std::make_shared<const std::vector<std::uint8_t>>(bytes,0x7b);
    return p;
}
void need(bool yes,const char* message) {
    if(!yes){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}
}
}

int main() {
    A2FORefitApi2 api{};api.size=sizeof(api);api.abi_version=A2FO_REFIT_ABI2;
    api.upload_model_transformed=model_upload;api.upload_encoded_texture=texture_upload;
    Resources2 resources(&api);std::string error="old";
    ModelUpload2 out{91,92,93};
    auto source=model(100);
    std::weak_ptr<const ModelAsset> held=source;
    need(resources.model(source,out,error) && error.empty(),"upload model through ABI2");
    need(out.id==1 && out.revision==1 && out.node_count==1 && host.model_calls==1,"admitted model identity");
    need(host.transformed==std::array<float,9>{2.f,4.f,-6.f,1.f,-.5f,8.f,0.f,2.f,4.f},
         "captured loader scale applied exactly to mutable XYZ copy");
    const auto original=source->bytes;
    source.reset();need(!held.expired(),"cache owns admitted model snapshot");
    need(held.lock()->bytes==original,"upload does not mutate source bytes");
    need(resources.model(held.lock(),out,error) && host.model_calls==1,"same owner cached without reupload");
    auto collision=model(100);
    ModelUpload2 untouched{71,72,73};
    need(!resources.model(collision,untouched,error) && untouched.id==71 && host.model_calls==1,
         "same serial with different model owner rejected");
    collision.reset();
    auto bad_policy=model(101,0.f);
    need(!resources.model(bad_policy,untouched,error) && host.model_calls==1,"unsupported model policy rejected before host");
    host.refuse_model=true;
    auto retry_model=model(102);std::weak_ptr<const ModelAsset> rejected=retry_model;
    need(!resources.model(retry_model,untouched,error) && untouched.id==71,"host model refusal leaves output untouched");
    retry_model.reset();need(rejected.expired(),"rejected model owner not retained");
    host.refuse_model=false;
    need(resources.model(model(102),out,error) && out.id==2 && host.model_calls==3,"model refusal can retry same id");

    std::string name="unchanged";std::uint32_t revision=99;
    auto tex=texture(33);std::weak_ptr<const TextureAsset> held_tex=tex;
    need(Resources2::resolve_texture(&resources,tex,name,revision,error),"upload texture via packet resolver");
    need(name=="tex_33" && revision==1 && host.texture_name==name &&
         host.encoded==*tex->bytes && host.texture_calls==1,"stable encoded texture identity and owned source bytes");
    tex.reset();need(!held_tex.expired(),"cache owns admitted texture snapshot");
    need(resources.texture(held_tex.lock(),name,revision,error) && host.texture_calls==1,"same texture owner cached");
    auto tex_collision=texture(33);
    need(!resources.texture(tex_collision,name,revision,error) && host.texture_calls==1,
         "same serial different texture owner rejected");
    need(!resources.texture(texture(34,12,true),name,revision,error) && host.texture_calls==1,
         "colour key rejected before host");
    auto alpha=std::make_shared<TextureAsset>(*texture(36));
    alpha->flags=a2fo::native11::texture_has_alpha;
    need(resources.texture(alpha,name,revision,error) && name=="tex_36" && revision==1 &&
         host.texture_calls==2 && host.encoded==*alpha->bytes,
         "native alpha flag retains encoded bytes and uploads descriptor flags zero");
    auto unknown=std::make_shared<TextureAsset>(*texture(37));unknown->flags=4;
    const auto before_unknown=host.texture_calls;
    const auto accepted_name=name;const auto accepted_revision=revision;
    need(!resources.texture(unknown,name,revision,error) && !error.empty() &&
         name==accepted_name && revision==accepted_revision && host.texture_calls==before_unknown,
         "unknown native texture flag rejected transactionally before host");
    need(!Resources2::resolve_texture(nullptr,held_tex.lock(),name,revision,error),"null resolver context rejected");
    host.refuse_texture=true;
    auto retry_tex=texture(35);std::weak_ptr<const TextureAsset> rejected_tex=retry_tex;
    name="unchanged";revision=99;
    need(!resources.texture(retry_tex,name,revision,error) && name=="unchanged" && revision==99,
         "host texture refusal leaves output untouched");
    retry_tex.reset();need(rejected_tex.expired(),"rejected texture owner not retained");
    host.refuse_texture=false;
    need(resources.texture(texture(35),name,revision,error) && name=="tex_35" && host.texture_calls==4,
         "texture refusal can retry");
    bool wrong_model=true,wrong_texture=true,wrong_reset=true;
    std::thread foreign([&] {
        ModelUpload2 x{};std::string e,n;std::uint32_t rev=0;
        wrong_model=resources.model(model(200),x,e);
        wrong_texture=resources.texture(texture(200),n,rev,e);
        wrong_reset=resources.reset_local(&api,2,e);
    });foreign.join();
    need(!wrong_model && !wrong_texture && !wrong_reset,"owner thread enforced on all entry points");

    // Explicit caller-controlled epoch transition: host reset happens elsewhere.
    need(resources.reset_local(&api,2,error) && error.empty(),"local cache reset after host epoch change");
    need(held.expired() && held_tex.expired(),"reset releases admitted owners");
    need(resources.model(model(100),out,error) && out.id==1,"model id restarts only after reset");
    need(resources.texture(texture(33),name,revision,error) && name=="tex_33","texture reuploads after reset");
    need(!resources.reset_local(&api,2,error),"same epoch reset rejected");

    // Budget and record count are bounded before any host call.
    need(!resources.model(model(500,2.f,4u*1024u*1024u+1),untouched,error),"single model host size limit");
    const unsigned calls_before=host.model_calls;
    for(std::uint64_t serial=1000;serial<1255;++serial)
        need(resources.model(model(serial),out,error),"fill model records");
    need(!resources.model(model(1255),untouched,error) && host.model_calls==calls_before+255,
         "256-model record budget rejects without host call");
    need(resources.reset_local(&api,3,error),"reset full model cache");
    for(std::uint64_t serial=3000;serial<3016;++serial)
        need(resources.model(model(serial,2.f,4u*1024u*1024u),out,error),"fill 64 MiB raw model budget");
    const unsigned raw_calls=host.model_calls;
    need(!resources.model(model(3016),untouched,error) && host.model_calls==raw_calls,
         "raw model byte budget rejects without host call");
    need(resources.reset_local(&api,4,error),"reset raw model cache");
    const auto large=texture(900,64u*1024u*1024u);
    need(resources.texture(large,name,revision,error),"fill encoded texture byte budget");
    const unsigned texture_before=host.texture_calls;
    need(!resources.texture(texture(901),name,revision,error) && host.texture_calls==texture_before,
         "encoded texture byte budget rejects without host call");
    need(resources.reset_local(&api,5,error),"reset full texture cache");
    for(std::uint64_t serial=2000;serial<2256;++serial)
        need(resources.texture(texture(serial),name,revision,error),"fill texture records");
    need(!resources.texture(texture(2256),name,revision,error),"256-texture record budget rejects");
    std::cout<<"PASS: Refit Resources2 owner cache, captured transform, retry, collisions, budgets, reset\n";
}
