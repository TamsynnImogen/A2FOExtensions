# Borg BuildYard tooltip ABI — v13

Status: phase 1 `TEST` visibly accepted in FOtest on 2026-09-24, with a user
screenshot and native stream readback (130 -> 135 bytes). Phase 2 replaces TEST
with missing requirements through that same append path. The user also confirmed
that the old build allowed installation without `bor_incubationZ`; the narrow
native count correction below addresses a concrete gate bug. Phase 2 is also accepted: the user confirmed requirement text, refusal to
install without the prerequisite, and disappearance/unlock after completion on
2026-09-24. The supplied screenshot visibly shows `Requires: Incubation Center`.

## Binary identity and evidence

All Fleet Ops addresses below are RVAs; its preferred image base is `0x5a800000`.
Armada addresses below are preferred VAs; its image base is `0x00400000`.

| Input | SHA-256 |
|---|---|
| supplied FleetOpsHook.dll | `0843058788c6ffd1138d69871e2d9b1911bb6be8ee06db42f59f56b9a4494a97` |
| supplied ArmadaL.exe | `63bd645d82aec75cdd7ee3e3271fb67bb54f4ae1d1636a736b0db620bd997e36` |
| FO3/Data/MSVCP60.DLL used for native tests | `2b6b93c2d66969eb00258e2b5ad6172decebada096e3b1b077a3380c80e4a072` |

The latest supplied live log records v11 provider invocation at module0, key
`AUTOTOOLTIP-bor_assimilator_moduleB.odf`, gate `0x70000000`, without an append
result. This establishes entry, not which subsequent early-return condition ran.
The existing v11 code skipped team/evaluation/empty-text cases silently. Therefore
neither the ESI defect nor localization filtering is claimed as the proven sole
cause of that run. Phase 1 removes those dependencies and checks stream bytes.

Disassembly excerpts are in `docs/buildyard-tooltip-disassembly.txt`; full local
objdump output and the before-edit source/DLL snapshot are under
`build/buildyard-tooltip-abi/`. The old renderer log is from September 22; the
September 24 startup proxy and extensions logs establish successful latest loading.
Historical proxy load failures are not evidence of the current tooltip failure.

## Caller and object layout

Armada `0x505a80` constructs tooltip streams of **0x8c bytes**. It stores the normal
stream in `Tooltip+0x34`, the verbose stream in `Tooltip+0x38`. These are stream
objects, not text pointers. At `0x505baa`, the normal path calls component vtable
slot 0; at `0x505cdc`, verbose calls slot +4. Both pass ECX=component and push the
complete stream pointer. They then call imported `basic_stringstream::str`, inspect
its returned string's size at +8, destroy that temporary with `_Tidy(true)`, and
append `std::ends` after the provider has returned. Thus our append runs before
the final terminating NUL is inserted by the caller.

| Offset in stream | Native subobject |
|---|---|
| +0 | basic_istream / stringstream base (virtual-base table pointer) |
| +8 | basic_ostream subobject; provider inserts here |
| +0x0c | basic_stringbuf, initialized with mode 3 |
| +0x58 | shared basic_ios / ios_base; virtual destructor lives here |
| total | 0x8c bytes allocated by both tooltip caller paths |

Fleet Ops constructor `+0x13d42c` installs vtable `base+0x243640`, stores config at
button+0x78 and module index at +0x7c. Initialization at `+0x13d51c` writes the
same provider into the first two virtual slots. The module record is 0x60 bytes:
+8 is RequiredTechnology, +0x0c is the tooltip key character pointer. The hook
uses the actual stored synthetic ID, not a possibly shared key, for eligibility.
The key is borrowed; neither provider nor hook assumes ownership of its storage.
No Delphi string helper is called on this path.

## Complete provider flow (+0x13d4a4 through +0x13d519)

1. ECX is the button; `[entry ESP+4]` is the complete stringstream pointer.
   The function returns with `ret 4`; EAX is not a useful provider result.
2. Compute record index * 0x60, read config record+0x0c as the key.
3. Set **ESI = stream+8** (or zero). The function does **not save/restore ESI**.
   This violates the normal thiscall callee-saved contract. The new dedicated
   `a2fo_buildyard_call_tooltip_provider` bridge preserves ESI for compiled C++.
   Generic thiscall bridges are unchanged.
