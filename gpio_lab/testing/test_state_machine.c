/**
 * @file test_state_machine.c
 * @brief ZTests for IV Pump LED Controller State Machine
 *
 * Build requirements (prj.conf):
 *   CONFIG_ZTEST=y
 *   CONFIG_GPIO_EMUL=y
 *   CONFIG_LOG=y
 *   CONFIG_LOG_BACKEND_UART=y
 *
 * CMakeLists.txt must rename student main() to student_main():
 *   target_compile_definitions(app PRIVATE "main=student_main")
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include "test_state_machine.h"

/* ------------------------------------------------------------------ */
/*  Thread boilerplate                                                  */
/* ------------------------------------------------------------------ */

K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);
static struct k_thread student_main_thread;
static k_tid_t          student_main_tid;
static volatile bool    main_running = false;

static void student_main_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
    main_running = true;
    student_main();
    main_running = false;
}

/** Kill the background thread cleanly. */
static void stop_main(void)
{
    if (main_running) {
        k_thread_abort(student_main_tid);
        k_msleep(50);  /* was 20 — give scheduler more time to fully clean up */
        main_running = false;
    }
}

/**
 * Clear all shared state, then start student_main() in a background thread.
 *
 * Event flags are cleared HERE so that stale presses from any previous test
 * cannot bleed into the new one regardless of what the test body set up before
 * calling us.  The before() fixture also clears them as a belt-and-suspenders
 * measure, but clearing them as late as possible (right before spawn) is the
 * most reliable guard.
 *
 * @param settle_ms  How long to wait after spawning before returning.
 *                   150 ms is enough for INIT to run and reach BLINKING_RUN.
 */
