#ifndef GPIO_TEST_H
#define GPIO_TEST_H

#include "bme554_lib.h"

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

extern int state;

extern int student_main(void);  /* renamed by CMake */

#endif // GPIO_TEST_H
