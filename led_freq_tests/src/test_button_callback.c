/**
 * @file test_led_blink.c
 * @brief Ztest suite for LED blink frequency and duration verification.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * Testing Strategy (mirrors the button-callback test approach)
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * The student's main() is renamed to student_main() by the CMake wrapper so
 * it can coexist with ztest's own main().  We spin student_main() up in a
 * background thread and then observe it from the test thread.
 *
 * Two observation layers are used so tests work in both environments:
 *
 *  1. SOFTWARE LAYER  – watch LED_STATE (always available, no wiring needed)
 *     A high-priority monitor thread samples LED_STATE every
 *     SAMPLE_INTERVAL_MS and records every edge (OFF→ON, ON→OFF) with a
 *     timestamp.  This is sufficient for native_sim.
 *
 *  2. HARDWARE LAYER  – watch the physical GPIO pin via interrupt (real HW)
 *     A second GPIO alias "ledmonitor" is configured as an INPUT with edge
 *     interrupts.  Each interrupt records a timestamp.  On native_sim this
 *     alias is wired to the same emulated pin as "ledtest", giving a true
 *     pin-level test.  See boards/ overlay notes below.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * Board overlay notes
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * For native_sim add to boards/native_sim.overlay (or native_sim_64.overlay):
 *
 *   / {
 *       aliases {
 *           ledtest     = &gpio_emu_led;
 *           ledmonitor  = &gpio_emu_led;   // same pin → loopback
 *       };
 *   };
 *
 * For real hardware (e.g. nRF52840 DK) wire a jumper from the LED pin to an
 * unused GPIO input pin and map that pin to the "ledmonitor" alias.
 *
 * If "ledmonitor" is not available in your overlay the hardware-layer tests
 * are automatically skipped; the software-layer tests still run.
 *
 * ═══════════════════════════════════════════════════════════════════════════
 * Test list
 * ═══════════════════════════════════════════════════════════════════════════
 *
 * Part 1 – Software layer (LED_STATE variable):
 *   test_led_starts_off          : LED_STATE is OFF before main() runs
 *   test_led_toggles_at_all      : at least one toggle observed
 *   test_led_toggle_count        : exactly EXPECTED_TOGGLES toggles
 *   test_led_half_period_timing  : each half-period within tolerance
 *   test_led_total_duration      : blinking window ≈ BLINK_DURATION_MS
 *   test_led_ends_off            : LED_STATE is OFF after main() exits
 *
 * Part 2 – Hardware layer (GPIO pin via interrupt):
 *   test_pin_toggles_at_all      : at least one interrupt fired
 *   test_pin_toggle_count        : interrupt count matches expected
 *   test_pin_half_period_timing  : interrupt-to-interrupt time within tol.
 *   test_pin_total_duration      : first-to-last interrupt span ≈ duration
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "test_led_blink.h"

/* ══════════════════════════════════════════════════════════════════════════
 * Compile-time knobs
 * ══════════════════════════════════════════════════════════════════════════ */

#define STUDENT_MAIN_STACK_SIZE  2048
#define STUDENT_MAIN_PRIORITY    5

/* How often the software monitor thread samples LED_STATE (ms) */
#define SAMPLE_INTERVAL_MS       10

/* Maximum number of edge events we record (software + hardware layers) */
#define MAX_EDGES                64

/* Extra margin we wait after BLINK_DURATION_MS before asserting "done" */
#define POST_BLINK_MARGIN_MS     500

/* ══════════════════════════════════════════════════════════════════════════
 * student_main() thread
 * ══════════════════════════════════════════════════════════════════════════ */

K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);
static struct k_thread student_main_thread;
static k_tid_t         student_main_tid;
static volatile bool   main_is_running = false;

extern int student_main(void);

static void student_main_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);
    main_is_running = true;
    student_main();
    main_is_running = false;
}

/* ══════════════════════════════════════════════════════════════════════════
 * Software-layer edge recorder
 * Records timestamps each time LED_STATE changes value.
 * ══════════════════════════════════════════════════════════════════════════ */

