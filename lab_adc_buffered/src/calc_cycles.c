#include <zephyr/kernel.h>

int calc_cycles(int16_t *buffer, int buffer_size) {
    int cycles = 0;
    for (int i = 0; i < buffer_size - 1; i++) {
        if (buffer[i] < 0 && buffer[i+1] > 0) {
            cycles++;
        }
    }
    return cycles;
}