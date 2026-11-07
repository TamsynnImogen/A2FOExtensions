# Temporary minimap diagnostics

## Built-in integration

The core-provided module name has an independent menu policy, but capture also
requires `[Diagnostics] Enabled=1` in `Data/A2FORadarDiagnostics.ini` at
startup. Enabling the menu policy after an INI-disabled startup does not
install hooks; restart with the INI enabled. Counters and log capture are
policy-gated. Built-in hooks stay resident and are not unpatched. The explicit
standalone SDK target remains available.

This module observes the existing D3D8 radar path. It does not implement a
renderer, draw replacement markers, change filters, change viewports, or enable
D3D11. It is deliberately excluded from the normal release target.

The opt-in configuration is `Data/A2FORadarDiagnostics.ini`:

```ini
[Diagnostics]
Enabled=1
```

The module must also be selected in the active mod's `[modules]` policy.
Without the INI setting it installs no hooks. Logs use the
`[A2FORadarDiagnostics]` category in `A2FOExtensions.log`.

On a supported, signature-checked Armada image it patches two complete call
instructions, preserving the existing implementations:

| Site RVA | Observation |
| --- | --- |
| `0x0ea547` | Craft iteration's call to `RadarComponent::RenderMapObject`, native entry `0x0ea150`; ECX `this`, four stack arguments, callee cleanup 16 bytes |
| `0x223b99` | `ST3D_DeviceDirectX8::DrawRectangle` call to `IDirect3DDevice8::Clear`, after flushing triangles; COM stdcall, seven arguments including device |

These addresses were checked in the installed Roots `ArmadaL.exe`, SHA256
`817b1ddb9876cfb9c5d781b42d2513de48fef9cbdbc408aa43c878c573581a18`.
FleetOps' outer RadarEnhancement callback remains intact. Changed sites reject
installation; the module does not scan for alternative addresses.

At most 96 distinct class/pass samples and 24 Clear samples are logged, plus
aggregate summaries every 30,000 craft calls. The Clear samples query
`GetViewport` before forwarding the original call and record the returned
HRESULT, rectangle, colour, current camera rectangle and provider module. All
Clear arguments and the HRESULT are passed through. No per-frame disk trace is
created. The first class sample is not a full frame or a visibility verdict.

`class18c` is a raw class field tested by native RenderMapObject before drawing;
`icon` is its sprite pointer. Their ODF command names have not been established.
Filter values are observations only; they are never reset by this module.
Counters distinguish the two native passes and rectangle Clear attempts, not
successful on-screen visibility. A successful Clear may still be clipped or
subsequently covered.

Build explicitly with `make radar-diagnostics-tests`. The console diagnostics
fixture checks forwarding, rollback (including failed rollback) and callback
lifetime. The separate GPU fixture
checks minimap-style rectangle pixels under Wine builtin D3D8 and private copies
of the installed wrapper chain. Neither fixture starts Armada or proves the
live game is fixed.

For one manual capture, fully restart Roots, launch the `first` map, leave the
missing markers visible for about 10 seconds, then quit normally. Preserve the
fresh log before another launch. A child mod with its own module selection needs
its own explicit opt-in; do not assume this Roots-only diagnostic is selected
there.

To disable, set `Enabled=0` with the game closed and restart. Deployment receipts
identify the exact original `info.ini` and any new files so they can be restored
without changing other modules or renderer settings.

Hooks and the module/logger owners remain pinned until process exit. Shutdown
only disables capture; it does not rewrite live instructions or clear callback
storage. An observation already in progress can finish. If the second startup
patch and restoration of the first both fail, the module retains the first
callback in pass-through mode and reports the failure rather than unloading
code that might still be called. Removing or changing this module requires a
full process restart.
