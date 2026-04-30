#ifndef BME554_TESTING_LIB_H
#define BME554_TESTING_LIB_H

#include <zephyr/kernel.h>


/* ------------------------------------------------------------------ */
/*  EVENT BITS — GPIO lab (bits 0–6)                                  */
/* ------------------------------------------------------------------ */
#define FREQ_UP_TEST_NOTICE         BIT(0)
#define FREQ_DOWN_TEST_NOTICE       BIT(1)
#define RESET_BTN_TEST_NOTICE       BIT(2)
#define SLEEP_BTN_TEST_NOTICE       BIT(3)
#define ERROR_TEST_NOTICE           BIT(4)
#define RESET_TEST_NOTICE           BIT(5)
#define SLEEP_TEST_NOTICE           BIT(6)

/* ------------------------------------------------------------------ */
/*  EVENT BITS — ADC lab                                              */
/*                                                                    */
/*  Phase 1  (adc_single_sample / v2.0.0)  bits 7–9                  */
/*  Phase 2  (diff_adc          / v2.1.0)  bits 10–12                */
/*  Phase 3  (async_adc         / v2.2.0)  bits 13–14                */
/* ------------------------------------------------------------------ */

/* Phase 1 — single-ended ADC read + LED1 blink */
#define ADC_READ_TRIGGERED_NOTICE   BIT(7)   /* read_button pressed, ADC starting   */
#define ADC_READ_COMPLETE_NOTICE    BIT(8)   /* ADC read finished, freq computed    */
#define ADC_BLINK_DONE_NOTICE       BIT(9)   /* 5-second blink window expired       */

/* Phase 2 — differential buffered ADC + calc_cycles */
#define ADC_SAMPLE_TRIGGERED_NOTICE BIT(10)  /* sample_button pressed, buffer start */
#define ADC_SAMPLE_COMPLETE_NOTICE  BIT(11)  /* full buffer acquired (sync)         */
#define ADC_CYCLES_COMPUTED_NOTICE  BIT(12)  /* calc_cycles() result logged         */

/* Phase 3 — async refactor of Phase 2 */
#define ADC_ASYNC_DONE_NOTICE       BIT(13)  /* k_poll signal received, buffer done */
#define ADC_ASYNC_TIMEOUT_NOTICE    BIT(14)  /* async acquisition timed out         */

/* ------------------------------------------------------------------ */
/*  GLOBAL VARIABLES                                                  */
/* ------------------------------------------------------------------ */
extern int student_frequency;          /* GPIO lab: current action-LED freq (Hz) */

extern int student_adc_mv;            /* ADC lab Ph1: last measured millivolts   */
extern float student_mapped_freq;     /* ADC lab Ph1: mapped blink frequency     */
extern int student_calc_cycles_result;/* ADC lab Ph2/3: result of calc_cycles()  */

extern struct k_event program_test_events;

/* ------------------------------------------------------------------ */
/*  MACROS — GPIO lab (unchanged)                                     */
/* ------------------------------------------------------------------ */
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

#define ERROR_STATE()                                               \
{                                                                   \
    k_event_post(&program_test_events, ERROR_TEST_NOTICE);          \
}

#define RESET_STATUS()                                              \
{                                                                   \
    k_event_post(&program_test_events, RESET_TEST_NOTICE);          \
}

#define SLEEP_STATE()                                               \
{                                                                   \
    k_event_post(&program_test_events, SLEEP_TEST_NOTICE);          \
}

/* ------------------------------------------------------------------ */
/*  MACROS — ADC lab Phase 1                                          */
/*                                                                    */
/*  Drop these into student code at the relevant points:             */
/*    ADC_READ_TRIGGERED()        — top of reading_entry()           */
/*    ADC_READ_COMPLETE(mv, freq) — after freq computed in           */
/*                                  reading_run(), before state chg  */
/*    ADC_BLINK_COMPLETE()        — in blinking_exit() or when       */
/*                                  TIMER_COMPLETE_EVENT fires        */
/* ------------------------------------------------------------------ */
#define ADC_READ_TRIGGERED()                                            \
{                                                                       \
    k_event_post(&program_test_events, ADC_READ_TRIGGERED_NOTICE);      \
}

#define ADC_READ_COMPLETE(mv, freq)                                     \
{                                                                       \
    do {                                                                \
        student_adc_mv = (mv);                                          \
        student_mapped_freq = (freq);                                   \
        k_event_post(&program_test_events, ADC_READ_COMPLETE_NOTICE);   \
    } while (0);                                                        \
}

#define ADC_BLINK_COMPLETE()                                            \
{                                                                       \
    k_event_post(&program_test_events, ADC_BLINK_DONE_NOTICE);          \
}

/* ------------------------------------------------------------------ */
/*  MACROS — ADC lab Phase 2                                          */
/*                                                                    */
/*    ADC_SAMPLE_TRIGGERED()      — top of sample_entry()            */
/*    ADC_SAMPLE_COMPLETE()       — when SAMPLE_COMPLETE_EVENT fires  */
/*                                  in sample_run() (sync path)       */
/*    ADC_CYCLES_COMPUTED(cycles) — after calc_cycles() in           */
/*                                  sample_run(), before state chg    */
/* ------------------------------------------------------------------ */
#define ADC_SAMPLE_TRIGGERED()                                          \
{                                                                       \
    k_event_post(&program_test_events, ADC_SAMPLE_TRIGGERED_NOTICE);    \
    printk("***** ADC_SAMPLE_TRIGGERED *****");                          \
}

#define ADC_SAMPLE_COMPLETE()                                           \
{                                                                       \
    k_event_post(&program_test_events, ADC_SAMPLE_COMPLETE_NOTICE);     \
    printk("***** ADC_SAMPLE_COMPLETE *****");                          \
}

#define ADC_CYCLES_COMPUTED(cycles)                                     \
{                                                                       \
    do {                                                                \
        student_calc_cycles_result = (cycles);                          \
        k_event_post(&program_test_events, ADC_CYCLES_COMPUTED_NOTICE); \
        printk("***** ADC_CYCLES_COMPUTED *****");                          \
    } while (0);                                                        \
}

/* ------------------------------------------------------------------ */
/*  MACROS — ADC lab Phase 3                                          */
/*                                                                    */
/*    ADC_ASYNC_COMPLETE()   — when k_poll signal received            */
/*    ADC_ASYNC_TIMED_OUT()  — when async acquisition times out       */
/* ------------------------------------------------------------------ */
#define ADC_ASYNC_COMPLETE()                                            \
{                                                                       \
    k_event_post(&program_test_events, ADC_ASYNC_DONE_NOTICE);          \
}

#define ADC_ASYNC_TIMED_OUT()                                           \
{                                                                       \
    k_event_post(&program_test_events, ADC_ASYNC_TIMEOUT_NOTICE);       \
}

#endif // BME554_TESTING_LIB_H