#!/usr/bin/env python3
import re
import sys
from pathlib import Path

overlay = Path(sys.argv[1]).read_text()

# Grab everything inside aliases { ... };
aliases_block = re.search(
    r'aliases\s*\{(.*?)\};',
    overlay,
    re.DOTALL
)

leds = []
buttons = []

if aliases_block:
    for line in aliases_block.group(1).splitlines():
        # remove comments and whitespace
        line = line.split("//")[0].strip()
        if not line:
            continue

        # match "name = &ledX;" or "name = ledX;"
        m1 = re.match(r'(\w+)\s*=\s*&?(led\d+);?', line)
        m2 = re.match(r'(\w+)\s*=\s*&?(button\d+);?', line)
        if m1:
            var_name, target = m1.groups()
            if target.startswith("led"):
                leds.append(var_name)
        if m2:
            var_name, target = m2.groups()
            if target.startswith("button"):
                buttons.append(var_name)

# Limit to 4 real LEDs and buttons
leds = leds[:4]
buttons = buttons[:4]

# Fill remaining slots with unique dummy names
for i in range(len(leds) + 1, 5):
    leds.append(f"unused{i}")
for i in range(len(buttons) + 1, 5):
    buttons.append(f"unused{i}")

# Write output
out = Path(sys.argv[2])
with out.open("w") as f:
    for i, name in enumerate(leds, start=1):
        f.write(f'set(LED{i} {name})\n')
    for i, name in enumerate(buttons, start=1):
        f.write(f'set(BUTTON{i} {name})\n')
