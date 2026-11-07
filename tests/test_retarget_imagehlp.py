import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
import retarget_imagehlp as target


class RetargetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        base=ROOT/'build/rust-migration-stage17/acceptance/test-installation'
        paths=[base/'Data/ArmadaL.exe',base/'Data/Mods/Fleet Ops 4.0/ArmadaL.exe']
        if not all(p.is_file() for p in paths):
            raise unittest.SkipTest('pinned stage17 images unavailable')
        cls.images=[p.read_bytes() for p in paths]
        for data in cls.images:
            assert target.sha(data) in target.KNOWN

    def test_both_actual_profiles_preserve_code_geometry_and_all_thunks(self):
        for data in self.images:
            result,record=target.rewrite(data)
            self.assertEqual(len(data),len(result))
            old=target.normal_imports(data);new=target.normal_imports(result)
            self.assertEqual(len(old),len(new))
            for a,b in zip(old,new):
                self.assertEqual(a['entries'],b['entries'])
                self.assertEqual(a['name_offset'],b['name_offset'])
                self.assertEqual(b['dll'],'DBGHELP.dll' if a['dll'].lower()=='imagehlp.dll' else a['dll'])
            self.assertTrue(record['executable_sections_unchanged'])
            field=target.BytesImage(result).optional+64
            self.assertEqual(struct.unpack_from('<I',result,field)[0],target.checksum(result,field))

    def test_dll_name_and_function_mismatch_fail_closed(self):
        data=self.images[0]
        imported=next(x for x in target.normal_imports(data) if x['dll'].lower()=='imagehlp.dll')
        bad=bytearray(data);bad[imported['name_offset']]=ord('X')
        with self.assertRaises(ValueError):target.rewrite(bytes(bad))
        function=data.index(b'UnDecorateSymbolName\0')
        bad=bytearray(data);bad[function]=ord('X')
        with self.assertRaises(ValueError):target.rewrite(bytes(bad))

    def test_production_entry_rejects_unknown_image(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as tmp:
            directory=Path(tmp);data=bytearray(self.images[0]);data[0x1001]^=1
            source=directory/'source.exe';source.write_bytes(data)
            with self.assertRaisesRegex(ValueError,'unknown engine image'):
                target.patch(source,directory/'patched.exe')
            self.assertFalse((directory/'patched.exe').exists())
            self.assertEqual(source.read_bytes(),data)

    def test_production_entry_records_exact_change_and_never_overwrites(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as tmp:
            directory=Path(tmp);source=directory/'source.exe';output=directory/'patched.exe'
            source.write_bytes(self.images[0]);record=target.patch(source,output)
            self.assertEqual(source.read_bytes(),self.images[0])
            self.assertEqual(record['after_sha256'],hashlib.sha256(output.read_bytes()).hexdigest())
            self.assertEqual(json.loads(output.with_name('patched.exe.receipt.json').read_text())['before_sha256'],target.sha(self.images[0]))
            self.assertTrue(Path(record['profile_proposal']).is_file())
            with self.assertRaisesRegex(ValueError,'fresh file'):target.patch(source,output)

    def test_symlinks_and_outside_build_are_rejected(self):
        with tempfile.TemporaryDirectory(dir=ROOT/'build') as tmp:
            directory=Path(tmp);source=directory/'source.exe';source.write_bytes(self.images[0])
            alias=directory/'alias.exe';alias.symlink_to(source)
            with self.assertRaisesRegex(ValueError,'symlink'):target.patch(alias,directory/'out.exe')
            outside=ROOT/'build/../forbidden-output.exe'
            with self.assertRaisesRegex(ValueError,'repository build'):target.patch(source,outside)
            self.assertFalse((ROOT/'forbidden-output.exe').exists())


if __name__=='__main__':unittest.main()
