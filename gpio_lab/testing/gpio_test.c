#include "gpio_test.h"

#include <zephyr/ztest.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

static void stop_main(void);

/* ------------------------------------------------------------------ */
/*  Fixture                                                             */
/* ------------------------------------------------------------------ */

static void before(void *)
{
    stop_main();  /* abort any leftover thread from the previous test */
    
    k_event_clear(&program_test_events, FREQ_UP_TEST_NOTICE);
    k_event_clear(&program_test_events, FREQ_DOWN_TEST_NOTICE);
    k_event_clear(&program_test_events, RESET_BTN_TEST_NOTICE);
    k_event_clear(&program_test_events, SLEEP_BTN_TEST_NOTICE);
    k_event_clear(&program_test_events, ERROR_TEST_NOTICE);
    k_event_clear(&program_test_events, RESET_TEST_NOTICE);
    k_event_clear(&program_test_events, SLEEP_TEST_NOTICE);
}

static void after(void *)
{
    stop_main();
    k_msleep(50);
}

/* ------------------------------------------------------------------ */
/*  Thread boilerplate                                                */
/* ------------------------------------------------------------------ */

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
        simulate_button_click(&reset_button);
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

static void led_edge_callback(const struct device *dev,
                              struct gpio_callback *cb,
                              uint32_t pins)
{
    g_led_toggles++;
}

static void assert_led_blink_freq(const struct gpio_dt_spec *led,
                                  int window_ms,
                                  int expected_hz,
                                  int tolerance_hz,
                                  const char *led_name)
{
    g_led_toggles = 0;  // reset counter

    struct gpio_callback cb;
    gpio_init_callback(&cb, led_edge_callback, BIT(led->pin));
    // zassert_true(ret == 0, "LED %s: failed to init callback", led_name);

    int ret = gpio_add_callback_dt(led, &cb);
    zassert_true(ret == 0, "LED %s: failed to add callback", led_name);

    ret = gpio_pin_interrupt_configure_dt(led, GPIO_INT_EDGE_BOTH);
    zassert_true(ret == 0, "LED %s: failed to configure interrupt", led_name);

    k_msleep(window_ms);  // wait measurement window

    ret = gpio_pin_interrupt_configure_dt(led, GPIO_INT_DISABLE);
    zassert_true(ret == 0, "LED %s: failed to disable interrupt", led_name);

    gpio_remove_callback_dt(led, &cb);

    int measured_hz = (g_led_toggles * 500) / window_ms;

    zassert_within(measured_hz, expected_hz, tolerance_hz,
        "LED %s: expected ~%d Hz but measured ~%d Hz (%d toggles in %d ms)",
        led_name, expected_hz, measured_hz, g_led_toggles, window_ms);
}

static void simulate_button_click(const struct gpio_dt_spec *button)
{
    gpio_emul_input_set(button->port, button->pin, 1);
    // gpio_emul_fire_callbacks(button->port, button->pin);
    
    k_sleep(K_MSEC(5));
    
    gpio_emul_input_set(button->port, button->pin, 0);
    // gpio_emul_fire_callbacks(button->port, button->pin);
}

/* Assert that an LED is OFF */
static void assert_led_off(const struct gpio_dt_spec *led, const char *led_name)
{
    zassert_equal(gpio_pin_get_dt(led) > 0, false, "Expected LED %s on pin %d to be OFF, but it is ON", led_name, led->pin);
}

/* Assert that an LED is ON */
static void assert_led_on(const struct gpio_dt_spec *led, const char *led_name)
{
    zassert_equal(gpio_pin_get_dt(led), 1,
        "Expected LED %s on pin %d to be ON, but it is OFF",
        led_name, led->pin);
}

/* ================================================================== */
/*  TESTS                                                             */
/* ================================================================== */

ZTEST(state_machine_tests, test_01_default_frequencies)
{
    start_main(1000);
    
    assert_led_blink_freq(&heartbeat_led, 4000, 1, 1, "heartbeat");
    assert_led_blink_freq(&iv_pump_led, 4000, 2, 1, "iv_pump");
    assert_led_blink_freq(&buzzer_led, 4000, 2, 1, "buzzer");
    assert_led_off(&error_led, "error");
}

