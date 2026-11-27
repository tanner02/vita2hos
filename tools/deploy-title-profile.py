"""Install or disable a verified title profile over FTP; preserve global hbmenu."""
import argparse
import configparser
from datetime import datetime, timezone
from ftplib import FTP, error_perm, error_temp
import getpass
import importlib.util
import json
from pathlib import Path, PurePosixPath
import zipfile


def sibling(name):
    spec = importlib.util.spec_from_file_location(name.replace("-", "_"), Path(__file__).with_name(name + ".py"))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


transfer = sibling("deploy-switch")
package = sibling("package-title-profile")


def read_profile(path):
    with zipfile.ZipFile(path) as archive:
        manifest = json.loads(archive.read("manifest.json"))
        title = package.program_id(manifest["host_program_id"])
        target = f"sd/atmosphere/contents/{title:016x}/exefs.nsp"
        if sorted(archive.namelist()) != sorted([target, "manifest.json", "TITLE-PROFILE.txt"]):
            raise ValueError("Unexpected title-profile files")
        data = archive.read(target)
        if transfer.digest(data) != manifest["files"].get(target):
            raise ValueError("Title-profile checksum mismatch")
        if package.retarget(data, title) != data:
            raise ValueError("Profile metadata does not match the host application")
        return f"{title:016x}", data


def title_entries(ftp, path):
    try:
        names = ftp.nlst(path)
    except (error_perm, error_temp) as error:
        if str(error).startswith(("450", "550")) and any(
                word in str(error).lower() for word in ("not found", "no such", "missing", "does not exist")):
            return {}
        raise
    return {PurePosixPath(name.rstrip("/")).name.lower(): PurePosixPath(name.rstrip("/")).name
            for name in names if PurePosixPath(name.rstrip("/")).name not in ("", ".", "..")}


def check_homebrew(ftp, root):
    raw = transfer.read_remote(ftp, root + "/atmosphere/config/override_config.ini")
    if raw is None:
        raise ValueError("Cannot verify normal hbmenu configuration")
    config = configparser.ConfigParser()
    config.read_string(raw.decode("utf-8-sig"))
    section = config["hbl_config"]
    path = section.get("path", "").replace("\\", "/").removeprefix("sdmc:/").lstrip("/")
    if (path != "atmosphere/hbl.nsp" or not section.getboolean("override_any_app", fallback=False) or
            section.get("override_any_app_key", "").upper() != "R" or
            section.get("override_any_app_address_space", "").lower() != "39_bit"):
        raise ValueError("Normal R / 39_bit hbmenu configuration must be restored first")
    for name in ("/hbmenu.nro", "/atmosphere/hbl.nsp"):
        if not transfer.read_remote(ftp, root + name):
            raise ValueError("Normal homebrew file missing or unreadable: " + name)


def deploy(ftp, title, data, backup_dir, stamp, root="/", expected=None, disable=False):
    title = f"{package.program_id(title):016x}"
    if not root.startswith("/") or ".." in PurePosixPath(root).parts:
        raise ValueError("SD root must be an absolute FTP path without parent traversal")
    root = root.rstrip("/")
    folder = root + "/atmosphere/contents/" + title
    entries = title_entries(ftp, folder)
    active = folder + "/" + entries.get("exefs.nsp", "exefs.nsp")
    current = transfer.read_remote(ftp, active) if "exefs.nsp" in entries else None
    if "exefs.nsp" in entries and current is None:
        raise ValueError("Existing ExeFS replacement is unreadable")
    if disable:
        if current is None: return "Already disabled"
        if current != data: raise ValueError("Refusing to disable an unknown ExeFS replacement")
        backup_dir.mkdir(parents=True, exist_ok=True)
        (backup_dir / (title + "-exefs-" + stamp + ".nsp")).write_bytes(current)
        ftp.rename(active, folder + "/exefs.vita2hos-disabled-" + stamp + ".nsp")
        return "Disabled; host application restored"
    check_homebrew(ftp, root)
    if "exefs" in entries: raise ValueError("Existing loose ExeFS overrides conflict with this profile")
    if current == data: return "Unchanged"
    if current is not None and (expected is None or transfer.digest(current) != expected):
        raise ValueError("Existing ExeFS replacement requires its matching --expected-installed-sha256")
    if current is None and expected is not None:
        raise ValueError("Expected installed profile is missing")
    for directory in (root + "/atmosphere", root + "/atmosphere/contents", folder):
        transfer.ensure_directory(ftp, directory)
    transfer.replace_verified(ftp, active, data, backup_dir, stamp, expected_previous=current)
    return "Installed; normal host launch runs vita2hos, R opens hbmenu"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("profile", type=Path)
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=5000)
    parser.add_argument("--user", default="anonymous")
    parser.add_argument("--ask-password", action="store_true")
    parser.add_argument("--sd-root", default="/")
    parser.add_argument("--backup-dir", type=Path, default=Path("switch-backups"))
    parser.add_argument("--expected-installed-sha256")
    parser.add_argument("--disable", action="store_true")
    args = parser.parse_args()
    title, data = read_profile(args.profile)
    password = getpass.getpass("FTP password: ") if args.ask_password else "anonymous@"
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    with FTP() as ftp:
        ftp.connect(args.host, args.port, timeout=15)
        ftp.login(args.user, password)
        print(deploy(ftp, title, data, args.backup_dir, stamp, args.sd_root,
                     args.expected_installed_sha256, args.disable))
    print("Global homebrew configuration was not changed. No application was launched.")


if __name__ == "__main__": main()
