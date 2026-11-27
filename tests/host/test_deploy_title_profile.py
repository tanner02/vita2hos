"""Offline title-profile installation and rollback checks."""
import importlib.util
from ftplib import error_perm
from pathlib import Path
import tempfile
import unittest

from test_deploy_switch import FakeFTP

source = Path(__file__).resolve().parents[2] / "tools/deploy-title-profile.py"
spec = importlib.util.spec_from_file_location("deploy_title_profile", source)
deploy = importlib.util.module_from_spec(spec)
spec.loader.exec_module(deploy)
TITLE = "0100152000022000"
FOLDER = "/atmosphere/contents/" + TITLE
ACTIVE = FOLDER + "/exefs.nsp"
CONFIG = "/atmosphere/config/override_config.ini"
NORMAL = b"[hbl_config]\npath=atmosphere/hbl.nsp\noverride_any_app=true\noverride_any_app_key=R\noverride_any_app_address_space=39_bit\n"


class TitleFTP(FakeFTP):
    def __init__(self):
        super().__init__()
        self.files = {CONFIG: NORMAL, "/hbmenu.nro": b"hbmenu", "/atmosphere/hbl.nsp": b"hbl"}
        self.dirs = {"/", "/atmosphere", "/atmosphere/config", "/atmosphere/contents"}
        self.current = "/"

    def nlst(self, path):
        if path not in self.dirs:
            raise error_perm("550 No such file or directory")
        return [name for name in self.files.keys() | self.dirs
                if name != path and str(Path(name).as_posix()).rsplit("/", 1)[0] == path]

    def mkd(self, path):
        self.dirs.add(path)

    def pwd(self): return self.current

    def cwd(self, path):
        if path not in self.dirs: raise error_perm("550 missing")
        self.current = path


class ProfileDeploymentTests(unittest.TestCase):
    def setUp(self):
        self.ftp = TitleFTP()
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.backup = Path(self.temp.name)

    def install(self, **kwargs):
        return deploy.deploy(self.ftp, TITLE, b"profile", self.backup, "stamp", **kwargs)

    def existing(self, data):
        self.ftp.dirs.add(FOLDER)
        self.ftp.files[ACTIVE] = data

    def test_install_and_repeat_preserve_homebrew(self):
        before = self.ftp.files.copy()
        self.install()
        self.assertEqual(self.ftp.files, {**before, ACTIVE: b"profile"})
        self.assertEqual(self.install(), "Unchanged")
        self.assertEqual(self.ftp.uploads, 1)

    def test_bad_global_configuration_refuses_before_writes(self):
        for config in (NORMAL.replace(b"39_bit", b"32_bit"),
                       NORMAL.replace(b"atmosphere/hbl.nsp", b"vita2hos/vita2hos.nsp")):
            self.ftp.files[CONFIG] = config
            before = self.ftp.files.copy()
            with self.assertRaises(ValueError): self.install()
            self.assertEqual(self.ftp.files, before)
            self.assertEqual(self.ftp.uploads, 0)

    def test_existing_loose_or_unknown_exefs_refused(self):
        self.ftp.dirs.update((FOLDER, FOLDER + "/exefs"))
        with self.assertRaises(ValueError): self.install()
        self.ftp.dirs.remove(FOLDER + "/exefs")
        self.existing(b"another mod")
        with self.assertRaises(ValueError): self.install()
        self.assertEqual(self.ftp.uploads, 0)

    def test_explicit_update_keeps_local_and_remote_backups(self):
        self.existing(b"old profile")
        self.install(expected=deploy.transfer.digest(b"old profile"))
        self.assertEqual(self.ftp.files[ACTIVE], b"profile")
        self.assertEqual(self.ftp.files[ACTIVE + ".backup-stamp"], b"old profile")
        self.assertEqual([p.read_bytes() for p in self.backup.iterdir()], [b"old profile"])

    def test_disable_only_matching_profile_without_global_preconditions(self):
        self.existing(b"unknown")
        with self.assertRaises(ValueError): self.install(disable=True)
        self.existing(b"profile")
        self.ftp.files[CONFIG] = b"unrelated configuration"
        self.install(disable=True)
        self.assertNotIn(ACTIVE, self.ftp.files)
        self.assertEqual(self.ftp.files[FOLDER + "/exefs.vita2hos-disabled-stamp.nsp"], b"profile")
        self.assertEqual(self.ftp.files[CONFIG], b"unrelated configuration")
        self.assertEqual(self.install(disable=True), "Already disabled")

    def test_transfer_failures_preserve_active_profile(self):
        for failure in ("corrupt", "fail_rename"):
            with self.subTest(failure=failure):
                self.ftp = TitleFTP()
                self.existing(b"old")
                setattr(self.ftp, failure, True)
                with self.assertRaises((RuntimeError, OSError)):
                    self.install(expected=deploy.transfer.digest(b"old"))
                self.assertEqual(self.ftp.files[ACTIVE], b"old")
                self.assertEqual(self.ftp.files[CONFIG], NORMAL)

    def test_stale_previous_value_refuses_before_upload(self):
        self.existing(b"changed")
        with self.assertRaises(ValueError):
            deploy.transfer.replace_verified(self.ftp, ACTIVE, b"profile", self.backup,
                                             "stamp", expected_previous=b"old")
        self.assertEqual(self.ftp.uploads, 0)

    def test_parent_traversal_rejected(self):
        with self.assertRaises(ValueError): self.install(root="/sd/../")
        self.assertEqual(self.ftp.uploads, 0)


if __name__ == "__main__": unittest.main()
