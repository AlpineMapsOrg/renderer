#!/usr/bin/env python3
import re
import sys
from pathlib import Path
from setup_utils import log, fail

def remove_comments(code):
    code = re.sub(r'//.*?$', '', code, flags=re.MULTILINE)
    code = re.sub(r'/\*.*?\*/', '', code, flags=re.DOTALL)
    return code

def remove_empty_lines(code):
    lines = [line for line in code.split('\n') if line.strip()]
    return '\n'.join(lines)

def minify_shader_directory(input_dir, output_dir):
    input_dir = Path(input_dir)
    output_dir = Path(output_dir)
    shader_files = list(input_dir.rglob('*.wgsl'))
    log(f"Minifying {len(shader_files)} shader files...")
    total_original_size = 0
    total_minified_size = 0
    for shader_file in shader_files:
        try:
            with open(shader_file, 'r', encoding='utf-8') as f:
                original_code = f.read()
        except Exception as e:
            fail(f"Failed to read {shader_file}: {e}")
        original_size = len(original_code)
        total_original_size += original_size
        minified_code = remove_comments(original_code)
        minified_code = remove_empty_lines(minified_code)
        total_minified_size += len(minified_code)
        relative_path = shader_file.relative_to(input_dir)
        output_file = output_dir / relative_path
        try:
            output_file.parent.mkdir(parents=True, exist_ok=True)
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(minified_code)
        except Exception as e:
            fail(f"Failed to write {output_file}: {e}")
    total_reduction = (1 - total_minified_size / total_original_size) * 100 if total_original_size > 0 else 0
    log(f"Total: {total_original_size} -> {total_minified_size} bytes ({total_reduction:.1f}% reduction)")

if __name__ == '__main__':
    if len(sys.argv) != 3:
        fail("Usage: python minify_shaders.py <input_dir> <output_dir>")
    input_dir = Path(sys.argv[1])
    output_dir = Path(sys.argv[2])
    if not input_dir.exists():
        fail(f"Input directory '{input_dir}' does not exist")
    minify_shader_directory(input_dir, output_dir)
