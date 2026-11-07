# Claude handoff: A2FO native DX12 / Vulkan renderer

Updated 2026-11-07. This is a working handoff; implementation is unfinished.

## Claude parallel work and current takeover state (2026-11-07, read first)

Codex ran out of usage around 01:46. Claude is continuing the work. Everything below was done by Claude
alongside Codex and is documented in its own files. Codex's sections further down remain accurate for
its threads, except where this section corrects them.

**State corrections found at takeover:**
- **Material colours progressed beyond its section below.** In
  `build/refit/material-colours-candidate/controls/1794014521617326874`, the DX12 `off` control
  passed. The DX12 `on` control (revision `material-colours-dx12-on-Duel-1794014624798586977`)
  **failed safe**: the log says `authored material colours refused: unsupported material colour model;
  original retained`. The controller then recovered the fixture (`recovery.json`, restored true) and
  wrote `post-controls-restoration.json`, Core `a5b4a87e…`, status `restored-after-failed-control`.
- Codex then built `fixtures-lambert/`, a rebuilt Display (`display-source/`, `d323e9c1…`) and an
  ABI2-strict/ABI3-opt-in callback test that passed on both backends (`callback-runtime/1794015651338479343`).
  It also wrote `run-controls-v2.py` (01:46), which **never ran**. This thread is unfinished, and
  Claude has not continued it.
- `post-controls-restoration.json` is the valid DX12 bridge `--previous`, *not*
  `geometry-rhw-candidate/post-vulkan-restoration.json`. An empty child `sod/` directory remains from
  that control; it is harmless and was left alone.
- Codex's note about "another thread" on the old shared Vulkan trial referred to Claude. That work
  is finished, and the trial was restored after every series.

**Claude features and analyses** (each document has receipts and hashes):
- **SOD coverage census and usage weighting:**
  [refit-sod-coverage-census.md](refit-sod-coverage-census.md), tool `tools/refit_sod_census/`.
  - Of the 230 unit models, 74.5% of triangles are statically eligible and 16.7% are two-group.
  - Lighting materials are a **first-come global namespace**: undefined names resolve to an earlier
    SOD's definition, or to the built-in `default` at `db+0xc` (proven at `0x6468b5`).
  - About 12.7% of eligible triangles are exposed to load-order-dependent material definitions.
    Core's exact `material_current` refuses those safely.
- **Two-group (base + team) meshes, opt-in `[Refit]TwoGroupMeshes=1`, default off:**
  [refit-two-group-meshes.md](refit-two-group-meshes.md). Code is in Root
  `core/refit_scene.cpp` (`GroupPlan`, per-group packets consumed in native draw order) and
  `renderer/refit/mesh_scope.*` (`select_opaque_group2`). There is also an observation-only
  `[Refit]DiagnoseGroups=1`.
  - The native behaviour was proven live first: one checked opaque draw per 0x1c-byte group record,
    in SOD group order.
  - All 5 fixtures are fully replaced on private Vulkan. On DX12, the sliver candidate gives:
    `fed_saber`, `fed_premonition`, `rom_talon`, `rom_eresis` (LOD) all both groups;
    `kli_mixedtech_yard_fed` all both groups, so **all 5 on DX12 too**.
  - White/black controls pass on both backends. `TwoGroupMeshes=0` keeps the old behaviour exactly.
- **Geometry-parity sliver tolerance (user-approved):**
  - `geometry_sliver_area2 = 1e-3` px² is used **only after** the strict match fails.
  - Expected slivers may be absent, and native slivers must match authored ones.
  - Test: `tests/refit_geometry_sliver_test.cpp`.
  - Root cause was `rom_talon`'s 4 edge-on slivers of 3e-5..1e-4 px² (median 1.41).
- **`explain_geometry_parity2`:** a failure-only diagnostic, now rotation-tolerant matching.
- **Upstream provenance audit:** [refit-native-provenance.md](refit-native-provenance.md). The
  manifest is exact against a fresh fetch. 23 modified, 8 added, 0 missing; licences are intact.

**Root source now contains:** Codex's tolerant matcher, the RHW assignment retry, Claude's sliver
path, and the two-group code. All parity suites pass: `refit_geometry_{sliver,motion,rhw_assignment}_test`
and `refit_mesh_{group,scope}_test`.
- The latest Claude Root build is `build/refit/sliver-tolerant/A2FOExtensions.dll`, `6903cfdd…`,
  with 198 exports and no drift. It does **not** include Codex's isolated material-colours objects.
- **The manual package now runs Core `10ecb1a5…` + Renderer `bdb3b522…` (spotlights) in the replacement children** (staged at the user's request by `build/refit/spotlights/stage-manual.py`; backup `candidate-backups/spotlights-1794052954195424535/`; `Check-Package.sh` passes; ODF lights still off in those children). Before that: `a7384890…`: `6903cfdd` behaviour plus coverage telemetry and the merged, inert colour code. Staged by `build/refit/coverage-telemetry/stage-manual.py`; backup `candidate-backups/coverage-telemetry-1794049898255614377/`; `Check-Package.sh` passes. Previously: `6903cfdd…`, with `TwoGroupMeshes=1` in the replacement children only. It was staged by `build/refit/sliver-tolerant/stage-manual.py` (Codex's staging procedure plus the two configs) and passes `Check-Package.sh`. Backup: `candidate-backups/two-group-sliver-1794017479704549619/`. Receipt: `build/refit/sliver-tolerant/manual-stage-receipt.json`. User visual check pending.

**Intermittent load-time freeze (not detected by the runner):** about 2 of the last 100 private captures
show a madExcept "The application seems to be frozen" dialog during asset loading. Neither run reached
any Refit scene work. The two runs were Codex's Core `894a0edc` (`restore-codex-core-Duel-1794016846152098049`)
and merged Root Core `e1ee05ec` (`material-colours-lambert-vulkan-off-Duel-1794018592971835523`). Both
were on the DX12 trial and pre-date any mesh scope, so this is environmental and not a code regression.
`revise_trial.py` still receipts such runs as `launch-diagnostic-complete`, so checks must keep
requiring scene log lines. Restore runs are valid as state transitions only. The dialog comes from madExcept's freeze watchdog (`CheckFreeze`/`FreezeTimeout`) compiled into the
protected original `FleetOpsHook.dll`; it is absent from `ArmadaL.exe` and Core. A slow software-GPU
asset load that stalls the message pump trips it. Do not modify that DLL. Retry the run instead.

**Material colours merged into Root (2026-11-07, Claude):** see
[refit-material-colours-status.md](refit-material-colours-status.md).
- Root now contains Codex's `[Refit]MaterialColours` (Constant/Lambert), its ABI3 RHW callback (Root
  `third_party` Display equals Display `d323e9c1…`), Claude's two-group/sliver work, and opt-in
  `[Refit]DiagnoseColours=1`.
