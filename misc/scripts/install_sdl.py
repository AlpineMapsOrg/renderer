#!/usr/bin/env python3

import argparse
import sys
import os
import tempfile
import shutil

from setup_utils import log, fail, run_command


def main():
    parser = argparse.ArgumentParser(description="Install SDL2 from source")
    parser.add_argument("--install-prefix", required=True, help="Installation prefix directory")
    parser.add_argument("--cmake-path", default="cmake", help="Path to CMake executable")
    parser.add_argument("--ninja-path", default="ninja", help="Path to Ninja executable")
    parser.add_argument("--git-path", default="git", help="Path to Git executable")
    args = parser.parse_args()

    install_prefix = os.path.abspath(args.install_prefix)
    tmp_dir = tempfile.mkdtemp(prefix="installsdl_")
    clone_dir = os.path.join(tmp_dir, "SDL")
    repo_url = "https://github.com/libsdl-org/SDL.git"

    try:
        log(f"Cloning SDL2 repository from {repo_url}")
        run_command(
            [args.git_path, "clone", repo_url, clone_dir],
            description="Cloning SDL repository"
        )

        log("Checking out SDL2 branch")
        run_command(
            [args.git_path, "checkout", "SDL2"],
            cwd=clone_dir,
            description="Switching to SDL2 branch"
        )

        for build_type in ["Release"]:
            log(f"Building SDL2 ({build_type} configuration)")
            build_dir = os.path.join(clone_dir, "build", build_type)
            os.makedirs(build_dir, exist_ok=True)

            run_command(
                [
                    args.cmake_path, clone_dir,
                    "-G", "Ninja",
                    f"-DCMAKE_BUILD_TYPE={build_type}",
                    f"-DCMAKE_INSTALL_PREFIX={install_prefix}"
                ],
                cwd=build_dir,
                description=f"Configuring SDL2 ({build_type})"
            )

            run_command(
                [args.ninja_path],
                cwd=build_dir,
                description=f"Building SDL2 ({build_type})"
            )

            run_command(
                [args.ninja_path, "install"],
                cwd=build_dir,
                description=f"Installing SDL2 ({build_type})"
            )

        log("Cleaning up temporary build files")
        shutil.rmtree(tmp_dir, ignore_errors=True)

        log(f"Successfully installed SDL2 to {install_prefix}")
        return 0

    except Exception as exc:
        log(f"Installation failed: {exc}")
        if os.path.exists(tmp_dir):
            shutil.rmtree(tmp_dir, ignore_errors=True)
        sys.exit(1)


if __name__ == "__main__":
    sys.exit(main())
