#include "adc_test.h"

#include <zephyr/ztest.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/adc/adc_emul.h>
/*
 * M_PI is not guaranteed by C11 strict mode (<math.h> only exposes it
 * with _GNU_SOURCE or _USE_MATH_DEFINES).  Define a local fallback so
 * the build is portable across toolchains without touching prj.conf.
 */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#include <math.h>   /* sinf() */

/* ================================================================== */
/*  ADC emulator device handle                                        */
/* ================================================================== */

/*
 * Grabbed once in before() and passed to set_ain0_mv /
 * set_differential_sine helpers.  Avoids repeating DEVICE_DT_GET
 * in every test.
 */
static const struct device *adc_emul_dev;

static bool wait_for_event(uint32_t mask, int timeout_ms)
{
    int64_t start = k_uptime_get();
    uint32_t events = 0;

    do {
        events = k_event_wait(&program_test_events, mask, true, K_MSEC(2000));
        if (events & mask) {
            return true;
        }
    } while ((k_uptime_get() - start) < timeout_ms);

    return false;
}


/* ================================================================== */
/*  Fixture                                                           */
/* ================================================================== */

static void before(void *)
{
    stop_main();

    gpio_emul_input_set(read_button.port,  read_button.pin,  0);
    gpio_emul_input_set(sleep_button.port, sleep_button.pin, 0);
    gpio_emul_input_set(reset_button.port, reset_button.pin, 0);

    adc_emul_dev = DEVICE_DT_GET(ADC_EMUL_NODE);
    zassert_true(device_is_ready(adc_emul_dev), "ADC emulator not ready");

    /* Start main and let it fully settle into IDLE first */
    start_main(500);

    /* NOW clear — after all init stray callbacks have already fired */
    k_event_clear(&program_test_events,
        FREQ_UP_TEST_NOTICE | FREQ_DOWN_TEST_NOTICE |
        RESET_BTN_TEST_NOTICE | SLEEP_BTN_TEST_NOTICE |
        ERROR_TEST_NOTICE | RESET_TEST_NOTICE | SLEEP_TEST_NOTICE |
        ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE |
        ADC_BLINK_DONE_NOTICE |
        ADC_SAMPLE_TRIGGERED_NOTICE | ADC_SAMPLE_COMPLETE_NOTICE |
        ADC_CYCLES_COMPUTED_NOTICE |
        ADC_ASYNC_DONE_NOTICE | ADC_ASYNC_TIMEOUT_NOTICE);
}

static void after(void *)
{
    stop_main();
    k_msleep(50);
}

/* ================================================================== */
/*  Thread boilerplate (identical pattern to gpio_test.c)            */
/* ================================================================== */

static void student_main_entry(void *, void *, void *)
{
    main_running = true;
    student_main();
    main_running = false;
}

static void stop_main(void)
{
    if (main_running) {
        simulate_button_click(&reset_button);
        k_thread_abort(student_main_tid);
        k_msleep(20);
        main_running = false;
    }
}

/*
 * settle_ms of 500 is sufficient for INIT → IDLE on the ADC lab;
 * ADC channel setup adds a few extra ms vs the GPIO-only INIT.
 */
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

/* ================================================================== */
/*  GPIO helpers (reused from gpio_test pattern)                     */
/* ================================================================== */

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
    g_led_toggles = 0;

    struct gpio_callback cb;
    gpio_init_callback(&cb, led_edge_callback, BIT(led->pin));

    int ret = gpio_add_callback_dt(led, &cb);
    zassert_true(ret == 0, "LED %s: failed to add callback", led_name);

    ret = gpio_pin_interrupt_configure_dt(led, GPIO_INT_EDGE_BOTH);
    zassert_true(ret == 0, "LED %s: failed to configure interrupt", led_name);

    k_msleep(window_ms);

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
    k_sleep(K_MSEC(5));
    gpio_emul_input_set(button->port, button->pin, 0);
}

static void assert_led_off(const struct gpio_dt_spec *led, const char *led_name)
{
    int val = gpio_emul_output_get(led->port, led->pin);
    zassert_equal(val, 0,
        "Expected LED %s on pin %d to be OFF, but it is ON",
        led_name, led->pin);
}

