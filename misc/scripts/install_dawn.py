#!/usr/bin/env python3

import argparse
import sys
import os
import tempfile
import shutil

from setup_utils import log, fail, download, extract_zip, run_command


def main():
    parser = argparse.ArgumentParser(description="Install Dawn WebGPU library from source")
    parser.add_argument("--extern-dir", required=True, help="External dependencies directory")
    parser.add_argument("--dawn-version", required=True, help="Dawn version to install")
    parser.add_argument("--cmake-path", default="cmake", help="Path to CMake executable")
    parser.add_argument("--ninja-path", default="ninja", help="Path to Ninja executable")
    args = parser.parse_args()

    extern_dir = os.path.abspath(args.extern_dir)
    dawn_dir = os.path.join(extern_dir, "dawn")

    tmp_dir = tempfile.mkdtemp(prefix="installdawn_")
    zip_name = f"v{args.dawn_version}.zip"
    zip_path = os.path.join(tmp_dir, zip_name)
    url = f"https://github.com/google/dawn/archive/refs/tags/{zip_name}"

    try:
        download(url, zip_path)
        extract_zip(zip_path, extern_dir)
        os.unlink(zip_path)

        extracted_dir = None
        for entry in os.listdir(extern_dir):
            full = os.path.join(extern_dir, entry)
            if os.path.isdir(full) and entry.startswith("dawn-"):
                extracted_dir = full
                break

        if not extracted_dir:
            fail("Cannot find extracted dawn-* directory")

        if os.path.exists(dawn_dir):
            log(f"Removing existing Dawn directory: {dawn_dir}")
            shutil.rmtree(dawn_dir)

        log(f"Moving extracted Dawn to {dawn_dir}")
        shutil.move(extracted_dir, dawn_dir)

        for build_type in ["Debug", "Release"]:
            log(f"Building Dawn ({build_type} configuration)")

            out_dir = os.path.join(dawn_dir, "out", build_type)
            install_prefix = os.path.join(dawn_dir, "install", build_type)
            os.makedirs(out_dir, exist_ok=True)

            cmake_args = [
                args.cmake_path,
                "-G", "Ninja",
                dawn_dir,
                "-B", out_dir,
                "-DDAWN_BUILD_MONOLITHIC_LIBRARY=STATIC",
                "-DDAWN_FORCE_SYSTEM_COMPONENT_LOAD=ON",
                "-DDAWN_FETCH_DEPENDENCIES=ON",
                "-DDAWN_ENABLE_INSTALL=ON",
                f"-DCMAKE_BUILD_TYPE={build_type}",
                "-DTINT_BUILD_SPV_READER=OFF",
                "-DTINT_BUILD_TESTS=OFF",
                "-DTINT_BUILD_FUZZERS=OFF",
                "-DTINT_BUILD_BENCHMARKS=OFF",
                "-DTINT_BUILD_AS_OTHER_OS=OFF",
                "-DDAWN_BUILD_SAMPLES=OFF",
                "-DDAWN_ENABLE_D3D11=OFF",
                "-DDAWN_ENABLE_D3D12=OFF",
                "-DDAWN_ENABLE_METAL=OFF",
                "-DDAWN_ENABLE_NULL=OFF",
                "-DDAWN_ENABLE_DESKTOP_GL=OFF",
                "-DDAWN_ENABLE_OPENGLES=OFF",
                "-DDAWN_ENABLE_VULKAN=ON",
                "-DDAWN_USE_WINDOWS_UI=OFF",
                "-DDAWN_USE_GLFW=OFF"
            ]

            run_command(
                cmake_args,
                description=f"Configuring Dawn ({build_type})"
            )

            run_command(
                [args.cmake_path, "--build", out_dir],
                description=f"Building Dawn ({build_type})"
            )

            run_command(
                [args.cmake_path, "--install", out_dir, "--prefix", install_prefix],
                description=f"Installing Dawn ({build_type})"
            )

        log("Cleaning up Dawn build files and sources")
        for entry in os.listdir(dawn_dir):
            if entry != "install":
                full_path = os.path.join(dawn_dir, entry)
                if os.path.isdir(full_path):
                    shutil.rmtree(full_path, ignore_errors=True)
                else:
                    os.remove(full_path)

        log(f"Successfully installed Dawn to {dawn_dir}/install")
        return 0

    except Exception as e:
        log(f"Installation failed: {e}")
        if os.path.exists(tmp_dir):
            shutil.rmtree(tmp_dir, ignore_errors=True)
        if os.path.exists(dawn_dir):
            shutil.rmtree(dawn_dir, ignore_errors=True)
        sys.exit(1)

    finally:
        if os.path.exists(tmp_dir):
            shutil.rmtree(tmp_dir, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
