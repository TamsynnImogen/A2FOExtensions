# Private live ownership checkpoint

This historical 122-case checkpoint is superseded by the
[Fleet constructor/template checkpoint](native11-fleet-templates.md): 124 private
cases pass and the final private model load captures a 30-node template.


Status: staged candidate, latest numbered checkpoint **stage 52**. All nine
implementation gates 53–61 remain open. The exact candidate core SHA-256 is
`2b2a2bc5fc175e66f8337b9df6cbb8a938f60c362c300f91b67f8c6479c7e059`.
Its strict build and [122/122 private runtime cases](../../build/native11/live-owner-compat-full-results.json)
pass. This count includes the earlier 119-case evidence and new ownership
fixtures; it is not a coverage percentage or native D3D11 gameplay proof.

The saved [private 90-second run](../../build/native11/private-game-owner-compat90/bootstrap-receipt.json)
and [review](../../build/native11/private-game-owner-compat90/review.json) pin
the candidate, record seven unchanged protected installed-file hashes and no
new exception, and preserve a visible animated Armada II/Refit menu in the
[final capture](../../build/native11/private-game-owner-compat90/captures/bootstrap-final.png).
The menu still uses legacy graphics. Native D3D11 frame production and
presentation takeover remain unavailable.

The [new extension log](../../build/native11/private-game-owner-compat90/logs/new-A2FOExtensions.log)
reports scene, model, DDS, TGA, sprite, material, node and animation clock
diagnostics armed. One model read copied 25,610 bytes. Its first load template
reported `unavailable` because the checked profile or model ownership did not
match. The private read diagnostics found a known Fleet constructor patch over
the 20 bytes at Root VA `0x63faac..0x63fac0`; that profile difference has not
been qualified or fixed. No fresh node or class/LOD authority follows from the
armed diagnostic messages.

The model and TGA stream readers qualify the exact six-byte Root fopen redirect
at VA `0x640162` to the pinned Fleet FOFS callback. The sprite reader qualifies
Fleet's 44-byte device-dimension NOP range at VA `0x621636` and accepts a changed
seven-byte refresh prefix at VA `0x624710` only with a trusted owner callback.
Unknown owners are refused. Private asset tests check reactivation and removal
of a reused database record when FOFS ownership is lost. The separate sprite
coexistence fixture checks approval, revocation and recovery with a **synthetic**
owner callback; it does not prove the renderer's live hook/gateway logic.

The world-light reader now waits for copied class configuration and the first
owned view before admission. Its separate worker/main-thread fixture checks
owner binding and foreign-thread refusal. Review of `dllmain_step15` and the
renderer/clock initialization path places the clock's early initialization in
`DLL_PROCESS_ATTACH`, before the deferred worker. A clock owner-thread mismatch
is a conditional risk, not a failure established by this private run. The
observed load-template profile mismatch remains the concrete next blocker.
