// Production Rust DLL acceptance through an injected memory-only host.
// Never creates windows, reads input, draws on the desktop, or opens an audio
// device.
#include "../rust/media_ffi.h"
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
const A2FO_MediaApiV1 *api;
struct Host {
  std::mutex mutex;
  std::atomic<unsigned> frames{0};
  ULONGLONG start = GetTickCount64(), audio_start = 0;
  std::uint64_t submitted = 0, total = 0, played = 0;
  unsigned opened = 0, closed = 0, resets = 0, mode = 0, id = 0, width = 128,
           height = 96;
  std::uint64_t pcm_hash = 1469598103934665603ull;
  std::atomic<bool> blocked{false}, release{false};
  std::atomic<bool> triggered{false};
  bool live = false;
  A2FO_MediaHostV1 table{};
  Host(unsigned kind) : mode(kind) {
    table.size = sizeof(table);
    table.version = 1;
    table.context = this;
    table.now_us = +[](void *p) -> int64_t {
      auto &h = *static_cast<Host *>(p);
      return static_cast<int64_t>((GetTickCount64() - h.start) * 1000 *
                                  (h.mode == 17 ? 1000 : 1));
    };
    table.sleep_ms = +[](void *p, uint32_t ms) {
      Sleep(static_cast<Host *>(p)->mode == 17 ? 0 : ms);
    };
    table.poll = +[](void *p, uint32_t, A2FO_MediaPoll *out) -> uint32_t {
      auto &h = *static_cast<Host *>(p);
      const auto elapsed = GetTickCount64() - h.start;
      *out = {h.width, h.height, 0, 1, 1, 0};
      if (h.mode == 16 && h.frames.load()) {
        h.blocked = true;
        while (!h.release.load())
          Sleep(1);
      }
      if (h.mode == 1 && elapsed > 280)
        out->keys = 1; // Fresh skip after grace/release.
      if (h.mode == 2 && elapsed > 40)
        out->closing = 1;
      if (h.mode == 3 && elapsed > 40)
        return 0;
      if (h.mode == 4 && elapsed > 40)
        out->valid = 0;
      if (h.frames.load() >= 3 && !h.triggered.exchange(true)) {
        if (h.mode == 5 || h.mode == 18)
          assert(api->seek(h.id, h.mode == 18 ? 1000000000 : 200000) == 1);
        if (h.mode == 6) {
          assert(api->shutdown() == 0);
          assert(api->startup() == 0);
        }
        if (h.mode == 7)
          api->close(h.id);
        if (h.mode == 8) {
          A2FO_MediaPlay play{sizeof(play), 1, 0, &h.table};
          assert(api->play(h.id, &play) == 3);
        }
      }
      return 1;
    };
    table.present = +[](void *p, const uint8_t *data, uint32_t w, uint32_t h,
                        uint32_t stride) -> uint32_t {
      auto &host = *static_cast<Host *>(p);
      assert(data && w == host.width && h == host.height && stride == w * 4);
      if (host.mode != 17 && host.mode != 19)
        for (unsigned x = 0; x < w * h; ++x)
          assert(data[x * 4 + 3] == 255);
      ++host.frames;
      return host.mode == 9 ? 0 : 1;
    };
    table.audio_open = +[](void *p) -> uint32_t {
      auto &h = *static_cast<Host *>(p);
      std::lock_guard<std::mutex> lock(h.mutex);
      assert(!h.live);
      if (h.mode == 10)
        return 0;
      h.live = true;
      ++h.opened;
      h.submitted = h.played = 0;
      h.audio_start = 0;
      return 1;
    };
    table.audio_write =
        +[](void *p, const int16_t *pcm, uint32_t frames) -> uint32_t {
      auto &h = *static_cast<Host *>(p);
      std::lock_guard<std::mutex> lock(h.mutex);
      assert(h.live && pcm && frames <= 1024);
      if (h.mode == 11)
        return 2;
      h.update();
      if (h.submitted - h.played >= 6144)
        return 0;
      if (!h.audio_start)
        h.audio_start = GetTickCount64();
      h.submitted += frames;
      h.total += frames;
      for (unsigned i = 0; i < frames * 2; ++i) {
        h.pcm_hash ^= static_cast<uint16_t>(pcm[i]);
        h.pcm_hash *= 1099511628211ull;
      }
      return 1;
    };
    table.audio_position = +[](void *p) -> uint64_t {
      auto &h = *static_cast<Host *>(p);
      std::lock_guard<std::mutex> lock(h.mutex);
      h.update();
      return h.played;
    };
    table.audio_pending = +[](void *p) -> uint32_t {
      auto &h = *static_cast<Host *>(p);
      std::lock_guard<std::mutex> lock(h.mutex);
      if (h.mode == 12)
        return 2;
      h.update();
      return h.played < h.submitted ? 1 : 0;
    };
    table.audio_reset = +[](void *p) {
      auto &h = *static_cast<Host *>(p);
      std::lock_guard<std::mutex> lock(h.mutex);
      assert(h.live);
      ++h.resets;
      h.played = h.submitted;
    };
    table.audio_close = +[](void *p) {
      auto &h = *static_cast<Host *>(p);
      std::lock_guard<std::mutex> lock(h.mutex);
      assert(h.live);
      h.live = false;
      ++h.closed;
    };
  }
  void update() {
    if (mode == 17) {
      played = submitted;
      return;
    }
    if (audio_start)
      played = std::max(
          played,
          std::min(submitted, static_cast<uint64_t>(
                                  (GetTickCount64() - audio_start) * 48)));
  }
};
void log_message(void *, const char *message) {
  std::printf("  %s\n", message);
  std::fflush(stdout);
}
std::uint32_t open(const char *filename) {
  const int n = MultiByteToWideChar(CP_UTF8, 0, filename, -1, nullptr, 0);
  assert(n > 0);
  std::vector<wchar_t> wide(n);
  assert(MultiByteToWideChar(CP_UTF8, 0, filename, -1, wide.data(), n));
  A2FO_MediaOpen request{sizeof(request),
                         1,
                         reinterpret_cast<const uint16_t *>(wide.data()),
                         static_cast<uint32_t>(n - 1),
                         4,
                         log_message,
                         nullptr};
  return api->open(&request);
}
void run(const char *filename, unsigned mode, unsigned expected_frames = 0,
         unsigned expected_pcm = 0) {
  assert(api->startup() == 1);
  Host host(mode);
  if (mode == 17 || mode == 19) {
    host.width = mode == 19 ? 1920 : 1280;
    host.height = mode == 19 ? 1080 : 720;
  }
  host.id = open(filename);
  assert(host.id);
  A2FO_MediaInfo info{};
  info.size = sizeof(info);
  assert(api->info(host.id, &info));
  A2FO_MediaPlay play{sizeof(play), 1, 0, &host.table};
  std::thread other;
  if (mode == 13 || mode == 14 || mode == 15)
    other = std::thread([&] {
      while (!host.frames.load())
        Sleep(1);
      if (mode == 13)
        api->cancel(host.id);
      else if (mode == 14)
        assert(api->shutdown() == 1);
      else {
        api->close(host.id);
        assert(api->shutdown() == 1);
      }
    });
  if (mode == 16)
    other = std::thread([&] {
      while (!host.blocked.load())
        Sleep(1);
      std::atomic<bool> closing{false}, closed{false}, stopping{false},
          stopped{false};
      std::thread closer([&] {
        closing = true;
        api->close(host.id);
        closed = true;
      });
      while (!closing.load()) {
        Sleep(1);
      }
      Sleep(15);
      std::thread stopper([&] {
        stopping = true;
        assert(api->shutdown() == 1);
        stopped = true;
      });
      while (!stopping.load()) {
        Sleep(1);
      }
      Sleep(15);
      assert(!closed.load() && !stopped.load());
      assert(!api->startup());
      host.release = true;
      closer.join();
      stopper.join();
    });
  const auto result = api->play(host.id, &play);
  if (other.joinable())
    other.join();
  if (mode == 0 || mode == 5 || mode == 8 || mode == 17 || mode == 18 ||
      mode == 19)
    assert(result == 1);
  else if (mode == 3 || mode == 9 || mode == 10 || mode == 11 || mode == 12)
    assert(result == 0);
  else
    assert(result == 2);
  assert(!host.live && host.opened == host.closed);
  if (expected_frames)
    assert(host.frames == expected_frames);
  if (expected_pcm)
    assert(host.total == expected_pcm);
  if (mode == 5) {
    assert(host.opened == 2 && host.closed == 2);
    assert(host.frames < 9);
  }
  if (mode == 0)
    assert(GetTickCount64() - host.start >= 300);
  api->close(host.id);
  assert(!api->info(host.id, &info));
  assert(!api->seek(host.id, 0));
  assert(api->shutdown() == 1);
  std::printf("PASS mode=%u frames=%u pcm=%llu opens=%u closes=%u elapsed=%llu "
              "pcm_hash=%llu\n",
              mode, host.frames.load(),
              static_cast<unsigned long long>(host.total), host.opened,
              host.closed,
              static_cast<unsigned long long>(GetTickCount64() - host.start),
              static_cast<unsigned long long>(host.pcm_hash));
}
void abi_tests(const char *filename) {
  assert(api->startup());
  assert(!api->open(nullptr));
  uint32_t tiny[2] = {8, 1};
  assert(!api->open(reinterpret_cast<const A2FO_MediaOpen *>(tiny)));
  Host host(0);
  host.id = open(filename);
  assert(host.id);
  assert(!api->info(host.id, reinterpret_cast<A2FO_MediaInfo *>(tiny)));
  assert(!api->play(host.id, reinterpret_cast<const A2FO_MediaPlay *>(tiny)));
  A2FO_MediaPlay play{sizeof(play), 1, 0,
                      reinterpret_cast<const A2FO_MediaHostV1 *>(tiny)};
  assert(!api->play(host.id, &play));
  play.host = &host.table;
  assert(api->seek(host.id, 100000));
  assert(api->play(host.id, &play) == 1);
  assert(host.frames == 6 && host.total == 11998);
  assert(!api->seek(host.id, 0));
  assert(!host.live && host.opened == host.closed);
  api->close(host.id);
  Host at_end(0);
  at_end.id = open(filename);
  assert(api->seek(at_end.id, 1000000000));
  play.host = &at_end.table;
  assert(api->play(at_end.id, &play) == 1);
  assert(at_end.frames == 0 && at_end.opened == 0);
  api->close(at_end.id);
  assert(api->shutdown());
  std::puts("PASS ABI short tables, retry, pre-play seek, and terminal seek "
            "rejection");
}
int main(int argc, char **argv) {
  assert(argc >= 3);
  HMODULE dll = LoadLibraryA(argv[1]);
  if (!dll) {
    std::printf("LoadLibrary error=%lu\n", GetLastError());
    return 2;
  }
  const FARPROC symbol = GetProcAddress(dll, "A2FOMedia_GetApiV1");
  A2FO_MediaGetApi entry = nullptr;
  std::memcpy(&entry, &symbol, sizeof(entry));
  assert(entry && entry(2) == nullptr);
  api = entry(1);
  assert(api && api->version == 1 && api->size == sizeof(*api));
  if (argc == 4 && (std::strcmp(argv[2], "--bench") == 0 ||
                    std::strcmp(argv[2], "--paced") == 0)) {
    run(argv[3], std::strcmp(argv[2], "--paced") == 0 ? 19 : 17);
    FreeLibrary(dll);
    return 0;
  }
  const std::string root = argv[2];
  const auto path = [&](const char *name) { return root + "/" + name; };
  abi_tests(path("aac.mp4").c_str());
  run(path("aac.mp4").c_str(), 0, 9, 16798);
  run(path("opus.webm").c_str(), 0, 9, 16800);
  run(path("vorbis.webm").c_str(), 0, 9, 16801);
  run(path("silent.mp4").c_str(), 0, 9);
  run(path("aac.mp4").c_str(), 18);
  for (unsigned mode = 1; mode <= 16; ++mode)
    run(path("aac.mp4").c_str(), mode);
  assert(api->startup());
  assert(!open("missing.mp4"));
  assert(api->shutdown());
  FreeLibrary(dll);
  std::puts(
      "Rust DLL media acceptance passed (23 playback cases, memory host only)");
}
