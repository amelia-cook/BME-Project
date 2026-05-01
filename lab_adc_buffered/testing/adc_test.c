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

#define SAMPLE_INTERVAL                 2500

#define SINE_SAMPLES 800

/* ================================================================== */
/*  ADC emulator device handle                                        */
/* ================================================================== */

/*
 * Grabbed once in before() and passed to set_ain0_mv /
 * set_differential_sine helpers.  Avoids repeating DEVICE_DT_GET
 * in every test.
 */
static const struct device *adc_emul_dev;
static volatile int32_t g_sine_sample_idx;

// static uint16_t sine_lut[SINE_SAMPLES];
// static volatile uint32_t idx;

/* ================================================================== */
/*  Fixture                                                           */
/* ================================================================== */

// static const struct device *adc_emul_dev;

static bool wait_for_event(uint32_t mask, int timeout_ms)
{
    int64_t start = k_uptime_get();
    uint32_t events = 0;

    do {
        events = k_event_wait(&program_test_events, mask, false, K_MSEC(20));
        if (events & mask) {
            return true;
        }
    } while ((k_uptime_get() - start) < timeout_ms);

    return false;
}

static void before(void *)
{
    stop_main();
    g_sine_sample_idx = 0;

    gpio_emul_input_set(read_button.port,  read_button.pin,  0);
    gpio_emul_input_set(sleep_button.port, sleep_button.pin, 0);
    gpio_emul_input_set(reset_button.port, reset_button.pin, 0);
    gpio_emul_input_set(sample_button.port, sample_button.pin, 0);

    /* Grab the emulated ADC device */
    adc_emul_dev = DEVICE_DT_GET(ADC_EMUL_NODE);
    zassert_true(device_is_ready(adc_emul_dev), "ADC emulator not ready");

    // adc_emul_channel_reset(adc_emul_dev, AIN1_CHANNEL_ID);
    // adc_emul_channel_reset(adc_emul_dev, AIN2_CHANNEL_ID); 
    
    // for diff zephyr version 
    adc_emul_value_func_set(adc_emul_dev, AIN1_CHANNEL_ID, NULL, NULL);
    adc_emul_value_func_set(adc_emul_dev, AIN2_CHANNEL_ID, NULL, NULL);

    start_main(500);

    /* Clear all event bits */
    k_event_clear(&program_test_events,
        ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE |
        ADC_BLINK_DONE_NOTICE |
        ADC_SAMPLE_TRIGGERED_NOTICE | ADC_SAMPLE_COMPLETE_NOTICE |
        ADC_CYCLES_COMPUTED_NOTICE |
        ADC_ASYNC_DONE_NOTICE | ADC_ASYNC_TIMEOUT_NOTICE);
        
        
        /* FREQ_UP_TEST_NOTICE | FREQ_DOWN_TEST_NOTICE |
        RESET_BTN_TEST_NOTICE | SLEEP_BTN_TEST_NOTICE |
        ERROR_TEST_NOTICE | RESET_TEST_NOTICE | SLEEP_TEST_NOTICE |
        ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE |
        ADC_BLINK_DONE_NOTICE |
        ADC_SAMPLE_TRIGGERED_NOTICE | ADC_SAMPLE_COMPLETE_NOTICE |
        ADC_CYCLES_COMPUTED_NOTICE |
        ADC_ASYNC_DONE_NOTICE | ADC_ASYNC_TIMEOUT_NOTICE);
        */
    
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

// static void assert_led_off(const struct gpio_dt_spec *led, const char *led_name)
// {
//     int val = gpio_emul_output_get(led->port, led->pin);
//     zassert_equal(val, 0,
//         "Expected LED %s on pin %d to be OFF, but it is ON",
//         led_name, led->pin);
// }

// static void assert_led_on(const struct gpio_dt_spec *led, const char *led_name)
// {
//     int val = gpio_emul_output_get(led->port, led->pin);
//     zassert_equal(val, 1,
//         "Expected LED %s on pin %d to be ON, but it is OFF",
//         led_name, led->pin);
// }

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

static int32_t g_ain0_raw_value;

// static int ain0_const_cb(const struct device *dev,
//                          unsigned int chan,
//                          void *data,
//                          uint32_t *result)
// {
//     ARG_UNUSED(dev);
//     ARG_UNUSED(chan);
//     ARG_UNUSED(data);
//     *result = g_ain0_raw_value;
//     return 0;
// }

// void init_sine(int amplitude, int offset)
// {
//     for (int i = 0; i < SINE_SAMPLES; i++) {
//         float s = sinf(2.0f * M_PI * i / SINE_SAMPLES);
//         sine_lut[i] = (uint16_t)(offset + amplitude * s);
//     }
//     idx = 0;
// }

// static int sine_cb(const struct device *dev,
//                    unsigned int chan,
//                    void *data,
//                    uint32_t *result)
// {
//     *result = sine_lut[idx++ % SINE_SAMPLES];
//     return 0;
// }

static int sine_cb(const struct device *dev,
                   unsigned int chan,
                   void *data,
                   uint32_t *result)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);
    ARG_UNUSED(data);

    float t_s = (float)g_sine_sample_idx *
                (float)g_sine_ain1.sample_interval_us / 1e6f;

    float sine_val = sinf(2.0f * M_PI *
                          (float)g_sine_ain1.freq_hz * t_s);

    int16_t raw = (int16_t)(g_sine_ain1.amplitude_raw * sine_val);

    /* IMPORTANT: DO NOT CAST TO uint32_t DIRECTLY */
    *result = (uint32_t)(int32_t)raw;

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
    // int ret = adc_emul_const_value_set(dev, AIN0_CHANNEL_ID, millivolts);

    int ret = adc_emul_const_value_set(dev, AIN0_CHANNEL_ID, millivolts);
    zassert_ok(ret, "adc_emul_const_value_set failed (%d)", ret);
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

