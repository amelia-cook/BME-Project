/**
 * @file test_led_blink.h
 * @brief Exposes symbols from main.c for the LED blink test suite.
 *
 * This header is the mirror of test_button_callback.h from the button tests.
 * It gives the test file access to:
 *   - LED_STATE    : software mirror of the physical pin
 *   - led_test     : the gpio_dt_spec so tests can read the real pin
 *   - Timing macros that must match the values in main.c
 *
 * IMPORTANT: Do NOT modify this file unless you also update main.c.
 */

#ifndef TEST_LED_BLINK_H
#define TEST_LED_BLINK_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* ── Software state mirror (defined in main.c) ───────────────────────────── */
extern int LED_STATE;

/* ── Physical GPIO spec (defined in main.c) ──────────────────────────────── */
extern const struct gpio_dt_spec led_test;

/* ── LED state macros ────────────────────────────────────────────────────── */
#define LED_ON  1
#define LED_OFF 0

/* ── Timing constants — MUST match main.c ───────────────────────────────── */
#define BLINK_INTERVAL_MS  500   /* expected half-period                     */
#define BLINK_DURATION_MS  5000  /* expected total blink window              */

#define BLINK_PERIOD_MS        (BLINK_INTERVAL_MS * 2)
#define EXPECTED_TOGGLES       (BLINK_DURATION_MS / BLINK_INTERVAL_MS)
#define TIMING_TOLERANCE_MS    (BLINK_INTERVAL_MS / 10)  /* 10 % */

#endif /* TEST_LED_BLINK_H */