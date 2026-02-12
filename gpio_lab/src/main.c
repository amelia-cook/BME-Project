#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h> 
#include <zephyr/logging/log.h>  // needs CONFIG_LOG=y in your prj.conf
// #include <zephyr/drivers/adc.h> // CONFIG_ADC=y
// #include <zephyr/drivers/pwm.h> // CONFIG_PWM=y
// #include <zephyr/smf.h> // CONFIG_SMF=y

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* DEFINE MACROS */
#define HEARTBEAT_TOGGLE_INTERVAL_MS 500
#define MS_PER_HZ                    1000
#define LED_BLINK_FREQ_HZ            2
#define FREQ_UP_INC_HZ               1
#define FREQ_DOWN_INC_HZ             1
#define MAX_FREQ_HZ                  5
#define MIN_FREQ_HZ                  1

/* DEFINE GLOBALS AND DT-BASED HARDWARE STRUCTS */
// buttons
const struct gpio_dt_spec sleep_button = GPIO_DT_SPEC_GET(DT_ALIAS(sleepbutton), gpios);
const struct gpio_dt_spec freq_up_button = GPIO_DT_SPEC_GET(DT_ALIAS(frequpbutton), gpios);
const struct gpio_dt_spec freq_down_button = GPIO_DT_SPEC_GET(DT_ALIAS(freqdownbutton), gpios);
const struct gpio_dt_spec reset_button = GPIO_DT_SPEC_GET(DT_ALIAS(resetbutton), gpios);

// LEDs
const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(heartbeat), gpios);
const struct gpio_dt_spec iv_pump_led = GPIO_DT_SPEC_GET(DT_ALIAS(ivpump), gpios);
const struct gpio_dt_spec buzzer_led = GPIO_DT_SPEC_GET(DT_ALIAS(buzzer), gpios);
const struct gpio_dt_spec error_led = GPIO_DT_SPEC_GET(DT_ALIAS(error), gpios);

/* DEFINE CALLBACK FUNCTIONS */
void sleep_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void freq_up_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void freq_down_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void reset_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

/* INITIALIZE GPIO CALLBACK STRUCTS */
static struct gpio_callback sleep_button_cb;
static struct gpio_callback freq_up_button_cb;
static struct gpio_callback freq_down_button_cb;
static struct gpio_callback reset_button_cb;

/* DEFINE STATES FOR STATE MACHINE */
enum states { INIT, BLINKING_ENTRY, BLINKING_RUN, SLEEP, RESET, ERROR };

/* DEFINE LED INFORMATION STRUCTS */
struct led {
    int64_t toggle_time;
    bool illuminated;
};
struct led heartbeat_led_status = {
    .toggle_time = 0,
    .illuminated = true
};
struct led iv_pump_led_status = {
    .toggle_time = 0,
    .illuminated = true
};

/* DECLARE FUNCTION PROTOTYPES */
int heartbeat();
int actionLEDs();

/* DECLARE GLOBAL VARIABLES */
int state = INIT;
int next_state = INIT;
int64_t current_time;
int action_led_hz = LED_BLINK_FREQ_HZ;
bool sleep_button_event = 0;
bool up_button_event = 0;
bool down_button_event = 0;
bool reset_button_event = 0;

