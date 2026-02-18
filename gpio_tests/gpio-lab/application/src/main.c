#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h> 
#include <zephyr/logging/log.h>  // needs CONFIG_LOG=y in your prj.conf
// #include <zephyr/drivers/adc.h> // CONFIG_ADC=y
// #include <zephyr/drivers/pwm.h> // CONFIG_PWM=y
// #include <zephyr/smf.h> // CONFIG_SMF=y

LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

// define macros 
#define HEARTBEAT_TOGGLE_INTERVAL_MS 500 //originally 500 
#define LED_BLINK_FREQ_HZ 2 // frequency of blinking for an LED
#define FREQ_UP_INC_HZ 1
#define FREQ_DOWN_INC_HZ 1 
#define ACTION_BUTTON_MIN_THRESHOLD_HZ 1
#define ACTION_BUTTON_MAX_THRESHOLD_HZ 5

// declare function prototypes
void increase_action_led_blink_frequency(void);
void decrease_action_led_blink_frequency(void);
void heartbeat_blink(int64_t current_time);

// define globals and DT-based hardware structs
//static const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(heartbeat), gpios);
static const struct gpio_dt_spec sleep_button = GPIO_DT_SPEC_GET(DT_ALIAS(sleepbutton), gpios);
static const struct gpio_dt_spec reset_button = GPIO_DT_SPEC_GET(DT_ALIAS(resetbutton), gpios);
static const struct gpio_dt_spec freq_up_button = GPIO_DT_SPEC_GET(DT_ALIAS(frequpbutton), gpios);
static const struct gpio_dt_spec freq_down_button = GPIO_DT_SPEC_GET(DT_ALIAS(freqdownbutton), gpios);

static const struct gpio_dt_spec heartbeat_led = GPIO_DT_SPEC_GET(DT_ALIAS(heartbeat), gpios);
static const struct gpio_dt_spec iv_pump_led = GPIO_DT_SPEC_GET(DT_ALIAS(ivpump), gpios);
static const struct gpio_dt_spec buzzer_led = GPIO_DT_SPEC_GET(DT_ALIAS(buzzer), gpios);
static const struct gpio_dt_spec error_led = GPIO_DT_SPEC_GET(DT_ALIAS(error), gpios);

bool sleep_button_event = 0;  // flag to indicate that the sleep button has been pressed
bool reset_button_event = 0;  // flag to indicate that the reset button has been pressed
bool freq_up_button_event = 0;  // flag to indicate that the reset button has been pressed
bool freq_down_button_event = 0;  // flag to indicate that the reset button has been pressed

// define callback functions
void sleep_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void reset_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void freq_up_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);
void freq_down_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins);

// initialize GPIO Callback Structs
static struct gpio_callback sleep_button_cb;  // example; need one per callback (button)  
static struct gpio_callback reset_button_cb;  // example; need one per callback (button)  
static struct gpio_callback freq_up_button_cb;  // example; need one per callback (button)  
static struct gpio_callback freq_down_button_cb;  // example; need one per callback (button

// define states for state machine (THESE ARE ONLY PLACEHOLDERS)
enum states { INIT, DEFAULT_SETUP, AWAKE_ENTRY, AWAKE_RUN, AWAKE_EXIT, SLEEP, ERROR};
int state = INIT; // initial state

struct led {
    int64_t toggle_time;  // int64_t b/c time functions in Zephyr use this type
    bool illuminated; // state of the LED (on/off)
};

struct led heartbeat_led_status = {
    .toggle_time = 0,
    .illuminated = false
};

struct led iv_pump_led_status = {
    .toggle_time = 0,
    .illuminated = true
};

struct led buzzer_led_status = {
    .toggle_time = 0,
    .illuminated = false
};

struct led error_led_status = {
    .toggle_time = 0,
    .illuminated = false
};

// placeholder variables
int condition_to_leave_awake_state = 0;
int next_state = SLEEP;

int last_action_led_toggle_freq = LED_BLINK_FREQ_HZ;
int64_t last_action_led_toggle_time = 0;
int sleep_button_pressed = 0;