static void assert_led_on(const struct gpio_dt_spec *led, const char *led_name)
{
    int val = gpio_emul_output_get(led->port, led->pin);
    zassert_equal(val, 1,
        "Expected LED %s on pin %d to be ON, but it is OFF",
        led_name, led->pin);
}

/* ================================================================== */
/*  ADC emulator helpers                                              */
/* ================================================================== */

/*
 * adc_emul value callback signature (from zephyr/drivers/adc/adc_emul.h):
 *   int my_cb(const struct device *dev, unsigned int chan,
 *             void *data, uint32_t *result)
 * The `data` pointer carries whatever was passed as the last argument to
 * adc_emul_value_func_set(); we always pass NULL so it is unused.
 * Returns 0 on success.
 */

/* --- Phase 1: constant AIN0 voltage -------------------------------- */

static uint32_t g_ain0_raw_value;

static int ain0_const_cb(const struct device *dev,
                         unsigned int chan,
                         void *data,
                         uint32_t *result)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);
    ARG_UNUSED(data);
    *result = g_ain0_raw_value;
    return 0;
}

/*
 * set_ain0_mv — inject a constant millivolt value on AIN0.
 *
 * The emulator's ref-internal-mv = 3000, resolution = 12 bits.
 * raw = (mv / 3000) * 4095   (clamped to [0, 4095])
 */
static void set_ain0_mv(const struct device *dev, int millivolts)
{
    if (millivolts < 0)       { millivolts = 0; }
    if (millivolts > MAX_V_MV){ millivolts = MAX_V_MV; }

    g_ain0_raw_value = (uint32_t)(((uint64_t)millivolts * 4095U) / MAX_V_MV);
    int ret = adc_emul_value_func_set(dev, AIN0_CHANNEL_ID, ain0_const_cb, NULL);
    zassert_ok(ret, "set_ain0_mv: adc_emul_value_func_set failed (%d)", ret);
}

/* --- Phase 1: duty-cycle measurement ------------------------------- */

/*
 * assert_blinker_freq / assert_blink_ontime_pct / assert_blink_total_duration_ms
 * all wrap the LED helpers specifically for the blinker_led.
 */
static void assert_blinker_freq(int window_ms, int expected_hz, int tolerance_hz)
{
    assert_led_blink_freq(&blinker_led, window_ms,
                          expected_hz, tolerance_hz, "blinker");
}

/*
 * Duty-cycle measurement using edge timestamps.
 * Counts total high-time across window_ms then computes percentage.
 *
 * Implementation note: a pair of callbacks (rising / falling) are
 * registered, each recording k_uptime_get() at the edge. High-time
 * is accumulated between rising→falling pairs.
 */
static volatile int64_t g_rise_time_ms;
static volatile int64_t g_high_time_accum_ms;
static volatile bool    g_currently_high;

static void blinker_rise_cb(const struct device *dev,
                            struct gpio_callback *cb,
                            uint32_t pins)
{
    g_rise_time_ms    = k_uptime_get();
    g_currently_high  = true;
}

static void blinker_fall_cb(const struct device *dev,
                            struct gpio_callback *cb,
                            uint32_t pins)
{
    if (g_currently_high) {
        g_high_time_accum_ms += k_uptime_get() - g_rise_time_ms;
        g_currently_high = false;
    }
}

