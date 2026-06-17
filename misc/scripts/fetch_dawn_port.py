#!/usr/bin/env python3

import argparse
import sys
import os
import tempfile
import shutil

from setup_utils import log, fail, download, extract_zip


def main():
    parser = argparse.ArgumentParser(description="Fetch Dawn Emscripten port package")
    parser.add_argument("--extern-dir", required=True, help="External dependencies directory")
    parser.add_argument("--dawn-version", required=True, help="Dawn version to fetch")
    args = parser.parse_args()

    extern_dir = os.path.abspath(args.extern_dir)
    pkg_dir = os.path.join(extern_dir, "emdawnwebgpu_pkg")
    port_file = os.path.join(pkg_dir, "emdawnwebgpu.port.py")

    tmp = tempfile.mkdtemp(prefix="fetchdawn_")

    zipname = f"emdawnwebgpu_pkg-v{args.dawn_version}.zip"
    url = f"https://github.com/google/dawn/releases/download/v{args.dawn_version}/{zipname}"
    zippath = os.path.join(tmp, zipname)

    try:
        download(url, zippath)
        extract_zip(zippath, tmp)

        found_pkg = None
        for root, dirs, _ in os.walk(tmp):
            if "emdawnwebgpu_pkg" in dirs:
                found_pkg = os.path.join(root, "emdawnwebgpu_pkg")
                break

        if not found_pkg:
            fail("emdawnwebgpu_pkg not found in the extracted Dawn package")

        if os.path.exists(pkg_dir):
            log(f"Removing existing package directory: {pkg_dir}")
            shutil.rmtree(pkg_dir)

        log(f"Moving package to {pkg_dir}")
        shutil.move(found_pkg, pkg_dir)

        if not os.path.exists(port_file):
            fail("emdawnwebgpu.port.py missing after installation")

        log(f"Successfully fetched Dawn port to {pkg_dir}")
        return 0

    except Exception as e:
        fail(str(e))

    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
