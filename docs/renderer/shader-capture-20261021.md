# Native D3D8 shader evidence gate

The requested native capture has now been preserved and tracing disabled.
Three programs are complete, one remains unknown to the diagnostic parser.
See [captured VS1.1 slice](vs11-slice-20261021.md) for identities, shader translation,
paired GPU evidence and the remaining compatibility boundaries.

The real Fleet Operations window has now displayed the one-frame SM5 triangle.
Normal game rendering still uses its legacy D3D8 provider. Full D3D11 backend
selection remains refused while the compatibility facade is incomplete.

The next gate captures shader declarations and instructions under the existing
optional `Trace=1`. Earlier native FO trace `A2FOD3D8-208-121731484.jsonl` records
four successful vertex-shader creations, 25,852 SetVertexShader calls and
52,560 vertex-constant updates, but only argument pointers/register counts.
Those counts are cumulative snapshots, not totals obtained by summing snapshots.
They establish usage without identifying the shader programs or constant data.
Absence in a capped trace never establishes non-use.

Existing A2FO DOT3 and cloak paths create programmable shaders. In particular,
the cloak compositor builds VS 1.1 variants for zero through eight lights and
saves/restores constants c8 through c26. Pixel assets use PS 1.4. These paths
must remain compatible during migration; transformed fixed-function UI shaders
alone cannot replace them.

## Capture boundary

`renderer/shaders/d3d8_tokens.*` finds bounded D3D8 declaration and shader stream
boundaries using original CPU code. It supports VS 1.1 and PS 1.0 through 1.4,
skips declaration CONSTMEM/EXT payloads, DEF immediate values and shader comments,
and includes the real END token. An END-shaped constant is data. Unknown
versions/opcodes are refused; opcode 75/TEXM3x3DIFF is deliberately unsupported
until its operand contract is established. This parser does not validate shader
semantics or translate legacy instructions to SM5.

The observer's private `shader_diagnostics.inl` runs only after successful native
CreateVertexShader/CreatePixelShader calls with tracing enabled. Every native
argument, HRESULT, output handle and COM identity remains authoritative. It
reads one additional DWORD at a time only while parsing requires more input,
checks readable memory, and stops at END. Limits are 64 creation captures per
process, 4,096 DWORDs per stream, and the existing 4 MiB trace budget. Partial,
unreadable, invalid, unsupported or over-limit streams have explicit statuses
and empty word arrays. Native calls continue after a diagnostic limit/failure.
Successful creation with a null vertex function is recorded as a declaration
with an absent function, not a fabricated shader program.

JSONL schema 2 adds `shader` events with capture/frame/device ID, stage, returned
handle, usage, declaration words and function words. Limits and exhaustion are
recorded. Existing method counters and call samples continue; sampled calls are
not a complete chronological state/draw replay. State-block semantics, constant
values, texture/buffer contents and draw-to-state correspondence remain pending.
The existing `Trace=0, Backend=d3d8` path still returns before observer allocation,
import interception or GPU work. No extra release-log draw spam or graphics
configuration options were added.

`tools/renderer_shader_report.py TRACE... --output report.json` reports token
SHA256 hashes over little-endian DWORDs, shader versions, creation metadata,
capture status/limits and cumulative method counts. Different signature
snapshots remain separately keyed; counts are not summed across repeated
snapshots. Schema 1 pointer-only inputs retain unknown contents. Reports are
unlabelled evidence: fixture files must never be passed off as game captures.
Raw payloads remain in the original JSONL; hashes cannot reconstruct them.

## Validation

The parser and modified COM fixtures build with warnings treated as errors.
The synthetic observer fixture verifies native return values/handles, Trace=0
inactivity, failed-call gating, guard-page END/truncation handling, an embedded
END-shaped DEF constant and capture exhaustion without suppressing native calls.
Eight Python tests cover report hashes, incomplete/absent payloads, malformed
DWORDs and cumulative-count handling. Existing FVF, texture and SM5-cache helper
fixtures also pass.

The private real-provider window fixture creates VS/PS 1.1 programs, verifies
native shader/declaration readback forwarding, deletes them, and retains the existing SM5
probe, legacy render-state/resource/Present/Reset checks. Wine can leave the
input capacity unchanged for a successful nonnull readback buffer; the fixture
queries the required length separately rather than assuming that capacity is
overwritten. The preserved dxwrapper chain returns a 32-byte translated vertex
program for the 20-byte input and refuses a null-buffer declaration query
(D3DERR_INVALIDCALL). The final fixture compares getter HRESULTs, sizes and bytes
directly with the saved native provider functions; capture fidelity is checked
separately against the original CreateShader arrays. These provider limitations
are preserved, not converted to successful calls. The earlier failing fixture
assumptions and their logs are retained. A sandboxed GPU run failed before
device creation; the GPU-access runs exercise the actual provider contracts.

Receipts are under `build/renderer-shader-capture-20261021/`. These are synthetic
fixtures, not native-game acceptance. No reference implementation was copied.
Public token fields were checked against the installed D3D8 SDK and Microsoft's
[instruction-token description](https://learn.microsoft.com/en-us/windows-hardware/drivers/display/instruction-token)
and [opcode formats](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/d3d9types/ne-d3d9types-_d3dshader_instruction_opcode_type).

## Manual capture

Use the existing Roots installation with `[D3D8Compat] Backend=d3d8` and temporary
`Trace=1`. Fully restart, reach the menu, load a small Instant Action map, view
ships/stations, and exercise cloaking if readily available. Quit normally so
the log/trace can be preserved before another launch. No triangle is expected.
Check normal rendering and minimap markers. This captures native usage; it
does not switch gameplay to D3D11. Restore Trace=0 after preserving the run.

Next, classify the captured programs and declarations, implement their required
resource/state/constant contracts, and validate an actual legacy draw replay.
Do not silently skip unsupported calls or disable existing visual paths.

## Deployment

Core SHA256: `a7e4d0d1da4e291de1c18cef3a3f0d633ed7818de0540a272758d889c87d733c`.
The prior tested core, complete INI and fresh logs were backed up under
`.deployment-backups/shader-capture-20261021-100325Z/originals/Data/`.
Its `receipt.json` records before/after hashes and 96 protected files verified
unchanged, including corrected ShellDisplay, wrappers and mod configuration.
Only the core and `[D3D8Compat] Trace=0` to `Trace=1` changed; Backend remains
`d3d8`. Eight Windows fixture processes and eight Python tests passed before
deployment. No mandatory D3D11/DXGI/compiler imports were introduced.

To roll back this capture build, close the game and restore only
`A2FOExtensions.dll` and `A2FORenderer.ini` from this backup. That restores the
tested triangle-capable core with the probe disabled and Trace=0, preserving
the separate minimap fix. A native game shader capture is still pending.
