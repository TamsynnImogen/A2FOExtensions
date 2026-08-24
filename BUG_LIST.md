# A2FOHookExtensions Bug List

## Open issues

- [ ] **AMD Windows driver crashes in Fleet Operations' native DOT3 draw**
  - **Severity:** High
  - **Expected:** A bumped Fleet Operations mesh renders through the system
    Direct3D 9 chain without entering invalid driver state.
  - **Observed:** On an RX 6800 with AMD driver `32.0.21043.12001`, gameplay
    crashes at `AMDXN32.DLL+0x54510`, called directly by Fleet Ops'
    `ST3D_Dot3_MeshVB_Render_Callback` at `FleetOpsHook+0x1e67c8`.
    Disabling Bump Mapping avoids the failure.
  - **Evidence:** Build `20260822-renderer-form-reuse-02` did not install either
    A2FO DOT3 draw hook and the run never entered Graphics Settings. Its last
    renderer event was A2FO retaining the shared DX8 device from an adjacent
    ordinary-material boundary.
  - **Candidate implementation:** Build
    `20260822-renderer-system-isolation-03` installs no A2FO DX8 hooks on the
    system backend and makes the controller skip all mapped-material SOD and
    texture mutations. Fleet Operations' native bump pipeline remains enabled;
    A2FO bump suffixes, emissive maps, specular maps, decals, and bloom require
    managed DXVK. Manual AMD validation is required.

- [ ] **Reopening Graphics Settings can destabilize the renderer session**
  - **Severity:** High
  - **Expected:** Graphics Settings can be opened and closed repeatedly without
    changing subsequent rendering stability.
  - **Observed:** A tester can enter the screen once, but repeated visits in one
    process eventually lead to a rendering crash. The captured core log has
    only one successful dynamic-control construction sequence.
  - **Root cause:** `WM_NCDESTROY` discarded A2FO's VCL control pointers even
    though Fleet Operations can retain the `TGraphicOptionsForm` component and
    recreate only its HWND. A later visit consequently registered a second set
    of owned renderer/effect controls on the same form.
  - **Candidate implementation:** Build
    `20260822-renderer-form-reuse-02` retains the form-owned objects, validates
    their VMT/Owner/Parent against the live native controls, reacquires the
    combo HWND, and reuses the original set after handle recreation. Manual
    repeated-entry and subsequent in-game rendering validation is required.

- [ ] **Windows renderer selection reverts to System Direct3D 9 instead of activating DXVK**
  - **Severity:** Medium
  - **Expected:** Selecting `DXVK (Vulkan)`, fully exiting Fleet Operations,
    and relaunching should activate the managed
    `Data\\renderers\\dxvk\\d3d9.dll` payload and retain DXVK as the selected
    renderer.
  - **Observed:** The selection appears to register, but the next launch is
    still using System Direct3D 9 and the Graphics Options selector has
    reverted. Earlier affected runs produced no `A2FORenderer.log`, indicating
    that the post-exit helper was not launched or did not reach its logging
    entry point.
  - **Current diagnostics:** Build `20260822-renderer-audit-01` adds a core and
    helper fingerprint to the logs and makes the core create
    `A2FORenderer.log` even when no helper action is scheduled. This should
    distinguish a failed selection event, helper-launch failure, missing DXVK
    payload, and activation/restore failure when work resumes.
  - **Related result:** The system renderer can still crash in AMD's native
    DOT3 driver path, so Vulkan persistence now also blocks the preferred
    mapped-material backend on that hardware.
  - **Workaround:** Use System Direct3D 9 with Bump Mapping disabled until the
    managed DXVK switch is reliable.

- [ ] **A2FO mapped-material shaders distort Fleet Operations bump maps**
  - **Severity:** High
  - **Expected:** Global bump suffixes should attach the derived native normal
    map while Fleet Operations continues using its stock
    `Shaders\\dot3_directional.nvv` DOT3 vertex shader.
  - **Observed:** A native Windows tester reports that bump/normal maps look
    incorrect whenever A2FO's packaged DX8 shader set activates; removing that
    set returns the same models to Fleet Operations' expected bump rendering.
  - **Evidence:** The successful tester log confirms two derived bump maps were
    attached with native `0x200` flags and the extension pixel shader was
    selected. The previous core simultaneously replaced Armada's checked
    `shaders\\dot3_directional.nvv` path with
    `shaders\\dx8\\vertex\\vs.nvv`, so the problem was after map discovery.
  - **Candidate implementation:** The core now leaves Fleet Operations'
    complete multipass DOT3 renderer native. The shader-handle route no longer
    selects an extension pixel shader while the normal map occupies stage 0;
    bumped emissives use the existing scoped stage-2 fallback at the final
    indexed draw. Bumped specular masks use a separate bounded additive replay
    after the native final draw. Automated verification passes; manual
    bump/specular comparison is still required.

