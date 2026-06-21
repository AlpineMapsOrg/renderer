#!/usr/bin/env python3
import os
import re
import shutil
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

FALLBACK_WASM_OBJDUMP = r"C:\tmp\path\wabt-1.0.41\bin\wasm-objdump.exe"


def find_wasm_objdump():
    if shutil.which("wasm-objdump"):
        return "wasm-objdump"
    if os.path.isfile(FALLBACK_WASM_OBJDUMP):
        return FALLBACK_WASM_OBJDUMP
    print("Error: wasm-objdump not found in PATH and fallback path does not exist:")
    print(f"  {FALLBACK_WASM_OBJDUMP}")
    sys.exit(1)


def find_newest_wasm(build_dir: Path) -> Path:
    wasm_files = list(build_dir.rglob("*.wasm"))
    if not wasm_files:
        print(f"Error: No .wasm files found under {build_dir}")
        sys.exit(1)
    return max(wasm_files, key=lambda p: p.stat().st_mtime)


def parse_sections(output: str) -> list[tuple[str, int, int | None]]:
    sections = []
    in_sections = False
    for line in output.splitlines():
        stripped = line.strip()
        if stripped.startswith("Sections:"):
            in_sections = True
            continue
        if not in_sections or not stripped:
            continue
        m_size = re.search(r'\(size=0x([0-9a-fA-F]+)\)', stripped)
        m_count = re.search(r'count:\s*(\d+)', stripped)
        parts = stripped.split()
        if not parts or not m_size:
            continue
        name = parts[0]
        size = int(m_size.group(1), 16)
        count = int(m_count.group(1)) if m_count else None
        sections.append((name, size, count))
    return sections


def human_size(n: float) -> str:
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return f"{n:.1f} {unit}" if unit != "B" else f"{int(n)} B"
        n /= 1024


def bar(fraction: float, width: int = 28) -> str:
    filled = round(fraction * width)
    return "#" * filled + "-" * (width - filled)


def library_from_path(obj_path: Path, build_root: Path) -> str:
    """Extract a human-readable library name from a CMake object file path."""
    try:
        rel = obj_path.relative_to(build_root)
    except ValueError:
        return obj_path.parts[-1]
    parts = rel.parts
    for i, part in enumerate(parts):
        if part == "CMakeFiles" and i + 1 < len(parts):
            lib = parts[i + 1]
            if lib.endswith(".dir"):
                lib = lib[:-4]
            if lib.endswith("_autogen"):
                lib = lib[:-8]
            return lib
    return parts[0]


def parse_ninja_link(build_root: Path, wasm_file: Path) -> tuple[list[Path], list[tuple[Path, str]]] | None:
    """Parse build.ninja and return (direct_obj_files, archive_list).
    archive_list entries are (abs_path, lib_name). Returns None if not parseable."""
    ninja = build_root / "build.ninja"
    if not ninja.exists():
        return None

    target_stem = wasm_file.stem
    text = ninja.read_text(encoding="utf-8", errors="replace")
    pattern = rf"^build [^\n]*{re.escape(target_stem)}\.js:.*?(?=^build |\Z)"
    m = re.search(pattern, text, re.MULTILINE | re.DOTALL)
    if not m:
        return None
    block = m.group(0)

    def resolve(token: str) -> Path:
        token = re.sub(r"^C\$:", "C:", token)
        p = Path(token)
        return p if p.is_absolute() else build_root / p

    # Direct .o files (before the first |)
    obj_files: list[Path] = []
    for token in block.split("|")[0].split():
        if token.endswith(".o"):
            p = resolve(token)
            if p.exists():
                obj_files.append(p)

    # Archives from LINK_LIBRARIES
    archives: list[tuple[Path, str]] = []
    m_libs = re.search(r"^\s*LINK_LIBRARIES\s*=\s*(.+)$", block, re.MULTILINE)
    if m_libs:
        seen: set[Path] = set()
        for token in m_libs.group(1).split():
            if not token.endswith(".a"):
                continue
            lib_path = resolve(token)
            if lib_path in seen:
                continue
            seen.add(lib_path)
            lib_name = lib_path.stem.removeprefix("lib")
            archives.append((lib_path, lib_name))

    return obj_files, archives


