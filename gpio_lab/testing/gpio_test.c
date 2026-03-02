#include "gpio_test.h"

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

/**
 * thread for student main code
 * - thread declaration
 * - thread teardown (after tests)
 * - thread start (before tests)
 * 
 * helper functions
 * - trigger button ISRs
 * - check blink frequency
 * 
 * fixtures
 * - before
 * - after
 * 
 * tests:
 * - 
 */

/* ------------------------------------------------------------------ */
/*  Thread boilerplate                                                */
/* ------------------------------------------------------------------ */

K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);
static struct k_thread student_main_thread;
static k_tid_t         student_main_tid;
static volatile bool   main_running = false;

static void student_main_entry(void *, void *, void *)
{
    main_running = true;
    student_main();
    main_running = false;
}

/** Kill the background thread cleanly. */
static void stop_main(void)
{
    if (main_running) {
        k_thread_abort(student_main_tid);
        k_msleep(20);
        main_running = false;
    }
}

/**
 * Start student_main() in a background thread.
 *
 * @param settle_ms  How long to wait after spawning before returning.
 *                   150 ms is enough for INIT to run and reach BLINKING_RUN.
 */
static void start_main(int settle_ms)
{
    /* Spawn student thread */
    student_main_tid = k_thread_create(
        &student_main_thread,
        student_main_stack,
        K_THREAD_STACK_SIZEOF(student_main_stack),
        student_main_entry,
        NULL, NULL, NULL,
        STUDENT_MAIN_PRIORITY, 0, K_NO_WAIT);

    k_msleep(settle_ms);
}

/* ------------------------------------------------------------------ */
/*  Helpers                                                             */
/* ------------------------------------------------------------------ */

/** Simulate a button press and wait for the main loop to react. */
static void press_button(volatile bool *event_flag, int settle_ms)
{
    *event_flag = true;
    k_msleep(settle_ms);
}

/** Read logical LED state (true = illuminated) */
static inline bool led_is_on(const struct gpio_dt_spec *led)
{
    return gpio_pin_get_dt(led) > 0;
}

/**
 * Assert that an LED toggles at approximately the expected frequency.
 *
 * Algorithm:
 *  1. Sync: wait for the very next edge so counting starts clean.
 *  2. Count every subsequent edge over `window_ms`.
 *  3. Hz = (toggles / 2) / (window_ms / 1000) = toggles * 500 / window_ms
 *
 * Sampling every 5 ms gives ±5 ms timing resolution, well within the
 * ±1 Hz tolerance used everywhere in this file.
 *
 * The old implementation sampled every 10 ms and compared against the
 * *initial* state without updating it between samples, which caused it to
 * miss edges and report ~double the true frequency.
 */
static void assert_led_blink_freq(const struct gpio_dt_spec *led,
                                  int window_ms,
                                  int expected_hz,
                                  int tolerance_hz)
{
    if (!device_is_ready(led->port)) {
        zassert_unreachable("LED device not ready");
        return;
    }

    /* Step 1 – sync to next edge */
    bool initial = led_is_on(led);
    int64_t sync_limit_ms = (2 * 1000) / expected_hz;
    int64_t sync_deadline = k_uptime_get() + sync_limit_ms;

    while (led_is_on(led) == initial) {
        k_msleep(5);
        if (k_uptime_get() > sync_deadline) {
            zassert_unreachable(
                "LED never toggled while syncing (expected %d Hz)",
                expected_hz);
            return;
        }
    }

    /* Step 2 – count edges */
    bool last = led_is_on(led);
    int toggles = 0;
    int64_t end = k_uptime_get() + window_ms;

    while (k_uptime_get() < end) {
        k_msleep(5);

        bool now = led_is_on(led);
        if (now != last) {
            toggles++;
            last = now;
        }
    }

    /* Step 3 – compute frequency */
    int measured_hz = (toggles * 500) / window_ms;

    zassert_within(measured_hz, expected_hz, tolerance_hz,
        "Expected ~%d Hz but measured ~%d Hz (%d toggles in %d ms)",
        expected_hz, measured_hz, toggles, window_ms);
}

/* ------------------------------------------------------------------ */
/*  Fixture                                                             */
/* ------------------------------------------------------------------ */

static void before(void *)
{
    stop_main();  /* abort any leftover thread from the previous test */
}

static void after(void *)
{
    stop_main();
}

/* ================================================================== */
/*  TESTS                                                             */
/* ================================================================== */

ZTEST(state_machine_tests, test_03_blinking_run_default_freq)
{
    start_main(150);
    
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1);
    assert_led_blink_freq(&iv_pump_led, 2000, 2, 1);
    assert_led_blink_freq(&buzzer_led, 2000, 2, 1);
}












/* ================================================================== */
/*  Register suite                                                    */
/* ================================================================== */
ZTEST_SUITE(state_machine_tests, NULL, NULL, before, after, NULL);
