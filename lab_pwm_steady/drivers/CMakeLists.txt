# Declare a Zephyr library for this driver
zephyr_library()

# Add your driver source file(s) here
zephyr_library_sources(
    pwm_native_sim.c
)