#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h> 
#include <zephyr/logging/log.h>  // needs CONFIG_LOG=y in your prj.conf
// #include <zephyr/drivers/adc.h> // CONFIG_ADC=y
// #include <zephyr/drivers/pwm.h> // CONFIG_PWM=y
// #include <zephyr/smf.h> // CONFIG_SMF=y
#include "bme554_lib.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* DEFINE MACROS */
#define HEARTBEAT_TOGGLE_INTERVAL_MS 500
#define MS_PER_HZ                    1000
#define LED_BLINK_FREQ_HZ            2
#define FREQ_UP_INC_HZ               1
#define FREQ_DOWN_INC_HZ             1
#define MAX_FREQ_HZ                  5
#define MIN_FREQ_HZ                  1
#define SLEEP_EVENT                  BIT(0)
#define FREQ_UP_EVENT                BIT(1)
#define FREQ_DOWN_EVENT              BIT(2)
#define RESET_EVENT                  BIT(3)

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

/* DEFINE TIMER FUNCTIONS */
void action_timer_interval_handler(struct k_timer *action_timer);

/* DEFINE THREAD FUNCTIONS */
void heartbeat_thread(void *, void *, void *);

/* DEFINE KERNEL EVENTS */
K_EVENT_DEFINE(button_events);

/* INITIALIZE GPIO CALLBACK STRUCTS */
static struct gpio_callback sleep_button_cb;
static struct gpio_callback freq_up_button_cb;
static struct gpio_callback freq_down_button_cb;
static struct gpio_callback reset_button_cb;

/* DEFINE TIMERS */
K_TIMER_DEFINE(action_timer, action_timer_interval_handler, NULL);

/* DEFINE THREADS */
K_THREAD_DEFINE(heartbeat_thread_id, 1024, heartbeat_thread, NULL, NULL, NULL, 5, 0, 0);

/* DEFINE STATES FOR STATE MACHINE */
enum states { INIT, BLINKING_ENTRY, BLINKING_RUN, BLINKING_EXIT, SLEEP_ENTRY,
              SLEEP, RESET, ERROR_ENTRY, ERROR };

/* DECLARE GLOBAL VARIABLES */
int state = INIT;
int next_state = INIT;
int action_led_hz = LED_BLINK_FREQ_HZ;
int timer_error = 0;
int64_t remaining_phase = 0;
bool iv_illuminated = true;