4. Push key, then address `[EBP-0x18]`, call `[FleetOps+0x210a48]`.
   The slot contains Armada `0x482300`.
5. Push returned EAX (address of the temporary string), push ESI, call
   `[FleetOps+0x20e640]` (Armada `0x672d9c`). Both calls are cdecl; `add esp,16`
   cleans the combined four arguments.
6. Read temporary+4 (payload pointer). If non-null, inspect byte `payload[-1]`.
   If zero or 0xff, call `FleetOps+0x1e1c5c(payload-1)` as cdecl.
   That forwards through +0x1e3ca8 and +0x1e3bec to the msvcrt `free` IAT entry
   at +0x24cac0. The original provider does nothing for other marker values.
7. Restore EBP/ESP and `ret 4`. There is no delayed use of its temporary.

The temporary occupies 16 bytes at EBP-0x18..-0x09, not all 24 bytes of local
stack allocation. The other locals are the key and saved button.

## String helpers and ownership

| Helper | ABI and return |
|---|---|
| Armada 0x482300 | cdecl, explicit hidden result pointer at stack+4, `const char* key` at stack+8; returns the same result-object address in EAX |
| Armada 0x482150 | thiscall localization hash lookup; ECX manager, one key pointer; `ret 4`; returns borrowed char pointer or null |
| Armada 0x672d9c | `jmp [0x7b7dac]`, imported cdecl `std::operator<<(ostream&, const std::string&)`; returns ostream address |
| IAT 0x7b7dcc | thiscall `std::string::_Tidy(bool)`; ECX=string, one callee-cleaned stack argument |
| IAT 0x7b7dd8 | thiscall `std::string::assign(const char*, unsigned)`; ECX=string, stack chars then length; `ret 8`; returns string address |
| IAT 0x7b7db0 | thiscall `basic_stringstream::str()` with explicit hidden result pointer on stack; ECX=complete stream, `ret 4`; returns result address |

The export names are checked against Armada's loaded IAT pointers using
`GetProcAddress` from the already-loaded MSVCP60. No helper RVA from a different
runtime is assumed. The insertion slot and its entire six-byte thunk are checked.
The provider's complete 118-byte body is checked, with its two absolute slot
operands adjusted for image relocation. Only its first six whole instruction
bytes are overwritten for the existing inline-hook mechanism.

The result object is **MSVC6 std::basic_string<char>**, not Delphi AnsiString,
not a C string and not a handle:

| Offset | Field |
|---|---|
| +0 | allocator byte plus padding (four bytes total) |
| +4 | character payload pointer |
| +8 | character count, excluding NUL |
| +12 | capacity |

The payload has a one-byte sharing marker at -1. `_Tidy(true)` handles both owned
and shared buffers (including reference decrement); `_Tidy(false)` initializes
empty storage. Do not free it through MinGW, use Delphi `_LStrClr`, pass a MinGW
`std::string`, or manually mimic just the zero/0xff branch for arbitrary shared
strings. The new RAII temporaries call the actual native `_Tidy` on every normal
exit; inputs remain alive until insertion and readback finish. Insertion copies
the text into the native stream and does not retain our temporary object.

Resolver `0x482300` consults the manager at 0x7379fc. Depending on manager flags,
it calls `0x482150`; a missing key can become the empty literal at 0x6fab24
instead of preserving the input. It then constructs a native string via
`_Tidy(false)` / `assign`, processes angle-bracket formatting, copies into the
hidden result and releases the local. Consequently arbitrary literal text
must not be treated as a localization key. Phase 1 creates literal `\nTEST`
with native `assign`, then uses the **same** string insertion path as the provider.

In the tested native runtime, the string insertion overload (+0x2695) borrows
string+4 and forwards to the C-string insertion routine (+0x26b5). That routine
gets the length, checks stream state, handles width/fill, and calls streambuf
vtable+0x1c with ECX=buffer and stack=(chars,length); its bulk writer (+0x27b7)
copies into the put area and calls overflow when growth is needed. It returns
synchronously. `stringstream::str` (+0x15620) forwards to stringbuf+0x0c's getter
(+0x141b8), which constructs an owned native string from the put-area extent.
These additional callees explain why preserving the native stream and temporary
lifetimes matters; no internal buffer pointer is replaced by our hook.

