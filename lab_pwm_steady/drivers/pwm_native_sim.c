#include <zephyr/drivers/pwm.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(pwm_native_sim, LOG_LEVEL_INF);

/* Driver runtime data */
struct pwm_native_sim_data {
    uint32_t last_period_ns;
    uint32_t last_pulse_ns;
    pwm_flags_t last_flags;
};

/* PWM API implementation */
static int pwm_native_sim_pin_set(const struct device *dev, uint32_t pwm,
                                  uint32_t period_ns, uint32_t pulse_ns,
                                  pwm_flags_t flags)
{
    struct pwm_native_sim_data *data = dev->data;

    data->last_period_ns = period_ns;
    data->last_pulse_ns  = pulse_ns;
    data->last_flags     = flags;

    LOG_INF("PWM channel %d set: period=%u ns, pulse=%u ns, flags=0x%x",
            pwm, period_ns, pulse_ns, flags);

    return 0;
}

/* PWM driver API struct */
static const struct pwm_driver_api pwm_native_sim_api = {
    .pin_set = pwm_native_sim_pin_set,
};

/* Init function */
static int pwm_native_sim_init(const struct device *dev)
{
    LOG_INF("Simulated PWM device '%s' initialized", dev->name);
    return 0;
}

/* Bind to pwm0 node */
DEVICE_DT_DEFINE(DT_NODELABEL(pwm0),
                 pwm_native_sim_init,
                 NULL,
                 NULL,
                 NULL,
                 POST_KERNEL,
                 CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,
                 &pwm_native_sim_api);