#include "gpio_test.h"

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>

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

/**
 * Assert that an LED toggles at approximately the expected frequency.
 */
static void assert_led_blink_freq(const struct gpio_dt_spec *led,
                                  int window_ms,
                                  int expected_hz,
                                  int tolerance_hz,
                                  const char* led_name)
{
    /* Step 1 – count edges over the full window */
    // bool last = led_is_on(led);
    int toggles = 0;
    int64_t end = k_uptime_get() + window_ms;
    bool last = gpio_pin_get_dt(led) > 0;

    while (k_uptime_get() < end) {
        // bool now = led_is_on(led);
        bool now = gpio_pin_get_dt(led) > 0;
        if (now != last) {
            toggles++;
            last = now;
        }
        k_msleep(1);  // 1 ms
    }

    /* Step 2 – compute frequency in Hz */
    // Each toggle represents half a cycle
    int measured_hz = (toggles * 500) / window_ms;

    /* Step 3 – assert within tolerance */
    zassert_within(measured_hz, expected_hz, tolerance_hz,
        "LED %s: epected ~%d Hz but measured ~%d Hz (%d toggles in %d ms)",
        led_name, expected_hz, measured_hz, toggles, window_ms);
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
static void assert_led_off(const struct gpio_dt_spec *led)
{
    int val = gpio_pin_get_dt(led);
    zassert_equal(val, 0,
                  "Expected LED on pin %d to be OFF, but value is %d",
                  led->pin, val);
}

/* Assert that an LED is ON */
static void assert_led_on(const struct gpio_dt_spec *led)
{
    int val = gpio_pin_get_dt(led);
    zassert_not_equal(val, 0,
                      "Expected LED on pin %d to be ON, but value is %d",
                      led->pin, val);
}

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
}

/* ================================================================== */
/*  TESTS                                                             */
/* ================================================================== */

/* check default frequencies frequency */
ZTEST(state_machine_tests, test_01_default_frequencies)
{
    start_main(150);
    
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
    assert_led_blink_freq(&iv_pump_led, 2000, 2, 1, "iv_pump");
    assert_led_blink_freq(&buzzer_led, 2000, 2, 1, "buzzer");
    assert_led_off(&error_led);
}







// /* freq up once + check */
// ZTEST(state_machine_tests, test_02_freq_up_one_test)
// {
//     start_main(150);
    
//     simulate_button_click(&freq_up_button);
//     uint32_t events = k_event_wait(&program_test_events,
//                                    FREQ_UP_TEST_NOTICE,
//                                    true,
//                                    K_MSEC(200));
//     (void) events;
    
//     assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
//     assert_led_blink_freq(&iv_pump_led, 2000, 3, 1, "iv_pump");
//     assert_led_blink_freq(&buzzer_led, 2000, 3, 1, "buzzer");
//     assert_led_off(&error_led);
// }

// /* freq down once + check */
// ZTEST(state_machine_tests, test_xx_name)
// {
//     start_main(150);
    
//     simulate_button_click(&freq_down_button);
//     uint32_t events = k_event_wait(&program_test_events,
//                                    FREQ_DOWN_TEST_NOTICE,
//                                    true,
//                                    K_MSEC(200));
//     (void) events;
    
//     assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
//     assert_led_blink_freq(&iv_pump_led, 2000, 1, 1, "iv_pump");
//     assert_led_blink_freq(&buzzer_led, 2000, 1, 1, "buzzer");
//     assert_led_off(&error_led);
// }

// /* freq up once + reset + check */
// ZTEST(state_machine_tests, test_02_freq_up_one_test)
// {
//     start_main(150);
    
//     simulate_button_click(&freq_up_button);
//     uint32_t events = k_event_wait(&program_test_events,
//                                    FREQ_UP_TEST_NOTICE,
//                                    true,
//                                    K_MSEC(200));
    
//     simulate_button_click(&reset_button);
//     events = k_event_wait(&program_test_events,
//                           RESET_BTN_TEST_NOTICE,
//                           true,
//                           K_MSEC(200));
//     (void) events;
    
//     assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
//     assert_led_blink_freq(&iv_pump_led, 2000, 2, 1, "iv_pump");
//     assert_led_blink_freq(&buzzer_led, 2000, 2, 1, "buzzer");
//     assert_led_off(&error_led);
// }

// /* freq down once + reset + check */
// ZTEST(state_machine_tests, test_02_freq_up_one_test)
// {
//     start_main(150);
    
//     simulate_button_click(&freq_down_button);
//     uint32_t events = k_event_wait(&program_test_events,
//                                    FREQ_DOWN_TEST_NOTICE,
//                                    true,
//                                    K_MSEC(200));
    
//     simulate_button_click(&reset_button);
//     events = k_event_wait(&program_test_events,
//                           RESET_BTN_TEST_NOTICE,
//                           true,
//                           K_MSEC(200));
//     (void) events;
    
//     assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
//     assert_led_blink_freq(&iv_pump_led, 2000, 2, 1, "iv_pump");
//     assert_led_blink_freq(&buzzer_led, 2000, 2, 1, "buzzer");
//     assert_led_off(&error_led);
// }

// /* sleep + check */
// ZTEST(state_machine_tests, test_xx_name)
// {
//     start_main(150);
    
//     simulate_button_click(&sleep_button);
//     uint32_t events = k_event_wait(&program_test_events,
//                                    SLEEP_BTN_TEST_NOTICE,
//                                    true,
//                                    K_MSEC(200));
//     (void) events;
    
//     assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
//     assert_led_off(&iv_pump_led);
//     assert_led_off(&buzzer_led);
//     assert_led_off(&error_led);
// }

// /* sleep + sleep + check */
// ZTEST(state_machine_tests, test_xx_name)
// {
//     start_main(150);
    
//     simulate_button_click(&sleep_button);
//     uint32_t events = k_event_wait(&program_test_events,
//                                    SLEEP_BTN_TEST_NOTICE,
//                                    true,
//                                    K_MSEC(200));
    
//     simulate_button_click(&sleep_button);
//     uint32_t events = k_event_wait(&program_test_events,
//                                    SLEEP_BTN_TEST_NOTICE,
//                                    true,
//                                    K_MSEC(200));
//     (void) events;
    
//     assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
//     assert_led_blink_freq(&iv_pump_led, 2000, 2, 1, "iv_pump");
//     assert_led_blink_freq(&buzzer_led, 2000, 2, 1, "buzzer");
//     assert_led_off(&error_led);
// }






// /* description */
// ZTEST(state_machine_tests, test_xx_name)
// {
//     start_main(150);
// }

/**
 * up one -> freq 3, hb still 1
 * down one -> freq 1, hb still 1
 * up one, reset -> freq 2, hb still 1
 * down one, reset -> freq 2, hb still 1
 * sleep -> not blinking
 * sleep, sleep -> blinking
 * 
 * up two, sleep, sleep -> freq 4, hb still 1
 * up one, sleep, reset -> freq 2, hb still 1
 * down two -> error on, hb still 1
 * up four -> error on, hb still 1
 * down two, reset -> freq 2, hb still 1
 */












/* ================================================================== */
/*  Register suite                                                    */
/* ================================================================== */
ZTEST_SUITE(state_machine_tests, NULL, NULL, before, after, NULL);