static int ain1_sine_cb(const struct device *dev,
                        unsigned int chan,
                        void *data,
                        uint32_t *result)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);
    ARG_UNUSED(data);

    float t_s = (float)g_sine_sample_idx *
                (float)g_sine_ain1.sample_interval_us / 1e6f;

    float sine_val = sinf(2.0f * M_PI * (float)g_sine_ain1.freq_hz * t_s);

    int32_t raw = (int32_t)(sine_val * (g_sine_ain1.amplitude_raw / 2.0f));

    *result = (uint32_t)(2048 + raw);   // keep ADC in valid [0, 4095] range

    g_sine_sample_idx++;   // increment HERE (AIN1 is sampled first)
    return 0;
}

static int ain2_sine_inverted_cb(const struct device *dev,
                                 unsigned int chan,
                                 void *data,
                                 uint32_t *result)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);
    ARG_UNUSED(data);

    // Use g_sine_sample_idx - 1 because AIN1 already incremented it
    float t_s = (float)(g_sine_sample_idx - 1) *
                (float)g_sine_ain1.sample_interval_us / 1e6f;

    float sine_val = sinf(2.0f * M_PI * (float)g_sine_ain1.freq_hz * t_s);

    int32_t raw = (int32_t)(-sine_val * (g_sine_ain1.amplitude_raw / 2.0f));

    *result = (uint32_t)(2048 + raw);

    return 0;
}

static void set_differential_sine(const struct device *dev,
                                  int freq_hz,
                                  int amplitude_raw,
                                  int sample_iv_us)
{
    g_sine_ain1.freq_hz            = freq_hz;
    g_sine_ain1.amplitude_raw      = amplitude_raw;
    g_sine_ain1.sample_interval_us = sample_iv_us;

    g_sine_sample_idx = 0;   // <-- reset counter here

    int ret;
    ret = adc_emul_value_func_set(dev, AIN1_CHANNEL_ID, ain1_sine_cb, NULL);
    zassert_ok(ret, "AIN1 set failed");
    ret = adc_emul_value_func_set(dev, AIN2_CHANNEL_ID, ain2_sine_inverted_cb, NULL);
    zassert_ok(ret, "AIN2 set failed");
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

    // uint32_t events = k_event_wait(&program_test_events,
    //                                ADC_CYCLES_COMPUTED_NOTICE,
    //                                true,
    //                                K_MSEC(timeout_ms));
    // zassert_true(events & ADC_CYCLES_COMPUTED_NOTICE,
    //     "ADC_CYCLES_COMPUTED_NOTICE never fired (timeout %d ms)", timeout_ms);

    // int timeout_ms = (BUFFER_ARRAY_LEN * SAMPLE_INTERVAL) / 1000 + 500;

    zassert_true(wait_for_event(ADC_CYCLES_COMPUTED_NOTICE, 10000),
        "ADC_CYCLES_COMPUTED_NOTICE never fired (timeout %d ms)", 10000);

    zassert_within(student_calc_cycles_result, expected_cycles, tolerance,
        "calc_cycles: expected ~%d but got %d",
        expected_cycles, student_calc_cycles_result);
}

/*
 * DC signal (constant midpoint value) → zero crossings = 0.
 */
static int ain1_dc_cb(const struct device *dev, unsigned int chan, void *data, uint32_t *result)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);
    ARG_UNUSED(data);

    *result = 1500;
    return 0;
}