int main(void)
{
    while (1) {
        current_time = k_uptime_get();
        int err = 0;
        
        switch (state) {
            case INIT:
                // check if interface is ready
                if (!device_is_ready(sleep_button.port)) {
                    LOG_ERR("gpio0 interface not ready.");
                    return -1;
                }
                
                /* CONFIGURE BUTTON GPIO PINS */
                err = gpio_pin_configure_dt(&sleep_button, GPIO_INPUT);
                if (err < 0) {
                    LOG_ERR("Cannot configure sleep button.");
                    return err;
                }
                err = gpio_pin_configure_dt(&freq_up_button, GPIO_INPUT);
                if (err < 0) {
                    LOG_ERR("Cannot configure freq_up button.");
                    return err;
                }
                err = gpio_pin_configure_dt(&freq_down_button, GPIO_INPUT);
                if (err < 0) {
                    LOG_ERR("Cannot configure freq_down button.");
                    return err;
                }
                err = gpio_pin_configure_dt(&reset_button, GPIO_INPUT);
                if (err < 0) {
                    LOG_ERR("Cannot configure reset button.");
                    return err;
                }
                
                /* CONFIGURE BUTTON CALLBACKS */
                // sleep
                err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw0.");
                }
                gpio_init_callback(&sleep_button_cb, sleep_button_callback, BIT(sleep_button.pin));
                err = gpio_add_callback_dt(&sleep_button, &sleep_button_cb);
                if (err < 0) {
                    LOG_ERR("Cannot add callback to sw0.");
                }
                // freq_up
                err = gpio_pin_interrupt_configure_dt(&freq_up_button, GPIO_INT_EDGE_TO_ACTIVE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw1.");
                }
                gpio_init_callback(&freq_up_button_cb, freq_up_button_callback, BIT(freq_up_button.pin));
                err = gpio_add_callback_dt(&freq_up_button, &freq_up_button_cb);
                if (err < 0) {
                    LOG_ERR("Cannot add callback to sw1.");
                }
                // freq_down
                err = gpio_pin_interrupt_configure_dt(&freq_down_button, GPIO_INT_EDGE_TO_ACTIVE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw2.");
                }
                gpio_init_callback(&freq_down_button_cb, freq_down_button_callback, BIT(freq_down_button.pin));
                err = gpio_add_callback_dt(&freq_down_button, &freq_down_button_cb);
                if (err < 0) {
                    LOG_ERR("Cannot add callback to sw2.");
                }
                // reset
                err = gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_EDGE_TO_ACTIVE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw3.");
                }
                gpio_init_callback(&reset_button_cb, reset_button_callback, BIT(reset_button.pin));
                err = gpio_add_callback_dt(&reset_button, &reset_button_cb);
                if (err < 0) {
                    LOG_ERR("Cannot add callback to sw3.");
                }
                
                /* CONFIGURE LEDs */
                err = gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_ACTIVE);
                if (err < 0) {
                    LOG_ERR("Cannot configure heartbeat LED.");
                    return err;
                }
                err = gpio_pin_configure_dt(&iv_pump_led, GPIO_OUTPUT_ACTIVE);
                if (err < 0) {
                    LOG_ERR("Cannot configure iv_pump LED.");
                    return err;
                }
                err = gpio_pin_configure_dt(&buzzer_led, GPIO_OUTPUT_INACTIVE);
                if (err < 0) {
                    LOG_ERR("Cannot configure buzzer LED.");
                    return err;
                }
                err = gpio_pin_configure_dt(&error_led, GPIO_OUTPUT_INACTIVE);
                if (err < 0) {
                    LOG_ERR("Cannot configure error LED.");
                    return err;
                }
                
                // skip entry setup because we just did it
                next_state = BLINKING_RUN;
                break;
            case BLINKING_ENTRY:
                err = heartbeat();
                if (err < 0) {
                    return err;
                }
                
                /* RECONFIGURE BUTTONS TO ENABLE CALLBACKS */
                err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw0.");
                }
                err = gpio_pin_interrupt_configure_dt(&freq_up_button, GPIO_INT_EDGE_TO_ACTIVE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw1.");
                }
                err = gpio_pin_interrupt_configure_dt(&freq_down_button, GPIO_INT_EDGE_TO_ACTIVE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw2.");
                }
                
                /* SET LEDS */
                err =  gpio_pin_set_dt(&iv_pump_led, iv_pump_led_status.illuminated);
                if (err < 0) {
                    LOG_ERR("Failed to set iv_pump LED.");
                    return err;
                }
                err =  gpio_pin_set_dt(&buzzer_led, !iv_pump_led_status.illuminated);
                if (err < 0) {
                    LOG_ERR("Failed to set buzzer LED.");
                    return err;
                }
                err =  gpio_pin_set_dt(&error_led, 0);
                if (err < 0) {
                    LOG_ERR("Failed to set error LED.");
                    return err;
                }
                
                next_state = BLINKING_RUN;
                break;
            case BLINKING_RUN:
                err = heartbeat();
                if (err < 0) {
                    return err;
                }
                err = actionLEDs();
                if (err < 0) {
                    return err;
                }
                break;
            case ERROR:
                err = heartbeat();
                if (err < 0) {
                    return err;
                }
                
                /* RECONFIGURE BUTTONS TO DISABLE CALLBACKS */
                err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_DISABLE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw0.");
                }
                err = gpio_pin_interrupt_configure_dt(&freq_up_button, GPIO_INT_DISABLE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw1.");
                }
                err = gpio_pin_interrupt_configure_dt(&freq_down_button, GPIO_INT_DISABLE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw2.");
                }
                
                /* SET LEDS */
                err =  gpio_pin_set_dt(&iv_pump_led, 0);
                if (err < 0) {
                    LOG_ERR("Failed to set iv_pump LED.");
                    return err;
                }
                iv_pump_led_status.illuminated = false;
                err =  gpio_pin_set_dt(&buzzer_led, 0);
                if (err < 0) {
                    LOG_ERR("Failed to set buzzer LED.");
                    return err;
                }
                err =  gpio_pin_set_dt(&error_led, 1);
                if (err < 0) {
                    LOG_ERR("Failed to set error LED.");
                    return err;
                }
                
                next_state = ERROR;
                break;
            case RESET:
                err = heartbeat();
                if (err < 0) {
                    return err;
                }
                
                action_led_hz = LED_BLINK_FREQ_HZ;
                
                next_state = BLINKING_ENTRY;
                break;
            case SLEEP:
                err = heartbeat();
                if (err < 0) {
                    return err;
                }
                
                /* RECONFIGURE BUTTONS TO DISABLE CALLBACKS */
                err = gpio_pin_interrupt_configure_dt(&freq_up_button, GPIO_INT_DISABLE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw1.");
                }
                err = gpio_pin_interrupt_configure_dt(&freq_down_button, GPIO_INT_DISABLE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw2.");
                }
                
                /* TURN OFF LEDS */
                err =  gpio_pin_set_dt(&iv_pump_led, 0);
                if (err < 0) {
                    LOG_ERR("Failed to set iv_pump LED.");
                    return err;
                }
                err =  gpio_pin_set_dt(&buzzer_led, 0);
                if (err < 0) {
                    LOG_ERR("Failed to set buzzer LED.");
                    return err;
                }
                
                next_state = SLEEP;
                break;
            default:
                break;
        }
        
        /* CHECK BUTTON CALLBACKS */
        if (sleep_button_event) {
            LOG_INF("Sleep button pressed");
            next_state = state == SLEEP ? BLINKING_ENTRY : SLEEP;
            sleep_button_event = false;
        }
        if (up_button_event) {
            action_led_hz += FREQ_UP_INC_HZ;
            LOG_INF("Freq Up button pressed, frequency is %d", action_led_hz);
            up_button_event = false;
        }
        if (down_button_event) {
            action_led_hz -= FREQ_DOWN_INC_HZ;
            LOG_INF("Freq Down button pressed, frequency is %d", action_led_hz);
            down_button_event = false;
        }
        if ((action_led_hz < MIN_FREQ_HZ || action_led_hz > MAX_FREQ_HZ) && state != ERROR) {
            LOG_ERR("Action freq out of range 1-5: %d.", action_led_hz);
            next_state = ERROR;
        }
        if (reset_button_event) {
            LOG_INF("Reset button pressed, resetting frequency to %d", LED_BLINK_FREQ_HZ);
            next_state = RESET;
            reset_button_event = false;
        }
        
        state = next_state;
        k_msleep(10);
    }
    
    return 0;
}

