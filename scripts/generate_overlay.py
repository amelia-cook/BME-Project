#!/usr/bin/env python3
import re
import sys
from pathlib import Path

if len(sys.argv) != 3:
    print("Usage: generate_native_sim_overlay.py <student.overlay> <output.overlay>")
    sys.exit(1)

student_overlay_path = Path(sys.argv[1])
output_overlay_path = Path(sys.argv[2])

if not student_overlay_path.exists():
    print(f"Error: {student_overlay_path} does not exist")
    sys.exit(1)

overlay_text = student_overlay_path.read_text()

# Extract aliases block
aliases_block = re.search(r"aliases\s*{([^}]*)};", overlay_text, re.DOTALL)
if not aliases_block:
    print("Warning: No aliases block found in student overlay")
    student_aliases = []
else:
    aliases_text = aliases_block.group(1)
    # Match alias names like: ledtest = &led0;
    student_aliases = re.findall(r"(\w+)\s*=\s*&\w+\s*;", aliases_text)

if not student_aliases:
    print("No aliases found. Output overlay will be empty except enabling gpio0.")

# Generate the native_sim overlay
lines = []
lines.append("/ {")
lines.append("    aliases {")
for alias in student_aliases:
    lines.append(f"        {alias} = &sim_{alias};")
lines.append("    };")
lines.append("")

gpio_pin = 10
for alias in student_aliases:
    if "led" in alias.lower():
        lines.append(f"    sim_{alias}: gpio_led_{alias} {{")
        lines.append('        compatible = "gpio-leds";')
        lines.append(f"        {alias} {{")
        lines.append(f"            gpios = <&gpio0 {gpio_pin} GPIO_ACTIVE_HIGH>;")
        lines.append("        };")
        lines.append("    };")
    else:
        lines.append(f"    sim_{alias}: gpio_key_{alias} {{")
        lines.append('        compatible = "gpio-keys";')
        lines.append(f"        {alias} {{")
        lines.append(f"            gpios = <&gpio0 {gpio_pin} GPIO_ACTIVE_LOW>;")
        lines.append("        };")
        lines.append("    };")
    gpio_pin += 1

lines.append("};")
lines.append("&gpio0 { status = \"okay\"; };")

# Write the overlay file
output_overlay_path.write_text("\n".join(lines))
print(f"Generated native_sim overlay at {output_overlay_path}")
