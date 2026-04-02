#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h> 
#include <zephyr/logging/log.h>  // needs CONFIG_LOG=y in your prj.conf
// #include <zephyr/drivers/adc.h> // CONFIG_ADC=y
// #include <zephyr/drivers/pwm.h> // CONFIG_PWM=y
#include <zephyr/smf.h> // CONFIG_SMF=y
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

/* DECLARE STRUCT STATE TABLE AND STRUCT */
struct s_object {
        struct smf_ctx ctx;
        
        /* USER-DEFINED VARS FOR STATE MACHINE*/
        int action_led_hz;
        int64_t remaining_phase;
        bool iv_illuminated;
};

struct s_object s_context = {
    .action_led_hz = LED_BLINK_FREQ_HZ,
    .remaining_phase = 0,
    .iv_illuminated = true,
};

/* DECLARE STATE FUNCTIONS */
static void init(void *o);
static void blinking_entry(void *o);
static void blinking_run(void *o);
static void blinking_exit(void *o);
static void sleep_entry(void *o);
static void sleep_run(void *o);
static void sleep_exit(void *o);
static void reset(void *o);
static void error_entry(void *o);
static void error_run(void *o);
static void error_exit(void *o);

/* DEFINE STATES FOR STATE MACHINE */
enum state { INIT, BLINKING, SLEEP, RESET, ERROR };

/* POPULATE STATE TABLE */
static const struct smf_state states[] = {
        [INIT] = SMF_CREATE_STATE(NULL, init, NULL, NULL, NULL),
        [BLINKING] = SMF_CREATE_STATE(blinking_entry, blinking_run, blinking_exit, NULL, NULL),
        [SLEEP] = SMF_CREATE_STATE(sleep_entry, sleep_run, sleep_exit, NULL, NULL),
        [RESET] = SMF_CREATE_STATE(NULL, reset, NULL, NULL, NULL),
        [ERROR] = SMF_CREATE_STATE(error_entry, error_run, error_exit, NULL, NULL),
};

/* DECLARE GLOBAL VARIABLES */
// int state = INIT;
// int next_state = INIT;
// int action_led_hz = LED_BLINK_FREQ_HZ;
int timer_error = 0;
// int64_t remaining_phase = 0;
// bool iv_illuminated = true;

int main(void)
{
    smf_set_initial(SMF_CTX(&s_context), &states[INIT]);
    
    while (1) {
        if (timer_error < 0) {
            return timer_error;
        }
        
        int err = smf_run_state(SMF_CTX(&s_context));
        if (err) {
            /* handle return code and terminate state machine */
            smf_set_terminate(SMF_CTX(&s_context), err);
            break;
        }
    }
    
    return 0;
}

void sleep_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    SLEEP_PRESSED();
    k_event_post(&button_events, SLEEP_EVENT);
}

void freq_up_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    FREQUENCY_UP_PRESSED(action_led_hz);
    k_event_post(&button_events, FREQ_UP_EVENT);
}

void freq_down_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    FREQUENCY_DOWN_PRESSED(action_led_hz);
    k_event_post(&button_events, FREQ_DOWN_EVENT);
}

void reset_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    RESET_PRESSED();
    k_event_post(&button_events, RESET_EVENT);
}