static int ain2_dc_cb(const struct device *dev, unsigned int chan, void *data, uint32_t *result)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);
    ARG_UNUSED(data);

    *result = 1500;
    return 0;
}


/* ================================================================== */
/*  PHASE 2 TESTS — diff_adc_tests                                   */
/* ================================================================== */

/*
 * Press sample_button and verify the acquisition starts and completes.
 */
ZTEST(diff_adc_tests, test_p2_01_sample_button_triggers_acquisition)
{
    set_differential_sine(adc_emul_dev, 10, 2000, SAMPLE_INTERVAL);
    k_event_clear(&program_test_events, ADC_SAMPLE_TRIGGERED_NOTICE | ADC_SAMPLE_COMPLETE_NOTICE);

    // start_main(500);

    simulate_button_click(&sample_button);

    zassert_true(wait_for_event(ADC_SAMPLE_TRIGGERED_NOTICE, 800),
        "ADC_SAMPLE_TRIGGERED_NOTICE never fired");

    zassert_true(wait_for_event(ADC_SAMPLE_COMPLETE_NOTICE, 10000), "ADC_SAMPLE_COMPLETE_NOTICE never fired");

}

/*
 * 10 Hz sine, 800 samples @ 2500 µs → ~20 cycles.
 * calc_cycles counts negative→positive zero crossings.
 */
ZTEST(diff_adc_tests, test_p2_02_10hz_sine_detects_correct_cycles)
{
    set_differential_sine(adc_emul_dev, 10, 2000, SAMPLE_INTERVAL);
    g_sine_sample_idx = 0; 

    k_event_clear(&program_test_events,
        ADC_SAMPLE_TRIGGERED_NOTICE | ADC_SAMPLE_COMPLETE_NOTICE |
        ADC_CYCLES_COMPUTED_NOTICE);

    // start_main(500);

    simulate_button_click(&sample_button);

    assert_cycles_computed(20, 2); /* ±2 cycles tolerance */
}

/*
 * Second sample_button press during active acquisition must be ignored.
 */
ZTEST(diff_adc_tests, test_p2_03_sample_button_disabled_during_acquisition)
{
    set_differential_sine(adc_emul_dev, 10, 2000, SAMPLE_INTERVAL);
    // start_main(500);

    simulate_button_click(&sample_button);

    /* Confirm first acquisition started */
    // uint32_t events = k_event_wait(&program_test_events,
    //                                ADC_SAMPLE_TRIGGERED_NOTICE,
    //                                true, K_MSEC(300));
    // zassert_true(events & ADC_SAMPLE_TRIGGERED_NOTICE, "First press not detected");
    zassert_true(wait_for_event(ADC_SAMPLE_TRIGGERED_NOTICE, 300), "First press not detected");

    k_event_clear(&program_test_events, ADC_SAMPLE_TRIGGERED_NOTICE);

    /* Second press — should be ignored (interrupt disabled in IDLE exit) */
    simulate_button_click(&sample_button);
    k_msleep(100);

    // events = k_event_wait(&program_test_events,
    //                       ADC_SAMPLE_TRIGGERED_NOTICE,
    //                       false, K_MSEC(100));
    // zassert_false(events & ADC_SAMPLE_TRIGGERED_NOTICE,
    //     "sample_button not disabled: second ADC_SAMPLE_TRIGGERED fired");

    zassert_false(wait_for_event(ADC_SAMPLE_TRIGGERED_NOTICE, 100), "sample_button not disabled: second ADC_SAMPLE_TRIGGERED fired");
}


/*
 * After acquisition completes, state machine returns to IDLE.
 * Verify by checking that a subsequent read_button press triggers
 * ADC_READ_TRIGGERED_NOTICE (only possible from IDLE).
 */