static void start_main(int settle_ms)
{
    /* Extra yield after stop_main() to let the scheduler fully drain
     * the aborted thread before we touch shared globals */
    k_msleep(50);

    /* NOW clear flags — after any residual scheduling is done */
    sleep_button_event = false;
    up_button_event    = false;
    down_button_event  = false;
    reset_button_event = false;

    heartbeat_led_status.toggle_time = 0;
    heartbeat_led_status.illuminated = true;
    iv_pump_led_status.toggle_time   = 0;
    iv_pump_led_status.illuminated   = true;

    student_main_tid = k_thread_create(...);
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
static void assert_blink_freq(struct led_status *status,
                               int window_ms,
                               int expected_hz,
                               int tolerance_hz)
{
    /* Step 1 – sync to the next edge.
     * Allow up to 2 full cycles at the expected frequency before giving up. */
    bool    initial       = status->illuminated;
    int64_t sync_limit_ms = (2 * 1000) / expected_hz;
    int64_t sync_deadline = k_uptime_get() + sync_limit_ms;

    while (status->illuminated == initial) {
        k_msleep(5);
        if (k_uptime_get() > sync_deadline) {
            zassert_unreachable(
                "LED never toggled while syncing (expected %d Hz)", expected_hz);
            return;
        }
    }

    /* Step 2 – count edges over the observation window */
    bool    last    = status->illuminated;
    int     toggles = 0;
    int64_t end     = k_uptime_get() + window_ms;

    while (k_uptime_get() < end) {
        k_msleep(5);
        if (status->illuminated != last) {
            toggles++;
            last = status->illuminated;
        }
    }

    /* Step 3 – compute and assert Hz */
    int measured_hz = (toggles * 500) / window_ms;

    zassert_within(measured_hz, expected_hz, tolerance_hz,
        "Expected ~%d Hz but measured ~%d Hz (%d toggles in %d ms)",
        expected_hz, measured_hz, toggles, window_ms);
}

/* ------------------------------------------------------------------ */
/*  Fixture                                                             */
/* ------------------------------------------------------------------ */

static void before(void *f)
{
    ARG_UNUSED(f);
    stop_main();  /* abort any leftover thread from the previous test */

    /* Reset state machine globals to a known baseline */
    state          = INIT;
    next_state     = INIT;
    action_led_hz  = LED_BLINK_FREQ_HZ;

    /* Belt-and-suspenders clear of event flags (also done in start_main) */
    sleep_button_event = false;
    up_button_event    = false;
    down_button_event  = false;
    reset_button_event = false;
}

static void after(void *f)
{
    ARG_UNUSED(f);
    stop_main();
}

/* ================================================================== */
/*  TEST 1 – INIT: GPIO and LED initialisation                         */
/* ================================================================== */
ZTEST(state_machine_tests, test_01_init_gpio_ready)
{
    start_main(150);

    zassert_equal(state, BLINKING_RUN,
        "Expected state == BLINKING_RUN after INIT, got %d", state);
    zassert_true(main_running,
        "student_main() should still be running (not returned an error)");
}

/* ================================================================== */
/*  TEST 2 – INIT → BLINKING_RUN transition (no BLINKING_ENTRY)       */
/* ================================================================== */
ZTEST(state_machine_tests, test_02_init_to_blinking_run)
{
    start_main(150);

    zassert_equal(state, BLINKING_RUN,
        "INIT should transition directly to BLINKING_RUN, got %d", state);

    /* Heartbeat ticking confirms BLINKING_RUN is active */
    bool illuminated_before = heartbeat_led_status.illuminated;
    k_msleep(600);
    zassert_not_equal(heartbeat_led_status.illuminated, illuminated_before,
        "Heartbeat should have toggled, proving BLINKING_RUN is active");
}

/* ================================================================== */
/*  TEST 3 – BLINKING_RUN: 2 Hz action LEDs, 1 Hz heartbeat           */
/* ================================================================== */
ZTEST(state_machine_tests, test_03_blinking_run_default_freq)
{
    start_main(150);

    zassert_equal(action_led_hz, LED_BLINK_FREQ_HZ,
        "Default frequency should be %d Hz", LED_BLINK_FREQ_HZ);
    zassert_equal(state, BLINKING_RUN,
        "State should remain BLINKING_RUN with no button presses");

    assert_blink_freq(&heartbeat_led_status, 2000, 1, 1);
    assert_blink_freq(&iv_pump_led_status,   2000, 2, 1);
}

/* ================================================================== */
/*  TEST 4 – BLINKING_ENTRY restores LEDs after returning from RESET   */
/* ================================================================== */
ZTEST(state_machine_tests, test_04_blinking_entry_restores_state)
{
    /* Force a path through BLINKING_ENTRY by pre-setting state to RESET */
    state      = RESET;
    next_state = RESET;
    start_main(200);

    k_msleep(100);
    zassert_equal(state, BLINKING_RUN,
        "Should reach BLINKING_RUN via BLINKING_ENTRY, got %d", state);
    zassert_equal(action_led_hz, LED_BLINK_FREQ_HZ,
        "action_led_hz should be %d after RESET", LED_BLINK_FREQ_HZ);
}

/* ================================================================== */
/*  TEST 5 – Freq Down: 2 Hz → 1 Hz                                   */
/* ================================================================== */
ZTEST(state_machine_tests, test_05_freq_down_2_to_1)
{
    start_main(150);

    zassert_equal(action_led_hz, 2, "Starting frequency should be 2 Hz");

    press_button(&down_button_event, 50);

    zassert_equal(action_led_hz, 1,
        "action_led_hz should be 1 after one freq_down press, got %d", action_led_hz);
    zassert_equal(state, BLINKING_RUN,
        "State should remain BLINKING_RUN at 1 Hz");

    assert_blink_freq(&iv_pump_led_status,   2000, 1, 1);
    assert_blink_freq(&heartbeat_led_status, 2000, 1, 1);
}

/* ================================================================== */
/*  TEST 6 – Freq Down below MIN triggers ERROR                        */
/* ================================================================== */
ZTEST(state_machine_tests, test_06_freq_down_below_min_triggers_error)
{
    action_led_hz = 1;  /* already at minimum */
    start_main(150);

    press_button(&down_button_event, 100);

    zassert_equal(state, ERROR,
        "State should be ERROR when action_led_hz < 1, got %d", state);

    bool hb_before = heartbeat_led_status.illuminated;
    k_msleep(600);
    zassert_not_equal(heartbeat_led_status.illuminated, hb_before,
        "Heartbeat LED must keep toggling in ERROR state");
}

/* ================================================================== */
/*  TEST 7 – ERROR state: Reset button → BLINKING_RUN                 */
/* ================================================================== */
ZTEST(state_machine_tests, test_07_error_reset_button)
{
    state         = ERROR;
    next_state    = ERROR;
    action_led_hz = 0;
    start_main(150);

    press_button(&reset_button_event, 150);

    zassert_equal(action_led_hz, LED_BLINK_FREQ_HZ,
        "action_led_hz should be restored to %d by RESET", LED_BLINK_FREQ_HZ);
    zassert_equal(state, BLINKING_RUN,
        "Should reach BLINKING_RUN after reset from ERROR, got %d", state);
}

/* ================================================================== */
/*  TEST 8 – Freq Up: 2 Hz → 3 Hz                                     */
/* ================================================================== */
ZTEST(state_machine_tests, test_08_freq_up_2_to_3)
{
    start_main(150);

    press_button(&up_button_event, 50);

    zassert_equal(action_led_hz, 3,
        "action_led_hz should be 3 after one freq_up press, got %d", action_led_hz);
    zassert_equal(state, BLINKING_RUN, "Should remain BLINKING_RUN at 3 Hz");

    assert_blink_freq(&iv_pump_led_status,   2000, 3, 1);
    assert_blink_freq(&heartbeat_led_status, 2000, 1, 1);
}

/* ================================================================== */
/*  TEST 9 – Freq Up: 3 Hz → 4 Hz                                     */
/* ================================================================== */
ZTEST(state_machine_tests, test_09_freq_up_3_to_4)
{
    /* Skip INIT so our pre-set action_led_hz is not overwritten */
    action_led_hz = 3;
    state         = BLINKING_RUN;
    next_state    = BLINKING_RUN;
    start_main(150);

    press_button(&up_button_event, 50);

    zassert_equal(action_led_hz, 4,
        "action_led_hz should be 4, got %d", action_led_hz);
    zassert_equal(state, BLINKING_RUN, "Should remain BLINKING_RUN at 4 Hz");

    assert_blink_freq(&iv_pump_led_status,   2000, 4, 1);
    assert_blink_freq(&heartbeat_led_status, 2000, 1, 1);
}

/* ================================================================== */
/*  TEST 10 – Freq Up: 4 Hz → 5 Hz (max valid)                        */
/* ================================================================== */
ZTEST(state_machine_tests, test_10_freq_up_4_to_5)
{
    action_led_hz = 4;
    state         = BLINKING_RUN;
    next_state    = BLINKING_RUN;
    start_main(150);

    press_button(&up_button_event, 50);

    zassert_equal(action_led_hz, 5,
        "action_led_hz should be 5 (max), got %d", action_led_hz);
    zassert_equal(state, BLINKING_RUN,
        "Should remain BLINKING_RUN at max freq 5 Hz");

    assert_blink_freq(&iv_pump_led_status,   2000, 5, 1);
    assert_blink_freq(&heartbeat_led_status, 2000, 1, 1);
}

/* ================================================================== */
/*  TEST 11 – Freq Up above MAX triggers ERROR                         */
/* ================================================================== */
ZTEST(state_machine_tests, test_11_freq_up_above_max_triggers_error)
{
    action_led_hz = 5;  /* already at maximum */
    state         = BLINKING_RUN;
    next_state    = BLINKING_RUN;
    start_main(150);

    press_button(&up_button_event, 100);

    zassert_equal(state, ERROR,
        "State should be ERROR when action_led_hz > 5, got %d", state);

    bool hb_before = heartbeat_led_status.illuminated;
    k_msleep(600);
    zassert_not_equal(heartbeat_led_status.illuminated, hb_before,
        "Heartbeat LED must keep toggling in ERROR state");
}

/* ================================================================== */
/*  TEST 12 – ERROR (upper overflow): Reset restores default freq      */
/* ================================================================== */
ZTEST(state_machine_tests, test_12_error_upper_overflow_reset)
{
    state         = ERROR;
    next_state    = ERROR;
    action_led_hz = 6;
    start_main(150);

    press_button(&reset_button_event, 150);

    zassert_equal(action_led_hz, LED_BLINK_FREQ_HZ,
        "action_led_hz should reset to %d, got %d", LED_BLINK_FREQ_HZ, action_led_hz);
    zassert_equal(state, BLINKING_RUN,
        "Should reach BLINKING_RUN after reset, got %d", state);
}

/* ================================================================== */
/*  TEST 13 – BLINKING_RUN: Sleep button → SLEEP                      */
/* ================================================================== */
ZTEST(state_machine_tests, test_13_sleep_button_enters_sleep)
{
    start_main(150);
    zassert_equal(state, BLINKING_RUN, "Precondition: must be in BLINKING_RUN");

    press_button(&sleep_button_event, 100);

    zassert_equal(state, SLEEP,
        "State should be SLEEP after sleep button press, got %d", state);
}

/* ================================================================== */
/*  TEST 14 – SLEEP state: LEDs off, heartbeat on, freq preserved     */
/* ================================================================== */
ZTEST(state_machine_tests, test_14_sleep_state_behavior)
{
    int saved_hz = 3;
    action_led_hz = saved_hz;
    start_main(150);

    /* Extra settle so SLEEP case fully executes before we start observing */
    press_button(&sleep_button_event, 150);
    zassert_equal(state, SLEEP, "Precondition: must be in SLEEP");
    k_msleep(100);

    /* Frequency must be preserved */
    zassert_equal(action_led_hz, saved_hz,
        "SLEEP should preserve action_led_hz (%d), got %d", saved_hz, action_led_hz);

    /* iv_pump LED must NOT toggle while asleep */
    bool iv_before = iv_pump_led_status.illuminated;
    k_msleep(700);
    zassert_equal(iv_pump_led_status.illuminated, iv_before,
        "iv_pump_led should NOT toggle in SLEEP state");

    /* Heartbeat MUST keep toggling */
    bool hb_before = heartbeat_led_status.illuminated;
    k_msleep(600);
    zassert_not_equal(heartbeat_led_status.illuminated, hb_before,
        "Heartbeat LED should keep toggling in SLEEP state");
}

/* ================================================================== */
/*  TEST 15 – SLEEP: Sleep button again → BLINKING_RUN                */
/* ================================================================== */
ZTEST(state_machine_tests, test_15_sleep_button_wakes_up)
{
    start_main(150);

    press_button(&sleep_button_event, 100);
    zassert_equal(state, SLEEP, "Precondition: must be in SLEEP");

    press_button(&sleep_button_event, 200);

    zassert_equal(state, BLINKING_RUN,
        "Should wake to BLINKING_RUN via BLINKING_ENTRY, got %d", state);

    /* iv_pump must resume toggling after wake */
    bool iv_before = iv_pump_led_status.illuminated;
    k_msleep(700);
    zassert_not_equal(iv_pump_led_status.illuminated, iv_before,
        "iv_pump_led should resume toggling after waking from SLEEP");
}

/* ================================================================== */
/*  TEST 16 – SLEEP: Reset button → RESET → BLINKING_RUN              */
/* ================================================================== */
ZTEST(state_machine_tests, test_16_sleep_then_reset_button)
{
    action_led_hz = 4;
    start_main(150);

    press_button(&sleep_button_event, 100);
    zassert_equal(state, SLEEP, "Precondition: must be in SLEEP");

    press_button(&reset_button_event, 200);

    zassert_equal(action_led_hz, LED_BLINK_FREQ_HZ,
        "RESET should restore frequency to %d, got %d",
        LED_BLINK_FREQ_HZ, action_led_hz);
    zassert_equal(state, BLINKING_RUN,
        "Should reach BLINKING_RUN after reset from SLEEP, got %d", state);
}

/* ================================================================== */
/*  Register suite                                                      */
/* ================================================================== */
ZTEST_SUITE(state_machine_tests, NULL, NULL, before, after, NULL);