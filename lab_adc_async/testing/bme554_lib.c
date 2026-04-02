#include "bme554_lib.h"

K_EVENT_DEFINE(program_test_events);

/* GPIO lab */
int student_frequency;

/* ADC lab — Phase 1 */
int   student_adc_mv;
float student_mapped_freq;

/* ADC lab — Phase 2/3 */
int student_calc_cycles_result;