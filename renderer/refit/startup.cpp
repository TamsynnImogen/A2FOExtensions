#include "startup.hpp"
#include "../../core/extension_roots.hpp"
#include "fo_profile.hpp"
#include "host.h"
#include "../../core/refit_scene_profile.hpp"
#include "../../rust/native_hooks_ffi.h"
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <d3d8.h>
#include <new>
#include <shellapi.h>

extern "C" {
std::uintptr_t a2fo_refit_workspace_ctor_original=0;
void a2fo_refit_workspace_ctor_tap();
}

// Reuse Core's already linked file-hash implementation, outside loader lock.
namespace a2fo::native11_capture {
bool exact_image_file(HMODULE, const unsigned char *) noexcept;
}
namespace a2fo::refit {
namespace {
using Factory = IDirect3D8 *(WINAPI *)(UINT);
// MS thiscall's ECX + three callee-popped stack arguments, expressed as
// fastcall with an unused EDX argument. The checked finder overwrites EDX
// before using it. This also avoids GCC's non-member-thiscall diagnostics.
using Finder = std::uintptr_t(__fastcall *)(void *, void *, int, int, int);
struct Runtime {
  HMODULE image = nullptr, display = nullptr;
  const profile::Image *identity = nullptr;
  StartupLog log = nullptr;
  StartupSourcesReady sources_ready = nullptr;
  DWORD owner = 0;
  HANDLE phase_event = nullptr; // one-shot probe; retained for a late worker
  unsigned backend = 0, width = 0, height = 0;
  std::string helper;
  Factory original = nullptr, native = nullptr;
  Finder finder = nullptr;
  std::atomic<unsigned> state{
      0}; // 0 not attempted, 1 preparing, 2 ready, 3 failed
  std::atomic<bool> modes{false};
  std::atomic<bool> reserved{false};
  bool sources_hooked=false,sources_attempted=false;
  HMODULE constructor_fleet=nullptr;
  std::uintptr_t constructor_original=0;
};
Runtime *runtime =
    nullptr; // process lifetime; owning module is pinned before hooks

void report(const char *text) noexcept {
  try {
    if (runtime && runtime->log)
      runtime->log(std::string("[RefitNative] ") + text);
  } catch (...) {
  }
}
bool read_at(HMODULE image, std::uint32_t rva, void *output,
             std::size_t count) noexcept {
  const auto base = reinterpret_cast<std::uintptr_t>(image);
  if (!image || rva > UINTPTR_MAX - base || count > UINTPTR_MAX - (base + rva))
    return false;
  SIZE_T copied = 0;
  return ReadProcessMemory(GetCurrentProcess(),
                           reinterpret_cast<void *>(base + rva), output, count,
                           &copied) &&
         copied == count;
}
const profile::Image *identity(HMODULE image) noexcept {
  IMAGE_DOS_HEADER dos{};
  IMAGE_NT_HEADERS32 nt{};
  if (!read_at(image, 0, &dos, sizeof(dos)) ||
      dos.e_magic != IMAGE_DOS_SIGNATURE || dos.e_lfanew < 0 ||
      dos.e_lfanew > 65536 ||
      !read_at(image, static_cast<std::uint32_t>(dos.e_lfanew), &nt,
               sizeof(nt)) ||
      nt.Signature != IMAGE_NT_SIGNATURE ||
      nt.FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
      nt.OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
      nt.OptionalHeader.ImageBase != profile::preferred_base ||
      nt.FileHeader.NumberOfSections < 1 ||
      nt.FileHeader.NumberOfSections > 96 ||
      nt.FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32))
    return nullptr;
  std::uint32_t fnv = 0x811c9dc5;
  const auto hash = [&](const unsigned char *bytes, std::size_t n) {
    for (std::size_t i = 0; i < n; ++i)
      fnv = (fnv ^ bytes[i]) * 0x01000193u;
  };
  const std::uint32_t table = static_cast<std::uint32_t>(dos.e_lfanew) + 24u +
                              nt.FileHeader.SizeOfOptionalHeader;
  for (unsigned i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
    IMAGE_SECTION_HEADER s{};
    if (!read_at(image, table + i * sizeof(s), &s, sizeof(s)))
      return nullptr;
    hash(s.Name, 8);
    const std::uint32_t words[] = {s.VirtualAddress, s.Misc.VirtualSize,
                                   s.SizeOfRawData};
    hash(reinterpret_cast<const unsigned char *>(words), sizeof(words));
  }
  for (const auto &p : profile::images)
    if (p.timestamp == nt.FileHeader.TimeDateStamp &&
        p.size == nt.OptionalHeader.SizeOfImage && p.sections == fnv)
      return &p;
  return nullptr;
}
bool mode_code_matches(HMODULE image) noexcept {
  std::array<unsigned char, profile::finder_bytes.size()> bytes{};
  if (!read_at(image, profile::finder_rva, bytes.data(), bytes.size()) ||
      bytes != profile::finder_bytes)
    return false;
  const auto base = reinterpret_cast<std::uintptr_t>(image);
  for (auto rva : profile::mode_calls) {
    unsigned char call[5]{};
    std::int32_t disp = 0;
    if (!read_at(image, rva, call, sizeof(call)) || call[0] != 0xe8)
      return false;
    std::memcpy(&disp, call + 1, 4);
    if (static_cast<std::uint32_t>(base + rva + 5 + disp) !=
        static_cast<std::uint32_t>(base + profile::finder_rva))
      return false;
  }
  return true;
}
std::uintptr_t __fastcall mode_hook(void *device, void *, int width, int height,
                                    int bpp) {
  const auto found = runtime->finder(device, nullptr, width, height, bpp);
  if (found || !device || !runtime->modes.load(std::memory_order_acquire) ||
      GetCurrentThreadId() != runtime->owner)
    return found;
  // Let the checked engine finder interpret its own list. Never walk an
  // assumed GOG/FO object layout or mutate the user's saved graphics profile.
  return runtime->finder(device, nullptr, static_cast<int>(runtime->width),
                         static_cast<int>(runtime->height), 32);
}
bool install_modes() noexcept {
  if (!mode_code_matches(runtime->image))
    return false;
  const auto base = reinterpret_cast<std::uintptr_t>(runtime->image);
  runtime->finder = reinterpret_cast<Finder>(base + profile::finder_rva);
  std::array<std::array<unsigned char, 5>, 6> original{}, replacement{};
  struct Page {
    std::uintptr_t address = 0;
    DWORD protection = 0;
  };
  std::array<Page, 6> pages{};
  std::size_t page_count = 0;
  SYSTEM_INFO system{};
  GetSystemInfo(&system);
  const auto page_size = static_cast<std::uintptr_t>(system.dwPageSize);
  if (!page_size || (page_size & (page_size - 1)))
    return false;
  const auto callback = reinterpret_cast<std::uintptr_t>(&mode_hook);
  for (std::size_t i = 0; i < profile::mode_calls.size(); ++i) {
    const auto address = base + profile::mode_calls[i];
    const auto delta = static_cast<std::int64_t>(callback) -
                       static_cast<std::int64_t>(address + 5);
    if (delta < INT32_MIN || delta > INT32_MAX ||
        !read_at(runtime->image, profile::mode_calls[i], original[i].data(), 5))
      return false;
    replacement[i][0] = 0xe8;
    const auto disp = static_cast<std::int32_t>(delta);
    std::memcpy(replacement[i].data() + 1, &disp, 4);
    const auto page = address & ~(page_size - 1);
    if (address + 4 >= page + page_size)
      return false;
    bool seen = false;
    for (std::size_t k = 0; k < page_count; ++k)
      seen = seen || pages[k].address == page;
    if (!seen)
      pages[page_count++].address = page;
  }
  std::size_t writable = 0;
  for (; writable < page_count; ++writable)
    if (!VirtualProtect(reinterpret_cast<void *>(pages[writable].address),
                        page_size, PAGE_EXECUTE_READWRITE,
                        &pages[writable].protection))
      break;
  // Each page is handled once: the paired calls share three pages. Saving
  // protections per call would restore the second call's temporary RWX state.
  if (writable != page_count || !mode_code_matches(runtime->image)) {
    for (std::size_t i = 0; i < writable; ++i) {
      DWORD ignored = 0;
      VirtualProtect(reinterpret_cast<void *>(pages[i].address), page_size,
                     pages[i].protection, &ignored);
    }
    return false;
  }
  // Startup is externally serialized on the game thread, before a device
  // exists. Publish mode aliases only after all six writes and protection
  // restores.
  for (std::size_t i = 0; i < profile::mode_calls.size(); ++i)
    std::memcpy(reinterpret_cast<void *>(base + profile::mode_calls[i]),
                replacement[i].data(), 5);
  bool ok = FlushInstructionCache(GetCurrentProcess(), nullptr, 0) != FALSE;
  for (std::size_t i = 0; i < page_count; ++i) {
    DWORD ignored = 0;
    ok = (VirtualProtect(reinterpret_cast<void *>(pages[i].address), page_size,
                         pages[i].protection, &ignored) != FALSE) &&
         ok;
  }
  if (!ok) {
    // A rollback failure leaves an inert forwarding thunk; its pinned owner
    // and original function remain valid, and no native device is admitted.
    for (std::size_t k = 0; k < page_count; ++k) {
      DWORD old = 0, ignored = 0;
      auto *at = reinterpret_cast<void *>(pages[k].address);
      if (VirtualProtect(at, page_size, PAGE_EXECUTE_READWRITE, &old)) {
        for (std::size_t i = 0; i < profile::mode_calls.size(); ++i) {
          const auto call = base + profile::mode_calls[i];
          if ((call & ~(page_size - 1)) == pages[k].address)
            std::memcpy(reinterpret_cast<void *>(call), original[i].data(), 5);
        }
        VirtualProtect(at, page_size, pages[k].protection, &ignored);
      }
    }
    FlushInstructionCache(GetCurrentProcess(), nullptr, 0);
    return false;
  }
  runtime->modes.store(true, std::memory_order_release);
  return true;
}
template <class T> T proc(HMODULE module, const char *name) noexcept {
  const auto raw = GetProcAddress(module, name);
  T result = nullptr;
  static_assert(sizeof(result) == sizeof(raw));
  std::memcpy(&result, &raw, sizeof(result));
  return result;
}
// Windows serializes DLL initialization and the initial entry of a newly
// created thread. A fresh thread cannot signal this event while our caller
// owns the loader lock. Only a positive signal permits helper loading; a slow
// thread or nested DLL callout gets the original renderer, without waiting
// indefinitely for the serialized startup. The pinned event/module survive a
// late worker after timeout. No game/window/input operation occurs here.
DWORD WINAPI phase_probe(void *event) noexcept {
  SetEvent(static_cast<HANDLE>(event));
  return 0;
}
bool confirm_factory_phase() noexcept {
  runtime->phase_event = CreateEventA(nullptr, TRUE, FALSE, nullptr);
  if (!runtime->phase_event)
    return false;
  HANDLE thread =
      CreateThread(nullptr, 0, &phase_probe, runtime->phase_event, 0, nullptr);
  if (!thread) {
    CloseHandle(runtime->phase_event);
    runtime->phase_event = nullptr;
    return false;
  }
  CloseHandle(thread);
  return WaitForSingleObject(runtime->phase_event, 100) == WAIT_OBJECT_0;
}
constexpr std::uint32_t workspace_context_rva=0x13e470,workspace_call_rva=0x13e484;
bool fleet_constructor_matches(HMODULE fleet) noexcept {
  namespace p=a2fo::refit_scene::profile;
  if(!runtime||!fleet)return false;
  auto bytes=p::fleet_constructor_bytes;
  if(!read_at(fleet,p::fleet_constructor_rva,bytes.data(),bytes.size()))return false;
  const auto base=reinterpret_cast<std::uintptr_t>(fleet);
  const auto delta=static_cast<std::uint32_t>(base)-p::fleet_preferred_base;
  for(const auto at:p::fleet_constructor_relocations){
    std::uint32_t value=0;std::memcpy(&value,bytes.data()+at,4);value-=delta;
    std::memcpy(bytes.data()+at,&value,4);
  }
  std::uint32_t slot=0,original=0;
  return bytes==p::fleet_constructor_bytes&&read_at(fleet,0x211e60,&slot,4)&&
      slot==base+0x210b90&&read_at(fleet,0x210b90,&original,4)&&
      original==reinterpret_cast<std::uintptr_t>(runtime->image)+0x22ae80;
}
bool workspace_code_matches(bool hooked) noexcept {
  namespace p=a2fo::refit_scene::profile;
  auto actual=p::workspace_context_bytes;
  if(!runtime||!read_at(runtime->image,workspace_context_rva,actual.data(),actual.size()))return false;
  constexpr auto offset=workspace_call_rva-workspace_context_rva;
  std::int32_t relative=0;std::memcpy(&relative,actual.data()+offset+1,4);
  const auto site=reinterpret_cast<std::uintptr_t>(runtime->image)+workspace_call_rva;
  const auto target=static_cast<std::uint32_t>(site+5+relative);
  if(hooked){
    if(actual[offset]!=0xe8||target!=
       reinterpret_cast<std::uintptr_t>(&a2fo_refit_workspace_ctor_tap))return false;
    std::memcpy(actual.data()+offset,p::workspace_context_bytes.data()+offset,5);
    if(runtime->constructor_fleet&&!fleet_constructor_matches(runtime->constructor_fleet))return false;
  }else if(actual[offset]==0xe8){
    const auto native=reinterpret_cast<std::uintptr_t>(runtime->image)+0x22ae80;
    if(target!=native){
      HMODULE fleet=GetModuleHandleA("FleetOpsHook.dll");
      if(!fleet||target!=reinterpret_cast<std::uintptr_t>(fleet)+p::fleet_constructor_rva||
         !fleet_constructor_matches(fleet))return false;
      runtime->constructor_fleet=fleet;
      std::memcpy(actual.data()+offset,p::workspace_context_bytes.data()+offset,5);
    }
    runtime->constructor_original=target;
  }
  const auto delta=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(runtime->image))-0x400000u;
  for(const auto at:p::workspace_context_relocations){
    std::uint32_t value=0;std::memcpy(&value,actual.data()+at,4);value-=delta;
    std::memcpy(actual.data()+at,&value,4);
  }
  if(actual!=p::workspace_context_bytes){
    for(std::size_t i=0;i<actual.size();++i)if(actual[i]!=p::workspace_context_bytes[i]){
      char message[128]{};
      std::snprintf(message,sizeof(message),"workspace context differs at RVA %08x: actual %02x expected %02x",
          static_cast<unsigned>(workspace_context_rva+i),actual[i],p::workspace_context_bytes[i]);
      report(message);break;
    }
    return false;
  }
  return true;
}
void sources_before_workspace() noexcept {
  if(!runtime||!runtime->sources_ready||!runtime->sources_hooked||runtime->sources_attempted||
     GetCurrentThreadId()!=runtime->owner||runtime->state.load(std::memory_order_acquire)!=0)return;
  runtime->sources_attempted=true;
  std::uint32_t workspace=1;
  if(!confirm_factory_phase()||!workspace_code_matches(true)||
     !read_at(runtime->image,0x3ad508,&workspace,4)||workspace||
     !a2fo::native11_capture::exact_image_file(runtime->image,runtime->identity->sha.data())||
     (runtime->constructor_fleet&&!a2fo::native11_capture::exact_image_file(
          runtime->constructor_fleet,a2fo::refit_scene::profile::fleet_sha.data()))){
    report("pre-workspace CPU source admission refused; original constructor retained");return;
  }
  report("pre-workspace CPU sources admitted outside loader lock; original constructor retained");
  runtime->sources_ready();
}
bool install_workspace_sources() noexcept {
  if(!runtime||!runtime->sources_ready||!IsProcessorFeaturePresent(PF_XMMI_INSTRUCTIONS_AVAILABLE)||
     !workspace_code_matches(false))return false;
  const auto base=reinterpret_cast<std::uintptr_t>(runtime->image);
  a2fo_refit_workspace_ctor_original=runtime->constructor_original;
  std::array<std::uint8_t,5> call{};
  if(!read_at(runtime->image,workspace_call_rva,call.data(),call.size()))return false;
  const bool patched=a2fo_rs_windows_patch_call(reinterpret_cast<void*>(base+workspace_call_rva),
      reinterpret_cast<void*>(&a2fo_refit_workspace_ctor_tap),call.data(),call.size());
  runtime->sources_hooked=workspace_code_matches(true);
  return patched&&runtime->sources_hooked;
}
IDirect3D8 *prepare_factory(UINT sdk) {
  if (!a2fo::native11_capture::exact_image_file(
          runtime->image, runtime->identity->sha.data()) ||
      !mode_code_matches(runtime->image)) {
    report("exact executable or live mode ownership rejected; test route "
           "unavailable");
    return nullptr;
  }
  // Loading and hash providers are deliberately deferred out of DllMain.
  runtime->display = LoadLibraryExA(runtime->helper.c_str(), nullptr,
                                    LOAD_WITH_ALTERED_SEARCH_PATH);
  if (!runtime->display) {
    report("could not load the child mod's A2FORefitDisplay.dll");
    return nullptr;
  }
  const auto prepare = proc<A2FORefitDisplayPrepare>(
      runtime->display, "A2FORefitDisplay_Prepare_v1");
  const auto passthrough = proc<A2FORefitDisplayPassthrough>(
      runtime->display, "A2FORefitDisplay_Passthrough_v1");
  runtime->native = proc<Factory>(runtime->display, "Direct3DCreate8");
  if (runtime->state.load(std::memory_order_acquire) != 1 || !prepare ||
      !passthrough || !runtime->native) {
    report("child Display helper lacks the required explicit host ABI");
    return nullptr;
  }
  HMODULE pinned = nullptr;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                              GET_MODULE_HANDLE_EX_FLAG_PIN,
                          reinterpret_cast<LPCSTR>(prepare), &pinned) ||
      pinned != runtime->display)
    return nullptr;
  const int width = GetSystemMetrics(SM_CXSCREEN),
            height = GetSystemMetrics(SM_CYSCREEN);
  if (width < 1 || height < 1 || width > 8192 || height > 8192)
    return nullptr;
  runtime->width = static_cast<unsigned>(width);
  runtime->height = static_cast<unsigned>(height);
  if (prepare(runtime->backend, runtime->width, runtime->height, 0) != 0 ||
      passthrough(1) != 0)
    return nullptr;
  IDirect3D8 *factory = runtime->native(sdk);
  if (!factory)
    return nullptr;
  if (runtime->state.load(std::memory_order_acquire) != 1 || !install_modes()) {
    factory->Release();
    report("six-call mode alias transaction failed; no native game device "
           "created");
    return nullptr;
  }
  report(runtime->backend == 1 ? "Vulkan factory returned; waiting for the "
                                 "game's CreateDevice (FO UI retained)"
                               : "DX12 factory returned; waiting for the "
                                 "game's CreateDevice (FO UI retained)");
  return factory;
}
IDirect3D8 *WINAPI factory_hook(UINT sdk) noexcept {
  if (!runtime)
    return nullptr;
  const auto legacy = [sdk]() noexcept {
    return runtime->original ? runtime->original(sdk) : nullptr;
  };
  const unsigned state = runtime->state.load(std::memory_order_acquire);
  if (GetCurrentThreadId() != runtime->owner) {
    unsigned expected = 0;
    if (state == 3 || runtime->state.compare_exchange_strong(expected, 3))
      return legacy(); // disarmed before preparation could start
    // Preparing or live native factories belong exclusively to the owner.
    // Do not change their state or hand out a competing legacy interface.
    return nullptr;
  }
  if (state == 2)
    return runtime->native(sdk);
  if (state != 0) {
    if (state == 1) {
      runtime->state.store(3, std::memory_order_release); // reject reentry
      return nullptr;
    }
    return legacy();
  }
  unsigned expected = 0;
  if (!runtime->state.compare_exchange_strong(expected, 1))
    return legacy();
  try {
    if (!confirm_factory_phase()) {
      runtime->state.store(3, std::memory_order_release);
      report("loader phase could not be confirmed within 100ms; original "
             "renderer retained");
      return legacy();
    }
    auto *factory = prepare_factory(sdk);
    runtime->state.store(factory ? 2 : 3, std::memory_order_release);
    if (!factory) {
      report("native factory preparation refused; original renderer retained");
      return legacy();
    }
    return factory;
  } catch (...) {
    runtime->state.store(3, std::memory_order_release);
    report("native factory preparation failed; original renderer retained");
    return legacy();
  }
}
// Caller has validated this import slot and pinned/published runtime. Keep
// the pointer transaction independent of file/profile admission so failures
// never leave an active native route behind a partially protected import.
bool patch_factory_slot(void **slot, Factory original) noexcept {
  DWORD old = 0;
  if (!VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &old)) {
    runtime->state.store(3, std::memory_order_release);
    return false;
  }
  void *previous = InterlockedCompareExchangePointer(
      reinterpret_cast<PVOID volatile *>(slot),
      reinterpret_cast<void *>(&factory_hook),
      reinterpret_cast<void *>(original));
  DWORD ignored = 0;
  const bool restored =
      VirtualProtect(slot, sizeof(void *), old, &ignored) != FALSE;
  if (previous != reinterpret_cast<void *>(original) || !restored) {
    runtime->state.store(3, std::memory_order_release);
    if (previous == reinterpret_cast<void *>(original))
      InterlockedCompareExchangePointer(
          reinterpret_cast<PVOID volatile *>(slot),
          reinterpret_cast<void *>(original),
          reinterpret_cast<void *>(&factory_hook));
    // A failed protection restore leaves the page writable. Retry after the
    // rollback; never discard the pinned context even if the OS refuses.
    if (!restored)
      VirtualProtect(slot, sizeof(void *), old, &ignored);
    return false;
  }
  return true;
}
} // namespace
bool install_startup(HMODULE executable, const std::string &root,
                     StartupLog log,StartupSourcesReady sources_ready) noexcept {
  bool requested = false;
  try {
    const auto selected = a2fo::active_mod_from_command_line(GetCommandLineA());
    if (!a2fo::safe_mod_directory_name(selected))
      return false;
    std::string data = root, mod, ini;
    bool found = false;
    for (unsigned up = 0; up < 4 && !data.empty(); ++up) {
      mod = data + "\\Mods\\" + selected;
      ini = mod + "\\A2FORenderer.ini";
      if (GetFileAttributesA((mod + "\\info.ini").c_str()) !=
          INVALID_FILE_ATTRIBUTES) {
        found = true;
        break;
      }
      const auto split = data.find_last_of("\\/");
      if (split == std::string::npos)
        break;
      data.resize(split);
    }
    if (!found)
      return false;
    char backend[64]{};
    GetPrivateProfileStringA("D3D8Compat", "Backend", "", backend,
                             sizeof(backend), ini.c_str());
    unsigned kind = 0;
    if (!_stricmp(backend, "refit-vulkan-test"))
      kind = 1;
    else if (!_stricmp(backend, "refit-d3d12-test"))
      kind = 2;
    else
      return false;
    requested = true;
    // FO's /d3d9 family substitutes a different factory and device ABI.
    // This checked D3D8 route must never be applied to those call sites.
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) {
      if (log)
        log("[RefitNative] Could not validate graphics mode; no factory hook "
            "installed");
      return true;
    }
    bool d3d9 = false;
    for (int i = 1; i < argc; ++i)
      d3d9 =
          d3d9 || !_wcsicmp(argv[i], L"/d3d9") || !_wcsicmp(argv[i], L"-d3d9");
    LocalFree(argv);
    if (d3d9) {
      if (log)
        log("[RefitNative] FO D3D9 mode requires a separate device bridge; "
            "legacy renderer retained");
      return true;
    }
    char parent[260]{};
    GetPrivateProfileStringA("mod", "ParentMod", "", parent, sizeof(parent),
                             (mod + "\\info.ini").c_str());
    if (!a2fo::safe_mod_directory_name(parent) ||
        !_stricmp(parent, selected.c_str())) {
      if (log)
        log("[RefitNative] Backend requires an explicit child mod; no factory "
            "hook installed");
      return true;
    }
    const auto *profile = identity(executable);
    if (executable != GetModuleHandleA(nullptr) || !profile ||
        !mode_code_matches(executable)) {
      if (log)
        log("[RefitNative] Unsupported executable/live mode profile; legacy "
            "renderer retained");
      return true;
    }
    HMODULE provider = GetModuleHandleA("d3d8.dll");
    const auto original =
        provider ? proc<Factory>(provider, "Direct3DCreate8") : nullptr;
    Factory slot_value = nullptr;
    if (!original ||
        !read_at(executable, profile::factory_iat_rva, &slot_value,
                 sizeof(slot_value)) ||
        slot_value != original) {
      if (log)
        log("[RefitNative] Factory import ownership differs; no hook "
            "installed");
      return true;
    }
    if (runtime) {
      if (log)
        log("[RefitNative] Duplicate startup request rejected");
      return true;
    }
    auto *pending = new (std::nothrow) Runtime;
    if (!pending)
      return true;
    pending->image = executable;
    pending->identity = profile;
    pending->log = log;
    pending->sources_ready = sources_ready;
    pending->owner = GetCurrentThreadId();
    pending->backend = kind;
    pending->helper = mod + "\\modules\\refit\\A2FORefitDisplay.dll";
    pending->original = original;
    HMODULE owner = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_PIN,
                            reinterpret_cast<LPCSTR>(&factory_hook), &owner)) {
      delete pending;
      return true;
    }
    auto **slot =
        reinterpret_cast<void **>(reinterpret_cast<std::uintptr_t>(executable) +
                                  profile::factory_iat_rva);
    runtime = pending;
    if (!patch_factory_slot(slot, original)) {
      Factory remaining = nullptr;
      runtime->reserved.store(read_at(executable, profile::factory_iat_rva,
                                      &remaining, sizeof(remaining)) &&
                                  remaining == &factory_hook,
                              std::memory_order_release);
      report("factory import transaction failed; native startup refused");
      return true;
    }
    runtime->reserved.store(true, std::memory_order_release);
    if(runtime->sources_ready&&!install_workspace_sources())
      report("pre-workspace source hook refused; Display route remains available without live parts");
    report("child-only factory route armed; helper/GPU work deferred until "
           "Direct3DCreate8");
    return true;
  } catch (...) {
    if (requested && log)
      try {
        log("[RefitNative] Early setup failed; no native success claimed");
      } catch (...) {
      }
    return requested;
  }
}
bool reserves_graphics_route() noexcept {
  return runtime && runtime->reserved.load(std::memory_order_acquire);
}
bool live_host_context(HMODULE& display,std::string& helper,unsigned& backend) noexcept {
  try {
    if(!runtime||GetCurrentThreadId()!=runtime->owner||
       runtime->state.load(std::memory_order_acquire)!=2||!runtime->display)return false;
    std::string path=runtime->helper;
    const auto slash=path.find_last_of("\\/");
    if(slash==std::string::npos)return false;
    path.resize(slash+1);path+="A2FORefitRenderer.dll";
    helper=std::move(path);display=runtime->display;backend=runtime->backend;return true;
  }catch(...){return false;}
}
} // namespace a2fo::refit

extern "C" void a2fo_refit_prepare_sources() noexcept {
  a2fo::refit::sources_before_workspace();
}
