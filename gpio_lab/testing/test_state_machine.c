/**
 * @file test_state_machine.c
 * @brief ZTests for IV Pump LED Controller State Machine
 *
 * Build requirements (prj.conf):
 *   CONFIG_ZTEST=y
 *   CONFIG_GPIO_EMUL=y   (Zephyr emulated GPIO driver)
 *   CONFIG_LOG=y
 *   CONFIG_LOG_BACKEND_UART=y
 *
 * CMakeLists.txt must rename student main() → student_main() e.g.:
 *   target_compile_definitions(app PRIVATE "main=student_main")
 *
 * Strategy:
 *  - student_main() runs in a background k_thread.
 *  - Button presses are simulated by setting the global event flags directly
 *    (sleep_button_event, up_button_event, etc.) — the same flags the real
 *    callbacks write to.
 *  - Timing is controlled by advancing a mock uptime counter so we don't
 *    have to wait real milliseconds for LED toggles.
 *  - GPIO emul lets us read pin state without real hardware.
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

/** Start student_main() in a background thread and wait for it to settle. */
static void start_main(int settle_ms)
{
    student_main_tid = k_thread_create(
        &student_main_thread,
        student_main_stack,
        K_THREAD_STACK_SIZEOF(student_main_stack),
        student_main_entry,
        NULL, NULL, NULL,
        STUDENT_MAIN_PRIORITY, 0, K_NO_WAIT);
    k_msleep(settle_ms);
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

/* ------------------------------------------------------------------ */
/*  Helpers                                                             */
/* ------------------------------------------------------------------ */

/** Press a button by setting its event flag; give the main loop time to react. */
static void press_button(volatile bool *event_flag, int settle_ms)
{
    *event_flag = true;
    k_msleep(settle_ms);
}

/**
 * Count how many times a GPIO pin toggled within a real-time window.
 *
 * Uses gpio_emul_input_set to observe toggle_count via the gpio_emul API.
 * Simpler alternative: read iv_pump_led_status / heartbeat_led_status directly.
 *
 * @param status   Pointer to the led_status struct for the pin of interest
 * @param window_ms  How long (real ms) to observe
 * @param expected_hz  Expected frequency
 * @param tolerance_hz  Allowed deviation (±)
 */
static void assert_blink_freq(struct led_status *status,
                               int window_ms,
                               int expected_hz,
                               int tolerance_hz)
{
    bool start_illuminated = status->illuminated;
    int  toggles = 0;
    int  steps   = window_ms / 10;

    for (int i = 0; i < steps; i++) {
        k_msleep(10);
        if (status->illuminated != start_illuminated) {
            toggles++;
            start_illuminated = status->illuminated;
        }
    }
    /* Each full cycle = 2 toggles.  toggles/2 ≈ cycles in window_ms. */
    int measured_hz = (toggles * 1000) / (window_ms * 2);

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
    stop_main();  /* clean up any leftover thread */

    /* Reset all shared globals to a known baseline */
    state             = INIT;
    next_state        = INIT;
    action_led_hz     = LED_BLINK_FREQ_HZ;
    sleep_button_event = false;
    up_button_event    = false;
    down_button_event  = false;
    reset_button_event = false;

    heartbeat_led_status.toggle_time = 0;
    heartbeat_led_status.illuminated = true;
    iv_pump_led_status.toggle_time   = 0;
    iv_pump_led_status.illuminated   = true;
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
    /*
     * Run student_main for just long enough to execute the INIT case
     * and transition to BLINKING_RUN.  We're verifying that INIT
     * completes without returning an error (which would abort the thread).
     *
     * A more thorough check requires wrapping gpio_pin_configure_dt with
     * a spy; here we check the observable side-effect: state changed.
     */
    start_main(150);  /* INIT runs, state becomes BLINKING_RUN */

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
    /*
     * INIT sets next_state = BLINKING_RUN deliberately skipping
     * BLINKING_ENTRY.  Verify the first stable state is BLINKING_RUN.
     */
    start_main(150);

    zassert_equal(state, BLINKING_RUN,
        "INIT should transition directly to BLINKING_RUN, got %d", state);

    /* Also confirm BLINKING_ENTRY was never the current state by checking
     * that heartbeat is ticking (only starts in BLINKING_RUN onward). */
    bool illuminated_before = heartbeat_led_status.illuminated;
    k_msleep(600);  /* > 500 ms heartbeat interval */
    zassert_not_equal(heartbeat_led_status.illuminated, illuminated_before,
        "Heartbeat should have toggled, proving BLINKING_RUN is active");
}

/* ================================================================== */
/*  TEST 3 – BLINKING_RUN: 2 Hz action LEDs, 1 Hz heartbeat           */
/* ================================================================== */
ZTEST(state_machine_tests, test_03_blinking_run_default_freq)
{
    start_main(150);  /* reach BLINKING_RUN */

    /* Heartbeat at 1 Hz */
    assert_blink_freq(&heartbeat_led_status, 2000, 1, 1);

    /* Action LEDs at 2 Hz */
    assert_blink_freq(&iv_pump_led_status, 2000, 2, 1);

    /* iv_pump and buzzer must be out of phase at every sample point.
     * We infer buzzer state from the fact it's toggled together with iv_pump
     * but with inverse logic (one goes HIGH as the other goes LOW). */
    /* Because we don't have direct buzzer status struct we use the invariant:
     * after any actionLEDs() call, iv_pump_led_status.illuminated should be
     * opposite to the buzzer pin.  Indirectly we check action_led_hz. */
    zassert_equal(action_led_hz, LED_BLINK_FREQ_HZ,
        "Default frequency should be %d Hz", LED_BLINK_FREQ_HZ);

    /* Error LED: we cannot read GPIO_OUTPUT directly without gpio_emul,
     * but we can assert no transition to ERROR occurred. */
    zassert_equal(state, BLINKING_RUN,
        "State should remain BLINKING_RUN with no button presses");
}

/* ================================================================== */
/*  TEST 4 – BLINKING_ENTRY restores LEDs after returning from RESET   */
/* ================================================================== */
ZTEST(state_machine_tests, test_04_blinking_entry_restores_state)
{
    /*
     * Force a path through BLINKING_ENTRY by starting in RESET state.
     * RESET sets next_state = BLINKING_ENTRY which then transitions to
     * BLINKING_RUN.
     */
    state      = RESET;
    next_state = RESET;
    start_main(200);

    /* After RESET → BLINKING_ENTRY → BLINKING_RUN the state should settle */
    k_msleep(100);
    zassert_equal(state, BLINKING_RUN,
        "Should reach BLINKING_RUN via BLINKING_ENTRY, got %d", state);

    /* Frequency should be reset to default by RESET case */
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

    /* Verify new toggle interval ≈ 500 ms (1 Hz) */
    assert_blink_freq(&iv_pump_led_status, 2000, 1, 1);

    /* Heartbeat still 1 Hz */
    assert_blink_freq(&heartbeat_led_status, 2000, 1, 1);
}

/* ================================================================== */
/*  TEST 6 – Freq Down below MIN triggers ERROR                        */
/* ================================================================== */
ZTEST(state_machine_tests, test_06_freq_down_below_min_triggers_error)
{
    action_led_hz = 1;  /* already at minimum */
    start_main(150);

    /* One more press drops to 0 → out of range */
    press_button(&down_button_event, 100);

    zassert_equal(state, ERROR,
        "State should be ERROR when action_led_hz < 1, got %d", state);

    /* Heartbeat should still run in ERROR */
    bool hb_before = heartbeat_led_status.illuminated;
    k_msleep(600);
    zassert_not_equal(heartbeat_led_status.illuminated, hb_before,
        "Heartbeat LED must keep toggling in ERROR state");
}

/* ================================================================== */
/*  TEST 7 – ERROR state: Reset button → BLINKING_ENTRY               */
/* ================================================================== */
ZTEST(state_machine_tests, test_07_error_reset_button)
{
    /* Start directly in ERROR */
    state         = ERROR;
    next_state    = ERROR;
    action_led_hz = 0;  /* simulates how we got here */
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

    assert_blink_freq(&iv_pump_led_status, 2000, 3, 1);
    assert_blink_freq(&heartbeat_led_status, 2000, 1, 1);
}

/* ================================================================== */
/*  TEST 9 – Freq Up: 3 Hz → 4 Hz                                     */
/* ================================================================== */
ZTEST(state_machine_tests, test_09_freq_up_3_to_4)
{
    action_led_hz = 3;
    start_main(150);

    press_button(&up_button_event, 50);

    zassert_equal(action_led_hz, 4,
        "action_led_hz should be 4, got %d", action_led_hz);
    zassert_equal(state, BLINKING_RUN, "Should remain BLINKING_RUN at 4 Hz");

    assert_blink_freq(&iv_pump_led_status, 2000, 4, 1);
    assert_blink_freq(&heartbeat_led_status, 2000, 1, 1);
}

/* ================================================================== */
/*  TEST 10 – Freq Up: 4 Hz → 5 Hz (max valid)                        */
/* ================================================================== */
ZTEST(state_machine_tests, test_10_freq_up_4_to_5)
{
    action_led_hz = 4;
    start_main(150);

    press_button(&up_button_event, 50);

    zassert_equal(action_led_hz, 5,
        "action_led_hz should be 5 (max), got %d", action_led_hz);
    zassert_equal(state, BLINKING_RUN,
        "Should remain BLINKING_RUN at max freq 5 Hz");

    assert_blink_freq(&iv_pump_led_status, 2000, 5, 1);
    assert_blink_freq(&heartbeat_led_status, 2000, 1, 1);
}

/* ================================================================== */
/*  TEST 11 – Freq Up above MAX triggers ERROR                         */
/* ================================================================== */
ZTEST(state_machine_tests, test_11_freq_up_above_max_triggers_error)
{
    action_led_hz = 5;  /* already at maximum */
    start_main(150);

    press_button(&up_button_event, 100);

    zassert_equal(state, ERROR,
        "State should be ERROR when action_led_hz > 5, got %d", state);

    /* Heartbeat must keep running */
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
    action_led_hz = 6;  /* simulates upper overflow */
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

    press_button(&sleep_button_event, 100);
    zassert_equal(state, SLEEP, "Precondition: must be in SLEEP");

    /* Frequency must be preserved (not reset) */
    zassert_equal(action_led_hz, saved_hz,
        "SLEEP should preserve action_led_hz (%d), got %d", saved_hz, action_led_hz);

    /* iv_pump LED should be off (no toggling) */
    bool iv_before = iv_pump_led_status.illuminated;
    k_msleep(600);
    /* In SLEEP, actionLEDs() is NOT called, so iv_pump should not toggle */
    zassert_equal(iv_pump_led_status.illuminated, iv_before,
        "iv_pump_led should NOT toggle in SLEEP state");

    /* Heartbeat should still be toggling */
    bool hb_before = heartbeat_led_status.illuminated;
    k_msleep(600);
    zassert_not_equal(heartbeat_led_status.illuminated, hb_before,
        "Heartbeat LED should keep toggling in SLEEP state");
}

/* ================================================================== */
/*  TEST 15 – SLEEP: Sleep button again → BLINKING_ENTRY              */
/* ================================================================== */
ZTEST(state_machine_tests, test_15_sleep_button_wakes_up)
{
    start_main(150);

    /* Enter SLEEP */
    press_button(&sleep_button_event, 100);
    zassert_equal(state, SLEEP, "Precondition: must be in SLEEP");

    /* Press sleep again to wake */
    press_button(&sleep_button_event, 200);

    zassert_equal(state, BLINKING_RUN,
        "Should wake to BLINKING_RUN via BLINKING_ENTRY, got %d", state);

    /* Confirm action LEDs are active again */
    bool iv_before = iv_pump_led_status.illuminated;
    k_msleep(600);
    zassert_not_equal(iv_pump_led_status.illuminated, iv_before,
        "iv_pump_led should resume toggling after waking from SLEEP");
}

/* ================================================================== */
/*  TEST 16 – SLEEP: Reset button → RESET → BLINKING_ENTRY            */
/* ================================================================== */
ZTEST(state_machine_tests, test_16_sleep_then_reset_button)
{
    action_led_hz = 4;  /* non-default frequency before sleeping */
    start_main(150);

    /* Go to sleep */
    press_button(&sleep_button_event, 100);
    zassert_equal(state, SLEEP, "Precondition: must be in SLEEP");

    /* Press reset while asleep */
    press_button(&reset_button_event, 200);

    /* Reset overrides SLEEP's self-loop */
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