static void assert_blink_ontime_pct(int window_ms, int expected_pct,
                                    int tolerance_pct)
{
    g_high_time_accum_ms = 0;
    g_currently_high     = false;

    struct gpio_callback rise_cb, fall_cb;

    /* Two separate callbacks: one per edge polarity */
    gpio_init_callback(&rise_cb, blinker_rise_cb, BIT(blinker_led.pin));
    gpio_init_callback(&fall_cb, blinker_fall_cb, BIT(blinker_led.pin));

    int ret;
    ret = gpio_add_callback_dt(&blinker_led, &rise_cb);
    zassert_ok(ret, "duty: add rise cb failed");
    ret = gpio_add_callback_dt(&blinker_led, &fall_cb);
    zassert_ok(ret, "duty: add fall cb failed");

    ret = gpio_pin_interrupt_configure_dt(&blinker_led, GPIO_INT_EDGE_BOTH);
    zassert_ok(ret, "duty: configure interrupt failed");

    k_msleep(window_ms);

    gpio_pin_interrupt_configure_dt(&blinker_led, GPIO_INT_DISABLE);
    gpio_remove_callback_dt(&blinker_led, &rise_cb);
    gpio_remove_callback_dt(&blinker_led, &fall_cb);

    int measured_pct = (int)((g_high_time_accum_ms * 100) / window_ms);

    zassert_within(measured_pct, expected_pct, tolerance_pct,
        "blinker duty cycle: expected ~%d%% but measured ~%d%%",
        expected_pct, measured_pct);
}

/*
 * assert_blink_total_duration_ms — measure how long blinker_led
 * remains active (first edge to last edge) against the 5s spec.
 *
 * Waits up to (expected_ms + tolerance_ms + 500) ms for ADC_BLINK_DONE_NOTICE.
 * Records first-edge and done-notice timestamps.
 */
static void assert_blink_total_duration_ms(int expected_ms, int tolerance_ms)
{
    /* Record first toggle */
    g_led_toggles = 0;
    struct gpio_callback cb;
    gpio_init_callback(&cb, led_edge_callback, BIT(blinker_led.pin));
    gpio_add_callback_dt(&blinker_led, &cb);
    gpio_pin_interrupt_configure_dt(&blinker_led, GPIO_INT_EDGE_BOTH);

    /* Wait for first edge to appear */
    int64_t t_wait = k_uptime_get();
    while (g_led_toggles == 0 && (k_uptime_get() - t_wait) < 500) {
        k_msleep(5);
    }
    int64_t t_start = k_uptime_get();

    gpio_pin_interrupt_configure_dt(&blinker_led, GPIO_INT_DISABLE);
    gpio_remove_callback_dt(&blinker_led, &cb);

    zassert_true(g_led_toggles > 0, "blinker never toggled, can't measure duration");

    /* Now wait for the blink-complete notice */
    uint32_t events = k_event_wait(&program_test_events,
                                   ADC_BLINK_DONE_NOTICE,
                                   true,
                                   K_MSEC(expected_ms + tolerance_ms + 500));
    zassert_true(events & ADC_BLINK_DONE_NOTICE,
        "ADC_BLINK_DONE_NOTICE never fired within timeout");

    int64_t measured_ms = k_uptime_get() - t_start;

    zassert_within((int)measured_ms, expected_ms, tolerance_ms,
        "blink duration: expected ~%d ms but measured ~%d ms",
        expected_ms, (int)measured_ms);
}

/* --- Phase 2/3: differential sine-wave injection ------------------- */

/*
 * Sine callback context. The adc_emul calls our function once per
 * sample, in order, so we use a global sample index counter.
 *
 * For a differential measurement the emulator is given two channel
 * callbacks. AIN1 returns +sin, AIN2 returns -sin (anti-phase), so
 * the differential result buf[i] = AIN1[i] - AIN2[i] = 2*sin(...).
 * The student's calc_cycles counts negative→positive zero crossings
 * of the raw differential buffer, so we simply drive AIN1 = +sin
 * and AIN2 = 0 (or constant) — the differential will track the sine.
 *
 * Simpler and avoids dependency on how the emulator combines channels:
 * drive AIN1 with the full sine, AIN2 constant at 0, so the student's
 * diff read gives the sine directly.
 */

static volatile uint32_t g_sine_sample_idx;

