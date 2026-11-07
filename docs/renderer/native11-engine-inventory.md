# Native11 stage 51 engine inventory

This is a bounded static inventory for the exact pinned
`build/native11/capture-profile-input/ArmadaL.exe` image, SHA-256
`4300ab71731e323f5767d8ee05457e6444401b823253aebee86c5ac23c2b00de`.
The checker is `tools/check_native11_engine_inventory.py`; run it with
`--image PATH --output PATH`. It verifies PE32 file-backed ranges, selected
instruction and data anchors, the `_ftol` import, and relocation blocks before
writing JSON plus one disassembly sibling per range. The JSON includes exact
bytes and their hashes, relocation offsets, assembly hashes, and the image hash.
The expanded inventory contains 14 ranges; its optional clock-header output
selects eleven checked regions and two dispatch slots for read-only diagnostics.
The complete animation-channel loader is split into two regions so regeneration
preserves the runtime channel reader's checked name, type and target mapping.
All addresses below are RVAs unless explicitly described as preferred VAs.
Names are working labels from the historical map and disassembly, not verified
replacement interfaces or ABI contracts.

| Range | Reviewed RVA, end exclusive | Observed role |
| --- | --- | --- |
| instance render | `0x22e780..0x22e987` | Sets engine render state, visits animation channels, then calls DB draw |
| instance animate | `0x22eab0..0x22eb13` | Sets animation time and visits channels |
| channel phase | `0x213910..0x213a7b` | Computes frame/key phase and calls rounding helper and `_ftol` |
| start time | `0x213670..0x2136ba` | Updates shared start and elapsed time |
| force first | `0x2136c0..0x2136cb` | Clears shared elapsed time |
| rounding | `0x244a10..0x244a30` | Changes x87 control-word rounding bits |
| refresh | `0x224630..0x224743` | Contains final EndScene and Present path |
| first channel | `0x22f1c0..0x22f1d3` | Reads DB count and first list payload |
| next channel | `0x221080..0x221096` | Uses channel owner and list backlink |
| engine time | `0x22d530..0x22d537` | Reads engine time at +0x8c |
| float channel play | `0x2146a0..0x214720` | Float-channel evaluation route |
| matrix channel play | `0x2161c0..0x216218` | Matrix-channel evaluation route |

At render `0x22e78a..0x22e794`, the code loads the preferred-VA engine
pointer at `0x7ad508` and writes engine `+0x94`. At `0x22e81d`, it writes the
current instance to engine `+0x100`. Both render branches select the calls at
`0x213670` (SetStartTime candidate) or `0x2136c0` (ForceFirst candidate),
then loop through DB channel virtual slot `+0x10` before DB draw. The animate
range also makes these time and channel calls. The stored DB node hierarchy and
shared mutable global time therefore do not establish an independently fresh
offscreen pose. They do not justify replaying simulation or suppressing native
draws for stage 53.

The start-time function writes preferred-VA `0x7a8e78` and updates elapsed
time at `0x7a8e74`; the channel-phase range reads elapsed time and the engine
pointer. It directly calls helper RVA `0x244a10`. Calls from channel phase to
RVA `0x273024` reach a jump through IAT RVA `0x3b806c`, verified by PE imports
as `MSVCRT.dll!_ftol`. Checked float bit patterns are `0xbf800000` at RVA
`0x2ae6f0` (-1.0f) and `0x3d4ccccd` at RVA `0x2bc364` (approximately 0.05f).
These observations bound stage 52 recovery work; no arithmetic parity claim is
made here.

The final RefreshDisplay path at `0x224710` begins with a load from wrapper
`+0x90`, push and vtable load. Its call at `0x224719` uses vtable offset
`+0x8c`; the later Present call at `0x224735` uses offset `+0x3c`. This is a
candidate context for final-frame integration only. Static bytes do not prove
replacement ABI, ownership, independent startup, or in-game acceptance.

Reproduce the evidence with:

```sh
python3 tools/check_native11_engine_inventory.py \
  --image build/native11/capture-profile-input/ArmadaL.exe \
  --output build/native11/stage51-engine-evidence/inventory.json
```

The inventory closes only stage 51's listed static boundary work. Stages
52–65 in `native11-stage51-65.md` retain their separate producer, integration,
deployment, and native acceptance gates.
