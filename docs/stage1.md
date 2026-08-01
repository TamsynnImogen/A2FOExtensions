# Stage 1: native module boundary

> Historical design note. For the current ownership and startup model, see
> [`architecture.md`](architecture.md).

This stage changes startup ownership without yet moving working features:

1. The startup proxy's `DllMain` loads the renamed shipped startup DLL, whose
   own attach path loads `FleetOpsHook.dll` before Armada starts.
2. The proxy then attaches `A2FOExtensions.dll` so its checked recursive ODF
   and cocoon hooks retain the proven monolithic startup timing.
3. The core schedules `A2FO_Initialize` on a worker serialized behind DLL
   attachment.
4. The worker validates Armada/Fleet Ops, installs the classlabel alias, and
   then loads `modules\\*.dll` outside the loader lock.
5. Native modules receive a versioned API containing logging, module handles,
   root path, and checked patch helpers.

The shipped startup chain and the proven early built-ins still attach before
Armada's entry point. Third-party module discovery never runs under loader lock,
while the native ABI establishes the boundary needed for Stage 2.
