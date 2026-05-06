#ifndef ADC_TEST_H
#define ADC_TEST_H

#include "bme554_lib.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/adc.h>
#include <zephyr/drivers/adc/adc_emul.h>

/* ------------------------------------------------------------------ */
/*  Thread config                                                      */
/* ------------------------------------------------------------------ */
#define STUDENT_MAIN_STACK_SIZE 4096
#define STUDENT_MAIN_PRIORITY   5

/* ------------------------------------------------------------------ */
/*  ADC emulator channel IDs (must match reg = <N> in overlay)        */
/*                                                                     */
/*  vadc_ch   → channel@0  → AIN0_CHANNEL_ID 0  (single-ended read)  */
/*  diffadc_ch → channel@1 → AIN1_CHANNEL_ID 1  (Phase 2 diff +)     */
/*  (no channel@2 in overlay yet)  AIN2_CHANNEL_ID 2  (Phase 2 diff -) */
/* ------------------------------------------------------------------ */
#define AIN0_CHANNEL_ID   0
#define AIN1_CHANNEL_ID   1
#define AIN2_CHANNEL_ID   2

/* ------------------------------------------------------------------ */
/*  Timing / range constants (mirror student macros)                  */
/* ------------------------------------------------------------------ */
#define BLINKING_TIME_MS   5000
#define MIN_V_MV           0
#define MAX_V_MV           3000
#define MIN_FREQ_HZ        1
#define MAX_FREQ_HZ        5
#define BUFFER_ARRAY_LEN   800   /* Phase 2/3 only */

/* ------------------------------------------------------------------ */
/*  Student GPIO pins — defined in student main.c                     */
/*                                                                     */
/*  NOTE: sample_button is a Phase 2 peripheral. It is NOT present    */
/*  in the student's Phase 1 main.c and is NOT declared here.         */
/*  Add it back when building the Phase 2 test suite.                 */
/* ------------------------------------------------------------------ */
extern const struct gpio_dt_spec read_button;
extern const struct gpio_dt_spec sleep_button;
extern const struct gpio_dt_spec reset_button;

extern const struct gpio_dt_spec heartbeat_led;
extern const struct gpio_dt_spec blinker_led;
extern const struct gpio_dt_spec error_led;

/* ------------------------------------------------------------------ */
/*  ADC emulator device node                                          */
/*                                                                     */
/*  Requires in native_sim.overlay:                                   */
/*    adc_emul: adc_emul {                                            */
/*        compatible = "zephyr,adc-emul";                             */
/*        nchannels = <3>;                                            */
/*        ref-internal-mv = <3000>;                                   */
/*        #io-channel-cells = <1>;                                    */
/*        status = "okay";                                            */
/*    };                                                              */
/*                                                                     */
/*  And in prj.conf:                                                  */
/*    CONFIG_ADC=y                                                    */
/*    CONFIG_ADC_EMUL=y                                               */
/* ------------------------------------------------------------------ */
#define ADC_EMUL_NODE DT_NODELABEL(adc_emul)

extern int student_main(void);  /* renamed by CMakeLists: main=student_main */

/* ------------------------------------------------------------------ */
/*  Thread boilerplate                                                 */
/* ------------------------------------------------------------------ */
K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);
static struct k_thread  student_main_thread;
static k_tid_t          student_main_tid;
static volatile bool    main_running = false;

static void student_main_entry(void *, void *, void *);
static void stop_main(void);
static void start_main(int settle_ms);

/* ------------------------------------------------------------------ */
/*  Fixture                                                            */
/* ------------------------------------------------------------------ */
static void before(void *);
static void after(void *);

/* ------------------------------------------------------------------ */
/*  GPIO helpers                                                       */
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
static void assert_led_on(const struct gpio_dt_spec *led,  const char *led_name);

/* ------------------------------------------------------------------ */
/*  ADC emulator helpers                                              */
/* ------------------------------------------------------------------ */

/*
 * Sine wave generator context for Phase 2/3 differential tests.
 * AIN1 is driven with +sine; AIN2 is held at amplitude_raw (midpoint)
 * so the differential buf[i] = AIN1[i] - AIN2[i] oscillates around 0.
 */
struct sine_ctx {
    int freq_hz;
    int amplitude_raw;
    int sample_interval_us;
};

static struct sine_ctx g_sine_ain1;

/*
 * set_ain0_mv() — inject a constant millivolt value on AIN0.
 *
 * raw = (mv / 3000) * 4095, clamped to [0, 4095].
 * Uses a value-callback registered on the adc_emul device so each
 * call to adc_read() returns this constant.
 *
 * Must be called before simulate_button_click(&read_button).
 */
static void set_ain0_mv(const struct device *adc_emul_dev, int millivolts);

/*
 * set_differential_sine() — configure AIN1 and AIN2 for Phase 2/3.
 *   freq_hz:        signal frequency (e.g. 10)
 *   amplitude_raw:  peak count (e.g. 2000 ≈ 2 V on 3 V / 12-bit ADC)
 *   sample_iv_us:   must match SAMPLE_INTERVAL macro in student code
 *
 * Must be called before simulate_button_click(&sample_button).
 */
static void set_differential_sine(const struct device *adc_emul_dev,
                                   int freq_hz,
                                   int amplitude_raw,
                                   int sample_iv_us);

/*
 * assert_blinker_freq() — convenience wrapper for blinker_led.
 */
// static void assert_blinker_freq(int window_ms, int expected_hz,
//                                  int tolerance_hz);

/*
 * assert_blink_ontime_pct() — verify blinker_led duty cycle.
 *   expected_pct:  0–100  (spec says 10%)
 *   tolerance_pct: ±      (suggested 5)
 */
static void assert_blink_ontime_pct(int window_ms, int expected_pct,
                                     int tolerance_pct);

/*
 * assert_blink_total_duration_ms() — measure blinker_led on-time duration.
 *   expected_ms:  5000 (per spec)
 *   tolerance_ms: ±    (suggested 300)
 *
 * Waits for ADC_BLINK_DONE_NOTICE, which fires from ADC_BLINK_COMPLETE()
 * in blinking_exit().
 */
static void assert_blink_total_duration_ms(int expected_ms, int tolerance_ms);

/*
 * assert_cycles_computed() — Phase 2/3 only.
 * Waits for ADC_CYCLES_COMPUTED_NOTICE then checks student_calc_cycles_result.
 */
static void assert_cycles_computed(int expected_cycles, int tolerance);
static bool wait_for_event(uint32_t mask, int timeout_ms);




struct duty_ctx {
    const struct gpio_dt_spec *led;

    int64_t last_ts;
    bool last_state;

    int64_t on_time;
    int64_t total_time;
};
static struct duty_ctx ctx;

#endif /* ADC_TEST_H */