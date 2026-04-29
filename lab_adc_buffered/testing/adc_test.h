#ifndef ADC_TEST_H
#define ADC_TEST_H

#include "bme554_lib.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>

/* ------------------------------------------------------------------ */
/*  Thread config — runs student_main in the background               */
/* ------------------------------------------------------------------ */
#define STUDENT_MAIN_STACK_SIZE 4096   /* larger than GPIO lab: ADC + diff buf */
#define STUDENT_MAIN_PRIORITY   5

/* ------------------------------------------------------------------ */
/*  ADC emulator channel IDs (match student's DT overlay)            */
/* ------------------------------------------------------------------ */
#define AIN0_CHANNEL_ID   0   /* single-ended,  read_button   → READING state  */
#define AIN1_CHANNEL_ID   1   /* differential+, read_button → SAMPLE state   */
#define AIN2_CHANNEL_ID   2   /* differential−, read_button → SAMPLE state   */

/* ------------------------------------------------------------------ */
/*  Blink timing constants (mirror student macros)                   */
/* ------------------------------------------------------------------ */
#define BLINKING_TIME_MS   5000
#define MIN_V_MV           0
#define MAX_V_MV           3000
#define MIN_FREQ_HZ        1
#define MAX_FREQ_HZ        5
#define BUFFER_ARRAY_LEN   800

/* ------------------------------------------------------------------ */
/*  Student GPIO pins (defined in student's main.c)                  */
/* ------------------------------------------------------------------ */
// Buttons
extern const struct gpio_dt_spec sample_button;
extern const struct gpio_dt_spec read_button;
extern const struct gpio_dt_spec sleep_button;
extern const struct gpio_dt_spec reset_button;

// LEDs
extern const struct gpio_dt_spec heartbeat_led;
extern const struct gpio_dt_spec blinker_led;
extern const struct gpio_dt_spec error_led;

/* ------------------------------------------------------------------ */
/*  ADC emulator device (defined in student's DT overlay)            */
/* ------------------------------------------------------------------ */
/*
 * The adc_emul device node must appear in the student's DT overlay.
 * Example overlay snippet:
 *
 *   / {
 *       adc_emul: adc_emul {
 *           compatible = "zephyr,adc-emul";
 *           nchannels = <3>;
 *           ref-internal-mv = <3000>;
 *           ref-vdd-mv = <3300>;
 *           #io-channel-cells = <1>;
 *           status = "okay";
 *       };
 *   };
 *
 * And prj.conf must include:
 *   CONFIG_ADC=y
 *   CONFIG_ADC_EMUL=y
 */
#define ADC_EMUL_NODE DT_NODELABEL(adc_emul)

extern int student_main(void);  /* renamed by CMake */

/* ------------------------------------------------------------------ */
/*  Thread boilerplate                                                */
/* ------------------------------------------------------------------ */
K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);
static struct k_thread student_main_thread;
static k_tid_t         student_main_tid;
static volatile bool   main_running = false;

static void student_main_entry(void *, void *, void *);
static void stop_main(void);
static void start_main(int settle_ms);

/* ------------------------------------------------------------------ */
/*  Fixture                                                           */
/* ------------------------------------------------------------------ */
static void before(void *);
static void after(void *);

/* ------------------------------------------------------------------ */
/*  GPIO helpers (identical pattern to gpio_test.h)                  */
/* ------------------------------------------------------------------ */
static volatile int g_led_toggles = 0;
static void led_edge_callback(const struct device *dev,
                              struct gpio_callback *cb,
                              uint32_t pins);
static void assert_led_blink_freq(const struct gpio_dt_spec *led,
                                  int window_ms,
                                  int expected_hz,
                                  int tolerance_hz,
                                  const char *led_name);
static void simulate_button_click(const struct gpio_dt_spec *button);
static void assert_led_off(const struct gpio_dt_spec *led, const char *led_name);
static void assert_led_on(const struct gpio_dt_spec *led, const char *led_name);

/* ------------------------------------------------------------------ */
/*  ADC emulator helpers                                              */
/* ------------------------------------------------------------------ */

/*
 * Sine wave generator state used by the adc_emul value callback.
 * AIN1 is driven with the sine; AIN2 is held at the midpoint DC value
 * so the differential result oscillates cleanly around zero.
 */
