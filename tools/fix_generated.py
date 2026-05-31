#!/usr/bin/env python3
"""Post-process generated C/C++ files: fix template syntax issues for C++ compatibility."""
import sys
import os
import re

def fix_file(path):
    with open(path, 'r') as f:
        content = f.read()

    original = content

    # Fix 1: {{ in C code -> {
    content = content.replace('{{', '{')
    content = content.replace('}}', '}')

    # Fix 2: integer float suffixes like 420f -> 420.0f (C++ needs 420.0f)
    # Use (?<![\d.]) to avoid matching digits that are part of an existing
    # decimal number like 0.001f (which would become 0.001.0f otherwise).
    content = re.sub(r'(?<![\d.])\d+f\b', lambda m: m.group(0).rstrip('f') + '.0f', content)

    # Fix 3: ACTION_ACTION_ -> ACTION_ (double prefix bug from yaml template)
    content = content.replace('ACTION_ACTION_', 'ACTION_')

    # Fix 4: duplicate dots like 2..0f -> 2.0f
    content = re.sub(r'(\d+)\.\.0f', r'\1.0f', content)

    # Fix 5: "typedef struct { ... } NAME;" -> "struct NAME { ... };"
    # for .h files only (C++ compatibility with forward declarations)
    if path.endswith('.h'):
        content = re.sub(
            r'typedef struct \{\n(.*?)\}\s*(\w+);',
            r'struct \2 {\n\1};',
            content,
            flags=re.DOTALL
        )

    if content != original:
        with open(path, 'w') as f:
            f.write(content)
        print(f"  fixed: {path}")
    else:
        print(f"  ok:    {path}")

def main():
    if len(sys.argv) < 2:
        print("Usage: fix_generated.py <generated_dir>")
        sys.exit(1)

    gen_dir = sys.argv[1]
    print(f"Post-processing generated files in {gen_dir}...")

    for fname in os.listdir(gen_dir):
        if fname.endswith('.h') or fname.endswith('.c') or fname.endswith('.cpp'):
            fix_file(os.path.join(gen_dir, fname))

    print("Done.")

if __name__ == '__main__':
    main()