int main(void)
{
    while (1) {
        if (timer_error < 0) {
            return timer_error;
        }
        
        int err = 0;
        uint32_t events = 0;
        
        switch (state) {
            case INIT:
                /* CHECK INTERFACE READY */
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
                
                /* START TIMERS */
                k_timer_start(&action_timer,
                              K_MSEC(MS_PER_HZ / (action_led_hz * 2)),
                              K_MSEC(MS_PER_HZ / (action_led_hz * 2)));
                
                k_event_init(&button_events);
                
                // skip entry setup because we just did it
                state = BLINKING_RUN;
                break;
            case BLINKING_ENTRY:
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
                err =  gpio_pin_set_dt(&iv_pump_led, iv_illuminated);
                if (err < 0) {
                    LOG_ERR("Failed to set iv_pump LED.");
                    return err;
                }
                err =  gpio_pin_set_dt(&buzzer_led, !iv_illuminated);
                if (err < 0) {
                    LOG_ERR("Failed to set buzzer LED.");
                    return err;
                }
                err =  gpio_pin_set_dt(&error_led, 0);
                if (err < 0) {
                    LOG_ERR("Failed to set error LED.");
                    return err;
                }
                
                /* RESTART TIMER */
                k_timer_start(&action_timer,
                              K_MSEC(remaining_phase),
                              K_MSEC(MS_PER_HZ / (action_led_hz * 2)));
                
                state = BLINKING_RUN;
                break;
            case BLINKING_RUN:
                events = k_event_wait(&button_events, 0xF, true, K_FOREVER);
                if (events & SLEEP_EVENT) {
                    LOG_INF("Sleep button pressed");
                    state = BLINKING_EXIT;
                    next_state = SLEEP_ENTRY;
                }
                if (events & FREQ_UP_EVENT) {
                    action_led_hz += FREQ_UP_INC_HZ;
                    LOG_INF("Freq Up button pressed, frequency is %d", action_led_hz);
                    if (action_led_hz > 5) {
                        LOG_ERR("Action freq out of range 1-5: %d.", action_led_hz);
                        state = BLINKING_EXIT;
                        next_state = ERROR_ENTRY;
                        continue;
                    }
                }
                if (events & FREQ_DOWN_EVENT) {
                    action_led_hz -= FREQ_DOWN_INC_HZ;
                    LOG_INF("Freq Down button pressed, frequency is %d", action_led_hz);
                    if (action_led_hz < 1) {
                        LOG_ERR("Action freq out of range 1-5: %d.", action_led_hz);
                        state = BLINKING_EXIT;
                        next_state = ERROR_ENTRY;
                        continue;
                    }
                }
                if (events & FREQ_UP_EVENT || events & FREQ_DOWN_EVENT) {
                    k_timer_start(&action_timer,
                                  K_MSEC(MS_PER_HZ / (action_led_hz * 2)),
                                  K_MSEC(MS_PER_HZ / (action_led_hz * 2)));
                }
                if (events & RESET_EVENT) {
                    LOG_INF("Reset button pressed, resetting frequency to %d", LED_BLINK_FREQ_HZ);
                    state = RESET;
                }
                
                break;
            case BLINKING_EXIT:
                /* RECONFIGURE BUTTONS TO DISABLE CALLBACKS */
                err = gpio_pin_interrupt_configure_dt(&freq_up_button, GPIO_INT_DISABLE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw1.");
                }
                err = gpio_pin_interrupt_configure_dt(&freq_down_button, GPIO_INT_DISABLE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw2.");
                }
                
                state = next_state;
                break;
            case ERROR_ENTRY:
                /* RECONFIGURE BUTTONS TO DISABLE CALLBACKS */
                err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_DISABLE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw0.");
                }
                
                /* SET LEDS */
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
                err =  gpio_pin_set_dt(&error_led, 1);
                if (err < 0) {
                    LOG_ERR("Failed to set error LED.");
                    return err;
                }
                
                /* STOP ACTION TIMER */
                k_timer_stop(&action_timer);
                remaining_phase = 0;
                
                state = ERROR;
                break;
            case ERROR:
                events = k_event_wait(&button_events, RESET_EVENT, true, K_FOREVER);
                if (events & RESET_EVENT) {
                    LOG_INF("Reset button pressed, resetting frequency to %d", LED_BLINK_FREQ_HZ);
                    state = RESET;
                }
                
                break;
            case RESET:
                action_led_hz = LED_BLINK_FREQ_HZ;
                
                state = BLINKING_ENTRY;
                break;
            case SLEEP_ENTRY:
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
                
                /* STOP ACTION TIMER */
                remaining_phase = k_timer_remaining_get(&action_timer);
                k_timer_stop(&action_timer);
                
                state = SLEEP;
                break;
            case SLEEP:
                events = k_event_wait(&button_events, SLEEP_EVENT | RESET_EVENT, true, K_FOREVER);
                if (events & SLEEP_EVENT) {
                    LOG_INF("Sleep button pressed");
                    state = BLINKING_ENTRY;
                }
                if (events & RESET_EVENT) {
                    LOG_INF("Reset button pressed, resetting frequency to %d", LED_BLINK_FREQ_HZ);
                    state = RESET;
                }
                
                break;
            default:
                break;
        }
        
        k_msleep(10);
    }
    
    return 0;
}

void sleep_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_event_post(&button_events, SLEEP_EVENT);
}

void freq_up_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_event_post(&button_events, FREQ_UP_EVENT);
}

void freq_down_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_event_post(&button_events, FREQ_DOWN_EVENT);
}

void reset_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_event_post(&button_events, RESET_EVENT);
}

void action_timer_interval_handler(struct k_timer *action_timer) {
    int err = gpio_pin_toggle_dt(&iv_pump_led);
    if (err < 0) {
        LOG_ERR("Failed to toggle iv_pump LED.");
        timer_error = err;
        return;
    }
    err =  gpio_pin_set_dt(&buzzer_led, iv_illuminated);
    if (err < 0) {
        LOG_ERR("Failed to toggle buzzer LED.");
        timer_error = err;
        return;
    }
    iv_illuminated = !iv_illuminated;
    LOG_INF("IV Pump and Buzzer LEDs toggled");
}

void heartbeat_thread(void *, void *, void *) {
    while (1) {
        k_msleep(250);  // scheduler can run other tasks now
        int err = gpio_pin_toggle_dt(&heartbeat_led);
        if (err < 0) {
            LOG_ERR("Failed to toggle heartbeat LED.");
            timer_error = err;
            return;
        }
        LOG_INF("Heartbeat LED toggled");
        k_msleep(750); // scheduler can run other tasks now
        err = gpio_pin_toggle_dt(&heartbeat_led);
        if (err < 0) {
            LOG_ERR("Failed to toggle heartbeat LED.");
            timer_error = err;
            return;
        }
        LOG_INF("Heartbeat LED toggled");
    }
}
