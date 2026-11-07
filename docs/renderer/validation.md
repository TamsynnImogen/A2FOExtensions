# Renderer first-pass validation

This is a diagnostic infrastructure milestone. Hidden console fixtures do not prove Armada/FO launch, menus, gameplay, visual parity or native Windows behavior. No game entrypoint or desktop input automation is used by these checks.

## Reproduce

```sh
make -j2 build/A2FOExtensions.dll renderer-modernisation-tests
python3 tools/generate_d3d8_observer.py --check
python3 tools/renderer_usage_matrix.py
python3 tools/run_renderer_validation.py \
  --prefix /home/tamsynn/.cache/a2fo-refit-native-prefix-20261018 \
  --output build/renderer-first-pass/builtin-final
python3 tools/run_renderer_validation.py \
  --prefix /home/tamsynn/.cache/a2fo-refit-native-prefix-20261018 \
  --output build/renderer-first-pass/managed-final \
  --wrapper-root '/home/tamsynn/NVMeData/Fleet Ops Roots/Data'
```

The prefix must already exist and be dedicated to A2FO fixtures. Use a different dedicated prefix where appropriate. The runner removes desktop DISPLAY/WAYLAND variables, uses a fresh private runtime directory, starts GPU fixtures only beneath `gamescope --backend headless`, and cleans only its own process group. It never launches a game executable or sends input. The wrapper variant copies the installed D3D8/dxwrapper/D3D9 DLLs and `dxwrapper.ini` into the private output directory; originals are read only. Non-overlapping source/output paths are required. `SKIP` is reported distinctly and fails the all-passed gate.

## Contracts exercised

| Fixture | What it establishes |
| --- | --- |
| `d3d8_observer_test.exe` | synthetic PE32 named import patch/idempotence, untouched default and refusal of unavailable full D3D11 backend; native object/IUnknown/GetDevice identity, exact input/output/HRESULT/lock arguments; provider vtables unchanged; public Release(0) resurrection and address reuse; internal address reuse with a newly constructed table; bounded samples/histograms/file size; malformed/unterminated/overflowing/execute-only PE rejection |
| `d3d11_probe_native.exe` | invalid/missing/foreign-thread HWND and invalid dimensions rejected; hidden HAL FL11_0 device, RTV/depth/viewport, clear/Present; debug-layer fallback; shutdown and same-window reinitialization |
| `d3d8_d3d11_window_test.exe` | real executable D3D8 factory import; native HAL caps, VB/IB/texture/surface lock/update/identity; one D3D11 probe on the live hidden D3D8 HWND; D3D8 state/viewport and established window style/rect/visibility preserved; subsequent native RGB readback and Present; state block; Reset and subsequent Present; one-shot behavior |

The shared-HWND check validates the narrow windowed discard path for the tested Wine providers. It does not certify exclusive fullscreen, Windows drivers, every DXGI implementation, arbitrary wrappers, or the actual Armada message/window lifecycle. D3D11 device timing in the fixture is not a game performance result.

Final runs on 2026-10-21 passed all three fixtures for both the built-in provider and the private installed-wrapper chain: six passes, no skips. Receipts are `build/renderer-first-pass/builtin-final/results.json` and `managed-final/results.json`, with complete per-fixture console logs and JSONL traces alongside. The synthetic traces reached the 4 MiB limit and retained valid JSON/limit markers. Both real-provider fixtures completed state blocks, later RGB readback, Reset and later Present after their one successful D3D11 probe. The core DLL build passed with existing warnings in unrelated menu/renderer code; new fixture targets compile with `-Werror`. Import inspection found only COMCTL32/GDI32/KERNEL32/msvcrt/USER32 dependencies, with no mandatory D3D11/DXGI import.

## Failures retained and corrected

The early real-provider run reached successful D3D11 Present but terminated later when the native backbuffer was reacquired. Native Direct3D can retain implicit resource ownership after public `Release()` returns zero. A cloned per-object vtable must therefore outlive the observation record. The fix retains bounded per-provider/type dispatch tables for process lifetime and lazily recreates metadata on reentry; no native COM reference is retained by the observer. A synthetic reproduction and the subsequent real-provider pass cover this. Early evidence is `/tmp/a2fo-d3d8-d3d11-window-output/A2FOD3D8-220-120005675.jsonl`, with the failure phase identified from its final API events (state-block completion preceded reacquiring the backbuffer).