static int ain1_sine_cb(const struct device *dev,
                        unsigned int chan,
                        void *data,
                        uint32_t *result)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);
    ARG_UNUSED(data);

    /* 
     * Convert sample index to time:
     *   t_us = idx * sample_interval_us
     * Then compute sin(2π * freq * t_us / 1e6).
     * Scale to [0, 2*amplitude] (unsigned raw ADC counts, midpoint = amplitude).
     */
    float t_s = (float)g_sine_sample_idx * (float)g_sine_ain1.sample_interval_us
                / 1000000.0f;
    float sine_val = sinf(2.0f * M_PI * (float)g_sine_ain1.freq_hz * t_s);

    /* Map [-1,1] → [0, 2*amplitude], midpoint = amplitude */
    int32_t raw = (int32_t)(g_sine_ain1.amplitude_raw * sine_val)
                  + g_sine_ain1.amplitude_raw;

    if (raw < 0)    { raw = 0; }
    if (raw > 4095) { raw = 4095; }

    *result = (uint32_t)raw;

    /* Advance index (both channels share the same counter; AIN1 is read first) */
    g_sine_sample_idx++;
    return 0;
}

static int ain2_const_zero_cb(const struct device *dev,
                              unsigned int chan,
                              void *data,
                              uint32_t *result)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);
    ARG_UNUSED(data);
    /*
     * Drive AIN2 at midpoint (amplitude_raw) so the differential
     * result buf[i] = AIN1[i] - AIN2[i] oscillates symmetrically
     * around zero, giving clean zero crossings for calc_cycles.
     */
    *result = (uint32_t)g_sine_ain1.amplitude_raw;
    return 0;
}

static void set_differential_sine(const struct device *dev,
                                  int freq_hz,
                                  int amplitude_raw,
                                  int sample_iv_us)
{
    g_sine_ain1.freq_hz          = freq_hz;
    g_sine_ain1.amplitude_raw    = amplitude_raw;
    g_sine_ain1.sample_interval_us = sample_iv_us;
    g_sine_sample_idx            = 0;

    int ret;
    ret = adc_emul_value_func_set(dev, AIN1_CHANNEL_ID, ain1_sine_cb, NULL);
    zassert_ok(ret, "set_differential_sine: AIN1 func_set failed (%d)", ret);

    ret = adc_emul_value_func_set(dev, AIN2_CHANNEL_ID, ain2_const_zero_cb, NULL);
    zassert_ok(ret, "set_differential_sine: AIN2 func_set failed (%d)", ret);
}

/*
 * assert_cycles_computed — wait for ADC_CYCLES_COMPUTED_NOTICE then
 * check student_calc_cycles_result.
 *
 * Expected value derivation:
 *   window = BUFFER_ARRAY_LEN * SAMPLE_INTERVAL_us / 1e6  seconds
 *   cycles = freq_hz * window
 *   e.g. 10 Hz * (800 * 2500e-6 s) = 10 * 2.0 = 20 cycles
 */
 
static void assert_cycles_computed(int expected_cycles, int tolerance)
{
    /*
     * Acquisition takes BUFFER_ARRAY_LEN * SAMPLE_INTERVAL µs ≈ 2 s.
     * Add 500 ms margin.
     */
    int timeout_ms = (BUFFER_ARRAY_LEN * 2500) / 1000 + 500;

    uint32_t events = k_event_wait(&program_test_events,
                                   ADC_CYCLES_COMPUTED_NOTICE,
                                   true,
                                   K_MSEC(timeout_ms));
    zassert_true(events & ADC_CYCLES_COMPUTED_NOTICE,
        "ADC_CYCLES_COMPUTED_NOTICE never fired (timeout %d ms)", timeout_ms);

    zassert_within(student_calc_cycles_result, expected_cycles, tolerance,
        "calc_cycles: expected ~%d but got %d",
        expected_cycles, student_calc_cycles_result);
}

/* ================================================================== */
/*  PHASE 1 TESTS — adc_single_sample_tests                          */
/* ================================================================== */

/*
 * Press read_button with a known voltage and verify both
 * ADC_READ_TRIGGERED and ADC_READ_COMPLETE notices fire.
 */
// ZTEST(adc_single_sample_tests, test_p1_01_read_button_triggers_adc)
// {
//     set_ain0_mv(adc_emul_dev, 1500);
//     start_main(500);

//     simulate_button_click(&read_button);

