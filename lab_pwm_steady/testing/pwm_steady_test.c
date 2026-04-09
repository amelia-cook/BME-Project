#include "pwm_steady_test.h"

#include <zephyr/ztest.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/pwm.h>

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
    int val = gpio_emul_output_get(led->port, led->pin);
    zassert_equal(val, 0,
        "Expected LED %s on pin %d to be OFF, but it is ON",
        led_name, led->pin);
}

/* Assert that an LED is ON */
static void assert_led_on(const struct gpio_dt_spec *led, const char *led_name)
{
    int val = gpio_emul_output_get(led->port, led->pin);
    zassert_equal(val, 1,
        "Expected LED %s on pin %d to be ON, but it is OFF",
        led_name, led->pin);
}

/* ================================================================== */
/*  TESTS                                                             */
/* ================================================================== */

#include <zephyr/ztest.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/devicetree.h>

/* Use same alias as student code */
static const struct pwm_dt_spec pwm1 = PWM_DT_SPEC_GET(DT_ALIAS(pwm1));

ZTEST(pwm_emulation, test_pwm1_duty_cycle)
{
    zassert_true(device_is_ready(pwm1.dev), "PWM device not ready");

    /* Sleep to allow student code to set the PWM */
    k_sleep(K_MSEC(1000));  // 100 ms should be plenty in native_sim

    /* Read pulse and period */
    uint32_t pulse = pwm1.pulse;
    uint32_t period = pwm1.period;

    /* Verify that pulse is non-zero */
    zassert_true(pulse > 0, "PWM pulse not set");

    /* Compute duty cycle */
    float duty_cycle = (float)pulse / (float)period;

    /* Student sample code sets 50% duty cycle */
    zassert_within(duty_cycle, 0.5f, 0.01f, "PWM duty cycle incorrect");
}









// /* description */
// ZTEST(pwm_emulation, test_xx_name)
// {
//     start_main(1000);
// }

/* ================================================================== */
/*  Register suite                                                    */
/* ================================================================== */
ZTEST_SUITE(pwm_emulation, NULL, NULL, before, after, NULL);
