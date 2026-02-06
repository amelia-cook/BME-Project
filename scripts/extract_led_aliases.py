#!/usr/bin/env python3
import re
import sys
from pathlib import Path

overlay = Path(sys.argv[1]).read_text()

aliases_block = re.search(
    r'aliases\s*\{([^}]*)\};',
    overlay,
    re.DOTALL
)

leds = []

if aliases_block:
    for line in aliases_block.group(1).splitlines():
        m = re.match(r'\s*(\w+)\s*=', line)
        if m and m.group(1).startswith("led"):
            leds.append(m.group(1))

# Limit to 4 real LEDs
leds = leds[:4]

# Fill remaining slots with unique dummy names
for i in range(len(leds) + 1, 5):
    leds.append(f"unused{i}")

out = Path(sys.argv[2])
with out.open("w") as f:
    for i, name in enumerate(leds, start=1):
        f.write(f'set(LED{i} {name})\n')