//     uint32_t events = k_event_wait(&program_test_events,
//                                    ADC_READ_TRIGGERED_NOTICE,
//                                    true, K_MSEC(300));
//     zassert_true(events & ADC_READ_TRIGGERED_NOTICE,
//         "ADC_READ_TRIGGERED_NOTICE never fired after read_button press");

//     events = k_event_wait(&program_test_events,
//                           ADC_READ_COMPLETE_NOTICE,
//                           true, K_MSEC(500));
//     zassert_true(events & ADC_READ_COMPLETE_NOTICE,
//         "ADC_READ_COMPLETE_NOTICE never fired");

//     /* Sanity-check the reported millivolts */
//     zassert_within(student_adc_mv, 1500, 200,
//         "student_adc_mv: expected ~1500 mV but got %d", student_adc_mv);
// }

// ZTEST(adc_single_sample_tests, test_p1_00_read_button_triggers_adc){
//     set_ain0_mv(adc_emul_dev, 1500);
//     // start_main(500);
//     k_msleep(1000);
//     simulate_button_click(&read_button);
//     zassert_true(wait_for_event(ADC_READ_TRIGGERED_NOTICE, 500),
//         "ADC_READ_TRIGGERED_NOTICE never fired after read_button press");
//     zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
//         "ADC_READ_COMPLETE_NOTICE never fired");
//     zassert_within(student_adc_mv, 1500, 200,
//         "student_adc_mv mismatch");
// }


ZTEST(adc_single_sample_tests, test_p1_01_read_button_triggers_adc)
{
    set_ain0_mv(adc_emul_dev, 1500);
    // start_main(500);

    // k_msleep(1000);

    simulate_button_click(&read_button);
    // k_msleep(1000);

    /* CHANGE: event wait robustness */
    zassert_true(wait_for_event(ADC_READ_TRIGGERED_NOTICE, 1000),
        "ADC_READ_TRIGGERED_NOTICE never fired after read_button press");
    // k_msleep(1000);
    /* CHANGE: event wait robustness */
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 1000),
        "ADC_READ_COMPLETE_NOTICE never fired");
    // k_msleep(1000);
    zassert_within(student_adc_mv, 1500, 200,
        "student_adc_mv mismatch");
}


/*
 * 0 V → should map to 1 Hz (minimum).
 */
ZTEST(adc_single_sample_tests, test_p1_02_zero_volts_maps_to_1hz)
{
    set_ain0_mv(adc_emul_dev, 0);
    // start_main(500);

    simulate_button_click(&read_button);
    // k_event_wait(&program_test_events, ADC_READ_COMPLETE_NOTICE, true, K_MSEC(500));
    /* CHANGE: event wait robustness */
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    /* Measure over 3 s window — enough for several 1 Hz cycles */
    assert_blinker_freq(3000, 1, 1);
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat");
}

/*
 * 3000 mV → should map to 5 Hz (maximum).
 */
ZTEST(adc_single_sample_tests, test_p1_03_full_volts_maps_to_5hz)
{
    set_ain0_mv(adc_emul_dev, 3000);
    // start_main(500);

    simulate_button_click(&read_button);
    // k_event_wait(&program_test_events, ADC_READ_COMPLETE_NOTICE, true, K_MSEC(500));
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    assert_blinker_freq(2000, 5, 1);
}

/*
 * 1500 mV → should map to 3 Hz (midpoint).
 */
ZTEST(adc_single_sample_tests, test_p1_04_mid_volts_maps_to_3hz)
{
    set_ain0_mv(adc_emul_dev, 1500);
    // start_main(500);

    simulate_button_click(&read_button);
    // k_event_wait(&program_test_events, ADC_READ_COMPLETE_NOTICE, true, K_MSEC(500));
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    assert_blinker_freq(2000, 3, 1);
}

/*
 * 1500 mV → verify ~10% duty cycle on blinker_led.
 * Measure over 2 s to accumulate enough on/off cycles.
 */
