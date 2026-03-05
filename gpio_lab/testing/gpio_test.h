#ifndef GPIO_TEST_H
#define GPIO_TEST_H

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
static void assert_led_off(const struct gpio_dt_spec *led);
static void assert_led_on(const struct gpio_dt_spec *led);

#endif // GPIO_TEST_H
