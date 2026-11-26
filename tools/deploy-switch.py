"""Deploy a verified smoke-test bundle over FTP; does not launch or reboot the Switch."""

import argparse
from datetime import datetime, timezone
from ftplib import FTP, error_perm, error_temp
import getpass
import hashlib
import io
import json
from pathlib import Path
import re
import zipfile


def digest(data):
    return hashlib.sha256(data).hexdigest()


def read_remote(ftp, path):
    output = io.BytesIO()
    try:
        ftp.retrbinary("RETR " + path, output.write)
    except (error_perm, error_temp) as error:
        if str(error).startswith("550") or (
            str(error).startswith("450") and "no such file" in str(error).lower()
        ):
            return None
        raise
    return output.getvalue()


def ensure_directory(ftp, path):
    try:
        ftp.mkd(path)
    except error_perm:
        # Only ignore MKD failure if the directory actually exists.
        previous = ftp.pwd()
        ftp.cwd(path)
        ftp.cwd(previous)


def replace_verified(ftp, path, data, backup_dir, stamp):
    """Verify a temporary upload, save the old file locally, then replace it."""
    previous = read_remote(ftp, path)
    if previous == data:
        print("Unchanged:", path)
        return
    if previous is not None:
        backup = backup_dir / (path.strip("/").replace("/", "__") + "." + stamp)
        backup.parent.mkdir(parents=True, exist_ok=True)
        backup.write_bytes(previous)
        print("Local backup:", backup)
    temporary = path + ".upload-" + stamp
    ftp.storbinary("STOR " + temporary, io.BytesIO(data))
    uploaded = read_remote(ftp, temporary)
    if uploaded is None or digest(uploaded) != digest(data):
        raise RuntimeError("Upload verification failed; original unchanged: " + path)
    remote_backup = path + ".backup-" + stamp
    if previous is not None:
        ftp.rename(path, remote_backup)
    try:
        ftp.rename(temporary, path)
    except Exception:
        if previous is not None:
            ftp.rename(remote_backup, path)
        raise
    print("Uploaded and verified:", path)


def override_config(existing):
    """Preserve other sections/comments, updating only vita2hos override keys."""
    values = {
        "override_any_app": "true",
        "override_any_app_key": "R",
        "override_any_app_address_space": "32_bit",
        "path": "vita2hos/vita2hos.nsp",
    }
    lines = existing.decode("utf-8-sig").splitlines() if existing else []
    result, in_hbl, found, written = [], False, False, set()

    def append_missing():
        for key, value in values.items():
            if key not in written:
                result.append(key + "=" + value)
                written.add(key)

    for line in lines:
        section = re.match(r"\s*\[([^]]+)\]", line)
        if section:
            if in_hbl:
                append_missing()
            in_hbl = section.group(1).lower() == "hbl_config"
            if in_hbl and found:
                raise ValueError("Duplicate hbl_config sections; edit configuration manually")
            found |= in_hbl
        setting = re.match(r"\s*([A-Za-z0-9_]+)\s*=", line)
        key = setting.group(1).lower() if setting else None
        if in_hbl and key in values:
            if key not in written:
                result.append(key + "=" + values[key])
                written.add(key)
        else:
            result.append(line)
    if not found:
        result.extend(["", "[hbl_config]"])
    append_missing()
    return ("\n".join(result).lstrip("\n") + "\n").encode("utf-8")


def read_bundle(path, sample):
    with zipfile.ZipFile(path) as archive:
        manifest = json.loads(archive.read("manifest.json"))
        paths = ["sd/vita2hos/vita2hos.nsp", "samples/" + sample + "/executable"]
        contents = [archive.read(name) for name in paths]
        for name, data in zip(paths, contents):
            if digest(data) != manifest["files"].get(name):
                raise ValueError("Bundle checksum mismatch: " + name)
        if contents[0][:4] != b"PFS0" or contents[1][:4] != b"\x7fELF":
            raise ValueError("Bundle must contain an NSP and a Vita ELF sample")
        return manifest, contents


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=5000)
    parser.add_argument("--user", default="anonymous")
    parser.add_argument("--ask-password", action="store_true")
    parser.add_argument("--sample", choices=["gxm_cube_sample", "gxm_textured_cube_sample"],
                        default="gxm_cube_sample")
    parser.add_argument("--sd-root", default="/", help="FTP path corresponding to SD root")
    parser.add_argument("--backup-dir", type=Path, default=Path("switch-backups"))
    parser.add_argument("--configure-override", action="store_true",
                        help="Also change R application override to vita2hos; backs up existing config")
    args = parser.parse_args()
    manifest, (nsp, executable) = read_bundle(args.bundle, args.sample)
    password = getpass.getpass("FTP password: ") if args.ask_password else "anonymous@"
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    root = args.sd_root.rstrip("/")
    with FTP() as ftp:
        ftp.connect(args.host, args.port, timeout=15)
        ftp.login(args.user, password)
        print("Connected:", ftp.getwelcome())
        if args.configure_override:
            # Verify the Atmosphere folder before making any deployment changes.
            ftp.cwd(root + "/atmosphere/config")
            ftp.cwd("/")
        ensure_directory(ftp, root + "/vita2hos")
        for name, data in [("vita2hos.nsp", nsp), ("executable", executable)]:
            replace_verified(ftp, root + "/vita2hos/" + name, data, args.backup_dir, stamp)
        if args.configure_override:
            path = root + "/atmosphere/config/override_config.ini"
            previous = read_remote(ftp, path)
            replace_verified(ftp, path, override_config(previous), args.backup_dir, stamp)
        print("Runtime commit:", manifest["runtime_commit"])
        print("Sample:", args.sample)
        print("Files ready. Exit FTP, reboot if the override changed, then hold R while launching a game.")
        print("FTP transfer does not launch the application or capture a rendered result.")


if __name__ == "__main__":
    main()
