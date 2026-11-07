# Shell intro presentation v7 — 2026-10-02

The v6 live log showed `A2FOShellDisplay` loading and reporting successful Bink
hook installation, but there were no `Intro window matched shell` or `Intro Bink
viewport` messages. The shell itself later logged correct 1920x1080 borderless
geometry. This established two startup-order issues rather than a scaling-math
failure:

1. Native modules are normally initialized on the core's deferred worker, so the
   intro may begin before `A2FOShellDisplay` is ready.
2. v6 only published presentation state from the shell activation hook; the intro
   naturally precedes that activation and therefore had no snapshot to consume.

v7 makes the startup proxy distinguish `attached` from `initialized` and joins
`A2FO_Initialize` before the first proxied startup export returns. It does not load
modules under the Windows loader lock. `A2FO_Initialize` now safely waits for an
already-running worker instead of returning a transient failure.

For the pre-shell movie, `A2FOShellDisplay` lazily seeds presentation state from
FleetOpsHook's `TFoSettings` singleton at DLL+0x20e644 using the reconstructed
layout: width +0x30, height +0x34, depth +0x38, adapter +0x64 and Window Mode
+0x72. A zero width/height means the selected adapter's current desktop mode,
matching FleetOps' save behaviour. Once shell activation occurs, the existing
live snapshot becomes authoritative.

Expected new diagnostics on a successful intro run:

- `Completing deferred A2FO_Initialize before startup export returns` in
  `A2FOStartupProxy.log` when the worker has not already finished.
- `[A2FOShellDisplay] Intro presentation seeded from FoSettings: ...`
- `[A2FOShellDisplay] Intro window matched shell: ...`
- `[A2FOShellDisplay] Intro Bink viewport: ...`
