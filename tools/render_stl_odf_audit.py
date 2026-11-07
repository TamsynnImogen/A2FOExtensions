#!/usr/bin/env python3
"""Render a static ODF audit and add textual references to current extension source."""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re


def cell(value, limit=None):
    text = str(value).replace('\n', ' / ').replace('|', '\\|').replace('`', "'")
    return text if limit is None or len(text) <= limit else text[:limit] + '…'


def link(root, occurrence):
    path = Path(root) / occurrence['source']
    label = f"{occurrence['source']}:{occurrence['line']}"
    return f'[{label}](<{path}:{occurrence["line"]}>)'


def main():
    cli = argparse.ArgumentParser(description=__doc__)
    cli.add_argument('--audit', type=Path, required=True)
    cli.add_argument('--project', type=Path, default=Path(__file__).resolve().parents[1])
    args = cli.parse_args()
    read = lambda name: json.loads((args.audit / name).read_text())
    manifest, matrix = read('manifest.json'), read('command-matrix.json')['rows']
    candidates = read('candidates.json')['occurrences']
    candidate_keys = {o['key'].casefold() for o in candidates}
    by_key = defaultdict(list)
    for o in candidates:
        by_key[o['key'].casefold()].append(o)
    references = defaultdict(list)
    source_files = {}
    # Literal strings only: a source match is a search lead, not proof of a consumer.
    quoted = re.compile(r'"([^"\r\n]+)"')
    for tree in ('modules', 'core', 'sdk'):
        for path in sorted((args.project / tree).rglob('*')):
            if not path.is_file() or path.suffix not in ('.cpp', '.hpp', '.h', '.inl', '.c', '.S'):
                continue
            data = path.read_bytes()
            found = False
            for number, line in enumerate(data.decode('utf-8', errors='replace').splitlines(), 1):
                for key in set(m.casefold() for m in quoted.findall(line)) & candidate_keys:
                    references[key].append({'path': str(path.relative_to(args.project)), 'line': number,
                                            'text': line.strip()})
                    found = True
            if found:
                source_files[str(path.relative_to(args.project))] = hashlib.sha256(data).hexdigest()
    # Indexed fields are often assembled at runtime; these are explicit research pointers.
    patterns = [(r'^animation\d+', 'A2FOAnimations'), (r'^swarm\d+', 'A2FOSwarmSystem'),
                (r'^squadmember\d+|^squadreinforceatyard$', 'A2FOSquadrons'),
                (r'^breeder', 'A2FOBreeders')]
    index = []
    domains = defaultdict(list)
    all_rows = []
    corpora = list(manifest['corpora'])
    headings = {'STL': 'STL', 'STA1 Classic Mod': 'STA1', 'STA2 Classic Mod': 'STA2',
                'Fleet Ops 4.0': 'FO4', 'FO_ROOT': 'Data root'}
    for row in matrix:
        key = row['key']
        stl = row['corpora']['STL']
        examples = stl['examples']
        counts = [str(row['corpora'][name]['file_count']) for name in corpora]
        shown = ', '.join(stl['spellings'])
        all_rows.append('| ' + ' | '.join([f'`{cell(shown)}`', f'`{cell(row["family"])}`'] + counts +
                       [cell(examples[0]['value'], 100), link(manifest['corpora']['STL']['root'], examples[0])]) + ' |')
        if key not in candidate_keys:
            continue
        occurrences = by_key[key]
        folders = sorted({Path(o['source']).parts[1] if Path(o['source']).parts[0] == 'odf'
                          else Path(o['source']).parts[0] for o in occurrences})
        indexed_modules = [module for pattern, module in patterns if re.search(pattern, key)]
        local_modules = sorted(set(indexed_modules + [Path(r['path']).parts[1] for r in references[key]
                                                     if r['path'].startswith('modules/')]))
        fixtures = sorted({o['source'] for o in occurrences if Path(o['source']).stem.startswith('nox_')
                           or Path(o['source']).name == 'fsquad.odf'})
        entry = {'key': key, 'spellings': stl['spellings'], 'family': row['family'],
                 'stl_file_count': stl['file_count'], 'stl_assignment_count': stl['assignment_count'],
                 'source_folders': folders, 'shared_data_file_count': row['corpora']['FO_ROOT']['file_count'],
                 'shared_data_family_file_count': row['family_comparison']['FO_ROOT']['file_count'],
                 'extension_literal_references': references[key], 'indexed_extension_module_pointers': indexed_modules,
                 'local_fixture_sources': fixtures, 'occurrences': occurrences}
        index.append(entry)
        notes = []
        if row['family_comparison']['FO_ROOT']['file_count']:
            notes.append('family occurs in shared Data')
        if local_modules:
            notes.append('extension search: ' + ', '.join(local_modules))
        if fixtures:
            notes.append('local fixture')
        domains[folders[0]].append('| ' + ' | '.join([f'`{cell(shown)}`', f'`{cell(row["family"])}`',
             str(stl['assignment_count']), str(stl['file_count']), str(row['corpora']['FO_ROOT']['file_count']),
             cell(examples[0]['value'], 120), link(manifest['corpora']['STL']['root'], examples[0]),
             cell('; '.join(notes))]) + ' |')
    prefix = '# STL ODF command comparison\n\nGenerated from the pinned snapshot in [manifest.json](manifest.json). Counts are files containing active direct assignments; they are not engine support indicators. Case-folded keys and numbered families are lexical comparison aids. All values, source lines and comments for candidates are in [candidates.json](candidates.json); direct includes are recorded per file in [manifest.json](manifest.json).\n\n'
    header = '| Command spellings | Family | ' + ' | '.join(headings[n] for n in corpora) + ' | Example value | STL source |\n'
    header += '| ' + ' | '.join(['---'] * (len(corpora) + 4)) + ' |\n'
    (args.audit / 'command-matrix.md').write_text(prefix + header + '\n'.join(all_rows) + '\n')
    text = '# Commands absent from the three mod baselines\n\nThese commands belong to lexical families with no active assignment in the supplied STA1, STA2 or FO4 mod trees. Presence in the shared Data tree is reported separately. This is a candidate list, not a list of unsupported commands or proof of stock Legacy provenance. Local test definitions remain visible. Extension source matches are research pointers, not runtime acceptance.\n\nSee [research-index.json](research-index.json) for **every** occurrence, full values and comments, literal extension source references, indexed extension pointers and local fixture tags. Commands occurring in multiple source folders are grouped here by their first alphabetical folder.\n\n'
    for domain in sorted(domains):
        text += f'## {domain}\n\n| Command spellings | Family | Assignments | STL files | Data root files | Example value | STL source | Research notes |\n'
        text += '| --- | --- | --- | --- | --- | --- | --- | --- |\n' + '\n'.join(domains[domain]) + '\n\n'
    (args.audit / 'candidates.md').write_text(text)
    result = {'method': 'Literal quoted-string references in current modules/core/sdk source only. Indexed module pointers are manually specified lexical patterns. References do not prove runtime behavior.',
              'source_file_sha256': source_files, 'commands': index}
    (args.audit / 'research-index.json').write_text(json.dumps(result, indent=2, ensure_ascii=False) + '\n')
    print(json.dumps({'matrix_rows': len(all_rows), 'candidate_keys': len(index),
                      'candidate_families': len({r['family'] for r in index}),
                      'candidate_keys_with_shared_data_family': sum(bool(r['shared_data_family_file_count']) for r in index),
                      'candidate_keys_with_literal_extension_refs': sum(bool(r['extension_literal_references']) for r in index),
                      'candidate_keys_with_indexed_extension_pointers': sum(bool(r['indexed_extension_module_pointers']) for r in index),
                      'candidate_keys_in_local_fixtures': sum(bool(r['local_fixture_sources']) for r in index)}, indent=2))


if __name__ == '__main__':
    main()
