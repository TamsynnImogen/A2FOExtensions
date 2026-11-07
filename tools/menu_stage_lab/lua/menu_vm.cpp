#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace {
constexpr size_t kMemoryLimit = 1024 * 1024;
constexpr size_t kScriptLimit = 16 * 1024;
constexpr int kInstructionLimit = 10000;
constexpr size_t kTimerLimit = 32;
constexpr size_t kEventLimit = 64;

struct Vm;
struct Timer {
  Vm* vm;
  std::string id;
  double interval;
  double started_at;
  bool started;
  bool repeat = false;
  int function_ref = LUA_NOREF;
  int self_ref = LUA_NOREF;
  std::string hook_id;
};
struct Event { double time; std::string command, view; double duration; };
struct Vm {
  lua_State* state = nullptr;
  size_t allocated = 0;
  int instructions = 0;
  double now = 0;
  std::string view;
  std::string error;
  std::vector<Event> events;
  std::vector<std::unique_ptr<Timer>> timers;
  ~Vm() {
    if (state) {
      for (auto& timer : timers) {
        luaL_unref(state, LUA_REGISTRYINDEX, timer->function_ref);
        luaL_unref(state, LUA_REGISTRYINDEX, timer->self_ref);
      }
      lua_close(state);
    }
  }
};

