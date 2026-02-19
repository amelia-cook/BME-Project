/**
 * @brief Shared types and declarations for IV Pump state machine ZTests
 *
 * Place this alongside test_state_machine.c in your tests/ folder.
 * Your CMakeLists.txt should rename the student's main() to student_main().
 */

#ifndef TEST_STATE_MACHINE_H
#define TEST_STATE_MACHINE_H

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

/* ------------------------------------------------------------------ */
/*  External references to student globals (exposed for testing)       */
/* ------------------------------------------------------------------ */

/* State machine */
extern int state;
extern int next_state;

/* Button event flags (set by callbacks, cleared by main loop) */
extern bool sleep_button_event;
extern bool up_button_event;
extern bool down_button_event;
extern bool reset_button_event;

/* LED status structs so tests can read toggle_time / illuminated */
struct led_status {
    int64_t toggle_time;
    bool    illuminated;
};
extern struct led_status heartbeat_led_status;
extern struct led_status iv_pump_led_status;

/* Blink frequency controlled by freq_up / freq_down buttons */
extern int action_led_hz;

/* ------------------------------------------------------------------ */
/*  State enum (mirrors student code)                                  */
/* ------------------------------------------------------------------ */
enum states { INIT, BLINKING_ENTRY, BLINKING_RUN, SLEEP, RESET, ERROR };

/* ------------------------------------------------------------------ */
/*  Timing constants (mirrors student macros)                          */
/* ------------------------------------------------------------------ */
#define HEARTBEAT_TOGGLE_INTERVAL_MS 500
#define MS_PER_HZ                    1000
#define LED_BLINK_FREQ_HZ            2
#define MAX_FREQ_HZ                  5
#define MIN_FREQ_HZ                  1

/* ------------------------------------------------------------------ */
/*  Thread config for running student_main in background               */
/* ------------------------------------------------------------------ */
#define STUDENT_MAIN_STACK_SIZE 2048
#define STUDENT_MAIN_PRIORITY   5

extern int student_main(void);  /* renamed by CMake */

#endif /* TEST_STATE_MACHINE_H */