ZTEST(diff_adc_tests, test_p2_04_returns_to_idle_after_sample)
{
    set_differential_sine(adc_emul_dev, 10, 2000, SAMPLE_INTERVAL);
    k_event_clear(&program_test_events, ADC_SAMPLE_TRIGGERED_NOTICE | ADC_SAMPLE_COMPLETE_NOTICE);

    simulate_button_click(&sample_button);

    zassert_true(wait_for_event(ADC_SAMPLE_TRIGGERED_NOTICE, 800),
        "ADC_SAMPLE_TRIGGERED_NOTICE never fired");

    zassert_true(wait_for_event(ADC_SAMPLE_COMPLETE_NOTICE, 15000), "ADC_SAMPLE_COMPLETE_NOTICE never fired");


    k_msleep(1500); /* let state machine settle in IDLE */

    /* Now read_button should be responsive */
    set_ain0_mv(adc_emul_dev, 1500);
    k_event_clear(&program_test_events, ADC_READ_TRIGGERED_NOTICE | ADC_READ_COMPLETE_NOTICE | ADC_CYCLES_COMPUTED_NOTICE);    
    simulate_button_click(&read_button);

    // events = k_event_wait(&program_test_events,
    //                       ADC_READ_TRIGGERED_NOTICE,
    //                       true, K_MSEC(300));
    // zassert_true(events & ADC_READ_TRIGGERED_NOTICE,
    //     "read_button not responsive after SAMPLE→IDLE transition");
    zassert_true(wait_for_event(ADC_READ_TRIGGERED_NOTICE, 1000), "read_button not responsive after SAMPLE→IDLE transition");
}

/*
 * sleep_button presses during SAMPLE state should be ignored.
 */
ZTEST(diff_adc_tests, test_p2_05_sleep_button_disabled_during_sample)
{
    set_differential_sine(adc_emul_dev, 10, 2000, SAMPLE_INTERVAL);
    // start_main(500);

    simulate_button_click(&sample_button);
    zassert_true(wait_for_event(ADC_SAMPLE_TRIGGERED_NOTICE, 300),
        "sample_button first press not detected");

    k_event_clear(&program_test_events, SLEEP_TEST_NOTICE);
    simulate_button_click(&sleep_button);
    k_msleep(100);

    // uint32_t events = k_event_wait(&program_test_events,
    //                                SLEEP_TEST_NOTICE,
    //                                false, K_MSEC(100));
    // zassert_false(events & SLEEP_TEST_NOTICE,
    //     "sleep_button was not disabled during SAMPLE state");
    zassert_false(wait_for_event(SLEEP_TEST_NOTICE, 100),"sleep_button was not disabled during SAMPLE state");
}

/*
 * reset_button presses during SAMPLE state should be ignored.
 */
ZTEST(diff_adc_tests, test_p2_06_reset_button_disabled_during_sample)
{
    set_differential_sine(adc_emul_dev, 10, 2000, SAMPLE_INTERVAL);
    // start_main(500);

    simulate_button_click(&sample_button);
    zassert_true(wait_for_event(ADC_SAMPLE_TRIGGERED_NOTICE, 300),"sample_button first press not detected");

    k_event_clear(&program_test_events, RESET_TEST_NOTICE);
    simulate_button_click(&reset_button);
    k_msleep(100);

    // uint32_t events = k_event_wait(&program_test_events,
    //                                RESET_TEST_NOTICE,
    //                                false, K_MSEC(100));
    // zassert_false(events & RESET_TEST_NOTICE,
    //     "reset_button was not disabled during SAMPLE state");

    zassert_false(wait_for_event(RESET_TEST_NOTICE, 100), "reset_button was not disabled during SAMPLE state");
}

ZTEST(diff_adc_tests, test_p2_07_dc_signal_zero_cycles)
{
    /* AIN1 = constant positive, AIN2 = same constant → diff = 0 always */
    // adc_emul_value_func_set(adc_emul_dev, AIN1_CHANNEL_ID, ain1_dc_cb, NULL);
    // adc_emul_value_func_set(adc_emul_dev, AIN2_CHANNEL_ID, ain1_dc_cb, NULL);
    adc_emul_value_func_set(adc_emul_dev, AIN1_CHANNEL_ID, ain1_dc_cb, NULL);
    adc_emul_value_func_set(adc_emul_dev, AIN2_CHANNEL_ID, ain2_dc_cb, NULL);

    // start_main(500);
    simulate_button_click(&sample_button);

    assert_cycles_computed(0, 1);  /* ±1 just in case of off-by-one */
}

/*
 * Heartbeat continues at 1 Hz throughout the SAMPLE state.
 */
ZTEST(diff_adc_tests, test_p2_08_heartbeat_unaffected)
{
    set_differential_sine(adc_emul_dev, 10, 2000, 2500);
    // start_main(500);

    simulate_button_click(&sample_button);
    k_event_wait(&program_test_events, ADC_SAMPLE_TRIGGERED_NOTICE, true, K_MSEC(300));

    /* Measure heartbeat during the ~2 s acquisition window */
    assert_led_blink_freq(&heartbeat_led, 2000, 1, 1, "heartbeat (during SAMPLE)");
}

/* ================================================================== */
/*  Register suites                                                   */
/* ================================================================== */
ZTEST_SUITE(diff_adc_tests,          NULL, NULL, before, after, NULL);
