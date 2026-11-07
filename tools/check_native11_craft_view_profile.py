{
  "prior": "world-association-milestone.json",
  "runtime": "world-view-full-results.json",
  "case_count": 137,
  "contract": "world-view-core-contract.json",
  "receipt": "world-view-milestone.json",
  "directory": "world-view-integration-wip",
  "ledger_key": "world_view_integration_wip",
  "status": "PASS owned world-roster Craft distance/fade policy snapshots; all nine implementation gates 53-61 OPEN",
  "executables": {
    "world-craft-view": "native11_world_view_capture.exe",
    "world-view-source": "native11_asset_guard_test.exe",
    "craft-view-dynamic-fade": "native11_craft_view_native.exe"
  },
  "sources": [
    "Makefile",
    "core/build_identity.hpp",
    "renderer/native11/craft_view_capture.hpp",
    "renderer/native11/craft_view_capture.cpp",
    "renderer/native11/world_view_capture.hpp",
    "renderer/native11/world_view_capture.cpp",
    "core/native11_world_view_snapshot.hpp",
    "core/native11_capture.hpp",
    "core/native11_capture.cpp",
    "core/native11_craft_view_profile.hpp",
    "tests/native11_world_view_capture_test.cpp",
    "tests/native11_asset_guard_test.cpp",
    "tools/native11_private_bootstrap.py",
    "docs/renderer/native11-world-views.md",
    "docs/renderer/native11-progress.md",
    "docs/renderer/native11-stage51-65.md",
    "docs/renderer/native11-private-bootstrap.md",
    "renderer/native11/craft_view_policy.hpp",
    "renderer/native11/craft_view_policy.cpp",
    "core/native11_animation_clock.cpp",
    "tests/native11_craft_view_native.cpp",
    "tests/native11_craft_view_capture_test.cpp",
    "tools/check_native11_craft_view_profile.py"
  ],
  "evidence": [
    "check_world_view_core.py",
    "world-view-core-build.log",
    "world-view-core-contract.log",
    "world-view-full-build.log",
    "world-view-final-asset-build.log",
    "world-view-full-runtime.log",
    "world-view-syntax.log",
    "world-view-host-build.log",
    "world-view-host-runtime.log",
    "world_view_capture_host",
    "world-view-focused-build.log",
    "world-view-pure-results.json",
    "world-view-pure-runtime.log",
    "check_world_view_gate.py",
    "world-view-fixture-gate.json",
    "world-view-final-gate.log",
    "private-bootstrap-world-view90.log",
    "check_world_view_bootstrap.py",
    "world-view-bootstrap-check.log",
    "world-view-review.json",
    "world-view-next-render-entry.asm",
    "world-view-next-instance-setup.asm",
    "private-game-world-view90/Data/A2FOExtensions.log",
    "private-game-world-view90/Data/A2FOMissions-fault.log",
    "private-game-world-view90/bootstrap-receipt.json",
    "private-game-world-view90/captures/bootstrap-final.png",
    "private-game-world-view90/logs/bootstrap-progress.json",
    "private-game-world-view90/logs/new-A2FOExtensions.log",
    "private-game-world-view90/logs/new-A2FOMissions-fault.log",
    "private-game-world-view90/logs/read-profiles.json",
    "private-game-world-view90/review.json",
    "world-view-dynamic-core-build.log",
    "world-view-dynamic-focused-build.log",
    "world-view-dynamic-full-build.log",
    "world-view-dynamic-host-build.log",
    "world-view-dynamic-host-runtime.log",
    "world-view-dynamic-pure-results.json",
    "world-view-dynamic-pure-runtime.log",
    "craft-view-profile.json",
    "world-view-before-dynamic-fix/manifest.json",
    "world-view-before-dynamic-fix/status.json",
    "world-view-before-dynamic-fix/A2FOExtensions.dll",
    "world-view-before-dynamic-fix/bootstrap-receipt.json",
    "world-view-before-dynamic-fix/logs/bootstrap-progress.json",
    "world-view-dynamic-focused-results.json",
    "world-view-dynamic-focused-runtime.log",
    "world-view-before-dynamic-fix/world-view-core-contract.json",
    "world-view-before-dynamic-fix/world-view-full-results.json",
    "world-view-before-dynamic-fix/world-view-full-runtime.log",
    "world-view-before-dynamic-fix/world-view-core-build.log",
    "world-view-before-dynamic-fix/world-view-full-build.log",
    "world-view-before-dynamic-fix/world-view-final-gate.log",
    "world-view-before-dynamic-fix/world-view-review.json",
    "world-view-before-dynamic-fix/world-view-fixture-gate.json",
    "world-view-before-dynamic-fix/world-view-final-focused-results.json",
    "world-view-before-dynamic-fix/world-view-final-focused-runtime.log",
    "world-view-before-dynamic-fix/logs/read-profiles.json",
    "world-view-before-dynamic-fix/logs/new-A2FOExtensions.log",
    "world-view-before-dynamic-fix/logs/new-A2FOMissions-fault.log",
    "world-view-before-dynamic-fix/captures/bootstrap-final.png",
    "world-view-before-dynamic-fix/source/core/native11_capture.cpp",
    "world-view-before-dynamic-fix/source/core/native11_animation_clock.cpp",
    "world-view-before-dynamic-fix/source/core/native11_craft_view_profile.hpp",
    "world-view-before-dynamic-fix/source/renderer/native11/craft_view_policy.hpp",
    "world-view-before-dynamic-fix/source/renderer/native11/craft_view_policy.cpp",
    "world-view-before-dynamic-fix/source/renderer/native11/craft_view_capture.hpp",
    "world-view-before-dynamic-fix/source/renderer/native11/craft_view_capture.cpp",
    "world-view-before-dynamic-fix/source/renderer/native11/world_view_capture.cpp",
    "world-view-source-changes.patch"
  ],
  "private_bootstrap": "private-game-world-view90/bootstrap-receipt.json",
  "scope": [
    "137 final-artifact private cases pass, including full-roster distance/fade and source revocation",
    "Strict core build; canonical198 exports and eleven imports; helper and seven reference previews unchanged",
    "Pure world reader passed Clang ASan/UBSan; two sweep consistency plus final roster/model/placement rewalk",
    "Private 90-second legacy menu and loaded placement/view profile compatibility checked, not live world acceptance",
    "No native gameplay/presentation or installed deployment; seven protected installed hashes unchanged",
    "Actual late near-fade startup mismatch fixed by capturing mutable values; 49152 extra native CPU comparisons over12 precision/rounding modes"
  ],
  "remaining": [
    "All nine implementation gates 53-61 remain OPEN",
    "Live skirmish association/view acceptance; raw current DB does not prove render selection",
    "Fresh class/procedural/FOW/material/per-part pose and actual complete-world producer",
    "Other world domains, full lights/casters, UI/effects/video, native presentation and independent startup",
    "Device/restart, native Windows and gameplay acceptance"
  ],
  "isolated_native_routines": [
    {
      "image_sha256": "4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de",
      "rvas": [
        "0x213910",
        "0x2146a0",
        "0x2161c0",
        "0x213670"
      ],
      "scope": "CPU-only original phase, scalar, Matrix34 channel and independent instance clock comparisons under 12 x87 modes",
      "phase_comparisons": 18432,
      "scalar_comparisons": 55296,
      "matrix_channel_comparisons": 55296,
      "clock_comparisons": 1176
    },
    {
      "image_sha256": "4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de",
      "rvas": [
        "0xcb330"
      ],
      "scope": "Original CPU damage threshold helper; synthetic inputs"
    },
    {
      "image_sha256": "4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de",
      "rvas": [
        "0x65940",
        "0x239460",
        "0x230a50",
        "0x22c370"
      ],
      "scope": "CPU-only native matrix, local scale, LOD selector and force-highest leaf",
      "matrix_products": 24576,
      "scaled_locals": 24576,
      "lod_selections": 501120
    },
    {
      "image_sha256": "4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de",
      "rvas": [
        "0x220ddb..0x220e77"
      ],
      "scope": "Reviewed straight-line original camera translation CPU block copied privately with appended RET",
      "camera_translations": 24576
    },
    {
      "image_sha256": "4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de",
      "rvas": [
        "0x2337a0"
      ],
      "comparisons": 24576,
      "vertices": 392856,
      "scope": "Original mesh scaling CPU only, preallocated retained-original buffers; synthetic invalidate callback; no native allocation, simulation or graphics"
    },
    {
      "image_sha256": "4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de",
      "rvas": [
        "0x225590",
        "0x2254a0",
        "0x23b150"
      ],
      "scope": "Original CPU sprite vertex builder with synthetic texture-zero sprite/material, virtual state/draw recorders; no graphics or startup",
      "colour_comparisons": 49152,
      "vertices": 196608
    },
    {
      "image_sha256": "4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de",
      "rvas": [
        "0xd5bb0",
        "0xcaff0",
        "0xd5a20",
        "0x73c20"
      ],
      "scope": "Original CPU Craft distance/cull/fade with synthetic object/game settings and isolated data globals; no simulation or graphics",
      "view_comparisons": 98304,
      "default_view_comparisons": 49152,
      "dynamic_fade_comparisons": 49152
    }
  ]
}
