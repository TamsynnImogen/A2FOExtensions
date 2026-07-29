# Supported binary addresses

All values below are RVAs. Runtime addresses are calculated from the module's
actual load base.

## ArmadaL.exe

SHA-256: `88347ab635e521bf8d86c78c558d7c6a79e81d01fa374bf84ad1c44cad3c44e7`

Purpose | RVA
--- | ---:
EvolverClass::BuildClass | `0x0a85e0`
EvolverClass destructor | `0x0a85d0`
Fleet Ops cocoon selector jump | `0x0b0534`
ParameterDB::Get_String | `0x135350`
SOD database load method | `0x22cf10`
SOD database singleton pointer | `0x3ad508`
Default cocoon geometry pointer | `0x33fccc`
Alternative cocoon geometry pointer | `0x33fd3c`

## FleetOpsHook.dll

SHA-256: `83c545ff757b069bc9a9c25d206c022a962021c535ffd1ce27054a3fe6b891a9`

Purpose | RVA
--- | ---:
Delphi `@LStrClr` | `0x0056b8`
Delphi `@LStrFromPChar` | `0x0058b0`
Delphi `TList.Add` | `0x080824`
FPQ basename hash function | `0x0fa83c`
TFOFSVirtualDirectory class reference | `0x10870c`
TFOFSVirtualDirectory constructor | `0x108b6c`
TFOFSVirtualDirectory.RenewOverrides | `0x108c14`
TFOFS.AddFileToHashTable | `0x1092d0`
TFOFS.AddItemsFromDisk | `0x109488`
TFOFS.AddItemsFromPack | `0x109650`
FOFS_ItemGet hash-lookup call | `0x105fec`
TFOFS.GetFileFromHashTable | `0x109c14`

These addresses came from the shipped Fleet Operations map, the Armada II 1.1
debug symbols, and byte-level verification against the supported binaries.
The `FOFS_ItemGet` call-site hook runs only on Fleet Operations' direct parser
lookup path. It preserves the native hash lookup as a fallback and returns a
recursive file entry only when that entry won the normal mod/root/packed
precedence calculation.
