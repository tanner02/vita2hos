"""Offline checks for FTP backup/verification behavior; no network connections."""
import importlib.util
import io
from ftplib import error_perm, error_temp
from pathlib import Path
import tempfile
import unittest

source = Path(__file__).resolve().parents[2] / "tools/deploy-switch.py"
spec = importlib.util.spec_from_file_location("deploy_switch", source)
deploy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deploy)


class FakeFTP:
    def __init__(self):
        self.files = {"/test": b"original"}
        self.corrupt = False
        self.fail_rename = False
        self.uploads = 0

    def retrbinary(self, command, callback):
        path = command.removeprefix("RETR ")
        if path not in self.files:
            raise error_perm("550 missing")
        callback(self.files[path])

    def storbinary(self, command, stream):
        self.uploads += 1
        data = stream.read()
        self.files[command.removeprefix("STOR ")] = b"bad" if self.corrupt else data

    def rename(self, old, new):
        if self.fail_rename and ".upload-" in old:
            self.fail_rename = False
            raise OSError("simulated failed rename")
        self.files[new] = self.files.pop(old)


class DeploymentTests(unittest.TestCase):
    def test_preserve_configuration_and_idempotence(self):
        original = b"; comment\n[hbl_config]\npath=old\n[default_config]\ncheat_enable_key=!L\n"
        result = deploy.override_config(original)
        self.assertIn(b"; comment", result)
        self.assertIn(b"[default_config]\ncheat_enable_key=!L", result)
        self.assertIn(b"override_any_app_address_space=32_bit", result)
        self.assertEqual(deploy.override_config(result), result)

    def test_duplicate_sections_rejected(self):
        with self.assertRaises(ValueError):
            deploy.override_config(b"[hbl_config]\n[hbl_config]\n")

    def test_verified_replacement_keeps_both_backups(self):
        ftp = FakeFTP()
        with tempfile.TemporaryDirectory() as directory:
            deploy.replace_verified(ftp, "/test", b"new", Path(directory), "stamp")
            self.assertEqual(ftp.files["/test"], b"new")
            self.assertEqual(ftp.files["/test.backup-stamp"], b"original")
            self.assertEqual((Path(directory) / "test.stamp").read_bytes(), b"original")

    def test_corrupt_upload_preserves_original(self):
        ftp = FakeFTP()
        ftp.corrupt = True
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(RuntimeError):
                deploy.replace_verified(ftp, "/test", b"new", Path(directory), "stamp")
        self.assertEqual(ftp.files["/test"], b"original")

    def test_failed_rename_restores_original(self):
        ftp = FakeFTP()
        ftp.fail_rename = True
        with tempfile.TemporaryDirectory() as directory:
            with self.assertRaises(OSError):
                deploy.replace_verified(ftp, "/test", b"new", Path(directory), "stamp")
        self.assertEqual(ftp.files["/test"], b"original")

    def test_unchanged_does_not_upload(self):
        ftp = FakeFTP()
        with tempfile.TemporaryDirectory() as directory:
            deploy.replace_verified(ftp, "/test", b"original", Path(directory), "stamp")
        self.assertEqual(ftp.uploads, 0)

    def test_server_specific_missing_response(self):
        class Missing:
            def retrbinary(self, command, callback):
                raise error_temp("450 No such file or directory")
        self.assertIsNone(deploy.read_remote(Missing(), "/missing"))


if __name__ == "__main__":
    unittest.main()
