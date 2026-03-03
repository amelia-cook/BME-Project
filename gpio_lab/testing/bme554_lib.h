#ifndef BME554_TESTING_LIB_H
#define BME554_TESTING_LIB_H

#include <zephyr/kernel.h>

/* DECLARE PROGRAM EVENTS FOR TESTING */
#define FREQ_UP_TEST_NOTICE         BIT(0)
#define FREQ_DOWN_TEST_NOTICE       BIT(1)
#define RESET_BTN_TEST_NOTICE       BIT(2)
#define SLEEP_BTN_TEST_NOTICE       BIT(3)
#define ERROR_TEST_NOTICE           BIT(4)
#define RESET_TEST_NOTICE           BIT(5)
#define SLEEP_TEST_NOTICE           BIT(6)

/* GLOBAL VARIABLES */
extern int student_frequency;
extern struct k_event program_test_events;

/* MACROS FOR STUDENTS */
#define FREQUENCY_UP_PRESSED(new_frequency)                         \
{                                                                   \
    do {                                                            \
        student_frequency = (new_frequency);                        \
        k_event_post(&program_test_events, FREQ_UP_TEST_NOTICE);    \
    } while (0);                                                    \
}

#define FREQUENCY_DOWN_PRESSED(new_frequency)                       \
{                                                                   \
    do {                                                            \
        student_frequency = (new_frequency);                        \
        k_event_post(&program_test_events, FREQ_DOWN_TEST_NOTICE);  \
    } while (0);                                                    \
}

#define RESET_PRESSED()                                             \
{                                                                   \
    k_event_post(&program_test_events, RESET_BTN_TEST_NOTICE);      \
}

#define SLEEP_PRESSED()                                             \
{                                                                   \
    k_event_post(&program_test_events, SLEEP_BTN_TEST_NOTICE);      \
}

#define ERROR_STATE()                                              \
{                                                                   \
    k_event_post(&program_test_events, ERROR_TEST_NOTICE);          \
}

#define RESET_STATUS()                                              \
{                                                                   \
    k_event_post(&program_test_events, ERROR_TEST_NOTICE);          \
}

#define SLEEP_STATE()                                              \
{                                                                   \
    k_event_post(&program_test_events, SLEEP_TEST_NOTICE);          \
}

#endif // BME554_TESTING_LIB_H