void action_timer_interval_handler(struct k_timer *action_timer) {
    int err = gpio_pin_toggle_dt(&iv_pump_led);
    if (err < 0) {
        LOG_ERR("Failed to toggle iv_pump LED.");
        timer_error = err;
        return;
    }
    err =  gpio_pin_set_dt(&buzzer_led, s_context.iv_illuminated);
    if (err < 0) {
        LOG_ERR("Failed to toggle buzzer LED.");
        timer_error = err;
        return;
    }
    s_context.iv_illuminated = !s_context.iv_illuminated;
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

static void init(void *o) {
    int err = 0;
    
    /* CHECK INTERFACE READY */
    if (!device_is_ready(sleep_button.port)) {
        LOG_ERR("gpio0 interface not ready.");
        smf_set_terminate(SMF_CTX(&s_context), -1);
    }
    
    /* CONFIGURE BUTTON GPIO PINS */
    err = gpio_pin_configure_dt(&sleep_button, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Cannot configure sleep button.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_configure_dt(&freq_up_button, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Cannot configure freq_up button.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_configure_dt(&freq_down_button, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Cannot configure freq_down button.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_configure_dt(&reset_button, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Cannot configure reset button.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* CONFIGURE BUTTON CALLBACKS */
    // sleep
    err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw0.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    gpio_init_callback(&sleep_button_cb, sleep_button_callback, BIT(sleep_button.pin));
    err = gpio_add_callback_dt(&sleep_button, &sleep_button_cb);
    if (err < 0) {
        LOG_ERR("Cannot add callback to sw0.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    // freq_up
    err = gpio_pin_interrupt_configure_dt(&freq_up_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw1.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    gpio_init_callback(&freq_up_button_cb, freq_up_button_callback, BIT(freq_up_button.pin));
    err = gpio_add_callback_dt(&freq_up_button, &freq_up_button_cb);
    if (err < 0) {
        LOG_ERR("Cannot add callback to sw1.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    // freq_down
    err = gpio_pin_interrupt_configure_dt(&freq_down_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw2.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    gpio_init_callback(&freq_down_button_cb, freq_down_button_callback, BIT(freq_down_button.pin));
    err = gpio_add_callback_dt(&freq_down_button, &freq_down_button_cb);
    if (err < 0) {
        LOG_ERR("Cannot add callback to sw2.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    // reset
    err = gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw3.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    gpio_init_callback(&reset_button_cb, reset_button_callback, BIT(reset_button.pin));
    err = gpio_add_callback_dt(&reset_button, &reset_button_cb);
    if (err < 0) {
        LOG_ERR("Cannot add callback to sw3.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* CONFIGURE LEDs */
    err = gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        LOG_ERR("Cannot configure heartbeat LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_configure_dt(&iv_pump_led, GPIO_OUTPUT_ACTIVE);
    if (err < 0) {
        LOG_ERR("Cannot configure iv_pump LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_configure_dt(&buzzer_led, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        LOG_ERR("Cannot configure buzzer LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_configure_dt(&error_led, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        LOG_ERR("Cannot configure error LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* START TIMERS */
    k_timer_start(&action_timer,
                    K_MSEC(MS_PER_HZ / (s_context.action_led_hz * 2)),
                    K_MSEC(MS_PER_HZ / (s_context.action_led_hz * 2)));
    
    k_event_init(&button_events);
    
    smf_set_state(SMF_CTX(&s_context), &states[BLINKING]);
}

static void blinking_entry(void *o) {
    int err = 0;
    
    /* RECONFIGURE BUTTONS TO ENABLE CALLBACKS */
    err = gpio_pin_interrupt_configure_dt(&freq_up_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw1.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_interrupt_configure_dt(&freq_down_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw2.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* SET LEDS */
    err =  gpio_pin_set_dt(&iv_pump_led, s_context.iv_illuminated);
    if (err < 0) {
        LOG_ERR("Failed to set iv_pump LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err =  gpio_pin_set_dt(&buzzer_led, !s_context.iv_illuminated);
    if (err < 0) {
        LOG_ERR("Failed to set buzzer LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
}

static void blinking_run(void *o) {
    uint32_t events = k_event_wait(&button_events, 0xF, true, K_FOREVER);
    if (events & SLEEP_EVENT) {
        LOG_INF("Sleep button pressed");
        smf_set_state(SMF_CTX(&s_context), &states[SLEEP]);
    }
    if (events & FREQ_UP_EVENT) {
        s_context.action_led_hz += FREQ_UP_INC_HZ;
        LOG_INF("Freq Up button pressed, frequency is %d", s_context.action_led_hz);
    }
    if (events & FREQ_DOWN_EVENT) {
        s_context.action_led_hz -= FREQ_DOWN_INC_HZ;
        LOG_INF("Freq Down button pressed, frequency is %d", s_context.action_led_hz);
    }
    if (events & FREQ_UP_EVENT || events & FREQ_DOWN_EVENT) {
        if (s_context.action_led_hz < 1 || s_context.action_led_hz > 5) {
            LOG_ERR("Action freq out of range 1-5: %d.", s_context.action_led_hz);
            smf_set_state(SMF_CTX(&s_context), &states[ERROR]);
        } else {
            k_timer_start(&action_timer,
                            K_MSEC(MS_PER_HZ / (s_context.action_led_hz * 2)),
                            K_MSEC(MS_PER_HZ / (s_context.action_led_hz * 2)));
        }
    }
    if (events & RESET_EVENT) {
        LOG_INF("Reset button pressed, resetting frequency to %d", LED_BLINK_FREQ_HZ);
        smf_set_state(SMF_CTX(&s_context), &states[RESET]);
    }
    
}

static void blinking_exit(void *o) {
    int err = 0;
    
    /* RECONFIGURE BUTTONS TO DISABLE CALLBACKS */
    err = gpio_pin_interrupt_configure_dt(&freq_up_button, GPIO_INT_DISABLE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw1.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_interrupt_configure_dt(&freq_down_button, GPIO_INT_DISABLE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw2.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* TURN OFF LEDS */
    err =  gpio_pin_set_dt(&iv_pump_led, 0);
    if (err < 0) {
        LOG_ERR("Failed to set iv_pump LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err =  gpio_pin_set_dt(&buzzer_led, 0);
    if (err < 0) {
        LOG_ERR("Failed to set buzzer LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
}

static void sleep_entry(void *o) {
    /* STOP ACTION TIMER */
    s_context.remaining_phase = k_timer_remaining_get(&action_timer);
    k_timer_stop(&action_timer);
    
    SLEEP_STATE();
}

static void sleep_run(void *o) {
    uint32_t events = k_event_wait(&button_events, SLEEP_EVENT | RESET_EVENT, true, K_FOREVER);
    if (events & SLEEP_EVENT) {
        LOG_INF("Sleep button pressed");
        smf_set_state(SMF_CTX(&s_context), &states[BLINKING]);
    }
    if (events & RESET_EVENT) {
        LOG_INF("Reset button pressed, resetting frequency to %d", LED_BLINK_FREQ_HZ);
        smf_set_state(SMF_CTX(&s_context), &states[RESET]);
    }
}

static void sleep_exit(void *o) {
    /* RESTART TIMER */
    k_timer_start(&action_timer,
                    K_MSEC(s_context.remaining_phase),
                    K_MSEC(MS_PER_HZ / (s_context.action_led_hz * 2)));
}

static void reset(void *o) {
    s_context.action_led_hz = LED_BLINK_FREQ_HZ;
    
    /* RESTART TIMER */
    k_timer_start(&action_timer,
                    K_MSEC(MS_PER_HZ / (s_context.action_led_hz * 2)),
                    K_MSEC(MS_PER_HZ / (s_context.action_led_hz * 2)));
    
    smf_set_state(SMF_CTX(&s_context), &states[BLINKING]);
    
    RESET_STATUS();
}

static void error_entry(void *o) {
    int err = 0;
    
    /* RECONFIGURE BUTTON TO DISABLE CALLBACK */
    err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_DISABLE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw0.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* TURN ON LED */
    err =  gpio_pin_set_dt(&error_led, 1);
    if (err < 0) {
        LOG_ERR("Failed to set error LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* STOP ACTION TIMER */
    k_timer_stop(&action_timer);
    
    ERROR_STATE();
}

static void error_run(void *o) {
    uint32_t events = k_event_wait(&button_events, RESET_EVENT, true, K_FOREVER);
    if (events & RESET_EVENT) {
        LOG_INF("Reset button pressed, resetting frequency to %d", LED_BLINK_FREQ_HZ);
        smf_set_state(SMF_CTX(&s_context), &states[RESET]);
    }
}

static void error_exit(void *o) {
    int err = 0;
    
    /* RECONFIGURE BUTTON TO ENABLE CALLBACK */
    err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw0.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* TURN OFF LED */
    err =  gpio_pin_set_dt(&error_led, 0);
    if (err < 0) {
        LOG_ERR("Failed to set error LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
}
