#!/usr/bin/env python3
"""Prove a shader refactor is behaviour-preserving against git HEAD.

Written for the 2026-09-13 material/friction dispatcher refactor; the RENAMED / DISPATCHERS
tables at the bottom encode that refactor's intentional changes and need editing for a
different one. Kept because the technique - resolve includes on both sides, extract every
top-level definition, normalise intentional renames, diff bodies - is reusable.

Resolves ///use includes per kernel from git HEAD and the working tree, extracts every
top-level definition, normalises the intentional renames inside each body, and compares.
Dispatcher wrappers (pure indirection) are folded away; functions a kernel never calls may
disappear (mpm_common used to inject everything into every kernel).
"""
import difflib, os, re, subprocess, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SHADERS = "webgpu/compute/shaders"
KERNELS = ["mpm_seed", "mpm_p2g", "mpm_grid_update", "mpm_g2p"]
USE_RE = re.compile(r"^\s*///use\s+(?:([A-Za-z_][A-Za-z0-9_]*)::)?([/\w .-]+?)\s*$")

read_head = lambda rel: (lambda r: r.stdout if r.returncode == 0 else None)(
    subprocess.run(["git", "show", f"HEAD:{rel}"], cwd=ROOT, capture_output=True, text=True))
read_tree = lambda rel: open(os.path.join(ROOT, rel)).read() if os.path.exists(os.path.join(ROOT, rel)) else None


def resolve(name, reader, seen):
    if name in seen:
        return ""
    seen.add(name)
    src = reader(f"{SHADERS}/{name}.wgsl")
    if src is None:
        return ""
    out = []
    for line in src.splitlines(keepends=True):
        m = USE_RE.match(line)
        if m:
            if not m.group(1):
                out.append(resolve(m.group(2), reader, seen))
        elif not line.lstrip().startswith("///"):
            out.append(line)
    return "".join(out)


def strip(src):
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    src = re.sub(r"//[^\n]*", "", src)
    return src


def defs(src):
    """name -> whitespace-normalised body, extracted BEFORE any renaming."""
    text = strip(src)
    out = {}
    for m in re.finditer(r"\b(fn|struct)\s+(\w+)[^{]*\{", text):
        depth, i = 0, m.start()
        while i < len(text):
            if text[i] == "{": depth += 1
            elif text[i] == "}":
                depth -= 1
                if depth == 0: break
            i += 1
        out[m.group(2)] = re.sub(r"\s+", " ", text[m.start():i + 1]).strip()
    for m in re.finditer(r"\bconst\s+(\w+)\s*:[^;]*;", text):
        out[m.group(1)] = re.sub(r"\s+", " ", m.group(0)).strip()
    return out


def norm(body):
    """Intentional renames, applied inside one body."""
    body = body.replace("snow_stress", "stomakhin_stress").replace("apply_plasticity", "stomakhin_plasticity")
    body = body.replace("PlasticState", "PlasticReturn")
    body = re.sub(r"\bjp\b", "plastic_state", body)
    # call-site indirection through the dispatcher
    body = body.replace("material_stress(", "stomakhin_stress(").replace("material_plasticity(", "stomakhin_plasticity(")
    body = body.replace("material_initial_state()", "1.0")
    # step 2: the friction call gained an apply_basal_drag flag; Coulomb ignores it
    body = re.sub(r"resolve_terrain_collision\(([^;]*?), (?:true|false)\)", r"resolve_terrain_collision(\1)", body)
    return body


DISPATCHERS = {"material_stress", "material_plasticity", "material_initial_state",
               "stomakhin_initial_state", "MATERIAL_STOMAKHIN", "FRICTION_COULOMB",
               # step 2: Voellmy is a genuinely new branch, verified separately in test_friction.py
               "voellmy_friction", "FRICTION_VOELLMY"}

RENAMED = {"snow_stress": "stomakhin_stress", "apply_plasticity": "stomakhin_plasticity", "PlasticState": "PlasticReturn"}

failed = False
for k in KERNELS:
    old = {RENAMED.get(n, n): norm(b) for n, b in defs(resolve(k, read_head, set())).items()}
    new = {n: norm(b) for n, b in defs(resolve(k, read_tree, set())).items()}
    for d in DISPATCHERS:
        new.pop(d, None)
    entry = new.get("computeMain", "")

    # friction: old single fn vs new split pair - compare fused text
    if "resolve_terrain_collision" in old and "coulomb_friction" in new:
        fused = new.pop("resolve_terrain_collision") + " " + new.pop("coulomb_friction")
        for stmt in ["let vn = dot(velocity, normal);", "let vt = velocity - normal * vn;",
                     "if vt_len <= -settings.terrain_friction * vn", "vt * (1.0 + settings.terrain_friction * vn / vt_len)",
                     "if vn >= 0.0"]:
            if stmt not in old["resolve_terrain_collision"] or stmt not in fused:
                print(f"[{k}] friction logic differs on: {stmt}"); failed = True
        old.pop("resolve_terrain_collision")

    for name in set(old) - set(new):
        # allowed only if the kernel never referenced it (dead code the old common injected)
        if re.search(rf"\b{re.escape(name)}\b", entry):
            print(f"[{k}] REMOVED but still referenced: {name}"); failed = True
        else:
            print(f"[{k}] dropped unused: {name}")
    for name in set(new) - set(old):
        print(f"[{k}] NEW definition: {name}"); failed = True

    for name in sorted(set(old) & set(new)):
        if old[name] == new[name]:
            continue
        if name == "MpmSettings":
            added = set(re.findall(r"(\w+): (?:u32|f32)", new[name])) - set(re.findall(r"(\w+): (?:u32|f32)", old[name]))
            if added == {"constitutive_model", "basal_friction_model", "voellmy_xi", "_pad_b"} \
                    and new[name].replace(" constitutive_model: u32, basal_friction_model: u32, voellmy_xi: f32, _pad_b: u32,", "") == old[name]:
                print(f"[{k}] MpmSettings: +4 expected fields only")
                continue
        failed = True
        print(f"\n[{k}] BODY CHANGED: {name}")
        for line in difflib.unified_diff(old[name].split(" "), new[name].split(" "), lineterm="", n=3):
            print("   ", line)

print("\nRESULT:", "DIFFERENCES FOUND" if failed else "behaviour-preserving: every retained function body identical modulo renames")
sys.exit(1 if failed else 0)
