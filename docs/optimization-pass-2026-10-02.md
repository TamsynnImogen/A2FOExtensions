# A2FOExtensions optimisation pass 1 — 2026-10-02

This pass targets the FleetOps virtual-filesystem hot path before touching renderer pointer validation or D3D state handling.

## 1. Lock-free published FOFS dispatcher

`core/dllmain.cpp`

The FOFS interception hook previously entered `g_state_lock` for every `GetFileFromHashTable` interception solely to copy the registered handler and user-data pointers. Module policy registration is startup-only and has an explicit close point, so the final dispatcher is now copied into immutable process-lifetime storage when registration closes.

An acquire/release atomic readiness flag publishes that storage. During startup, while rollback is still possible, the hook keeps the old synchronized path. During normal gameplay it reads the published dispatcher without taking `g_state_lock` and no longer waits for deferred initialization once publication has occurred.

Expected effect: lower synchronization overhead on every FOFS get/locate/exists/project-ID lookup, especially under Wine where critical sections in very hot paths can become visible.

## 2. Recursive ODF lookup miss path no longer locks

`modules/A2FOFeaturePack/odf_recursive.cpp`

The two ordered maps used for recursive ODF winners and aliases were replaced with one `std::unordered_map<std::string, RecursiveOdfWinner>` containing both the FleetOps entry pointer and resolved path.

The table is constructed while `g_recursive_odf_state == 1` and becomes immutable before state 2 is published. Once state 2 is visible, readers can safely perform concurrent lookups without `g_state_lock`.

`g_live_file_system` is now an atomic pointer, removing another lock that previously occurred at the beginning of every FeaturePack FOFS lookup.

The common case for a file that is *not* a recursive ODF winner is now:

1. normalize the requested basename;
2. one unordered-map lookup;
3. return to the native FleetOps hash lookup.

Actual recursive ODF hits still enter `g_state_lock` for mutable lazy project-ID registration and one-time lookup logging.

## Build/test status

The source changes were reviewed structurally, but this environment does not contain `i686-w64-mingw32-g++`, so the Windows DLLs could not be rebuilt here.

Recommended first validation on the normal A2FOExtensions build machine:

```sh
make build/A2FOExtensions.dll build/modules/A2FOFeaturePack.dll
```

Then smoke-test:

- clean Fleet Ops startup with FeaturePack enabled;
- recursive ODF from the active mod;
- inherited recursive ODF from a parent mod;
- packed-vs-loose precedence;
- BuildYard/project-ID resolution for a recursive ODF;
- `FOFS_ItemGet`, locate and exists behaviour for ordinary non-ODF files;
- startup with a module registration failure/rollback.

If those are clean, the next optimisation pass should target repeated `VirtualQuery` validation in validated renderer/CraftIdentity hot paths.
