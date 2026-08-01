# Current architecture

## Startup and ownership

`Win2kDisableTaskSwitch.dll` is a startup proxy. During process attachment it
loads the renamed shipped startup DLL and attaches `A2FOExtensions.dll`, keeping
Fleet Ops' required early load order. The core performs deferred initialization
on a worker outside the Windows loader lock.

The core permanently owns shared or lifetime-sensitive engine sites:

- checked patch installation and supported-binary validation;
- the FOFS item-lookup dispatcher;
- ParameterDB/classlabel and Evolver/cocoon dispatch;
- Craft destruction snapshot, replacement construction, and publication;
- extension-root overlay and native/Lua loading order.

`A2FOFeaturePack.dll` owns optional policies and feature-specific integration:

- recursive loose-folder and FPQ ODF indexing;
- `wingman -> craft` and the `cocoon` command;
- Ctrl-fill and Ctrl+Alt continuous Producer queues.

The queue feature chains feature-specific Armada and Fleet Ops Producer sites
after exact signature checks. If the supported build or any required signature
does not match, the affected feature is disabled rather than patching an
unknown binary.

## Deterministic extension overlay

Roots are ordered from lowest to highest precedence: shared `Data`, each
`ParentMod`, then the active mod. DLLs and scripts with the same case-insensitive
basename are replaced by the higher-precedence copy. The resulting native DLLs
and Lua files execute in case-insensitive filename order.

## Registration transactions

Each `A2FO_ModuleInit` and each Lua startup chunk is a transaction. The core
records its dispatcher registrations and ownership claims. If initialization
returns false, throws where catchable, or reports a Lua error, the core rolls
that script/module back before continuing. A rejected DLL is unloaded only
after its registrations have been removed.

Registrations are startup-only. Low-level hooks installed directly by a module
cannot generally be undone safely; modules must therefore validate everything
that can fail before installing their first hook. The feature pack initializes
its queue hooks last for this reason.

## Destroyed-object flow

```text
Craft::Explode (checked core hook)
  -> copy handle, team, transform, source ODF and declared ODF fields
  -> native handlers in module/registration order
  -> Lua handlers in script/registration order
  -> first valid claim wins
  -> core finds, constructs, positions and publishes replacement
  -> original explosion continues
```

Every handler declares its required ODF field names at startup. The snapshot is
the case-insensitive union of active declarations, plus `basename`. Native
event pointers and Lua ODF views are callback-scoped. No script receives an
engine pointer, and the core validates replacement names, flags, and ownership
before acting.

This dispatcher is also useful groundwork for later Noxter mechanics: it gives
future infestation or spawn-on-death modules a deterministic, synchronized
lifecycle event without competing for the Craft destruction hook.

## ABI compatibility

The native ABI remains major version 4. Revision 1 only appends fields, exposes
its revision and capability mask, and provides a member-size macro. Existing v4
modules continue to receive their original struct prefix. Lua has an independent
major/revision pair and `a2fo.require_api`.

See [`../sdk/README.md`](../sdk/README.md), [`lua-api.md`](lua-api.md), and
[`addresses.md`](addresses.md).
