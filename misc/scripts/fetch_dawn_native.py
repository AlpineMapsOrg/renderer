#!/usr/bin/env python3

import argparse
import json
import os
import shutil
import sys
import tarfile
import tempfile
import urllib.request

from setup_utils import log, fail, download


def safe_extract(tar_path, extract_to):
    extract_root = os.path.abspath(extract_to)
    try:
        with tarfile.open(tar_path, "r:gz") as archive:
            for member in archive.getmembers():
                member_path = os.path.abspath(os.path.join(extract_root, member.name))
                if not member_path.startswith(extract_root + os.sep):
                    fail(f"Unsafe path in Dawn archive: {member.name}")
            archive.extractall(extract_root)
    except Exception as exc:
        fail(f"Failed to extract {tar_path}: {exc}")


def release_assets(version):
    url = f"https://api.github.com/repos/google/dawn/releases/tags/v{version}"
    request = urllib.request.Request(url, headers={"User-Agent": "webigeo-build"})
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            return json.load(response).get("assets", [])
    except Exception as exc:
        fail(f"Failed to read Dawn release metadata for v{version}: {exc}")


def find_package_root(root):
    for current, _, files in os.walk(root):
        parts = os.path.normpath(current).split(os.sep)
        if "DawnConfig.cmake" in files and len(parts) >= 3 and parts[-2:] == ["cmake", "Dawn"]:
            return os.path.abspath(os.path.join(current, "..", "..", ".."))
    fail("DawnConfig.cmake was not found in the native Dawn package")


def main():
    parser = argparse.ArgumentParser(description="Fetch a prebuilt native Dawn package")
    parser.add_argument("--extern-dir", required=True, help="External dependencies directory")
    parser.add_argument("--dawn-version", required=True, help="Dawn version to fetch")
    parser.add_argument("--build-type", required=True, choices=["Debug", "Release"], help="Dawn package configuration")
    parser.add_argument("--platform", default="ubuntu-latest", help="Dawn release platform asset name")
    args = parser.parse_args()

    extern_dir = os.path.abspath(args.extern_dir)
    dawn_dir = os.path.join(extern_dir, "dawn")
    install_dir = os.path.join(dawn_dir, "install", args.build_type)
    asset_suffix = f"-{args.platform}-{args.build_type}.tar.gz"

    matches = [
        asset for asset in release_assets(args.dawn_version)
        if asset.get("name", "").endswith(asset_suffix)
    ]
    if not matches:
        fail(f"No Dawn release asset matching *{asset_suffix} for v{args.dawn_version}")
    if len(matches) > 1:
        fail(f"Multiple Dawn release assets match *{asset_suffix} for v{args.dawn_version}")

    asset = matches[0]
    tmp_dir = tempfile.mkdtemp(prefix="fetchdawn_native_")
    archive_path = os.path.join(tmp_dir, asset["name"])

    try:
        download(asset["browser_download_url"], archive_path)
        safe_extract(archive_path, tmp_dir)
        package_root = find_package_root(tmp_dir)

        if os.path.exists(install_dir):
            log(f"Removing existing Dawn native package: {install_dir}")
            shutil.rmtree(install_dir)
        os.makedirs(os.path.dirname(install_dir), exist_ok=True)
        log(f"Installing Dawn native package to {install_dir}")
        shutil.move(package_root, install_dir)

        log(f"Successfully fetched native Dawn to {install_dir}")
        return 0
    except Exception as exc:
        log(f"Native Dawn fetch failed: {exc}")
        if os.path.exists(install_dir):
            shutil.rmtree(install_dir, ignore_errors=True)
        sys.exit(1)
    finally:
        shutil.rmtree(tmp_dir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
