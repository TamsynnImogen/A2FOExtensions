# D3D11 sampled depth snapshot

2026-11-02. Implemented in the experimental game backend; this is a prerequisite
for future effects, not an ambient-occlusion, spotlight, or shadow effect.
Nexus retains its current managed DXVK route.

`Backend::snapshot_depth(source, out)` copies a native depth `Target` into a
separate caller-owned `Texture`. D16, D24S8, and D32 map respectively to
R16_TYPELESS/R16_UNORM, R24G8_TYPELESS/R24_UNORM_X8_TYPELESS, and
R32_TYPELESS/R32_FLOAT storage/SRV pairs. The original depth resource is not
reformatted, cleared, replaced, or unbound. Source ownership, device, format,
subresource layout and thread admission are checked before use.

`CopyResource` operates while the original depth remains bound; the method
does not change any OM, shader, resource, or sampler binding. An earlier draft
temporarily rebound OM targets; independent review identified that this could
drop UAVs, so the round trip was removed before acceptance. Immediate device
removal status is checked before publishing `out`. Success means the GPU copy
was queued, not synchronously completed. Previous `out` survives rejected or
failed calls. A depth-only snapshot can be sampled but cannot be uploaded or
converted to a render target through the existing wrapper API.

The source/reference design agrees with Microsoft's
[CopyResource contract](https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-copyresource)
and [typed depth/SRV resource guidance](https://learn.microsoft.com/en-us/windows/uwp/gaming/create-depth-buffer-resource--view--and-sampler-state).

## Verification

`tests/renderer_depth_snapshot_native.cpp` cross-builds with `-Werror` through
`make renderer-depth-tests`. It runs under the task-local private Xvfb and
PortProton Wine prefix, never the user's desktop or a game process.

For all three formats it draws an asymmetric near occluder into the source,
copies it, tests original depth occlusion and D24 stencil, takes another
snapshot and clears the source. Shader sampling still reads the original
near/far depths (8-bit red 51/204). It checks RTV/DSV/depth-state/OM-UAV binding
identity, independent snapshots, invalid-input output preservation, foreign
device rejection, immutable upload/RTV rejection and the no-auto-depth case.
It does not verify UAV hidden counters or D3D11 debug-layer messages.

The test also passes after rebuilding against the production core's paired
Rust library. The existing translated-game fixture passes geometry, shader,
sampler, texture, BC5/BC7 reference sampling, D3DX/file bridge, partial-clear,
state-block, copy, resize/reset and no-auto-depth coverage. The assembled core
passes its separate fake-host load/worker/retry fixture. Evidence is in
`build/ingame-shadows/audit4-assembled-tests.log`; the initial worker run is
retained separately in `renderer_depth_snapshot_native.log`.

Native Windows and in-game acceptance remain separate. The default game
backend does not automatically call this method or activate a new post-process.
The snapshot currently supports the backend's single-sample, single-subresource
depth layout; MSAA sampling/resolution and scene-normal buffers remain future
work. AO also needs correct camera reconstruction and a scene-colour composition
pass; a depth SRV alone is not a finished effect.