- [ ] **Directional shields and ammunition are absent on stations with build queues**
  - **Severity:** High
  - **Expected:** A selected station should retain its configured ammunition
    and directional-shield presentation when Armada switches to the tall
    producer/build-queue information panel.
  - **Observed:** Both extensions disappear while a station's build queue is
    shown, although their ODF policies are registered and the same station can
    expose valid live values.
  - **Root cause:** `A2FOCraftIdentity` hooked only Armada's ordinary selected
    renderer at RVA `0x000f3770`. Producers use the separate tall-panel
    renderer at RVA `0x000f3560`, whose text context lives at InfoDisplay
    offsets `+0x100`/`+0x104` instead of the ordinary captain component at
    `+0xbc`.
  - **Candidate implementation:** The module now preflights and hooks both
    native renderers. The tall path rebases its live `infoBuildName` or
    `infoBuildClass` rectangle onto `infoSingleCaptainTextArea`, then submits
    ammunition and directional shields through the existing authoritative
    `infoSingle*` extension rectangles. The targeted build, full unit and
    documentation suite, Wine DLL/module-init smoke, and binary verification
    pass; in-game validation is still required. Installed candidate SHA-256:
    `e7a732122424f8b039601b4ddfcc8d638a61f85d6c2a42688b1ff78b585189f4`.
    Rollback:
    `Data/rollback/2026-08-21-builder-panel-ui/A2FOCraftIdentity.dll`.

- [ ] **Windows crash when entering edit mode or returning from the map editor to gameplay**
  - **Severity:** Critical
  - **Expected:** Switching between gameplay and edit/map-editor modes should
    preserve or safely recreate the renderer without terminating the game.
  - **Observed:** Native Windows testers consistently crash during the mode
    transition when `A2FOFeaturePack` and `A2FOHybridBuild` are both enabled.
    `AlwaysShowShields` is not required. Disabling either FeaturePack or
    HybridBuild prevents the crash; the pre-live-build-submenu HybridBuild
    baseline also crashes, ruling out the submenu changes.
  - **Evidence:** `exceptArmada2.txt` contains two access violations at the
    identical `A2FOExtensions.dll` RVA `0x0004989E`, despite different module
    load addresses. The shipped 2026-08-20 beta binary maps this to
    `adopt_live_device(IDirect3DDevice8*)`, at the indirect call implementing
    `g_device->SetTexture(1, nullptr)`. Both reported invalid read addresses
    equal the corrupt `EDX` vtable value plus `0xF4`; one stale vtable value
    has been overwritten with the ASCII bytes for `"Blur"`.
  - **Root cause:** Core `A2FOExtensions.dll` retains `g_device` as a borrowed
    raw COM pointer. Windows destroys or replaces the Direct3D device during
    the mode transition. When the next craft render supplies a new device,
    `adopt_live_device()` attempts to clean up through the already-freed old
    pointer. HybridBuild exposes the path by linking the Nebula emissive
    context through the Fleet Ops craft-render boundary; FeaturePack is an
    activation dependency, not the crashing implementation.
  - **Platform note:** Wine currently masks the lifetime bug by retaining or
    reusing its Direct3D wrapper differently. This is not a Windows defect and
    could still surface on Wine under different allocation or timing.
  - **Follow-up evidence:** An owned-`AddRef()` candidate removed the original
    `SetTexture` fault but exposed a null `DeletePixelShader` route during late
    old-device cleanup (`A2FOExtensions.dll` RVA `0x0004C196`) and a separate
    `d3d9.dll` failure inside Fleet Ops' device-destruction callback. The outer
    dxwrapper object can remain allocated while its underlying device has
    already been dismantled, so ownership alone is insufficient.
  - **Required fix:** Retain the live device during normal rendering, but hook
    Fleet Ops' authoritative device-destruction callback. Release extension
    shaders, textures, caches, and the owned COM reference before native
    destruction begins; clear `g_device`, then adopt the replacement device
    when rendering resumes. Reset handling must independently invalidate the
    same GPU caches. Validate edit-mode, map-editor-to-gameplay, and process
    shutdown transitions on native Windows.
  - **Candidate implementation:** The core now acquires the replacement COM
    reference before cleaning the preceding device, releases the old owned
    reference only after its shaders/textures/targets are invalidated, and
    provides the matching orderly-shutdown release. The checked pre-`Reset`
    hook is installed for every DX8 backend (not only DXVK bloom) and clears
    all extension GPU caches for lazy recreation. A focused ownership-order
    regression test and the full automated suite pass; native Windows mode-
    transition validation is still required before resolving this issue.
  - **Workaround:** Disable `A2FOHybridBuild` until a corrected
    `A2FOExtensions.dll` is available.

## Resolved issues

