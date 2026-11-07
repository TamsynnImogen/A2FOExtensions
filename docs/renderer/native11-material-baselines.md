# Completed-load material baselines

This work supplies immutable original mesh texture slots for a future independent
material selector. It does not select effective materials or close gate 53.

The pinned Root executable's `CraftClass::SetBorgMeshTextures` at RVA `BFB70`
already jumps into an on-disk code cave at RVA `40129E`. Its non-Borg path
restores both diffuse slot 0 and an auxiliary slot at vector offset `0x0c`.
The Borg path reads mesh field `+0x13c`. Reading the shared mesh's diffuse slot
after another instance draws cannot establish the original material state.
The research disassemblies are saved with the checkpoint; no executable was
patched to produce them.

The pinned file's texture setter at RVA `2313B0` writes the slot vector and may
grow its count/capacity; its complete disassembly shows no direct `+0x13c`
write. This source finding does not establish loaded hook ownership or when a
class's original-texture table is built relative to database completion.

`native11_animation_clock::observe_model_loaded` now attempts an optional
material baseline immediately after successful node-template publication, while
the completed native database loader's caller remains suspended. It never
captures material defaults from a later instance draw, getter or cache miss.
Failed material capture leaves the node template available and cannot be
backfilled by a duplicate completed-load notification.

The pure reader checks the exact database header, roles, full native node
identity and topology against the completed-load template in two complete
passes. For each authored mesh it copies every slot from the native vector
(`+0x114`, count `+0x11c`, maximum eight), including null slots, and the load-time
value of `+0x13c`. Non-null slots retain exact immutable texture versions;
runtime name, flags and colour-key metadata must agree with those versions.
Nonzero flags and keyed textures may be retained; their future draw support
requires separate renderer admission.
Slot-vector, texture-owner or metadata changes reject publication. It does not
read mutable node flags, matrices or scales to derive material defaults.

The cache retains weak model ownership and strong immutable template and texture
owners. It is bounded to 64 models and 128 MiB of accounted metadata and unique
texture byte buffers. A live owner tuple cannot be replaced. Access requires
the original owner thread, checked load/node ownership, the pinned mesh type
and slot getter, exact current model/template owners and current texture
versions. Texture retirement or address reuse revokes access without revoking
the separate node template. Restart clears the cache; disable revokes access.

Raw slot order is not a mesh-group or shader-channel mapping. The recorded
alternate is its value at this load boundary, not proof of a later class's Borg
selection. Native base/Borg selection, faction texture/node changes and scoped
subsystem overrides must still be derived and applied in their verified native
order. The texture-variant module's lazily populated maps currently provide no
stable snapshot API; empty maps cannot prove the absence of overlays.

Validation: **148/148 final-artifact private runtime cases pass**, including the
actual clock completed-load route in plain, Fleet and builtin-fallback variants.
Strict changed-source syntax and linked fixture builds, host reader/cache checks
and Clang ASan/UBSan pass. Cases cover mutations between passes, missing or
replaced texture owners, native metadata mismatch, null slots, raw flagged/keyed
textures, invalid topology, capacity, weak lifetime and failed-publication output
preservation. The relocated inspection-only EXE fixture needed its additional
mesh-type data slot normalized and restored; production admission was unchanged.

The final 90-second private legacy menu run captures **24 mesh baselines** for
the actual 25,610-byte model, alongside its **30-node** template. The final
legacy menu screenshot shows textured ships, menu text and the planet. Existing
base/Fleet/view/placement source checks still pass, the new-fault log is empty
and namespace/Xvfb cleanup is verified. Seven protected installed files, the
renderer helper and seven reference previews remain unchanged. This is real
completed-load capture evidence, without native DX11 gameplay acceptance.

Artifacts and source hashes are recorded in the
[checkpoint receipt](../../build/native11/material-baseline-milestone.json).
No native geometry provider, complete world frame or native presentation
takeover is registered. Live skirmish material acceptance and all nine
implementation gates 53–61 remain open. Installed files remain unchanged.
