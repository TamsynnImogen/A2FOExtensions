// Synthetic PE and gateway fixture; no bytes in the image are executed.
#include "../core/builtin_animations.cpp"
#include "../core/builtin_modules.hpp"
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {
using namespace a2fo;
std::uint8_t* image = nullptr;
const std::string roots = "C:\\a2fo-animations-native-fixture";
unsigned installs = 0, reads = 0, destroys = 0;
bool fail_second = false;
std::string failure_mode;
std::string last_log;
unsigned class_registrations = 0, producer_registrations = 0;
void* expected_channel = nullptr;
void* expected_stream = nullptr;
A2FO_GameObjectClassLoadedHandler class_callback = nullptr;
A2FO_CraftEventHandler craft_callback = nullptr;
A2FO_ProducerEventHandler producer_callback = nullptr;
A2FO_WeaponClassLoadedHandler weapon_callback = nullptr;
A2FO_WeaponTriggerHandler trigger_callback = nullptr;
void require(bool okay, const char* message) { if (!okay) throw std::runtime_error(message); }
void A2FO_CALL log_message(const char*, const char* message) {
    last_log = message ? message : "";
}
void* A2FO_CALL armada_module() { return image; }
void __fastcall native_read(void* channel, void*, void* stream) {
    ++reads;
    require(channel == expected_channel && stream == expected_stream,
            "animation read forwards this and stream");
}
void __fastcall native_destroy(void* channel, void*) {
    ++destroys;
    require(channel == expected_channel, "animation destroy forwards this");
}
bool A2FO_CALL install_hook(void* target, void* replacement, std::size_t length,
                            const std::uint8_t* signature, A2FO_InlineHook* hook) {
    const auto specs = hook_specs();
    require(installs < specs.size() && target == image + specs[installs].rva &&
            replacement == specs[installs].function &&
            length == specs[installs].bytes.size() &&
            std::memcmp(target, signature, length) == 0, "animation hook contract");
    ++installs;
    if (failure_mode == "throw-hook" && installs == 2)
        throw std::runtime_error("synthetic hook allocation failure");
    if (fail_second && installs == 2) return false;
    hook->target = target;
    hook->gateway = installs == 1 ? reinterpret_cast<void*>(&native_read) :
                    installs == 3 ? reinterpret_cast<void*>(&native_destroy) :
                    reinterpret_cast<void*>(&native_read);
    hook->length = length;
    return true;
}
bool A2FO_CALL register_class(const char*, const char* const*, std::uint32_t count,
                              A2FO_GameObjectClassLoadedHandler callback, void*) {
    require(count && count <= 64, "animation class field chunk");
    ++class_registrations;
    if ((failure_mode == "throw-class-first" && class_registrations == 1) ||
        (failure_mode == "throw-class-later" && class_registrations == 2))
        throw std::runtime_error("synthetic class registration failure");
    if (failure_mode == "false-class-first" && class_registrations == 1)
        return false;
    class_callback = callback; return true;
}
bool A2FO_CALL register_craft(const char*, A2FO_CraftEventHandler callback, void*) {
    craft_callback = callback; return true;
}
bool A2FO_CALL register_producer(const char*, A2FO_ProducerEventHandler callback, void*) {
    ++producer_registrations;
    if (failure_mode == "throw-producer")
        throw std::runtime_error("synthetic producer registration failure");
    producer_callback = callback; return true;
}
bool A2FO_CALL register_weapon(const char*, const char* const*, std::uint32_t,
                               A2FO_WeaponClassLoadedHandler callback, void*) {
    weapon_callback = callback; return true;
}
bool A2FO_CALL register_trigger(const char*, A2FO_WeaponTriggerHandler callback, void*) {
    trigger_callback = callback; return true;
}
void seed(bool bad_signature) {
    image = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 0x410000,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    require(image, "animation synthetic image");
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    dos->e_magic = IMAGE_DOS_SIGNATURE; dos->e_lfanew = 0x80;
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS32*>(image + 0x80);
    nt->Signature = IMAGE_NT_SIGNATURE;
    nt->FileHeader.Machine = IMAGE_FILE_MACHINE_I386;
    nt->FileHeader.TimeDateStamp = supported_armada::kCanonicalTimestamp;
    nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR32_MAGIC;
    nt->OptionalHeader.SizeOfImage = supported_armada::kCanonicalImageSize;
    for (const auto& spec : helper_specs())
        std::memcpy(image + spec.rva, spec.bytes.data(), spec.bytes.size());
    for (const auto& spec : hook_specs())
        std::memcpy(image + spec.rva, spec.bytes.data(), spec.bytes.size());
    if (bad_signature) image[hook_specs()[0].rva] ^= 1;
}
void choose(bool enabled) {
    ModulePolicy policy;
    policy.managed = true;
    ModulePolicyEntry entry;
    entry.name = "A2FOAnimations";
    entry.state = enabled ? ModulePolicyState::active : ModulePolicyState::inactive;
    policy.entries.push_back(entry);
    std::string error;
    require(apply_builtin_module_policy({roots}, policy, error),
            error.empty() ? "animation policy" : error.c_str());
    require(builtin_module_enabled(BuiltinFeature::animations) == enabled,
            "animation gate");
}
void run(const std::string& mode) {
    failure_mode = mode;
    fail_second = mode == "partial";
    seed(mode == "signature");
    A2FO_ModuleApi api{};
    api.struct_size = sizeof(api); api.api_version = A2FO_MODULE_API_VERSION;
    api.log = log_message; api.armada_module = armada_module;
    api.install_inline_hook = install_hook;
    api.register_game_object_class_loaded_handler = register_class;
    api.register_craft_event_handler = register_craft;
    api.register_producer_event_handler = register_producer;
    api.register_weapon_class_loaded_handler = register_weapon;
    api.register_weapon_trigger_handler = register_trigger;
    set_builtin_module_roots({roots});
    const bool prepared = mode == "success" || mode == "dispatch";
    require(prepare_builtin_module(BuiltinFeature::animations, api, false) == prepared,
            "animation preparation");
    finish_builtin_module_preparation(false, mode != "dispatch", mode != "dispatch",
        false, mode != "dispatch", mode != "dispatch", false, mode != "dispatch");
    const bool available = prepared && mode != "dispatch";
    require(builtin_module_available(BuiltinFeature::animations) == available,
            "animation dispatch availability");
    const auto original_probe=[](){
        animations::NativePlaybackProof value{};
        value.image=reinterpret_cast<HMODULE>(std::uintptr_t{0x1111});
        value.target=reinterpret_cast<void*>(std::uintptr_t{0x2222});
        value.replacement=reinterpret_cast<void*>(std::uintptr_t{0x3333});
        value.gateway=reinterpret_cast<void*>(std::uintptr_t{0x4444});
        value.length=77;value.epoch=88;return value;
    };
    const auto same_probe=[](const animations::NativePlaybackProof& a,
                             const animations::NativePlaybackProof& b){
        return a.image==b.image&&a.target==b.target&&a.replacement==b.replacement&&
               a.gateway==b.gateway&&a.length==b.length&&a.epoch==b.epoch;
    };
    if(!prepared){
        auto probe=original_probe();const auto before=probe;
        require(!builtin_animations_native_playback(reinterpret_cast<HMODULE>(image),probe)&&
                same_probe(probe,before),"unready native playback proof refused atomically");
    }
    require(installs == (mode == "signature" ? 0u :
                         (mode == "partial" || mode == "throw-hook") ? 2u : 8u),
            "animation hook count");
    const unsigned expected_classes = mode == "throw-class-first" ? 1u :
        mode == "throw-class-later" ? 2u :
        (mode == "signature" || mode == "partial" || mode == "throw-hook") ? 0u : 4u;
    require(class_registrations == expected_classes, "animation registration stop order");
    require(producer_registrations == (mode == "success" || mode == "dispatch" ||
        mode == "throw-producer" ? 1u : 0u), "animation later registration stop order");
    if (mode.rfind("throw-", 0) == 0)
        require(last_log == "Animation initialization failed; native playback retained",
                "animation exception diagnostic");
    if (mode == "false-class-first")
        require(last_log == "Animation callbacks unavailable; native playback retained",
                "animation false-return diagnostic");
    if (!available) {
        ModulePolicy request;
        ModulePolicyEntry entry;
        entry.name = "A2FOAnimations";
        entry.state = ModulePolicyState::active;
        request.entries.push_back(entry);
        std::string error;
        require(!apply_builtin_module_policy({roots}, request, error) && !error.empty(),
                "unavailable animation activation rejected");
        if (h_read.gateway) {
            std::array<std::uint8_t, 0x60> channel{};
            expected_channel = channel.data(); expected_stream = image;
            read_hook(expected_channel, nullptr, expected_stream);
            require(reads == 1 && g_samples.empty(), "failed preparation pass-through");
        }
        return;
    }
    require(class_callback && craft_callback && producer_callback &&
            weapon_callback && trigger_callback, "animation callbacks retained");
    auto proof=original_probe();
    require(builtin_animations_native_playback(reinterpret_cast<HMODULE>(image),proof)&&
            proof.image==reinterpret_cast<HMODULE>(image)&&
            proof.target==h_evaluate.target&&proof.replacement==reinterpret_cast<void*>(evaluate_hook)&&
            proof.gateway==h_evaluate.gateway&&proof.length==h_evaluate.length&&proof.epoch,
            "zero-class native playback proof published");
    const auto empty_epoch=proof.epoch;
    auto wrong=original_probe();const auto wrong_before=wrong;
    require(!builtin_animations_native_playback(reinterpret_cast<HMODULE>(image+1),wrong)&&
            same_probe(wrong,wrong_before),"foreign image proof refused atomically");
    const A2FO_OdfFieldView clip_fields[]{
        {{"animation0event",15},{"spawn",5}},
        {{"animation0start",15},{"0",1}},
        {{"animation0end",13},{"1",1}},
    };
    std::array<std::uint8_t,8> clip_class{};
    const A2FO_GameObjectClassLoadedEvent clip_event{sizeof(clip_event),clip_class.data(),
        {"fixture.odf",11},clip_fields,3};
    class_callback(&clip_event,nullptr);
    require(g_classes.count(clip_class.data())==1,"actual class callback publishes one clip");
    auto blocked=original_probe();const auto blocked_before=blocked;
    require(!builtin_animations_native_playback(reinterpret_cast<HMODULE>(image),blocked)&&
            same_probe(blocked,blocked_before),"custom class refuses native fallback atomically");
    const A2FO_GameObjectClassLoadedEvent empty_event{sizeof(empty_event),clip_class.data(),
        {"fixture.odf",11},nullptr,0};
    class_callback(&empty_event,nullptr);
    require(g_classes.empty()&&
            builtin_animations_native_playback(reinterpret_cast<HMODULE>(image),proof)&&
            proof.epoch>empty_epoch,"empty class reload restores native proof at new epoch");
    std::array<std::uint8_t, 0x60> channel{};
    std::array<float, 12> frame{};
    const std::uint32_t count = 1;
    void* frames = frame.data();
    std::memcpy(channel.data() + 0x34, &count, sizeof(count));
    std::memcpy(channel.data() + 0x4c, &frames, sizeof(frames));
    expected_channel = channel.data(); expected_stream = image;
    choose(false);
    read_hook(expected_channel, nullptr, expected_stream);
    require(reads == 1 && g_samples.count(expected_channel) == 1,
            "source frames retained while animation disabled");
    void* klass = channel.data();
    g_classes[klass] = {};
    g_class_names[klass] = "synthetic.odf";
    g_weapon_events[klass] = "test_fire";
    g_states[klass] = State{};
    g_instances[klass] = klass;
    g_launches[klass] = Launch{};
    g_docks[klass] = klass;
    g_jobs[klass] = 1;
    choose(true);
    require(runtime_enabled() && g_states.empty() && g_instances.empty() &&
            g_launches.empty() && g_docks.empty() && g_jobs.empty(),
            "activation clears transient animation state");
    require(g_samples.count(expected_channel) && g_classes.count(klass) &&
            g_class_names.count(klass) &&
            g_weapon_events.count(klass), "animation source and class metadata retained");
    choose(false);
    require(!runtime_enabled(), "animation off generation observed");
    require(producer_callback(reinterpret_cast<const A2FO_ProducerEvent*>(
                std::uintptr_t{1}), nullptr),
            "disabled producer event returns before reading its pointer");
    g_states[klass] = State{};
    g_jobs[klass] = 2;
    A2FO_CraftEvent cleanup_event{sizeof(cleanup_event), A2FO_CRAFT_EVENT_CLEANUP,
                                  klass, 0.0f};
    craft_callback(&cleanup_event, nullptr);
    require(g_states.empty() && g_jobs.empty(), "craft cleanup runs while disabled");
    destroy_hook(expected_channel, nullptr);
    require(destroys == 1 && !g_samples.count(expected_channel),
            "channel destroy forwards native and releases source");
    auto after_reinit=original_probe();const auto before_reinit=after_reinit;
    require(!A2FOBuiltinAnimations_Init(nullptr)&&!g_ready&&
            !builtin_animations_native_playback(reinterpret_cast<HMODULE>(image),after_reinit)&&
            same_probe(after_reinit,before_reinit),
            "failed reinitialization revokes passive proof without changing caller output");
}
}
int main(int argc, char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

    if (argc != 2) return 2;
    try { run(argv[1]); std::printf("builtin animations %s: PASS\n", argv[1]); return 0; }
    catch (const std::exception& error) {
        std::fprintf(stderr, "builtin animations %s: FAIL: %s\n", argv[1], error.what());
        return 1;
    }
}
