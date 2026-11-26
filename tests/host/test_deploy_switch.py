"""Offline checks for FTP backup/verification behavior; no network connections."""
import importlib.util
import io
from ftplib import error_perm, error_temp
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

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
    def test_removed_override_option_rejected_before_io(self):
        with patch("sys.argv", ["deploy-switch.py", "missing.zip", "--host",
                                "192.0.2.1", "--configure-override"]), \
             patch.object(deploy, "FTP") as ftp, \
             patch.object(deploy, "read_bundle") as bundle, \
             patch("sys.stderr", new_callable=io.StringIO):
            with self.assertRaises(SystemExit) as result:
                deploy.main()
            self.assertEqual(result.exception.code, 2)
            ftp.assert_not_called()
            bundle.assert_not_called()

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
