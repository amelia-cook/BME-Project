#!/usr/bin/env python3

import re
import sys
from pathlib import Path

if len(sys.argv) != 3:
    print("Usage: generate_overlay.py <student_overlay> <output_overlay>")
    sys.exit(1)

student_overlay = Path(sys.argv[1])
output_overlay = Path(sys.argv[2])

text = student_overlay.read_text()

# Extract alias names ONLY (ignore what they point to)
alias_pattern = re.compile(r'(\w+)\s*=\s*&\w+\s*;')
aliases = alias_pattern.findall(text)

if not aliases:
    print("Error: no aliases found in student overlay")
    sys.exit(1)

# Start generating native_sim overlay
lines = []
lines.append("/ {")
lines.append("    aliases {")

for alias in aliases:
    lines.append(f"        {alias} = &sim_{alias};")

lines.append("    };")
lines.append("")
lines.append("    sim_leds {")
lines.append("        compatible = \"gpio-leds\";")
lines.append("")

# Generate one LED per alias
gpio_pin = 10
for alias in aliases:
    lines.append(f"        sim_{alias}: led_{gpio_pin} {{")
    lines.append(f"            gpios = <&gpio0 {gpio_pin} GPIO_ACTIVE_HIGH>;")
    lines.append(f"            label = \"SIM_{alias.upper()}\";")
    lines.append("        };")
    lines.append("")
    gpio_pin += 1

lines.append("    };")
lines.append("};")
lines.append("")
lines.append("&gpio0 {")
lines.append("    status = \"okay\";")
lines.append("};")

output_overlay.write_text("\n".join(lines))

print(f"Generated native_sim overlay with aliases: {', '.join(aliases)}")