static int64_t sw_edge_times_ms[MAX_EDGES];  /* timestamp of each edge       */
static int     sw_edge_count;                /* how many edges recorded      */
static int     sw_edge_values[MAX_EDGES];    /* LED_STATE value after edge   */

/* Monitor thread samples LED_STATE at SAMPLE_INTERVAL_MS */
#define MONITOR_STACK_SIZE 512
#define MONITOR_PRIORITY   3   /* higher priority than student_main */

K_THREAD_STACK_DEFINE(monitor_stack, MONITOR_STACK_SIZE);
static struct k_thread monitor_thread;
static volatile bool   monitor_running = false;

static void monitor_thread_entry(void *p1, void *p2, void *p3)
{
    ARG_UNUSED(p1); ARG_UNUSED(p2); ARG_UNUSED(p3);

    int last_state = LED_STATE;
    monitor_running = true;

    while (monitor_running) {
        k_msleep(SAMPLE_INTERVAL_MS);

        int current = LED_STATE;
        if (current != last_state && sw_edge_count < MAX_EDGES) {
            sw_edge_times_ms[sw_edge_count] = k_uptime_get();
            sw_edge_values[sw_edge_count]   = current;
            sw_edge_count++;
            last_state = current;
        }
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * Hardware-layer edge recorder (GPIO interrupt on "ledmonitor" alias)
 * ══════════════════════════════════════════════════════════════════════════ */

#if DT_HAS_ALIAS(ledmonitor)

static const struct gpio_dt_spec led_monitor_pin =
    GPIO_DT_SPEC_GET(DT_ALIAS(ledmonitor), gpios);

static struct gpio_callback monitor_gpio_cb;

static int64_t hw_edge_times_ms[MAX_EDGES];
static int     hw_edge_count;

static void gpio_monitor_callback(const struct device *dev,
                                  struct gpio_callback *cb,
                                  uint32_t pins)
{
    ARG_UNUSED(dev); ARG_UNUSED(cb); ARG_UNUSED(pins);
    if (hw_edge_count < MAX_EDGES) {
        hw_edge_times_ms[hw_edge_count++] = k_uptime_get();
    }
}

static bool hw_layer_available = false;

static int init_hw_monitor(void)
{
    if (!device_is_ready(led_monitor_pin.port)) {
        printk("[HW monitor] GPIO port not ready – skipping HW tests.\n");
        return -ENODEV;
    }

    int err = gpio_pin_configure_dt(&led_monitor_pin, GPIO_INPUT);
    if (err < 0) {
        printk("[HW monitor] configure failed (%d)\n", err);
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&led_monitor_pin,
                                          GPIO_INT_EDGE_BOTH);
    if (err < 0) {
        printk("[HW monitor] interrupt configure failed (%d)\n", err);
        return err;
    }

    gpio_init_callback(&monitor_gpio_cb, gpio_monitor_callback,
                       BIT(led_monitor_pin.pin));
    gpio_add_callback_dt(&led_monitor_pin, &monitor_gpio_cb);

    hw_layer_available = true;
    printk("[HW monitor] GPIO interrupt monitor active.\n");
    return 0;
}

#else
/* "ledmonitor" alias not present – HW layer disabled at compile time */
static bool hw_layer_available = false;
static int64_t hw_edge_times_ms[MAX_EDGES];
static int     hw_edge_count = 0;
static int init_hw_monitor(void) { return -ENOTSUP; }
#endif /* DT_HAS_ALIAS(ledmonitor) */

/* ══════════════════════════════════════════════════════════════════════════
 * Helper: start a fresh student_main() run plus the monitor thread
 * ══════════════════════════════════════════════════════════════════════════ */

static void start_student_main_with_monitor(void)
{
    /* Reset software-layer state */
    sw_edge_count = 0;
    memset(sw_edge_times_ms, 0, sizeof(sw_edge_times_ms));
    memset(sw_edge_values,   0, sizeof(sw_edge_values));

    /* Reset hardware-layer state */
    hw_edge_count = 0;
    memset(hw_edge_times_ms, 0, sizeof(hw_edge_times_ms));

    /* Start monitor thread BEFORE student_main so we don't miss early edges */
    monitor_running = true;
    k_thread_create(&monitor_thread,
                    monitor_stack,
                    K_THREAD_STACK_SIZEOF(monitor_stack),
                    monitor_thread_entry,
                    NULL, NULL, NULL,
                    MONITOR_PRIORITY, 0, K_NO_WAIT);

    /* Give monitor a moment to start */
    k_msleep(20);

    /* Start student_main */
    student_main_tid = k_thread_create(&student_main_thread,
                                       student_main_stack,
                                       K_THREAD_STACK_SIZEOF(student_main_stack),
                                       student_main_thread_entry,
                                       NULL, NULL, NULL,
                                       STUDENT_MAIN_PRIORITY, 0, K_NO_WAIT);
}

static void wait_for_blink_to_finish(void)
{
    /* Wait for the full blink window plus a safety margin */
    k_msleep(BLINK_DURATION_MS + POST_BLINK_MARGIN_MS);

    /* Stop the software monitor */
    monitor_running = false;
    k_msleep(SAMPLE_INTERVAL_MS * 2);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Test fixture
 * ══════════════════════════════════════════════════════════════════════════ */

static void test_before(void *fixture)
{
    ARG_UNUSED(fixture);

    /* Kill any leftover threads from a previous test */
    if (main_is_running) {
        k_thread_abort(student_main_tid);
        k_msleep(50);
    }
    if (monitor_running) {
        monitor_running = false;
        k_msleep(50);
    }

    LED_STATE = LED_OFF;
}

static void test_after(void *fixture)
{
    ARG_UNUSED(fixture);

    if (main_is_running) {
        k_thread_abort(student_main_tid);
        k_msleep(50);
    }
    if (monitor_running) {
        monitor_running = false;
        k_msleep(50);
    }
}

/* ══════════════════════════════════════════════════════════════════════════
 * PART 1 – Software-layer tests (LED_STATE variable)
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief LED_STATE must be OFF before student_main() is called.
 */
ZTEST(led_blink_tests, test_led_starts_off)
{
    zassert_equal(LED_STATE, LED_OFF,
                  "LED_STATE should be LED_OFF before main() runs, got %d",
                  LED_STATE);
    printk("✓ LED starts OFF\n");
}

/**
 * @brief At least one toggle must be observed (basic sanity).
 */
ZTEST(led_blink_tests, test_led_toggles_at_all)
{
    start_student_main_with_monitor();
    wait_for_blink_to_finish();

    zassert_true(sw_edge_count > 0,
                 "No LED_STATE changes detected — is main() toggling the LED?");
    printk("✓ Detected %d software-layer edge(s)\n", sw_edge_count);
}

/**
 * @brief The number of LED_STATE toggles must equal EXPECTED_TOGGLES.
 *
 * With BLINK_INTERVAL_MS=500 and BLINK_DURATION_MS=5000:
 *   expected = 5000 / 500 = 10 toggles
 *
 * We allow ±1 toggle of slack for the final partial interval near the
 * 5-second boundary.
 */
ZTEST(led_blink_tests, test_led_toggle_count)
{
    start_student_main_with_monitor();
    wait_for_blink_to_finish();

    printk("Expected %d toggles, detected %d\n",
           EXPECTED_TOGGLES, sw_edge_count);

    zassert_between_inclusive(sw_edge_count,
                              EXPECTED_TOGGLES - 1,
                              EXPECTED_TOGGLES + 1,
                              "Toggle count %d is outside acceptable range [%d, %d]",
                              sw_edge_count,
                              EXPECTED_TOGGLES - 1,
                              EXPECTED_TOGGLES + 1);
    printk("✓ Toggle count correct (%d)\n", sw_edge_count);
}

/**
 * @brief Each half-period (time between consecutive edges) must be
 *        within TIMING_TOLERANCE_MS of BLINK_INTERVAL_MS.
 */
ZTEST(led_blink_tests, test_led_half_period_timing)
{
    start_student_main_with_monitor();
    wait_for_blink_to_finish();

    zassert_true(sw_edge_count >= 2,
                 "Need at least 2 edges to measure timing, got %d",
                 sw_edge_count);

    int64_t min_half_period = INT64_MAX;
    int64_t max_half_period = 0;
    bool    all_within_tolerance = true;

    for (int i = 1; i < sw_edge_count; i++) {
        int64_t half_period = sw_edge_times_ms[i] - sw_edge_times_ms[i - 1];

        if (half_period < min_half_period) min_half_period = half_period;
        if (half_period > max_half_period) max_half_period = half_period;

        int64_t deviation = half_period - BLINK_INTERVAL_MS;
        if (deviation < 0) deviation = -deviation;

        printk("  Edge %d→%d: %lld ms (expected %d ms, deviation %lld ms)\n",
               i - 1, i, (long long)half_period,
               BLINK_INTERVAL_MS, (long long)deviation);

        if (deviation > TIMING_TOLERANCE_MS) {
            all_within_tolerance = false;
            printk("  ✗ Deviation %lld ms exceeds tolerance %d ms!\n",
                   (long long)deviation, TIMING_TOLERANCE_MS);
        }
    }

    printk("Half-period range: %lld – %lld ms (target %d ± %d ms)\n",
           (long long)min_half_period, (long long)max_half_period,
           BLINK_INTERVAL_MS, TIMING_TOLERANCE_MS);

    zassert_true(all_within_tolerance,
                 "One or more half-periods exceeded the %d ms tolerance",
                 TIMING_TOLERANCE_MS);
    printk("✓ Half-period timing within tolerance\n");
}

/**
 * @brief The total blink window (first edge to last edge) must be
 *        approximately BLINK_DURATION_MS.
 *
 * Acceptable range: [BLINK_DURATION_MS - BLINK_INTERVAL_MS,
 *                    BLINK_DURATION_MS + BLINK_INTERVAL_MS]
 *
 * We use one full half-period as the window tolerance because the last
 * toggle may land slightly before or after the exact 5-second mark.
 */
ZTEST(led_blink_tests, test_led_total_duration)
{
    start_student_main_with_monitor();
    wait_for_blink_to_finish();

    zassert_true(sw_edge_count >= 2,
                 "Need at least 2 edges to measure duration, got %d",
                 sw_edge_count);

    int64_t measured_duration =
        sw_edge_times_ms[sw_edge_count - 1] - sw_edge_times_ms[0];

    int64_t lower = BLINK_DURATION_MS - BLINK_INTERVAL_MS;
    int64_t upper = BLINK_DURATION_MS + BLINK_INTERVAL_MS;

    printk("Measured blink duration: %lld ms (expected ~%d ms, window [%lld, %lld])\n",
           (long long)measured_duration,
           BLINK_DURATION_MS,
           (long long)lower, (long long)upper);

    zassert_between_inclusive(measured_duration, lower, upper,
                              "Blink duration %lld ms is outside [%lld, %lld]",
                              (long long)measured_duration,
                              (long long)lower, (long long)upper);
    printk("✓ Total blink duration correct\n");
}

/**
 * @brief After main() returns, LED_STATE must be LED_OFF.
 */
ZTEST(led_blink_tests, test_led_ends_off)
{
    start_student_main_with_monitor();
    wait_for_blink_to_finish();

    /* Give student_main a moment to finish its cleanup */
    k_msleep(100);

    zassert_equal(LED_STATE, LED_OFF,
                  "LED_STATE should be LED_OFF after main() exits, got %d",
                  LED_STATE);
    printk("✓ LED ends OFF after blink sequence\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * PART 2 – Hardware-layer tests (GPIO pin via interrupt)
 *
 * These tests are functionally identical to Part 1 but use hw_edge_times_ms[]
 * populated by the GPIO interrupt callback instead of the polling monitor.
 * They verify the physical pin, not just the software variable.
 *
 * If the "ledmonitor" alias is absent (hw_layer_available == false) every
 * hardware test prints a skip notice and passes unconditionally so the
 * overall suite is still green on native_sim without a loopback overlay.
 * ══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief At least one GPIO interrupt must fire (pin is actually toggling).
 */
ZTEST(led_blink_tests, test_pin_toggles_at_all)
{
    if (!hw_layer_available) {
        printk("[SKIP] ledmonitor alias not present – hardware test skipped.\n");
        ztest_test_skip();
        return;
    }

    start_student_main_with_monitor();
    wait_for_blink_to_finish();

    zassert_true(hw_edge_count > 0,
                 "No GPIO interrupts detected on ledmonitor pin");
    printk("✓ Detected %d hardware-layer edge(s) on pin\n", hw_edge_count);
}

/**
 * @brief GPIO interrupt count must match EXPECTED_TOGGLES (±1 tolerance).
 */
ZTEST(led_blink_tests, test_pin_toggle_count)
{
    if (!hw_layer_available) {
        printk("[SKIP] ledmonitor alias not present – hardware test skipped.\n");
        ztest_test_skip();
        return;
    }

    start_student_main_with_monitor();
    wait_for_blink_to_finish();

    printk("Expected %d pin edges, detected %d\n",
           EXPECTED_TOGGLES, hw_edge_count);

    zassert_between_inclusive(hw_edge_count,
                              EXPECTED_TOGGLES - 1,
                              EXPECTED_TOGGLES + 1,
                              "Pin edge count %d outside [%d, %d]",
                              hw_edge_count,
                              EXPECTED_TOGGLES - 1,
                              EXPECTED_TOGGLES + 1);
    printk("✓ Pin edge count correct (%d)\n", hw_edge_count);
}

/**
 * @brief Time between consecutive GPIO interrupts must equal BLINK_INTERVAL_MS
 *        within TIMING_TOLERANCE_MS.
 */
ZTEST(led_blink_tests, test_pin_half_period_timing)
{
    if (!hw_layer_available) {
        printk("[SKIP] ledmonitor alias not present – hardware test skipped.\n");
        ztest_test_skip();
        return;
    }

    start_student_main_with_monitor();
    wait_for_blink_to_finish();

    zassert_true(hw_edge_count >= 2,
                 "Need at least 2 pin edges to measure timing, got %d",
                 hw_edge_count);

    bool all_within_tolerance = true;

    for (int i = 1; i < hw_edge_count; i++) {
        int64_t half_period = hw_edge_times_ms[i] - hw_edge_times_ms[i - 1];
        int64_t deviation   = half_period - BLINK_INTERVAL_MS;
        if (deviation < 0) deviation = -deviation;

        printk("  Pin edge %d→%d: %lld ms (deviation %lld ms)\n",
               i - 1, i, (long long)half_period, (long long)deviation);

        if (deviation > TIMING_TOLERANCE_MS) {
            all_within_tolerance = false;
        }
    }

    zassert_true(all_within_tolerance,
                 "One or more pin half-periods exceeded tolerance %d ms",
                 TIMING_TOLERANCE_MS);
    printk("✓ Pin half-period timing within tolerance\n");
}

/**
 * @brief Total span from first to last GPIO interrupt must be ≈ BLINK_DURATION_MS.
 */
ZTEST(led_blink_tests, test_pin_total_duration)
{
    if (!hw_layer_available) {
        printk("[SKIP] ledmonitor alias not present – hardware test skipped.\n");
        ztest_test_skip();
        return;
    }

    start_student_main_with_monitor();
    wait_for_blink_to_finish();

    zassert_true(hw_edge_count >= 2,
                 "Need at least 2 pin edges to measure duration");

    int64_t measured = hw_edge_times_ms[hw_edge_count - 1] - hw_edge_times_ms[0];
    int64_t lower    = BLINK_DURATION_MS - BLINK_INTERVAL_MS;
    int64_t upper    = BLINK_DURATION_MS + BLINK_INTERVAL_MS;

    printk("Pin blink duration: %lld ms (expected ~%d ms)\n",
           (long long)measured, BLINK_DURATION_MS);

    zassert_between_inclusive(measured, lower, upper,
                              "Pin blink duration %lld ms outside [%lld, %lld]",
                              (long long)measured,
                              (long long)lower, (long long)upper);
    printk("✓ Pin total duration correct\n");
}

/* ══════════════════════════════════════════════════════════════════════════
 * Suite registration
 * ══════════════════════════════════════════════════════════════════════════ */

ZTEST_SUITE(led_blink_tests, NULL, NULL, test_before, test_after, NULL);