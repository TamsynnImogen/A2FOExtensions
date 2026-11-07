# Private native bootstrap diagnostic

`tools/native11_private_bootstrap.py` runs a copied legacy game under a private
Bubblewrap and Xvfb environment. It sends no input and does not deploy to the
installed game. The sandbox has no host input devices, hardware GPU, session
bus or network route. Previous private attempts are archived before another
launch, and the receipt records tested core identity, screen size, log
extraction mode and the seven protected installed-file hashes. The baseline
and candidate receipts report all seven unchanged. Legacy D3D8/9 is allowed for
bootstrap; this is not native D3D11 gameplay.

For the [API11 live-material candidate](native11-live-materials.md), pass
`--renderer-helper build/native11/A2FONative11Renderer.dll` alongside `--candidate`
and the matching fixture-gate receipt. The harness checks the helper against
`core_dll.helper_sha256`, preserves the previous private copy by hash and
atomically replaces only the trial's helper. The receipt records
`tested_helper_sha256`. The task-owned INI enables `LiveMaterials=1` with
`LiveMaterialPreview=0` and `OffscreenWorld=0`; legacy presentation remains in
charge. A menu-only run does not prove observed skirmish material submission.
The final API11 candidate remains alive for **90.9 seconds**, with the animated
legacy menu visible, no new fault, the 24-mesh baseline and 30-node template
retained, matching early/late source regions and verified namespace/Xvfb cleanup.
The receipt records the staged Core and helper hashes. Evidence is saved under
`build/native11/private-game-live-material90`; material GPU acceptance comes from
the separate private fixtures, not this menu run.

The archived `--baseline --screen 1280x1024 --seconds 90` receipt under
`/home/tamsynn/NVMeData/Projects/A2FO-Native11-Private/attempts/1793797970842440259-1947773/`
records the preserved installed core SHA-256
`0b7f7af5bbd3946348595772ceeff399d95615ced75306718ebb5e349ebf3e21`.
The 90-second probe captured a black screen early (about 24 seconds), then
captured the actual animated Armada II/Refit main menu in
`captures/bootstrap-final.png` near 82 seconds. It recorded a nonzero native
D3D interface and no new fault-log entry. The native device dimensions are
800×600 at float offsets `+0x84/+0x88`; the earlier `+0x10/+0x14` probe read
zero and was not a valid size. This establishes legacy baseline menu bootstrap
only, not native D3D11 rendering or in-game acceptance.
Earlier baseline attempts
at 1280x720 and 800x600, and the older candidate core
`6ca87d302f46b9e49da1de3fbe54422606548b114b44613159d1918456fb0f92`,
recorded first-chance `c0000005` at native address `0x623607`. Those results
bound the startup diagnostic; they do not identify a Native11 renderer fault.

The archived 90-second candidate receipt under
`/home/tamsynn/NVMeData/Projects/A2FO-Native11-Private/attempts/1793798242244748998-1948888/`
pins core SHA-256
`a66cdd0b449ff0dcef0134d5ae47c00ee7bcb03c45c7c8944d77f77fc4bdc521`.
Its private final capture shows the animated legacy Armada II/Refit main menu,
and the fault-log delta is zero. The candidate log reports the checked Fleet
terminal cleanup and texture observers armed and the DDS resolver armed. Model
bytes, TGA capture and sprite tracing refused their loaded profile/ownership
checks; complete capture was therefore unavailable. This is a private legacy
bootstrap result, not native D3D11 presentation takeover. A later 40-second
diagnostic overwrote the trial's current receipt; archived attempts retain the
90-second evidence.

The isolated Fleet lifecycle guard is recorded separately in
[native retirement](native11-lifecycle.md). The candidate passes
[118 private runtime cases](../../build/native11/live-bootstrap-full-results.json),
and a distinct [world-light owner fixture](../../build/native11/world-light-owner-results.json)
passes one case, for 119 distinct cases in these results.
No live Fleet terminal-slot observer or game destructor was exercised by that
fixture. The latest numbered checkpoint remains stage 52; all nine gates
53–61 remain open in the [fixed completion plan](native11-stage51-65.md).

The newer [live-ownership checkpoint](native11-live-ownership.md) supersedes
that startup result: candidate core `2b2a2bc5fc175e66f8337b9df6cbb8a938f60c362c300f91b67f8c6479c7e059`
passes 122 private cases and preserves a visible legacy menu, no new exception
and seven unchanged protected hashes in its saved 90-second receipt. Scene,
model, DDS, TGA, sprite, material, node and clock diagnostics report armed;
one model copy has 25,610 bytes. The first load template remains unavailable
at a loaded profile/ownership mismatch. This is neither complete native frame
production nor D3D11 presentation takeover.


