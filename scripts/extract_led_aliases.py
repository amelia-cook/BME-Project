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

if aliases_block:
    for line in aliases_block.group(1).splitlines():
        # remove comments and whitespace
        line = line.split("//")[0].strip()
        if not line:
            continue

        # match "name = &ledX;" or "name = ledX;"
        m = re.match(r'(\w+)\s*=\s*&?(led\d+);?', line)
        if m:
            var_name, led_target = m.groups()
            if led_target.startswith("led"):
                leds.append(var_name)

# Limit to 4 real LEDs
leds = leds[:4]

# Fill remaining slots with unique dummy names
for i in range(len(leds) + 1, 5):
    leds.append(f"unused{i}")

# Write output
out = Path(sys.argv[2])
with out.open("w") as f:
    for i, name in enumerate(leds, start=1):
        f.write(f'set(LED{i} {name})\n')
