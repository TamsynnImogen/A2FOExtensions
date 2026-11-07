# armada2-refit-native: provenance and local adaptations

Created 2026-11-07 by Claude. This answers checklist §1 items 1–2 (upstream diff, local patch
hashes, licence provenance) for `third_party/armada2-refit-native/`. It is a read-only audit:
nothing in the vendor tree was changed. Per `A2FO-SOURCE.md`, local adaptations must be listed
separately, and this is that list.

Reproduce:

```sh
git init up && git -C up remote add origin https://github.com/cedrichaase/armada2-refit
git -C up fetch --depth 1 origin 1e2a7d2fc213282ef7f9ef00e1b11011da23ac68 && git -C up checkout FETCH_HEAD
python3 -I tools/refit_provenance_audit.py third_party/armada2-refit-native OUT_DIR up
```

Evidence is in `build/refit-provenance/20261107T011839Z/`: `provenance.json`,
`LOCAL-ADAPTATIONS.md` (the full per-file table with SHA-256 values) and `diffs/` (one unified
diff per modified file). The audit excludes `.git`, `target`, `node_modules` and `__pycache__`.

## Manifest integrity

`UPSTREAM-MANIFEST.json` was checked against a fresh shallow fetch of the pinned commit
`1e2a7d2fc213282ef7f9ef00e1b11011da23ac68`, and it is exact:
- the checkout HEAD equals the pinned commit;
- both sides have 1,652 files, with none missing on either side;
- there are **0 SHA-256 mismatches**.

The manifest is therefore a trustworthy upstream baseline.

## Local state

| Unchanged | Modified | Missing | Added |
|---:|---:|---:|---:|
| 1,629 | 23 | 0 | 8 |

**Added (8):**
- **Documentation:** `A2FO-SOURCE.md` and `UPSTREAM-MANIFEST.json`.
- **Explicit-host glue** (`display/src/a2fo_host.rs`, `display/src/{dx12,vk}/window_target.rs`,
  `render3d/core/src/a2fo_host.rs`, `render3d/core/src/scene/host_parts.rs`). All are compiled only
  under the `a2fo-host` feature.
- **`crates/a2fmt/src/dds.rs`:** A2FO-authored, not third-party. It is byte-identical (SHA-256
  `e3f732d1…`) to `third_party/armada2-refit/menu-renderer/a2fmt/src/dds.rs`. That file is absent
  from the older import's own upstream manifest (commit `37d60079…`) and is listed under "Added
  source/support files" in its `LOCAL-CHANGES.md`. `a2fmt/src/lib.rs` records the copy in a comment.
  The crate is MIT.

**Modified (23), by effect on a non-A2FO (stock) build.** These are hunks without any `a2fo-host` /
`a2fo_host` reference:

| Area | Files | Stock-build effect |
|---|---|---|
| Feature-gated host integration | `display/{Cargo.toml,src/dx12/mod.rs,src/vk/mod.rs,src/stub/mod.rs,src/passthrough/mod.rs}`, `render3d/{Cargo.toml,src/lib.rs}`, `render3d/core/{Cargo.toml,src/lib.rs,src/gfx/mod.rs,src/menu/mod.rs,src/scene/mod.rs}` | none: every hunk is gated |
| Gated; the one ungated hunk is a closing brace | `display/src/{api.rs,lib.rs}`, `render3d/core/src/assets/mod.rs` | none |
| **Behaviour change** | `render3d/core/src/frame/mod.rs` (hunk at upstream line 465) | the overlay/frame helper now returns `false` (was `true`) when no frame exists, and propagates `run_checked` failure: a frame is no longer credited when nothing was recorded |
| Diagnostic macro | `display/src/stub/{d3d.rs,device.rs}` (32 ungated `invalid_call!` sites) | per the in-code comment, HRESULT and branch unchanged; logging only in host builds |
| **Upstream bug fix** | `gpu/src/dx12/cmd.rs` | a readback outside a `Cmd` could record into a closed/stale list and return uninitialised memory; this is fixed for all builds |
| **Behaviour change** | `render3d/core/src/frame/post.rs` | `run_checked` stops the post chain when a pass fails to record; stock `run()` now also stops early instead of continuing |
| Misc | `crates/a2fmt/src/lib.rs` (`pub mod dds`), `display/user32.def` (`GetWindowThreadProcessId@8`), `display/README.md` (D3D12 passthrough note) | additive |

The `gpu/src/dx12/cmd.rs` fix and the checked post/frame chain are generally useful and could be offered
upstream. The `post.rs` and `frame/mod.rs` behaviour changes to stock paths should be confirmed as intended if the
non-host build is ever shipped.

## Licences and notices

All 9 licence and notice files in the manifest are present and byte-unchanged:
- the root `LICENSE` (MIT);
- `platform/vendor/STA2WidescreenPatch-1.0/{LICENSE,UltimateASILoader-license.txt}`;
- `platform/vendor/noto-sans-condensed-2.015/LICENSE.txt`;
- the `testbench/vendor/novnc-1.6.0` `AUTHORS`, `LICENSE.txt`, `LICENSE.MPL-2.0` and
  `vendor/pako/LICENSE`;
- the `ui/src/menus/screens/notice.rs` source notice.

The vendored UI is excluded from A2FO's runtime by design, but its notice remains in the tree.

## Not covered

- Built-artifact provenance (which of these files each shipped DLL actually compiles). The Renderer
  and Display build receipts and the frozen source bundles cover that separately.
- The older `third_party/armada2-refit/` (menu) import beyond the `dds.rs` chain.