/* freq up once + check */
ZTEST(state_machine_tests, test_02_freq_up_one)
{
    start_main(1000);
    
    simulate_button_click(&freq_up_button);
    uint32_t events = k_event_wait(&program_test_events,
                                   FREQ_UP_TEST_NOTICE,
                                   true,
                                   K_MSEC(200));
    (void) events;
    
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
    assert_led_blink_freq(&iv_pump_led, 2000, 3, 1, "iv_pump");
    assert_led_blink_freq(&buzzer_led, 2000, 3, 1, "buzzer");
    assert_led_off(&error_led, "error");
}

/* freq down once + check */
ZTEST(state_machine_tests, test_03_freq_down_once)
{
    start_main(1000);
    
    simulate_button_click(&freq_down_button);
    uint32_t events = k_event_wait(&program_test_events,
                                   FREQ_DOWN_TEST_NOTICE,
                                   true,
                                   K_MSEC(200));
    (void) events;
    
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
    assert_led_blink_freq(&iv_pump_led, 2000, 1, 1, "iv_pump");
    assert_led_blink_freq(&buzzer_led, 2000, 1, 1, "buzzer");
    assert_led_off(&error_led, "error");
}

/* freq up once + reset + check */
ZTEST(state_machine_tests, test_04_freq_up_reset)
{
    start_main(1000);
    
    simulate_button_click(&freq_up_button);
    uint32_t events = k_event_wait(&program_test_events,
                                   FREQ_UP_TEST_NOTICE,
                                   true,
                                   K_MSEC(200));
    
    simulate_button_click(&reset_button);
    events = k_event_wait(&program_test_events,
                          RESET_TEST_NOTICE,
                          true,
                          K_MSEC(200));
    (void) events;
    
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
    assert_led_blink_freq(&iv_pump_led, 2000, 2, 1, "iv_pump");
    assert_led_blink_freq(&buzzer_led, 2000, 2, 1, "buzzer");
    assert_led_off(&error_led, "error");
}

/* freq down once + reset + check */
ZTEST(state_machine_tests, test_05_freq_down_reset)
{
    start_main(1000);
    
    simulate_button_click(&freq_down_button);
    uint32_t events = k_event_wait(&program_test_events,
                                   FREQ_DOWN_TEST_NOTICE,
                                   true,
                                   K_MSEC(200));
    
    simulate_button_click(&reset_button);
    events = k_event_wait(&program_test_events,
                          RESET_TEST_NOTICE,
                          true,
                          K_MSEC(200));
    (void) events;
    
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
    assert_led_blink_freq(&iv_pump_led, 2000, 2, 1, "iv_pump");
    assert_led_blink_freq(&buzzer_led, 2000, 2, 1, "buzzer");
    assert_led_off(&error_led, "error");
}

/* sleep + check */
ZTEST(state_machine_tests, test_06_sleep)
{
    start_main(1000);
    
    simulate_button_click(&sleep_button);
    uint32_t events = k_event_wait(&program_test_events,
                                   SLEEP_TEST_NOTICE,
                                   true,
                                   K_MSEC(200));
    (void) events;
    
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
    assert_led_off(&iv_pump_led, "iv_pump");
    assert_led_off(&buzzer_led, "buzzer");
    assert_led_off(&error_led, "error");
}

/* sleep + sleep + check */
ZTEST(state_machine_tests, test_07_sleep_sleep)
{
    start_main(1000);
    
    simulate_button_click(&sleep_button);
    uint32_t events = k_event_wait(&program_test_events,
                                   SLEEP_TEST_NOTICE,
                                   true,
                                   K_MSEC(200));
    
    simulate_button_click(&sleep_button);
    events = k_event_wait(&program_test_events,
                          SLEEP_BTN_TEST_NOTICE,
                          true,
                          K_MSEC(200));
    (void) events;
    
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
    assert_led_blink_freq(&iv_pump_led, 2000, 2, 1, "iv_pump");
    assert_led_blink_freq(&buzzer_led, 2000, 2, 1, "buzzer");
    assert_led_off(&error_led, "error");
}