def analyze_objects(build_root: Path, wasm_file: Path, wasm_objdump: str) -> tuple[dict[str, dict[str, int]], bool]:
    """Scan object files and accumulate Code/Data sizes per library."""
    parsed = parse_ninja_link(build_root, wasm_file)
    used_ninja = parsed is not None

    if not used_ninja:
        all_objs: list[tuple[Path, str]] = [(p, library_from_path(p, build_root)) for p in build_root.rglob("*.o")]
        unresolved_archives: list[tuple[Path, str]] = []
    else:
        direct_objs, archives = parsed
        all_objs = [(p, library_from_path(p, build_root)) for p in direct_objs]
        unresolved_archives = []
        for lib_path, lib_name in archives:
            # Try to find .o files in the build tree for this archive
            cmake_dir = lib_path.parent / "CMakeFiles" / f"{lib_name}.dir"
            if cmake_dir.exists():
                for o in cmake_dir.rglob("*.o"):
                    all_objs.append((o, lib_name))
            else:
                # External archive (e.g. Qt) — no .o files in build tree
                unresolved_archives.append((lib_path, lib_name))

    # Deduplicate
    seen_paths: set[Path] = set()
    unique_objs = [(p, lib) for p, lib in all_objs if not (p in seen_paths or seen_paths.add(p))]

    print(f"Scanning {len(unique_objs)} .o files + {len(unresolved_archives)} external archives...", flush=True)

    totals: dict[str, dict[str, int]] = defaultdict(lambda: defaultdict(int))

    for obj, lib in unique_objs:
        result = subprocess.run([wasm_objdump, str(obj), "-h"], capture_output=True, text=True)
        if result.returncode != 0:
            continue
        for sec_name, size, _ in parse_sections(result.stdout):
            totals[lib][sec_name] += size

    # For external archives, extract and scan individual members with wasm2wat trick:
    # wasm-objdump doesn't understand .a format, so note them as unanalyzed.
    for lib_path, lib_name in unresolved_archives:
        totals[f"{lib_name} (external, not analyzed)"]["Code"] += 0

    return totals, used_ninja


def print_section_table(title: str, rows: list[tuple[str, int]], total: int, note: str = ""):
    label = "Library"
    print(f"\n{title}")
    if note:
        print(f"  ({note})")
    name_w = max((len(r[0]) for r in rows), default=10)
    name_w = max(name_w, len(label))
    size_w = max((len(human_size(r[1])) for r in rows), default=6)
    size_w = max(size_w, 6)
    header = f"  {label:<{name_w}}  {'Size':>{size_w}}  {'%':>5}  {'Bar':28}"
    print(header)
    print("-" * len(header))
    for name, size in sorted(rows, key=lambda x: x[1], reverse=True):
        pct = size / total * 100 if total else 0
        b = bar(size / total if total else 0)
        print(f"  {name:<{name_w}}  {human_size(size):>{size_w}}  {pct:>4.1f}%  {b}")
    print("-" * len(header))
    print(f"  {'Total':<{name_w}}  {human_size(total):>{size_w}}")


def main():
    project_root = Path(__file__).resolve().parent.parent.parent
    build_dir = project_root / "build"

    deep = "--nodeep" not in sys.argv

    wasm_objdump = find_wasm_objdump()
    wasm_file = find_newest_wasm(build_dir)
    build_root = wasm_file.parent.parent.parent  # build/wasm-release

    result = subprocess.run(
        [wasm_objdump, str(wasm_file), "-h"],
        capture_output=True, text=True
    )
    if result.returncode != 0:
        print("Error running wasm-objdump:")
        print(result.stderr)
        sys.exit(1)

    sections = parse_sections(result.stdout)
    if not sections:
        print("Could not parse section output.")
        print(result.stdout)
        sys.exit(1)

    total_sections = sum(s for _, s, _ in sections)
    file_size = wasm_file.stat().st_size

    print(f"\nWASM Binary Analysis")
    print(f"File  : {wasm_file}")
    print(f"Size  : {human_size(file_size)}")
    print(f"Build : {build_root}")

    name_w = max(len(s[0]) for s in sections)
    size_w = max(len(human_size(s[1])) for s in sections)
    header = f"\n  {'Section':<{name_w}}  {'Size':>{size_w}}  {'%':>5}  {'Bar':28}  Count"
    print(header)
    print("-" * len(header))
    for name, size, count in sorted(sections, key=lambda x: x[1], reverse=True):
        pct = size / total_sections * 100
        b = bar(size / total_sections)
        count_str = str(count) if count is not None else ""
        print(f"  {name:<{name_w}}  {human_size(size):>{size_w}}  {pct:>4.1f}%  {b}  {count_str}")
    print("-" * len(header))
    print(f"  {'Total (sections)':<{name_w}}  {human_size(total_sections):>{size_w}}")

    if not deep:
        print()
        return

    lib_data, used_ninja = analyze_objects(build_root, wasm_file, wasm_objdump)

    code_rows = [(lib, secs.get("Code", 0)) for lib, secs in lib_data.items() if secs.get("Code", 0) > 0]
    data_rows = [(lib, secs.get("Data", 0)) for lib, secs in lib_data.items() if secs.get("Data", 0) > 0]

    total_code = sum(s for _, s in code_rows)
    total_data = sum(s for _, s in data_rows)

    note = "pre-DCE upper bound per library; Emscripten eliminates unused functions at link time"
    src = "from build.ninja link graph" if used_ninja else "WARNING: build.ninja not parsed, scanning all .o files"

    print_section_table(f"Code section -- by library ({src})", code_rows, total_code, note)
    print_section_table(f"Data section -- by library ({src})", data_rows, total_data)
    print()


if __name__ == "__main__":
    main()
