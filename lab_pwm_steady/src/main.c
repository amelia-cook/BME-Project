// sample code

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h> 
#include <zephyr/logging/log.h>  // needs CONFIG_LOG=y in your prj.conf
#include <zephyr/drivers/adc.h> // CONFIG_ADC=y
#include <zephyr/drivers/pwm.h> // CONFIG_PWM=y
#include <zephyr/smf.h> // CONFIG_SMF=y

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/printk.h>

/* Use Devicetree alias for PWM */
static const struct pwm_dt_spec pwm1 = PWM_DT_SPEC_GET(DT_ALIAS(pwm1));

void main(void)
{
    if (!device_is_ready(pwm1.dev)) {
        printk("PWM device not ready\n");
        return;
    }

    printk("Setting PWM LED to 50%% duty cycle\n");

    /* Use the period from the Devicetree spec */
    uint32_t period = pwm1.period; 

    /* 50% duty cycle → pulse = period / 2 */
    uint32_t pulse = period / 2;

    /* Set pulse only, keeping period the same */
    pwm_set_pulse_dt(&pwm1, pulse);

    while (1) {
        k_sleep(K_SECONDS(1));  // keep running
    }
}
