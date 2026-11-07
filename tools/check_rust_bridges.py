#!/usr/bin/env python3
"""Compare i386 COFF bridge instructions, symbols and relocation semantics.

COFF field layouts follow Microsoft's PE/COFF specification. Empty sections and
compiler metadata are excluded; every nonempty code/data section is compared.
No disassembly text or compiler success alone can establish byte parity.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

ROOT = Path(__file__).resolve().parent.parent


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def coff(path):
    data = Path(path).read_bytes()
    def span(at, size):
        if at < 0 or size < 0 or at + size > len(data):
            raise ValueError(f'COFF range outside file: {at}+{size}')
        return data[at:at + size]
    def unpack(fmt, at):
        return struct.unpack(fmt, span(at, struct.calcsize(fmt)))
    machine, count, _, symbol_at, symbol_count, optional, _ = unpack('<HHIIIHH', 0)
    if machine != 0x14c or optional != 0:
        raise ValueError('expected ordinary i386 COFF object, no optional header')
    strings_at = symbol_at + symbol_count * 18
    strings_size, = unpack('<I', strings_at)
    strings = span(strings_at, strings_size)
    def string(at):
        if at < 4 or at >= len(strings):
            raise ValueError('COFF string offset outside table')
        end = strings.find(b'\0', at)
        if end == -1:
            raise ValueError('unterminated COFF string')
        return strings[at:end].decode('ascii')
    def name(raw):
        if raw[:4] == b'\0' * 4:
            return string(struct.unpack('<I', raw[4:])[0])
        return raw.split(b'\0', 1)[0].decode('ascii')
    sections = []
    for i in range(count):
        raw, virtual, _, size, offset, relocation_at, _, relocations, _, flags = unpack('<8sIIIIIIHHI', 20 + i * 40)
        n = raw.split(b'\0', 1)[0].decode('ascii')
        if n.startswith('/'):
            n = string(int(n[1:]))
        body = span(offset, size) if offset else b''
        sections.append({'name': n, 'virtual': virtual, 'size': size,
                         'bytes': body.hex(), 'flags': flags,
                         'relocation_at': relocation_at, 'relocation_count': relocations})
    symbols = {}
    i = 0
    while i < symbol_count:
        raw, value, section, kind, storage, aux = unpack('<8sIhHBB', symbol_at + i * 18)
        symbols[i] = {'name': name(raw), 'value': value, 'section': section,
                      'kind': kind, 'storage': storage}
        i += 1 + aux
    def reference(index):
        if index not in symbols:
            raise ValueError('relocation refers to auxiliary/missing symbol')
        s = symbols[index]
        if s['storage'] == 2 or s['section'] <= 0:
            return ['symbol', s['name']]
        return ['local', sections[s['section'] - 1]['name'], s['value']]
    result = {'sections': {}, 'exports': {}, 'undefined': []}
    for s in symbols.values():
        if s['storage'] == 2:
            if s['section'] > 0:
                result['exports'][s['name']] = [sections[s['section'] - 1]['name'], s['value'], s['kind']]
            elif s['section'] == 0:
                result['undefined'].append(s['name'])
    result['undefined'].sort()
    for s in sections:
        if not (s['size'] or s['virtual']):
            continue
        # Refuse unreviewed compiler/linker sections rather than skipping bytes.
        if s['name'] not in ('.text', '.data', '.rdata', '.bss'):
            raise ValueError(f'unreviewed emitted section: {s["name"]}')
        relocs = []
        for i in range(s['relocation_count']):
            at, index, kind = unpack('<IIH', s['relocation_at'] + i * 10)
            if at + 4 > s['size']:
                raise ValueError('relocation outside section')
            relocs.append([at, kind, reference(index)])
        result['sections'][s['name']] = {k: s[k] for k in ('virtual', 'size', 'bytes', 'flags')}
        result['sections'][s['name']]['relocations'] = relocs
    return result


def differences(before, current):
    return [key for key in ('sections', 'exports', 'undefined') if before[key] != current[key]]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--work-dir', type=Path, required=True)
    parser.add_argument('--before', type=Path, required=True)
    parser.add_argument('--manifest', type=Path, default=ROOT / 'rust/a2fo_bridges/manifest.json')
    args = parser.parse_args()
    work = args.work_dir.resolve()
    work.mkdir(parents=True, exist_ok=False)
    manifest = json.loads(args.manifest.read_text())
    record = {'status': 'running', 'cases': {}, 'negative_controls': []}
    def save():
        (work / 'receipt.json').write_text(json.dumps(record, indent=2) + '\n')
    save()
    try:
        for entry in manifest['bridges']:
            key = entry['legacy']
            old = args.before.resolve() / key
            source = ROOT / entry['rust']
            pins = {str(p): sha(p) for p in [old, source, args.manifest, Path(__file__).resolve(), ROOT / 'tools/build_rust_bridge.py']}
            if sha(old) != entry['legacy_sha256']:
                raise RuntimeError(f'frozen source pin differs: {key}')
            folder = work / Path(key).with_suffix('')
            folder.mkdir(parents=True)
            before, current = folder / 'before.o', folder / 'current.o'
            commands = [
                ['i686-w64-mingw32-g++', '-c', str(old), '-o', str(before)],
                ['python3', str(ROOT / 'tools/build_rust_bridge.py'), '--source', str(source), '--output', str(current)],
            ]
            case = {'inputs': pins, 'commands': commands, 'exits': []}
            record['cases'][key] = case
            for i, command in enumerate(commands):
                with (folder / f'compile-{i}.log').open('wb') as log:
                    result = subprocess.run(command, cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
                case['exits'].append(result.returncode)
                save()
                if result.returncode:
                    raise RuntimeError(f'{key}: compiler {i} exit {result.returncode}')
            old_obj, new_obj = coff(before), coff(current)
            (folder / 'before.json').write_text(json.dumps(old_obj, indent=2) + '\n')
            (folder / 'current.json').write_text(json.dumps(new_obj, indent=2) + '\n')
            case['differences'] = differences(old_obj, new_obj)
            case['before_sha256'], case['current_sha256'] = sha(before), sha(current)
            case['exports'], case['relocations'] = len(new_obj['exports']), sum(len(s['relocations']) for s in new_obj['sections'].values())
            if any(sha(Path(p)) != h for p, h in pins.items()):
                raise RuntimeError(f'compiler input changed: {key}')
            save()
        mismatch = [n for n, c in record['cases'].items() if c['differences']]
        if mismatch:
            raise RuntimeError(f'COFF mismatch: {mismatch}')
        # Mutate actual COFF objects, then run the full parser again. These
        # controls must fail comparison before any bridge parity is credited.
        first = next(iter(record['cases']))
        folder = work / Path(first).with_suffix('')
        original_file = folder / 'current.o'
        original = coff(original_file)
        raw = original_file.read_bytes()
        _, section_count, _, symbol_at, symbol_count, _, _ = struct.unpack_from('<HHIIIHH', raw)
        text_header = next(20 + i * 40 for i in range(section_count)
                           if raw[20 + i * 40:28 + i * 40].rstrip(b'\0') == b'.text')
        text_offset = struct.unpack_from('<I', raw, text_header + 20)[0]
        relocation_at = struct.unpack_from('<I', raw, text_header + 24)[0]
        external = []
        i = 0
        while i < symbol_count:
            at = symbol_at + i * 18
            _, _, section, _, storage, aux = struct.unpack_from('<8sIhHBB', raw, at)
            if storage == 2:
                external.append((i, at, section))
            i += 1 + aux
        for field in ['instruction', 'export', 'relocation']:
            mutant = bytearray(raw)
            if field == 'instruction':
                mutant[text_offset] ^= 1
            elif field == 'export':
                _, at, _ = next(item for item in external if item[2] > 0)
                value, = struct.unpack_from('<I', mutant, at + 8)
                struct.pack_into('<I', mutant, at + 8, value + 1)
            else:
                old_index, = struct.unpack_from('<I', mutant, relocation_at + 4)
                new_index = next(index for index, _, _ in external if index != old_index)
                struct.pack_into('<I', mutant, relocation_at + 4, new_index)
            target = work / ('negative-' + field + '.o')
            target.write_bytes(mutant)
            if not differences(original, coff(target)):
                raise RuntimeError(f'verifier accepted {field} mutation')
            record['negative_controls'].append({'mutation': field, 'rejected': True,
                                               'object': str(target), 'sha256': sha(target)})
        record['status'] = 'passed'
        record['families'] = len(record['cases'])
        record['exports'] = sum(c['exports'] for c in record['cases'].values())
    except Exception as error:
        record.update(status='failed', error=repr(error))
        raise
    finally:
        save()
    print(f'PASS {record["families"]} Rust bridge families, {record["exports"]} emitted exports, exact COFF parity')


if __name__ == '__main__':
    main()
