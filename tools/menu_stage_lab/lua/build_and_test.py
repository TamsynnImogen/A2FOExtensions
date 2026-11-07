#!/usr/bin/env python3
"""Build the repository's Lua 5.1.5 C source and run a headless menu VM probe."""
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
LUA = ROOT / "third_party/mmm/source/external/lua/src"
OUT = ROOT / "build/menu-stage-lab/lua"
SOURCES = "lapi lauxlib lbaselib lcode ldebug ldo ldump lfunc lgc llex lmem lobject lopcodes lparser lstate lstring ltable ltm lundump lvm lzio lmathlib lstrlib ltablib".split()


def command(args):
    subprocess.run(args, check=True)


def execute(script, ticks=()):
    with tempfile.TemporaryDirectory(dir=OUT) as directory:
        path = Path(directory) / "menu.lua"
        path.write_bytes(script if isinstance(script, bytes) else script.encode())
        result = subprocess.run([str(OUT / "menu_vm"), str(path), *map(str, ticks)],
                                capture_output=True, text=True, check=False, timeout=2)
        if not result.stdout:
            raise AssertionError(result.stderr)
        return result.returncode, json.loads(result.stdout)


def check(name, script, ticks, expected_ok, expected_views, error_fragment=""):
    code, result = execute(script, ticks)
    assert (code == 0) == expected_ok, (name, code, result)
    assert result["ok"] == expected_ok, (name, result)
    assert [entry["view"] for entry in result["events"]] == expected_views, (name, result)
    assert error_fragment in result["error"], (name, result)
    print(json.dumps({"test": name, **result}, separators=(",", ":")))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    assert "Lua 5.1.5" in (LUA / "lua.h").read_text()
    objects = []
    for name in SOURCES:
        output = OUT / (name + ".o")
        command(["cc", "-std=c99", "-O2", "-I", str(LUA), "-c", str(LUA / (name + ".c")), "-o", str(output)])
        objects.append(str(output))
    command(["c++", "-std=c++17", "-O2", "-Wall", "-Wextra", "-I", str(LUA),
             str(HERE / "menu_vm.cpp"), *objects, "-lm", "-o", str(OUT / "menu_vm")])
    print("built:", OUT / "menu_vm", "from", LUA)
    check("example", (HERE / "menu.lua").read_bytes(), [0, 3.99, 4, 8], True, ["opening", "hangar"])
    check("order-stop-repeat", '''
Menu.Camera.gotoView("opening")
local first = Timer("first", 1, Timer.State.Started)
first:setRepeat(true)
first:hook("first", nil, function(t, h) Menu.Camera.panTo("hangar", 1); t:stop() end, nil)
local second = Timer("second", 1, Timer.State.Stopped)
second:hook("second", nil, function(t, h) Menu.Camera.gotoView("opening") end, nil)
second:start()
''', [1, 2], True, ["opening", "hangar", "opening"])
    check("repeat", '''
local t = Timer("repeat", 1, Timer.State.Started)
t:setRepeat(true)
t:hook("repeat", nil, function(self, hook) Menu.Camera.gotoView("opening") end, nil)
''', [1, 2, 3], True, ["opening", "opening", "opening"])
    check("callback-rollback", '''
Menu.Camera.gotoView("opening")
local t = Timer("error", 1, Timer.State.Started)
t:hook("error", nil, function(self, hook)
  Menu.Camera.panTo("hangar", 2)
  error("callback failed")
end, nil)
''', [1], False, ["opening"], "callback failed")
    check("unknown-view", 'Menu.Camera.gotoView("opening"); Menu.Camera.gotoView("missing")',
          [], False, [], "unknown view")
    check("loop-budget", 'Menu.Camera.gotoView("opening"); while true do end',
          [], False, [], "instruction budget exceeded")
    check("disallowed-globals", '''
assert(io == nil and os == nil and package == nil and debug == nil)
assert(load == nil and loadstring == nil and dofile == nil and loadfile == nil)
assert(string.dump == nil and require == nil and pcall == nil and xpcall == nil)
Menu.Camera.gotoView("opening")
''', [], True, ["opening"])
    check("pcall-budget-bypass", 'while true do pcall(function() while true do end end) end',
          [], False, [], "attempt to call global 'pcall'")
    check("xpcall-budget-bypass", 'while true do xpcall(function() while true do end end, function() end) end',
          [], False, [], "attempt to call global 'xpcall'")
    check("bytecode", b"\x1bLua", [], False, [], "bytecode rejected")
    check("script-limit", b" " * 16385, [], False, [], "script size limit")
    check("memory-limit", 'string.rep("x", 2000000)', [], False, [], "not enough memory")
    check("event-limit", 'for i=1,65 do Menu.Camera.gotoView("opening") end',
          [], False, [], "event limit exceeded")
    check("context-fresh", 'assert(Menu.Camera ~= nil); Menu.Camera.gotoView("hangar")',
          [], True, ["hangar"])
    print("PASS: 14 headless Lua 5.1.5 checks")


if __name__ == "__main__":
    main()
