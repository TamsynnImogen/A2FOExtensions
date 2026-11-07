"""Focused stage-only builder tests; no compiler or game process is used."""

import importlib.util
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).resolve().parents[1] / "tools/build_refit_renderer.py"
SPEC = importlib.util.spec_from_file_location("build_refit_renderer", SCRIPT)
builder = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(builder)


class BuilderTests(unittest.TestCase):
    def test_pe_dll_validation_allows_no_imports_but_rejects_malformed_images(self):
        import_free = (
            "renderer.dll: file format pei-i386\n\n"
            "Characteristics 0x2122\n\texecutable\n\t32 bit words\n\tDLL\n\n"
            "The Export Tables (interpreted .rdata section contents)\n"
        )
        self.assertTrue(builder.valid_pe_dll_tables(import_free))
        self.assertFalse(builder.valid_pe_dll_tables(import_free.replace("pei-i386", "pei-x86-64")))
        self.assertFalse(builder.valid_pe_dll_tables(import_free.replace("\tDLL\n", "")))
        self.assertFalse(builder.valid_pe_dll_tables(import_free.replace("The Export Tables", "No exports")))

    def test_linker_wrapper_supplies_flavor_once(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp) / "path with ' quote"
            root.mkdir()
            linker = root / "rust-lld"
            linker.write_text('#!/bin/sh\nprintf "%s\\n" "$@"\n', encoding="utf-8")
            linker.chmod(0o755)
            wrapper = root / "lld-link"
            builder.write_linker_wrapper(wrapper, linker)
            for arguments in (["-flavor", "link", "input.obj"], ["input.obj"]):
                result = subprocess.run([str(wrapper), *arguments], check=True, text=True, capture_output=True)
                self.assertEqual(result.stdout.splitlines(), ["-flavor", "link", "input.obj"])

    def test_provenance_distinguishes_baseline_drift_and_local_files(self):
        with tempfile.TemporaryDirectory() as temp:
            source = Path(temp)
            (source / "original.rs").write_text("patched", encoding="utf-8")
            (source / "local.rs").write_text("local", encoding="utf-8")
            manifest = source / "UPSTREAM-MANIFEST.json"
            files = {f"missing-{index}.rs": {"sha256": "old"} for index in range(1650)}
            files.update({"original.rs": {"sha256": "old"}, "gone.rs": {"sha256": "old"}})
            manifest.write_text(json.dumps({
                "commit": "1e2a7d2fc213282ef7f9ef00e1b11011da23ac68",
                "files": files,
            }), encoding="utf-8")
            with mock.patch.object(builder, "SOURCE", source), mock.patch.object(builder, "MANIFEST", manifest):
                record = builder.provenance()
            self.assertIn("original.rs", record["local_changes"]["changed"])
            self.assertIn("local.rs", record["local_changes"]["added"])
            self.assertIn("gone.rs", record["local_changes"]["missing"])
            self.assertEqual(record["local_patch_sha256"]["local.rs"], builder.digest(source / "local.rs"))

    def test_failed_build_clears_stale_stage_and_records_argv(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            component_dir = root / "component with space"
            component_dir.mkdir()
            (component_dir / "Cargo.toml").write_text('[features]\na2fo-host = []\n', encoding="utf-8")
            (component_dir / "Cargo.lock").write_text("", encoding="utf-8")
            rustup = root / "rustup"
            linker = rustup / "toolchains/1.96.0-x86_64-unknown-linux-gnu/lib/rustlib/x86_64-unknown-linux-gnu/bin/rust-lld"
            linker.parent.mkdir(parents=True)
            linker.write_text("", encoding="utf-8")
            output = root / "output with space"
            output.mkdir()
            stale = output / "A2FORefitRenderer.dll"
            stale.write_bytes(b"stale")
            source_record = {"actual_file_sha256": {"file": "hash"}, "local_changes": {"changed": [], "added": [], "missing": []}}
            calls = []

            def failed_run(argv, cwd, env=None):
                calls.append((argv, cwd, env))
                return {"argv": argv, "cwd": str(cwd), "exit_code": 2, "output": "compiler failed"}

            with mock.patch.dict(builder.COMPONENTS, {"renderer": (component_dir, "render3d.dll", "A2FORefitRenderer.dll", "a2fo-host")}), \
                    mock.patch.object(builder, "provenance", return_value=source_record), \
                    mock.patch.object(builder, "run", side_effect=failed_run), \
                    mock.patch.object(builder.shutil, "which", return_value="/usr/bin/tool"), \
                    mock.patch.dict(os.environ, {"RUSTUP_HOME": str(rustup)}):
                success, receipt_path = builder.build("renderer", output, offline=True)
            self.assertFalse(success)
            self.assertFalse(stale.exists())
            self.assertEqual(len(calls), 1)
            self.assertEqual(calls[0][1], component_dir)
            self.assertIn("--offline", calls[0][0])
            self.assertEqual(calls[0][0][-2:], ["--features", "a2fo-host"])
            self.assertEqual(calls[0][2]["CARGO_TARGET_DIR"], str(output / "target/renderer"))
            self.assertEqual(json.loads(receipt_path.read_text())["commands"][0]["exit_code"], 2)

    def test_missing_host_feature_never_invokes_compiler(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            directory = root / "render3d"
            directory.mkdir()
            (directory / "Cargo.toml").write_text("[features]\nworld = []\n", encoding="utf-8")
            (directory / "Cargo.lock").write_text("", encoding="utf-8")
            with mock.patch.dict(builder.COMPONENTS, {"renderer": (directory, "render3d.dll", "A2FORefitRenderer.dll", "a2fo-host")}), \
                    mock.patch.object(builder, "provenance", return_value={"actual_file_sha256": {}}), \
                    mock.patch.object(builder, "run") as run:
                success, receipt = builder.build("renderer", root / "stage")
            self.assertFalse(success)
            run.assert_not_called()
            self.assertIn("host adaptation is not ready", json.loads(receipt.read_text())["error"])

    def test_success_stages_only_inspected_binary_with_hash(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            directory = root / "display"
            directory.mkdir()
            (directory / "Cargo.toml").write_text("[package]\nname = 'display'\n[features]\na2fo-host = []\n", encoding="utf-8")
            (directory / "Cargo.lock").write_text("", encoding="utf-8")
            rustup = root / "rustup"
            linker = rustup / "toolchains/1.96.0-x86_64-unknown-linux-gnu/lib/rustlib/x86_64-unknown-linux-gnu/bin/rust-lld"
            linker.parent.mkdir(parents=True)
            linker.write_text("", encoding="utf-8")
            output = root / "stage"
            source_record = {"actual_file_sha256": {"file": "hash"}}

            def successful_run(argv, cwd, env=None):
                if argv[0] == "cargo":
                    built = output / "target/display/i686-pc-windows-msvc/release/display.dll"
                    built.parent.mkdir(parents=True)
                    built.write_bytes(b"new dll")
                    return {"argv": argv, "cwd": str(cwd), "exit_code": 0, "output": "compiled"}
                return {"argv": argv, "cwd": str(cwd), "exit_code": 0, "output": (
                    "display.dll: file format pei-i386\n\n"
                    "Characteristics 0x2122\n\texecutable\n\tDLL\n\n"
                    "The Export Tables (interpreted .rdata section contents)\n"
                )}

            with mock.patch.dict(builder.COMPONENTS, {"display": (directory, "display.dll", "A2FORefitDisplay.dll", "a2fo-host")}), \
                    mock.patch.object(builder, "provenance", return_value=source_record), \
                    mock.patch.object(builder, "run", side_effect=successful_run), \
                    mock.patch.object(builder.shutil, "which", return_value="/usr/bin/tool"), \
                    mock.patch.dict(os.environ, {"RUSTUP_HOME": str(rustup)}):
                success, receipt_path = builder.build("display", output)
            self.assertTrue(success)
            staged = output / "A2FORefitDisplay.dll"
            self.assertEqual(staged.read_bytes(), b"new dll")
            receipt = json.loads(receipt_path.read_text())
            self.assertEqual(receipt["artifact"]["sha256"], builder.digest(staged))
            self.assertEqual(len(receipt["commands"]), 2)
            self.assertEqual(receipt["status"], "staged")


if __name__ == "__main__":
    unittest.main()