- [x] **shipNameColor does not affect ship name text**
  - **Severity:** Medium
  - **Expected:** `shipNameColor` should control ship-name colour.
  - **Observed:** Changing `shipNameColor` has no visible effect; `infoTextColor` changes both ship-name and ship-class colours.
  - **Notes:** Possibly color binding conflict/miswired mapping between name/class style paths.
  - **Resolution:** `A2FOCraftIdentity` now applies `shipNameColor` only to
    the native selected-name components during their render and restores the
    shared component state immediately afterward. A2FO's added text rows keep
    their separate colour settings. The targeted module build, identity unit
    tests, documentation tests, and Wine DLL-load smoke pass. Candidate SHA-256:
    `2810ef9ae37aed3466fbd096fccf2bb06b977089c05a3c3084ed8fb1c8820983`.
    Rollbacks:
    `Data/rollback/A2FOCraftIdentity.pre-ship-name-colour-20260820.dll` and
    `Data/rollback/A2FOCraftIdentity.pre-system-icon-colours-20260820.dll`.
    The `STA2 Classic: Test` fixture now gives each selected-panel element a
    distinct colour and makes `fbattle.odf` deterministically expose class,
    ship name, captain, registry, Photon/Quantum ammunition, and directional
    shields in one selection. The fixture also assigns distinct healthy, low,
    critical, disabled, and destroyed subsystem icon/value-text colours, and
    `fbattle.odf` exposes all five native subsystem icons. The same live-state
    palette now colours hull, shields, and crew consistently in the mouse-over
    strip, plus crew in the selected presentation. The native officer
    icon/value uses a separate fixed `officerIconColor` and never inherits
    this health palette. The subsystem-colour bridge
    preserves Fleet Operations' pre-existing `SystemIcon::Render` detour;
    the initial conflicting entry-hook candidate was rejected by runtime
    signature preflight. The cooperative candidate and its independently
    configurable icon/value colours were confirmed in-game. The first
    value-text candidate was also rejected
    after runtime testing showed that RVA `0x000ec748` colours the value icon,
    not the text glyph. The replacement hooks the checked native text draw at
    RVA `0x0010c393`, changes only its temporary RGB record, exact-checks
    `SystemValue`/`HullText`/`ShieldText`/`CrewNumText`/
    `OfficerTextAndSprite` before applying any
    colour, and preserves Fleet Operations' existing live draw-call target
    when that call is already replaced. A rejected intermediate build targeted the preceding
    `mov ecx,[esi+4]` at RVA `0x0010c390`; runtime byte diagnostics identified
    the correct CALL boundary three bytes later without modifying it. The
    first officer candidate also mistook the preceding energy widget at
    vtable RVA `0x002b4adc` for the officer; the constructor and RTTI identify
    the actual `OfficerTextAndSprite` vtable at RVA `0x002b4c50`. The same
    exact `EnergyText` identification now gives the special-energy icon and
    value text its own fixed `specialEnergyIconColor`, independent of both the
    live-health palette and `officerIconColor`; the test mod uses yellow.
    Current candidate SHA-256:
    `c36264936cfe1612af904ec6c1f2c162457902647515c809bd10686f35030f40`.
    Its immediate rollback is
    `Data/rollback/2026-08-21-special-energy-colour/A2FOCraftIdentity.dll`.

- [x] **New single-player menu is not integrated into FO window**
  - **Severity:** High
  - **Expected:** The new single-player menu should be reachable and embedded in the FO menu flow.
  - **Observed:** The new menu is currently detached / not integrated.
  - **Notes:** Verify window registration, lifecycle hooks, and menu navigation paths.
  - **Resolution:** `A2FOMissionSelector` now resolves Armada's game-window
    owner through the same display-engine path as native `DoSingle` and reuses
    stock borderless dialog resource `115`. The previous synthetic template's
    `WS_CAPTION`, `WS_SYSMENU`, and `WS_EX_APPWINDOW` styles—and its guessed
    `GetActiveWindow` owner—have been removed. The scrollable browser remains
    modal for native `SetupMission`, Back, Escape, and shell-state handling,
    but no longer creates a detached captioned window or taskbar entry.
    The targeted build, full unit/documentation suite, Wine module-init smoke,
    and Wine DLL-load smoke pass. Candidate SHA-256:
    `4ed5547cefada9d0a23ac64331d890723bd65818b5cc396b6b032ee0e254cdba`.
    Rollbacks:
    `Data/rollback/A2FOMissionSelector.pre-shell-integration-20260820.dll`
    and
    `Data/rollback/A2FOMissionSelector.pre-full-client-20260820.dll`.
    Manual validation confirmed the browser is embedded but its initial
    86%-by-82% sizing left an unnecessary outer margin. The follow-up candidate
    now uses 100% of the native owner's client rectangle while retaining the
    controls' internal layout padding. The full-client result was accepted
    during manual in-game validation.

## Environment / context

- File path: `/home/tamsynn/A2FOHookExtensions/BUG_LIST.md`
- Previously resolved bugs were manually validated on 2026-08-20.
- Windows renderer-lifetime crash recorded from beta-tester evidence on
  2026-08-21.
