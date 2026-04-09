#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h> 
#include <zephyr/logging/log.h>  // needs CONFIG_LOG=y in your prj.conf
#include <zephyr/drivers/adc.h> // CONFIG_ADC=y
// #include <zephyr/drivers/pwm.h> // CONFIG_PWM=y
#include <zephyr/smf.h> // CONFIG_SMF=y
#include "bme554_lib.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

/* DEFINE MACROS */
#define MS_PER_HZ                       1000
#define HB_ON_TIME                      250
#define MAX_FREQ_HZ                     5
#define MIN_FREQ_HZ                     1
#define MIN_V_MV                        0
#define MAX_V_MV                        3000
#define READ_EVENT                      BIT(0)
#define SLEEP_EVENT                     BIT(1)
#define TIMER_COMPLETE_EVENT            BIT(2)
#define RESET_EVENT                     BIT(3)
#define BLINKING_TIME_MS                5000
#define ADC_DT_SPEC_GET_BY_ALIAS(adc_alias)                 \
{                                                           \
    .dev = DEVICE_DT_GET(DT_PARENT(DT_ALIAS(adc_alias))),   \
    .channel_id = DT_REG_ADDR(DT_ALIAS(adc_alias)),         \
    ADC_CHANNEL_CFG_FROM_DT_NODE(DT_ALIAS(adc_alias))       \
}

/* DEFINE GLOBALS AND DT-BASED HARDWARE STRUCTS */
// buttons
const struct gpio_dt_spec read_button = GPIO_DT_SPEC_GET(DT_ALIAS(readbutton), gpios);
const struct gpio_dt_spec sleep_button = GPIO_DT_SPEC_GET(DT_ALIAS(sleepbutton), gpios);
const struct gpio_dt_spec reset_button = GPIO_DT_SPEC_GET(DT_ALIAS(resetbutton), gpios);

// LEDs
const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(heartbeat), gpios);
const struct gpio_dt_spec blinker_led = GPIO_DT_SPEC_GET(DT_ALIAS(blinker), gpios);
const struct gpio_dt_spec error_led = GPIO_DT_SPEC_GET(DT_ALIAS(error), gpios);

/* DEFINE CALLBACK FUNCTIONS */
void read_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void sleep_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void reset_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

/* DEFINE TIMER FUNCTIONS */
void blinking_interrupt_handler(struct k_timer *blinking_timer);
void led_on_interrupt_handler(struct k_timer *led_on_timer);
void led_off_interrupt_handler(struct k_timer *led_off_timer);
void led_on_stop_handler(struct k_timer *led_on_timer);

/* DEFINE THREAD FUNCTIONS */
void heartbeat_thread(void *, void *, void *);

/* DEFINE KERNEL EVENTS */
K_EVENT_DEFINE(button_events);

/* INITIALIZE GPIO CALLBACK STRUCTS */
static struct gpio_callback read_button_cb;
static struct gpio_callback sleep_button_cb;
static struct gpio_callback reset_button_cb;

/* INITIALIZE ADC STRUCT */
static const struct adc_dt_spec adc_vadc = ADC_DT_SPEC_GET_BY_ALIAS(vadc);

/* DEFINE TIMERS */
K_TIMER_DEFINE(blinking_timer, blinking_interrupt_handler, NULL);
K_TIMER_DEFINE(led_on_timer, led_on_interrupt_handler, led_on_stop_handler);
K_TIMER_DEFINE(led_off_timer, led_off_interrupt_handler, NULL);

/* DEFINE THREADS */
K_THREAD_DEFINE(heartbeat_thread_id, 1024, heartbeat_thread, NULL, NULL, NULL, 5, 0, 0);

/* DECLARE STRUCT STATE TABLE AND STRUCT */
struct s_object {
    struct smf_ctx ctx;
    
    /* USER-DEFINED VARS FOR STATE MACHINE*/
    int32_t millivolts;
    float freq;
    float ontime;
    float offtime;
    
    int64_t starttime;
    int64_t endtime;
};

struct s_object s_context = {
    .millivolts = 0,
    .freq = 0,
    .ontime = 0,
    .offtime = 0,
    .starttime = 0,
    .endtime = 0,
};

int16_t buf;
struct adc_sequence sequence = {
    .buffer = &buf,
    .buffer_size = sizeof(buf), // bytes
};