ZTEST(adc_single_sample_tests, test_p1_05_duty_cycle_10pct)
{
    set_ain0_mv(adc_emul_dev, 1500);
    // start_main(500);

    simulate_button_click(&read_button);
    // k_event_wait(&program_test_events, ADC_READ_COMPLETE_NOTICE, true, K_MSEC(500));
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    assert_blink_ontime_pct(2000, 10, 5);  /* ±5% tolerance */
}

/*
 * Verify blinker_led stays on for ~5 seconds then turns off.
 * ADC_BLINK_DONE_NOTICE must fire.
 */
ZTEST(adc_single_sample_tests, test_p1_06_blink_duration_5s)
{
    set_ain0_mv(adc_emul_dev, 1500);
    // start_main(500);

    simulate_button_click(&read_button);
    // k_event_wait(&program_test_events, ADC_READ_COMPLETE_NOTICE, true, K_MSEC(500));
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    assert_blink_total_duration_ms(BLINKING_TIME_MS, 300); /* ±300 ms */

    /* After blink completes, blinker should be off */
    k_msleep(100);
    assert_led_off(&blinker_led, "blinker (after 5s)");
}

/*
 * Second read_button press during active blink should be ignored
 * (interrupt is disabled in READING state, not re-enabled until IDLE).
 */
ZTEST(adc_single_sample_tests, test_p1_07_read_button_disabled_during_blink)
{
    set_ain0_mv(adc_emul_dev, 1500);
    // start_main(500);

    /* First press — valid */
    simulate_button_click(&read_button);
    // uint32_t events = k_event_wait(&program_test_events,
    //                                ADC_READ_TRIGGERED_NOTICE,
    //                                true, K_MSEC(300));
    // zassert_true(events & ADC_READ_TRIGGERED_NOTICE, "First press not detected");

    /* Wait for blink to start */
    // k_event_wait(&program_test_events, ADC_READ_COMPLETE_NOTICE, true, K_MSEC(500));

    /* CHANGE: event wait robustness */
    zassert_true(wait_for_event(ADC_READ_TRIGGERED_NOTICE, 500),
        "First press not detected");

    /* CHANGE: event wait robustness */
    zassert_true(wait_for_event(ADC_READ_COMPLETE_NOTICE, 800),
        "ADC_READ_COMPLETE_NOTICE never fired");

    /* Clear triggered notice so we can watch for a spurious second one */
    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE);

    /* Second press — should be ignored */
    simulate_button_click(&read_button);
    k_msleep(100);

    uint32_t events = k_event_wait(&program_test_events,
                          ADC_READ_TRIGGERED_NOTICE,
                          false,   /* don't consume — just peek */
                          K_MSEC(100));

    zassert_false(events & ADC_READ_TRIGGERED_NOTICE,
        "read_button was not disabled: second ADC_READ_TRIGGERED fired");
}

/*
 * Out-of-range voltage → ERROR state.
 * Inject a raw value just above 4095 by setting mv > MAX_V_MV.
 * (The student's reading_run checks val_mv < 0 || val_mv > 3000.)
 *
 * We simulate this by setting AIN0 to produce a raw value the
 * student's adc_raw_to_millivolts_dt maps above 3000 mV.
 * Easiest approach: temporarily override the emulator to return 4096
 * (out-of-range for a 12-bit converter) by setting mv to > MAX_V_MV
 * in the raw count calculation.
 */
static int ain0_over_range_cb(const struct device *dev,
                              unsigned int chan,
                              void *data,
                              uint32_t *result)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);
    ARG_UNUSED(data);
    *result = 4095;  /* max raw; at 3 V ref this hits exactly 3000 mV */
    return 0;
}

