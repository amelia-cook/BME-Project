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
#define HEARTBEAT_TOGGLE_INTERVAL_MS 1000

int err = 0;
int LED_STATE = LED_OFF;

K_EVENT_DEFINE(button_events);
#define BUTTON_EVENT BIT(0)

static const struct gpio_dt_spec ledtest = GPIO_DT_SPEC_GET(DT_ALIAS(ledtest), gpios);
static const struct gpio_dt_spec buttontest = GPIO_DT_SPEC_GET(DT_ALIAS(buttontest), gpios);

void button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
static struct gpio_callback button_cb;  

static int init(){

    k_event_init(&button_events);
    // check if interface is ready
    if (!device_is_ready(ledtest.port)) {
        LOG_ERR("gpio0 interface not ready.");  // logging module output
        return -1;  // exit code that will exit main()
    }

    // configure GPIO pin
    err = gpio_pin_configure_dt(&ledtest, GPIO_OUTPUT_ACTIVE);  // ACTIVE referes to ON, not HIGH
    if (err < 0) {
        LOG_ERR("Cannot configure GPIO output pin.");
        return err;
    }

    if (!device_is_ready(buttontest.port)) {
        LOG_ERR("gpio0 interface not ready.");  // logging module output
        return -1;  // exit code that will exit main()
    }

    err = gpio_pin_configure_dt(&buttontest, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Cannot configure sw0 pin.");
        return err;
    }

    err = gpio_pin_interrupt_configure_dt(&buttontest, GPIO_INT_EDGE_TO_ACTIVE);
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw0.");
    }

    gpio_init_callback(&button_cb, button_callback, BIT(buttontest.pin)); // populate CB struct with information about the CB function and pin
    gpio_add_callback_dt(buttontest, &button_cb);

    gpio_pin_set_dt(&ledtest, LED_STATE);

    return 0;
}

int main(void)
{
    int err = init();

    if(err != 0){
        return -1;
    }

    uint32_t events = k_event_wait(&button_events, 0xF, true, K_FOREVER);

    if (events & BUTTON_EVENT) {
        LED_STATE = !LED_STATE;
        gpio_pin_set_dt(&ledtest, LED_STATE);
        k_event_clear(&button_events, BUTTON_EVENT);
        if(LED_STATE == LED_OFF){
            LOG_INF("Button OFF pressed, LED OFF");
        } else {
            LOG_INF("Button ON pressed, LED ON");
        }
    }

    return 0;
}


void button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    k_event_post(&button_events, BUTTON_EVENT);
}