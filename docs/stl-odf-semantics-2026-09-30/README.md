# Star Trek: Legacy ODF command meanings — 2026-09-30

An initial interpretation of every retained command in the cleaned STL corpus:
**538 exact keys, grouped into 413 lexical families**. Each entry gives a
proposed meaning, value form, observed examples, source file and line, confidence,
remaining uncertainty, and a specific next investigation.

Start with the [alphabetical command index](command-index.md). Detailed entries
are split into:

- [Ships, weapons, stations and construction](reference-gameplay.md).
- [Particles, explosions and chunks](reference-effects.md).
- [Physics, captain tables, nebulae and configuration](reference-other.md).
- [Unresolved and low-confidence entries](unresolved.md).

## Evidence and confidence

| Evidence basis | Families | What the label means |
| --- | ---: | --- |
| Documented | 213 | Authored ODF comments explain the intended operation or data format. |
| Inferred | 191 | Assignment names, values, neighbouring fields and class context suggest an operation. |
| Unresolved | 9 | Available evidence does not settle a useful interpretation of the operation. |

Confidence is assessed separately: 183 families are high confidence, 205 medium,
and 25 low. An explicit comment can describe an obsolete field or leave its
formula unclear. A descriptive name can suggest a purpose without settling
units or implementation. Neither label establishes actual runtime behaviour.

The reference derives meanings from the supplied ODFs and their comments. It
does not identify executable consumers, resolve inherited defaults, prove
accepted ranges, or establish Fleet Operations support. Source citations point
to actual assignments; neighbouring comments supply the explanation where
available. Indexed families such as `upgrade<N>` group multiple exact keys;
fields with the same lexical family can have different meanings in different
classes, as the `size<N>` entry explains.

## Useful findings

- `upgrade<N>` has an explicitly documented string grammar:
  `key UI-label operation cost allowMultiple`, where operation is `add`, `mult`,
  `toggle`, or `special`. The `add` and `mult` forms append `systemName value`.
  The currency, exact arithmetic and remaining operation operands still need
  consumer checks. See [upgrade entries](reference-gameplay.md#upgradeindex).
- Race/era planet build lists pair an ODF name with the **required planet
  upgrade level, starting at zero**. The trailing number is not described as a
  Boolean flag. See, for example,
  [Federation ENT build list](reference-gameplay.md#federationentbuildlist).
- `minShipSpeed` is described as full-throttle speed at the **low Engine stat
  endpoint**. It does not describe a lower limit on movement speed. Other
  minimum/maximum motion fields use the same endpoint scheme; the exact stat
  interpolation remains open. See [minShipSpeed](reference-other.md#minshipspeed).
- Some physics fields are explicitly labelled obsolete, read but unused, or
  ineffective. Their names alone would overstate what they do. Several particle
  and chunk fields also have conflicting comments or values that prevent a
  conventional minimum/maximum interpretation.
- Captain tables contain bonus/experience/extra-information tuples, with
  exceptional top-level abilities and comments that conflict with some values.
  Shield proportions describe relative shield size but do not settle whether
  that means capacity, angular coverage, or another weighting.

## Scope after cleanup

The user's edited [candidate table](../stl-odf-audit-2026-09-30/candidates.md)
defines the investigation scope. Its 557 retained keys were intersected with
the freshly scanned ODFs: 538 remain current candidates. Nineteen retained rows
have no current candidate occurrence after source cleanup; they are recorded
in [scope.json](scope.json), rather than interpreted from deleted files.

Three fresh candidates were absent from the edited table and remain excluded:
`possibleCaptainNames`, `rakeSpeed`, and `shieldHitSoundGroup`. The complete
[fresh input audit](input-audit/summary.json) contains 541 candidate keys in
416 families. This reference covers the retained subset only.

The original audit and edited candidate table are preserved. During this pass,
the scanner's baseline summary counting was corrected: looking up an absent
STL key previously added an empty entry to a baseline index and inflated its
distinct-key/family totals. Candidate selection was unaffected. The fresh
baseline totals are STA1 631/572, STA2 1,180/1,002, FO4 2,072/1,468, and shared
FO_ROOT 1,466/1,026 (exact keys/families).

## Source identity and binary research pointers

All 1,685 current STL ODFs match the locally installed Legacy ODFs byte for
byte, with no missing, different, or extra ODFs in the comparison. This records
local correspondence; it does not authenticate an unmodified retail corpus.
The per-file hashes and comparison are in
[source-inventory.json](source-inventory.json) and the
[input manifest](input-audit/manifest.json).

A whole-identifier search of printable ASCII strings in the hash-pinned local
`Legacy.exe` found 293 of the 538 scoped keys, including 287 as standalone
literals. The same search in the selected `ArmadaL.exe` found four keys,
including two standalone literals. Raw file offsets and containing strings are
recorded in the source inventory. Strings are research pointers: their presence
does not prove a consumer, and absence does not prove lack of support. The
selected Data directory had no `FleetOpsHook.dll`; its support was not checked
by this string inventory.

An external research lead is the creator-authored
[Dissection of an ODF starship file](https://www.gamefront.com/games/star-trek-legacy/file/dissection-of-an-odf-starship-file-overview-and-troubleshooting-tips)
listing. Its tutorial archive was not obtained (download returned HTTP 403),
so its contents were not used to establish any command meaning here.

## Next investigations

1. Recover weapon-energy, ammunition, targeting and shield consumers. Settle
   rate-versus-period units, damage formulas, pool ownership and boost behaviour
   before translating these fields to FO systems.
2. Recover subsystem hit selection, subsystem stat interpolation, upgrade
   operations and command/production-point accounting. Test the specific
   alternatives recorded in each entry rather than relying on field names.
3. Resolve the nine unresolved families and determine which apparently obsolete
   physics fields still have consumers. Keep renderer/material meanings tied to
   their owning class.
4. Establish FO support independently, then choose a bounded implementation
   from [legacy_TODO.md](../../legacy_TODO.md). Runtime acceptance remains a
   separate stage from this intended-meaning reference.

## Durable data and reproduction

- [semantics.json](semantics.json): complete machine-readable interpretations
  and provenance; [summary](semantics-summary.json): aggregate counts.
- `interpretations-*.json`: curated explanations used to render the reference.
- `evidence-*.json`: command occurrences, comments, nearby context and values.
- [scope.json](scope.json): edited-table hash, exact scope and exclusions.
- [source-inventory.json](source-inventory.json): local corpus identity and
  executable-string research pointers.
- [validation.json](validation.json): coverage, source/hash/link checks and test
  results for this pass.

From the repository root, regenerate and validate the curated reference with:

```sh
python3 tools/render_stl_odf_semantics.py --audit docs/stl-odf-semantics-2026-09-30
python3 -m unittest discover -s tests -p stl_odf_audit_test.py -v
```

The renderer requires exact scope coverage and verifies all 648 source
citations against their assignment lines. It refuses a changed edited-table
hash so scope changes can be reviewed deliberately. Regeneration renders
curated explanations; it does not automatically determine meanings.