The first private dxwrapper/DXVK run failed the window-state assertion because its baseline was recorded before native D3D8 device creation. Additional state snapshots showed dxwrapper changes exstyle `0x08000080` to `0x08000000` during that creation, before the diagnostic probe. The fixture now compares against the established native-device state and logs both snapshots. The initial failure remains under `build/renderer-first-pass/managed-wrappers`; the diagnostic rerun is under `managed-wrappers-second`.

The initial readback fixture sampled outside its own non-default viewport. That sample was corrected to a pixel inside the viewport; the color assertion remains enabled. This was a fixture error, not an accepted renderer mismatch.

Independent source review found import-string bounds, whole-log size limits and literal-log allocation failure issues; these were fixed and exercised in the synthetic fixture. Review also identified implicit lifetime/provider assumptions. The reproduced lifetime issue above is fixed; new native-table construction invalidates stale metadata, dispatch kinds are checked, object records cap at 65,536 and dispatch tables at 256. Observation declines further new tables at the limit. The installer remains explicitly early/startup-only and serialized by core attach; hot install/uninstall is not supported.

## Game acceptance

No installation was deployed or manually launched in this pass. The locally staged candidate and baseline hash receipt are under `build/renderer-first-pass/candidate`. The next acceptance step is a backed-up test installation with `Backend=d3d8, Trace=1`, followed by `d3d11-probe` in normal windowed/borderless mode. Fully restart for either setting. Preserve returned game logs and supply screenshots manually where needed. Keep game traces separated from these fixture traces when generating the compatibility matrix.

The full parity scenarios and staged exit gates are in [modernisation.md](modernisation.md). Shader translation, persistent compiled shader caching, UI/gameplay parity and modern presentation are pending.

## Subsequent SM5 UI slice

See [ui-slice-20261021.md](ui-slice-20261021.md) for the transformed-vertex draw backend, persistent bytecode cache, 19 paired pixel scenes and updated triangle probe. Native FO game observations now populate the compatibility matrix; reproduce it with the three named `--fleetops-trace` inputs recorded in that slice rather than overwriting the matrix with fixture or empty evidence. Full game D3D11 rendering is still pending.

## Subsequent real-window proof and shader diagnostics

The user confirmed the brief SM5 triangle in Fleet Operations' real 1920x1080
window. The preserved native log records successful device creation, draw and
Present; the probe was then disabled. This is real-window diagnostic proof,
not a translated game frame or complete gameplay parity.

The next [shader-capture slice](shader-capture-20261021.md) adds optional bounded
declaration/program payloads and an offline hash report. Eight Windows fixture
processes pass across helpers and both native/provider window paths; eight
Python report tests pass. Exact CreateShader input hashes are verified
independently of provider readback, which can be normalized or unsupported.
Evidence: `build/renderer-shader-capture-20261021/fixture-shader-report.json`,
`helpers/results.json`, `builtin-final-provider-contract/results.json`, and
`managed-final-provider-contract/results.json`. Earlier FAIL logs are retained
and are not final receipts. The backed-up Roots deployment enables only
temporary tracing with the legacy backend; native-game shader contents remain
unknown until a fresh manual capture is returned.

## Captured vertex-program slice

The returned native FO run is preserved under
`build/renderer-shader-capture-20261021/native-20261021-101843Z/`; tracing is now
disabled. Three complete A2FO vertex programs have been translated to SM5.
The [VS1.1 slice report](vs11-slice-20261021.md) records six paired scenes on
Wine D3D8 (exact RGB) and preserved dxwrapper/DXVK at its actual eight samples
(maximum channel delta one), along with persistent cache reuse and CPU checks.
One native shader payload remains unknown. These are offscreen shader-program
comparisons, not game frames routed through a complete D3D11 compatibility device.

## Compressed texture slice

[BC5/BC7 validation](bc-textures-20261021.md) covers bounded DDS parsing, typed
immutable full-mip uploads, 24 raw/normal/sRGB GPU comparisons, a transformed-UI
BC7 draw and a CPU-reference check of one converted game texture. Receipts live
in `build/renderer-bc-textures-20261021/`. These prove backend components, not
legacy game loading or measured in-game VRAM reduction.

The [FO4 BC child/resource bridge](fo4-bc-test-20261021.md) adds supplied-mip
conversion checks, D3D8 texture/surface contracts, implicit binding lifetime and
real BC5/BC7 GPU sampling through the facade. All-child allocation checks are
separate from pixel quality and game acceptance; no full D3D11 game route is enabled.