int heartbeat() {
    if (current_time - heartbeat_led_status.toggle_time > HEARTBEAT_TOGGLE_INTERVAL_MS) {
        int err = gpio_pin_toggle_dt(&heartbeat_led);
        if (err < 0) {
            LOG_ERR("Failed to toggle heartbeat LED.");
            return err;
        }
        heartbeat_led_status.toggle_time = current_time;
        heartbeat_led_status.illuminated = !heartbeat_led_status.illuminated;
        LOG_INF("Heartbeat LED toggled");
    }
    
    return 0;
}

int actionLEDs() {
    int64_t target_time_passed = MS_PER_HZ / (action_led_hz * 2);
    if (current_time - iv_pump_led_status.toggle_time > target_time_passed) {
        int err = gpio_pin_toggle_dt(&iv_pump_led);
        if (err < 0) {
            LOG_ERR("Failed to toggle iv_pump LED.");
            return err;
        }
        err = gpio_pin_toggle_dt(&buzzer_led);
        if (err < 0) {
            LOG_ERR("Failed to toggle buzzer LED.");
            return err;
        }
        iv_pump_led_status.toggle_time = current_time;
        iv_pump_led_status.illuminated = !iv_pump_led_status.illuminated;
        LOG_INF("IV Pump and Buzzer LEDs toggled");
    }
    
    return 0;
}

void sleep_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    sleep_button_event = true;
}

void freq_up_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    up_button_event = true;
}

void freq_down_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    down_button_event = true;
}

void reset_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    reset_button_event = true;
}
