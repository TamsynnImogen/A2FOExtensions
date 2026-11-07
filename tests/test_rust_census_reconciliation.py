"""Adversarial tests for census credit and complete native-suite accounting."""

import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from verify_rust_census_reconciliation import verify_membership, verify_parity
from run_rust_stage7_parity import GROUPS


def inventories():
    units = []
    for index in range(151):
        units.append({
            'path': f'unit-{index}.cpp',
            'status': 'verified_prior' if index < 116 else 'not_migrated',
            'baseline_source_sha256': f'{index:064x}',
            'baseline_source_lines': index + 1,
            'legacy_census_paths': [f'legacy-{index}.cpp'],
            'expanded_from_aggregation_wrapper': False,
        })
    historical = {'units': copy.deepcopy(units)}
    baseline = copy.deepcopy(historical)
    for unit in units[116:]:
        unit['status'] = 'completed_pending_census'
        unit['component_completion'] = {'strict_census_credit': False}
    previous = {'units': copy.deepcopy(units)}
    for unit in units[116:]:
        unit.update(status='verified_stage17', stage='stage17',
                    rust_source='source.rs', rust_sources=['source.rs'],
                    rust_symbols=['a2fo_rs_example'], verified_cpp_sha256='current',
                    census_verification={'path': 'proof.json', 'sha256': 'pin'})
    current = {'units': units, 'totals': {
        'implementation_units': 151, 'verified_migrated_units': 151,
        'pending_units': 0, 'verified_percent': 100}}
    return current, previous, historical, baseline


class CreditTests(unittest.TestCase):
    def test_exact_credit_partition(self):
        units, pending = verify_membership(*inventories())
        self.assertEqual((len(units), len(pending)), (151, 35))

    def reject(self, mutate):
        current, previous, historical, baseline = inventories()
        mutate(current)
        with self.assertRaises(ValueError):
            verify_membership(current, previous, historical, baseline)

    def test_duplicate_unit_cannot_replace_membership(self):
        self.reject(lambda data: data['units'].__setitem__(150, data['units'][149]))

    def test_retirement_cannot_reduce_denominator(self):
        self.reject(lambda data: data['units'].pop())

    def test_existing_credit_cannot_be_rewritten(self):
        self.reject(lambda data: data['units'][0].update(rust_symbols=['forged']))

    def test_historical_false_gate_remains_historical(self):
        self.reject(lambda data: data['units'][-1]['component_completion'].update(
            strict_census_credit=True))

    def test_baseline_provenance_cannot_be_rebased(self):
        self.reject(lambda data: data['units'][-1].update(baseline_source_sha256='new'))

    def test_totals_cannot_exceed_statuses(self):
        self.reject(lambda data: data['units'][-1].update(status='completed_pending_census'))


class ParityTests(unittest.TestCase):
    def receipt(self):
        names = [row[0] for row in GROUPS]
        return {'status': 'passed', 'selected_groups': names, 'results': [
            {'group': name, 'status': 'passed', 'exit_code': 0,
             'changed_inputs': [], 'child_receipt_missing': False}
            for name in names]}

    def test_complete_suite(self):
        verify_parity(self.receipt())

    def test_duplicate_pass_cannot_hide_missing_group(self):
        receipt = self.receipt()
        receipt['results'][-1] = receipt['results'][0]
        with self.assertRaises(ValueError):
            verify_parity(receipt)

    def test_failure_or_input_drift_cannot_inherit_pass(self):
        for field, value in [('exit_code', 1), ('changed_inputs', ['source.cpp']),
                             ('child_receipt_missing', True)]:
            with self.subTest(field=field):
                receipt = self.receipt()
                receipt['results'][0][field] = value
                with self.assertRaises(ValueError):
                    verify_parity(receipt)


if __name__ == '__main__':
    unittest.main()