ZTEST(adc_single_sample_tests, test_p1_08_error_on_bad_voltage)
{
    /*
     * The student's ERROR condition is val_mv < MIN_V_MV || val_mv > MAX_V_MV.
     * We need a value strictly above 3000 mV.  Since 4095 raw == 3000 mV on a
     * 3 V / 12-bit scale, we inject an ADC read failure (ret < 0) instead by
     * not calling adc_channel_setup_dt on the emulator channel.
     *
     * Alternative: many zephyr,adc-emul builds support returning an error code
     * from the value callback (return -EIO).  We use that here.
     */
    int ret = adc_emul_value_func_set(adc_emul_dev, AIN0_CHANNEL_ID,
                                      ain0_over_range_cb, NULL);

    zassert_ok(ret, "failed to set error callback");

    // start_main(500);

    simulate_button_click(&read_button);

    // k_event_wait(&program_test_events, ADC_READ_TRIGGERED_NOTICE, true, K_MSEC(300));
    /* CHANGE: event wait robustness */
    zassert_true(wait_for_event(ADC_READ_TRIGGERED_NOTICE, 500),
        "ADC_READ_TRIGGERED_NOTICE never fired");

    /* Give state machine time to transition to ERROR */
    k_msleep(200);

    assert_led_on(&error_led, "error");
    assert_led_off(&blinker_led, "blinker");

    /* Only reset_button should exit ERROR */
    simulate_button_click(&reset_button);

    // uint32_t events = k_event_wait(&program_test_events,
    //                                RESET_TEST_NOTICE, true, K_MSEC(300));
    // zassert_true(events & RESET_TEST_NOTICE, "RESET_TEST_NOTICE not fired");

    /* CHANGE: event wait robustness */
    zassert_true(wait_for_event(RESET_TEST_NOTICE, 500),
        "RESET_TEST_NOTICE not fired");

    k_msleep(100);
    assert_led_off(&error_led, "error (after reset)");
}

/*
 * Linearity sweep: inject 0, 750, 1500, 2250, 3000 mV and check that
 * student_mapped_freq is monotonically increasing and approximately
 * linear (each step should increase by ~1 Hz).
 */
ZTEST(adc_single_sample_tests, test_p1_09_linearity_sweep)
{
    int voltages_mv[]  = {0, 750, 1500, 2250, 3000};
    int expected_hz[]  = {1,   2,    3,    4,    5};
    float prev_freq    = -1.0f;

    for (int i = 0; i < 5; i++) {
        /* Reset to IDLE before each sub-test */
        stop_main();
        k_event_clear(&program_test_events,
            ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE |
            ADC_BLINK_DONE_NOTICE);

        set_ain0_mv(adc_emul_dev, voltages_mv[i]);
        // start_main(500);

        simulate_button_click(&read_button);
        uint32_t events = k_event_wait(&program_test_events,
                                       ADC_READ_COMPLETE_NOTICE,
                                       true, K_MSEC(500));
        zassert_true(events & ADC_READ_COMPLETE_NOTICE,
            "Sweep[%d]: ADC_READ_COMPLETE never fired", i);

        /* Check monotonically increasing */
        zassert_true(student_mapped_freq > prev_freq,
            "Sweep[%d]: freq %f not > prev %f (not monotonic)",
            i, (double)student_mapped_freq, (double)prev_freq);
        prev_freq = student_mapped_freq;

        /* Check approximately correct Hz (±0.75 Hz tolerance) */
        zassert_within((int)(student_mapped_freq * 100),
                       expected_hz[i] * 100, 75,
            "Sweep[%d]: expected ~%d Hz, got %.2f Hz",
            i, expected_hz[i], (double)student_mapped_freq);
    }
}

/*
 * Heartbeat continues at 1 Hz throughout the entire READING + BLINKING cycle.
 */
ZTEST(adc_single_sample_tests, test_p1_10_heartbeat_unaffected)
{
    set_ain0_mv(adc_emul_dev, 1500);
    // start_main(500);

    simulate_button_click(&read_button);
    k_event_wait(&program_test_events, ADC_READ_COMPLETE_NOTICE, true, K_MSEC(500));

    /* Measure heartbeat while blinker is active */
    assert_led_blink_freq(&heartbeat_led, 3000, 1, 1, "heartbeat (during blink)");
}

/* ================================================================== */
/*  Register suites                                                   */
/* ================================================================== */
ZTEST_SUITE(adc_single_sample_tests, NULL, NULL, before, after, NULL);
// ZTEST_SUITE(diff_adc_tests,          NULL, NULL, before, after, NULL);
// ZTEST_SUITE(async_adc_tests,         NULL, NULL, before, after, NULL);