struct sine_ctx {
    int freq_hz;            /* signal frequency                           */
    int amplitude_raw;      /* peak amplitude in raw ADC counts           */
    int sample_interval_us; /* matches student's SAMPLE_INTERVAL macro    */
};

static struct sine_ctx g_sine_ain1;

/*
 * set_ain0_mv() — inject a constant voltage on AIN0.
 *   millivolts: 0–3000 (clamped to valid range).
 *   Maps to a raw ADC value using a 12-bit / 3 V scale.
 *
 * Must be called BEFORE simulate_button_click(&read_button).
 */
static void set_ain0_mv(const struct device *adc_emul_dev, int millivolts);

/*
 * set_differential_sine() — configure the adc_emul callback so that
 * AIN1 and AIN2 carry a sinusoidal signal at the given frequency.
 *   freq_hz:        signal frequency (e.g. 10)
 *   amplitude_raw:  peak count (e.g. 2000 for ≈2 V on a 3 V / 12-bit ADC)
 *   sample_iv_us:   must match SAMPLE_INTERVAL in student code
 *
 * Must be called BEFORE simulate_button_click(&read_button).
 */
static void set_differential_sine(const struct device *adc_emul_dev,
                                  int freq_hz,
                                  int amplitude_raw,
                                  int sample_iv_us);

/*
 * assert_blinker_freq() — convenience wrapper targeting the single
 * blinker_led used in the ADC lab (maps to LED1 in the spec).
 */
// static void assert_blinker_freq(int window_ms, int expected_hz, int tolerance_hz);

/*
 * assert_blink_ontime_pct() — verify duty cycle of blinker_led.
 *   Counts high-time vs total period over window_ms.
 *   expected_pct: 0–100 (e.g. 10 for 10% duty cycle per spec).
 *   tolerance_pct: acceptable ± deviation.
 */
// static void assert_blink_ontime_pct(int window_ms, int expected_pct,int tolerance_pct);

/*
 * assert_blink_total_duration_ms() — measure how long blinker_led
 * stays active from first edge to last edge (5-second on-time spec).
 *   expected_ms:  5000
 *   tolerance_ms: acceptable ± deviation (e.g. 200)
 */
static void assert_blink_total_duration_ms(int expected_ms, int tolerance_ms);

/*
 * assert_cycles_computed() — after a SAMPLE state completes, check
 * that the student's calc_cycles() result is within range.
 *   For a 10 Hz signal sampled at SAMPLE_INTERVAL over BUFFER_ARRAY_LEN
 *   samples, the expected number of zero crossings is ~20.
 */
static void assert_cycles_computed(int expected_cycles, int tolerance);

#endif // ADC_TEST_H