/* DECLARE STATE FUNCTIONS */
static void init(void *o);
static void reset(void *o);
static void idle_entry(void *o);
static void idle_run(void *o);
static void idle_exit(void *o);
static void sleep_run(void *o);
static void reading_entry(void *o);
static void reading_run(void *o);
static void reading_exit(void *o);
static void blinking_entry(void *o);
static void blinking_run(void *o);
static void blinking_exit(void *o);
static void error_entry(void *o);
static void error_run(void *o);
static void error_exit(void *o);

/* DEFINE STATES FOR STATE MACHINE */
enum state { INIT, RESET, IDLE, SLEEP, READING, BLINKING, ERROR };

/* POPULATE STATE TABLE */
static const struct smf_state states[] = {
    [INIT] = SMF_CREATE_STATE(NULL, init, NULL, NULL, NULL),
    [RESET] = SMF_CREATE_STATE(NULL, reset, NULL, NULL, NULL),
    [IDLE] = SMF_CREATE_STATE(idle_entry, idle_run, idle_exit, NULL, NULL),
    [SLEEP] = SMF_CREATE_STATE(NULL, sleep_run, NULL, NULL, NULL),
    [READING] = SMF_CREATE_STATE(reading_entry, reading_run, reading_exit, NULL, NULL),
    [BLINKING] = SMF_CREATE_STATE(blinking_entry, blinking_run, blinking_exit, NULL, NULL),
    [ERROR] = SMF_CREATE_STATE(error_entry, error_run, error_exit, NULL, NULL),
};

/* DECLARE GLOBAL VARIABLES */
int timer_error = 0;

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

void read_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_event_post(&button_events, READ_EVENT);
}

void sleep_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_event_post(&button_events, SLEEP_EVENT);
}

void reset_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    k_event_post(&button_events, RESET_EVENT);
}

void heartbeat_thread(void *, void *, void *) {
    while (1) {
        k_msleep(HB_ON_TIME);  // scheduler can run other tasks now
        int err = gpio_pin_toggle_dt(&heartbeat_led);
        if (err < 0) {
            LOG_ERR("Failed to toggle heartbeat LED.");
            timer_error = err;
            return;
        }
        LOG_INF("Heartbeat LED toggled");
        k_msleep(MS_PER_HZ - HB_ON_TIME); // scheduler can run other tasks now
        err = gpio_pin_toggle_dt(&heartbeat_led);
        if (err < 0) {
            LOG_ERR("Failed to toggle heartbeat LED.");
            timer_error = err;
            return;
        }
        LOG_INF("Heartbeat LED toggled");
    }
}

void blinking_interrupt_handler(struct k_timer *blinking_timer) {
    /* STOP TIMERS */
    k_timer_stop(&led_on_timer);
    k_timer_stop(&led_off_timer);
    
    s_context.endtime = k_uptime_get();
    
    k_event_post(&button_events, TIMER_COMPLETE_EVENT);
}

