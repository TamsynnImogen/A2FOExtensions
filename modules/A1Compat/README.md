# A1Compat native module

`A1Compat.dll` owns Armada 1-specific compatibility policy for the
`STA1 Classic` parent mod. The DLL is installed centrally in `Data/modules`
and selected as a required module by that parent's `info.ini`. The parent and
its children activate the policy through the separate marker described below;
ordinary mods cannot inherit it accidentally.

Initialization also requires `a1compat.ini` in one of the active extension
roots. Selecting the DLL for an unrelated mod therefore rejects initialization
rather than enabling A1 policy. The marker is owned by the parent and is
inherited automatically by its child addons.

The marker may also contain one diagnostic setting:

```ini
[A1Compat]
SafeMode = 0
```

`SafeMode` defaults to false and accepts `1/true/yes/on/enabled` or
`0/false/no/off/disabled`, case-insensitively. When enabled, the module keeps
the `wingman` alias, missing-only class defaults, and `Addon` overlay but does
not install its riskier executable diagnostics or officer-quarter runtime
hooks. This is intended for isolating a conversion-specific startup failure,
not for ordinary play.

The initial implementation registers the Armada 1 classlabel alias:

```ini
classLabel = "wingman"
```

as Armada's compatible native `craft` class. The checked ParameterDB hook and
transactional registration remain owned by `A2FOExtensions.dll`; this module
owns the declarative A1 policy at that shared site.

The module also declares `Addon` as an override-priority ODF directory. The
FeaturePack's shared recursive filesystem index consumes that declaration, so
A1 ODFs remain in their original folder and an `Addon` file wins over a
same-basename structured ODF in the same mod root. Child/parent mod precedence
still wins before this within-root rule.

The module additionally owns a signature-checked inline guard for
`NebulaClass::s_SetTexturesRecursive` at Armada RVA `0x0009dd40`. A legacy SOD
can produce an `ST3D_SpriteNode` whose type-specific-data pointer at offset
`0xc0` is null; native code dereferences it at RVA `0x0009dd62`. A1Compat logs
the node and parent names and skips only that invalid node. Valid nodes execute
through the native gateway unchanged. See `docs/addresses.md` for the complete
address and byte contract.

A temporary non-bypassing diagnostic also observes
`RtimeClass::Load(FileReader&)` at Armada RVA `0x0013c2da`. It reports a
serialized 40-byte class name which has no registered runtime factory, then
continues through the native gateway unchanged. The checked site starts after
an absolute-address instruction so the signature itself contains no
image-base-dependent operand.

Two further non-bypassing diagnostics retain the same evidence-first policy.
The Armada RVA `0x0010ad23` site reports a missing generated StandardText
sprite before native GUI code dereferences it. The Fleet Ops RVA `0x001dbdcb`
site reports the exact craft and object chain when `Craft_mLevelUp` receives a
Side whose Race pointer is null before the native `Race+0x634` `canGainXP`
read. Both execute their displaced instructions through checked gateways and
therefore leave native failure behaviour unchanged.

Officer-quarter compatibility restores the A1 starbase progression on top of
FO's retained A2 classes. Numeric A1 commands are read through the validated
`ParameterDB::GetString` entry and parsed locally. Armada RVA `0x00135200` is
`ParameterDB::GetProjectId`; treating it as `GetInt` corrupts project-ID
registration and is explicitly forbidden. The module does not detour that site.
The module parses `maximumUpgrades` and `officerGain`
after `StarbaseClass::BuildClass`, tracks completion per starbase, and prepares
exact `oq<number>` model nodes in `Starbase::InitializeGeometry`. Models with no
numbered `oq` nodes remain unchanged.

FeaturePack owns Fleet Ops' Producer hooks and emits admission,
claimable pre-completion, post-completion, and destruction events. A1Compat
uses the shared admission/destruction channel to reject wrong-race orders,
enforce the A1 maximum, and release sidecar state. Because A2's retained
upgrade `Build()` returns null, the actual officer completion is intercepted at
Armada's A2 `Starbase::FinishBuild` RVA `0x000bbd90`, matching the boundary used
by A1's removed special case. This prevents A2's outer Starbase routine from
sending a null finished object to `OutputQueueManager`; all ordinary builds
chain through the native gateway. Starbase policy/menu hooks are installed
independently, so an unavailable completion bridge cannot remove ordinary
construction. Starbase SetTeam/ClearTeam hooks add and remove
base/quarter capacity on creation, capture, removal, and destruction.
Versioned sidecar state stores completed count and cumulative gain before the
native Starbase save payload.

API revision 9 also dispatches `STARTING_EFFECT` from HybridBuild's shared
Armada Producer effect hook. A1Compat claims it for matching officer upgrades
because `OfficerUpgradeClass` is not a CraftClass and cannot safely back the
cosmetic construction renderer. Only the visual effect is skipped; build time,
costs, queue state, and in-place completion are preserved.

A1's officer counter represented available/max officers, whereas A2/FO uses
enlisted/max. This stage changes only FO's native maximum; adding the A1 gain to
FO's enlisted count would falsely consume the newly granted capacity. The
separate A1 resource/UI compatibility task still needs to restore the original
available/max presentation and starting limits.

Failure to install any defensive/diagnostic hook is logged but does not
discard the required `wingman` alias or `Addon` overlay registration. Those
policies remain useful independently and are the module's essential contract.

Future A1 compatibility belongs here when it cannot be handled safely by
unchanged data or an offline converter. Shared facilities such as recursive ODF
lookup and optional `Textures/RGB`, `Textures/Index8`, and
`Textures/Compressed` loading remain in their general modules.

Install `A1Compat.dll` centrally, but select it only through the `STA1 Classic`
parent's `[modules]` policy. Ordinary FO4 and STA2 mods remain unaffected when
the marker is absent.

Do not keep backup `.dll` files in `Data/modules`. They become separate
installed module candidates and legacy load-all mods may initialize them.
Store backups outside the game tree or give them a non-DLL extension.