void* bounded_alloc(void* ud, void* ptr, size_t old_size, size_t new_size) {
  auto& vm = *static_cast<Vm*>(ud);
  if (!ptr) old_size = 0;  // Lua 5.1 supplies a type tag as osize for new objects.
  if (new_size == 0) {
    std::free(ptr);
    vm.allocated -= old_size;
    return nullptr;
  }
  if (new_size > old_size && new_size - old_size > kMemoryLimit - vm.allocated)
    return nullptr;
  void* next = std::realloc(ptr, new_size);
  if (next) vm.allocated = vm.allocated - old_size + new_size;
  return next;
}
void budget_hook(lua_State* state, lua_Debug*) {
  lua_getfield(state, LUA_REGISTRYINDEX, "menu.vm");
  auto* vm = static_cast<Vm*>(lua_touserdata(state, -1));
  lua_pop(state, 1);
  vm->instructions += 100;
  if (vm->instructions > kInstructionLimit) luaL_error(state, "instruction budget exceeded");
}
Vm* context(lua_State* state) { return static_cast<Vm*>(lua_touserdata(state, lua_upvalueindex(1))); }
Timer* timer_arg(lua_State* state) {
  return *static_cast<Timer**>(luaL_checkudata(state, 1, "menu.timer"));
}
int timer_new(lua_State* state) {
  Vm* vm = context(state);
  lua_remove(state, 1);  // __call receives the Timer table before user arguments.
  const char* id = luaL_checkstring(state, 1);
  double interval = luaL_checknumber(state, 2);
  int mode = luaL_checkinteger(state, 3);
  if (!*id || std::strlen(id) > 64 || !std::isfinite(interval) || interval <= 0 || interval > 3600 ||
      (mode != 0 && mode != 1) || vm->timers.size() >= kTimerLimit)
    return luaL_error(state, "invalid timer or timer limit exceeded");
  auto timer = std::make_unique<Timer>();
  timer->vm = vm; timer->id = id; timer->interval = interval;
  timer->started_at = vm->now; timer->started = mode == 1;
  Timer* raw = timer.get();
  vm->timers.push_back(std::move(timer));
  *static_cast<Timer**>(lua_newuserdata(state, sizeof(Timer*))) = raw;
  luaL_getmetatable(state, "menu.timer"); lua_setmetatable(state, -2);
  lua_pushvalue(state, -1);
  raw->self_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  return 1;
}
int timer_hook(lua_State* state) {
  Timer* timer = timer_arg(state);
  const char* id = luaL_checkstring(state, 2);
  if (!lua_isnil(state, 3) || !lua_isfunction(state, 4) || !lua_isnil(state, 5))
    return luaL_error(state, "hook requires (id, nil, function, nil)");
  if (!*id || std::strlen(id) > 64) return luaL_error(state, "invalid hook id");
  if (!timer->hook_id.empty()) { lua_pushboolean(state, 0); return 1; }
  timer->hook_id = id;
  lua_pushvalue(state, 4);
  timer->function_ref = luaL_ref(state, LUA_REGISTRYINDEX);
  lua_pushboolean(state, 1); return 1;
}
int timer_start(lua_State* state) {
  Timer* timer = timer_arg(state);
  timer->started = true; timer->started_at = timer->vm->now;
  return 0;
}
int timer_stop(lua_State* state) { timer_arg(state)->started = false; return 0; }
int timer_repeat(lua_State* state) {
  timer_arg(state)->repeat = lua_toboolean(state, 2) != 0;
  return 0;
}
int camera(lua_State* state) {
  Vm* vm = context(state);
  const char* name = luaL_checkstring(state, 1);
  bool pan = lua_toboolean(state, lua_upvalueindex(2)) != 0;
  double duration = pan ? luaL_checknumber(state, 2) : 0;
  if (std::strcmp(name, "opening") && std::strcmp(name, "hangar"))
    return luaL_error(state, "unknown view: %s", name);
  if (!std::isfinite(duration) || duration < 0 || duration > 60 || vm->events.size() >= kEventLimit)
    return luaL_error(state, "invalid transition or event limit exceeded");
  vm->view = name;
  vm->events.push_back({vm->now, pan ? "panTo" : "gotoView", name, duration});
  return 0;
}
void add_method(lua_State* state, const char* name, lua_CFunction fn) {
  lua_pushcfunction(state, fn); lua_setfield(state, -2, name);
}
void open_library(lua_State* state, const char* name, lua_CFunction fn) {
  lua_pushcfunction(state, fn); lua_pushstring(state, name); lua_call(state, 1, 0);
}
void configure(Vm& vm) {
  lua_State* state = vm.state;
  lua_pushlightuserdata(state, &vm);
  lua_setfield(state, LUA_REGISTRYINDEX, "menu.vm");
  open_library(state, "", luaopen_base);
  open_library(state, LUA_TABLIBNAME, luaopen_table);
  open_library(state, LUA_STRLIBNAME, luaopen_string);
  open_library(state, LUA_MATHLIBNAME, luaopen_math);
  const char* disabled[] = {"dofile", "loadfile", "load", "loadstring", "require", "module", "collectgarbage", "newproxy", "getfenv", "setfenv", "pcall", "xpcall"};
  for (const char* key : disabled) { lua_pushnil(state); lua_setglobal(state, key); }
  lua_getglobal(state, "string"); lua_pushnil(state); lua_setfield(state, -2, "dump"); lua_pop(state, 1);
  luaL_newmetatable(state, "menu.timer");
  lua_newtable(state);
  add_method(state, "hook", timer_hook); add_method(state, "start", timer_start);
  add_method(state, "stop", timer_stop); add_method(state, "setRepeat", timer_repeat);
  lua_setfield(state, -2, "__index"); lua_pop(state, 1);
  lua_newtable(state); lua_newtable(state);
  lua_pushinteger(state, 0); lua_setfield(state, -2, "Stopped");
  lua_pushinteger(state, 1); lua_setfield(state, -2, "Started");
  lua_setfield(state, -2, "State");
  lua_newtable(state);
  lua_pushlightuserdata(state, &vm); lua_pushcclosure(state, timer_new, 1);
  lua_setfield(state, -2, "__call");
  lua_setmetatable(state, -2); lua_setglobal(state, "Timer");
  lua_newtable(state); lua_newtable(state);
  for (int pan = 0; pan <= 1; ++pan) {
    lua_pushlightuserdata(state, &vm); lua_pushboolean(state, pan);
    lua_pushcclosure(state, camera, 2);
    lua_setfield(state, -2, pan ? "panTo" : "gotoView");
  }
  lua_setfield(state, -2, "Camera"); lua_setglobal(state, "Menu");
  lua_sethook(state, budget_hook, LUA_MASKCOUNT, 100);
}
bool protected_call(Vm& vm, int arguments) {
  vm.instructions = 0;
  size_t old_count = vm.events.size();
  std::string old_view = vm.view;
  if (lua_pcall(vm.state, arguments, 0, 0) == 0) return true;
  const char* message = lua_tostring(vm.state, -1);
  vm.error = message ? message : "Lua error (no message)";
  lua_pop(vm.state, 1);
  vm.events.resize(old_count); vm.view = old_view;
  return false;
}
bool run(Vm& vm, const std::string& script, const std::vector<double>& times) {
  if (script.size() > kScriptLimit || (!script.empty() && static_cast<unsigned char>(script[0]) == 0x1b)) {
    vm.error = "script size limit or bytecode rejected"; return false;
  }
  if (luaL_loadbuffer(vm.state, script.data(), script.size(), "menu.lua") != 0) {
    vm.error = lua_tostring(vm.state, -1); lua_pop(vm.state, 1); return false;
  }
  if (!protected_call(vm, 0)) return false;
  for (double now : times) {
    if (!std::isfinite(now) || now < vm.now) { vm.error = "clock must be finite and monotonic"; return false; }
    vm.now = now;
    size_t count = vm.timers.size();
    for (size_t i = 0; i < count; ++i) {
      Timer& timer = *vm.timers[i];
      if (!timer.started || now - timer.started_at < timer.interval) continue;
      if (timer.function_ref != LUA_NOREF) {
        lua_rawgeti(vm.state, LUA_REGISTRYINDEX, timer.function_ref);
        lua_rawgeti(vm.state, LUA_REGISTRYINDEX, timer.self_ref);
        lua_pushstring(vm.state, timer.hook_id.c_str());
        if (!protected_call(vm, 2)) return false;
      }
      if (timer.repeat && timer.started) timer.started_at = now;
      else timer.started = false;
    }
  }
  return true;
}
void json_string(const std::string& value) {
  std::putchar('"');
  for (unsigned char c : value) {
    if (c == '"' || c == '\\') { std::putchar('\\'); std::putchar(c); }
    else if (c >= 32 && c < 127) std::putchar(c);
    else std::printf("\\u%04x", c);
  }
  std::putchar('"');
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) { std::fprintf(stderr, "usage: menu_vm script.lua [tick_seconds ...]\n"); return 2; }
  std::ifstream input(argv[1], std::ios::binary);
  if (!input) { std::fprintf(stderr, "cannot open script\n"); return 2; }
  input.seekg(0, std::ios::end);
  if (input.tellg() < 0 || input.tellg() > static_cast<std::streamoff>(kScriptLimit)) {
    std::puts("{\"ok\":false,\"lua\":\"Lua 5.1.5\",\"view\":\"\",\"error\":\"script size limit exceeded\",\"events\":[]}");
    return 1;
  }
  input.seekg(0);
  std::string script((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
  std::vector<double> ticks;
  for (int i = 2; i < argc; ++i) {
    char* end = nullptr; double time = std::strtod(argv[i], &end);
    if (!end || *end) { std::fprintf(stderr, "invalid tick\n"); return 2; }
    ticks.push_back(time);
  }
  Vm vm;
  vm.state = lua_newstate(bounded_alloc, &vm);
  if (!vm.state) { std::fprintf(stderr, "cannot create bounded Lua VM\n"); return 2; }
  configure(vm);
  bool ok = run(vm, script, ticks);
  std::printf("{\"ok\":%s,\"lua\":", ok ? "true" : "false"); json_string(LUA_RELEASE);
  std::printf(",\"view\":"); json_string(vm.view);
  std::printf(",\"error\":"); json_string(vm.error);
  std::printf(",\"events\":[");
  for (size_t i = 0; i < vm.events.size(); ++i) {
    if (i) std::putchar(',');
    const auto& event = vm.events[i];
    std::printf("{\"time\":%.6g,\"command\":", event.time); json_string(event.command);
    std::printf(",\"view\":"); json_string(event.view);
    std::printf(",\"duration\":%.6g}", event.duration);
  }
  std::printf("]}\n");
  return ok ? 0 : 1;
}
