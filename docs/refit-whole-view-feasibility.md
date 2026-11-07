# Full takeover: per-mesh replacement vs upstream whole-view rendering

Claude, 2026-11-07. This is a read-only static analysis, and **a decision for the project owner**. No
upstream stock hook was enabled, nothing was patched, and no address was transferred into A2FO code.

## The two architectures

- **A2FO today (Codex's design):** an explicit host with no stock hooks. Core lets the engine run its
  own frame and replaces individual qualified mesh draws through Renderer ABI2/3. Everything else
  stays native: effects, sprites, sky, planets, transparency. Each new category needs its own
  replacement path, profile and qualification. That is the source of the 3–6 week effects estimate.
- **Upstream render3d (stock Armada II 1.1):** takes over the whole 3D view
  (`Armada_RenderAllOurStuff` at `0x598110`) with eight modules (hulls, sky, particles, ordnance,
  effects, world, overlays, specials). The upstream README says that by default, nothing in a whole
  match is left to the engine. It also brings HDR, 4× MSAA, bloom by energy, and shadows from the
  current frame's hulls.

## Finding: Fleet Ops runs the stock Armada II 1.1 engine image plus a patch section

| | GOG Armada2.exe 1.1 (upstream profile) | FO `Data/ArmadaL.exe` (`4300ab71`) | FO 4.0 `ArmadaL.exe` (`c8846226`) |
|---|---|---|---|
| PE timestamp | `0x3c4c76bd` | **`0x3c4c76bd`** | `0x6a7d02b1` |
| Image size | `0x3fa000` | `0x405000` | `0x404000` |
| Sections | `.text … .reloc` | identical layout up to `0x3fa000`, **plus `.test`** (42 KB) | same, `.test` 39 KB |

`tools/refit_profile_portability.py` checks every byte-verified symbol in
`profiles/gog-1.1/render3d*.toml` against a target executable, at the same address or by
pattern count. Evidence: `build/refit-whole-view-feasibility/portability.json`.

- **Root `ArmadaL.exe`: 133/133 match, 0 misses. FO 4.0 `ArmadaL.exe`: 133/133 match, 0 misses.**
- This covers all eight modules plus core: core 35, effects 19, hulls 23, ordnance 17, overlays 18,
  world 8, specials 6, particles 4, sky 3.
- A further 172 entries (globals, vtables, fields, calls) carry no byte checks and cannot be verified
  statically. Because the code layout is identical, they are likely at the same addresses, but that
  is unproven.

## Where Fleet Ops diverges (the real porting work)

The `.text` section contains **163** rel32 `call`/`jmp` instructions into FO's `.test` code
(`build/refit-whole-view-feasibility/fo-test-redirects.json`). **13** of them are within 0x200
bytes of an upstream render3d symbol, so FO modifies behaviour *inside* functions that upstream
replaces:

| Area | Upstream symbols nearby |
|---|---|
| Shields | `effects:fn_drawshield` |
| Explosion / build fragments | `effects:site_explode_fragment` (×3), `effects:site_build_fragment` |
| Construction visuals | `effects:fn_construction_object_render` |
| Borg evolver | `hulls:fn_evolver_ri` (×3) |
| GameObject render (core hull path) | `hulls:fn_go_ri`, `hulls:fn_go_render`, `particles:site_billboard` (×2) |
| Sprites / footprints / placeholder | `effects:site_sprite`, `overlays:fn_footprints`, `hulls:fn_placeholder_render` (×2) |

If upstream modules replace these functions, FO's changes there would be bypassed. Each area needs
FO's `.test` routine read and either its behaviour ported into the module, or the module set to stand
down for that object class. Upstream already stands down per class when it cannot draw something.

**Not measurable statically:**
- Runtime patches by `FleetOpsHook.dll` (Delphi, protected original) and by A2FO Core's own modules.
  Upstream re-checks stock bytes at hook time, so conflicting patches would make a module stand down
  rather than crash, but which modules survive must be measured in-process.
- FO content differences: ODF classes, SOD variants, Nexus mechanics.

## What this means for the estimate

| Route | Effects/sky/planets/particles | Risks |
|---|---|---|
| Per-mesh (current) | Build each category from scratch in A2FO: 3–6 weeks for effects alone, plus shadows (2–3 weeks) | Slow, but every step is fully qualified and parity-checked |
| Whole-view (upstream modules on an FO profile) | Already implemented upstream. The work is an FO profile, in-process hook-survival checks, the 6 divergence areas, A2FO HUD/mechanics interplay, and qualification | Reverses the "no stock hooks" decision. Needs runtime verification that FleetOpsHook/A2FO hooks and upstream hooks coexist, and it changes what "parity" means: upstream looks better and does not match native |

A plausible middle path: keep the explicit-host per-mesh path for ships, now covering two-group,
colours and spots, and evaluate **individual upstream modules** that are largely FO-untouched (sky,
world/planets/nebulae, ordnance, particles) behind explicit host admission, one at a time.

## Runtime result: in-process hook-site audit (done)

Opt-in `[Refit]AuditUpstreamSites=1` (Core `build/refit/upstream-audit/`, `1f937396…`; site table
generated into `core/refit_upstream_sites.hpp`) re-reads every fixed-address byte-checked upstream
site in process memory once a match renders. At that point FleetOpsHook and every FO4 Nexus A2FO
module have installed their patches. It is read-only and installs no hook. Private Vulkan trial,
revision `upstream-audit-Duel-1794051963137984413`:

| Module | Stock sites | Patched at runtime |
|---|---|---|
| core | 34/35 | `fn_new` `0x652710` (engine operator new: an allocator hook) |
| effects | 18/19 | `fn_create_hit` `0x4743b0` (likely A2FO directional shields) |
| hulls | 21/23 | `fn_craft_ri` `0x4c8630`, `fn_go_render` `0x4d54e0` |
| ordnance | 16/17 | `fn_prj_odf` `0x6593a0` (projectile ODF; likely A2FO weapon modules) |
| overlays | **18/18** | none |
| particles | **4/4** | none |
| sky | 2/3 | `fn_go_render` (shared) |
| specials | 5/6 | `fn_prj_odf` (shared) |
| world (planets, moons, asteroids, nebulae) | **8/8** | none |
| **total** | **126/133** | |

`fn_go_render` (RVA `0xd54e0`) is A2FO's **own** Native11 craft-preparation hook
(`core/native11_craft_preparation_profile.hpp`, `entity_prepare_dispatch`), the capture path the
per-mesh renderer itself depends on. Attribution by RVA search of A2FO source:
- `fn_create_hit` is used by `modules/A2FODirectionalShields` and `A2FOAlwaysShowShields`
  (`kCreateShieldHitRva`).
- `fn_prj_odf` is used by `modules/A1Compat` and `A2FOAnimations` (`cPrjID::GetOdfName`, a 5-byte
  `jmp` patch).
- `fn_new` (operator new) is referenced by `modules/A2FOEditMenu`, `A2FOHybridBuild` and
  `A2FOSOLMaps`.
- Only `fn_craft_ri` (`0x4c8630`) is unattributed in A2FO source. It may be `FleetOpsHook.dll`.

**6 of the 7 runtime conflicts are A2FO's own modules.** Conflicts with A2FO's own code can be coordinated, unlike conflicts with a
third-party DLL.

**Implication:** world, particles and overlays have no runtime conflicts at all. Effects, ordnance,
specials and sky each have one shared conflict point. Hulls has two, both tied to A2FO's capture
hooks, which is expected because the per-mesh path already owns hull drawing. A module-by-module
admission of upstream **world** (planets/nebulae), **particles** and **overlays** behind explicit host
control looks like the lowest-risk first step if the owner chooses the whole-view direction.

## Suggested next step (cheap, decisive) — superseded by the runtime audit above

An observation-only in-process check in a private trial, with no hook installed. At runtime, after
FleetOpsHook and A2FO Core load, re-read the upstream symbol check bytes and report which modules'
hook sites are still stock. That tells exactly which upstream modules could be admitted without
fighting another patch.
