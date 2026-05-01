# generate_sine_lut.py  — run once, commit the output
import math

SAMPLES     = 800
FREQ_HZ     = 10
INTERVAL_US = 2500
AMPLITUDE = 16000

with open("sine_lut.h", "w") as f:
    f.write("#pragma once\n")
    f.write(f"#define SINE_LUT_LEN {SAMPLES}\n")
    f.write("static const int16_t sine_lut[SINE_LUT_LEN] = {\n    ")
    vals = []
    for i in range(SAMPLES):
        t = i * INTERVAL_US / 1e6
        v = int(AMPLITUDE * math.sin(2 * math.pi * FREQ_HZ * t))
        vals.append(str(v))
    f.write(", ".join(vals))
    f.write("\n};\n")