int main(void)
{
    /* below are some functional examples to get you started, but this is 
       just a starting point, not a complete program! */

    while (1) {
        // run the state machine in this indefinite loop

        // some useful functions
        // gpio_pin_toggle_dt() - toggle the state of a pin (e.g. gpio_pin_toggle_dt(&led0))
    
        int64_t current_time = k_uptime_get();  // get the current time in milliseconds

        switch (state) {
            case INIT:
                // check if interface is ready
                if (!device_is_ready(sleep_button.port)) {
                    LOG_ERR("gpio0 interface not ready.");  // logging module output
                    return -1;  // exit code that will exit main()
                }

                // configure GPIO pin
                int err = gpio_pin_configure_dt(&sleep_button, GPIO_INPUT);
                if (err < 0) {
                    LOG_ERR("Cannot configure sw0 pin.");
                    return err;
                }
                err = gpio_pin_configure_dt(&reset_button, GPIO_INPUT);
                if (err < 0) {
                    LOG_ERR("Cannot configure reset button.");
                    return err;
                }
                err = gpio_pin_configure_dt(&freq_up_button, GPIO_INPUT);
                if (err < 0) {
                    LOG_ERR("Cannot configure frequency up button.");
                    return err;
                }
                err = gpio_pin_configure_dt(&freq_down_button, GPIO_INPUT);
                if (err < 0) {
                    LOG_ERR("Cannot configure frequency down button.");
                    return err;
                }

                err = gpio_pin_configure_dt(&heartbeat_led, GPIO_OUTPUT_ACTIVE);
                if (err < 0) {
                    LOG_ERR("Cannot configure heartbeat LED.");
                    return err;
                }

                err = gpio_pin_configure_dt(&iv_pump_led, GPIO_OUTPUT_ACTIVE);
                if (err < 0) {
                    LOG_ERR("Cannot configure iv pump LED.");
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

                // associate callback with GPIO pin
                // trigger on transition from INACTIVE -> ACTIVE; ACTIVE could be HIGH or LOW
                err = gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw0.");
                }
                err = gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_EDGE_TO_ACTIVE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw3.");
                }
                err = gpio_pin_interrupt_configure_dt(&freq_up_button, GPIO_INT_EDGE_TO_ACTIVE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw1.");
                }
                err = gpio_pin_interrupt_configure_dt(&freq_down_button, GPIO_INT_EDGE_TO_ACTIVE); 
                if (err < 0) {
                    LOG_ERR("Cannot attach callback to sw2.");
                }
                // populate CB struct with information about the CB function and pin
                gpio_init_callback(&sleep_button_cb, sleep_button_callback, BIT(sleep_button.pin)); // associate callback with GPIO pin
                gpio_add_callback_dt(&sleep_button, &sleep_button_cb);

                gpio_init_callback(&reset_button_cb, reset_button_callback, BIT(reset_button.pin)); // associate callback with GPIO pin
                gpio_add_callback_dt(&reset_button, &reset_button_cb);

                gpio_init_callback(&freq_up_button_cb, freq_up_button_callback, BIT(freq_up_button.pin)); // associate callback with GPIO pin
                gpio_add_callback_dt(&freq_up_button, &freq_up_button_cb);

                gpio_init_callback(&freq_down_button_cb, freq_down_button_callback, BIT(freq_down_button.pin)); // associate callback with GPIO pin
                gpio_add_callback_dt(&freq_down_button, &freq_down_button_cb);

                state = DEFAULT_SETUP;  // transition to the next state

                break;
            case DEFAULT_SETUP:

                gpio_pin_interrupt_configure_dt(&freq_up_button, GPIO_INT_EDGE_TO_ACTIVE);
                gpio_pin_interrupt_configure_dt(&freq_down_button, GPIO_INT_EDGE_TO_ACTIVE);
                gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE);
                gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_EDGE_TO_ACTIVE);

                // if (current_time - heartbeat_led_status.toggle_time > HEARTBEAT_TOGGLE_INTERVAL_MS) {
                //     LOG_INF("Toggling heartbeat LED at 1 Hz");
                //     gpio_pin_toggle_dt(&heartbeat_led);
                //     heartbeat_led_status.toggle_time = current_time;
                // }
                heartbeat_blink(current_time);

                last_action_led_toggle_freq = LED_BLINK_FREQ_HZ;

                gpio_pin_set_dt(&iv_pump_led, 1);
                iv_pump_led_status.illuminated = 1;

                gpio_pin_set_dt(&buzzer_led, 0);
                buzzer_led_status.illuminated = 0;

                if (current_time - iv_pump_led_status.toggle_time > (1000 / (2 * LED_BLINK_FREQ_HZ))) {
                    LOG_INF("Toggling action LEDs at frequency: %d Hz", last_action_led_toggle_freq);

                    gpio_pin_toggle_dt(&iv_pump_led);
                    iv_pump_led_status.toggle_time = current_time;
                    iv_pump_led_status.illuminated = !iv_pump_led_status.illuminated;

                    gpio_pin_toggle_dt(&buzzer_led);
                    buzzer_led_status.toggle_time = current_time;
                    buzzer_led_status.illuminated = !buzzer_led_status.illuminated;
                }

                gpio_pin_set_dt(&error_led, 0); // ensure error LED is off

                state = AWAKE_ENTRY;  // transition to the next state
                break;  
            case AWAKE_ENTRY:

                if(last_action_led_toggle_freq < ACTION_BUTTON_MIN_THRESHOLD_HZ){
                    state = ERROR; // go to error state if frequency is too low
                }else if(last_action_led_toggle_freq > ACTION_BUTTON_MAX_THRESHOLD_HZ){
                    state = ERROR; // go to error state if frequency is too high
                }else {
                    state = AWAKE_RUN;  // transition to the next state
                }

                // if (current_time - heartbeat_led_status.toggle_time > HEARTBEAT_TOGGLE_INTERVAL_MS) {
                //     LOG_INF("Toggling heartbeat LED at 1 Hz");
                //     gpio_pin_toggle_dt(&heartbeat_led);
                //     heartbeat_led_status.toggle_time = current_time;
                // }
                heartbeat_blink(current_time);

                break;

            case AWAKE_RUN:
                // do something

                // if (current_time - heartbeat_led_status.toggle_time > HEARTBEAT_TOGGLE_INTERVAL_MS) {
                //     LOG_INF("Toggling heartbeat LED at 1 Hz");
                //     gpio_pin_toggle_dt(&heartbeat_led);
                //     heartbeat_led_status.toggle_time = current_time;
                // }
                heartbeat_blink(current_time);
                
                gpio_pin_set_dt(&iv_pump_led, iv_pump_led_status.illuminated);
                gpio_pin_set_dt(&buzzer_led, buzzer_led_status.illuminated);

                if (current_time - iv_pump_led_status.toggle_time > (1000 / (2 * last_action_led_toggle_freq))) {
                    LOG_INF("Toggling action LEDs at frequency: %d Hz", last_action_led_toggle_freq);
                    gpio_pin_toggle_dt(&iv_pump_led);
                    iv_pump_led_status.toggle_time = current_time;
                    iv_pump_led_status.illuminated = !iv_pump_led_status.illuminated;

                    gpio_pin_toggle_dt(&buzzer_led);
                    buzzer_led_status.toggle_time = current_time;
                    buzzer_led_status.illuminated = !buzzer_led_status.illuminated;
                }
                break;  // break out of the switch statement without evaluating other cases
            
            case SLEEP:

                gpio_pin_set_dt(&iv_pump_led, 0);
                gpio_pin_set_dt(&buzzer_led, 0);

                // if (current_time - heartbeat_led_status.toggle_time > HEARTBEAT_TOGGLE_INTERVAL_MS) {
                //     LOG_INF("Toggling heartbeat LED at 1 Hz");
                //     gpio_pin_toggle_dt(&heartbeat_led);
                //     // gpio_pin_set_dt() - explicitly set the state of a pin (e.g. gpio_pin_set_dt(&led0, 1))
                //     heartbeat_led_status.toggle_time = current_time;
                // }
                heartbeat_blink(current_time);

                LOG_INF("In sleep state, action LEDs off.");

                break;

            case ERROR:
                
                LOG_ERR("In error state!");

                gpio_pin_set_dt(&error_led, 1);
                gpio_pin_set_dt(&iv_pump_led, 0);
                gpio_pin_set_dt(&buzzer_led, 0);

                // if (current_time - heartbeat_led_status.toggle_time > HEARTBEAT_TOGGLE_INTERVAL_MS) {
                //     LOG_INF("Toggling heartbeat LED at 1 Hz");
                //     gpio_pin_toggle_dt(&heartbeat_led);
                //     heartbeat_led_status.toggle_time = current_time;
                // }
                heartbeat_blink(current_time);

                gpio_pin_interrupt_configure_dt(&freq_up_button, GPIO_INT_DISABLE);
                gpio_pin_interrupt_configure_dt(&freq_down_button, GPIO_INT_DISABLE);
                gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_DISABLE);
                gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_EDGE_TO_ACTIVE);

                if(reset_button_event) {
                    LOG_INF("reset button pressed."); 
                    gpio_pin_interrupt_configure_dt(&freq_up_button, GPIO_INT_EDGE_TO_ACTIVE);
                    gpio_pin_interrupt_configure_dt(&freq_down_button, GPIO_INT_EDGE_TO_ACTIVE);
                    gpio_pin_interrupt_configure_dt(&sleep_button, GPIO_INT_EDGE_TO_ACTIVE);
                    gpio_pin_interrupt_configure_dt(&reset_button, GPIO_INT_EDGE_TO_ACTIVE);

                    state = DEFAULT_SETUP;
                    reset_button_event = 0;  // clear the event after taking action
                }
                break;

            default:
                // handle unexpected state
                break;
        }

        // test for the callback event state in your code
        if (sleep_button_event) {
            LOG_INF("sleep button pressed."); 
            // do something based on the event
            if(!sleep_button_pressed){
                sleep_button_pressed = 1;
                state = SLEEP;
            }else if(sleep_button_pressed){
                sleep_button_pressed = 0;
                state = AWAKE_ENTRY;
            }
            sleep_button_event = 0;  // clear the event after taking action
        } 

        if(reset_button_event) {
            LOG_INF("reset button pressed."); 
            state = DEFAULT_SETUP;
            reset_button_event = 0;  // clear the event after taking action
        } 

        if(freq_up_button_event) {
            LOG_INF("frequency increase/up button pressed."); 
            increase_action_led_blink_frequency();
            freq_up_button_event = 0;  // clear the event after taking action
            state = AWAKE_ENTRY;
        }  

        if(freq_down_button_event) {
            LOG_INF("frequency decrease/down button pressed."); 
            decrease_action_led_blink_frequency();
            freq_down_button_event = 0;  // clear the event after taking action
            state = AWAKE_ENTRY;
        }
    
        k_msleep(10);  // include a very short sleep statement to allow any LOG messages to be printed
    }
}