The latest [Fleet constructor/template checkpoint](native11-fleet-templates.md)
passes 124 private runtime cases. Its saved 90-second legacy run uses core
`15b9a60885d454058aeac7db4329d2bf18902ccc2aae576c652027a7e2db0ae3`,
captures one actual 30-node load template and 25,610 model bytes, and ends with
the animated menu visible. The game remains alive at the limit, the new fault
log is empty, private cleanup passes and seven protected installed hashes are
unchanged. This supersedes the earlier first-template rejection. Runtime
playback, full native frames and D3D11 presentation remain unapproved.

The harness allows up to 120 seconds for Wine’s separate WoW64 prefix update;
the game observation remains 90 seconds. Earlier 45-second updater timeouts
occurred before Armada launched and are preserved in the private attempt
archives. Bounded construction, playback and lifetime reads run twice, after
12 and 60 game seconds; they do not invoke native routines or send input.


The [native animation fallback checkpoint](native11-animation-fallback.md)
adds a final private run of core
`24fc8041d58de78d65d9fe94342d3cd7e6a26c98447902c33c8afdcdf2ec1821`.
It logs owned builtin native fallback qualification at epoch 3 and actual
30-node load-template capture. The animated legacy menu stays visible through
the 90-second limit, no new fault is logged, cleanup passes and the protected
installation remains unchanged. This follows 132 successful private runtime
cases; custom clips and live skirmish pose/native gameplay acceptance remain open.


The [world-model association checkpoint](native11-world-associations.md) follows
134 successful final-artifact private cases. Core
`a0cf93c6286e2a461b9e75db656964d0e4878e03f1c6a23cf6c62ad31998d4ec`
stays alive for the final 90-second isolated legacy menu run, retains the actual
30-node template and 25,610-byte model capture, logs no new fault and passes
cleanup. Bounded early and late process diagnostics now also check the four
placement-code regions and three exact vtable slots used by the current-model
association reader; all seven match the pinned Root profile. This is loaded
profile compatibility, not live world-list association or native gameplay
acceptance. The protected installation remains unchanged.

The failed pre-launch staging attempts are preserved. A new diagnostic loop
reused the DLL destination variable for a slot target integer; `slot_target`
now keeps the path intact. Staging preserves the prior private image by hash,
verifies a synced sibling temporary copy and atomically replaces only the
private DLL. The trial remains single-owner; a mismatched backup blocks staging.


The [world Craft-view checkpoint](native11-world-views.md) passes 137 final
private cases. Its initial 90-second run found a late mismatch: near-fade data
changed from stock defaults to `(0.9, 0.99, 0.9)`. The exact initial image,
source fragments and diagnostic evidence are preserved in
`build/native11/world-view-before-dynamic-fix`.

The corrected core
`f9a90f0e9dce98f59e53f1dc03b3cafd90afa7d435365a890713abb6047284f7`
captures those three runtime floats instead of treating them as code bytes.
Its final 90-second isolated legacy run stays alive, logs no new fault, retains
the actual 30-node/25,610-byte model capture and verifies cleanup. Twelve static
view regions and two slots match early and late; the mutable fade values pass
validation. Placement ownership checks still match. This proves loaded profile
compatibility, not live world-view sampling or full native gameplay acceptance.


The world logical-preparation checkpoint uses the final 143-case fixture gate
and core build `20261105-native11-world-preparation-wip`. Its 90-second private
legacy menu run logs actual checked Fleet base-setup qualification. Bounded
process reads record the original setup at two seconds before Fleet installs
its hook, then validate the five-region callback closure and exact saved-original
gateway in all 44 subsequent snapshots. Root logical-body/getter/constants and
slots, placement/view ownership and runtime fade values pass early/late checks.
The actual 30-node template remains captured; no new fault is logged and cleanup
verifies. The menu screenshot was visually checked. These are startup/source
checks; live skirmish states and native D3D11 presentation remain unaccepted.
Evidence: `build/native11/private-game-world-preparation90` and
`build/native11/world-preparation-milestone.json`; installed files unchanged.


The world base-visibility integration uses the 147-case final-artifact gate and
core `20261105-native11-world-base-visibility-wip`. The final 90-second isolated
legacy run logs the new bounded base-source qualification. Six Root code/data
regions and one CF slot match in both early and late process reads, as do the
previous placement/view and Fleet setup checks. A pre-Fleet first snapshot is
accepted only as the pinned original database destructor, with no Fleet proof
claimed at that point. The actual load template remains captured; no new fault
is logged and namespace/Xvfb cleanup verifies. The final rendered-menu screenshot
was inspected. These checks do not validate a live skirmish visibility roster,
per-instance context or native D3D11 presentation. Evidence:
`build/native11/private-game-world-base-visibility90` and
`build/native11/world-visibility-milestone.json`; installed hashes unchanged.