- Latest Root Core: `build/refit/colours-oracle-lights/A2FOExtensions.dll` (`fb83d0ae…`). It is
  `e1ee05ec…` plus light details in the diagnostic. Both builds have 0 warnings, no drift and 198
  exports.
- Colour parity against native, Lambert receiver: DX12 2.65 and Vulkan 2.68 mean abs, about 95% of
  pixels within 8/255 (35 without colours).
- With `MaterialColours=1`, Phong groups make their whole mesh stay native. That includes most
  two-group hulls, so keep colours off in the manual package until Phong is handled. The Phong design
  choice is open: per-pixel modern specular, or native-lit vertex colours.
- **DX12 trial now:** Core `fb83d0ae…`, Display `d323e9c1…`, config `8325ba2b…`, empty fixture
  dirs. Next `--previous`: `build/refit/colours-oracle-lights/post-oracle-restoration.json`.

**Spotlights done (2026-11-07, Claude):** [refit-spotlights.md](refit-spotlights.md).
- Renderer ABI3: `GetInterface(3)`, `prepare_parts3`, up to 4 cone spots, and SOD shader c28–c43
  regenerated with the pinned toolchain. Renderer `build/refit/renderer-spotlight/` (`bdb3b522…`);
  Root Core `build/refit/spotlights/` (`10ecb1a5…`).
- The GPU fixture passes on both backends, and live ODF spot aimed/away controls pass on both.
- Without ABI3, spots stay refused (fail-safe). Both trials are restored to their original Renderer
  `e9a78291…`.
- Not staged in the manual package.

**Full-takeover strategy finding (2026-11-07, Claude):** [refit-whole-view-feasibility.md](refit-whole-view-feasibility.md).
- FO's `ArmadaL.exe` is the stock Armada II 1.1 image plus a `.test` patch section. All 133
  byte-checked upstream render3d symbols match on disk.
- At runtime, 126/133 upstream hook sites are still stock. World, particles and overlays are fully
  untouched. The patched sites include A2FO's own `fn_go_render` capture hook.
- **Owner decision pending:** keep per-mesh only, or admit upstream whole-view modules one at a time.
- Opt-in audit `[Refit]AuditUpstreamSites=1`, Core `build/refit/upstream-audit/` (`1f937396…`).

**Per-pixel Phong done (2026-11-07, Claude, owner decision):** see the Phong section of
[refit-material-colours-status.md](refit-material-colours-status.md).
- Renderer `build/refit/renderer-phong/` (`ab13779d…`); Core `build/refit/phong/` (`83af9c8d…`).
- The GPU fixture passes on both backends. Live DX12 Phong content with colours on replaces
  correctly; Mediterranean error against native drops from 12.5 to 7.9.
- Not staged.
- **Next, at the owner's request:** BC1–BC7 textures, then specular maps, in the Refit renderer. The full
  verified plan is in [refit-bc-and-specular-maps-plan.md](refit-bc-and-specular-maps-plan.md).

**Runner defect:** `build/refit/revise_trial.py --material-control` uses the regex
`^HeadlessMaterialControl=`. It misses the spaced `HeadlessMaterialControl = none` that both trial
configs use, and appends a stray line under `[Native11]`. The control then silently stays inactive.
Normalise the key under a preparation receipt before using it, or fix the regex.

**Trial pointers:**
- Shared Vulkan trial: `restore-codex-core-Duel-1794018503571304423` (Core `85439152…`, original
  fixture).