The hook preserves the stock provider first. It snapshots the stream, constructs
the literal, inserts at stream+8, snapshots again, and checks exact prefix equality,
length growth by the literal length, and exact suffix bytes. Phase 1 used
`\nTEST`; phase 2 uses `\nRequires: ...`. Empty/embedded-NUL literals are
rejected. Logs are once per module/key/gate/team/result/requirement expression. This is stream verification, not a claim about rendered pixels.
Foreign C++/SEH exceptions are not assumed to unwind safely through MinGW. The
local catch is for local C++ failures; native out-of-memory exceptions are not a
validated recovery path. No speculative native exception handler is added.

## MODULE object-count correction

The original evaluator called `FleetOps+0x1def64` (EAX=&projectID, EDX=team).
That forwards through slot +0x210cd0 to Armada 0x498d60. Fleet Ops patches that
entry to +0x1207b0 (installation at +0x121236), whose thiscall adapter invokes
+0x120680. This tests **build availability**, not completed ownership. Thus a
buildable but absent Incubation Center incorrectly satisfied MODULE `<obj>`.

Native XML evidence establishes the replacement rather than its address alone:

- XML parsing at +0x11f4ba recognizes `obj` and reads `mustNotHave`.
- Group evaluation calls +0x11f0cc at +0x1214d8 with EAX=team tree,
  EDX=project ID, ECX=0 and requires signed EAX > 0. The negative branch calls
  it at +0x1214ff and requires EAX == 0.
- `TTechItemObjectRequirement` evaluation at +0x1219dc likewise tests its
  referenced technology item's signed +0x14 count, using >0 or ==0.
- Completed-object registration increments item+0x14 at +0x1207eb.

`+0x11f0cc` is a complete 36-byte Delphi register routine. It indexes
`[tree+0x0c][projectID-1]`, returns zero for a null item, otherwise reads signed
item+0x14. Nonzero CL additionally includes item+0x18 (queued count). EAX returns
a scalar count; no string, allocation, ownership transfer or cleanup occurs.
EBX is preserved. The extension uses `regparm(3)` with ECX=0, after validating
all 36 bytes. The call adds no hook.

The pointer slot +0x212f08 contains bytes `0c 10 a4 5a`, the preferred
team-table address 0x5aa4100c. XML callers iterating from 0x5aa41010
start at team 1 in this same table. Native initialization
+0x1211d8 creates exactly nine trees, indices 0..8. Before calling, the extension
checks that bound, the tree/items pointers, the Delphi dynamic-array length at
items-4, the project index, the item slot and count field. Unresolved IDs,
unavailable count ABI and unreadable data fail closed, including mustNotHave.
Only MODULE direct `<obj>` evaluation uses this change; separately configured
native RequiredTechnology still uses its original availability semantics.

The tooltip uses the separately signature-checked Armada local-team getter
0x4d0060 (cdecl, scalar EAX; tailcalls 0x5578b0). Gameplay evaluation continues
to use the team argument supplied to the existing gate hook. Display names and
hover state never influence gameplay decisions. No RNG, writes to technology
counts, new synchronization or new gameplay hook is introduced. All multiplayer
participants must use the same extension build; actual multiplayer testing is
still pending.

Formatting removes satisfied objects from each AND group. Multiple remaining
alternatives are parenthesized and joined by OR. Any fully satisfied alternative
makes the symbolic condition pass. A blocked separate native RequiredTechnology
is retained as `Required technology`, without showing already-satisfied symbolic
branches. `mustNotHave` is displayed as `Not <name>`. The actual requirement ODF's
cached `unitName` is used, otherwise its basename; the MODULE label is never
resolved as an ODF to generate this text.

## Tests and boundaries

The focused tests execute real MSVCP60 helpers and the supplied FleetOpsHook
provider and count routine with controlled engine data. See
[the test guide](../BUILDYARD_TECHNOLOGY_ENTRY_TEST.md) for commands and coverage.
The provider fixture replaces localization with `Native tooltip`; it captures
the hook gateway rather than installing a detour, so core detour installation
and actual game object-count updates remain in-game checks.

The inactive v6–v9 functions remain uninstalled and are ignored; no global
localization, StandardComponent setter, or render hook is added. Normal hover and install denial/unlock are accepted in-game. Additional verbose
hover, extended clipping/repetition checks, save/load and multiplayer testing
remain outside that acceptance.
