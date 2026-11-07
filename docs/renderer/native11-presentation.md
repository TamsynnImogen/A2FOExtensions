# Native presentation service (API8)

API8 extends API7 with `configure_presentation` and `present`. API1–7 tables,
records and capabilities retain their original values. The helper continues to
export only `A2FO_Native11_GetInterface`. No legacy D3D8/9 object crosses this ABI.

The renderer keeps its own offscreen colour/depth targets and D3D11 device. An
optional DXGI swapchain belongs to an existing, same-process, creating-thread
window supplied by the host. Presentation copies the completed colour texture
to the swapchain entirely on the GPU. It does not use CPU readback or another
renderer's backbuffer. The service never creates, shows, activates, resizes or
destroys the host window and disables automatic Alt+Enter handling.

## Admission and image lifetime

Attach requires exact, nonzero agreement between the offscreen target, requested
dimensions and window client area. Null window with zero dimensions detaches.
Every attach, admitted configuration or resize invalidates presentation eligibility;
each attempted presentation consumes it. A fresh successful submission is needed
before presenting again, including after an error or occlusion. The host owns
window resizing and must then resize the service target and configure presentation.
Target loss does not prevent detach, resource cleanup or offscreen recovery.

`S_OK` means the driver accepted presentation. `S_FALSE` means occluded. Neither
is proof that a human saw the intended image. Other statuses do not authorize
the complete-frame coordinator to publish success.

## Complete-frame coordinator

`FrameBundle` explicitly accounts for background, terrain, objects, effects,
video, HUD and menu domains. Each must have an ordered pass or an explicit absent
bit; contradictions and missing domains fail. It carries view, frame, source epoch
and source ticket identity plus a host assertion of complete geometry. **These
declarations do not discover game data or prove that an engine producer is complete.**

Packets own their payloads. The host must keep bundles and packets immutable,
including through any retained mutable aliases; `shared_ptr<const>` alone does
not enforce this obligation. Every pass supplies an
identical ordered resource manifest and the same frame/source identity. The
first pass clears colour and depth; subsequent passes retain colour and can
choose their depth policy. Full-target viewports are explicit. At most 16 passes,
8,192 aggregate draws and 128 MiB aggregate resource payload are admitted.
Presently a represented domain needs at least one draw; an empty domain is
declared absent. This contract is stricter than the raw primitive service.

`PresentationSession` validates and encodes all passes before GPU admission,
assigns monotonic internal submission serials, and presents only after every
pass succeeds. Sources are sampled before/after submissions and presentation.
Incomplete input, partial rendering, revocation, failure and occlusion publish
no current native frame. A later frame starts with a complete clear. Published
snapshot access revalidates the source callback. Callbacks/context stay alive
through explicit close, and all operations are externally serialized on the
creating thread; callbacks must not throw. Reentry and foreign threads are refused.

A post-present revocation check can detect a changed source but cannot undo a
DXGI submission already made. The real host must serialize source changes and
presentation ownership. A `fallback` result requests host policy; this library
does not itself resume or seize the engine renderer.

## Independent startup and verification scope

`NativeStartup` loads one explicit absolute DLL path with DLL-directory/system
dependency search, requests API8 and owns its presentation session. It consumes
no engine renderer or translation-layer handles. The caller explicitly shuts
down before unloading; failed cleanup retains the DLL for retry. This is a
native-service host path, not a patched game executable startup path.

Private fixtures use only hidden windows on an isolated Xvfb/Wine display.
They never show/activate windows or synthesize input. A controlled COM fixture
reads copied swapchain pixels before `Present`, using SDK-derived vtable slots,
and injects occlusion, presentation and resize errors. Such injected failures do
not physically remove a GPU. Native Windows, visible presentation, real engine
draw producers and full gameplay acceptance remain separate release gates.
