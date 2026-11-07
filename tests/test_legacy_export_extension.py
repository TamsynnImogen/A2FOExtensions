from pathlib import Path
import sys
import unittest
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from rust_dll_linker import validate_contract

class ExportExtensionTests(unittest.TestCase):
    def setUp(self):self.current=(ROOT/'core/A2FOExtensions.def').read_bytes()
    def test_original_and_exact_append_only_extension_are_accepted(self):
        self.assertEqual(len(validate_contract(self.current)),198)
        old=b''.join(l for l in self.current.splitlines(keepends=True) if b'A2FO_InitializeNoUnwind' not in l)
        self.assertEqual(len(validate_contract(old)),197)
    def test_existing_ordinal_or_name_changes_are_rejected(self):
        for source in [self.current.replace(b'A2FO_Initialize @166',b'A2FO_Initialize @165'),
                       self.current.replace(b'A2FO_Initialize @166',b'A2FO_InitializeOther @166')]:
            with self.assertRaises(ValueError):validate_contract(source)
    def test_unapproved_extensions_and_wrong_ordinal_are_rejected(self):
        for source in [self.current.replace(b'NoUnwind @198',b'NoUnwind @199'),
                       self.current+b'    A2FO_Unapproved @199\n']:
            with self.assertRaises(ValueError):validate_contract(source)

if __name__=='__main__':unittest.main()
