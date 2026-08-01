# Stage 2: semantic ODF module boundary

> Historical design note. For the current ownership and startup model, see
> [`architecture.md`](architecture.md).

Stage 2 keeps the timing-sensitive FOFS call-site patch in the core and moves
the recursive ODF feature behind API v2:

1. The core installs and signature-checks one FOFS item-lookup dispatcher before
   Armada begins loading ODF classes.
2. Deferred initialization loads native modules outside the loader lock.
3. `A2FOFeaturePack.dll` validates its additional Fleet Ops scanner functions
   and registers the dispatcher handler.
4. The module owns recursive directory/FPQ discovery, VFS registration,
   precedence calculation, and winner selection.
5. Returning `false` from the handler always preserves Fleet Operations' native
   hash lookup.

Recursive winner selection uses explicit mod, root, and loose/packed metadata.
It does not discard an entry solely because Fleet Operations marked it
`overridden`; that flag is unreliable for a parent's recursive loose ODF when
a child mod adds another root. This preserves normal child-over-parent priority
while allowing a loose parent ODF to override its own packed parent copy.

The former built-in recursion implementation was removed after the module path
was confirmed in Fleet Ops. The module was later broadened and renamed when it
also took ownership of the wingman compatibility alias and Evolver cocoon
command. The core owns hook safety and dispatch; the feature pack owns these
three optional native behaviours.