/**
 * ==========================================================================
 * TEST SUITE SUMMARY — adc_single_sample_tests  (Phase 1 / v2.0.0)
 * ==========================================================================
 *
 * test_p1_01_read_button_triggers_adc
 *      - Press read_button with AIN0 = 1500 mV
 *      - Verifies: ADC_READ_TRIGGERED_NOTICE fires, then
 *                  ADC_READ_COMPLETE_NOTICE fires with valid mv/freq
 *
 * test_p1_02_zero_volts_maps_to_1hz
 *      - AIN0 = 0 mV → press read_button
 *      - Verifies: blinker_led blinks at ~1 Hz, heartbeat continues at 1 Hz
 *
 * test_p1_03_full_volts_maps_to_5hz
 *      - AIN0 = 3000 mV → press read_button
 *      - Verifies: blinker_led blinks at ~5 Hz
 *
 * test_p1_04_mid_volts_maps_to_3hz
 *      - AIN0 = 1500 mV → press read_button
 *      - Verifies: blinker_led blinks at ~3 Hz
 *
 * test_p1_05_duty_cycle_10pct
 *      - AIN0 = 1500 mV → press read_button
 *      - Verifies: blinker_led duty cycle ≈ 10%
 *
 * test_p1_06_blink_duration_5s
 *      - AIN0 = 1500 mV → press read_button
 *      - Verifies: blinker_led active for ~5000 ms then turns off,
 *                  ADC_BLINK_DONE_NOTICE fires
 *
 * test_p1_07_read_button_disabled_during_blink
 *      - AIN0 = 1500 mV → press read_button → immediately press read_button again
 *      - Verifies: second press produces no second ADC_READ_TRIGGERED_NOTICE
 *
 * test_p1_08_error_on_bad_voltage
 *      - Set AIN0 > 3000 mV (inject out-of-range raw value) → press read_button
 *      - Verifies: ERROR state entered, error_led on, blinker_led off,
 *                  reset_button returns to IDLE
 *
 * test_p1_09_linearity_sweep
 *      - AIN0 = 0, 750, 1500, 2250, 3000 mV → press read_button for each
 *      - Verifies: student_mapped_freq values are monotonically increasing
 *                  and approximately linear (slope ≈ 4/3000 Hz/mV)
 *
 * test_p1_10_heartbeat_unaffected
 *      - AIN0 = 1500 mV → press read_button
 *      - Verifies: heartbeat_led continues at 1 Hz throughout entire
 *                  READING + BLINKING cycle
 *
 * ==========================================================================
 * TEST SUITE SUMMARY — diff_adc_tests  (Phase 2 / v2.1.0)
 * ==========================================================================
 *
 * test_p2_01_read_button_triggers_acquisition
 *      - Configure 10 Hz sine on AIN1/AIN2 → press read_button
 *      - Verifies: ADC_SAMPLE_TRIGGERED_NOTICE fires, then
 *                  ADC_SAMPLE_COMPLETE_NOTICE fires
 *
 * test_p2_02_10hz_sine_detects_correct_cycles
 *      - Inject 10 Hz sine, 800 samples at 2500 µs interval (2 s window)
 *      - Verifies: ADC_CYCLES_COMPUTED fires,
 *                  student_calc_cycles_result ≈ 20 (tolerance ±2)
 *
 * test_p2_03_read_button_disabled_during_acquisition
 *      - Press read_button → immediately press read_button again
 *      - Verifies: second press produces no second ADC_SAMPLE_TRIGGERED_NOTICE
 *
 * test_p2_04_returns_to_idle_after_sample
 *      - Full acquisition cycle (10 Hz sine)
 *      - Verifies: state returns to IDLE (read_button becomes responsive again)
 *
 * test_p2_05_sleep_button_disabled_during_sample
 *      - Press read_button → press sleep_button during acquisition
 *      - Verifies: no SLEEP_TEST_NOTICE fires, sleep is ignored
 *
 * test_p2_06_reset_button_disabled_during_sample
 *      - Press read_button → press reset_button during acquisition
 *      - Verifies: no RESET_TEST_NOTICE fires, reset is ignored during acquisition
 *
 * test_p2_07_dc_signal_zero_cycles
 *      - Inject a flat DC signal (constant positive value) on AIN1/AIN2
 *      - Verifies: student_calc_cycles_result == 0 (no zero crossings)
 *
 * test_p2_08_heartbeat_unaffected
 *      - Full acquisition cycle (10 Hz sine)
 *      - Verifies: heartbeat_led continues at 1 Hz throughout SAMPLE state
 *
 * ==========================================================================
 * TEST SUITE SUMMARY — async_adc_tests  (Phase 3 / v2.2.0)
 * ==========================================================================
 *
 * test_p3_01_async_produces_same_cycle_count
 *      - Same setup as test_p2_02 but using async path
 *      - Verifies: ADC_ASYNC_DONE_NOTICE fires (not just SAMPLE_COMPLETE),
 *                  student_calc_cycles_result ≈ 20 (tolerance ±2)
 *
 * test_p3_02_async_signal_fires
 *      - Inject 10 Hz sine → press read_button
 *      - Verifies: ADC_ASYNC_DONE_NOTICE fires within reasonable timeout
 *
 * test_p3_03_async_timeout_mechanism
 *      - Do NOT configure AIN1/AIN2 emulator (no data ready)
 *      - Verifies: ADC_ASYNC_TIMEOUT_NOTICE fires, state returns to ERROR
 *
 * test_p3_04_async_returns_to_idle
 *      - Full async acquisition cycle (10 Hz sine)
 *      - Verifies: state returns to IDLE after async completes
 *
 * test_p3_05_heartbeat_unaffected
 *      - Full async acquisition cycle
 *      - Verifies: heartbeat_led continues at 1 Hz throughout
 */