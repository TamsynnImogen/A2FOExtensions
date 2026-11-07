// Direct Win32 backend contract. All executable/mapped addresses are owned by
// this console fixture. No game image, window, or input operation is used.
#include "../rust/native_hooks_ffi.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <windows.h>
namespace {
void check(bool ok, const char *message) {
  if (!ok) {
    std::fprintf(stderr, "FAIL %s\n", message);
    std::exit(1);
  }
}
DWORD protection(void *p) {
  MEMORY_BASIC_INFORMATION info{};
  check(VirtualQuery(p, &info, sizeof(info)) == sizeof(info), "VirtualQuery");
  return info.Protect;
}
using Function = std::uint32_t(__cdecl *)();
std::uint32_t invoke(void *p) {
  Function f = nullptr;
  static_assert(sizeof(f) == sizeof(p));
  std::memcpy(&f, &p, sizeof(f));
  return f();
}
struct Publication {
  void *target;
  void *current = nullptr;
  void *last = nullptr;
  unsigned calls = 0;
  bool execute = false;
  bool freed_before_clear = false;
};
void publish(void *raw, void *gateway) {
  auto &p = *static_cast<Publication *>(raw);
  ++p.calls;
  p.current = gateway;
  if (gateway) {
    p.last = gateway;
    if (p.execute) {
      check(*static_cast<std::uint8_t *>(p.target) == 0xb8,
            "publish precedes target patch");
      check(invoke(gateway) == 37,
            "published gateway already flushed/executable");
    }
  } else {
    MEMORY_BASIC_INFORMATION info{};
    check(VirtualQuery(p.last, &info, sizeof(info)) == sizeof(info),
          "freed gateway query");
    p.freed_before_clear = info.State == MEM_FREE;
  }
}
void restore(void *target, const void *source, std::size_t n) {
  A2foRsPatchReport result{};
  check(a2fo_rs_windows_write_report(target, source, n, &result) &&
            result.written && result.restored,
        "restore private code");
}
} // namespace
int main() {
  static_assert(sizeof(void *) == 4 && sizeof(A2foRsPatchReport) == 12 &&
                sizeof(A2foRsVideoInstall) == 8);
  SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
               SEM_NOOPENFILEERRORBOX);
  auto *page = static_cast<std::uint8_t *>(
      VirtualAlloc(nullptr, 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  check(page, "own executable page");
  const std::uint8_t original[] = {0xb8, 37, 0, 0, 0, 0xc3};
  const std::uint8_t changed[] = {0xb8, 55, 0, 0, 0, 0xc3};
  const std::uint8_t replacement[] = {0xb8, 99, 0, 0, 0, 0xc3};
  const std::uint8_t video_original[] = {0xb8, 37, 0, 0, 0, 0x90, 0xc3};
  std::memcpy(page, original, 6);
  std::memcpy(page + 32, original, 6);
  std::memcpy(page + 64, original, 6);
  std::memcpy(page + 128, replacement, 6);
  std::memcpy(page + 192, video_original, 7);
  DWORD old = 0;
  check(VirtualProtect(page, 4096, PAGE_EXECUTE_READ, &old), "seal code page");
  check(FlushInstructionCache(GetCurrentProcess(), page, 4096),
        "initial cache flush");
  A2foRsPatchReport report{};
  check(a2fo_rs_windows_write_report(page, changed, 6, &report) &&
            report.written == 1 && report.restored == 1 &&
            report.restore_error == 0,
        "raw write report");
  check(invoke(page) == 55 && protection(page) == PAGE_EXECUTE_READ,
        "raw code and protection");
  check(a2fo_rs_windows_jump_report(page + 32, page + 128, &report) &&
            report.written && report.restored,
        "jump report");
  check(invoke(page + 32) == 99 && protection(page + 32) == PAGE_EXECUTE_READ,
        "jump execution");
  check(a2fo_rs_windows_patch_double(page + 64, changed, original, 6),
        "double-check byte patch");
  check(invoke(page + 64) == 55 && protection(page + 64) == PAGE_EXECUTE_READ,
        "double-check execution");
  check(!a2fo_rs_windows_patch_double(page + 64, original, original, 6) &&
            invoke(page + 64) == 55,
        "double-check mismatch inert");
  std::puts("windows:raw-write,jump,double-check,protection:PASS");

  Publication publication{page + 192};
  publication.execute = true;
  A2foRsVideoInstall installed{};
  check(a2fo_rs_windows_video_install(page + 192, page + 128, 6, video_original,
                                      &installed, &publication, publish),
        "video trampoline install");
  check(installed.status == 0 && installed.gateway == publication.current &&
            publication.calls == 1,
        "video publication/status");
  check(invoke(page + 192) == 99 && invoke(installed.gateway) == 37 &&
            protection(page + 192) == PAGE_EXECUTE_READ,
        "video target and gateway execution");
  restore(page + 192, video_original, 7);
  check(VirtualFree(installed.gateway, 0, MEM_RELEASE),
        "release restored gateway");
  std::puts("windows:video-publish-before-patch,target=99,gateway=37:PASS");

  HANDLE mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr,
                                      PAGE_READONLY, 0, 4096, nullptr);
  check(mapping != nullptr, "owned readonly mapping");
  auto *readonly = static_cast<std::uint8_t *>(
      MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 4096));
  check(readonly, "owned readonly view");
  std::uint8_t zeros[6]{};
  Publication rejected{readonly};
  A2foRsVideoInstall failure{};
  check(!a2fo_rs_windows_video_install(readonly, page + 128, 6, zeros, &failure,
                                       &rejected, publish),
        "readonly target refuses executable write");
  check(failure.status == 3 && failure.gateway == nullptr &&
            rejected.calls == 2 && rejected.current == nullptr &&
            rejected.freed_before_clear,
        "failed patch frees before clearing publication");
  check(std::memcmp(readonly, zeros, 6) == 0, "failed target unchanged");
  check(!a2fo_rs_windows_write_report(readonly, changed, 6, &report) &&
            !report.written,
        "raw readonly write refused");
  check(UnmapViewOfFile(readonly) && CloseHandle(mapping), "release mapping");
  check(VirtualFree(page, 0, MEM_RELEASE), "release code page");
  std::puts("windows:video-patch-failure,free-before-clear,no-write:PASS");
}
