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

# Extract aliases block
aliases_block_match = re.search(
    r'aliases\s*{([^}]*)};',
    text,
    re.DOTALL
)

if not aliases_block_match:
    print("Error: no aliases block found")
    sys.exit(1)

aliases_block = aliases_block_match.group(1)

# Extract alias name + RHS target
alias_pattern = re.compile(
    r'^\s*(\w+)\s*=\s*&(\w+)\s*;',
    re.MULTILINE
)

matches = alias_pattern.findall(aliases_block)

if not matches:
    print("Error: no aliases found")
    sys.exit(1)

led_aliases = []
button_aliases = []

for alias, target in matches:
    target_lower = target.lower()
    if target_lower.startswith("led"):
        led_aliases.append(alias)
    elif target_lower.startswith("button"):
        button_aliases.append(alias)
    else:
        print(f"Warning: skipping unknown target '&{target}'")

# Start overlay
lines = []
lines.append("/ {")
lines.append("    aliases {")

for alias in led_aliases + button_aliases:
    lines.append(f"        {alias} = &sim_{alias};")

lines.append("    };")
lines.append("")

gpio_pin = 10

# gpio-leds
if led_aliases:
    lines.append("    sim_leds {")
    lines.append("        compatible = \"gpio-leds\";")
    lines.append("")

    for alias in led_aliases:
        lines.append(f"        sim_{alias}: led_{gpio_pin} {{")
        lines.append(f"            gpios = <&gpio0 {gpio_pin} GPIO_ACTIVE_HIGH>;")
        lines.append(f"            label = \"SIM_{alias.upper()}\";")
        lines.append("        };")
        lines.append("")
        gpio_pin += 1

    lines.append("    };")
    lines.append("")

# gpio-keys
if button_aliases:
    lines.append("    sim_keys {")
    lines.append("        compatible = \"gpio-keys\";")
    lines.append("")

    for alias in button_aliases:
        lines.append(f"        sim_{alias}: key_{gpio_pin} {{")
        lines.append(f"            gpios = <&gpio0 {gpio_pin} GPIO_ACTIVE_HIGH>;")
        lines.append(f"            label = \"SIM_{alias.upper()}\";")
        lines.append("            zephyr,code = <1>; /* KEY_ESC */")
        lines.append("        };")
        lines.append("")
        gpio_pin += 1

    lines.append("    };")
    lines.append("")

lines.append("};")
lines.append("")
lines.append("&gpio0 {")
lines.append("    status = \"okay\";")
lines.append("};")

output_overlay.write_text("\n".join(lines))

print("Generated native_sim overlay")
print(f"  LEDs: {', '.join(led_aliases) or 'none'}")
print(f"  Buttons: {', '.join(button_aliases) or 'none'}")