void led_on_interrupt_handler(struct k_timer *led_on_timer) {
    int err;
    
    /* TURN OFF LED */
    err =  gpio_pin_set_dt(&blinker_led, 0);
    if (err < 0) {
        LOG_ERR("Failed to set blinker LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* START TIMER */
    k_timer_start(&led_off_timer,
                  K_MSEC(s_context.offtime),
                  K_NO_WAIT);
}

void led_off_interrupt_handler(struct k_timer *led_off_timer) {
    int err;
    
    /* TURN OFF LED */
    err =  gpio_pin_set_dt(&blinker_led, 1);
    if (err < 0) {
        LOG_ERR("Failed to set blinker LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* START TIMERS */
    k_timer_start(&led_on_timer,
                  K_MSEC(s_context.ontime),
                  K_NO_WAIT);
}

void led_on_stop_handler(struct k_timer *led_on_timer) {
    int err;
    
    /* TURN OFF LED */
    err =  gpio_pin_set_dt(&blinker_led, 0);
    if (err < 0) {
        LOG_ERR("Failed to set blinker LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
}

static void init(void *o) {
    int err = 0;
    
    /* CHECK INTERFACE READY */
    if (!device_is_ready(read_button.port)) {
        LOG_ERR("gpio0 interface not ready.");
        smf_set_terminate(SMF_CTX(&s_context), -1);
    }
    if (!device_is_ready(adc_vadc.dev)) {
        LOG_ERR("ADC controller device(s) not ready");
        smf_set_terminate(SMF_CTX(&s_context), -1);
    }
    
    /* CONFIGURE BUTTON GPIO PINS */
    err = gpio_pin_configure_dt(&read_button, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Cannot configure sleep button.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_configure_dt(&sleep_button, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Cannot configure sleep button.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_configure_dt(&reset_button, GPIO_INPUT);
    if (err < 0) {
        LOG_ERR("Cannot configure reset button.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* CONFIGURE BUTTON CALLBACKS */
    // read
    err = gpio_pin_interrupt_configure_dt(&read_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw0.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    gpio_init_callback(&read_button_cb, read_button_callback, BIT(read_button.pin));
    err = gpio_add_callback_dt(&read_button, &read_button_cb);
    if (err < 0) {
        LOG_ERR("Cannot add callback to sw0.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    // sleep
    err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw1.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    gpio_init_callback(&sleep_button_cb, sleep_button_callback, BIT(sleep_button.pin));
    err = gpio_add_callback_dt(&sleep_button, &sleep_button_cb);
    if (err < 0) {
        LOG_ERR("Cannot add callback to sw1.");
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
    err = gpio_pin_configure_dt(&blinker_led, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        LOG_ERR("Cannot configure iv_pump LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_configure_dt(&error_led, GPIO_OUTPUT_INACTIVE);
    if (err < 0) {
        LOG_ERR("Cannot configure error LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* CONFIGURE ADC CHANNEL */
    err = adc_channel_setup_dt(&adc_vadc);
    if (err < 0) {
        LOG_ERR("Could not setup ADC channel (%d)", err);
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    k_event_init(&button_events);
    
    smf_set_state(SMF_CTX(&s_context), &states[IDLE]);
}

static void reset(void *o) {
    smf_set_state(SMF_CTX(&s_context), &states[IDLE]);
}

static void idle_entry(void *o) {
    int err = 0;
    
    /* RECONFIGURE BUTTONS TO ENABLE CALLBACKS */
    err = gpio_pin_interrupt_configure_dt(&read_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw1.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
}

static void idle_run(void *o) {
    uint32_t events = k_event_wait(&button_events, READ_EVENT | SLEEP_EVENT | RESET_EVENT, true, K_FOREVER);
    if (events & READ_EVENT) {
        LOG_INF("Read button pressed");
        smf_set_state(SMF_CTX(&s_context), &states[READING]);
    }
    if (events & SLEEP_EVENT) {
        LOG_INF("Sleep button pressed");
        smf_set_state(SMF_CTX(&s_context), &states[SLEEP]);
    }
    if (events & RESET_EVENT) {
        LOG_INF("Reset button pressed");
        smf_set_state(SMF_CTX(&s_context), &states[RESET]);
    }
}

static void idle_exit(void *o) {
    int err = 0;
    
    /* RECONFIGURE BUTTONS TO DISABLE CALLBACKS */
    err = gpio_pin_interrupt_configure_dt(&read_button, GPIO_INT_DISABLE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw0.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
}

static void sleep_run(void *o) {
    uint32_t events = k_event_wait(&button_events, SLEEP_EVENT | RESET_EVENT, true, K_FOREVER);
    if (events & SLEEP_EVENT) {
        LOG_INF("Sleep button pressed");
        smf_set_state(SMF_CTX(&s_context), &states[IDLE]);
    }
    if (events & RESET_EVENT) {
        LOG_INF("Reset button pressed");
        smf_set_state(SMF_CTX(&s_context), &states[RESET]);
    }
}

static void reading_entry(void *o) {
    ADC_READ_TRIGGERED();
    int err = 0;
    
    /* RECONFIGURE BUTTONS TO DISABLE CALLBACKS */
    err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_DISABLE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw1.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_DISABLE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw3.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    (void)adc_sequence_init_dt(&adc_vadc, &sequence);
}

static void reading_run(void *o) {
    int ret;
    ret = adc_read(adc_vadc.dev, &sequence);
    if (ret < 0) {
        LOG_ERR("Could not read (%d)", ret);
    } else {
        LOG_DBG("Raw ADC Buffer: %d", buf);
    }
    
    int32_t val_mv;
    val_mv = buf;
    ret = adc_raw_to_millivolts_dt(&adc_vadc, &val_mv);
    if (ret < 0) {
        LOG_ERR("Buffer cannot be converted to mV; returning raw buffer value.");
    } else {
        LOG_INF("ADC Value (mV): %d", val_mv);
    }
    
    s_context.millivolts = val_mv;

    float max_v = MAX_V_MV;
    s_context.freq = ((s_context.millivolts * (MAX_FREQ_HZ - MIN_FREQ_HZ)) / max_v) + MIN_FREQ_HZ;
    LOG_INF("Mapped frequency (Hz): %f", (double)s_context.freq);
    
    s_context.ontime = (MS_PER_HZ / s_context.freq) / 10;
    s_context.offtime = (MS_PER_HZ / s_context.freq) - s_context.ontime;

    ADC_READ_COMPLETE(val_mv, s_context.freq);
    
    if (val_mv < MIN_V_MV || val_mv > MAX_V_MV) {
        smf_set_state(SMF_CTX(&s_context), &states[ERROR]);
    } else {
        smf_set_state(SMF_CTX(&s_context), &states[BLINKING]);
    }

}

static void reading_exit(void *o) {
    int err = 0;
    
    /* RECONFIGURE BUTTONS TO ENABLE CALLBACKS */
    err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw1.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    err = gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw3.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
}

static void blinking_entry(void *o) {
    int err;
    
    /* TURN ON LED */
    err =  gpio_pin_set_dt(&blinker_led, 1);
    if (err < 0) {
        LOG_ERR("Failed to set blinker LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* START TIMERS */
    k_timer_start(&led_on_timer,
                  K_MSEC(s_context.ontime),
                  K_NO_WAIT);
    k_timer_start(&blinking_timer,
                  K_MSEC(BLINKING_TIME_MS),
                  K_NO_WAIT);
    
    s_context.starttime = k_uptime_get();
}

static void blinking_run(void *o) {
    uint32_t events = k_event_wait(&button_events, TIMER_COMPLETE_EVENT | SLEEP_EVENT | RESET_EVENT, true, K_FOREVER);
    if (events & TIMER_COMPLETE_EVENT) {
        LOG_INF("Timer is complete!");
        smf_set_state(SMF_CTX(&s_context), &states[IDLE]);
    }
    if (events & SLEEP_EVENT) {
        LOG_INF("Sleep button pressed");
        smf_set_state(SMF_CTX(&s_context), &states[SLEEP]);
    }
    if (events & RESET_EVENT) {
        LOG_INF("Reset button pressed");
        smf_set_state(SMF_CTX(&s_context), &states[RESET]);
    }
}

static void blinking_exit(void *o) {
    k_timer_stop(&blinking_timer);
    k_timer_stop(&led_on_timer);
    k_timer_stop(&led_off_timer);

    ADC_BLINK_COMPLETE();
    
    LOG_INF("Blinking timer off, ran for %lld ms", s_context.endtime - s_context.starttime);
}

static void error_entry(void *o) {
    int err = 0;
    
    /* RECONFIGURE BUTTON TO DISABLE CALLBACK */
    err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_DISABLE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw1.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* TURN ON LED */
    err =  gpio_pin_set_dt(&error_led, 1);
    if (err < 0) {
        LOG_ERR("Failed to set error LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
}

static void error_run(void *o) {
    uint32_t events = k_event_wait(&button_events, RESET_EVENT, true, K_FOREVER);
    if (events & RESET_EVENT) {
        LOG_INF("Reset button pressed");
        smf_set_state(SMF_CTX(&s_context), &states[RESET]);
    }
}

static void error_exit(void *o) {
    int err = 0;
    
    /* RECONFIGURE BUTTON TO ENABLE CALLBACK */
    err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE); 
    if (err < 0) {
        LOG_ERR("Cannot attach callback to sw1.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
    
    /* TURN OFF LED */
    err =  gpio_pin_set_dt(&error_led, 0);
    if (err < 0) {
        LOG_ERR("Failed to set error LED.");
        smf_set_terminate(SMF_CTX(&s_context), err);
    }
}
