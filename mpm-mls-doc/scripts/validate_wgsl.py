#!/usr/bin/env python3
"""Validate the MPM WGSL kernels with tint.

The kernels use weBIGeo's `///use` include directive, which tint does not understand, so
includes are resolved into a flat file first. Much faster than launching the app, and WGSL
otherwise only fails at runtime.

    python3 mpm-mls-doc/scripts/validate_wgsl.py
"""
import os
import re
import subprocess
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
TINT = os.path.join(ROOT, "extern/dawn/install/Release/bin/tint")
NS_DIRS = {
    "webgpu_compute": os.path.join(ROOT, "webgpu/compute/shaders"),
    "webgpu": os.path.join(ROOT, "webgpu/base/shaders"),
}
USE_RE = re.compile(r"^\s*///use\s+(?:([A-Za-z_][A-Za-z0-9_]*)::)?([/\w .-]+?)\s*$")

KERNELS = ["mpm_prepare", "mpm_seed", "mpm_clear_grid", "mpm_p2g",
           "mpm_grid_update", "mpm_g2p", "mpm_splat", "mpm_rasterize"]


def resolve(path, namespace, seen):
    """Inline ///use includes; drop other /// directives (none used by the MPM kernels)."""
    if path in seen:
        return ""
    seen.add(path)
    out = []
    with open(path) as f:
        for line in f:
            m = USE_RE.match(line)
            if not m:
                if not line.lstrip().startswith("///"):
                    out.append(line)
                continue
            inc_ns = m.group(1) or namespace
            inc_dir = NS_DIRS.get(inc_ns)
            if inc_dir is None:
                print(f"  !! unknown shader namespace {inc_ns}", file=sys.stderr)
                continue
            inc_path = os.path.join(inc_dir, m.group(2) + ".wgsl")
            if not os.path.exists(inc_path):
                print(f"  !! missing include {inc_path}", file=sys.stderr)
                continue
            out.append(resolve(inc_path, inc_ns, seen))
    return "".join(out)


def main():
    if not os.path.exists(TINT):
        print(f"tint not found at {TINT}\n"
              "It ships with the vendored Dawn; build once so extern/dawn is populated.",
              file=sys.stderr)
        return 2

    failed = 0
    for kernel in KERNELS:
        src = resolve(os.path.join(ROOT, "webgpu/compute/shaders", kernel + ".wgsl"),
                      "webgpu_compute", set())
        tmp = os.path.join(os.path.dirname(__file__), f"_resolved_{kernel}.wgsl")
        with open(tmp, "w") as f:
            f.write(src)
        result = subprocess.run([TINT, "--format", "wgsl", tmp],
                                capture_output=True, text=True)
        if result.returncode != 0:
            failed += 1
            print(f"=== {kernel}: FAIL ===")
            print(result.stderr.strip()[:3000])
        else:
            print(f"=== {kernel}: ok ===")
            os.remove(tmp)  # keep only the ones that failed, for inspection

    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
