#!/usr/bin/env python3
"""Replay one A2FORecoveryTests capture with the recovered comparison library."""
import argparse
import ctypes as C
import json
import sys
import tempfile
from pathlib import Path


class Snapshot(C.Structure):
    _fields_ = [('routine', C.c_uint32), ('object', C.c_uint32),
                ('control_word', C.c_uint16), ('fp_tag', C.c_uint8),
                ('reserved', C.c_uint8), ('node_before', C.c_uint8 * 136),
                ('node_after', C.c_uint8 * 136),
                ('context_addresses', C.c_uint32 * 3),
                ('context_before', C.c_uint32 * 3),
                ('context_after', C.c_uint32 * 3),
                ('actual_return', C.c_uint32)]


assert C.sizeof(Snapshot) == 324
FIELDS = {'version', 'routine', 'object', 'control_word', 'fp_tag', 'reserved',
          'node_before', 'node_after', 'context_addresses', 'context_before',
          'context_after', 'actual_return', 'expected_return', 'outcome',
          'reason', 'mismatch_offset', 'expected_word', 'actual_word'}
PROVENANCE = {'input_sha256', 'module_build'}
PINNED_SHA256 = '63bd645d82aec75cdd7ee3e3271fb67bb54f4ae1d1636a736b0db620bd997e36'
MODULE_BUILD = '20260930-recovery-tests-1'
OUTCOMES = {'match': 0, 'mismatch': 1, 'skip': 2}
ROUTINES = {'engine_context': 0, 'node_transform': 1}


def uint(value, bits, label):
    if type(value) is not int or not 0 <= value < 1 << bits:
        raise ValueError(f'{label} must be an unsigned {bits}-bit integer')
    return value


def words(value, label):
    if not isinstance(value, list) or len(value) != 3:
        raise ValueError(f'{label} must contain exactly three words')
    return [uint(item, 32, f'{label}[{index}]') for index, item in enumerate(value)]


def image(value, label):
    if not isinstance(value, str) or len(value) != 272:
        raise ValueError(f'{label} must contain exactly 136 hex bytes')
    try:
        data = bytes.fromhex(value)
    except ValueError as error:
        raise ValueError(f'{label} is not hexadecimal') from error
    if len(data) != 136 or data.hex() != value:
        raise ValueError(f'{label} must use lowercase contiguous hex')
    return data


def no_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f'duplicate key: {key}')
        result[key] = value
    return result


def load_record(path):
    record = json.loads(path.read_text(encoding='utf-8'),
                        object_pairs_hook=no_duplicate_keys,
                        parse_constant=lambda word: (_ for _ in ()).throw(ValueError(word)))
    if not isinstance(record, dict) or set(record) != (FIELDS | PROVENANCE):
        raise ValueError('capture keys differ from version 1 schema')
    if record['input_sha256'] != PINNED_SHA256 or record['module_build'] != MODULE_BUILD:
        raise ValueError('capture provenance differs from pinned module build')
    if uint(record['version'], 32, 'version') != 1:
        raise ValueError('unsupported capture version')
    if (not isinstance(record['routine'], str) or
            not isinstance(record['outcome'], str) or
            record['routine'] not in ROUTINES or record['outcome'] not in OUTCOMES):
        raise ValueError('unknown routine or outcome')
    if not isinstance(record['reason'], str) or not record['reason']:
        raise ValueError('reason must be nonempty text')
    snapshot = Snapshot()
    snapshot.routine = ROUTINES[record['routine']]
    for field, bits in (('object', 32), ('control_word', 16), ('fp_tag', 8),
                        ('reserved', 8), ('actual_return', 32),
                        ('expected_return', 32), ('mismatch_offset', 32),
                        ('expected_word', 32), ('actual_word', 32)):
        uint(record[field], bits, field)
        if field in ('object', 'control_word', 'fp_tag', 'reserved', 'actual_return'):
            setattr(snapshot, field, record[field])
    snapshot.node_before[:] = image(record['node_before'], 'node_before')
    snapshot.node_after[:] = image(record['node_after'], 'node_after')
    for field in ('context_addresses', 'context_before', 'context_after'):
        getattr(snapshot, field)[:] = words(record[field], field)
    return record, snapshot


