# Stages 41–50: complete-frame admission and native presentation

These stages advance the independent D3D11 presentation path. They do not
certify game producers that do not yet exist. No legacy surface, previous-frame
pose or authored fallback becomes authoritative geometry by passing these tests.
Installed game files and frozen stages 1–40 remain unchanged.

41. Define complete-frame domain inventory with explicit absence and source identity.
42. Validate ordered, owned world/effects/video/HUD/menu passes before GPU admission.
43. Attach an existing host window to the offscreen D3D11 device and copy on-GPU.
44. Guard presentation against stale targets, resize, window loss and occlusion.
45. Publish append-only API8 while preserving API1–7 binary clients.
46. Coordinate complete-frame rendering and present with owner-thread/reentry guards.
47. Load and close the native service independently of legacy renderer startup.
48. Revoke stale sources and recover across suspension, resize and restart.
49. Exercise native presentation and complete-frame fallback in private hidden-window fixtures.
50. Freeze a fresh complete compatibility run, source/binary receipts and installation audit.

The host still has to implement and accept every real game domain, authoritative
offscreen animation/material evaluation, safe engine presentation ownership and
Windows/gameplay acceptance. A host declaration is an explicit contract, not
automatic discovery or independent proof of that declaration.
