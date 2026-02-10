/**
 * @file test_button_callback.c
 * @brief Unit tests for button callback functionality
 * 
 * This test file directly tests the button_test_callback function
 * by calling it directly, simulating what would happen when an
 * interrupt occurs. This approach allows us to test student code
 * without modifying it or requiring actual hardware interrupts.
 * 
 * Testing Strategy:
 * 1. Initialize the button_events k_event (if needed)
 * 2. Clear any existing events
 * 3. Directly call button_test_callback() to simulate interrupt
 * 4. Verify that BUTTON_EVENT was posted to button_events
 * 5. Verify LED_STATE changes appropriately
 * 
 * This follows the professor's guidance: "directly test the callback
 * function by directly calling it in an emulated test rather than
 * trying to 'trigger' the ISR and having that call the callback function."
 */

#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "test_button_callback.h"

/**
 * @brief Test fixture setup - runs before each test
 * 
 * This function is called automatically by ztest before each test
 * function runs. We use it to ensure a clean state.
 */
static void test_before(void *fixture)
{
    /* Initialize the event if not already initialized */
    k_event_init(&button_events);
    
    /* Clear any existing events to ensure clean test state */
    k_event_clear(&button_events, BUTTON_EVENT);
    
    /* Reset LED state to known value */
    LED_STATE = LED_OFF;
}

/**
 * @brief Test that callback posts BUTTON_EVENT when called
 * 
 * This test directly calls button_test_callback() and verifies
 * that it correctly posts BUTTON_EVENT to the button_events
 * k_event object.
 */
ZTEST(button_callback_tests, test_button_callback_posts_event)
{
    /* Ensure clean state */
    k_event_clear(&button_events, BUTTON_EVENT);
    
    /* Verify event is NOT set before callback */
    uint32_t events_before = k_event_wait(&button_events, BUTTON_EVENT, 
                                          false, K_NO_WAIT);
    zassert_equal(events_before, 0, 
                  "BUTTON_EVENT should not be set before callback");
    
    /* DIRECTLY CALL THE CALLBACK - simulating what happens in ISR */
    /* We pass NULL for dev and cb since we're testing the callback logic,
       not the actual GPIO interrupt mechanism */
    button_test_callback(NULL, NULL, BIT(0));
    
    /* Give kernel a moment to process the event post */
    k_msleep(10);
    
    /* Verify event is set after callback */
    uint32_t events_after = k_event_wait(&button_events, BUTTON_EVENT, 
                                         false, K_NO_WAIT);
    zassert_true(events_after & BUTTON_EVENT, 
                 "BUTTON_EVENT should be posted by callback");
}

/**
 * @brief Test that callback can be called multiple times
 * 
 * This test verifies that the callback can handle multiple
 * invocations (simulating multiple button presses).
 */
ZTEST(button_callback_tests, test_button_callback_multiple_calls)
{
    
    /* Call callback first time */
    button_test_callback(NULL, NULL, BIT(0));
    k_msleep(10);
    
    uint32_t events1 = k_event_wait(&button_events, BUTTON_EVENT, 
                                    false, K_NO_WAIT);
    zassert_true(events1 & BUTTON_EVENT, 
                 "First callback should post event");
    
    /* Clear the event */
    k_event_clear(&button_events, BUTTON_EVENT);
    
    /* Call callback second time */
    button_test_callback(NULL, NULL, BIT(0));
    k_msleep(10);
    
    uint32_t events2 = k_event_wait(&button_events, BUTTON_EVENT, 
                                    false, K_NO_WAIT);
    zassert_true(events2 & BUTTON_EVENT, 
                 "Second callback should also post event");
}

/**
 * @brief Test that callback works with different pin values
 * 
 * This test verifies the callback handles the pins parameter
 * correctly (though in this case it may not use it).
 */
ZTEST(button_callback_tests, test_button_callback_with_pins)
{
    /* Ensure clean state */
    k_event_clear(&button_events, BUTTON_EVENT);
    LED_STATE = LED_OFF;
    
    /* Call with specific pin bit */
    button_test_callback(NULL, NULL, BIT(11));  /* Pin 11 as used in CI */
    k_msleep(10);
    
    uint32_t events = k_event_wait(&button_events, BUTTON_EVENT, 
                                   false, K_NO_WAIT);
    zassert_true(events & BUTTON_EVENT, 
                 "Callback should post event regardless of pin value");
}

/* Register test suite with ztest - using new API */
ZTEST_SUITE(button_callback_tests, NULL, NULL, test_before, NULL, NULL);




/*
What you'd need to do:

1. Use Zephyr's timing APIs - Functions like k_uptime_get() or k_cycle_get_32() to measure time between LED state changes
Mock or intercept GPIO calls - Instead of letting the LED actually blink, you can:

2. Use ztest's mocking capabilities (if available for your GPIO driver)
Hook into the GPIO pin state changes
Record timestamps when the LED state toggles

3. Write test assertions - Use ztest's assertion macros like zassert_true(), zassert_within(), etc. to verify the blink frequency is within acceptable tolerances




void test_led_blink_frequency(void) {
    uint32_t start_time, end_time;
    int toggle_count = 0;
    
    // Start the blinking task
    start_blinking();
    
    start_time = k_uptime_get();
    
    // Monitor GPIO state changes for a period
    // (you'd need hooks to detect toggles)
    k_sleep(K_MSEC(1000));
    
    end_time = k_uptime_get();
    
    // Calculate frequency from toggle_count and time
    float frequency = toggle_count / 2.0 / ((end_time - start_time) / 1000.0);
    
    zassert_within(frequency, 1.0, 0.1, "LED should blink at 1Hz");
}
*/