def replay(path, library):
    record, snapshot = load_record(path)
    output = C.create_string_buffer(8192)
    code = library.recovery_compare(C.byref(snapshot), output, len(output))
    if code < 0:
        raise RuntimeError('comparison bridge rejected the capture')
    current = json.loads(output.value)
    if code != OUTCOMES[record['outcome']]:
        raise ValueError('replayed outcome changed')
    for field in ('outcome', 'expected_return', 'reason', 'mismatch_offset',
                  'expected_word', 'actual_word'):
        if current[field] != record[field]:
            raise ValueError(f'replayed {field} changed')
    return {'status': 'PASS', 'capture': str(path), 'outcome': current['outcome'],
            'expected_return': current['expected_return']}


def self_test(library):
    def record(routine, object_, before, after, addresses, values, actual,
               expected, outcome, reason, offset=0xffffffff, wanted=0, seen=0):
        return {'version': 1, 'input_sha256': PINNED_SHA256,
                'module_build': MODULE_BUILD, 'routine': routine, 'object': object_,
                'control_word': 0x37f, 'fp_tag': 0, 'reserved': 0,
                'node_before': before.hex(), 'node_after': after.hex(),
                'context_addresses': addresses, 'context_before': values,
                'context_after': values, 'actual_return': actual,
                'expected_return': expected, 'outcome': outcome, 'reason': reason,
                'mismatch_offset': offset, 'expected_word': wanted,
                'actual_word': seen}
    blank = bytes(136)
    engine, selected, value = 0x20001000, 0x20003000, 0x12345678
    addresses = [engine + 0xc0, engine + 0xd0, selected + 0x28]
    context = record('engine_context', engine, blank, blank, addresses,
                     [1, selected, value], value, value, 'match', 'matched')
    negative = dict(context, actual_return=value ^ 1, outcome='mismatch',
                    reason='return_mismatch', expected_word=value, actual_word=value ^ 1)
    node = bytearray(blank)
    node[0x54:0x58] = (0x3f800000).to_bytes(4, 'little')
    node_object = 0x20004000
    unit = record('node_transform', node_object, node, node, [0, 0, 0],
                  [0, 0, 0], node_object + 0x24, node_object + 0x24,
                  'match', 'matched')
    with tempfile.TemporaryDirectory(prefix='a2fo-recovery-replay-') as folder:
        path = Path(folder) / 'capture.json'
        for fixture in (context, negative, unit):
            path.write_text(json.dumps(fixture), encoding='utf-8')
            replay(path, library)
        malformed = dict(context, node_before='00')
        path.write_text(json.dumps(malformed), encoding='utf-8')
        try:
            replay(path, library)
        except ValueError:
            pass
        else:
            raise AssertionError('malformed capture was accepted')
        wrong_provenance = dict(context, input_sha256='0' * 64)
        path.write_text(json.dumps(wrong_provenance), encoding='utf-8')
        try:
            replay(path, library)
        except ValueError:
            pass
        else:
            raise AssertionError('wrong provenance was accepted')
        missing_provenance = dict(context)
        del missing_provenance['module_build']
        path.write_text(json.dumps(missing_provenance), encoding='utf-8')
        try:
            replay(path, library)
        except ValueError:
            pass
        else:
            raise AssertionError('missing provenance was accepted')
        malformed_outcome = dict(context, outcome=['match'])
        path.write_text(json.dumps(malformed_outcome), encoding='utf-8')
        try:
            replay(path, library)
        except ValueError:
            pass
        else:
            raise AssertionError('malformed outcome was accepted')
    return {'status': 'PASS', 'self_test_cases': 7, 'reproduced_mismatch': 1,
            'malformed_rejections': 4}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('capture', nargs='?', type=Path)
    parser.add_argument('--library', type=Path, default=Path('build/librecovery_tests_replay.so'))
    parser.add_argument('--self-test', action='store_true')
    args = parser.parse_args()
    if not args.self_test and args.capture is None:
        parser.error('capture path required unless --self-test is selected')
    library = C.CDLL(str(args.library.resolve()))
    library.recovery_compare.argtypes = [C.POINTER(Snapshot), C.c_void_p, C.c_size_t]
    library.recovery_compare.restype = C.c_int
    try:
        result = self_test(library) if args.self_test else replay(args.capture, library)
    except (OSError, RuntimeError, ValueError, AssertionError, json.JSONDecodeError) as error:
        print(json.dumps({'status': 'FAIL', 'reason': str(error)}), file=sys.stderr)
        return 1
    print(json.dumps(result))
    return 0


if __name__ == '__main__':
    sys.exit(main())
