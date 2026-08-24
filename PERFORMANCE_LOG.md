# Performance Log

Use the same mod, map/save, camera position, resolution, and game speed when
comparing two builds. Record a stable reading after the scene has settled;
loading screens and menus are not comparable gameplay samples.

| Date | Build/change | Mod and scene | FPS | Process CPU | Process RAM | GPU use | Total VRAM use | Result |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| 2026-08-20 | Initial FPS-counter baseline; VSync forced off | Fleet Ops 4.0, gameplay, approximately 503 live objects | ~20 | 90.4% (approximately one saturated core) | 816,456 KiB resident | 18% | 76 MiB | Baseline; CPU-bound |
| 2026-08-20 | `A2FOAlwaysShowShields.dll` disabled | Same Fleet Ops 4.0 comparison scene | ~20 | — | — | — | — | No measurable FPS change; global shield scan is not the primary bottleneck |
| 2026-08-20 | All 24 optional module DLLs disabled; A2FO core retained | Same Fleet Ops 4.0 comparison scene | ~22 | — | — | — | — | Tentative +2 FPS (~10%); repeat before treating as conclusive |
| 2026-08-20 | A2FO completely bypassed by restoring the shipped startup DLL | Fleet Ops 4.0 and stock A2 Classic comparisons | ~23 | — | — | — | — | Underlying game/wrapper baseline remains low; A2FO costs approximately 3 FPS in the original scene |
| 2026-08-20 | ReShade disabled and A2FO bypassed | Stock A2 Classic comparison scene | ~75 | — | — | — | — | ReShade-off engine/wrapper baseline |
| 2026-08-20 | ReShade disabled; full A2FO core and 24 modules enabled | Same stock A2 Classic comparison scene | ~70 | — | — | — | — | A2FO costs approximately 5 FPS (~6.7%) in this scene |
| 2026-08-20 | Cache/hot-path candidate installed: empty TextureVariants policies removed, root texture index/header reads trimmed, shield safety scan timed, cleanup-only Craft callbacks masked, inactive energy/shield/turret paths skipped, non-animated hardpoint negative caches avoided, class-cache diagnostics bounded | Same stock A2 Classic comparison | 68–74 sustained; 73–76 brief high | — | — | — | — | Overlaps the ~70 FPS full-A2FO baseline; no reliable FPS uplift established from this run |
| 2026-08-20 | Same optimisation candidate; all-factions AI stress match | Large active battles | 20–25 | — | — | — | — | Stable and visually smooth while playing and panning, with no noticed frame drop or hitching; establishes the heavy-scene baseline, but the telemetry helper attached only for the final 15 seconds |
| 2026-08-20 | Revision-16 precheck-only weapon callbacks installed; three no-op committed callbacks removed per accepted shot | Brief repeat of the all-factions battle | Not recorded | — | — | — | — | Subjectively potentially better, but too brief to distinguish from normal variation; telemetry captured only six seconds of startup |
| 2026-08-20 | Existing local 32-bit DXVK 2.6.2 D3D9 backend plus GameMode; revision-16 A2FO candidate retained | Quick gameplay test; exact comparison scene not held | >200 | 67.7% average, 130.3% peak | 854,164 KiB peak PSS | 18.9% average, 94% peak | 738 MiB GPU-wide; 269 MiB process peak | Preliminary major uplift; DXVK confirmed by new `ArmadaL.dxvk-cache`. Custom Nebula pixel shader creation failed and fell back to native rendering, so visual compatibility still needs checking |

## Baseline configuration

- `D3d8to9 = 1`
- `EnableVSync = 0`
- `ForceVsyncMode = 1`
- `ShowFPSCounter = 1`
- `LimitPerFrameFPS = 0`

The process command line identified this sample as `/mod Fleet Ops 4.0`.
NVIDIA telemetry showed low GPU utilisation during the sample, while the
32-bit game process occupied roughly one CPU core. This points to a CPU/main-
thread limit rather than VSync, VRAM pressure, or GPU fill rate.

The total VRAM figure is the GPU-wide allocation reported by `nvidia-smi`;
Wine's Direct3D graphics context was not listed as a compute process, so it
should be treated as an approximate system-wide reading rather than an exact
per-game allocation.

## ReShade result

The low 20--23 FPS readings were repeatable with the active 32-bit ReShade
proxy and its five-effect preset. Removing only that local `d3d9.dll` raised
the stock A2 Classic comparison to approximately 75 FPS without A2FO and 70
FPS with the complete extension stack. The dxwrapper counter was therefore
reporting the large performance change plausibly; the primary bottleneck was
the ReShade path, not the FPS overlay.

The supplied `d3d9_no_reshade.dll` is a 64-bit binary and is not compatible
with the 32-bit Armada process. The active 32-bit ReShade DLL is retained at
`Data/performance-disabled/reshade-20260820/d3d9.dll` for recovery.

## Rejected comparison

Removing `A2FOExtensions.dll` prevented Fleet Operations from loading in the
installed proxy configuration. No FPS result was collected. The core and all
24 optional modules were restored immediately; future baseline comparisons
must keep the core DLL present.

## Memory and leak telemetry

`make telemetry` builds the native 64-bit Linux monitor. It does not inject
into Armada, inspect game objects, or control windows/input. Start it before
the game and then reproduce a stable sequence such as shell -> match -> shell
-> same match:

```bash
build/a2fo_telemetry --interval 5 --output build/a2fo-memory.csv --gpu
```

The CSV records process virtual size, RSS, PSS, private/shared memory, swap,
thread and file-descriptor counts, plus GPU-wide utilisation and VRAM use and,
when NVML exposes it for the Wine graphics process, process VRAM use.
On exit it reports the start, final, and peak PSS/private allocation. A leak
candidate is repeatable retained growth after returning to the same state;
one-time loading growth or Linux filesystem cache growth is not sufficient.

The first short post-optimisation capture ran for 8.1 seconds while the game
loaded. PSS rose from 32 MiB at process start to a 277 MiB plateau, then one
additional load step ended at 286 MiB. Threads settled at 32, file descriptors
at 163, GPU use at 0--26%, and GPU-wide VRAM stayed at 466--467 MiB. This rules
out an immediate thread, descriptor, or VRAM runaway in that interval, but is
too short to test retained growth across repeated matches.

The helper is useful for measurement and could later preconvert large legacy
TGA assets or pre-index configuration outside the 32-bit address space. It
cannot enlarge Armada's 32-bit address space, own the game's D3D8 resources,
or safely move per-frame object/render hooks out of process. Per-frame IPC
would normally cost more CPU than it saves.

The installed STA2 Classic texture tree contains about 223 MiB across 2,221
TGAs; the sampled files are already uncompressed true-colour rather than RLE
or colour-mapped. A conversion worker would therefore provide no benefit for
that mod. The in-process compatibility check now reads only the 18-byte header
for this common format instead of loading the complete source file first.

The first DXVK/GameMode capture covered 84.5 seconds. DXVK added approximately
five worker/runtime threads (37 total versus the earlier 32), process CPU
reached 130%, and GPU utilisation reached 94%; this confirms that graphics
translation/driver work used resources beyond Armada's main thread. Peak PSS
was 834 MiB and process VRAM 269 MiB, neither indicating address-space or VRAM
pressure. The capture ended after the quick test and does not establish a
retained-memory leak.
