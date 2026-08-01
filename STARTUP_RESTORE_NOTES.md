# Stage 1 startup-order restoration

> Historical restoration note. The live architecture has since moved optional
> behaviours into `A2FOFeaturePack.dll`; see `docs/architecture.md`.

This build restores the startup order of the previously working monolithic
A2FOExtensions build:

1. Proxy attaches the original startup DLL immediately so FleetOpsHook loads
   before Armada's entry point.
2. A2FOExtensions attaches immediately and installs the proven early recursive
   ODF and evolver hooks.
3. Its deferred worker installs the classlabel alias and completes validation.
4. Native modules are loaded last, outside loader lock.

For the first test, leave the `modules` directory empty or absent.
