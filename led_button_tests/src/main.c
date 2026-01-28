#include <zephyr/kernel.h>
// #include <zephyr/gpio.h>

// #include <zephyr/ztest.h>
#include <zephyr/sys/printk.h>
// #include <zephyr/drivers/adc.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h> 
#include <zephyr/logging/log.h>
// #include <zephyr/drivers/adc/adc_emul.h>

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#define LED_ON 1
#define LED_OFF 0
#define HEARTBEAT_TOGGLE_INTERVAL_MS 500

int err = 0;
int LED_STATE = LED_OFF;

K_EVENT_DEFINE(button_events);
#define BUTTON_EVENT BIT(0)

static const struct gpio_dt_spec led_test = GPIO_DT_SPEC_GET(DT_ALIAS(ledtest), gpios);
static const struct gpio_dt_spec button_test = GPIO_DT_SPEC_GET(DT_ALIAS(buttontest), gpios);

static struct gpio_callback button_test_cb;  

void button_test_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

static int init(){

    k_event_init(&button_events);

    if (!device_is_ready(button_test.port)) {
        LOG_ERR("gpio0 interface not ready.");  // logging module output
        return -1;  // exit code that will exit main()
    }

    err = gpio_pin_configure_dt(&button_test, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Cannot configure sw0 pin.");
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&button_test, GPIO_INT_EDGE_TO_ACTIVE);
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw0.");
    }

    gpio_init_callback(&button_test_cb, button_test_callback, BIT(button_test.pin)); // populate CB struct with information about the CB function and pin
    gpio_add_callback_dt(&button_test, &button_test_cb);

    // configure GPIO pin
    err = gpio_pin_configure_dt(&led_test, GPIO_OUTPUT_ACTIVE);  // ACTIVE referes to ON, not HIGH
    if (err < 0) {
        LOG_ERR("Cannot configure GPIO output pin.");
        return err;
    }

    gpio_pin_set_dt(&led_test, LED_STATE);

    return 0;
}

int main(void)
{
    int err = init();

    if(err != 0){
        return -1;
    }

    uint32_t events = k_event_wait(&button_events, BUTTON_EVENT, true, K_FOREVER);

    if (events & BUTTON_EVENT) {
        LED_STATE = !LED_STATE;
        gpio_pin_set_dt(&led_test, LED_STATE);
        k_event_clear(&button_events, BUTTON_EVENT);
        if(LED_STATE == LED_OFF){
            LOG_INF("Button OFF pressed, LED OFF");
        } else {
            LOG_INF("Button ON pressed, LED ON");
        }
    }

    uint32_t event_2 = k_event_wait(&button_events, BUTTON_EVENT, true, K_FOREVER);

    if (event_2 & BUTTON_EVENT) {
        LED_STATE = !LED_STATE;
        gpio_pin_set_dt(&led_test, LED_STATE);
        k_event_clear(&button_events, BUTTON_EVENT);
        if(LED_STATE == LED_OFF){
            LOG_INF("Button OFF pressed, LED OFF");
        } else {
            LOG_INF("Button ON pressed, LED ON");
        }
    }

    LOG_INF("exiting code");
    return 0;
}


void button_test_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_event_post(&button_events, BUTTON_EVENT);
}