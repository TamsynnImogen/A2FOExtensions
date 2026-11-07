# Menu stage research lab

Read [the assessment](../../docs/menu-bzn-lua-assessment-2026-10-19.md) first.
These tools validate independent components. They do not implement a native BZN
loader, map writer, or a replacement production menu runtime. No installed DLL,
map or render setting is changed by these commands.

## Legacy placement evidence

```sh
python3 -B -m unittest discover -s tools/menu_stage_lab -p test_scene_plan.py -v
python3 tools/menu_stage_lab/scene_plan.py \
  --map-ini '/home/tamsynn/NVMeData/Fleet Ops Roots/Data/RefitScene/Menu/map.ini' \
  --output build/menu-stage-lab/conversion
```

Outputs are a JSON research plan and actor/camera CSVs. Nebula expansion,
native editor bounds, and BZN coordinate/serialization contracts remain unverified.
The current fixture's transforms were independently checked against the renderer.
Numeric parsing is deliberately stricter about malformed/nonfinite input but
accepts exponent notation more broadly than the legacy renderer; this is not
a general drop-in implementation of its permissive INI parser.

## Engine-free Lua timer probe

```sh
python3 tools/menu_stage_lab/lua/build_and_test.py
build/menu-stage-lab/lua/menu_vm tools/menu_stage_lab/lua/menu.lua 0 3.99 4 8
```

See [the supported subset and notices](lua/README.md). The two view names are
fixture data. Camera operations record events; they do not move a renderer camera.
The interpreter source is read from the existing vendored Lua tree, unchanged.

## Source-isolated renderer profiling

```sh
python3 tools/menu_stage_lab/profile_renderer.py --prepare
python3 tools/menu_stage_lab/profile_renderer.py --build
```

Preparation refuses to replace an existing copy. Output is restricted to
`build/menu-stage-lab/profile/`; the profiler never deploys its candidate DLL.
Keep that directory/provenance for repeat runs. Timers are inclusive wall-clock elapsed
scopes on the graphics owner thread, not CPU consumption or GPU timestamps. Their totals overlap. BZN, Lua, and M3D phases do not
exist in this candidate.

The lead staged a private `profile/host` with the installed recipe/profile,
DXVK and candidate, then ran the existing private **headless** fixture:

```sh
python3 tools/run_refit_scene_worker_test.py --fixture scene \
  --prefix /home/tamsynn/.cache/a2fo-refit-native-prefix-20261018 \
  --host-root build/menu-stage-lab/profile/host \
  --assets '/home/tamsynn/NVMeData/Fleet Ops Roots/Data' \
  --log build/menu-stage-lab/profile/gpu.log
```

This fixture uses a private headless compositor, hidden nonactivating windows
and no input automation. Do not run overlapping tests on its Wine prefix. It
does not launch ArmadaL or establish in-game acceptance. Inspect both the runner
log and `profile/host/RefitScene/A2FORefitScene.log`, including missing assets.

## Native BZN acceptance gate

Before implementing a production stage loader, obtain a normal editor-authored
calibration map with asymmetric actor positions/headings, two named camera/target
pairs, one planet, one nebula, ownership colours, and environment/light settings.
Preserve the original and a separately saved edited copy. Change one property
per save to identify the actual record fields, then validate the full orientation
basis, label identity, unknown-class handling, map bounds and file versions.

Next build a bounds-checked read-only projection, prove marker-derived rendering
in the current modern provider, and test missing/malformed bundle fallback. Only
then create a native writer or editor-assisted converted scene and perform a
manual editor save/reopen round trip. Never label these JSON/CSV artifacts `.bzn`.

The future campaign adapter consumes arbitrary campaign IDs and view names.
A2FOAchievements is planned and is not a dependency of these experiments.
