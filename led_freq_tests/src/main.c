#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/printk.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define LED_ON  1
#define LED_OFF 0

#define BLINK_INTERVAL_MS  500   /* half-period: 500 ms ON, 500 ms OFF      */
#define BLINK_DURATION_MS  5000  /* total active time: 5 seconds            */

int LED_STATE = LED_OFF;

static const struct gpio_dt_spec led_test =
    GPIO_DT_SPEC_GET(DT_ALIAS(ledtest), gpios);

static int init(void)
{
    if (!device_is_ready(led_test.port)) {
        LOG_ERR("LED GPIO port not ready.");
        return -ENODEV;
    }

    int err = gpio_pin_configure_dt(&led_test, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        LOG_ERR("Cannot configure LED pin (err %d).", err);
        return err;
    }

    LED_STATE = LED_OFF;
    gpio_pin_set_dt(&led_test, LED_STATE);
    return 0;
}

int main(void)
{
    int err = init();
    if (err != 0) {
        LOG_ERR("Initialisation failed (%d), aborting.", err);
        return err;
    }

    LOG_INF("Starting LED blink: %d ms interval for %d ms total.",
            BLINK_INTERVAL_MS, BLINK_DURATION_MS);

    int64_t start_ms   = k_uptime_get();
    int64_t elapsed_ms = 0;

    while (elapsed_ms < BLINK_DURATION_MS) {
        /* Toggle LED */
        LED_STATE = !LED_STATE;
        gpio_pin_set_dt(&led_test, LED_STATE);

        LOG_DBG("LED %s", LED_STATE ? "ON" : "OFF");

        k_msleep(BLINK_INTERVAL_MS);
        elapsed_ms = k_uptime_get() - start_ms;
    }

    /* Ensure LED is left OFF when we exit */
    LED_STATE = LED_OFF;
    gpio_pin_set_dt(&led_test, LED_STATE);

    LOG_INF("Blinking complete. LED off. Exiting.");
    return 0;
}