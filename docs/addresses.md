# Supported binaries and checked addresses

All values below are relative virtual addresses (RVAs). Runtime code adds the
loaded module base and verifies the PE identity and/or exact instruction bytes
before patching. These are compatibility records, not wildcard signatures for
other game versions.

## Binary identities

| Image | PE timestamp | `SizeOfImage` |
| --- | ---: | ---: |
| `ArmadaL.exe` (Armada II 1.1) | `0x3c4c76bd` | `0x00403999` |
| `FleetOpsHook.dll` (supported Fleet Ops build) | `0x51f6475c` | `0x00322000` |

## Core-owned sites

| Image | RVA | Use |
| --- | ---: | --- |
| Armada | `0x0a85e0` | `EvolverClass::BuildClass` hook |
| Armada | `0x0a85d0` | Evolver class destruction/lifetime hook |
| Armada | `0x0b0534` | cocoon selector jump |
| Armada | `0x135350` | `ParameterDB::GetString` dispatcher input |
| Armada | `0x0c6ab0` | `Craft::Explode` destruction dispatcher |
| Armada | `0x0cd370` | find `GameObjectClass` by ODF name |
| Armada | `0x0cd390` | construct replacement object |
| Armada | `0x0ce370` | source class ODF name |
| Armada | `0x0cfd50` | source transform |
| Armada | `0x13c8a0` | `GetDefaultUserProfileGameSpeed` supplier |
| Armada | `0x00001370` | current `AiMission` |
| Armada | `0x22cf10` | SOD load helper |
| Armada | `0x3ad508` | SOD database pointer |
| Fleet Ops | `0x105fec` | FOFS native hash-lookup CALL dispatcher |
| Fleet Ops | `0x109c14` | original file hash lookup |
| Fleet Ops | `0x10ab98` | active mod user-directory hook for `SettingsDirectory` |
| Fleet Ops | `0x13e744`, `0x13e824` | settings singleton load/save for first-run `DefaultGameSpeed` |
| Fleet Ops | `0x13e93c` | new game-configuration defaults for `DefaultGameSpeed` |
| Fleet Ops | `0x13ea8c` | operational profile-load boundary for first-run `DefaultGameSpeed` |
| Fleet Ops | `0x00570c` | Delphi `System.@LStrAsg` lifetime-safe path assignment |

## Recursive ODF feature pack

| Fleet Ops RVA | Use |
| ---: | --- |
| `0x0056b8`, `0x0058b0` | Delphi string lifetime helpers |
| `0x080824` | list insertion |
| `0x0fa83c` | Fleet Ops filename hash |
| `0x10870c`, `0x108b6c` | virtual-directory class/constructor |
| `0x108c14` | override recalculation |
| `0x1092d0` | add file to hash table |
| `0x109488`, `0x109650` | scan disk/FPQ items |

## Queue feature pack

| Image | RVA | Use |
| --- | ---: | --- |
| Armada | `0x0d4280`, `0x0d45f0` | typed-class order send/receive paths |
| Armada | `0x0b77d0`, `0x0b7840` | Producer destruction/simulation |
| Armada | `0x0b88d0`, `0x0b8aa0` | Producer load/save |
| Armada | `0x0cd150` | class lookup by project ID |
| Armada | `0x36133c`, `0x361344` | Control/Alt command-state pointers |
| Fleet Ops | `0x12255c`, `0x122514` | Producer finish/cancel callbacks |
| Fleet Ops | `0x1229b8`, `0x122a10` | checked push/build-command push |
| Fleet Ops | `0x122c8c`, `0x122ef4` | delete/clear queue callbacks |

## Upgrade-pod feature pack

| Image | RVA | Use |
| --- | ---: | --- |
| Armada | `0x0b95f0`, `0x0b95a0` | ResearchPod attach/detach tracking |
| Armada | `0x0b99b0`, `0x0b9b50` | per-station tier-list lifetime |
| Armada | `0x096340` | Team upgrade-manager lookup |
| Armada | `0x0987d0` | bounded tier-3 multiplier projection |
| Armada | `0x0cd370` | tier build-item class lookup |
| Armada | `0x135350` | tier build-item ParameterDB reads through the core dispatcher |
| Fleet Ops | `0x10c5e4`, `0x10c618` | ResearchPodClass load/lifetime sidecar |
| Fleet Ops | `0x11c988` | ProducerClass 57-slot build-list allocation at class `+0x450` |
| Fleet Ops | `0x1e3e00` | ResearchStationClass secondary-list and `providedBuildItem` parsing |
| Fleet Ops | `0x1e3ea0` | ResearchStationClass build permission for sidecar higher-tier pod classes |
| Fleet Ops | `0x1fcffc` | extended ResearchPod same-type comparison |

Address provenance is the Armada II 1.1 symbol map/PDB and the Fleet Ops public
map for the identified binaries, followed by instruction-level disassembly and
runtime-byte preflight. Fleet Ops' map code offsets require the `.text` section
offset when translating to image RVAs; the table records the final RVAs used by
the implementation.
