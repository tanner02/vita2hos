"""Deploy a verified smoke-test bundle over FTP; does not launch or reboot the Switch."""

import argparse
from datetime import datetime, timezone
from ftplib import FTP, error_perm, error_temp
import getpass
import hashlib
import io
import json
from pathlib import Path
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


_UNCHECKED = object()


def replace_verified(ftp, path, data, backup_dir, stamp, expected_previous=_UNCHECKED):
    """Verify a temporary upload, save the old file locally, then replace it."""
    previous = read_remote(ftp, path)
    if expected_previous is not _UNCHECKED and previous != expected_previous:
        raise ValueError("Remote file changed before replacement: " + path)
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
    parser.add_argument("--sample", choices=["gxm_cube_sample", "gxm_textured_cube_sample", "surface_probe", "draw_probe", "visible_cube"],
                        default="gxm_cube_sample")
    parser.add_argument("--sd-root", default="/", help="FTP path corresponding to SD root")
    parser.add_argument("--backup-dir", type=Path, default=Path("switch-backups"))
    parser.add_argument("--sample-only", action="store_true",
                        help="Change only executable; preserve the installed runtime and override")
    args = parser.parse_args()
    manifest, (nsp, executable) = read_bundle(args.bundle, args.sample)
    password = getpass.getpass("FTP password: ") if args.ask_password else "anonymous@"
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    root = args.sd_root.rstrip("/")
    with FTP() as ftp:
        ftp.connect(args.host, args.port, timeout=15)
        ftp.login(args.user, password)
        print("Connected:", ftp.getwelcome())
        ensure_directory(ftp, root + "/vita2hos")
        if args.sample_only and read_remote(ftp, root + "/vita2hos/vita2hos.nsp") != nsp:
            raise ValueError("Installed runtime differs from this bundle; sample-only transfer aborted")
        transfers = [("executable", executable)] if args.sample_only else [
            ("vita2hos.nsp", nsp), ("executable", executable)]
        for name, data in transfers:
            replace_verified(ftp, root + "/vita2hos/" + name, data, args.backup_dir, stamp)
        print("Runtime commit:", manifest["runtime_commit"])
        print("Sample:", args.sample)
        print("Files ready. Atmosphere configuration was not changed.")
        print("FTP transfer does not launch the application or capture a rendered result.")


if __name__ == "__main__":
    main()
