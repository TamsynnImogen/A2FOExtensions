# Cloak prototype activation and named ship diagnostics

Date: 2026-10-22. User feedback: no identifiable change in Roots or A2 Classic,
and appearance did not match Armada II Refit.

## Finding

The tested installation at `/home/tamsynn/NVMeData/Fleet Ops Roots/Data` had
core SHA-256 `605608d738a64219a9f27ef4bec07ef0c9c0b14cf3743f0c806b94d985f8ac8f`.
The staged prototype was `d9abd7d3cad11fe1813793b095c8db8e96caca1a316c6f935bad59e0da29140e`.
The installed renderer INI had no RefitPrototype setting. The fresh 09:01 log
showed managed DXVK, Nebula, the old textured cloak compositor and active mod
`test`, inheriting `STA2 Classic Mod`. It contained no new refraction-draw
marker. These game tests therefore exercised the previous effect.

The initial prototype was only staged. Its successful synthetic GPU fixture
did not prove activation in this installation, nor matching Refit visuals.

## Changes and validation

Added bounded per-ODF, cloak-state and route-category diagnostics. Class names
come from the existing checked class-load dispatcher rather than new engine
addresses. When the prototype is off, class-name storage and mesh probes are
skipped. Named reports are deduplicated rather than emitted every frame.

New examples in A2FOExtensions.log:

```text
Refit cloak prototype build=20261022-activation-audit enabled on managed DXVK; named per-ODF status reporting active
Refit cloak ship: odf=kdestroy state=1 status=seen reason=engine-submitted hull; awaiting draw route
Refit cloak ship: odf=kdestroy state=1 status=active reason=D3D9 refraction DRAW executed
```

These are illustrative messages, not captured in-game proof. `seen` means the
engine submitted a cloaking craft. `active` proves a replacement draw executed.
`fallback` includes a reason such as standard MeshVB, native CPU sorting, M3D,
material restrictions, or a failed backend prerequisite. A mixed-part model may
report both active hull groups and fallback groups. States are 1 cloaking,
2 fully cloaked, 3 decloaking. No unseen ships are enumerated or revealed.

The updated native dispatch fixture passed argument/HRESULT preservation,
replacement/fallback/fatal outcomes, named active and fallback reports,
distinct route categories, deduplication and the disabled diagnostic gate.
The core rebuilt successfully with pre-existing warnings and passed the private
Wine DLL load/export check. Targeted independent source review passed after
correcting eager diagnostic probes on disabled routes. Shader/backend sources
are unchanged from the initial GPU-tested prototype; its existing headless
pixels, state restoration and reset evidence remain applicable to that pass.

## Deployment

Only these installed files changed:

- `Data/A2FOExtensions.dll`: SHA-256
  `aa5472acfd463c77b8f5792eca193610ab74c6d77747aeb45be2ed29f77ab2b6`.
- `Data/A2FORenderer.ini`: retained all original 330 bytes and appended only
  `[Cloak]` with `RefitPrototype=1`.

No game was running during deployment, and no game was launched by the agent.
The backup is
`Data/.deployment-backups/cloak-activation-20261022-081509Z/`, including the old
DLL/config and fresh logs. Verification found 109 protected files unchanged.
The full receipt, before/after hashes and prepared package are under
`build/cloak-activation-20261022-080457Z/`.

To roll back, close the game and restore A2FOExtensions.dll and A2FORenderer.ini
from that backup. To disable only the new effect, set RefitPrototype=0 and
restart. Renderer selection and mod/module configuration were not altered.

## Next manual check and visual limits

In Classic, `kdestroy.odf` is the Bird of Prey with `gkcloak`; `rscout.odf` is
the Talon with `grcloak`. Test cloak and decloak against stars or a nebula.
The fresh log can identify their exact route and whether refraction executed;
their ODF abilities alone do not prove every model group is supported.

The shader is still the initial approximation: local-Y/mesh-local waves,
screen-derived refraction inputs, simplified diffuse lighting, white rim/front,
no chromatic splitting and no owner-specific residual hull floor. Refit uses
coherent world-space longest-axis waves, camera/eye-based glass geometry,
channel splitting and separate rim/front colors. Matching that appearance
requires validated camera/world inputs and a further shader port. This
activation change deliberately does not change those visuals before the first
actual prototype comparison. Native in-game activation, appearance, MSAA and
performance remain pending user verification.
