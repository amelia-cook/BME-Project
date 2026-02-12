/**
 * @file test_button_callback.h
 * @brief Header file exposing symbols from main.c for unit testing
 * 
 * This header allows the test file to access functions and variables
 * defined in main.c without modifying the student's code.
 * 
 * IMPORTANT: This file is for testing purposes only. Students should
 * not need to modify this file.
 */

#ifndef TEST_BUTTON_CALLBACK_H
#define TEST_BUTTON_CALLBACK_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* External declarations - these symbols are defined in main.c */
extern struct k_event button_events;
extern int LED_STATE;
extern struct gpio_dt_spec led_test;
extern struct gpio_dt_spec button_test;
extern int student_main(void);

/* LED state macros (from main.c) */
#define LED_ON 1
#define LED_OFF 0

/* Macro for button event bit */
#define BUTTON_EVENT BIT(0)

/* Function declaration - callback function from main.c */
extern void button_test_callback(const struct device *dev, 
                                 struct gpio_callback *cb, 
                                 uint32_t pins);

#endif /* TEST_BUTTON_CALLBACK_H */
