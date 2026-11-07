#include <windows.h>
#include <d3d8.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static unsigned protect_calls = 0, flush_calls = 0;
static unsigned fail_protect = 0, fail_protect_second = 0, fail_flush = 0;
static BOOL WINAPI checked_protect(LPVOID, SIZE_T, DWORD, PDWORD);
static BOOL WINAPI checked_flush(HANDLE, LPCVOID, SIZE_T);
#define VirtualProtect checked_protect
#define FlushInstructionCache checked_flush
#include "../renderer/refit/startup.cpp"
#undef VirtualProtect
#undef FlushInstructionCache

static BOOL WINAPI checked_protect(LPVOID address, SIZE_T size, DWORD mode,
                                   PDWORD old) {
  ++protect_calls;
  if (protect_calls == fail_protect || protect_calls == fail_protect_second) {
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
  }
  return ::VirtualProtect(address, size, mode, old);
}
static BOOL WINAPI checked_flush(HANDLE process, LPCVOID address, SIZE_T size) {
  ++flush_calls;
  if (flush_calls == fail_flush) {
    SetLastError(ERROR_ACCESS_DENIED);
    return FALSE;
  }
  return ::FlushInstructionCache(process, address, size);
}
namespace a2fo::native11_capture {
unsigned exact_image_calls = 0;
bool allow_exact_image = false;
bool exact_image_file(HMODULE, const unsigned char *) noexcept {
  ++exact_image_calls;
  return allow_exact_image;
}
}
static void need(bool okay, const char *why) {
  if (!okay) {
    std::fprintf(stderr, "FAIL: %s (VP=%u Flush=%u)\n", why,
                 protect_calls, flush_calls);
    std::exit(1);
  }
}
static void faults(unsigned vp = 0, unsigned flush = 0, unsigned second = 0) {
  protect_calls = flush_calls = 0;
  fail_protect = vp;
  fail_protect_second = second;
  fail_flush = flush;
}
using namespace a2fo::refit;
struct ImageFixture {
  unsigned char *base;
  std::array<std::array<unsigned char, 5>, 6> calls{};
  Runtime context{};
  ImageFixture() {
    base = static_cast<unsigned char *>(VirtualAlloc(nullptr, 0x405000,
         MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    need(base != nullptr, "allocate owned synthetic image");
    std::memcpy(base + profile::finder_rva, profile::finder_bytes.data(),
                profile::finder_bytes.size());
    for (std::size_t i = 0; i < profile::mode_calls.size(); ++i) {
      auto *at = base + profile::mode_calls[i];
      at[0] = 0xe8;
      const auto disp = static_cast<std::int32_t>(profile::finder_rva -
                                                 (profile::mode_calls[i] + 5));
      std::memcpy(at + 1, &disp, 4);
      std::memcpy(calls[i].data(), at, 5);
    }
    DWORD old = 0;
    need(::VirtualProtect(base, 0x405000, PAGE_EXECUTE_READ, &old) != FALSE,
         "protect synthetic image RX");
    context.image = reinterpret_cast<HMODULE>(base);
    context.owner = GetCurrentThreadId();
    context.width = 1600;
    context.height = 900;
    runtime = &context;
    faults();
  }
  ~ImageFixture() {
    runtime = nullptr;
    VirtualFree(base, 0, MEM_RELEASE);
  }
  bool original_calls() const {
    for (std::size_t i = 0; i < calls.size(); ++i)
      if (std::memcmp(base + profile::mode_calls[i], calls[i].data(), 5))
        return false;
    return true;
  }
  bool patched_calls() const {
    const auto callback = reinterpret_cast<std::uintptr_t>(&mode_hook);
    for (auto rva : profile::mode_calls) {
      const auto *at = base + rva;
      std::int32_t disp = 0;
      std::memcpy(&disp, at + 1, 4);
      if (at[0] != 0xe8 ||
          static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(at) + 5 + disp) !=
              static_cast<std::uint32_t>(callback))
        return false;
    }
    return true;
  }
  bool pages_rx() const {
    for (auto rva : profile::mode_calls) {
      MEMORY_BASIC_INFORMATION info{};
      if (!VirtualQuery(base + rva, &info, sizeof(info)) ||
          info.Protect != PAGE_EXECUTE_READ)
        return false;
    }
    return true;
  }
  bool finder_unchanged() const {
    return !std::memcmp(base + profile::finder_rva,
                        profile::finder_bytes.data(), profile::finder_bytes.size());
  }
};
struct Mode { std::int32_t width, height, bpp; };
struct Node { Node *next; std::uint32_t unused; Mode *mode; };
struct Device { unsigned char prefix[20]; Node *head; };
static_assert(sizeof(Node) == 12 && offsetof(Node, mode) == 8 &&
              offsetof(Device, head) == 20, "32-bit finder fixture layout");
static void modes_success() {
  ImageFixture f;
  need(install_modes(), "install six mode calls");
  need(f.patched_calls() && f.pages_rx() && f.finder_unchanged(),
       "all six calls patched, pages RX, finder unchanged");
  need(protect_calls == 6 && flush_calls == 1 && f.context.modes.load(),
       "three pages acquired and restored, aliases published");
  Mode mode{1600, 900, 32};
  Node sentinel{}, node{};
  sentinel.next = &node;
  node.next = &sentinel;
  node.mode = &mode;
  Device device{};
  device.head = &sentinel;
  const auto found = reinterpret_cast<std::uintptr_t>(&mode);
  const auto active_exact = mode_hook(&device, nullptr, 1280, 720, 32);
  need(active_exact == found, "exact finder success retained");
  f.context.modes.store(false);
  need(mode_hook(&device, nullptr, 1280, 720, 32) == active_exact,
       "inactive and active modes preserve finder success identically");
  f.context.modes.store(true);
  need(mode_hook(&device, nullptr, 1920, 1080, 16) == found,
       "missing mode aliases to native dimensions and 32 bpp");
  f.context.modes.store(false);
  need(mode_hook(&device, nullptr, 1920, 1080, 16) == 0,
       "inactive alias forwards finder result");
  f.context.modes.store(true);
  f.context.owner = GetCurrentThreadId() + 1;
  need(mode_hook(&device, nullptr, 1920, 1080, 16) == 0,
       "wrong owner forwards finder result");
  std::puts("PASS: modes success and finder behavior");
}
static void mode_failures() {
  for (unsigned n = 1; n <= 3; ++n) {
    ImageFixture f;
    faults(n);
    need(!install_modes() && !f.context.modes.load(), "acquisition failure refused");
    need(f.original_calls() && f.finder_unchanged() && f.pages_rx(),
         "acquisition failure leaves original RX image");
  }
  {
    ImageFixture f;
    faults(0, 1);
    need(!install_modes() && !f.context.modes.load(), "flush failure refused");
    need(f.original_calls() && f.finder_unchanged() && f.pages_rx(),
         "flush failure rolls back original RX image");
  }
  for (unsigned n = 4; n <= 6; ++n) {
    ImageFixture f;
    faults(n);
    need(!install_modes() && !f.context.modes.load(), "restore failure refused");
    need(f.original_calls() && f.finder_unchanged() && f.pages_rx(),
         "restore failure rolls back original RX image");
  }
  {
    ImageFixture f;
    faults(4, 0, 7);
    need(!install_modes() && !f.context.modes.load(), "rollback fault refused");
    need(f.finder_unchanged(), "rollback fault leaves finder unchanged");
    // The first page may retain hook bytes after unrecoverable OS failure.
    // It is safe only while aliases remain inactive and the owner is pinned.
    need(!f.original_calls() && !f.patched_calls(),
         "rollback failure leaves mixed call sites");
    Mode mode{1600, 900, 32};
    Node sentinel{}, node{};
    sentinel.next = &node;
    node.next = &sentinel;
    node.mode = &mode;
    Device device{};
    device.head = &sentinel;
    need(mode_hook(&device, nullptr, 1920, 1080, 16) == 0,
         "residual thunk forwards with aliases inactive");
  }
  std::puts("PASS: modes fault transactions");
}
static unsigned source_callbacks=0;
static void sources_callback() noexcept {
  ++source_callbacks;
  sources_before_workspace(); // a callback cannot recursively admit sources
}
static void workspace_sources() {
  ImageFixture f;
  namespace p=a2fo::refit_scene::profile;
  DWORD old=0;
  need(::VirtualProtect(f.base,0x405000,PAGE_READWRITE,&old)!=FALSE,"workspace fixture writable");
  auto context=p::workspace_context_bytes;
  const auto delta=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(f.base))-0x400000u;
  for(const auto at:p::workspace_context_relocations){
    std::uint32_t value=0;std::memcpy(&value,context.data()+at,4);value+=delta;
    std::memcpy(context.data()+at,&value,4);
  }
  std::memcpy(f.base+workspace_context_rva,context.data(),context.size());
  need(::VirtualProtect(f.base,0x405000,PAGE_EXECUTE_READ,&old)!=FALSE,"workspace fixture RX");
  f.context.sources_ready=&sources_callback;f.context.identity=&profile::images[0];
  need(workspace_code_matches(false)&&install_workspace_sources()&&workspace_code_matches(true),
       "qualified first constructor call is owned");
  need(a2fo_refit_workspace_ctor_original==reinterpret_cast<std::uintptr_t>(f.base)+0x22ae80,
       "original constructor bound exactly");
  source_callbacks=0;const auto hashes=a2fo::native11_capture::exact_image_calls;
  f.context.owner=GetCurrentThreadId()+1;sources_before_workspace();
  need(!f.context.sources_attempted&&source_callbacks==0,"wrong owner cannot admit sources");
  f.context.owner=GetCurrentThreadId();f.context.state.store(2);sources_before_workspace();
  need(!f.context.sources_attempted&&source_callbacks==0,"live factory cannot admit late sources");
  f.context.state.store(0);sources_before_workspace();
  need(f.context.sources_attempted&&source_callbacks==0&&
       a2fo::native11_capture::exact_image_calls==hashes+1,"SHA refusal retains original without source callback");
  f.context.sources_attempted=false;a2fo::native11_capture::allow_exact_image=true;
  sources_before_workspace();sources_before_workspace();
  need(source_callbacks==1&&f.context.sources_attempted,"sources admitted exactly once despite repeat and reentry");
  a2fo::native11_capture::allow_exact_image=false;
  std::puts("PASS: workspace checked call, owner, late-state, SHA and one-shot/reentry source admission");
}
static void fleet_constructor_guards() {
  ImageFixture image;
  namespace p=a2fo::refit_scene::profile;
  auto *fleet=static_cast<unsigned char *>(VirtualAlloc(nullptr,0x212000,
      MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
  need(fleet!=nullptr,"allocate owned fake Fleet image");
  auto bytes=p::fleet_constructor_bytes;
  const auto base=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(fleet));
  const auto delta=base-p::fleet_preferred_base;
  for(const auto at:p::fleet_constructor_relocations) {
    std::uint32_t value=0;std::memcpy(&value,bytes.data()+at,4);value+=delta;
    std::memcpy(bytes.data()+at,&value,4);
  }
  std::memcpy(fleet+p::fleet_constructor_rva,bytes.data(),bytes.size());
  auto slot=base+0x210b90u;
  auto original=static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(image.base))+0x22ae80u;
  std::memcpy(fleet+0x211e60,&slot,4);
  std::memcpy(fleet+0x210b90,&original,4);
  const auto module=reinterpret_cast<HMODULE>(fleet);
  need(fleet_constructor_matches(module),"exact relocated Fleet constructor accepted");
  fleet[p::fleet_constructor_rva]^=1;
  need(!fleet_constructor_matches(module),"wrong Fleet wrapper byte refused");
  fleet[p::fleet_constructor_rva]^=1;
  fleet[p::fleet_constructor_rva+p::fleet_constructor_relocations[0]]^=1;
  need(!fleet_constructor_matches(module),"wrong relocated Fleet wrapper byte refused");
  fleet[p::fleet_constructor_rva+p::fleet_constructor_relocations[0]]^=1;
  slot+=4;std::memcpy(fleet+0x211e60,&slot,4);
  need(!fleet_constructor_matches(module),"wrong Fleet constructor pointer refused");
  slot-=4;std::memcpy(fleet+0x211e60,&slot,4);
  original+=4;std::memcpy(fleet+0x210b90,&original,4);
  need(!fleet_constructor_matches(module),"wrong Root original pointer refused");
  original-=4;std::memcpy(fleet+0x210b90,&original,4);
  need(fleet_constructor_matches(module),"exact Fleet constructor reaccepted after mutations");
  VirtualFree(fleet,0,MEM_RELEASE);
  std::puts("PASS: Fleet constructor wrapper bytes, relocations and pointer chain");
}
static unsigned original_factory_calls = 0;
static IDirect3D8 *WINAPI dummy_factory(UINT) {
  ++original_factory_calls;
  return reinterpret_cast<IDirect3D8 *>(static_cast<std::uintptr_t>(0x1234));
}
struct ThreadResult { IDirect3D8 *factory = nullptr; };
static DWORD WINAPI call_factory_on_worker(void *opaque) {
  auto *result = static_cast<ThreadResult *>(opaque);
  result->factory = factory_hook(8);
  return 0;
}
static void factory_thread_states() {
  const auto sentinel = reinterpret_cast<IDirect3D8 *>(static_cast<std::uintptr_t>(0x1234));
  for (unsigned initial = 0; initial <= 3; ++initial) {
    Runtime context{};
    context.owner = GetCurrentThreadId();
    context.original = &dummy_factory;
    context.state.store(initial);
    runtime = &context;
    original_factory_calls = 0;
    ThreadResult result{};
    HANDLE worker = CreateThread(nullptr, 0, &call_factory_on_worker,
                                 &result, 0, nullptr);
    need(worker != nullptr, "create non-owner factory worker");
    need(WaitForSingleObject(worker, 5000) == WAIT_OBJECT_0,
         "join non-owner factory worker");
    CloseHandle(worker);
    const bool legacy = initial == 0 || initial == 3;
    need(result.factory == (legacy ? sentinel : nullptr),
         "non-owner factory result follows state policy");
    need(context.state.load() == (initial == 0 ? 3u : initial),
         "non-owner does not corrupt preparing or ready state");
    need(original_factory_calls == (legacy ? 1u : 0u),
         "non-owner legacy factory call count");
  }
  Runtime reentry{};
  reentry.owner = GetCurrentThreadId();
  reentry.original = &dummy_factory;
  reentry.state.store(1);
  runtime = &reentry;
  original_factory_calls = 0;
  need(factory_hook(8) == nullptr && reentry.state.load() == 3 &&
       original_factory_calls == 0,
       "owner reentry disarms preparation without legacy factory");
  runtime = nullptr;
  std::puts("PASS: factory cross-thread and owner reentry states");
}
static bool inside_called = false;
static IDirect3D8 *inside_result = nullptr;
static unsigned inside_hashes = 0;
static unsigned inside_original_calls = 0;
extern "C" __declspec(dllexport) void refit_loader_probe_callback() {
  inside_called = true;
  inside_result = factory_hook(8);
  inside_hashes = a2fo::native11_capture::exact_image_calls;
  inside_original_calls = original_factory_calls;
}
static void loader_phase_cases(const char *dll) {
  const auto sentinel = reinterpret_cast<IDirect3D8 *>(static_cast<std::uintptr_t>(0x1234));
  auto *outside = new Runtime;
  outside->owner = GetCurrentThreadId();
  outside->original = &dummy_factory;
  outside->identity = &profile::images[0];
  runtime = outside;
  original_factory_calls = 0;
  a2fo::native11_capture::exact_image_calls = 0;
  need(factory_hook(8) == sentinel, "outside loader phase returns legacy on SHA refusal");
  need(outside->state.load() == 3 && original_factory_calls == 1 &&
       a2fo::native11_capture::exact_image_calls == 1 && outside->phase_event &&
       WaitForSingleObject(outside->phase_event, 1000) == WAIT_OBJECT_0,
       "outside loader phase passed probe before exact SHA refusal");
  auto *inside = new Runtime;
  inside->owner = GetCurrentThreadId();
  inside->original = &dummy_factory;
  inside->identity = &profile::images[0];
  runtime = inside;
  original_factory_calls = 0;
  a2fo::native11_capture::exact_image_calls = 0;
  inside_called = false;
  auto image = LoadLibraryA(dll);
  need(image != nullptr && inside_called, "owned DLL invoked callback in PROCESS_ATTACH");
  need(inside_result == sentinel && inside_original_calls == 1 &&
       inside_hashes == 0 && inside->state.load() == 3 &&
       !inside->modes.load() && inside->display == nullptr,
       "DllMain factory call fell back before SHA or helper load");
  need(inside->phase_event &&
       WaitForSingleObject(inside->phase_event, 1000) == WAIT_OBJECT_0,
       "late phase worker completed after DLL attach");
  need(factory_hook(8) == sentinel && original_factory_calls == 2 &&
       a2fo::native11_capture::exact_image_calls == 0,
       "failed loader phase remains legacy only");
  std::puts("PASS: loader phase outside and DLL_PROCESS_ATTACH");
  runtime = nullptr;
}
static void slot_case(unsigned fail_at, bool conflict) {
  auto *page = static_cast<unsigned char *>(VirtualAlloc(nullptr, 4096,
      MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
  need(page != nullptr, "allocate owned slot page");
  auto **slot = reinterpret_cast<void **>(page);
  auto original = reinterpret_cast<void *>(&dummy_factory);
  auto other = reinterpret_cast<void *>(static_cast<std::uintptr_t>(1));
  *slot = conflict ? other : original;
  DWORD old = 0;
  need(::VirtualProtect(page, 4096, PAGE_READONLY, &old) != FALSE,
       "protect owned IAT page RO");
  Runtime state{};
  state.owner = GetCurrentThreadId();
  state.original = &dummy_factory;
  runtime = &state;
  faults(fail_at);
  const bool ok = patch_factory_slot(slot, &dummy_factory);
  need(ok == (!fail_at && !conflict), "slot transaction outcome");
  need(*slot == (ok ? reinterpret_cast<void *>(&factory_hook) :
                      (conflict ? other : original)), "slot value after transaction");
  need(state.state.load() == (ok ? 0u : 3u), "failed slot is inert");
  MEMORY_BASIC_INFORMATION info{};
  need(VirtualQuery(page, &info, sizeof(info)) != 0, "query slot protection");
  need(info.Protect == PAGE_READONLY, "slot page protection restored");
  runtime = nullptr;
  VirtualFree(page, 0, MEM_RELEASE);
}
int main(int argc, char **argv) {
  need(argc == 2, "requires owned phase DLL argument");
  need(sizeof(void *) == 4, "requires i686 fixture");
  workspace_sources();
  fleet_constructor_guards();
  modes_success();
  mode_failures();
  slot_case(0, false);
  slot_case(1, false);
  slot_case(0, true);
  slot_case(2, false);
  std::puts("PASS: factory slot transactions");
  factory_thread_states();
  loader_phase_cases(argv[1]);
}
