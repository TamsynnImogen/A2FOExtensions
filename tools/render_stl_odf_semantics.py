#!/usr/bin/env python3
"""Validate and render curated STL ODF interpretations; never infer meanings."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re


def anchor(family):
    return re.sub(r'[^a-z0-9]+', '-', family.casefold().replace('<n>', 'index')).strip('-')


def table_text(text):
    return str(text).replace('|', '\\|').replace('\n', ' / ').replace('`', "'")


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--audit', type=Path, required=True)
    args = cli.parse_args()
    root = args.audit
    read = lambda p: json.loads((root / p).read_text())
    scope = read('scope.json')
    assert hashlib.sha256(Path(scope['curated_scope_path']).read_bytes()).hexdigest() == scope['curated_scope_sha256'], 'User-edited scope changed; refresh deliberately.'
    corpus_root = Path(read('input-audit/manifest.json')['corpora']['STL']['root'])
    entries, partitions = [], {}
    citation_count = 0
    for part in ('gameplay', 'effects', 'other'):
        evidence = {p['family']: p for p in read(f'evidence-{part}.json')}
        interpretations = read(f'interpretations-{part}.json')['entries']
        assert len(interpretations) == len(evidence), part
        assert {e['family'] for e in interpretations} == set(evidence), part
        for entry in interpretations:
            assert entry['basis'] in ('documented', 'inferred', 'unresolved'), entry['family']
            assert entry['confidence'] in ('high', 'medium', 'low'), entry['family']
            assert entry['role'] in ('gameplay', 'presentation', 'asset', 'ui', 'schema', 'ambiguous'), entry['family']
            assert all(entry.get(k) for k in ('meaning', 'value_form', 'units', 'caveats', 'follow_up', 'sources')), entry['family']
            packet = evidence[entry['family']]
            for cite in entry['sources']:
                lines = (corpus_root / cite['source']).read_bytes().decode('cp1252', errors='replace').splitlines()
                line = lines[cite['line'] - 1]
                # Source pointers must identify actual assignment lines for this family.
                match = re.match(r'\s*([A-Za-z_][\w.\[\]:-]*)\s*=', line)
                assert match and match[1].casefold() in packet['keys'], (entry['family'], cite, line)
                citation_count += 1
            enriched = dict(entry, keys=packet['keys'], spellings=packet['spellings'],
                            source_domains=packet['domains'], file_count=packet['file_count'],
                            occurrence_count=packet['occurrence_count'], observed_values=packet['observed_values'],
                            partition=part, anchor=anchor(entry['family']))
            entries.append(enriched)
        partitions[part] = [e for e in entries if e['partition'] == part]
    assert len({e['family'] for e in entries}) == len(entries)
    assert {k for e in entries for k in e['keys']} == set(scope['keys'])
    assert {e['family'] for e in entries} == set(scope['families'])
    assert len({e['anchor'] for e in entries}) == len(entries), 'Anchor collision'
    entries.sort(key=lambda e: e['family'])

    def source_link(cite):
        return f"[{cite['source']}:{cite['line']}](<{corpus_root / cite['source']}:{cite['line']}>)"

    titles = {'gameplay': 'Ships, weapons, stations and construction',
              'effects': 'Particles, explosions and chunks', 'other': 'Physics, captain tables, nebulae and configuration'}
    for part, rows in partitions.items():
        text = f'# {titles[part]}\n\n'
        text += 'See [README](README.md) for confidence definitions and [the index](command-index.md) for all families. Documented means authored ODF comments describe the intent; it does not mean tested runtime support.\n\n'
        for entry in sorted(rows, key=lambda e: e['family']):
            text += f'<a id="{entry["anchor"]}"></a>\n\n### `{entry["family"]}`\n\n'
            text += '**Spellings:** ' + ', '.join(f'`{s}`' for s in entry['spellings']) + '.\n\n'
            text += f"**{entry['basis'].capitalize()} / {entry['confidence']} confidence**; role: {entry['role']}.\n\n{entry['meaning']}\n\n"
            text += f"Value form: {entry['value_form']}. Units: {entry['units']}. Observed in {entry['file_count']} files ({entry['occurrence_count']} assignments).\n\n"
            examples = entry['observed_values'][:3]
            examples = [v if len(v) <= 240 else v[:240] + '…' for v in examples]
            text += 'Examples:\n\n```text\n' + '\n'.join(examples) + '\n```\n\n'
            text += f"Uncertainty: {entry['caveats']}\n\nNext investigation: {entry['follow_up']}\n\n"
            text += 'Evidence: ' + '; '.join(source_link(c) + ' — ' + c['reason'] for c in entry['sources']) + '.\n\n'
        (root / f'reference-{part}.md').write_text(text)
    index = '# STL command meaning index\n\nOne row per lexical family; every retained exact key maps to a detailed entry. Families can span different classes or contexts: consult the caveat before assuming one universal meaning.\n\n| Command spellings | Proposed meaning | Evidence basis | Confidence | Detail |\n| --- | --- | --- | --- | --- |\n'
    for entry in entries:
        spellings = ', '.join(entry['spellings'])
        index += '| ' + ' | '.join([f'`{table_text(spellings)}`', table_text(entry['meaning']), entry['basis'], entry['confidence'],
                    f"[sources and limits](reference-{entry['partition']}.md#{entry['anchor']})"]) + ' |\n'
    (root / 'command-index.md').write_text(index)
    unknowns = '# Commands needing further evidence\n\nThese entries are unresolved or have low confidence. All other entries also retain specific open questions in the full reference.\n\n'
    for entry in entries:
        if entry['basis'] == 'unresolved' or entry['confidence'] == 'low':
            unknowns += f"- [`{entry['family']}`](reference-{entry['partition']}.md#{entry['anchor']}): {entry['meaning']} {entry['follow_up']}\n"
    (root / 'unresolved.md').write_text(unknowns)
    totals = {'keys': len(scope['keys']), 'families': len(entries),
              'basis_counts': dict(Counter(e['basis'] for e in entries)),
              'confidence_counts': dict(Counter(e['confidence'] for e in entries)),
              'role_counts': dict(Counter(e['role'] for e in entries)),
              'source_citations_checked': citation_count,
              'retained_scope_keys_no_longer_candidates': len(scope['retained_but_no_current_candidate'])}
    (root / 'semantics.json').write_text(json.dumps({'methodology': 'Curated evidence-based intended meanings; no runtime support claim.', 'totals': totals, 'entries': entries}, indent=2, ensure_ascii=False) + '\n')
    (root / 'semantics-summary.json').write_text(json.dumps(totals, indent=2) + '\n')
    print(json.dumps(totals, indent=2))


if __name__ == '__main__':
    main()
