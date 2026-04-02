#ifndef KERNEL_TEST_H
#define KERNEL_TEST_H

#include "bme554_lib.h"
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#define HB_LED_FREQ 1

/* ------------------------------------------------------------------ */
/*  Thread config for running student_main in background              */
/* ------------------------------------------------------------------ */
#define STUDENT_MAIN_STACK_SIZE 2048
#define STUDENT_MAIN_PRIORITY   5

/* ------------------------------------------------------------------ */
/*  Student GPIOs                                                     */
/* ------------------------------------------------------------------ */
// buttons
extern const struct gpio_dt_spec sleep_button;
extern const struct gpio_dt_spec freq_up_button;
extern const struct gpio_dt_spec freq_down_button;
extern const struct gpio_dt_spec reset_button;

// LEDs
extern const struct gpio_dt_spec heartbeat_led;
extern const struct gpio_dt_spec iv_pump_led;
extern const struct gpio_dt_spec buzzer_led;
extern const struct gpio_dt_spec error_led;

extern int student_main(void);  /* renamed by CMake */

/* ------------------------------------------------------------------ */
/*  OUR FUCTIONS                                                      */
/* ------------------------------------------------------------------ */
// fixture
static void before(void *);
static void after(void *);

// thread boilerplate
K_THREAD_STACK_DEFINE(student_main_stack, STUDENT_MAIN_STACK_SIZE);
static struct k_thread student_main_thread;
static k_tid_t         student_main_tid;
static volatile bool   main_running = false;
static void student_main_entry(void *, void *, void *);
static void stop_main(void);
static void start_main(int settle_ms);

// helpers
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
static bool is_led_on(const struct gpio_dt_spec *led);
static void assert_led_duty_cycle_25(const struct gpio_dt_spec *led,
                                     const char *led_name);

#endif // KERNEL_TEST_H



/**
 * TEST SUITE SUMMARY - state_machine_tests
 *
 * test_01_default_frequencies
 *      - No button presses
 *      - Verifies: heartbeat at 1Hz, iv_pump and buzzer at 2Hz, error LED off
 *
 * test_02_freq_up_one
 *      - Freq up x1
 *      - Verifies: action LEDs increase to 3Hz, heartbeat unchanged, error LED off
 *
 * test_03_freq_down_once
 *      - Freq down x1
 *      - Verifies: action LEDs decrease to 1Hz, heartbeat unchanged, error LED off
 *
 * test_04_freq_up_reset
 *      - Freq up x1 -> reset
 *      - Verifies: reset returns action LEDs to default 2Hz from above default
 *
 * test_05_freq_down_reset
 *      - Freq down x1 -> reset
 *      - Verifies: reset returns action LEDs to default 2Hz from below default
 *
 * test_06_sleep
 *      - Sleep
 *      - Verifies: action LEDs turn off, heartbeat continues, error LED off
 *
 * test_07_sleep_sleep
 *      - Sleep -> sleep
 *      - Verifies: second sleep press resumes action LEDs at default 2Hz
 *
 * test_08_freq_up_sleep_sleep
 *      - Freq up x1 -> sleep -> sleep
 *      - Verifies: frequency is preserved (3Hz) after sleep/wake cycle
 *
 * test_09_freq_up_sleep_reset
 *      - Freq up x1 -> sleep -> reset
 *      - Verifies: reset from sleep state returns action LEDs to default 2Hz
 *
 * test_10_freq_down_twice
 *      - Freq down x2
 *      - Verifies: going below min frequency (1Hz) triggers error state,
 *                  action LEDs off, error LED on, heartbeat continues
 *
 * test_11_freq_up_four
 *      - Freq up x4
 *      - Verifies: going above max frequency (5Hz) triggers error state,
 *                  action LEDs off, error LED on, heartbeat continues
 *
 * test_12_freq_down_twice_reset
 *      - Freq down x2 -> reset
 *      - Verifies: reset exits error state, returns action LEDs to default 2Hz,
 *                  error LED turns off
 *
 * test_13_freq_up_four_reset
 *      - Freq up x4 -> reset
 *      - Verifies: reset exits error state entered from above, returns action
 *                  LEDs to default 2Hz, error LED turns off
 *
 * test_14_sleep_reset
 *      - Sleep -> reset
 *      - Verifies: reset works from sleep state, returns action LEDs to default 2Hz
 *
 * test_15_freq_up_sleep_buttons_disabled
 *      - Freq up x1 -> sleep -> freq up -> freq down -> sleep
 *      - Verifies: freq_up and freq_down are disabled during sleep, frequency
 *                  is unchanged (3Hz) after wake
 */