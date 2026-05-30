#!/usr/bin/env python3
"""Fix yaml_to_c.py: change .c->.cpp and fix seat_belt nested brace bug."""
with open('tools/yaml_to_c.py', 'r') as f:
    content = f.read()

# Fix 1: output extensions
for old, new in [
    ('"can_field_table.c"', '"can_field_table.cpp"'),
    ('"alarm_rule_table.c"', '"alarm_rule_table.cpp"'),
    ('"seat_belt_table.c"', '"seat_belt_table.cpp"'),
    ('"indicator_table.c"', '"indicator_table.cpp"'),
    ('"signal_table.c"', '"signal_table.cpp"'),
]:
    content = content.replace(old, new)
    print(f"  {old} -> {new}")

# Fix 2: seat_belt_table config_str - detect buggy lines and replace block
lines = content.split('\n')
new_lines = []
skip_count = 0
for i, line in enumerate(lines):
    if skip_count > 0:
        skip_count -= 1
        continue
    # Detect the start of buggy config_str block
    if "config_str = (" in line and i+2 < len(lines) and "{{{" in lines[i+1]:
        # Replace with fixed version
        new_lines.append("    speed = trigger.get('speed_threshold', 5.0)")
        new_lines.append("    occupied = ('true' if trigger.get('require_seat_occupied', True) else 'false')")
        new_lines.append("    config_str = (")
        new_lines.append('        f"    {speed}f, "')
        new_lines.append('        f"{occupied}, "')
        new_lines.append("        f'        f\"\\\\\"{msgs.get('single_zh', '{position}请系安全带')}\\\\\", '")
        new_lines.append("        f'        f\"\\\\\"{msgs.get('multiple_zh', '{positions}请系安全带')}\\\\\", '")
        new_lines.append("        f'        f\"\\\\\"{msgs.get('single_en', '{position} please buckle up')}\\\\\", '")
        new_lines.append("        f'        f\"\\\\\"{msgs.get('multiple_en', '{positions} please buckle up')}\\\\\\""')
        new_lines.append("    )")
        # Skip the buggy f-string lines (lines 310-315 approximately)
        skip_count = 5
        print("  Fixed seat_belt_table config_str block")
        continue
    new_lines.append(line)

content = '\n'.join(new_lines)

import ast
try:
    ast.parse(content)
    print("Syntax OK")
except SyntaxError as e:
    print(f"Syntax error at line {e.lineno}: {e.msg}")
    import sys; sys.exit(1)

with open('tools/yaml_to_c.py', 'w') as f:
    f.write(content)
print("Saved!")
