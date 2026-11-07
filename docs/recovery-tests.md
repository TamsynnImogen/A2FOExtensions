# ArmadaL recovery observation module

`A2FORecoveryTests.dll` is an explicit diagnostic build. Its
[module guide](../modules/A2FORecoveryTests/README.md) contains configuration,
manual activation, capture replay, and acceptance steps.

## Ownership and ABI

The module owns only context lookup `0x62b710..0x62b721` and node transform lookup
`0x639460..0x63951b`. Both complete bodies take the object in ECX, take no stack
arguments, return raw EAX with plain RET, and contain no CALL instructions.
The first six bytes are whole relocatable instructions: context's
`MOV EAX,[ECX+0xc0]`, and node's `PUSH EBP; MOV EBP,ESP; SUB ESP,0x0c`.
Existing shared transform/animation hooks target different routines.

Evidence sources are the recovered
[engine context evidence](../reverse_engineering/ArmadaL/scripts/engine_context_evidence.py)
and [node transform evidence](../reverse_engineering/ArmadaL/scripts/node_transform_evidence.py),
the complete pinned arrays in
[signatures.hpp](../modules/A2FORecoveryTests/signatures.hpp), and
the read-only signature verifier. Historical map names do not establish ABI.

The bridge saves all general registers, EFLAGS, and the full 512-byte FXSAVE
state before each C++ callback, clears DF and resets arithmetic state for that
callback, then restores the game state. It calls the original gateway exactly
once and restores the original post-call state before returning. Callbacks also
preserve Win32 LastError. Comparison code uses the recovered routines' token
memory interfaces against local buffers; those interfaces are not native game
function signatures.

The bridge's private frame pointer is at entry ESP minus 40. Including the
gateway CALL, the two pinned bodies touch at most 24 bytes below entry ESP.
The aligned observer frame is farther below that footprint. This bridge is
specific to these complete bodies: do not reuse it for callees with arguments,
CALLs, deeper stack use, or changed code.

Gateway memory is built, made executable, flushed, and published before the SDK
patch exposes the bridge. The SDK validates the displaced bytes again. All
selected complete bodies are checked before either hook is installed. A patch
failure may leave a successfully installed observer on the other routine; it is
logged and retained safely. Neither gateway nor pinned module is freed during
shutdown. The SDK host is pinned too, keeping its API table and callbacks
resident. No core or SDK changes are required.

## Capture and comparison

Sampling uses one nonblocking busy flag for both routines. Contended observations
are counted without waiting. `ReadProcessMemory` captures the node or three
context words and rereads the post-state. A capture failure counts as skipped;
it never substitutes a result or prevents the original call.

Context replay checks wrapped addresses, aliases, input stability, and raw
return equality. Node replay skips input changes outside the output cache,
reproduces finite scaled arithmetic with the captured x87 control word, and
compares the whole node image plus return pointer. The exact-unit path expects
no writes. Raw translation bits are retained, including nonfinite payloads.

The module does not make game memory atomic. Snapshots can miss a transient
mutation, and another thread can alter cache bytes without changing captured
inputs. Reports therefore identify discrepancy candidates. They become useful
recovery evidence when reproduced on controlled input and checked against the
original, disassembly, and relevant call sites.

## Evidence boundary

Portable tests check comparison positives, deliberately wrong results/cache,
domain/stability skips, wraparound/aliasing, and FP preservation. Python tests
check capture replay and malformed/provenance rejection. The Windows fixture
uses private copies of both pinned bodies, compares register/stack/EFLAGS/full
FXSAVE state against direct execution, and exercises checked detours, passive
guards, budgets, and capture output. These establish fixture behavior and build
compatibility; they do not prove installation into a running pinned game image.

The validation manifest records the delivered artifact identity and actual
headless outcomes. No installation files are modified by building or checking
this module. In-game acceptance is pending. Adding this observer does not count
as recovering additional ArmadaL routines.
