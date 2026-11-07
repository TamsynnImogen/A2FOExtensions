# Engine-free menu Lua component probe

`python3 tools/menu_stage_lab/lua/build_and_test.py` compiles the repository's
unmodified `third_party/mmm/source/external/lua/src` Lua 5.1.5 C files with
the host C compiler and tests an original, small C++ menu adapter. The Lua
source is independently MIT licensed; the copyright and license notice is in
`third_party/mmm/source/external/lua/src/lua.h`. This lab links that source
read-only and does not copy MMM implementation code.

The supported surface is `Timer(id, seconds, Timer.State.Started|Stopped)`,
`:hook(id, nil, function(timerObject, hook), nil)`, `:start()`, `:stop()`, and
`:setRepeat(boolean)`. This probe supports one hook per timer; its `hook`
argument is the hook ID string. `Menu.Camera.gotoView(name)` and
`Menu.Camera.panTo(name, seconds)` append commands for fixture views `opening`
and `hangar`. This is a menu-specific extension, not an existing MMM export.
The host pumps a monotonic menu clock; timers fire in creation order, at most
once per pump, and repeat from the pump time. There is no game clock or render
device. The probe is not full MMM or a BZN/renderer integration.
As in the inspected MMM `Timer::tick`, a nonrepeating timer is stopped after
its callback, even if that callback calls `:start()`. Callbacks that need a
later tick can set repeat first; this probe makes no broader MMM guarantee.

Only base, table, string, and math libraries are opened. File, process,
package, debug, dynamic loading, script loading and bytecode entry points are
unavailable. Source is limited to 16 KiB; Lua allocation to 1 MiB per context;
each protected Lua call to 10,000 VM instructions; timers to 32 and events to
64. Script/callback errors discard commands from the failed call and retain
the last valid view. Registry references are released before each VM closes.
`pcall` and `xpcall` are removed because catching the hook's instruction-limit
error would permit an endless outer loop. Lua C library calls are not themselves
bounded by the VM instruction hook. Each execution test has a two-second host
timeout. These are component safety bounds, not a production security
certification.

Direct run after building:

`build/menu-stage-lab/lua/menu_vm tools/menu_stage_lab/lua/menu.lua 0 3.99 4 8`

Output is one JSON object with Lua version, current view, error and timed
camera-command events. The tests exercise ordering, stop, repeat, rollback,
unknown views, loop budget, unavailable globals, bytecode and size rejection,
and a fresh context. The complete BZN proof remains gated by validated map
serialization and native editor round trips.