- DX12 trial (superseded, see the material-colours bullet above): `restore-codex-core-a5b4-Duel-1794017015347420615` (Core `a5b4a87e…`, config `8325ba2b…`, Codex's probe restored byte-exactly). Note that `material-colours-candidate/A2FOExtensions.dll` was rebuilt in place to `894a0edc…` after the controls. The exact `a5b4a87e…` is preserved at `build/refit/codex-dx12-restore-a5b4a87e/`. An intermediate restore run briefly installed `894a0edc…` (revision `restore-codex-core-Duel-1794016846152098049`), which was superseded.

## In-progress continuation: authored material colours (2026-11-07)

Current work is isolated in `build/refit/material-colours-candidate/`, based on the frozen Core13cc source with the reviewed RHW checker. **Manual Core854 and its shader/helpers remain untouched. Root core/refit_scene.cpp still belongs to the concurrent two-group work and was not edited for this feature.**

Lead recovered exact original native colour semantics: wrapper callbacks initialized at0x62314a select Constant0x6469b0/Lambert0x646bd0/Phong0x647060. Material-copy0x6481d0 writes workspace ambient+0x58/diffuse+0x64, team overrides. Lambert0x646c31..64 adds scene ambient to effective material ambient. Directional0x62f9c0 and point0x62f640 multiply contributions by effective material diffuse.11 guarded spans match both pinned original executable hashes; `build/refit-material-parity-native/profile.json` plus `generate-profile.py` and disassemblies bind this read-only evidence. No original executable was modified and no native function oracle was executed.

New opt-in `[Refit]MaterialColours=1` handles only Constant/Lambert. Pure `material_colours.hpp` folds authored colours into the existing ABI2 ambient/light constants, clears shader TEAM so ambient stays neutral, keeps native emissive separate, and leaves Phong native. Native workspace coefficients and fresh ambient/team/registered lights are rechecked at DIP even with OdfLights0. No shader or host ABI change. Strict CPU helper tests, strict i686 scene compilation and independent source/profile review pass. Full per-vertex/per-pixel, saturation, specular, movement and image parity remain open.

The first isolated DLL build SHA36591be9 is archived under `first-build-36591be9/` with its old scene source and receipts; it had no runtime qualification. A rebuild is underway to add bounded per-model successful draw diagnostics, needed to prove both controlled receivers reach the new path. The model diagnostic catches its own allocation/log errors so logging cannot convert a successful GPU draw into native fallback. Source/profile/helper integration remains private; do not normal-build Root as a substitute.

Next: finish build/export proof, generate two synthetic colour variants of Venture and Mediterranean SODs under candidate `fixtures/` with unique lighting-material names, stage them only in the **stopped private DX12 trial**, and run same-candidate off/on/native comparisons on each actual backend. Old shared Vulkan trial is still another thread's work; use the owned DX12 fixture with temporary backend switch/restoration. Current `--previous`: `build/refit/geometry-rhw-candidate/post-vulkan-restoration.json`. No material-colour gameplay or pixel claim is qualified yet. Preserve/restore private config/ODFs/assets and keep manual Core854 available.

## Latest continuation: user improvement, guarded-matcher hardening, session identity

User reported **“seems much better”** after running the full-path `Launch-DX12.sh`. Latest completed session: manual package `logs/20261107T003633Z-3e33efa9-dx12-replacement`,150.3s, mesh submission, natural process/Wine/cleanup0. Record: `build/refit/manual-feedback/20261107T003633Z-feedback.json`. This is a positive visual report, not complete flicker/renderer acceptance. Projected/count refusals still appear as deduplicated reasons. Earlier session `003524Z-0ff62b44` retains incomplete “running” metadata; preserve it, do not count it as a passed run.

**Manual package stays on improved Core85439152, Display3c317ae7, Renderere9a78291.** Only its evidence helper/metadata were updated in this continuation. The new helper pins actual critical input hashes/sizes and the manifest bytes before launch, then checks the same path set afterward even if inputs or the manifest change/disappear.18 headless tests pass independently. It was staged under the stopped-package lock with verified backup and unchanged Core/protected originals; `build/refit-session-identity-checks/{stage-receipt,staged-integration}.json` bind the update. Read-only integration rechecked all55 real inputs. This corrects an evidence gap: earlier sessions did not independently snapshot binary hashes and are attributed using the guarded launcher/deployment timing/current unchanged package, not a retroactively fabricated digest.

**Separate unstaged hardening candidate:** `build/refit/geometry-rhw-candidate/A2FOExtensions.dll`, SHA-256 `e2805da0ea4c85f253a3d2caa572eec1d906d674c7772c110213f6b827cd3130`.933 frozen baseline Core sources with only the geometry checker changed, reconstructed original objects and copied590 native link inputs; all198 exports pass. The matcher now retries ambiguous triangle assignments under a single shared reciprocal-W scale, using lower-endpoint candidates and the same bounded fail-closed work budget. It charges retry initialization and uses visitation generations.972 ordering/cyclic/scale cases pass; impossible shared-scale cases still reject transactionally, as do the existing motion/parity suites. The previous stable checker demonstrably failed the new witness. Independent review: `build/refit/geometry-rhw-candidate/independent-review.md`. This is a distinct reproducible guard defect; it is **not** established as the cause of the reported projected/count refusals or visual flicker.

DX12 actual-candidate100s scene check passed: `geometry-rhw-dx12-Duel-1794012552039869094/receipt.json` in the private DX12 trial. Vulkan qualification also passed100s using a temporary backend-only setting in that same stopped fixture; the original DX12 setting was verified restored. Both actual-factory logs, mesh submissions, captures0, owned cleanup0 and Xvfb reaping are bound in `build/refit/geometry-rhw-candidate/runtime-smoke.json`. Runtime Vulkan receipt: `geometry-rhw-vulkan-private-Duel-1794012818575170433/receipt.json` in that private trial. Both deliberately stopped games report-15; no new natural-exit or motion acceptance. Coordinator/preparation: `build/refit/geometry-rhw-candidate/check-vulkan.py`, owned copied runner and the resulting `geometry-rhw-vulkan-preparation-*` receipt. Child name still says DX12; establish the actual backend from the fresh **Vulkan factory** log and pinned backend config, not its label.

**Concurrent work warning:** another thread is actively using the old shared Vulkan trial for two-group experiments (`matcher-romeresis`, `talon-explain`, Coreb217 at discovery). Our attempted revision refused before mutation because its namespace was active. Do not stop or overwrite that work, assume its previous Core/receipt is still current, or bundle newer Root group changes into the stable manual build. We left it untouched, notified `all_parts_gpu`, and used the other stopped fixture for our Vulkan check. Root renderer source contains both the newer group/diagnostic work and our checker; isolated snapshots/objects define the actual candidate. No new shader/ODF lighting formula was guessed; captured ambient/diffuse/specular/power semantics still need native parity work before changing shading.

**Continuation-state pointer:** future revisions of the private DX12 fixture should use `build/refit/geometry-rhw-candidate/post-vulkan-restoration.json` as `--previous`, after checking no active namespace and hashes. It explicitly bridges the completed Vulkan receipt to the restored DX12 config and is **not** an extra runtime qualification. The earlier receipt paths lower in this document are historical. The manual helper's18 tests/review and staged55-file integration pass; the launcher harness now also requires `input_identity_after.all_inputs_match` to count future lifecycle runs as passing. No tests/builds remain running from this continuation; another thread's work may still be active. `milestone.json` beside each new evidence directory pins the completed bounded work.

## Active priority: moving-ship brightness flicker (2026-11-07)

User clarified the flicker is **brightness/colour**, not disappearing geometry. A deterministic tiny-motion regression reproduces a false geometry-parity rejection caused by exact lexicographic triangle pivots changing under subpixel projection rounding. A tolerant cyclic one-to-one matcher now passes strict motion and existing parity suites, independently reviewed. It preserves numeric XY/Z/UV/RHW tolerances and winding, and caps candidate/matching work; common-RHW admission now tests existence of a shared positive scale instead of anchoring to an arbitrary first corner. Dense/ambiguous geometry can still reject conservatively, and the separate triangle-count mismatch remains unresolved.

**Isolated candidate:** `build/refit/geometry-motion-candidate/A2FOExtensions.dll`, SHA-256 `854391520e2aea2a1d19e43b933be63febca91557233b1d2fb651e9806c6049c`; all198 exports pass. It relinks exact 13cc inputs with only the checker object changed. All933 source files and590 copied native link objects are attested in `provenance.json`; `build-isolated.py` records the reconstruction/link. The scene and mesh-scope original objects were reconstructed byte-identically from the frozen13cc source. **Do not replace this with a normal Root build:** Root concurrently contains newer two-group/diagnostic work that was not qualified for this flicker candidate. Other work has been preserved. Original failed regression and expanded test receipts live under `build/refit-geometry-motion/20261107T001414Z-3123568/` and `20261107T002054Z-3125919/`; independent review is beside the candidate.

**Current staged candidate and next action (takes precedence over historical status below):** Core85439152 is now installed **only** in `/home/tamsynn/NVMeData/Projects/A2FO-Manual-Renderer-Test`. Its55 critical inputs pass. User should launch **`Start-DX12.desktop`** and repeat the same moving ship; if brightness still flickers, compare a separate `Compare-DX12.sh` session using the same map/ship. Vulkan desktop acceptance is separate and remains open. Existing deduplicated baseline logs show submission and later parity refusals but do **not** prove the false pivot rejection caused the observed flicker. Visual confirmation is still required.

`build/refit/geometry-motion-candidate/runtime-smoke.json` binds actual-DLL checks on DX12 and Vulkan: each submitted the native scene mesh, remained alive100 seconds, captured successfully, cleaned its owned prefix (0), exited its sandbox (0) and reaped Xvfb. Neither static run logged a geometry refusal. Both were deliberately stopped (`game_exit=-15`), so these are not natural-exit or motion evidence. Captures and receipt/log snapshots are frozen under `validation/`; directional/point source probes remain intensity0 in these private trials. Current valid `--previous` receipts for further private revisions are:

- DX12: `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T030209Z-2684081-1793934129821624364/revisions/geometry-motion-dx12-Duel-1794011134901479799/receipt.json`
- Vulkan: `/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T024702Z-2681110-1793933222936563456/revisions/geometry-motion-vulkan-Duel-1794011286712606600/receipt.json`

The stopped-package lock, seven unchanged live originals, and hash-verified backup/update were checked. Backup (Core, package/readiness and before/after receipt): `/home/tamsynn/NVMeData/Projects/A2FO-Manual-Renderer-Test/candidate-backups/geometry-motion-1794011634620804351`. Stage receipt: `build/refit/geometry-motion-candidate/manual-stage-receipt.json`. Only the manual-copy Core and its metadata changed; helpers, configs, original EXEs/provider and the live original remain unchanged. Review resolved removable Python assert gates by making all safety checks unconditional; `staging-review.md` records the corrected script hash/optimized syntax proof. Lead fault-injected a post-update failure in a **private temporary fixture** and verified all3 files restored byte-identically (`validation/stage-rollback-check.json`). Abrupt process death still requires the preserved backup. No test/build remains running from this work, and no desktop input was automated.

## First successful user-operated desktop session (2026-11-07)

User reported success after the launcher fix. Fresh session `logs/20261107T000335Z-7a4bfbab-dx12-replacement` in the manual package confirms **DX12** replacement, factory/source admission, first owned mesh submitted through the explicit native scene API, and natural exit: process exit0, Wine launcher return0, owned cleanup0, evidence status finished. `manual-session-review.json` pins the process/evidence receipts. This is the first successful user desktop launch and scoped mesh-submission/natural-exit evidence. It does not establish long-session stability, complete visual/gameplay correctness or Vulkan desktop acceptance. Do not confuse the backend with Vulkan just because the earlier retry instruction suggested Vulkan. The older failed-launch report below is historical.

## Latest user report: manual launch did not appear to load

User double-clicked `Launch-Vulkan.sh`; no new session or fresh game log was recorded. Package hashes passed. The launcher's filesystem-only X11 check was incompatible with the observed environment: DISPLAY:0, no visible filesystem X0, but listening abstract @/tmp/.X11-unix/X0. It now supports that abstract transport by retaining the desktop network namespace only in that case; filesystem transport still unshares the network. Real home/input devices stay hidden. This is a corrected launcher incompatibility, **not proof of successful game startup**.

User should retry **`Start-Vulkan.desktop`** in `/home/tamsynn/NVMeData/Projects/A2FO-Manual-Renderer-Test` (DX12 equivalent also exists). It runs Konsole with --hold so errors remain visible. Both entries validated and executable. Early failures are now saved in `logs/prelaunch-error.json` and immutable timestamped copies even before a session exists. The new `--check-desktop` dry check passes the abstract endpoint/auth-path branch without opening a connection or window. Nine safety regressions independently pass; 55 package inputs pass. Original launcher/manifest/readiness are preserved under `launcher-backups/1794009336598668651`. Receipt: package `launcher-fix-receipt.json`.

No game or desktop window was launched by the assistant for this fix. Actual user retry and fresh errors/session logs are the next evidence required. Core13cc/Display3c/Renderere9 and all7 protected live originals remain unchanged. The earlier lifecycle checks below cover the common launcher path; they are not validation of the newly corrected desktop branch.

## Manual hardware-test milestone (latest task, complete)

The user asked to prepare the first controlled manual hardware session ahead of unfinished spotlight/shadow work. The package is now **ready for the user to launch** at `/home/tamsynn/NVMeData/Projects/A2FO-Manual-Renderer-Test`. It is a separate ~28 GB full game/prefix copy; all 7 protected live originals were rehashed unchanged. `package.json` pins 53 critical inputs; `readiness.json` binds the checks and guide. No desktop game was launched by the assistant.

User entry points: `Launch-Vulkan.sh`, `Launch-DX12.sh`, `Compare-Vulkan.sh`, `Compare-DX12.sh`, `Check-Package.sh`, `Stop-Test.sh`. Guide: package `README.md` / project `docs/refit-manual-test.md`. Start Vulkan from a normal desktop terminal, then manually start a Federation Duel Instant Action match. Launchers open the normal menu, with no map/autospawn/fog/camera/material fixture commands. Test Mediterranean/Venture, zoom/pan/select/build/combat and normal exit. Comparison uses the same native Display and observation/capture settings, differing only in SubmitMeshes0 versus1; it is not a legacy-backend rollback. Return to the original game shortcut for rollback; no original installation was altered.

Core13cc, Display3c and Renderere9 remain unchanged. Four manual children inherit FO4 Nexus and explicitly reject Refit menus. `LiveParts=1` and source capture flags are enabled; all headless fixtures, OdfLights and CaptureWorldLights are off for this first geometry/gameplay pass. Child settings are seeded valid, windowed1280x720 with nonzero camera height. Do not replace these with the earlier zero-intensity artificial lighting presets.

Launcher/evidence/harness sources: `tools/refit_manual_launcher.py`, `tools/refit_manual_evidence.py`, `tools/check_refit_manual_package.py`. Runtime uses the same system Wine11.13 with a private prefix and hidden real home, hardware GPU nodes, NVIDIA ICD, explicit user X11/Xauthority and optional Pulse socket; no whole session-runtime bind or input-device access. Stop signals only a PID-fd-bound verified wrapper, requests its private session to stop, and cleans only its own prefix. Session lock prevents overlap. Symlink-safe writes protect locks/markers/preflight logs. No global desktop shortcut or system package was installed.

Completed checks:

- Package integrity and shell syntax pass; six meaningful file/process/environment safety regressions pass, independently rerun by reviewer.
- Headless hardware preflight enumerated the NVIDIA GTX1660Ti; this proves device availability only, not game rendering. Package `logs/gpu-preflight.json` records no game/window launch.
- Vulkan lifecycle150s: `build/refit-manual-package-checks/vulkan-1794008047973544792/receipt.json`; DX12 lifecycle150s: `dx12-1794008248208690975/receipt.json`. Both pass factory/source admission, fixtures absent, concurrency rejection, owned Stop, own-prefix cleanup0, wrapper0 and Xvfb reaping. Both independently audited. DX12 additionally binds unchanged launcher/helper/Core hashes before/after.
- The direct-map headless world was empty/fogged without Instant Action setup, so these runs prove launcher lifecycle, not gameplay or active mesh coverage. The deliberate Stop gives Wine return1 / process stop130; it is not a natural-exit test. Prior controlled two-ship scenes provide the narrower mesh/lighting evidence below.
- First Vulkan receipt `vulkan-1794007774584079793` is preserved: it exposed the old Stop/bwrap termination bug, fixed before repeats. Never treat it as a passed final launcher.

No game, test or build remains running. Fresh session logs go to package `logs/<session-id>/after/`, plus `process.json`, `wine-output.log`, `launcher.log` and `evidence.json`. A deliberate Stop is labelled stopped, not a renderer acceptance. `TEST-REPORT.txt` is ready for the user. **Next action is the user's manual desktop test; hardware rendering, populated gameplay, visual correctness, stability and normal exit remain unaccepted.** Never operate their desktop or synthesize input. Keep the original 2% usage-stop instruction below.

## Current continuation status (authoritative)

User last reported **5% remaining** and asked to continue. The 2% stop instruction still applies; Codex cannot read the account meter and needs the user's stop message. No live installation was changed.

Latest staged Core: `build/refit/odf-native-colour-guard/A2FOExtensions.dll`, SHA-256 `13cc3d622698319daef3faa7d5a301723a25fe08a9594ae999f6ce5d8be2cf0c`. Build, unchanged input hashes and 198 exports pass. Its `source/` plus `source-snapshot.json` preserve all 933 attested Core files (7,117,875 bytes), including `.inl`. Helpers remain Display `3c317ae7…` and Renderer `e9a78291…`; full unchanged vendor source stays in the pinned baseline bundle.

New guard: enabled `a2foLight*` sources require explicit native `lightSource=0` **and no effective legacy `lightColor`**, including inherited values. Recovered FleetOpsHook callback creates a native point from `lightColor` alone; the colour-only case is source evidence, while installed-image observation covered the combined old-key probe. The guard is conservative; arbitrary native source ownership is not fully qualified. Strict parser/event/store checks pass in `build/native11-prefixed-light-checks/20261106T165039Z/receipt.json`.

The private probe now uses quoted type/colour/direction strings and `basename="fed_venture"`; the missing-model placeholder is gone. Source is at `(0,0,0)`, receiver at `(300,0,0)`, point full-strength distance 1000/fade 500. Both same-candidate captures in a pair differ only in intensity 3 versus0.

| Completed private colour control | Receiver red delta >=20 pixels | Mean red increase | Evidence directory under `build/refit/` |
| --- | ---: | ---: | --- |
| Vulkan directional | 4,828 | 40.0183 | `light-controls-vk-dir-1793984111079143360` |
| DX12 directional | 4,905 | 40.4362 | `light-controls-dx12-dir-1793984389096129977` |
| Vulkan point | 1,976 | 9.3122 | `light-controls-vk-point-1793984626764814072` |
| DX12 point | 1,975 | 9.4276 | `light-controls-dx12-point-long-1793985094097082647` |

Each directory has hashed `analysis.json`, immutable preparations/receipts through its referenced trial paths, on/off/difference crops and a separate `independent-review.md`. Review independently rehashed inputs and recomputed pixels. The six Vulkan/directional observations were 100 seconds; the DX12 point pair used 150 seconds. Armada was first seen at 2 seconds in all eight accepted runs. The ROI is `[1035,425,1155,600]` at1280x1024. Point light affects left/bevel surfaces, unlike the broad directional response. This is static Wine/software-GPU colour response, **not** movement, range, spotlight, geometry shadows, physical GPU, full material/image parity or gameplay acceptance. Combined four-control results: `build/refit-odf-light-control-results.json` (directional-only summary is also retained).

**All four pairs completed, passed analysis, and were independently audited.** No test or build remains running. The successful DX12 point retry used the same Core/probe and only extended the bounded launcher observation to 150 seconds. Its manager is `complete`. Latest completed receipts for any future runner are Vulkan `revisions/vk-point-point-0-Duel-1793984730651080048/receipt.json` and DX12 `revisions/dx12-point-long-point-0-Duel-1793985248101087004/receipt.json` under the trial roots below. Both private probes currently retain intensity 0; do not mistake that negative-control setup for a default showcase preset.

Two preserved non-product failures:

- First Vulkan manager `failed: fresh ODF source absent` because zero-intensity sources are intentionally omitted from the active light vector. Hashed logs show owned sources0 and successful mesh submission. Its analyzer records `corrected_zero_filter_assertion=true`; original failed summary is unchanged. The manager is now intensity-aware.
- First DX12 point attempt `light-controls-dx12-point-1793984871524001538/summary.json` did not finish loading within 100 seconds: first Armada presence was79 seconds and capture was still the loading screen. No probe class or point-lit draw occurred. Preserve it as incomplete acquisition, not a negative point-light result. The current retry changes only the observation window to 150 seconds.

Reviewed orchestration: `build/refit/run_light_controls.py` archives exact effective ODF/config and manager source, checks modules/pre/post hashes, uses existing isolated runner, and serializes intensity 3 then0. SIGTERM/SIGINT stops further scheduling and lets the current owned runner finish cleanup. Identify only the exact manager/trial; no broad Wine kills or desktop input.

**Frozen diagnostic capsule is complete:** `build/refit/odf-lighting-checkpoint/manifest.json`, SHA-256 `c616b7e4819d94ecaf293abe98cc0e7152fa032c2e53aa9b004934780f85f3c5`. Every copied file was rehashed after creation: **1,179 copied files plus README and manifest**, including all 933 Core source files, exact DLLs, four independently reviewed controls and the incomplete loading attempt as nonqualifying history. The copied files plus README total 48,767,021 bytes (manifest excluded). The unchanged full vendor baseline is an external path/hash reference, not duplicated. The capsule includes a pre-freeze copy of this handoff; this working file was subsequently advanced to record the completed freeze. Do not rewrite the capsule. Final protected-file check `build/refit-odf-final-live-protected-hashes.json` confirms all 7 live hashes unchanged. No Wine prefix or live tree was copied.

Remaining work is the open checklist: spotlight contract/shaders, complete current caster/frame ownership and geometry shadows, broader material/mesh/effect coverage, recovery/performance/multiplayer, physical GPU and independent gameplay acceptance. The next spotlight source map and staged compiler proof are recorded in `docs/refit-spotlight-next-step.md` and `build/refit/shader-tools/toolchain-receipt.json`. The compiler/validator are project-owned; no system package was installed. `baseline-sod-check/receipt.json` and `committed-output-comparison.json` under that toolchain directory prove successful unchanged-source SPIR-V/DXIL compilation, byte-identical to both committed shader arrays, with vendor files untouched. No new ABI/shader implementation has begun.

The older detailed sections below retain earlier candidate/evidence history. **This section takes precedence for newest Core, current private configuration and next commands.**

## User objective and stop instruction

Integrate the native DX12/Vulkan renderer from Armada II Refit into **A2FOExtensions**, for Fleet Ops / FO4 Nexus. Retain FO's HUD. **Do not integrate Refit's in-game UI.** Complete the real feature and acceptance checklist, rather than adding arbitrary numbered stages.

The user wants continued work and ideally completion today. Their latest instruction is: **when remaining usage reaches 2%, stop and write a handoff for Claude.** Codex cannot read their account's remaining usage percentage and told them so. Do not claim to monitor that meter. If the user says stop, stop new implementation/testing, preserve current evidence and finalize this file. This file was created in advance so the handoff is already usable.

## Safety and preservation

- Workspace: `/home/tamsynn/A2FOHookExtensions`. It contains extensive pre-existing dirty and untracked work. Do not reset, clean, revert or discard unrelated changes.
- Read `/home/tamsynn/.codex/AGENTS.md` and the user-supplied local instructions. **Never synthesize input, focus/activate desktop windows, or automate mouse/keyboard.** Only headless diagnostics and manual user instructions are allowed.
- All current game tests use owned Xvfb displays and private Wine prefixes inside a bubblewrap sandbox. No user desktop capture or input automation. The sandbox hides input devices, hardware graphics, the real home, network and session bus. Software GPU evidence is not physical-GPU acceptance.
- Do not write to the live installation for these experiments. No live deployment has occurred. Preserve the seven protected hashes in `build/refit-live-protected-hashes.json` and the frozen checkpoints.
- Do not delete shared X sockets. Earlier test cleanup lacked sufficiently unique socket provenance; this was corrected. Use explicit unused display numbers and initialized **owned** private prefixes.
- Do not change production source while a corresponding build is running. The source/build tools reject drift. Serialize source edits and dependent builds.
- Do not dump complete receipts: some contain thousands of PE entries or prefix-file hashes. Print selected scalar fields only.
- No memory files were updated. Do not silently edit the personal memory system.

## Stable achievement before the current lighting work

Baseline evidence bundle:

`build/refit/native-scene-pixel-checkpoint/`

It contains 2,788 recorded files, all 1,660 files from its final vendor source audit, 921 attested Core source files, DLLs and receipts. Its README and `checkpoint.json` define exact evidence scopes.

Baseline final artifacts:

| Component | Staged location | SHA-256 |
| --- | --- | --- |
| Core | `build/refit/pixel-control-candidate/A2FOExtensions.dll` | `c20cfef10fe7010da90b115a64207089531441ce30de10a3b8f6dc4073464b79` |
| Display | `build/refit/raster-parity/A2FORefitDisplay.dll` | `3c317ae7…` — read full hash from receipt/checkpoint |
| Renderer | `build/refit/white-fallback-lazy/A2FORefitRenderer.dll` | `e9a7829199f426dfe51639b992b8f7622692eddf5fbc4afe8d186769a3dbbfca` |

- Both Vulkan and DX12 rendered actual eligible FO ship geometry through the explicit native scene API, retaining the stock FO HUD.
- Both final normal-material runs survived 100 seconds and produced captures. Summary: `build/refit-normal-material-results.json`.
- Same-candidate live white/black controls on the earlier Renderer `2726ffeec1b5b191ac6f406d155ba084c69c359fc821c038be7dec35e0adb87b` proved visible ship-pixel contribution on both backends. Summary: `build/refit-live-pixel-controls.json`; 5,053 bright pixels in white and zero in black in the selected ROI. This is not whole-image or gameplay parity. The newer Renderer only changes fallback allocation/validation ordering and passed the same GPU fixture plus normal-material runs. Keep these artifact scopes distinct.
- Fixed an actual API2 bug: an empty explicit part texture promised white but sampled GPU's black dummy. The Renderer now owns a cached 1x1 white texture per content epoch, creates it only when needed after all part/resource validation, and clears it on reset/shutdown. The same new fixture fails old Renderer `ef1dd423…` and passes the corrected and final versions on both backends.
- The older `build/refit/first-live-owned-scene-checkpoint/` is historical partial source evidence. Its source copy omitted four local vendor files. `source-coverage-notice.json` records this; the newer full bundle includes them. Do not rewrite the original checkpoint to expand its proof.
- Full gameplay, hardware, performance, multiplayer, complete materials/effects, ODF spotlights and geometry shadows are **not accepted**. See `docs/refit-renderer-checklist.md`.

## Renderer architecture already in place

Imported upstream: `third_party/armada2-refit-native`, commit `1e2a7d2fc213282ef7f9ef00e1b11011da23ac68`, MIT, manifest preserved. Do not confuse this with the separate older menu-renderer import.

- `renderer/refit/host.h`: stable ABI1 and ABI2. ABI2 table is 60 bytes on x86; explicit owned SOD/model/texture/part prepare/draw, directional and point lights. It has **no spotlight cone or complete shadow-caster contract**.
- `core/refit_scene.cpp`: opt-in `[Refit]LiveParts=1`, `[Refit]SubmitMeshes=1` (SubmitMeshes defaults off). Uses authoritative post-animation observations and immutable model/texture owners.
- Supported replacement is narrow: standard legacy route, pass 0, one visible opaque authored group, supported SOD 1.7 through 1.93. Unsupported draws remain native submissions through Display.
- `core/refit_mesh_capture.cpp` and paired assembly bridges retain the original native mesh body exactly once and preserve calling/FP state. Full native-body/profile checks qualify the dispatch.
- Draw descriptor is stable 84 bytes; separate Caller2 is 12 bytes and captures ESI. Actual DIP caller and workspace/ranges are checked. Generic HUD/RHW calls cannot claim arbitrary geometry ownership.
- `renderer/refit/geometry_parity.*` compares the complete visible projected authored triangle multiset with the actual native RHW indexed stream. It checks UVs, depth, winding and one common positive reciprocal-W scale; requires unlimited native triangle budget and rejects clipping/degenerates/ambiguity. The first Mediterranean group has 80 authored faces but 38 emitted visible faces. Do not replace the count check with a blind `<=` rule; the engine also has a triangle quota mechanism.
- Native canonical lighting-material identity/name/parameters and texture slot0 are rechecked at draw. Native `Storm+0xa8` override is rejected. Field mapping is ambient `+0x18`, diffuse `+0x24`, specular `+0x30`, power `+0x3c`, model `+0x40`, self-illumination `+0x44`, team `+0x45`; qualified constructors/readers prove it. The modern shader does not yet consume all these coefficients.
- Native self-illuminating materials have a base pass plus an emissive pass. Replace only the checked base; set modern self-illumination strength to zero; retain native emissive drawing. Do not double-add emission.
- Raster2 query supplies the RHW-to-scene pixel-centre difference. Camera and geometry comparison use that adjustment.

## Current work: fresh ODF colour lights

Changes since the stable baseline are **not yet frozen as a completed feature**:

- `core/refit_scene.cpp/.hpp`: new `[Refit]OdfLights=1`, default off. Requires the existing `[Native11]CaptureWorldLights=1` class producer. It samples ODF lights fresh at mesh preparation and **again at the actual DIP**, never using the previous frame's `latest_lights()` publication.
- `core/renderer_ingame_shadows.inl`: new Refit RAII view scope in the existing checked world wrapper, after capture begin and owner-thread light-reader admission. It ends before native capture end. No new hook or native address was introduced; the original world renderer still executes once.
- `renderer/refit/view_scope.hpp`: externally owner-serialized, bounded 16-level view ticket stack. Only an owned outer view is usable. Nested/unowned views invalidate their parent for the rest of that view; overflow is non-usable; mismatched exits permanently fault ODF replacement until restart. Tickets are private synchronous view IDs, **not** native Collector completed-frame IDs.
- `renderer/refit/lights.*`: transactional ABI2 adapter appends directional/point sources with preserved colour/intensity/falloff and ID ordering; rejects spots/unknown shapes and limits. It compares every effective world-light field for freshness. Shadow requests are recorded/logged but not rendered.
- Meshes retain source snapshots, view ticket, original Storm registered lights and presentation. The DIP rechecks the view, lifecycle, class generation, fresh world-light frame, **fresh Storm ambient/team and registered roster**, plus existing material/geometry guards. The extra Storm recheck fixes a concrete review finding: resampling with saved ambient alone would not detect changed native lighting.
- `Makefile`: adds `renderer/refit/lights.cpp`.
- `core/refit_scene_test.cpp`: existing explicitly admitted private fixture can read `HeadlessLightProbe=1` and build `refit_light_probe` instead of its first Venture. No new native call or desktop/input operation.
- `build/refit/build_core_candidate.py`: source audit now includes `.inl` files as well as C/C++/assembly/Rust/headers. Earlier source counts were a smaller scope.

### Important discovery and correction

The first probe used legacy `lightSource=1`. **Armada itself already creates a native point light for this command**, independently of the new ODF sampler. Its registered point's falloff was exactly the probe's 1000/500. Appending another ODF light would double-light the object. The quoted legacy colour probe also produced invalid native RGB, so validation correctly retained the original draws. Do not weaken the light validator to admit that garbage or claim a renderer shader failure.

New namespaced commands avoid this native side effect:

```text
lightSource = 0
a2foLightSource = 1
a2foLightType = "directional"
a2foLightColor = "1 0 0"
a2foLightIntensity = 3
a2foLightDirection = "0 -1 0"
a2foLightFalloffStart = 1000
a2foLightFalloffRange = 500
a2foLightCastShadows = 0
```

`renderer/native11/light_config.hpp/.cpp` now request 20 names: the original ten followed by all ten `a2foLight*` aliases. Existing legacy parsing is preserved when no prefixed key is present. Any known prefixed key selects the prefixed family and requires `a2foLightSource`; enabled prefixed sources also require explicit legacy `lightSource=0`. A trailing `LightConfig::legacy_native_source` marks enabled legacy-family sources. Refit rejects those at preparation and draw to avoid unqualified duplicate native emission; the old Native11 pipeline keeps its previous parsing behaviour. The latest guard additionally rejects any effective legacy colour for enabled prefixed sources; inherited legacy type/falloff values do not override the selected prefixed family.

### Current candidate / latest experiment

Earlier explicit-native-disable Core (historical; newest candidate is listed above):

`build/refit/odf-explicit-zero/A2FOExtensions.dll`

SHA-256: `75b71311228fbe1e256951e3f17abdc7c87aab5aa4be59806c8b44f67d66cd82`

Its build/source attestation and 198-export check passed. Display and Renderer remain the baseline `raster-parity` and `white-fallback-lazy` versions; no Rust renderer/shader change was needed for ABI2 directional/point lighting.

The earlier **Vulkan namespaced directional probe completed** on Core `aa7ad2d587a07da4af3c60c752fcd52d3317c2b31eba9d6ec84dd15a3e598b9c` (`build/refit/odf-namespaced/`). Receipt:

`/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T024702Z-2681110-1793933222936563456/revisions/odf-prefixed-Duel-1793981021355761440/receipt.json`

It remained alive for 100 seconds, captured successfully, and cleaned up its owned Xvfb. However, **it did not qualify the ODF bridge**: `refit_light_probe` was created and all requested `a2foLight*` names were cached, then `Native11 class completion refused: copied class fields unavailable`. The world-light class barrier consequently stayed unavailable, and the Refit ODF path correctly retained native submissions. No visible ODF lighting contribution may be claimed from this run. Use `current-run-logs/A2FOExtensions.log` lines 1226–1242, not preceding logs.

The next diagnostic added the existing `LightClassStore::update` error string to Core's rejection log, **after releasing the class lock**, without relaxing validation. File: `core/native11_light_classes.cpp`. Candidate `build/refit/odf-class-diagnostic/A2FOExtensions.dll` built successfully with unchanged source digest and 198 exports: SHA-256 `e9118e6f993998fefefc6b6bfe545efd7acc9fde9fdd179b5144683ed604720f`.

Its private Vulkan revision `odf-class-diagnostic-Duel-1793981354756033609/receipt.json` completed, remained alive at 100 seconds and captured successfully. It exposes the actual rejection: **`invalid lightType`**. The first prefixed probe wrote unquoted `directional` and unquoted vectors, unlike the earlier legacy probe's correctly quoted strings. The next private probe uses the quoted example above; backup/hash receipt is `revisions/odf-quoted-setup-1793981493434164706/receipt.json`. Do not claim that correction works until the next run proves it.

No 16-field event cap was found: copied light events permit 128 fields. Do not increase limits speculatively. `rust/a2fo_core/src/stage8_core_odf.rs` returns a successful string query immediately, and only tries scalar fallback when the string query failed; it does not overwrite a successful copied string.

Independent review found no confirmed transaction/prepare/draw defect, but noted that a prefixed enabled source with an **absent** legacy `lightSource` assumes an unverified native default. That gap is now corrected: enabled prefixed sources require explicit `lightSource=0`, preserving legacy and disabled-source parsing. Strict parser/copied-event/class-store checks passed in `build/native11-prefixed-light-checks/20261106T161128Z/receipt.json`. The corrected Core candidate above passes build/source/export checks. All private probes already specify zero.

A **quoted-string Vulkan probe on this corrected candidate completed successfully at the source/submission boundary**:

`/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T024702Z-2681110-1793933222936563456/revisions/odf-quoted-Duel-1793981530732493149/receipt.json`

The process remained alive for 100 seconds; capture exit was 0; its Xvfb was reaped. The paired `current-run-logs/A2FOExtensions.log` has **zero class completion refusals**, one owned light source at frames 300 and 600, `fresh ODF directional/point colour lighting included in scoped packet`, and the first owned mesh submitted through the native scene API. The private capture shows a strongly red Mediterranean and the retained FO HUD. This is source/submission plus an unpaired visual observation, **not a same-candidate causal light-response control or complete visual acceptance**.

The capture also shows a white **`Error!` model placeholder** at the light-source craft. The probe includes `fed_venture.odf` but has no explicit `basename`, so its own ODF name has no matching SOD. Existing Nexus `fed_mediterraneanY.odf` demonstrates the native `basename = "fed_mediterranean"` convention. Before paired controls, archive/hash and add `basename = "fed_venture"` to the owned probe; verify it removes the placeholder. Do not misidentify this fixture asset error as a Vulkan/backend crash. The second Mediterranean mesh is the actual qualified submission.

No game test or build remains running from these experiments. The latest completed Vulkan receipt above is the next runner's `--previous`. DX12 still has only the earlier baseline and needs separate ODF qualification.

Expected evidence before calling the bridge live-qualified:

1. `refit_light_probe` created successfully; cached class fields include `a2foLight*`.
2. World sampler reports one enabled source.
3. Core reports fresh ODF colour light included and successful native scene submission.
4. No registered-light garbage / duplicate native point or freshness rejection.
5. Capture visibly responds to red light compared with a zero-intensity control on the **same candidate**.
6. Repeat directional and point controls independently on Vulkan and DX12. Spotlights and shadows stay open.

## Private game copies and runner

Vulkan trial:

`/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T024702Z-2681110-1793933222936563456`

DX12 trial:

`/home/tamsynn/NVMeData/Projects/A2FO-Refit-Startup-Trials/20261106T030209Z-2684081-1793934129821624364`

Child mods are `FO4 Refit Live Vulkan Test` / `FO4 Refit Live DX12 Test`, both inheriting FO4 Nexus. Helpers live in child `modules/refit/`. The Data Core DLL is separate in each copy. Native settings were seeded from each copy's own valid FO4 settings, with `firstRun=False`, `disable_bump=True`, a valid camera height (~720.556). Setting firstRun false on the newborn all-zero child profile is invalid; never do that.

The controlled revision runner is `build/refit/revise_trial.py`. It verifies preceding DLL hashes, original EXEs/provider, canonical trial ownership and absence of an active sandbox before revising. It archives previous DLLs, config, logs and native helper logs; launches only its own Xvfb/bwrap/Wine; observes for 100 seconds; captures its own display; terminates only its own test environment.

```sh
python3 build/refit/revise_trial.py \
  --trial ABSOLUTE_TRIAL \
  --previous ABSOLUTE_PREVIOUS_RECEIPT \
  --core build/refit/odf-native-colour-guard/A2FOExtensions.dll \
  --label UNIQUE_LABEL
```

Use `current-run-logs/` and `native-logs/` paired with a revision receipt. Older plain `logs/` directories sometimes contain the preceding run. The corrected runner records current hashes explicitly. Parallel full game captures once timed out; preserve those failures and run serially with the current 30-second capture timeout.

Vulkan current config has `OdfLights=1`, `HeadlessLightProbe=1`, `HeadlessSceneTest=1`, `HeadlessMaterialControl=none`, and `[Native11]CaptureWorldLights=1`. The newest probe ODF is at child `odf/ships/refit_light_probe.odf`, includes `fed_venture.odf`, and now uses the prefixed example above. Preparation/backup receipts:

- `revisions/odf-light-setup-1793980041293927191/receipt.json`: original legacy probe/config setup.
- `revisions/odf-prefixed-setup-1793981021151873647/receipt.json`: exact old/new ODF bytes and hashes for the namespaced correction.
- `revisions/odf-quoted-setup-1793981493434164706/receipt.json`: quote correction; this is the current exact probe ODF, still without the explicit model basename.
- Latest completed revision: `revisions/odf-quoted-Duel-1793981530732493149/receipt.json`, Core `75b71311…`; fresh source and native scene submission succeeded. Earlier `odf-registered-Duel-1793980376555307153` deliberately refused invalid legacy registered light data.

DX12 has not yet been changed for the new ODF probe. Its last accepted baseline is `revisions/normal-materials-Duel-1793976547565249221/receipt.json`, Core `c20c…` and Renderer `e9a7…`. Vulkan's equivalent old baseline is `normal-materials-Duel-1793976408597693520`.

When preparing a next experiment, first verify it is stopped, archive/hash the current child config and probe ODF, record the changes in a new preparation receipt, and use the latest completed revision as `--previous`. Do not silently rewrite old receipts or put changes in the live mod.

## Current checks

- `build/refit-light-adapter-checks/20261106T153852Z/receipt.json`: strict host adapter tests passed; transactional conversion, limits, IDs, unsupported spot, deep copies and freshness fields.
- `build/refit-view-scope-checks/20261106T154210Z/receipt.json`: strict host nesting/ownership/overflow/fault tests passed, no native calls.
- `build/refit-direct-light-checks/private-20261106T155103Z/receipt.json`: strict i686 full GPU fixture passed on both backends. Directional positive/opposite, point falloff, zero intensity and unsaturated intensity scaling; Display `3c31…`, Renderer `e9a7…`.
- `build/native11-prefixed-light-checks/20261106T160028Z/receipt.json`: config, copied event and class-store tests passed; namespace rules, legacy compatibility, alias parity, malformed/duplicate input and transactional failure.
- `build/native11-prefixed-light-checks/20261106T161128Z/receipt.json`: the same strict checks pass with the additional enabled-prefixed-source requirement for explicit native `lightSource=0`, including a transactional missing-key regression.
- Baseline GPU/ABI/geometry/material/white-cache tests and exact commands are in the frozen native-scene bundle.

Core builds: `python3 build/refit/build_core_candidate.py UNIQUE_LABEL`. This runs the appropriate make target, records source hashes before/after, refuses drift, stages under a fresh directory and verifies 198 exports. Do not reuse an existing stage label. The old `geometry-parity-pre-rhw-diagnostic` archive has an explicit invalid source-copy notice and must not be treated as a qualified candidate.

## Next implementation after the probe

- First correct the **private fixture only**: add explicit `basename = "fed_venture"` with backup/hash receipt, then verify the source craft no longer displays the missing-model placeholder. The production Core candidate remains `75b71311…`.
- Complete same-candidate live directional/point intensity controls on both backends, fix concrete failures, then update the checklist and freeze a new bounded evidence bundle.
- Add meaningful integration guard evidence for changed/moved/removed light sources as needed; existing pure tests cover comparisons and the world reader already double-reads roster/poses. Do not claim movement acceptance from static controls alone.
- Spotlights require a new explicit cone-capable renderer contract; ABI2 cannot silently treat them as points.
- Geometry shadows require complete current caster inputs and a suitable frame boundary. The old Native11 actual-geometry shadow pass exists, but per-mesh Refit ABI2 does not have a complete caster/shadow contract. Do not use previous-frame lights/casters as if current.
- Broader material coefficients, mesh routes, content/animation/LOD/effects, complete Nexus UI/menu integration, device recovery, physical GPU, performance and multiplayer remain open. The main checklist is authoritative about acceptance, not historical stage numbers.

## Existing source references

- `core/native11_world_lights.*`: owner-thread, exact-profile sampling; immutable snapshots with class generation/lifecycle.
- `renderer/native11/world_light_capture.*`: complete GameObject roster, copied poses/class owners, double-read mutation rejection.
- `renderer/native11/world_lighting.*`: source-ID ordering, world transforms, max eight active ODF lights.
- `core/native11_capture.cpp::end`: publishes old Native11 light snapshots **after** rendering; `latest_lights()` is not a fresh in-progress-view source.
- `renderer/native11/renderer.cpp`: actual-geometry shadow rendering; `observed_world.cpp` / `world_service.cpp` require complete caster roles.
- `tools/check_refit_scene_profile.py`: generates exact Root4300/FO4c884 source regions with relocation normalization. Never guess addresses or transfer stock-GOG offsets unqualified.

No full gameplay completion, native Windows verification or live deployment should be inferred from any of the above.
