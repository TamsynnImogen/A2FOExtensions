# Rust-owned x86 ABI bridges

`manifest.json` maps the 48 historical assembly families to their authoritative Rust sources. Each source uses Rust `global_asm!` for the exact ABI instructions, with an explicit i686 Windows GNU target guard. The former `.S` files remain as historical reference inputs; production Make rules compile the `.rs` files with `rustc`, without preprocessing or assembling the reference files.

These routines translate Delphi register, MSVC thiscall and custom interrupted-register/stack conventions. They intentionally have no compiler-generated Rust entry/exit sequence. Keeping each family in a separate object allows the main DLL and standalone compatibility modules to select their own callbacks without pulling in other modules' undefined symbols. Built-in CameraControls, ViewDistance, FeaturePack and HybridBuild variants retain their existing symbol isolation.

`tools/build_rust_bridge.py` validates the source registration, pins source/manifest/tool bytes, builds to a temporary object and publishes it only after a successful, unchanged-input compile. Each object has an adjacent compiler log and receipt. Make dependencies include the source, manifest and builder.

Verification compares all nonempty code/data sections, including COFF flags, symbol offsets, undefined symbols, raw relocation fields and referenced symbols. Object file timestamps, file-name records and empty sections are not instruction-equivalence claims. GNU COFF's final one-byte NOP padding is emitted explicitly so section sizes and offsets also match. The checker rejects actual object mutations to an instruction, exported offset and relocation target.

Build the selected main/standalone target through Make, which prepares these objects before invoking the Cargo DLL linker. `make rust-hook-bridge-build` builds all 48 objects and the headless instruction fixtures. Wine execution is a separate coordinated step; these build targets do not launch applications. To reproduce the frozen/GAS differential object check, provide a fresh output directory:

```sh
python3 tools/check_rust_bridges.py \
  --before build/rust-migration-stage13/hooks-assembly/before \
  --work-dir build/rust-migration-stage13/hooks-assembly/object-recheck-new
```

The 306 symbol definitions are counted across objects; some names intentionally repeat in separate DLL families. They are not 306 public DLL exports. The core DLL's 197-export ABI is checked separately. Byte equivalence preserves the existing bridge behavior; it does not establish that every original engine-side assumption is correct or that every game hook has been exercised.

Rust's [assembly reference](https://doc.rust-lang.org/reference/inline-assembly.html) describes global assembly outside compiler-generated function frames. The verifier follows Microsoft's [PE/COFF format](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format).