/* freq up + sleep + sleep + check */
ZTEST(state_machine_tests, test_08_freq_up_sleep_sleep)
{
    start_main(1000);
    
    simulate_button_click(&freq_up_button);
    uint32_t events = k_event_wait(&program_test_events,
                                   FREQ_UP_TEST_NOTICE,
                                   true,
                                   K_MSEC(200));
    
    simulate_button_click(&sleep_button);
    events = k_event_wait(&program_test_events,
                          SLEEP_TEST_NOTICE,
                          true,
                          K_MSEC(200));
    
    simulate_button_click(&sleep_button);
    events = k_event_wait(&program_test_events,
                          SLEEP_BTN_TEST_NOTICE,
                          true,
                          K_MSEC(200));
    (void) events;
    
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
    assert_led_blink_freq(&iv_pump_led, 2000, 3, 1, "iv_pump");
    assert_led_blink_freq(&buzzer_led, 2000, 3, 1, "buzzer");
    assert_led_off(&error_led, "error");
}

/* freq up + sleep + reset + check */
ZTEST(state_machine_tests, test_09_freq_up_sleep_reset)
{
    start_main(1000);
    
    simulate_button_click(&freq_up_button);
    uint32_t events = k_event_wait(&program_test_events,
                                   FREQ_UP_TEST_NOTICE,
                                   true,
                                   K_MSEC(200));
    
    simulate_button_click(&sleep_button);
    events = k_event_wait(&program_test_events,
                          SLEEP_TEST_NOTICE,
                          true,
                          K_MSEC(200));
    
    simulate_button_click(&reset_button);
    events = k_event_wait(&program_test_events,
                          RESET_TEST_NOTICE,
                          true,
                          K_MSEC(200));
    (void) events;
    
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
    assert_led_blink_freq(&iv_pump_led, 2000, 2, 1, "iv_pump");
    assert_led_blink_freq(&buzzer_led, 2000, 2, 1, "buzzer");
    assert_led_off(&error_led, "error");
}

/* freq down + freq down + check */
ZTEST(state_machine_tests, test_10_freq_down_twice)
{
    start_main(1000);
    
    simulate_button_click(&freq_down_button);
    uint32_t events = k_event_wait(&program_test_events,
                                   FREQ_DOWN_TEST_NOTICE,
                                   true,
                                   K_MSEC(200));
    
    simulate_button_click(&freq_down_button);
    
    events = k_event_wait(&program_test_events,
                          FREQ_DOWN_TEST_NOTICE,
                          true,
                          K_MSEC(200));
    (void) events;

    events = k_event_wait(&program_test_events,
                          ERROR_TEST_NOTICE,
                          true,
                          K_MSEC(200));
    (void) events;
    
    k_msleep(50);
    
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
    assert_led_off(&iv_pump_led, "iv_pump");
    assert_led_off(&buzzer_led, "buzzer");
    assert_led_on(&error_led, "error");
}


// ZTEST(state_machine_tests, test_10_freq_down_twice)
// {
//     start_main(1000);
    
//     simulate_button_click(&freq_down_button);
//     uint32_t events = k_event_wait(&program_test_events,
//                                    FREQ_DOWN_TEST_NOTICE,
//                                    true,
//                                    K_MSEC(200));
    
//     simulate_button_click(&freq_down_button);
    
//     events = k_event_wait(&program_test_events,
//                           ERROR_TEST_NOTICE,
//                           true,
//                           K_MSEC(200));
//     (void) events;
    
//     k_msleep(50);
    
//     assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
//     assert_led_off(&iv_pump_led, "iv_pump");
//     assert_led_off(&buzzer_led, "buzzer");
//     assert_led_on(&error_led, "error");
// }


















// /* description */
// ZTEST(state_machine_tests, test_xx_name)
// {
//     start_main(1000);
// }

/**
 * up one -> freq 3, hb still 1
 * down one -> freq 1, hb still 1
 * up one, reset -> freq 2, hb still 1
 * down one, reset -> freq 2, hb still 1
 * sleep -> not blinking
 * sleep, sleep -> blinking
 * up one, sleep, sleep -> freq 4, hb still 1
 * up one, sleep, reset -> freq 2, hb still 1
 * down two -> error on, hb still 1
 * 
 * up four -> error on, hb still 1
 * down two, reset -> freq 2, hb still 1
 */












/* ================================================================== */
/*  Register suite                                                    */
/* ================================================================== */
ZTEST_SUITE(state_machine_tests, NULL, NULL, before, after, NULL);