// define callback functions
void sleep_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    sleep_button_event = 1;  // conditional statement in main() can now do something based on the event detection
                             // we can also use actual system kernel event flags, but this is simpler (for now)
}

void reset_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    reset_button_event = 1;  // conditional statement in main() can now do something based on the event detection
                             // we can also use actual system kernel event flags, but this is simpler (for now)
}

void freq_up_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    freq_up_button_event = 1;  // conditional statement in main() can now do something based on the event detection
                             // we can also use actual system kernel event flags, but this is simpler (for now)
}

void freq_down_button_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    freq_down_button_event = 1;  // conditional statement in main() can now do something based on the event detection
                             // we can also use actual system kernel event flags, but this is simpler (for now)
}

void increase_action_led_blink_frequency(void) {
    // increase the blink frequency of the action LEDs by 1 Hz, up to a max of 5 Hz
    last_action_led_toggle_freq += FREQ_UP_INC_HZ;
    LOG_INF("Increased action LED blink frequency");
}

void decrease_action_led_blink_frequency(void) {
    // decrease the blink frequency of the action LEDs by 1 Hz, down to a min of 1 Hz
    last_action_led_toggle_freq -= FREQ_DOWN_INC_HZ;
    LOG_INF("Decreased action LED blink frequency");
}

void heartbeat_blink(int64_t current_time){
    if (current_time - heartbeat_led_status.toggle_time > HEARTBEAT_TOGGLE_INTERVAL_MS) {
        LOG_INF("Toggling heartbeat LED at 1 Hz");
        gpio_pin_toggle_dt(&heartbeat_led);
        heartbeat_led_status.toggle_time = current_time;
    }
}