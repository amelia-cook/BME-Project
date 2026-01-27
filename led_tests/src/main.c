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

static const struct gpio_dt_spec ledtest = GPIO_DT_SPEC_GET(DT_ALIAS(ledtest), gpios);
int err = 0;

static int init(){
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
    
    return 0;
}

static void run(){
    gpio_pin_set_dt(&ledtest, LED_ON);
    printk("LED ON");
    k_msleep(HEARTBEAT_TOGGLE_INTERVAL_MS);

    gpio_pin_set_dt(&ledtest, LED_OFF);
    printk("LED OFF");
    k_msleep(HEARTBEAT_TOGGLE_INTERVAL_MS);
}

int main(void)
{
    int err = init();

    if(err != 0){
        return -1;
    }

    for(int i = 0; i<5 ; i++){
        run();
    }

    return 0;
}
